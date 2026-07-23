/**
 * Memory guard — protect guest code pages to detect self-modifying code.
 * Uses mprotect(PROT_READ) + SIGSEGV handler to catch writes to compiled pages.
 */
#include "../../include/jit/jit_engine.h"
#include <sys/mman.h>
#include <signal.h>
#include <unordered_set>
#include <mutex>
#include <android/log.h>

#define LOG_TAG "X360:GUARD"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace jit {

static std::unordered_set<uint64_t> g_protectedPages; // page-aligned guest addrs
static std::mutex g_pagesMutex;

// Protect a guest memory page (make it read-only so writes trigger SIGSEGV)
void guardPage(uint8_t* hostAddr) {
    uint64_t pageAddr = (uint64_t)hostAddr & ~0xFFFULL;
    {
        std::lock_guard<std::mutex> lk(g_pagesMutex);
        if (g_protectedPages.count(pageAddr)) return;
        g_protectedPages.insert(pageAddr);
    }
    mprotect(reinterpret_cast<void*>(pageAddr), 0x1000, PROT_READ);
}

// Unprotect a page (called from SIGSEGV handler before the write proceeds)
void unguardPage(void* faultAddr) {
    uint64_t pageAddr = (uint64_t)faultAddr & ~0xFFFULL;
    {
        std::lock_guard<std::mutex> lk(g_pagesMutex);
        g_protectedPages.erase(pageAddr);
    }
    mprotect(reinterpret_cast<void*>(pageAddr), 0x1000, PROT_READ | PROT_WRITE);
}

bool isPageGuarded(void* addr) {
    uint64_t pageAddr = (uint64_t)addr & ~0xFFFULL;
    std::lock_guard<std::mutex> lk(g_pagesMutex);
    return g_protectedPages.count(pageAddr) > 0;
}

} // namespace jit
} // namespace x360
