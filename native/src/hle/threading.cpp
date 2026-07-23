/**
 * HLE Threading — ExCreateThread, mutexes, semaphores, events, wait functions.
 * Maps Xbox 360 kernel objects to POSIX primitives.
 */
#include "../../include/hle/hle_kernel.h"
#include <pthread.h>
#include <semaphore.h>
#include <chrono>
#include <android/log.h>

#define LOG_TAG "X360:THR"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace hle {

// ─── Thread ──────────────────────────────────────────────────────────────────

// Additional thread state beyond KernelThread header
struct KernelThreadFull : KernelThread {
    std::thread hostThread2; // The actual running thread
};

struct KernelSemaphoreFull : KernelObject {
    sem_t sem;
};

uint32_t HleKernel::createThread(uint64_t startAddress, uint64_t startParam,
                                  uint32_t stackSize, int priority, bool createSuspended) {
    uint32_t handle = allocHandle();
    auto obj = std::make_unique<KernelThreadFull>();
    obj->handle = handle;
    obj->type   = KernelObjectType::Thread;
    obj->startPc = startAddress;
    obj->startParam = startParam;
    obj->running = !createSuspended;

    LOGI("THR: createThread handle=0x%X pc=0x%llX param=0x%llX",
         handle, (unsigned long long)startAddress, (unsigned long long)startParam);

    // The JIT engine picks up new threads via the context table.
    // For now, register the thread and let the main JIT dispatch it.
    // A real implementation would start a new JIT thread here.

    KernelObject* rawPtr = obj.get();
    {
        std::lock_guard<std::mutex> lk(m_objectMutex);
        m_objects[handle] = std::move(obj);
    }
    return handle;
}

void HleKernel::terminateThread(uint32_t handle, uint32_t exitCode) {
    LOGI("THR: terminateThread 0x%X exitCode=0x%X", handle, exitCode);
    destroyObject(handle);
}

// ─── Mutex ───────────────────────────────────────────────────────────────────

uint32_t HleKernel::createMutex(bool initialOwner) {
    uint32_t handle = allocHandle();
    auto obj = std::make_unique<KernelMutex>();
    obj->handle = handle;
    obj->type   = KernelObjectType::Mutex;
    if (initialOwner) {
        obj->m.lock();
        obj->ownerThread = 1; // simplified
        obj->recursionCount = 1;
    }
    std::lock_guard<std::mutex> lk(m_objectMutex);
    m_objects[handle] = std::move(obj);
    LOGI("THR: createMutex 0x%X owner=%d", handle, (int)initialOwner);
    return handle;
}

void HleKernel::releaseMutex(uint32_t handle) {
    auto* obj = getObject(handle);
    if (!obj || obj->type != KernelObjectType::Mutex) return;
    auto* m = static_cast<KernelMutex*>(obj);
    if (m->recursionCount > 0) { m->recursionCount--; }
    if (m->recursionCount == 0) { m->m.unlock(); m->ownerThread = 0; }
}

// ─── Semaphore ───────────────────────────────────────────────────────────────

uint32_t HleKernel::createSemaphore(int initialCount, int maxCount) {
    uint32_t handle = allocHandle();
    auto obj = std::make_unique<KernelSemaphoreFull>();
    obj->handle = handle;
    obj->type   = KernelObjectType::Semaphore;
    sem_init(&obj->sem, 0, (unsigned)initialCount);
    std::lock_guard<std::mutex> lk(m_objectMutex);
    m_objects[handle] = std::move(obj);
    LOGI("THR: createSemaphore 0x%X init=%d max=%d", handle, initialCount, maxCount);
    return handle;
}

void HleKernel::releaseSemaphore(uint32_t handle, int releaseCount) {
    auto* obj = getObject(handle);
    if (!obj || obj->type != KernelObjectType::Semaphore) return;
    auto* s = static_cast<KernelSemaphoreFull*>(obj);
    for (int i = 0; i < releaseCount; i++) sem_post(&s->sem);
}

// ─── Event ───────────────────────────────────────────────────────────────────

uint32_t HleKernel::createEvent(bool manualReset, bool initialState) {
    uint32_t handle = allocHandle();
    auto obj = std::make_unique<KernelEvent>();
    obj->handle    = handle;
    obj->type      = KernelObjectType::Event;
    obj->autoReset = !manualReset;
    obj->signaled  = initialState;
    std::lock_guard<std::mutex> lk(m_objectMutex);
    m_objects[handle] = std::move(obj);
    LOGI("THR: createEvent 0x%X manual=%d init=%d", handle, (int)manualReset, (int)initialState);
    return handle;
}

void HleKernel::setEvent(uint32_t handle) {
    auto* obj = getObject(handle);
    if (!obj || obj->type != KernelObjectType::Event) return;
    auto* ev = static_cast<KernelEvent*>(obj);
    std::lock_guard<std::mutex> lk(ev->m);
    ev->signaled = true;
    ev->cv.notify_all();
}

void HleKernel::resetEvent(uint32_t handle) {
    auto* obj = getObject(handle);
    if (!obj || obj->type != KernelObjectType::Event) return;
    auto* ev = static_cast<KernelEvent*>(obj);
    std::lock_guard<std::mutex> lk(ev->m);
    ev->signaled = false;
}

void HleKernel::pulseEvent(uint32_t handle) {
    setEvent(handle);
    resetEvent(handle);
}

// ─── Wait ─────────────────────────────────────────────────────────────────────

uint32_t HleKernel::waitForObject(uint32_t handle, bool alertable, uint64_t timeoutMs) {
    auto* obj = getObject(handle);
    if (!obj) return 0xC0000008; // STATUS_INVALID_HANDLE

    if (obj->type == KernelObjectType::Event) {
        auto* ev = static_cast<KernelEvent*>(obj);
        std::unique_lock<std::mutex> lk(ev->m);
        if (timeoutMs == UINT64_MAX) {
            ev->cv.wait(lk, [ev] { return ev->signaled; });
        } else {
            bool ok = ev->cv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                                      [ev] { return ev->signaled; });
            if (!ok) return 0x00000102; // STATUS_TIMEOUT
        }
        if (ev->autoReset) ev->signaled = false;
        return 0; // STATUS_WAIT_0

    } else if (obj->type == KernelObjectType::Mutex) {
        auto* m = static_cast<KernelMutex*>(obj);
        if (timeoutMs == UINT64_MAX) {
            m->m.lock();
        } else {
            // Try-lock with timeout not directly supported; approximate
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            while (!m->m.try_lock()) {
                if (std::chrono::steady_clock::now() >= deadline)
                    return 0x00000102;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        m->ownerThread = 1;
        m->recursionCount++;
        return 0;
    }

    return 0; // Default: signaled
}

uint32_t HleKernel::waitForMultiple(const std::vector<uint32_t>& handles, bool waitAll,
                                     bool alertable, uint64_t timeoutMs) {
    // Simplified: wait for each handle sequentially
    for (size_t i = 0; i < handles.size(); i++) {
        uint32_t result = waitForObject(handles[i], alertable, waitAll ? timeoutMs : 0);
        if (!waitAll && result == 0) return (uint32_t)i; // STATUS_WAIT_i
        if (result == 0x00000102 && !waitAll) continue;
    }
    return 0;
}

} // namespace hle
} // namespace x360
