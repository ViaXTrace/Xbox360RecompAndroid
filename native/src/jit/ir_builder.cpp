/**
 * IR Builder — decodes PowerPC instructions into IrBlock (SSA IR).
 * Terminates a block at branches, HLE calls, and block size limits.
 */
#include "../../include/jit/ir.h"
#include <android/log.h>
#include <cstdint>
#include <cstring>

#define LOG_TAG "X360:IR"

namespace x360 {
namespace jit {

// PPC field extraction
static inline uint32_t PPC_PRIMARY(uint32_t i)  { return i >> 26; }
static inline uint32_t PPC_SECONDARY(uint32_t i){ return (i >> 1) & 0x3FF; }
static inline int      PPC_RA(uint32_t i)  { return (i >> 16) & 0x1F; }
static inline int      PPC_RB(uint32_t i)  { return (i >> 11) & 0x1F; }
static inline int      PPC_RC(uint32_t i)  { return (i >>  6) & 0x1F; }
static inline int      PPC_RD(uint32_t i)  { return (i >> 21) & 0x1F; }
static inline int      PPC_RS(uint32_t i)  { return (i >> 21) & 0x1F; }
static inline int      PPC_VD(uint32_t i)  { return (i >> 21) & 0x7F; }
static inline int      PPC_VA(uint32_t i)  { return (i >> 16) & 0x7F; }
static inline int      PPC_VB(uint32_t i)  { return (i >> 11) & 0x7F; }
static inline int      PPC_VC(uint32_t i)  { return (i >>  6) & 0x7F; }
static inline int32_t  PPC_SIMM16(uint32_t i) { return (int32_t)(int16_t)(i & 0xFFFF); }
static inline uint32_t PPC_UIMM16(uint32_t i)  { return i & 0xFFFF; }
static inline int      PPC_BI(uint32_t i)  { return (i >> 16) & 0x1F; }
static inline int      PPC_BO(uint32_t i)  { return (i >> 21) & 0x1F; }
static inline int32_t  PPC_BD(uint32_t i)  { return (int32_t)((int16_t)(i & 0xFFFC)); }
static inline int32_t  PPC_LI(uint32_t i)  {
    int32_t li = (i & 0x03FFFFFC) >> 2;
    return (li << 8) >> 8; // sign extend 24-bit
}
static inline bool PPC_AA(uint32_t i) { return (i >> 1) & 1; }
static inline bool PPC_LK(uint32_t i) { return i & 1; }
static inline bool PPC_RC(bool fromInstr, uint32_t i) { return i & 1; }
static inline int  PPC_SH(uint32_t i) { return (i >> 11) & 0x1F; }
static inline int  PPC_ME(uint32_t i) { return (i >>  1) & 0x1F; }
static inline int  PPC_MB(uint32_t i) { return (i >>  6) & 0x1F; }
static inline int  PPC_FRA(uint32_t i) { return (i >> 16) & 0x1F; }
static inline int  PPC_FRB(uint32_t i) { return (i >> 11) & 0x1F; }
static inline int  PPC_FRC(uint32_t i) { return (i >>  6) & 0x1F; }
static inline int  PPC_FRD(uint32_t i) { return (i >> 21) & 0x1F; }
static inline int  PPC_FRS(uint32_t i) { return (i >> 21) & 0x1F; }

static constexpr int kMaxBlockInstrs = 128;

static IrInstr decodeOne(uint32_t instr, uint64_t pc) {
    IrInstr ir;
    ir.guestPc = pc;
    const uint32_t prim = PPC_PRIMARY(instr);
    const uint32_t sec  = PPC_SECONDARY(instr);

    switch (prim) {
    // ── Integer immediate ───────────────────────────────────────────────────
    case 14: ir.op=IrOp::Add;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break; // addi
    case 15: ir.op=IrOp::Add;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=(int64_t)PPC_SIMM16(instr)<<16; break; // addis
    case 12: ir.op=IrOp::Add;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.setRecord=true; break; // addic
    case 13: ir.op=IrOp::Add;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.setRecord=true; break; // addic.
    case 7:  ir.op=IrOp::Mul;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break; // mulli
    case 8:  ir.op=IrOp::Sub;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break; // subfic
    case 24: ir.op=IrOp::Or;   ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.imm=PPC_UIMM16(instr); break; // ori
    case 25: ir.op=IrOp::Or;   ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.imm=(int64_t)PPC_UIMM16(instr)<<16; break; // oris
    case 26: ir.op=IrOp::Xor;  ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.imm=PPC_UIMM16(instr); break; // xori
    case 27: ir.op=IrOp::Xor;  ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.imm=(int64_t)PPC_UIMM16(instr)<<16; break; // xoris
    case 28: ir.op=IrOp::And;  ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.imm=PPC_UIMM16(instr); ir.setRecord=true; break; // andi.
    case 29: ir.op=IrOp::And;  ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.imm=(int64_t)PPC_UIMM16(instr)<<16; ir.setRecord=true; break; // andis.

    // ── Compare immediate ───────────────────────────────────────────────────
    case 11: ir.op=IrOp::Cmpi;  ir.rd=(instr>>23)&7; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 10: ir.op=IrOp::Cmpli; ir.rd=(instr>>23)&7; ir.ra=PPC_RA(instr); ir.imm=PPC_UIMM16(instr); break;

    // ── Loads ───────────────────────────────────────────────────────────────
    case 34: ir.op=IrOp::Lbz;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 35: ir.op=IrOp::Lbzu; ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;
    case 40: ir.op=IrOp::Lhz;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 41: ir.op=IrOp::Lhzu; ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;
    case 42: ir.op=IrOp::Lha;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 32: ir.op=IrOp::Lwz;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 33: ir.op=IrOp::Lwzu; ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;
    case 58: {
        int ds = (int)(instr & ~3); if (ds & 0x8000) ds |= ~0xFFFF; // sign extend
        if ((instr & 3) == 0) { ir.op=IrOp::Ld;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=ds; }
        else if ((instr&3)==1){ ir.op=IrOp::Ldu; ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=ds; ir.update=true; }
        else                  { ir.op=IrOp::Lwa; ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.imm=ds; }
        break;
    }
    case 48: ir.op=IrOp::Lfs;  ir.rd=PPC_FRD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 49: ir.op=IrOp::Lfsu; ir.rd=PPC_FRD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;
    case 50: ir.op=IrOp::Lfd;  ir.rd=PPC_FRD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 51: ir.op=IrOp::Lfdu; ir.rd=PPC_FRD(instr); ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;

    // ── Stores ──────────────────────────────────────────────────────────────
    case 38: ir.op=IrOp::Stb;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 39: ir.op=IrOp::Stbu; ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;
    case 44: ir.op=IrOp::Sth;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 45: ir.op=IrOp::Sthu; ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;
    case 36: ir.op=IrOp::Stw;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 37: ir.op=IrOp::Stwu; ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;
    case 62: {
        int ds = (int)(instr & ~3); if (ds & 0x8000) ds |= ~0xFFFF;
        if ((instr&3)==0) { ir.op=IrOp::Std;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=ds; }
        else              { ir.op=IrOp::Stdu; ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=ds; ir.update=true; }
        break;
    }
    case 52: ir.op=IrOp::Stfs; ir.rs=PPC_FRS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 53: ir.op=IrOp::Stfs; ir.rs=PPC_FRS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;
    case 54: ir.op=IrOp::Stfd; ir.rs=PPC_FRS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); break;
    case 55: ir.op=IrOp::Stfd; ir.rs=PPC_FRS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.imm=PPC_SIMM16(instr); ir.update=true; break;

    // ── Branches ─────────────────────────────────────────────────────────────
    case 18:
        ir.op = IrOp::B;
        ir.imm = PPC_AA(instr) ? ((int64_t)PPC_LI(instr) << 2)
                               : (int64_t)pc + ((int64_t)PPC_LI(instr) << 2);
        ir.link = PPC_LK(instr);
        break;
    case 16:
        ir.op = IrOp::BC;
        ir.rd = PPC_BO(instr);
        ir.ra = PPC_BI(instr);
        ir.imm = PPC_AA(instr) ? (int64_t)PPC_BD(instr) : (int64_t)pc + PPC_BD(instr);
        ir.link = PPC_LK(instr);
        break;

    // ── Extended opcodes ─────────────────────────────────────────────────────
    case 31:
        switch (sec) {
        case 266: ir.op=IrOp::Add;   ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); ir.setRecord=instr&1; break;
        case 40:  ir.op=IrOp::Sub;   ir.rd=PPC_RD(instr); ir.ra=PPC_RB(instr); ir.rb=PPC_RA(instr); ir.setRecord=instr&1; break;
        case 235: ir.op=IrOp::Mul;   ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); ir.setRecord=instr&1; break;
        case 491: ir.op=IrOp::Div;   ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 459: ir.op=IrOp::Div;   ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break; // divwu
        case 28:  ir.op=IrOp::And;   ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); ir.setRecord=instr&1; break;
        case 444: ir.op=IrOp::Or;    ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); ir.setRecord=instr&1; break;
        case 316: ir.op=IrOp::Xor;   ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); ir.setRecord=instr&1; break;
        case 476: ir.op=IrOp::Nand;  ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); break;
        case 124: ir.op=IrOp::Nor;   ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); ir.setRecord=instr&1; break;
        case 60:  ir.op=IrOp::Andc;  ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); break;
        case 412: ir.op=IrOp::Orc;   ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); break;
        case 0:   ir.op=IrOp::Cmp;   ir.rd=(instr>>23)&7; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 32:  ir.op=IrOp::Cmpl;  ir.rd=(instr>>23)&7; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 20:  ir.op=IrOp::ShiftL; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); ir.setRecord=instr&1; break;
        case 536: ir.op=IrOp::ShiftR; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); ir.setRecord=instr&1; break;
        case 792: ir.op=IrOp::ShiftRA; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); ir.setRecord=instr&1; break;
        case 824: ir.op=IrOp::ShiftRA; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.imm=PPC_SH(instr); ir.setRecord=instr&1; break; // srawi
        case 339: ir.op=IrOp::Mfspr; ir.rd=PPC_RD(instr); ir.imm=((instr>>11)&0x1F)|((instr>>16)&0x1F)<<5; break;
        case 467: ir.op=IrOp::Mtspr; ir.rs=PPC_RS(instr); ir.imm=((instr>>11)&0x1F)|((instr>>16)&0x1F)<<5; break;
        case 19:  ir.op=IrOp::Mfcr;  ir.rd=PPC_RD(instr); break;
        case 144: ir.op=IrOp::Mtcrf; ir.rs=PPC_RS(instr); ir.imm=(instr>>12)&0xFF; break;
        case 16:  ir.op=IrOp::BCLR;  ir.rd=PPC_BO(instr); ir.ra=PPC_BI(instr); ir.link=PPC_LK(instr); break;
        case 528: ir.op=IrOp::BCCTR; ir.rd=PPC_BO(instr); ir.ra=PPC_BI(instr); ir.link=PPC_LK(instr); break;
        case 150: ir.op=IrOp::Stwcx; ir.rs=PPC_RS(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 214: ir.op=IrOp::Stdcx; ir.rs=PPC_RS(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 20+1024: case 20+1025: ir.op=IrOp::Lwarx; ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 84:  ir.op=IrOp::Ldarx; ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 598: ir.op=IrOp::Sync;  ir.imm=(instr>>21)&3; break;
        case 566: ir.op=IrOp::Lwsync; break;
        case 854: ir.op=IrOp::Eieio; break;
        case 54:  ir.op=IrOp::Isync; break; // actually isync is special
        case 954: ir.op=IrOp::Extsb; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.setRecord=instr&1; break;
        case 922: ir.op=IrOp::Extsh; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.setRecord=instr&1; break;
        case 986: ir.op=IrOp::Extsw; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.setRecord=instr&1; break;
        case 26:  ir.op=IrOp::Cntlzw; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); break;
        case 58:  ir.op=IrOp::Cntlzd; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); break;
        // Indexed loads/stores
        case 23:  ir.op=IrOp::Lwzx;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 55:  ir.op=IrOp::Lwzux; ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); ir.update=true; break;
        case 87:  ir.op=IrOp::Lbzx;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 279: ir.op=IrOp::Lhzx;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 341: ir.op=IrOp::Lhax;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 21:  ir.op=IrOp::Ldx;   ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 151: ir.op=IrOp::Stwx;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 215: ir.op=IrOp::Stbx;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 407: ir.op=IrOp::Sthx;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 149: ir.op=IrOp::Stdx;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 790: ir.op=IrOp::Lhzx;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break; // lhbrx
        case 534: ir.op=IrOp::Lwzx;  ir.rd=PPC_RD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break; // lwbrx
        case 918: ir.op=IrOp::Sthx;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break; // sthbrx
        case 662: ir.op=IrOp::Stwx;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break; // stwbrx
        case 758: ir.op=IrOp::Stdx;  ir.rs=PPC_RS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break; // dcba
        // FPU indexed
        case 535: ir.op=IrOp::Lfsx;  ir.rd=PPC_FRD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 599: ir.op=IrOp::Lfdx;  ir.rd=PPC_FRD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 663: ir.op=IrOp::Stfsx; ir.rs=PPC_FRS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 727: ir.op=IrOp::Stfdx; ir.rs=PPC_FRS(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        // VMX indexed
        case 103: ir.op=IrOp::Lvx;   ir.rd=PPC_VD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 231: ir.op=IrOp::Stvx;  ir.rs=PPC_VD(instr); ir.rd=ir.rs; ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 6:   ir.op=IrOp::Lvsl;  ir.rd=PPC_VD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        case 38:  ir.op=IrOp::Lvsr;  ir.rd=PPC_VD(instr); ir.ra=PPC_RA(instr); ir.rb=PPC_RB(instr); break;
        default:  ir.op=IrOp::Unknown; break;
        }
        break;

    // ── FPU ──────────────────────────────────────────────────────────────────
    case 63:
        switch (sec) {
        case 21:  ir.op=IrOp::Fadd;  ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); ir.setRecord=instr&1; break;
        case 20:  ir.op=IrOp::Fsub;  ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); ir.setRecord=instr&1; break;
        case 25:  ir.op=IrOp::Fmul;  ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rc=PPC_FRC(instr); ir.setRecord=instr&1; break;
        case 18:  ir.op=IrOp::Fdiv;  ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); ir.setRecord=instr&1; break;
        case 29:  ir.op=IrOp::Fmadd; ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); ir.rc=PPC_FRC(instr); break;
        case 28:  ir.op=IrOp::Fmsub; ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); ir.rc=PPC_FRC(instr); break;
        case 22:  ir.op=IrOp::Fsqrt; ir.rd=PPC_FRD(instr); ir.rb=PPC_FRB(instr); break;
        case 12:  ir.op=IrOp::Frsp;  ir.rd=PPC_FRD(instr); ir.rb=PPC_FRB(instr); break;
        case 15:  ir.op=IrOp::Fctiwz;ir.rd=PPC_FRD(instr); ir.rb=PPC_FRB(instr); break;
        case 815: ir.op=IrOp::Fctidz;ir.rd=PPC_FRD(instr); ir.rb=PPC_FRB(instr); break;
        case 846: ir.op=IrOp::Fcfid; ir.rd=PPC_FRD(instr); ir.rb=PPC_FRB(instr); break;
        case 40:  ir.op=IrOp::Fneg;  ir.rd=PPC_FRD(instr); ir.rb=PPC_FRB(instr); break;
        case 264: ir.op=IrOp::Fabs;  ir.rd=PPC_FRD(instr); ir.rb=PPC_FRB(instr); break;
        case 0:   ir.op=IrOp::Fcmpu; ir.rd=(instr>>23)&7; ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); break;
        case 32:  ir.op=IrOp::Fcmpo; ir.rd=(instr>>23)&7; ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); break;
        case 23:  ir.op=IrOp::Fsel;  ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); ir.rc=PPC_FRC(instr); break;
        default:  ir.op=IrOp::Unknown; break;
        }
        break;

    // ── FPU single-precision ─────────────────────────────────────────────────
    case 59:
        switch (sec) {
        case 21: ir.op=IrOp::Fadd;  ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); break;
        case 20: ir.op=IrOp::Fsub;  ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); break;
        case 25: ir.op=IrOp::Fmul;  ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rc=PPC_FRC(instr); break;
        case 18: ir.op=IrOp::Fdiv;  ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); break;
        case 22: ir.op=IrOp::Fsqrt; ir.rd=PPC_FRD(instr); ir.rb=PPC_FRB(instr); break;
        case 29: ir.op=IrOp::Fmadd; ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); ir.rc=PPC_FRC(instr); break;
        case 28: ir.op=IrOp::Fmsub; ir.rd=PPC_FRD(instr); ir.ra=PPC_FRA(instr); ir.rb=PPC_FRB(instr); ir.rc=PPC_FRC(instr); break;
        default: ir.op=IrOp::Unknown; break;
        }
        break;

    // ── VMX / AltiVec (primary 4) ────────────────────────────────────────────
    case 4: {
        uint32_t vsec = (instr >> 1) & 0x3FF; // some use full 10-bit, some 9-bit
        switch (vsec) {
        case 10:   ir.op=IrOp::Vaddfp;  ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 74:   ir.op=IrOp::Vsubfp;  ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 43:   ir.op=IrOp::Vperm;   ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); ir.rc=PPC_VC(instr); break;
        case 42:   ir.op=IrOp::Vsel;    ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); ir.rc=PPC_VC(instr); break;
        case 1028: ir.op=IrOp::Vand;    ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 1284: ir.op=IrOp::Vandc;   ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 1156: ir.op=IrOp::Vor;     ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 1220: ir.op=IrOp::Vxor;    ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 1284+128: ir.op=IrOp::Vnor; ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); break;
        case 780:  ir.op=IrOp::Vsldoi;  ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); ir.imm=(instr>>6)&0xF; break;
        case 44:   ir.op=IrOp::Vmaddfp; ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); ir.rc=PPC_VC(instr); break;
        case 45:   ir.op=IrOp::Vnmsubfp;ir.rd=PPC_VD(instr); ir.ra=PPC_VA(instr); ir.rb=PPC_VB(instr); ir.rc=PPC_VC(instr); break;
        case 300:  ir.op=IrOp::Vspltb;  ir.rd=PPC_VD(instr); ir.rb=PPC_VB(instr); ir.imm=(instr>>16)&0xF; break;
        case 332:  ir.op=IrOp::Vsplth;  ir.rd=PPC_VD(instr); ir.rb=PPC_VB(instr); ir.imm=(instr>>16)&7; break;
        case 396:  ir.op=IrOp::Vspltw;  ir.rd=PPC_VD(instr); ir.rb=PPC_VB(instr); ir.imm=(instr>>16)&3; break;
        case 524:  ir.op=IrOp::Vspltisb;ir.rd=PPC_VD(instr); ir.imm=(int8_t)((instr>>16)&0x1F); break;
        case 556:  ir.op=IrOp::Vspltish;ir.rd=PPC_VD(instr); ir.imm=(int8_t)((instr>>16)&0x1F); break;
        case 588:  ir.op=IrOp::Vspltisw;ir.rd=PPC_VD(instr); ir.imm=(int8_t)((instr>>16)&0x1F); break;
        default:   ir.op=IrOp::Unknown; break;
        }
        break;
    }

    case 19: // branch condition extended
        switch ((instr>>1)&0x3FF) {
        case 16: ir.op=IrOp::BCLR; ir.rd=PPC_BO(instr); ir.ra=PPC_BI(instr); ir.link=PPC_LK(instr); break;
        case 528: ir.op=IrOp::BCCTR; ir.rd=PPC_BO(instr); ir.ra=PPC_BI(instr); ir.link=PPC_LK(instr); break;
        case 150: ir.op=IrOp::Isync; break;
        default: ir.op=IrOp::Nop; break;
        }
        break;

    case 20: ir.op=IrOp::RotateL; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_SH(instr); ir.imm=PPC_MB(instr)|(int64_t)PPC_ME(instr)<<5; ir.setRecord=instr&1; break; // rlwimi
    case 21: ir.op=IrOp::RotateL; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_SH(instr); ir.imm=PPC_MB(instr)|(int64_t)PPC_ME(instr)<<5; ir.setRecord=instr&1; break; // rlwinm
    case 23: ir.op=IrOp::RotateL; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr); ir.rb=PPC_RB(instr); ir.imm=PPC_MB(instr)|(int64_t)PPC_ME(instr)<<5; ir.setRecord=instr&1; break; // rlwnm
    case 30: { // 64-bit rotate
        uint32_t s30 = (instr>>1)&0xF;
        ir.op=IrOp::RotateL; ir.rd=PPC_RA(instr); ir.ra=PPC_RS(instr);
        ir.rb=(s30==0||s30==2||s30==4||s30==6)?PPC_SH(instr):PPC_RB(instr);
        ir.imm=s30; ir.setRecord=instr&1;
        break;
    }

    default:
        ir.op = IrOp::Unknown;
        break;
    }

    return ir;
}

