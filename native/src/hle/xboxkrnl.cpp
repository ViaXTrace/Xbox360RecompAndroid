/**
 * xboxkrnl.exe HLE dispatch — implements the Xbox 360 kernel exports by ordinal.
 * Reference: Xenia kernel exports (MIT) — architecture reference only, all code written from scratch.
 *
 * Ordinals match the Xbox 360 xboxkrnl.exe export table (documented in
 * Xenia's xboxkrnl_*.cc files and the Xbox 360 SDK).
 */
#include "../../include/hle/hle_kernel.h"
#include "../../include/jit/jit_engine.h"
#include <cstring>
#include <cstdlib>
#include <android/log.h>

#define LOG_TAG "X360:KRNL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Helper: read guest memory (big-endian) 
static inline uint32_t guestRead32(const uint8_t* gm, uint64_t guestAddr, uint64_t guestBase) {
    uint64_t off = guestAddr - guestBase;
    uint32_t v; __builtin_memcpy(&v, gm + off, 4);
    return __builtin_bswap32(v);
}
static inline void guestWrite32(uint8_t* gm, uint64_t guestAddr, uint64_t guestBase, uint32_t val) {
    uint64_t off = guestAddr - guestBase;
    val = __builtin_bswap32(val);
    __builtin_memcpy(gm + off, &val, 4);
}
static inline uint64_t guestRead64(const uint8_t* gm, uint64_t guestAddr, uint64_t guestBase) {
    uint64_t off = guestAddr - guestBase;
    uint64_t v; __builtin_memcpy(&v, gm + off, 8);
    return __builtin_bswap64(v);
}

// Forward declaration of XAM dispatch (defined in xam.cpp)
namespace x360 { namespace hle {
    void dispatchXamCall(HleKernel& kernel, uint32_t ordinal,
                         ::x360::jit::PPCContext& ctx,
                         uint8_t* gm, uint64_t gb);
} }


