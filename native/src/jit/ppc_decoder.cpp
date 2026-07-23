/**
 * PowerPC Xenon Instruction Decoder
 * Decodes PPC 2.02 + VMX128 + Xenon extensions into an IR form.
 * Reference: xenia ppc_decode_data.cc, IBM PowerPC 2.02 ISA
 */
#include "../../include/jit/jit_engine.h"
#include "../../include/jit/ir.h"
#include <android/log.h>
#include <cstdint>
#include <cstring>

#define LOG_TAG "X360:JIT"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace jit {

// ─── PowerPC instruction field extraction ──────────────────────────────────────
static inline int     PPC_RA(uint32_t i)  { return (i >> 16) & 0x1F; }
static inline int     PPC_RB(uint32_t i)  { return (i >> 11) & 0x1F; }
static inline int     PPC_RC(uint32_t i)  { return (i >>  6) & 0x1F; }
static inline int     PPC_RD(uint32_t i)  { return (i >> 21) & 0x1F; }
static inline int     PPC_RS(uint32_t i)  { return (i >> 21) & 0x1F; }
static inline int     PPC_VD(uint32_t i)  { return (i >> 21) & 0x7F; } // VMX128 7-bit VD
static inline int     PPC_VA(uint32_t i)  { return (i >> 16) & 0x7F; } // VMX128 7-bit VA
static inline int     PPC_VB(uint32_t i)  { return (i >> 11) & 0x7F; } // VMX128 7-bit VB
static inline int     PPC_VC(uint32_t i)  { return (i >>  6) & 0x7F; } // VMX128 7-bit VC
static inline int32_t PPC_SIMM16(uint32_t i) { return (int32_t)(int16_t)(i & 0xFFFF); }
static inline uint32_t PPC_UIMM16(uint32_t i){ return i & 0xFFFF; }
static inline uint32_t PPC_PRIMARY(uint32_t i){ return i >> 26; }
static inline uint32_t PPC_SECONDARY(uint32_t i){ return (i >> 1) & 0x3FF; }
static inline uint32_t PPC_SEC21(uint32_t i)  { return (i >> 1) & 0x1FF; }
static inline int      PPC_BI(uint32_t i)  { return (i >> 16) & 0x1F; }
static inline int      PPC_BO(uint32_t i)  { return (i >> 21) & 0x1F; }
static inline int32_t  PPC_BD(uint32_t i)  { return (int32_t)((int16_t)(i & 0xFFFC)); }
static inline int32_t  PPC_LI(uint32_t i)  {
    int32_t li = (i >> 2) & 0xFFFFFF;
    return (li << 8) >> 8; // sign extend 24-bit
}
static inline bool PPC_AA(uint32_t i) { return (i >> 1) & 1; }
static inline bool PPC_LK(uint32_t i) { return i & 1; }

// ─── IR types (shared with arm64_backend) ─────────────────────────────────────