// Build an IR basic block starting at guestPc
IrBlock buildIrBlock(const uint8_t* guestMemory, uint64_t guestPc,
                     uint64_t guestBase, uint64_t guestMemorySize) {
    IrBlock block;
    block.guestPcStart = guestPc;

    uint64_t pc = guestPc;
    for (int i = 0; i < kMaxBlockInstrs; i++) {
        uint64_t offset = pc - guestBase;
        if (offset + 4 > guestMemorySize) break;

        // Guest memory is big-endian; swap to host
        uint32_t raw;
        __builtin_memcpy(&raw, guestMemory + offset, 4);
        raw = __builtin_bswap32(raw);

        IrInstr ir = decodeOne(raw, pc);
        ir.guestPc = pc;
        block.instrs.push_back(ir);
        block.guestPcEnd = pc;
        pc += 4;

        // Terminate block at branches
        if (ir.op == IrOp::B || ir.op == IrOp::BCLR || ir.op == IrOp::BCCTR ||
            ir.op == IrOp::BC) {
            if (ir.op == IrOp::B) {
                block.endsWithBranch = true;
                block.branchTarget = (uint64_t)ir.imm;
            } else if (ir.op == IrOp::BCLR) {
                block.endsWithReturn = true;
            }
            block.fallthrough = pc;
            break;
        }
    }
    return block;
}

} // namespace jit
} // namespace x360
