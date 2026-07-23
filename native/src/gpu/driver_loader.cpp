/**
 * Custom GPU driver loader — AdrenoTools (Turnip/Mesa) + ExynosTools.
 * Allows loading a custom Vulkan ICD .so at runtime, bypassing the system driver.
 * Reference: libadrenotools (K11MCH1/AdrenoToolsDrivers), ExynosTools.
 */
#include "../../include/gpu/gpu_layer.h"
#include <dlfcn.h>
#include <cstring>
#include <android/log.h>

#define LOG_TAG "X360:DRIVER"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace gpu {

#ifdef HAVE_ADRENOTOOLS
// AdrenoTools API (conditionally compiled when the library is bundled)
extern "C" {
void* adrenoToolsOpenCustomDriver(const char* driverPath, const char* tmpLibDir);
}
#endif

bool GpuLayer::loadCustomDriver(const std::string& driverPath) {
#ifdef HAVE_ADRENOTOOLS
    LOGI("DRIVER: loading custom Vulkan driver: %s", driverPath.c_str());
    void* handle = adrenoToolsOpenCustomDriver(driverPath.c_str(), "/data/local/tmp");
    if (!handle) {
        LOGE("DRIVER: adrenoToolsOpenCustomDriver failed for %s", driverPath.c_str());
        return false;
    }
    LOGI("DRIVER: AdrenoTools custom driver loaded successfully");
    return true;
#else
    // Fallback: try dlopen directly (works on some devices/configurations)
    void* handle = dlopen(driverPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        LOGE("DRIVER: dlopen('%s') failed: %s", driverPath.c_str(), dlerror());
        return false;
    }

    // Check that it exports vkGetInstanceProcAddr
    void* sym = dlsym(handle, "vkGetInstanceProcAddr");
    if (!sym) {
        LOGE("DRIVER: not a Vulkan ICD (missing vkGetInstanceProcAddr)");
        dlclose(handle);
        return false;
    }

    LOGI("DRIVER: custom Vulkan ICD loaded: %s", driverPath.c_str());
    return true;
#endif
}

bool GpuLayer::loadTurnipDriver() {
    // Turnip is the Mesa open-source Adreno Vulkan driver.
    // On supported devices it offers significantly better performance.
    // The bundled libvulkan_freedreno.so must be placed in the APK.

    const char* turnipPaths[] = {
        "/data/data/com.viaxtrace.xbox360recomp/files/driver/libvulkan_freedreno.so",
        "/sdcard/Android/data/com.viaxtrace.xbox360recomp/driver/libvulkan_freedreno.so",
        nullptr
    };

    for (int i = 0; turnipPaths[i]; i++) {
        if (loadCustomDriver(turnipPaths[i])) {
            LOGI("DRIVER: Turnip loaded from %s", turnipPaths[i]);
            return true;
        }
    }

    LOGI("DRIVER: Turnip not found, using system driver");
    return false;
}

} // namespace gpu
} // namespace x360
