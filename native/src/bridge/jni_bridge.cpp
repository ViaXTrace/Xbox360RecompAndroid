/**
 * JNI Bridge — exposes the native engine to Dart via dart:ffi
 * and to Kotlin via JNI for surface management.
 */
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <atomic>

#include "../../include/loader/xex_loader.h"
#include "../../include/loader/stfs_parser.h"
#include "../../include/loader/iso_parser.h"
#include "../../include/jit/jit_engine.h"
#include "../../include/hle/hle_kernel.h"
#include "../../include/gpu/gpu_layer.h"

#define LOG_TAG "X360Recomp"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ─── Global engine instances ─────────────────────────────────────────────────
x360::XexLoader      g_xexLoader;
x360::jit::JitEngine g_jitEngine;
x360::hle::HleKernel g_hleKernel;
x360::gpu::GpuLayer  g_gpuLayer;

std::atomic<bool> g_initialized{false};
std::string g_gameDir;
std::string g_saveDir;

// Guest address space — 4 GB mmap region
static constexpr size_t   GUEST_MEM_SIZE = 0x100000000ULL;
uint8_t*                  g_guestMemory  = nullptr;
static constexpr uint64_t GUEST_BASE     = 0x00000000ULL;

static int g_logLevel = 1;

// ─── dart:ffi C entry points ─────────────────────────────────────────────────
extern "C" {

int32_t x360_engine_init(int32_t logLevel) {
    g_logLevel = logLevel;
    if (g_initialized.load()) return 0;

    LOGI("Initializing Xbox360 engine (log level %d)", logLevel);

    g_guestMemory = static_cast<uint8_t*>(mmap(nullptr, GUEST_MEM_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
        -1, 0));

    if (g_guestMemory == MAP_FAILED) {
        LOGE("Failed to mmap guest address space: %s", strerror(errno));
        g_guestMemory = nullptr;
        return -1;
    }

    LOGI("Guest address space: %p (4 GB)", static_cast<void*>(g_guestMemory));

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

int32_t x360_xex_load(const char* path, char* outTitleId, int32_t outLen) {
    if (!g_initialized.load()) return -1;
    LOGI("Loading XEX: %s", path);

    x360::XexImage img;
    if (!g_xexLoader.load(std::string(path), img)) {
        LOGE("XEX load failed: %s", g_xexLoader.lastError().c_str());
        return -2;
    }

    if (outTitleId && outLen > 0) {
        strncpy(outTitleId, img.titleId.c_str(), static_cast<size_t>(outLen) - 1);
        outTitleId[outLen - 1] = '\0';
    }

    const uint64_t imageOffset = img.baseAddress - GUEST_BASE;
    if (g_guestMemory && imageOffset + img.rawMemory.size() <= GUEST_MEM_SIZE) {
        memcpy(g_guestMemory + imageOffset, img.rawMemory.data(), img.rawMemory.size());
    }

    return g_jitEngine.init(g_guestMemory, GUEST_MEM_SIZE, GUEST_BASE, img.entryPoint) ? 0 : -3;
}

int32_t x360_stfs_load(const char* path, char* outTitleId, int32_t outLen) {
    if (!g_initialized.load()) return -1;
    LOGI("Loading STFS: %s", path);

    x360::StfsParser parser;
    x360::StfsPackage pkg;
    if (!parser.open(std::string(path), pkg)) {
        LOGE("STFS open failed: %s", parser.lastError().c_str());
        return -2;
    }

    if (outTitleId && outLen > 0) {
        strncpy(outTitleId, pkg.titleId.c_str(), static_cast<size_t>(outLen) - 1);
        outTitleId[outLen - 1] = '\0';
    }
    return 0;
}

int32_t x360_iso_load(const char* path, char* outTitleId, int32_t outLen) {
    if (!g_initialized.load()) return -1;
    LOGI("Loading ISO: %s", path);

    x360::IsoParser parser;
    if (!parser.open(std::string(path))) {
        LOGE("ISO open failed: %s", parser.lastError().c_str());
        return -2;
    }

    if (outTitleId && outLen > 0) {
        strncpy(outTitleId, "ISO_GAME", static_cast<size_t>(outLen) - 1);
        outTitleId[outLen - 1] = '\0';
    }
    return 0;
}

void x360_game_unload() {
    g_jitEngine.stop();
    LOGI("Game unloaded");
}

int32_t x360_jit_start(int32_t threadCount) {
    return g_jitEngine.start(threadCount) ? 0 : -1;
}

void x360_jit_stop() {
    g_jitEngine.stop();
}

int32_t x360_jit_get_state() {
    return static_cast<int32_t>(g_jitEngine.state());
}

int32_t x360_gpu_set_surface(void* nativeWindow) {
    if (!nativeWindow) return -1;
    auto* win = reinterpret_cast<ANativeWindow*>(nativeWindow);
    if (!g_gpuLayer.isInitialized()) {
        return g_gpuLayer.init(win, g_guestMemory, GUEST_BASE) ? 0 : -1;
    }
    return g_gpuLayer.setSurface(win) ? 0 : -1;
}

void x360_gpu_clear_surface() {
    g_gpuLayer.clearSurface();
}

void x360_input_set_state(int32_t pad, int32_t buttons,
                           int32_t lx, int32_t ly, int32_t rx, int32_t ry,
                           int32_t lt, int32_t rt) {
    g_hleKernel.setInputState(pad,
        static_cast<uint32_t>(buttons),
        static_cast<int16_t>(lx), static_cast<int16_t>(ly),
        static_cast<int16_t>(rx), static_cast<int16_t>(ry),
        static_cast<uint8_t>(lt),  static_cast<uint8_t>(rt));
}

void x360_input_set_rumble(int32_t /*pad*/, int32_t /*lowFreq*/, int32_t /*highFreq*/) {
    // TODO: map to Android Vibrator API
}

void x360_settings_set_resolution(int32_t width, int32_t height) {
    g_gpuLayer.onSurfaceChanged(width, height);
}
void x360_settings_set_af(int32_t /*af*/) {}
void x360_settings_set_aa(int32_t /*aa*/) {}
void x360_settings_set_fps_limit(int32_t /*limit*/) {}
void x360_settings_set_jit_threads(int32_t /*threads*/) {}

int32_t x360_saves_get_dir(char* outPath, int32_t outLen) {
    if (!outPath || outLen <= 0) return -1;
    strncpy(outPath, g_saveDir.c_str(), static_cast<size_t>(outLen) - 1);
    outPath[outLen - 1] = '\0';
    return 0;
}

int32_t x360_saves_backup(const char* /*titleId*/) { return 0; }

int32_t x360_logs_export(char* outBuf, int32_t outLen) {
    if (!outBuf || outLen <= 0) return -1;
    const char* msg = "[Log export not yet implemented]\n";
    strncpy(outBuf, msg, static_cast<size_t>(outLen) - 1);
    outBuf[outLen - 1] = '\0';
    return static_cast<int32_t>(strlen(msg));
}

} // extern "C"

// ─── JNI entry points for Kotlin ─────────────────────────────────────────────
extern "C" {

JNIEXPORT jstring JNICALL
Java_com_viaxecomp_xbox360recomp_MainActivity_nativeGetVersion(JNIEnv* env, jobject) {
    return env->NewStringUTF("0.1.0");
}

JNIEXPORT jboolean JNICALL
Java_com_viaxecomp_xbox360recomp_MainActivity_nativeIsEngineReady(JNIEnv*, jobject) {
    return g_initialized.load() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_viaxecomp_xbox360recomp_MainActivity_nativeSetDirectories(
        JNIEnv* env, jobject, jstring gameDir, jstring saveDir) {
    const char* gd = env->GetStringUTFChars(gameDir, nullptr);
    const char* sd = env->GetStringUTFChars(saveDir, nullptr);
    if (gd) { g_gameDir = gd; env->ReleaseStringUTFChars(gameDir, gd); }
    if (sd) { g_saveDir = sd; env->ReleaseStringUTFChars(saveDir, sd); }
    LOGI("Directories: game='%s' save='%s'", g_gameDir.c_str(), g_saveDir.c_str());
}

} // extern "C"
