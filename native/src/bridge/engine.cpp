/**
 * Engine state machine — top-level game lifecycle management.
 * Coordinates XEX/STFS/ISO loading, JIT startup, GPU init, and teardown.
 */
#include "../../include/loader/xex_loader.h"
#include "../../include/loader/stfs_parser.h"
#include "../../include/loader/iso_parser.h"
#include "../../include/jit/jit_engine.h"
#include "../../include/hle/hle_kernel.h"
#include "../../include/gpu/gpu_layer.h"
#include <android/log.h>
#include <string>
#include <filesystem>
#include <sys/mman.h>

#define LOG_TAG "X360:ENGINE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ─── Globals defined in jni_bridge.cpp (global scope, not namespaced) ────────
// engine.cpp uses extern declarations at global scope to match the definitions.
extern x360::XexLoader          g_xexLoader;
extern x360::jit::JitEngine     g_jitEngine;
extern x360::hle::HleKernel     g_hleKernel;
extern x360::gpu::GpuLayer      g_gpuLayer;
extern uint8_t*                  g_guestMemory;
extern std::string               g_gameDir;
extern std::string               g_saveDir;

static constexpr uint64_t GUEST_BASE     = 0x00000000ULL;
static constexpr size_t   GUEST_MEM_SIZE = 0x100000000ULL;

namespace x360 {
namespace bridge {

namespace fs = std::filesystem;

// ─── STFS game loader (delegates to StfsParser + XexLoader) ─────────────────

int32_t loadStfsGame(const char* path, char* outTitleId, int32_t outLen) {
    LOGI("ENGINE: loading STFS container: %s", path);

    x360::StfsParser parser;
    x360::StfsPackage pkg;

    if (!parser.open(path, pkg)) {
        LOGE("ENGINE: STFS open failed: %s", parser.lastError().c_str());
        return -1;
    }

    LOGI("ENGINE: STFS package '%s' (titleId=%s, %zu files)",
         pkg.displayName.c_str(), pkg.titleId.c_str(), pkg.files.size());

    // Extract to temp directory
    std::string extractDir = g_gameDir + "/.stfs_extract/" + pkg.titleId;
    int extracted = parser.extractAll(path, extractDir, pkg);
    if (extracted < 0) {
        LOGE("ENGINE: STFS extract failed");
        return -2;
    }

    // Find default.xex in extracted files
    std::string xexPath;
    for (const auto& f : pkg.files) {
        std::string lower = f.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("default.xex") != std::string::npos) {
            xexPath = extractDir + "/" + f.name;
            break;
        }
    }

    if (xexPath.empty()) {
        LOGE("ENGINE: default.xex not found in STFS package");
        return -3;
    }

    // Now load the XEX
    x360::XexImage img;
    if (!g_xexLoader.load(xexPath, img)) {
        LOGE("ENGINE: XEX load failed: %s", g_xexLoader.lastError().c_str());
        return -4;
    }

    if (outTitleId && outLen > 0) {
        strncpy(outTitleId, img.titleId.c_str(), outLen - 1);
        outTitleId[outLen - 1] = '\0';
    }

    // Map XEX into guest memory
    const uint64_t imageOffset = img.baseAddress - GUEST_BASE;
    if (g_guestMemory && imageOffset + img.rawMemory.size() <= GUEST_MEM_SIZE) {
        memcpy(g_guestMemory + imageOffset, img.rawMemory.data(), img.rawMemory.size());
    }

    return g_jitEngine.init(g_guestMemory, GUEST_MEM_SIZE, GUEST_BASE, img.entryPoint) ? 0 : -5;
}

// ─── ISO game loader ──────────────────────────────────────────────────────────

int32_t loadIsoGame(const char* path, char* outTitleId, int32_t outLen) {
    LOGI("ENGINE: loading ISO: %s", path);

    x360::IsoParser parser;
    if (!parser.open(path)) {
        LOGE("ENGINE: ISO open failed: %s", parser.lastError().c_str());
        return -1;
    }

    // Find default.xex
    const x360::IsoFileEntry* xexEntry = nullptr;
    for (const auto& f : parser.listFiles()) {
        std::string lower = f.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.find("default.xex") != std::string::npos) {
            xexEntry = &f;
            break;
        }
    }

    if (!xexEntry) {
        LOGE("ENGINE: default.xex not found in ISO");
        return -2;
    }

    // Extract to temp
    std::string xexPath = g_gameDir + "/.iso_extract/default.xex";
    fs::create_directories(g_gameDir + "/.iso_extract");
    if (!parser.extractFile(*xexEntry, xexPath)) {
        LOGE("ENGINE: ISO extract failed");
        return -3;
    }

    // Load XEX
    x360::XexImage img;
    if (!g_xexLoader.load(xexPath, img)) {
        LOGE("ENGINE: XEX load failed: %s", g_xexLoader.lastError().c_str());
        return -4;
    }

    if (outTitleId && outLen > 0) {
        strncpy(outTitleId, img.titleId.c_str(), outLen - 1);
        outTitleId[outLen - 1] = '\0';
    }

    const uint64_t imageOffset = img.baseAddress - GUEST_BASE;
    if (g_guestMemory && imageOffset + img.rawMemory.size() <= GUEST_MEM_SIZE) {
        memcpy(g_guestMemory + imageOffset, img.rawMemory.data(), img.rawMemory.size());
    }

    return g_jitEngine.init(g_guestMemory, GUEST_MEM_SIZE, GUEST_BASE, img.entryPoint) ? 0 : -5;
}

} // namespace bridge
} // namespace x360
