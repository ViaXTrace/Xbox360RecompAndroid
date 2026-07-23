/**
 * JNI Bridge — exposes the native engine to Dart via dart:ffi
 * and to Kotlin via JNI for surface management.
 *
 * dart:ffi entry points use C linkage with x360_ prefix.
 * JNI entry points use Java_com_viaxTrace_xbox360recomp_ prefix.
 */
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <string>
#include <cstring>
#include <atomic>

#include "../../include/loader/xex_loader.h"
#include "../../include/jit/jit_engine.h"
#include "../../include/hle/hle_kernel.h"
#include "../../include/gpu/gpu_layer.h"

#define LOG_TAG "X360Recomp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ─── Global engine instances ────────────────────────────────────────────────────
static x360::XexLoader    g_xexLoader;
static x360::jit::JitEngine g_jitEngine;
static x360::hle::HleKernel g_hleKernel;
static x360::gpu::GpuLayer  g_gpuLayer;

static std::atomic<bool> g_initialized{false};
static std::string g_gameDir;
static std::string g_saveDir;

// Guest address space — 4 GB mmap'd region
static constexpr size_t GUEST_MEM_SIZE = 0x100000000ULL; // 4 GB
static uint8_t* g_guestMemory = nullptr;
static constexpr uint64_t GUEST_BASE = 0x00000000ULL;

static int g_logLevel = 1;

// ─── dart:ffi C entry points ────────────────────────────────────────────────────

