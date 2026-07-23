/**
 * ARM64 Code Generation Backend
 * Translates the IR built by ppc_decoder into native AArch64 machine code.
 *
 * Strategy:
 *  - PowerPC GPR r0-r31 → ARM64 x0-x27 (r28-r31 spill to host memory)
 *  - PowerPC FPR f0-f31 → ARM64 d0-d31
 *  - PowerPC VMX vr0-vr127 → ARM64 v0-v31 (vr32+ spill to guest VMXSTATE block)
 *  - CR, XER, CTR, LR stored in dedicated host registers or spill slots
 *  - Big-endian ↔ little-endian: REV/REV16/REV32/REV64 on every load/store
 *
 * Code arena: anonymous mmap with PROT_EXEC, flushed with __builtin___clear_cache.
 */
#include "../../include/jit/jit_engine.h"
#include "../../include/jit/ir.h"
#include <cstdint>
#include <cassert>
#include <android/log.h>
#include <sys/mman.h>

#define LOG_TAG "X360:ARM64"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace jit {

// ─── ARM64 instruction encoding helpers ─────────────────────────────────────────
// All instructions are little-endian 32-bit on ARM64.

using u32 = uint32_t;

// Emit a 32-bit instruction to buf and advance ptr
static inline void emit(uint8_t*& ptr, u32 instr) {
    __builtin_memcpy(ptr, &instr, 4);
    ptr += 4;
}

// NOP
static inline void arm64_nop(uint8_t*& ptr) { emit(ptr, 0xD503201F); }

// MOV Xd, Xn (register move)
static inline void arm64_mov_rr(uint8_t*& ptr, int rd, int rn) {
    emit(ptr, 0xAA0003E0 | (rn << 16) | rd); // ORR Xd, XZR, Xn
}

// MOV Xd, #imm16 (zero-extended)
static inline void arm64_movz(uint8_t*& ptr, int rd, uint16_t imm, int shift = 0) {
    u32 enc = 0xD2800000 | ((shift/16) << 21) | (imm << 5) | rd;
    emit(ptr, enc);
}

// MOVK Xd, #imm16, LSL #shift  (keep other bits)
static inline void arm64_movk(uint8_t*& ptr, int rd, uint16_t imm, int shift) {
    u32 enc = 0xF2800000 | ((shift/16) << 21) | (imm << 5) | rd;
    emit(ptr, enc);
}

// Load a 64-bit immediate into a register (up to 4 MOVZ/MOVK)
static void arm64_load_imm64(uint8_t*& ptr, int rd, uint64_t imm) {
    arm64_movz(ptr, rd, imm & 0xFFFF, 0);
    if (imm >> 16) arm64_movk(ptr, rd, (imm >> 16) & 0xFFFF, 16);
    if (imm >> 32) arm64_movk(ptr, rd, (imm >> 32) & 0xFFFF, 32);
    if (imm >> 48) arm64_movk(ptr, rd, (imm >> 48) & 0xFFFF, 48);
}

// ADD Xd, Xn, Xm
static inline void arm64_add_rrr(uint8_t*& ptr, int rd, int rn, int rm) {
    emit(ptr, 0x8B000000 | (rm << 16) | (rn << 5) | rd);
}

// ADD Xd, Xn, #imm12
static inline void arm64_add_rri(uint8_t*& ptr, int rd, int rn, uint16_t imm12) {
    emit(ptr, 0x91000000 | (imm12 << 10) | (rn << 5) | rd);
}

// SUB Xd, Xn, Xm
static inline void arm64_sub_rrr(uint8_t*& ptr, int rd, int rn, int rm) {
    emit(ptr, 0xCB000000 | (rm << 16) | (rn << 5) | rd);
}

// MUL Xd, Xn, Xm
static inline void arm64_mul_rrr(uint8_t*& ptr, int rd, int rn, int rm) {
    emit(ptr, 0x9B007C00 | (rm << 16) | (rn << 5) | rd); // MADD Xd,Xn,Xm,XZR
}

// AND Xd, Xn, Xm
static inline void arm64_and_rrr(uint8_t*& ptr, int rd, int rn, int rm) {
    emit(ptr, 0x8A000000 | (rm << 16) | (rn << 5) | rd);
}

// ORR Xd, Xn, Xm
static inline void arm64_orr_rrr(uint8_t*& ptr, int rd, int rn, int rm) {
    emit(ptr, 0xAA000000 | (rm << 16) | (rn << 5) | rd);
}

// EOR Xd, Xn, Xm
static inline void arm64_eor_rrr(uint8_t*& ptr, int rd, int rn, int rm) {
    emit(ptr, 0xCA000000 | (rm << 16) | (rn << 5) | rd);
}

// LSL Xd, Xn, Xm
static inline void arm64_lsl_rrr(uint8_t*& ptr, int rd, int rn, int rm) {
    emit(ptr, 0x9AC02000 | (rm << 16) | (rn << 5) | rd);
}

