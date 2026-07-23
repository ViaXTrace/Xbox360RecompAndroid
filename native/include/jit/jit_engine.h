#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>

namespace x360 {
namespace jit {

// PowerPC register file per-thread context
struct PPCContext {
    uint64_t gpr[32];       // General purpose registers r0-r31
    double   fpr[32];       // Floating point registers f0-f31
    uint8_t  vmx[128][16];  // VMX128 vector registers vr0-vr127 (16 bytes each)
    uint32_t cr;            // Condition register
    uint32_t xer;           // Fixed point exception register
    uint64_t ctr;           // Count register
    uint64_t lr;            // Link register
    uint32_t fpscr;         // Floating point status/control
    uint64_t pc;            // Program counter (current instruction)
    uint32_t msr;           // Machine state register
    int      threadIndex;   // Xenon thread index (0-5)
};

// Compiled block cache entry
struct CompiledBlock {
    uint64_t guestPc;           // Guest PowerPC address
    uint8_t* nativeCode;        // Pointer to native ARM64 code
    size_t   nativeSize;        // Size of native code
    uint32_t guestHash;         // Hash of guest instructions (for invalidation)
    bool     isValid;
};

// JIT engine state
enum class JitState {
    Stopped,
    Running,
    Paused,
    Error,
};

// Callback for HLE kernel calls
using HleHandler = std::function<void(PPCContext& ctx, uint32_t ordinal)>;

class JitEngine {
public:
    JitEngine();
    ~JitEngine();

    // Initialize the JIT engine for a loaded guest image.
    // baseAddress: where the image is mapped in host virtual memory
    // guestBase: the guest virtual base address (e.g. 0x82000000)
    // entryPoint: guest entry point address
    bool init(uint8_t* guestMemory, uint64_t guestMemorySize,
              uint64_t guestBase, uint64_t entryPoint);

    // Start emulation on N threads (1–6, matching Xenon cores)
    bool start(int threadCount = 4);

    // Pause / resume all threads
    void pause();
    void resume();

    // Stop and clean up all threads
    void stop();

    JitState state() const { return m_state.load(); }

    // Register HLE handler for a specific module + ordinal
    void registerHle(const std::string& module, uint32_t ordinal, HleHandler handler);

    // Invalidate compiled blocks covering the given guest address range
    void invalidateRange(uint64_t guestAddr, size_t size);

    // Performance counters
    uint64_t blocksCompiled() const { return m_blocksCompiled.load(); }
    uint64_t instructionsExecuted() const { return m_instrExecuted.load(); }
    double   currentFps() const { return m_fps.load(); }
    double   frameTimeMs() const { return m_frameTimeMs.load(); }

private:
    // Per-Xenon-thread run loop
    void threadLoop(int threadIndex, uint64_t startPc);

    // Look up or compile a block for the given guest PC
    CompiledBlock* getOrCompile(uint64_t guestPc, PPCContext& ctx);

    // Compile a single basic block starting at guestPc
    CompiledBlock* compileBlock(uint64_t guestPc, PPCContext& ctx);

    // Execute a compiled block
    void executeBlock(CompiledBlock* block, PPCContext& ctx);

    // Handle SIGSEGV from guest code (self-modifying code detection)
    static void segvHandler(int sig, siginfo_t* info, void* uctx);

    // JIT code arena management
    uint8_t* allocJitPage(size_t size);
    void     flushICache(uint8_t* start, size_t size);

    // Guest memory
    uint8_t* m_guestMemory = nullptr;
    uint64_t m_guestMemorySize = 0;
    uint64_t m_guestBase = 0;
    uint64_t m_entryPoint = 0;

    // Block cache: guest PC → compiled block
    std::unordered_map<uint64_t, std::unique_ptr<CompiledBlock>> m_blockCache;
    std::mutex m_cacheMutex;

    // JIT code arena (executable memory)
    uint8_t* m_codeArena = nullptr;
    size_t   m_codeArenaSize = 64 * 1024 * 1024; // 64 MB
    std::atomic<size_t> m_codeArenaOffset{0};

    // HLE handlers
    std::unordered_map<uint64_t, HleHandler> m_hleHandlers; // key: module+ordinal hash

    // Per-thread contexts (up to 6 Xenon threads)
    std::vector<PPCContext> m_threadContexts;
    std::vector<std::thread> m_threads;

    std::atomic<JitState> m_state{JitState::Stopped};
    std::atomic<bool> m_pauseRequested{false};

    // Metrics
    std::atomic<uint64_t> m_blocksCompiled{0};
    std::atomic<uint64_t> m_instrExecuted{0};
    std::atomic<double>   m_fps{0.0};
    std::atomic<double>   m_frameTimeMs{0.0};
};

} // namespace jit
} // namespace x360