extern "C" {

/**
 * x360_engine_init — initialize guest memory, HLE kernel, register HLE handlers.
 * @param logLevel  0=off, 1=info, 2=verbose
 * @return 0 on success, negative on error
 */
int32_t x360_engine_init(int32_t logLevel) {
    g_logLevel = logLevel;
    if (g_initialized.load()) {
        LOGI("Engine already initialized");
        return 0;
    }

    LOGI("Initializing Xbox360 engine (log level %d)", logLevel);

    // Allocate guest address space via anonymous mmap
    g_guestMemory = (uint8_t*)mmap(nullptr, GUEST_MEM_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
        -1, 0);

    if (g_guestMemory == MAP_FAILED) {
        LOGE("Failed to mmap guest address space: %s", strerror(errno));
        return -1;
    }

    LOGI("Guest address space: %p (4 GB)", g_guestMemory);

    // Initialize HLE kernel
    if (!g_hleKernel.init(g_guestMemory, GUEST_BASE, g_gameDir, g_saveDir)) {
        LOGE("HLE kernel init failed: %s", g_hleKernel.lastError().c_str());
        munmap(g_guestMemory, GUEST_MEM_SIZE);
        g_guestMemory = nullptr;
        return -2;
    }

    g_initialized.store(true);
    LOGI("Engine initialized successfully");
    return 0;
}

/**
 * x360_engine_shutdown — stop all emulation and release resources.
 */
void x360_engine_shutdown() {
    if (!g_initialized.load()) return;
    LOGI("Shutting down engine");

    g_jitEngine.stop();
    g_gpuLayer.clearSurface();

    if (g_guestMemory) {
        munmap(g_guestMemory, GUEST_MEM_SIZE);
        g_guestMemory = nullptr;
    }
    g_initialized.store(false);
    LOGI("Engine shutdown complete");
}

/**
 * x360_xex_load — parse, decrypt, decompress and map a XEX2 executable.
 * @param path   null-terminated UTF-8 host file path
 * @param outTitleId  output buffer for title ID string
 * @param outLen size of outTitleId buffer
 * @return 0 on success
 */
int32_t x360_xex_load(const char* path, char* outTitleId, int32_t outLen) {
    if (!g_initialized.load()) return -1;
    LOGI("Loading XEX: %s", path);

    x360::XexImage img;
    if (!g_xexLoader.load(path, img)) {
        LOGE("XEX load failed: %s", g_xexLoader.lastError().c_str());
        return -2;
    }

    if (outTitleId && outLen > 0) {
        strncpy(outTitleId, img.titleId.c_str(), outLen - 1);
        outTitleId[outLen - 1] = '\0';
    }

    // Copy image to guest memory at its base address
    const uint64_t imageOffset = img.baseAddress - GUEST_BASE;
    if (imageOffset + img.rawMemory.size() <= GUEST_MEM_SIZE) {
        memcpy(g_guestMemory + imageOffset, img.rawMemory.data(), img.rawMemory.size());
    } else {
        LOGE("XEX image doesn't fit in guest address space");
        return -3;
    }

    // Initialize JIT with guest memory + entry point
    if (!g_jitEngine.init(g_guestMemory, GUEST_MEM_SIZE, GUEST_BASE, img.entryPoint)) {
        LOGE("JIT engine init failed");
        return -4;
    }

    // Register HLE handlers for xboxkrnl.exe and xam.xex imports
    for (const auto& import : img.imports) {
        g_jitEngine.registerHle(import.moduleName, import.ordinal,
            [&](x360::jit::PPCContext& ctx, uint32_t ordinal) {
                g_hleKernel.dispatchKernelCall(import.moduleName, ordinal,
                    reinterpret_cast<x360::jit::PPCContext&>(ctx));
            });
    }

    LOGI("XEX loaded: %s (entry: 0x%08llX)", img.titleId.c_str(), (unsigned long long)img.entryPoint);
    return 0;
}

int32_t x360_stfs_load(const char* path, char* outTitleId, int32_t outLen) {
    // TODO: parse STFS container, extract default.xex, then call x360_xex_load on it
    LOGI("STFS load: %s (stub)", path);
    if (outTitleId && outLen > 0) strncpy(outTitleId, "STFS_STUB", outLen - 1);
    return 0;
}

int32_t x360_iso_load(const char* path, char* outTitleId, int32_t outLen) {
    // TODO: mount ISO 9660 / XDVDFS, find default.xex, call x360_xex_load
    LOGI("ISO load: %s (stub)", path);
    if (outTitleId && outLen > 0) strncpy(outTitleId, "ISO_STUB", outLen - 1);
    return 0;
}

void x360_game_unload() {
    g_jitEngine.stop();
    LOGI("Game unloaded");
}

int32_t x360_jit_start(int32_t threadCount) {
    if (!g_initialized.load()) return -1;
    LOGI("Starting JIT with %d threads", threadCount);
    return g_jitEngine.start(threadCount) ? 0 : -1;
}

void x360_jit_stop() {
    g_jitEngine.stop();
}

int32_t x360_jit_get_state() {
    return static_cast<int32_t>(g_jitEngine.state());
}

int32_t x360_gpu_set_surface(void* nativeWindow) {
    auto* win = reinterpret_cast<ANativeWindow*>(nativeWindow);
    LOGI("GPU set surface: %p", win);
    if (!g_gpuLayer.isInitialized()) {
        return g_gpuLayer.init(win, g_guestMemory, GUEST_BASE) ? 0 : -1;
    }
    return g_gpuLayer.setSurface(win) ? 0 : -1;
}

void x360_gpu_clear_surface() {
    g_gpuLayer.clearSurface();
}

// Metrics
float x360_metrics_fps()        { return g_jitEngine.currentFps(); }
float x360_metrics_frame_time() { return (float)g_jitEngine.frameTimeMs(); }
int64_t x360_metrics_ram()      {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (int64_t)ru.ru_maxrss * 1024;
}
float x360_metrics_cpu_temp()   { return 0.0f; /* read from /sys/class/thermal/ */ }
float x360_metrics_gpu_temp()   { return 0.0f; }

// Input
void x360_input_set_state(int32_t pad, int32_t buttons,
    int32_t lx, int32_t ly, int32_t rx, int32_t ry, int32_t lt, int32_t rt) {
    g_hleKernel.setInputState(pad, (uint32_t)buttons,
        (int16_t)lx, (int16_t)ly, (int16_t)rx, (int16_t)ry,
        (uint8_t)lt, (uint8_t)rt);
}

void x360_input_set_rumble(int32_t pad, int32_t lowFreq, int32_t highFreq) {
    (void)pad; (void)lowFreq; (void)highFreq;
    // TODO: map to Android Vibrator API via JNI upcall
}

int32_t x360_logs_export(char* outBuf, int32_t outLen) {
    const char* stub = "Log export: connect ADB or enable file logging.\n";
    strncpy(outBuf, stub, outLen - 1);
    return (int32_t)strlen(stub);
}

} // extern "C"

// ─── JNI entry points (called from Kotlin SurfaceBridge.kt) ─────────────────────
extern "C" {

JNIEXPORT void JNICALL
Java_com_viaxtrace_xbox360recomp_SurfaceBridge_nativeSurfaceCreated(JNIEnv* env, jobject, jobject surface) {
    ANativeWindow* win = ANativeWindow_fromSurface(env, surface);
    if (win) {
        x360_gpu_set_surface(win);
        ANativeWindow_release(win);
    }
}

JNIEXPORT void JNICALL
Java_com_viaxtrace_xbox360recomp_SurfaceBridge_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    x360_gpu_clear_surface();
}

JNIEXPORT void JNICALL
Java_com_viaxtrace_xbox360recomp_SurfaceBridge_nativeSurfaceChanged(JNIEnv*, jobject, jint width, jint height) {
    g_gpuLayer.onSurfaceChanged(width, height);
}

JNIEXPORT jstring JNICALL
Java_com_viaxtrace_xbox360recomp_SurfaceBridge_nativeGetEngineVersion(JNIEnv* env, jobject) {
    return env->NewStringUTF("Xbox360RecompAndroid/0.1.0 (Phase1)");
}

} // extern "C"