namespace x360 {
namespace hle {

// Forward declarations from audio.cpp
void hleSubmitSourceBuffer(const uint8_t*, uint32_t, uint32_t, uint32_t, bool, float, bool);
void hleSetMasterVolume(float volume);

// HleKernel::init — register path mappings, initialize subsystems
bool HleKernel::init(uint8_t* guestMemory, uint64_t guestBase,
                     const std::string& gameDir, const std::string& saveDir) {
    m_guestMemory = guestMemory;
    m_guestBase   = guestBase;
    m_gameDir     = gameDir;
    m_saveDir     = saveDir;

    // Set up Xbox 360 path → Android path mappings (lowercase key)
    m_pathMappings["game:\\"]  = gameDir;
    m_pathMappings["d:\\"]     = gameDir;
    m_pathMappings["hdd1:\\"]  = saveDir + "/hdd1";
    m_pathMappings["usb:\\"]   = saveDir + "/usb";
    m_pathMappings["cache:\\"] = saveDir + "/cache";
    m_pathMappings["devkit:\\"]= saveDir + "/devkit";

    LOGI("KRNL: init game='%s' save='%s'", gameDir.c_str(), saveDir.c_str());
    return true;
}

// Handle allocator
uint32_t HleKernel::allocHandle() {
    std::lock_guard<std::mutex> lk(m_objectMutex);
    return m_nextHandle++;
}

KernelObject* HleKernel::getObject(uint32_t handle) {
    std::lock_guard<std::mutex> lk(m_objectMutex);
    auto it = m_objects.find(handle);
    return (it != m_objects.end()) ? it->second.get() : nullptr;
}

void HleKernel::destroyObject(uint32_t handle) {
    std::lock_guard<std::mutex> lk(m_objectMutex);
    m_objects.erase(handle);
}

HleKernel::HleKernel() = default;
HleKernel::~HleKernel() = default;

// ─── Main kernel dispatch ─────────────────────────────────────────────────────
// Called by the JIT when it executes a HLE thunk.
// ctx.gpr[0] = ordinal (set by loader), ctx.gpr[3..] = function arguments (PPC ABI)
// Return value in ctx.gpr[3] (r3)

void HleKernel::dispatchKernelCall(const std::string& module, uint32_t ordinal,
                                    jit::PPCContext& ctx) {
    uint8_t* gm  = m_guestMemory;
    uint64_t gb  = m_guestBase;

    // PPC calling convention: r3-r10 are args, r3 is return value
    auto& r = ctx.gpr;

    if (module == "xboxkrnl.exe") {
        switch (ordinal) {
        // ── Memory ───────────────────────────────────────────────────────────
        case 184: // NtAllocateVirtualMemory(ProcessHandle, BaseAddress*, ZeroBits, RegionSize*, AllocType, Protect)
        {
            uint64_t baseAddrPtr = r[4];
            uint64_t sizePtr     = r[6];
            uint32_t allocType   = (uint32_t)r[7];
            uint32_t protect     = (uint32_t)r[8];
            uint64_t reqBase = (baseAddrPtr != 0) ? guestRead64(gm, baseAddrPtr, gb) : 0;
            uint64_t reqSize = (sizePtr != 0)     ? guestRead64(gm, sizePtr, gb) : 0;
            uint64_t addr = allocVirtualMemory(reqBase, reqSize, allocType, protect);
            if (baseAddrPtr) guestWrite32(gm, baseAddrPtr, gb, (uint32_t)addr);
            r[3] = (addr != 0) ? 0 : 0xC0000017; // STATUS_NO_MEMORY
            break;
        }
        case 186: // NtFreeVirtualMemory
        {
            uint64_t basePtr = r[4];
            uint64_t addr = (basePtr != 0) ? guestRead32(gm, basePtr, gb) : 0;
            freeVirtualMemory(addr);
            r[3] = 0;
            break;
        }
        case 368: // XMemAlloc(Size, Params)
        {
            uint64_t size   = r[3];
            uint64_t params = r[4];
            r[3] = xMemAlloc(size, params);
            break;
        }
        case 369: // XMemFree(BaseAddress, Params)
            xMemFree(r[3]);
            r[3] = 0;
            break;

        // ── Threading ─────────────────────────────────────────────────────────
        case 13: // ExCreateThread(Handle*, StackSize, StackBaseAddr*, Func, Param, Flags, Priority)
        {
            uint64_t handlePtr  = r[3];
            uint32_t stackSize  = (uint32_t)r[4];
            uint64_t startFunc  = r[6];
            uint64_t startParam = r[7];
            int priority = (int)(int32_t)r[9];
            uint32_t handle = createThread(startFunc, startParam, stackSize, priority, false);
            if (handlePtr) guestWrite32(gm, handlePtr, gb, handle);
            r[3] = (handle != 0) ? 0 : 0xC0000005; // STATUS_ACCESS_VIOLATION
            break;
        }
        case 14: // ExTerminateThread(ExitCode)
            terminateThread(0xFFFFFFFF, (uint32_t)r[3]); // current thread
            r[3] = 0;
            break;

        case 30: // NtCreateMutant(Handle*, Access, ObjAttr*, InitialOwner)
        {
            uint64_t handlePtr = r[3];
            bool initOwner = r[6] != 0;
            uint32_t handle = createMutex(initOwner);
            if (handlePtr) guestWrite32(gm, handlePtr, gb, handle);
            r[3] = 0;
            break;
        }
        case 108: // NtReleaseMutant(Handle, PrevCount*)
            releaseMutex((uint32_t)r[3]);
            r[3] = 0;
            break;

        case 39: // NtCreateSemaphore(Handle*, Access, ObjAttr*, InitCount, MaxCount)
        {
            uint64_t handlePtr = r[3];
            int initCount = (int)r[5];
            int maxCount  = (int)r[6];
            uint32_t handle = createSemaphore(initCount, maxCount);
            if (handlePtr) guestWrite32(gm, handlePtr, gb, handle);
            r[3] = 0;
            break;
        }
        case 109: // NtReleaseSemaphore(Handle, ReleaseCount, PrevCount*)
            releaseSemaphore((uint32_t)r[3], (int)r[4]);
            r[3] = 0;
            break;

        case 23: // NtCreateEvent(Handle*, Access, ObjAttr*, EventType, InitState)
        {
            uint64_t handlePtr = r[3];
            bool manualReset   = r[5] == 1;
            bool initState     = r[6] != 0;
            uint32_t handle = createEvent(manualReset, initState);
            if (handlePtr) guestWrite32(gm, handlePtr, gb, handle);
            r[3] = 0;
            break;
        }
        case 131: // NtSetEvent(Handle, PrevState*)
            setEvent((uint32_t)r[3]);
            r[3] = 0;
            break;
        case 80: // NtClearEvent / NtResetEvent
            resetEvent((uint32_t)r[3]);
            r[3] = 0;
            break;
        case 90: // NtPulseEvent
            pulseEvent((uint32_t)r[3]);
            r[3] = 0;
            break;
        case 152: // NtWaitForSingleObjectEx(Handle, WaitMode, Alertable, Timeout*)
        {
            uint32_t handle = (uint32_t)r[3];
            bool alertable  = r[5] != 0;
            uint64_t timeoutMs = (r[6] != 0) ? 30000 : UINT64_MAX; // TODO: parse guest timeout
            r[3] = waitForObject(handle, alertable, timeoutMs);
            break;
        }
        case 153: // NtWaitForMultipleObjectsEx
        {
            uint32_t count = (uint32_t)r[3];
            uint64_t handlesPtr = r[4];
            bool waitAll   = r[5] != 0;
            bool alertable = r[6] != 0;
            std::vector<uint32_t> handles;
            for (uint32_t i = 0; i < std::min(count, 64u); i++) {
                handles.push_back(guestRead32(gm, handlesPtr + i * 4, gb));
            }
            r[3] = waitForMultiple(handles, waitAll, alertable, 30000);
            break;
        }

        // ── File System ───────────────────────────────────────────────────────
        case 78: // NtCreateFile(Handle*, Access, ObjAttr*, IoStatus*, Alloc, FileAttr, Share, Disp, Options, EaBuffer, EaLen)
        {
            uint64_t handlePtr   = r[3];
            uint32_t access      = (uint32_t)r[4];
            uint64_t objAttrPtr  = r[5];
            // Object attributes contain a UNICODE_STRING with the path
            // Simplified: read path from a known offset
            uint64_t usPtr = (objAttrPtr != 0) ? guestRead32(gm, objAttrPtr + 8, gb) : 0;
            uint32_t usLen = (usPtr != 0) ? guestRead32(gm, usPtr, gb) >> 16 : 0;
            uint64_t usBuf = (usPtr != 0) ? guestRead32(gm, usPtr + 4, gb) : 0;
            // Convert UTF-16LE guest path to ASCII
            std::string guestPath;
            for (uint32_t i = 0; i < usLen && usBuf; i++) {
                uint16_t wc = guestRead32(gm, usBuf + i * 2, gb) & 0xFFFF;
                guestPath += (wc < 128) ? (char)wc : '?';
            }
            uint32_t disp  = (uint32_t)r[8];
            uint32_t share = (uint32_t)r[7];
            uint32_t handle = guestPath.empty() ? 0xFFFFFFFF :
                              createFile(guestPath, access, share, disp);
            if (handlePtr) guestWrite32(gm, handlePtr, gb, handle);
            r[3] = (handle != 0xFFFFFFFF) ? 0 : 0xC0000034; // STATUS_OBJECT_NAME_NOT_FOUND
            break;
        }
        case 101: // NtReadFile(Handle, Event, APC, ApcCtx, IoStatus, Buffer, Length, ByteOffset, Key)
        {
            uint32_t handle = (uint32_t)r[3];
            uint64_t bufPtr = r[7];
            uint32_t len    = (uint32_t)r[8];
            uint32_t bytesRead = 0;
            bool ok = readFile(handle, gm + bufPtr - gb, len, &bytesRead);
            r[3] = ok ? 0 : 0xC0000010; // STATUS_INVALID_DEVICE_REQUEST
            break;
        }
        case 149: // NtWriteFile
        {
            uint32_t handle = (uint32_t)r[3];
            uint64_t bufPtr = r[7];
            uint32_t len    = (uint32_t)r[8];
            uint32_t bytesWritten = 0;
            bool ok = writeFile(handle, gm + bufPtr - gb, len, &bytesWritten);
            r[3] = ok ? 0 : 0xC0000010;
            break;
        }
        case 58: // NtClose(Handle)
            closeHandle((uint32_t)r[3]);
            r[3] = 0;
            break;

        // ── Misc ──────────────────────────────────────────────────────────────
        case 1: // DbgPrint(Format, ...)
            LOGI("KRNL DbgPrint (guest)");
            r[3] = 0;
            break;
        case 322: // KeQuerySystemTime(CurrentTime*)
        {
            // Return current time as FILETIME (100ns intervals since 1601)
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            uint64_t filetime = ((uint64_t)ts.tv_sec + 11644473600ULL) * 10000000ULL
                              + ts.tv_nsec / 100;
            if (r[3]) {
                filetime = __builtin_bswap64(filetime);
                __builtin_memcpy(gm + r[3] - gb, &filetime, 8);
            }
            r[3] = 0;
            break;
        }
        case 323: // KeQueryPerformanceFrequency
            r[3] = 10000000ULL; // 10MHz
            break;
        case 338: // KeRaiseIrqlToDpcLevel → nop
        case 105: // KfAcquireSpinLock → nop
        case 106: // KfReleaseSpinLock → nop
        case 107: // KeAcquireSpinLock → nop
            r[3] = 0;
            break;
        case 428: // RtlInitializeCriticalSection → stub
        case 429: // RtlEnterCriticalSection → stub
        case 430: // RtlLeaveCriticalSection → stub
        case 431: // RtlDeleteCriticalSection → stub
            r[3] = 0;
            break;

        // ── XInput ────────────────────────────────────────────────────────────
        case 336: // XInputGetState(UserIndex, State*)
        {
            int pad = (int)r[3];
            uint64_t statePtr = r[4];
            r[3] = getInputState(pad, (statePtr > gb) ? gm + statePtr - gb : nullptr);
            break;
        }
        case 337: // XInputSetState(UserIndex, Vibration*)
            r[3] = 0; // TODO: map to Android Vibrator
            break;

        default:
            LOGI("KRNL: unimplemented xboxkrnl ordinal %u", ordinal);
            r[3] = 0;
            break;
        }
    } else if (module == "xam.xex") {
        dispatchXamCall(*this, ordinal, ctx, m_guestMemory, m_guestBase);
    }
}

} // namespace hle
} // namespace x360
