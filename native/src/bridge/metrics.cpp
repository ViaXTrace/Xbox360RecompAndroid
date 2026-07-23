/**
 * Metrics bridge — collects and exposes real-time performance counters.
 * CPU temperature, GPU temperature, RAM usage, FPS, frame time.
 * Exposed to Dart/Flutter via dart:ffi.
 */
#include "../../include/jit/jit_engine.h"
#include "../../include/gpu/gpu_layer.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <android/log.h>
#include <sys/resource.h>

#define LOG_TAG "X360:METRICS"

extern x360::jit::JitEngine g_jitEngine;
extern x360::gpu::GpuLayer  g_gpuLayer;

namespace x360 {
namespace bridge {

// Read a thermal zone temperature from sysfs (Android exposes these under /sys/class/thermal)
static float readThermalZone(int zone) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", zone);
    FILE* f = fopen(path, "r");
    if (!f) return 0.0f;
    int milliC = 0;
    fscanf(f, "%d", &milliC);
    fclose(f);
    return milliC / 1000.0f;
}

// Find the "best" CPU temperature zone (highest reading = hottest core)
float getCpuTemperature() {
    float maxTemp = 0.0f;
    for (int z = 0; z < 20; z++) {
        float t = readThermalZone(z);
        if (t > maxTemp && t < 150.0f) maxTemp = t; // sanity check < 150°C
    }
    return maxTemp;
}

// GPU temperature — on Adreno it's typically zone 12 or similar
float getGpuTemperature() {
    // Try common Adreno/Mali thermal zones
    float best = 0.0f;
    for (int z = 10; z < 20; z++) {
        float t = readThermalZone(z);
        if (t > best && t < 120.0f) best = t;
    }
    return best;
}

// RSS in bytes
int64_t getRamUsage() {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (int64_t)ru.ru_maxrss * 1024LL;
}

// Blocks compiled so far
int64_t getBlocksCompiled() {
    return (int64_t)g_jitEngine.blocksCompiled();
}

// Total PPC instructions executed
int64_t getInstructionsExecuted() {
    return (int64_t)g_jitEngine.instructionsExecuted();
}

} // namespace bridge
} // namespace x360

// dart:ffi entry points
extern "C" {

float x360_metrics_cpu_temp() {
    return x360::bridge::getCpuTemperature();
}

float x360_metrics_gpu_temp() {
    return x360::bridge::getGpuTemperature();
}

int64_t x360_metrics_blocks_compiled() {
    return x360::bridge::getBlocksCompiled();
}

int64_t x360_metrics_instructions_executed() {
    return x360::bridge::getInstructionsExecuted();
}

float x360_metrics_fps() {
    return (float)g_jitEngine.currentFps();
}

float x360_metrics_frame_time() {
    return (float)g_jitEngine.frameTimeMs();
}

int64_t x360_metrics_ram() {
    return x360::bridge::getRamUsage();
}

} // extern "C"