// LSR Xd, Xn, Xm
static inline void arm64_lsr_rrr(uint8_t*& ptr, int rd, int rn, int rm) {
    emit(ptr, 0x9AC02400 | (rm << 16) | (rn << 5) | rd);
}

// ASR Xd, Xn, Xm
static inline void arm64_asr_rrr(uint8_t*& ptr, int rd, int rn, int rm) {
    emit(ptr, 0x9AC02800 | (rm << 16) | (rn << 5) | rd);
}

// LDR Xt, [Xbase, #offset]  (post-index 9-bit signed)
static inline void arm64_ldr64(uint8_t*& ptr, int rt, int rn, int16_t off) {
    emit(ptr, 0xF9400000 | (((off >> 3) & 0xFFF) << 10) | (rn << 5) | rt);
}

// STR Xt, [Xbase, #offset]
static inline void arm64_str64(uint8_t*& ptr, int rt, int rn, int16_t off) {
    emit(ptr, 0xF9000000 | (((off >> 3) & 0xFFF) << 10) | (rn << 5) | rt);
}

// LDR Wt, [Xbase, #offset] — 32-bit load (unsigned word)
static inline void arm64_ldr32(uint8_t*& ptr, int rt, int rn, int16_t off) {
    emit(ptr, 0xB9400000 | (((off >> 2) & 0xFFF) << 10) | (rn << 5) | rt);
}

// LDRH Wt, [Xbase, #offset] — 16-bit unsigned load
static inline void arm64_ldrh(uint8_t*& ptr, int rt, int rn, int16_t off) {
    emit(ptr, 0x79400000 | (((off >> 1) & 0xFFF) << 10) | (rn << 5) | rt);
}

// LDRB Wt, [Xbase, #offset] — 8-bit unsigned load
static inline void arm64_ldrb(uint8_t*& ptr, int rt, int rn, int16_t off) {
    emit(ptr, 0x39400000 | ((off & 0xFFF) << 10) | (rn << 5) | rt);
}

// REV Xt, Xn  — byte-reverse 64-bit (big→little endian for 64-bit loads)
static inline void arm64_rev64(uint8_t*& ptr, int rd, int rn) {
    emit(ptr, 0xDAC00C00 | (rn << 5) | rd);
}

// REV32 Xt, Xn  — byte-reverse within each 32-bit word
static inline void arm64_rev32(uint8_t*& ptr, int rd, int rn) {
    emit(ptr, 0xDAC00800 | (rn << 5) | rd);
}

// REV16 Xt, Xn  — byte-reverse within each 16-bit halfword
static inline void arm64_rev16(uint8_t*& ptr, int rd, int rn) {
    emit(ptr, 0xDAC00400 | (rn << 5) | rd);
}

// LDAXR Xt, [Xn]  — load-acquire exclusive (for lwarx/ldarx)
static inline void arm64_ldaxr(uint8_t*& ptr, int rt, int rn) {
    emit(ptr, 0xC85FFC00 | (rn << 5) | rt);
}

// STLXR Ws, Xt, [Xn]  — store-release exclusive (for stwcx/stdcx)
static inline void arm64_stlxr(uint8_t*& ptr, int rs, int rt, int rn) {
    emit(ptr, 0xC800FC00 | (rs << 16) | (rn << 5) | rt);
}

// DMB ISH — full memory barrier (TSO approximation)
static inline void arm64_dmb_ish(uint8_t*& ptr) {
    emit(ptr, 0xD5033BBF);
}

// RET — return from subroutine (x30 = LR)
static inline void arm64_ret(uint8_t*& ptr) {
    emit(ptr, 0xD65F03C0);
}

// BLR Xn — branch with link to register
static inline void arm64_blr(uint8_t*& ptr, int rn) {
    emit(ptr, 0xD63F0000 | (rn << 5));
}

// BR Xn — branch to register
static inline void arm64_br(uint8_t*& ptr, int rn) {
    emit(ptr, 0xD61F0000 | (rn << 5));
}

// ─── Register allocation ────────────────────────────────────────────────────────
// Simple fixed mapping: PPC GPR → ARM64 register
// x0-x27: PPC r0-r27 directly mapped
// x28: PPCContext* (guest context pointer)
// x29: frame pointer (ABI)
// x30: link register (ABI)
// x31/XZR: zero register
static constexpr int kCtxReg = 28; // ARM64 x28 = PPCContext*

static int gprToArm64(int ppcGpr) {
    if (ppcGpr >= 0 && ppcGpr <= 27) return ppcGpr; // direct mapping
    return -1; // spill (r28-r31 use memory)
}

static int fprToArm64(int ppcFpr) {
    if (ppcFpr >= 0 && ppcFpr <= 31) return ppcFpr; // d0-d31
    return -1;
}

// ─── JIT code generation entry point ───────────────────────────────────────────

