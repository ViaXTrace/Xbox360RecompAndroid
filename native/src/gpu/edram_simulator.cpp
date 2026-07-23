/**
 * eDRAM Simulator — emulates the Xbox 360's 10MB embedded DRAM render target.
 * Uses Vulkan transient attachments to avoid unnecessary memory bandwidth.
 * On resolve, copies eDRAM tile contents to main memory (guest texture address).
 */
#include "../../include/gpu/gpu_layer.h"
#include <cstring>
#include <android/log.h>

#define LOG_TAG "X360:EDRAM"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace gpu {

// eDRAM is 10MB, organized as 2048 × 20 pixel tiles of 4×4 pixels = 10240 KB
static constexpr uint32_t EDRAM_SIZE_BYTES = 10 * 1024 * 1024;

void GpuLayer::resolveEdram() {
    // The eDRAM resolve copies pixels from the render surface to a guest memory address.
    // This address is specified in the RB_COPY_DEST_BASE register (offset 0x2003 in regs).
    //
    // A full implementation:
    // 1. Reads RB_COPY_DEST_BASE (guest address of destination texture)
    // 2. Reads RB_COPY_DEST_PITCH, RB_COPY_DEST_INFO (format + dimensions)
    // 3. Reads RB_COPY_CONTROL (resolve region)
    // 4. vkCmdBlitImage: eDRAM attachment → host-visible staging buffer
    // 5. memcpy: staging buffer → guest memory at dest address
    //
    // For now, this is a stub that marks the resolve as complete.
    // The actual render output is visible via the swapchain surface.

    uint32_t destBase  = m_regs.rbColorInfo; // simplified: use color info as dest hint
    LOGI("EDRAM: resolveEdram destBase=0x%X (stub)", destBase);
}

} // namespace gpu
} // namespace x360
