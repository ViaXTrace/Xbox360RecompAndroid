/**
 * JIT Engine — main run loop, block cache dispatch, threading.
 * Manages up to 6 Xenon hardware threads as Android native threads.
 * References: xenia cpu/thread_state.cc, Xenia JIT dispatch loop.
 */
#include "../../include/jit/jit_engine.h"
#include "../../include/jit/ir.h"
#include <android/log.h>
#include <sys/mman.h>
#include <signal.h>
#include <cstring>
#include <cassert>
#include <chrono>

#define LOG_TAG "X360:JIT"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace jit {

// Thread-local current context pointer (for SIGSEGV handler)
static thread_local PPCContext* t_currentCtx = nullptr;
static JitEngine*               g_engineInstance = nullptr;

JitEngine::JitEngine() {
    g_engineInstance = this;
}

JitEngine::~JitEngine() {
    stop();
    if (m_codeArena && m_codeArena != MAP_FAILED) {
        munmap(m_codeArena, m_codeArenaSize);
        m_codeArena = nullptr;
    }
    g_engineInstance = nullptr;
}

bool JitEngine::init(uint8_t* guestMemory, uint64_t guestMemorySize,
                     uint64_t guestBase, uint64_t entryPoint) {
    m_guestMemory     = guestMemory;
    m_guestMemorySize = guestMemorySize;
    m_guestBase       = guestBase;
    m_entryPoint      = entryPoint;

    // Allocate JIT code arena (64 MB, PROT_EXEC)
    m_codeArena = (uint8_t*)mmap(nullptr, m_codeArenaSize,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m_codeArena == MAP_FAILED) {
        LOGE("Failed to allocate JIT code arena: %s", strerror(errno));
        m_codeArena = nullptr;
        return false;
    }
    m_codeArenaOffset.store(0);
    LOGI("JIT: code arena @ %p (%zu MB)", m_codeArena, m_codeArenaSize >> 20);

    // Install SIGSEGV handler for self-modifying code detection
    struct sigaction sa{};
    sa.sa_flags   = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = segvHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);

    m_state.store(JitState::Stopped);
    LOGI("JIT: initialized, entry=0x%llX base=0x%llX",
         (unsigned long long)entryPoint, (unsigned long long)guestBase);
    return true;
}

bool JitEngine::start(int threadCount) {
    if (m_state.load() != JitState::Stopped) return false;
    if (threadCount < 1) threadCount = 1;
    if (threadCount > 6) threadCount = 6;

    m_state.store(JitState::Running);
    m_pauseRequested.store(false);

    // Initialize per-thread contexts
    m_threadContexts.resize(threadCount);
    for (int i = 0; i < threadCount; i++) {
        memset(&m_threadContexts[i], 0, sizeof(PPCContext));
        m_threadContexts[i].threadIndex = i;
        // Thread 0 starts at the entry point; others wait for ExCreateThread
        m_threadContexts[i].pc = (i == 0) ? m_entryPoint : 0;
        // r1 = stack pointer (guest stack top — each thread gets 256 KB)
        uint64_t stackBase = 0x7F000000ULL - (uint64_t)i * 0x40000ULL;
        m_threadContexts[i].gpr[1] = stackBase + 0x40000ULL - 0x100ULL;
    }

    LOGI("JIT: starting %d threads", threadCount);

    m_threads.clear();
    for (int i = 0; i < threadCount; i++) {
        uint64_t startPc = m_threadContexts[i].pc;
        m_threads.emplace_back([this, i, startPc]() {
            threadLoop(i, startPc);
        });
    }
    return true;
}

void JitEngine::pause() {
    m_pauseRequested.store(true);
}

void JitEngine::resume() {
    m_pauseRequested.store(false);
}

void JitEngine::stop() {
    if (m_state.load() == JitState::Stopped) return;
    m_state.store(JitState::Stopped);
    for (auto& t : m_threads) {
        if (t.joinable()) t.join();
    }
    m_threads.clear();
    LOGI("JIT: all threads stopped");
}

void JitEngine::registerHle(const std::string& module, uint32_t ordinal, HleHandler handler) {
    // Key = hash of (module name + ordinal)
    uint64_t key = std::hash<std::string>{}(module) ^ ((uint64_t)ordinal << 32);
    m_hleHandlers[key] = std::move(handler);
    LOGI("JIT: HLE registered %s:0x%04X", module.c_str(), ordinal);
}

void JitEngine::invalidateRange(uint64_t guestAddr, size_t size) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_blockCache.begin();
    while (it != m_blockCache.end()) {
        uint64_t blockPc = it->first;
        if (blockPc >= guestAddr && blockPc < guestAddr + size) {
            it->second->isValid = false;
            it = m_blockCache.erase(it);
        } else {
            ++it;
        }
    }
}

// ─── Thread run loop ──────────────────────────────────────────────────────────