// Decode a single 32-bit instruction into an IrInstr
static IrInstr decodePPC(uint32_t instr, uint64_t pc) {
    IrInstr ir{};
    ir.rd = ir.ra = ir.rb = ir.rc = -1;
    ir.guestPc = pc;

    const uint32_t prim = PPC_PRIMARY(instr);
    switch (prim) {
    // ── Addi/Addic/Addis ────────────────────────────────────────────────────
    case 14: ir.op = IrOp::Add; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 15: ir.op = IrOp::Add; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr) << 16; break;
    case 12: ir.op = IrOp::Add; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); ir.setRecord = true; break;
    case 13: ir.op = IrOp::Add; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); ir.setRecord = true; break;

    // ── Mulli ───────────────────────────────────────────────────────────────
    case 7:  ir.op = IrOp::Mul; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;

    // ── Load instructions ───────────────────────────────────────────────────
    case 34: ir.op = IrOp::Lbz; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 40: ir.op = IrOp::Lhz; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 32: ir.op = IrOp::Lwz; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 58: ir.op = IrOp::Ld;  ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr) & ~3; break;

    // ── Store instructions ──────────────────────────────────────────────────
    case 38: ir.op = IrOp::Stb; ir.rs = PPC_RS(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 44: ir.op = IrOp::Sth; ir.rs = PPC_RS(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 36: ir.op = IrOp::Stw; ir.rs = PPC_RS(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 62: ir.op = IrOp::Std; ir.rs = PPC_RS(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr) & ~3; break;

    // ── Branch instructions ─────────────────────────────────────────────────
    case 18: // B / BL / BA / BLA
        ir.op = IrOp::B;
        ir.imm = PPC_AA(instr) ? (int64_t)PPC_LI(instr) << 2 : pc + ((int64_t)PPC_LI(instr) << 2);
        ir.link = PPC_LK(instr);
        break;
    case 16: // BC
        ir.op = IrOp::BC;
        ir.ra = PPC_BI(instr);
        ir.imm = PPC_AA(instr) ? (int64_t)PPC_BD(instr) : pc + (int64_t)PPC_BD(instr);
        ir.rd = PPC_BO(instr);
        ir.link = PPC_LK(instr);
        break;

    // ── Compare ─────────────────────────────────────────────────────────────
    case 11: ir.op = IrOp::Cmpi;  ir.rd = (instr >> 23) & 7; ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 10: ir.op = IrOp::Cmpli; ir.rd = (instr >> 23) & 7; ir.ra = PPC_RA(instr); ir.imm = PPC_UIMM16(instr); break;

    // ── FPU loads/stores ─────────────────────────────────────────────────────
    case 48: ir.op = IrOp::Lfs; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 50: ir.op = IrOp::Lfd; ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 52: ir.op = IrOp::Stfs; ir.rd = PPC_RS(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;
    case 54: ir.op = IrOp::Stfd; ir.rd = PPC_RS(instr); ir.ra = PPC_RA(instr); ir.imm = PPC_SIMM16(instr); break;

    // ── Extended opcodes (primary=31: integer, 63: FPU, 4: AltiVec, etc.) ─────
    case 31: {
        const uint32_t sec = PPC_SECONDARY(instr);
        switch (sec) {
        case 266: ir.op = IrOp::Add;   ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.rb = PPC_RB(instr); ir.setRecord = instr & 1; break;
        case 40:  ir.op = IrOp::Sub;   ir.rd = PPC_RD(instr); ir.ra = PPC_RB(instr); ir.rb = PPC_RA(instr); break;
        case 235: ir.op = IrOp::Mul;   ir.rd = PPC_RD(instr); ir.ra = PPC_RA(instr); ir.rb = PPC_RB(instr); break;
        case 28:  ir.op = IrOp::And;   ir.rd = PPC_RA(instr); ir.ra = PPC_RS(instr); ir.rb = PPC_RB(instr); break;
        case 444: ir.op = IrOp::Or;    ir.rd = PPC_RA(instr); ir.ra = PPC_RS(instr); ir.rb = PPC_RB(instr); break;
        case 316: ir.op = IrOp::Xor;   ir.rd = PPC_RA(instr); ir.ra = PPC_RS(instr); ir.rb = PPC_RB(instr); break;
        case 0:   ir.op = IrOp::Cmp;   ir.rd = (instr>>23)&7; ir.ra = PPC_RA(instr); ir.rb = PPC_RB(instr); break;
        case 32:  ir.op = IrOp::Cmpl;  ir.rd = (instr>>23)&7; ir.ra = PPC_RA(instr); ir.rb = PPC_RB(instr); break;
        case 19:  // mfcr
        case 339: ir.op = IrOp::Mfspr; ir.rd = PPC_RD(instr); ir.imm = ((instr>>11)&0x1F)|((instr>>16)&0x1F)<<5; break;
        case 467: ir.op = IrOp::Mtspr; ir.ra = PPC_RS(instr); ir.imm = ((instr>>11)&0x1F)|((instr>>16)&0x1F)<<5; break;
        case 16:  ir.op = IrOp::BCLR;  ir.rd = PPC_BO(instr); ir.ra = PPC_BI(instr); ir.link = PPC_LK(instr); break;
        case 528: ir.op = IrOp::BCCTR; ir.rd = PPC_BO(instr); ir.ra = PPC_BI(instr); ir.link = PPC_LK(instr); break;
        case 20:  ir.op = IrOp::ShiftL; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); break;
        case 536: ir.op = IrOp::ShiftR; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); break;
        case 792: ir.op = IrOp::ShiftRA; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); break;
        case 150: ir.op = IrOp::Stwcx; break;
        case 20+1024: ir.op = IrOp::Lwarx; ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 598: ir.op = IrOp::Sync;  break;
        case 854: ir.op = IrOp::Eieio; break;
        default:  ir.op = IrOp::Unknown; break;
        }
        break;
    }

    // ── VMX128 primary opcode 4 (AltiVec) ──────────────────────────────────
    case 4: {
        const uint32_t sec = PPC_SEC21(instr);
        switch (sec) {
        case 10: ir.op = IrOp::Vaddfp; ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 74: ir.op = IrOp::Vsubfp; ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 43: ir.op = IrOp::Vperm;  ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); ir.rc=PPC_VC(instr); break;
        case 42: ir.op = IrOp::Vsel;   ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); ir.rc=PPC_VC(instr); break;
        case 1028: ir.op = IrOp::Vand; ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 1156: ir.op = IrOp::Vor;  ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 1220: ir.op = IrOp::Vxor; ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        default:   ir.op = IrOp::Unknown; break;
        }
        break;
    }

    // ── VMX128 load/store (primary 31 extended) ─────────────────────────────
    case 31+128: // Xenon-specific: lvx/stvx extended
    default:
        ir.op = IrOp::Unknown;
        break;
    }

    return ir;
}

} // namespace jit
} // namespace x360
