/**
 * HLE Memory — NtAllocateVirtualMemory, NtFreeVirtualMemory, XMemAlloc/Free.
 * Guest address space: 4GB mmap'd at init. Sub-allocations via a free-list heap.
 */
#include "../../include/hle/hle_kernel.h"
#include <sys/mman.h>
#include <cstring>
#include <map>
#include <mutex>
#include <android/log.h>

#define LOG_TAG "X360:MEM"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace hle {

// Simple bump allocator within guest address space.
// Real implementation would need a proper free-list, but this handles most games.
static uint8_t* g_guestBase = nullptr;
static uint64_t g_guestPhysBase = 0;
static std::mutex g_heapMutex;

// Regions: guest addr → size (for free tracking)
static std::map<uint64_t, uint64_t> g_allocations;

// Next heap pointer for XMemAlloc (starts at guest 0x40000000 — "physical" region)
static uint64_t g_heapPtr = 0x40000000ULL;
static constexpr uint64_t kHeapEnd = 0x7FFFFFFFULL;

uint64_t HleKernel::allocVirtualMemory(uint64_t baseAddr, uint64_t size,
                                       uint32_t type, uint32_t protect) {
    std::lock_guard<std::mutex> lk(g_heapMutex);
    // Align size to 64KB
    size = (size + 0xFFFF) & ~0xFFFFULL;

    uint64_t addr = baseAddr;
    if (addr == 0) {
        addr = g_heapPtr;
        g_heapPtr += size;
    }

    if (addr + size > kHeapEnd) {
        LOGE("MEM: allocVirtualMemory OOM: requested 0x%llX", (unsigned long long)size);
        return 0;
    }

    // Zero-fill the region in host memory
    uint64_t hostOffset = addr - g_guestPhysBase;
    if (g_guestBase && hostOffset + size <= 0x100000000ULL) {
        memset(g_guestBase + hostOffset, 0, size);
    }

    g_allocations[addr] = size;
    LOGI("MEM: allocVirtualMemory 0x%llX size=0x%llX", (unsigned long long)addr, (unsigned long long)size);
    return addr;
}

void HleKernel::freeVirtualMemory(uint64_t baseAddr) {
    std::lock_guard<std::mutex> lk(g_heapMutex);
    g_allocations.erase(baseAddr);
    LOGI("MEM: freeVirtualMemory 0x%llX", (unsigned long long)baseAddr);
}

uint64_t HleKernel::xMemAlloc(uint64_t size, uint64_t params) {
    // XMemAlloc: allocate from the physical pool with optional alignment
    uint32_t alignment = (uint32_t)(params & 0xFFFF);
    if (alignment == 0) alignment = 16;

    std::lock_guard<std::mutex> lk(g_heapMutex);
    // Align pointer
    g_heapPtr = (g_heapPtr + alignment - 1) & ~(uint64_t)(alignment - 1);
    uint64_t addr = g_heapPtr;
    g_heapPtr += size;

    if (addr + size > kHeapEnd) {
        LOGE("MEM: xMemAlloc OOM: requested 0x%llX", (unsigned long long)size);
        return 0;
    }

    uint64_t hostOffset = addr - g_guestPhysBase;
    if (g_guestBase && hostOffset + size <= 0x100000000ULL) {
        memset(g_guestBase + hostOffset, 0, size);
    }

    g_allocations[addr] = size;
    LOGI("MEM: XMemAlloc 0x%llX size=0x%llX", (unsigned long long)addr, (unsigned long long)size);
    return addr;
}

void HleKernel::xMemFree(uint64_t baseAddr) {
    freeVirtualMemory(baseAddr);
}

// Called from HleKernel::init to set up memory pointers
void initMemorySubsystem(uint8_t* guestMemory, uint64_t guestBase) {
    g_guestBase    = guestMemory;
    g_guestPhysBase = guestBase;
    g_heapPtr = 0x40000000ULL;
    g_allocations.clear();
    LOGI("MEM: initialized, host=%p guest_base=0x%llX", guestMemory, (unsigned long long)guestBase);
}

} // namespace hle
} // namespace x360