void JitEngine::threadLoop(int threadIndex, uint64_t startPc) {
    PPCContext& ctx = m_threadContexts[threadIndex];
    t_currentCtx = &ctx;
    ctx.pc = startPc;

    LOGI("JIT: thread %d starting at 0x%llX", threadIndex, (unsigned long long)startPc);

    auto lastFpsTime = std::chrono::steady_clock::now();
    uint32_t frameCount = 0;

    while (m_state.load() == JitState::Running) {
        if (m_pauseRequested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        if (ctx.pc == 0) {
            // Thread waiting for work (secondary threads before ExCreateThread)
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        // Check if PC is in HLE range (0x80000000+ kernel module addresses)
        // These are detected as HLE imports by the loader.
        // The JIT sets PC to a special sentinel when it hits an HLE thunk.
        if ((ctx.pc & 0xFF000000) == 0x80000000) {
            // HLE call: dispatch via registered handlers
            uint32_t ordinal = (uint32_t)(ctx.pc & 0xFFFF);
            std::string module = (ctx.pc < 0x80100000) ? "xboxkrnl.exe" : "xam.xex";
            uint64_t key = std::hash<std::string>{}(module) ^ ((uint64_t)ordinal << 32);
            auto handlerIt = m_hleHandlers.find(key);
            if (handlerIt != m_hleHandlers.end()) {
                handlerIt->second(ctx, ordinal);
            }
            // Return: PC = LR
            ctx.pc = ctx.lr;
            continue;
        }

        // Translate guest PC to host memory pointer
        uint64_t pcOffset = ctx.pc - m_guestBase;
        if (pcOffset >= m_guestMemorySize) {
            LOGE("JIT: thread %d PC 0x%llX out of range", threadIndex, (unsigned long long)ctx.pc);
            m_state.store(JitState::Error);
            break;
        }

        CompiledBlock* block = getOrCompile(ctx.pc, ctx);
        if (!block || !block->isValid) {
            // Could not compile — advance by 4 (NOP behavior)
            ctx.pc += 4;
            continue;
        }

        executeBlock(block, ctx);
        m_instrExecuted.fetch_add(block->nativeSize / 4, std::memory_order_relaxed);

        // FPS tracking (count "frames" as 60Hz ticks from GPU swap)
        auto now = std::chrono::steady_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(now - lastFpsTime).count();
        if (elapsedMs >= 1000.0) {
            m_fps.store(frameCount * 1000.0 / elapsedMs);
            m_frameTimeMs.store(elapsedMs / std::max(1u, frameCount));
            frameCount = 0;
            lastFpsTime = now;
        }
    }

    LOGI("JIT: thread %d exited", threadIndex);
}

// ─── Block cache ──────────────────────────────────────────────────────────────

CompiledBlock* JitEngine::getOrCompile(uint64_t guestPc, PPCContext& ctx) {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_blockCache.find(guestPc);
        if (it != m_blockCache.end() && it->second->isValid) {
            return it->second.get();
        }
    }
    return compileBlock(guestPc, ctx);
}

// Forward declaration (from arm64_backend.cpp)
extern size_t jitCompileBlock(const IrBlock& block, uint8_t* out, size_t maxBytes,
                               uint8_t* guestMemory, uint64_t guestBase);
extern IrBlock buildIrBlock(const uint8_t* guestMemory, uint64_t guestPc,
                             uint64_t guestBase, uint64_t guestMemorySize);

CompiledBlock* JitEngine::compileBlock(uint64_t guestPc, PPCContext& ctx) {
    uint64_t pcOffset = guestPc - m_guestBase;
    if (pcOffset + 4 > m_guestMemorySize) return nullptr;

    // Build IR block
    IrBlock irBlock = buildIrBlock(m_guestMemory, guestPc, m_guestBase, m_guestMemorySize);
    if (irBlock.instrs.empty()) return nullptr;

    // Allocate native code buffer
    size_t maxNativeSize = irBlock.instrs.size() * 32; // max ~8 ARM64 instrs per PPC instr
    size_t arenaOffset = m_codeArenaOffset.fetch_add(maxNativeSize);
    if (arenaOffset + maxNativeSize > m_codeArenaSize) {
        LOGE("JIT: code arena exhausted");
        return nullptr;
    }

    uint8_t* nativePtr = m_codeArena + arenaOffset;
    size_t nativeSize = jitCompileBlock(irBlock, nativePtr, maxNativeSize,
                                        m_guestMemory, m_guestBase);

    flushICache(nativePtr, nativeSize);

    auto block = std::make_unique<CompiledBlock>();
    block->guestPc    = guestPc;
    block->nativeCode = nativePtr;
    block->nativeSize = nativeSize;
    block->isValid    = true;

    CompiledBlock* rawPtr = block.get();
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_blockCache[guestPc] = std::move(block);
    }
    m_blocksCompiled.fetch_add(1, std::memory_order_relaxed);
    return rawPtr;
}

void JitEngine::executeBlock(CompiledBlock* block, PPCContext& ctx) {
    // Call the compiled native code.
    // The compiled block expects:
    //   x28 = &ctx (PPCContext*)
    // and returns via RET (updating ctx.pc to next guest PC).
    using BlockFn = void(*)(PPCContext*);
    auto fn = reinterpret_cast<BlockFn>(block->nativeCode);
    fn(&ctx);
}

// ─── JIT arena allocation ─────────────────────────────────────────────────────

uint8_t* JitEngine::allocJitPage(size_t size) {
    size_t offset = m_codeArenaOffset.fetch_add(size);
    if (offset + size > m_codeArenaSize) return nullptr;
    return m_codeArena + offset;
}

// ─── SIGSEGV handler (self-modifying code detection) ─────────────────────────

void JitEngine::segvHandler(int /*sig*/, siginfo_t* info, void* /*uctx*/) {
    void* faultAddr = info->si_addr;
    if (!g_engineInstance) return;

    // Check if the fault is in guest code pages
    uint8_t* gm = g_engineInstance->m_guestMemory;
    size_t   gs = g_engineInstance->m_guestMemorySize;
    if ((uint8_t*)faultAddr >= gm && (uint8_t*)faultAddr < gm + gs) {
        uint64_t guestAddr = (uint64_t)((uint8_t*)faultAddr - gm) + g_engineInstance->m_guestBase;
        g_engineInstance->invalidateRange(guestAddr & ~0xFFFULL, 0x1000);
        // Re-protect as RW so the write can proceed
        mprotect((void*)((uint64_t)faultAddr & ~0xFFF), 0x1000, PROT_READ | PROT_WRITE);
    }
}

} // namespace jit
} // namespace x360