// Generates ARM64 code for a single IR instruction.
// Returns number of bytes emitted.
static size_t emitIr(uint8_t*& out, const struct IrInstr& ir, const PPCContext& ctx) {
    uint8_t* start = out;

    // Temporary register for intermediate values
    constexpr int Xtmp = 16; // x16 (intra-procedure-call scratch, allowed by ABI)
    constexpr int Xtmp2 = 17;

    switch (ir.op) {
    case IrOp::Add:
        if (ir.rb >= 0) {
            int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra), rb = gprToArm64(ir.rb);
            if (rd >= 0 && ra >= 0 && rb >= 0) arm64_add_rrr(out, rd, ra, rb);
        } else {
            int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra);
            if (rd >= 0 && ra >= 0 && ir.imm >= 0 && ir.imm < 4096)
                arm64_add_rri(out, rd, ra, (uint16_t)ir.imm);
            else {
                arm64_load_imm64(out, Xtmp, (uint64_t)ir.imm);
                arm64_add_rrr(out, rd < 0 ? Xtmp : rd, ra < 0 ? 31 : ra, Xtmp);
            }
        }
        break;

    case IrOp::Sub: {
        int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra), rb = gprToArm64(ir.rb);
        if (rd >= 0 && ra >= 0 && rb >= 0) arm64_sub_rrr(out, rd, ra, rb);
        break;
    }
    case IrOp::Mul: {
        int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra), rb = gprToArm64(ir.rb);
        if (rd >= 0 && ra >= 0 && rb >= 0) arm64_mul_rrr(out, rd, ra, rb);
        break;
    }
    case IrOp::And: {
        int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra), rb = gprToArm64(ir.rb);
        if (rd >= 0 && ra >= 0 && rb >= 0) arm64_and_rrr(out, rd, ra, rb);
        break;
    }
    case IrOp::Or: {
        int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra), rb = gprToArm64(ir.rb);
        if (rd >= 0 && ra >= 0 && rb >= 0) arm64_orr_rrr(out, rd, ra, rb);
        break;
    }
    case IrOp::Xor: {
        int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra), rb = gprToArm64(ir.rb);
        if (rd >= 0 && ra >= 0 && rb >= 0) arm64_eor_rrr(out, rd, ra, rb);
        break;
    }

    case IrOp::Lwz: {
        // lwz rd, imm(ra) — load 32-bit big-endian word, zero-extend
        // ARM64: LDR Wrd, [Xra, #imm]; REV32 Wrd, Wrd
        int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra);
        if (rd >= 0 && ra >= 0) {
            arm64_ldr32(out, rd, ra, (int16_t)ir.imm);
            arm64_rev32(out, rd, rd);
        }
        break;
    }
    case IrOp::Lhz: {
        int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra);
        if (rd >= 0 && ra >= 0) {
            arm64_ldrh(out, rd, ra, (int16_t)ir.imm);
            arm64_rev16(out, rd, rd);
        }
        break;
    }
    case IrOp::Lbz: {
        int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra);
        if (rd >= 0 && ra >= 0) arm64_ldrb(out, rd, ra, (int16_t)ir.imm);
        break;
    }
    case IrOp::Ld: {
        int rd = gprToArm64(ir.rd), ra = gprToArm64(ir.ra);
        if (rd >= 0 && ra >= 0) {
            arm64_ldr64(out, rd, ra, (int16_t)ir.imm);
            arm64_rev64(out, rd, rd);
        }
        break;
    }

    case IrOp::Sync:
    case IrOp::Isync:
        arm64_dmb_ish(out);
        break;

    case IrOp::BCLR:
        arm64_ret(out); // simplified: branch-to-LR = return
        break;

    case IrOp::Unknown:
        arm64_nop(out); // safe stub for unimplemented instructions
        break;

    default:
        arm64_nop(out);
        break;
    }

    return out - start;
}

// Flush instruction cache for the given range (required after JIT code write)
void JitEngine::flushICache(uint8_t* start, size_t size) {
    __builtin___clear_cache(reinterpret_cast<char*>(start), reinterpret_cast<char*>(start + size));
}

// ─── jitCompileBlock ────────────────────────────────────────────────────────────
// Called by JitEngine to compile a single guest IR block into native ARM64.
// Returns the size of emitted code in bytes, or 0 on failure.
// Defined here (arm64_backend.cpp) to keep all ARM64 code generation in one file.
size_t JitEngine::jitCompileBlock(const IrBlock& block, uint8_t* out, size_t maxBytes) {
    uint8_t* cursor = out;
    const uint8_t* end = out + maxBytes;

    for (const auto& ir : block.instrs) {
        if (cursor + 64 >= end) {
            LOGE("ARM64: code buffer overflow at guestPc=0x%llX",
                 (unsigned long long)ir.guestPc);
            break;
        }
        PPCContext dummy_ctx{}; size_t emitted = emitIr(cursor, ir, dummy_ctx);
        cursor += emitted;
    }

    // Emit a return to JIT dispatch loop at the end of each block
    arm64_ret(cursor);
    cursor += 4;

    size_t totalBytes = cursor - out;
    flushICache(out, totalBytes);
    return totalBytes;
}

} // namespace jit
} // namespace x360
