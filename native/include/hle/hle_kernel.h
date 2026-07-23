#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <functional>

namespace x360 {
namespace hle {

// ─── Kernel Object Types ───────────────────────────────────────────────────────
enum class KernelObjectType : uint32_t {
    Thread    = 0x01,
    Mutex     = 0x02,
    Semaphore = 0x03,
    Event     = 0x04,
    Timer     = 0x05,
    File      = 0x06,
    IoCompletion = 0x07,
};

struct KernelObject {
    uint32_t handle;
    KernelObjectType type;
    virtual ~KernelObject() = default;
};

// ─── Thread Object ─────────────────────────────────────────────────────────────
struct KernelThread : KernelObject {
    std::thread* hostThread = nullptr;
    uint64_t startPc = 0;
    uint64_t startParam = 0;
    int xenonThreadIndex = 0;
    bool running = false;
};

// ─── Mutex Object ──────────────────────────────────────────────────────────────
struct KernelMutex : KernelObject {
    std::mutex m;
    uint32_t ownerThread = 0;
    int recursionCount = 0;
};

// ─── Event Object ──────────────────────────────────────────────────────────────
struct KernelEvent : KernelObject {
    bool signaled = false;
    bool autoReset = true;
    std::mutex m;
    std::condition_variable cv;
};

// ─── File Object ───────────────────────────────────────────────────────────────
struct KernelFile : KernelObject {
    FILE* hostFile = nullptr;
    std::string guestPath;
    std::string hostPath;
    bool isDirectory = false;
};

// ─── HLE Kernel ────────────────────────────────────────────────────────────────
class HleKernel {
public:
    HleKernel();
    ~HleKernel();

    bool init(uint8_t* guestMemory, uint64_t guestBase,
              const std::string& gameDir, const std::string& saveDir);

    // ── Memory ──────────────────────────────────────────────────────────────────
    // NtAllocateVirtualMemory
    uint64_t allocVirtualMemory(uint64_t baseAddr, uint64_t size, uint32_t type, uint32_t protect);
    // NtFreeVirtualMemory
    void freeVirtualMemory(uint64_t baseAddr);
    // XMemAlloc
    uint64_t xMemAlloc(uint64_t size, uint64_t params);
    // XMemFree
    void xMemFree(uint64_t baseAddr);

    // ── Threading ───────────────────────────────────────────────────────────────
    uint32_t createThread(uint64_t startAddress, uint64_t startParam,
                          uint32_t stackSize, int priority, bool createSuspended);
    void terminateThread(uint32_t handle, uint32_t exitCode);
    uint32_t createMutex(bool initialOwner);
    void releaseMutex(uint32_t handle);
    uint32_t createSemaphore(int initialCount, int maxCount);
    void releaseSemaphore(uint32_t handle, int releaseCount);
    uint32_t createEvent(bool manualReset, bool initialState);
    void setEvent(uint32_t handle);
    void resetEvent(uint32_t handle);
    void pulseEvent(uint32_t handle);
    uint32_t waitForObject(uint32_t handle, bool alertable, uint64_t timeoutMs);
    uint32_t waitForMultiple(const std::vector<uint32_t>& handles, bool waitAll,
                             bool alertable, uint64_t timeoutMs);

    // ── File System ─────────────────────────────────────────────────────────────
    uint32_t createFile(const std::string& guestPath, uint32_t access,
                        uint32_t shareMode, uint32_t createDisposition);
    bool readFile(uint32_t handle, void* buffer, uint32_t size, uint32_t* bytesRead);
    bool writeFile(uint32_t handle, const void* buffer, uint32_t size, uint32_t* bytesWritten);
    void closeHandle(uint32_t handle);
    uint64_t getFileSize(uint32_t handle);
    bool setFilePointer(uint32_t handle, int64_t distanceToMove, uint32_t moveMethod);

    // ── Input ───────────────────────────────────────────────────────────────────
    void setInputState(int pad, uint32_t buttons, int16_t lx, int16_t ly,
                       int16_t rx, int16_t ry, uint8_t lt, uint8_t rt);
    uint32_t getInputState(int pad, void* outState); // returns XINPUT_GET_STATE result

    // ── Kernel exports (called from JIT via HLE trampoline) ─────────────────────
    // Dispatch a kernel call by module name + ordinal
    void dispatchKernelCall(const std::string& module, uint32_t ordinal,
                            struct PPCContext& ctx);

    const std::string& lastError() const { return m_lastError; }

private:
    // Guest path → host path mapping
    std::string resolveGuestPath(const std::string& guestPath) const;

    // Handle allocator
    uint32_t allocHandle();
    KernelObject* getObject(uint32_t handle);
    void destroyObject(uint32_t handle);

    uint8_t* m_guestMemory = nullptr;
    uint64_t m_guestBase = 0;
    std::string m_gameDir;
    std::string m_saveDir;

    // Object handle table
    std::unordered_map<uint32_t, std::unique_ptr<KernelObject>> m_objects;
    std::mutex m_objectMutex;
    uint32_t m_nextHandle = 0x0100;

    // Path mappings (Xbox path prefix → host dir)
    std::unordered_map<std::string, std::string> m_pathMappings;

    // XInput state (4 pads)
    struct XInputState {
        uint32_t buttons;
        int16_t lx, ly, rx, ry;
        uint8_t lt, rt;
        std::mutex m;
    } m_inputState[4];

    std::string m_lastError;
};

} // namespace hle
} // namespace x360
