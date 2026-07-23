#pragma once
#include <cstdint>
#include <vector>

namespace x360 {
namespace jit {

// ─── PowerPC → ARM64 IR (Intermediate Representation) ─────────────────────────

enum class IrOp {
    // Integer arithmetic
    Add, Sub, Mul, Div, And, Or, Xor, Nand, Nor, Eqv,
    Andc, Orc,
    ShiftL, ShiftR, ShiftRA, RotateL,
    Extsb, Extsh, Extsw,
    Cntlzw, Cntlzd, Popcntb,
    // Compare → CR update
    Cmp, Cmpl, Cmpli, Cmpi,
    // Branch
    B, BC, BCLR, BCCTR,
    // Load (big-endian → host little-endian via REV)
    Lbz, Lbzu, Lbzx, Lbzux,
    Lhz, Lhzu, Lhzx, Lhzux,
    Lha, Lhax,
    Lwz, Lwzu, Lwzx, Lwzux,
    Lwa, Lwax,
    Ld,  Ldu,  Ldx,  Ldux,
    // Store
    Stb, Stbu, Stbx, Stbux,
    Sth, Sthu, Sthx, Sthux,
    Stw, Stwu, Stwx, Stwux,
    Std, Stdu, Stdx, Stdux,
    // FPU scalar
    Fadd, Fsub, Fmul, Fdiv, Fmadd, Fmsub, Fnmadd, Fnmsub,
    Fsqrt, Frsp, Fctiwz, Fctidz, Fcfid, Fabs, Fneg,
    Lfs, Lfsu, Lfsx, Lfsx2, Lfd, Lfdu, Lfdx,
    Stfs, Stfsx, Stfd, Stfdx,
    Fsel, Fcmpu, Fcmpo,
    // VMX128 / AltiVec
    Lvx, Lvxl, Stvx, Stvxl, Lvsl, Lvsr,
    Vperm, Vsel,
    Vaddfp, Vsubfp, Vmulfp, Vmaddfp, Vnmsubfp,
    Vand, Vandc, Vor, Vorc, Vxor, Vnor,
    Vslo, Vsro, Vsl, Vsr, Vsldoi,
    Vspltb, Vsplth, Vspltw, Vspltisb, Vspltish, Vspltisw,
    Vmrghb, Vmrghh, Vmrghw, Vmrglb, Vmrglh, Vmrglw,
    Vaddsbs, Vaddshs, Vaddsws,
    Vaddubs, Vadduhs, Vadduws,
    Vsubsbs, Vsubshs, Vsubsws,
    Vsububs, Vsubuhs, Vsubuws,
    Vmaxsb, Vmaxsh, Vmaxsw, Vmaxub, Vmaxuh, Vmaxuw,
    Vminsb, Vminsh, Vminsw, Vminub, Vminuh, Vminuw,
    Vminub2,
    Vcmpgtfp, Vcmpeqfp, Vcmpgefp, Vcmpbfp,
    Vcmpequb, Vcmpequh, Vcmpequw,
    Vcmpgtsb, Vcmpgtsh, Vcmpgtsw,
    Vcmpgtub, Vcmpgtuh, Vcmpgtuw,
    Vrefp, Vrsqrtefp, Vlogefp, Vexptefp,
    Vrfin, Vrfiz, Vrfip, Vrfim,
    Vcfsx, Vcfux, Vctsxs, Vctuxs,
    Vpkuhum, Vpkuwum, Vpkshus, Vpkswus, Vpkshss, Vpkswss,
    Vupkhsb, Vupkhsh, Vupklsb, Vupklsh,
    // Special registers
    Mtspr, Mfspr, Mtcrf, Mfcr, Mfocrf,
    // Memory sync / barriers
    Sync, Isync, Eieio, Lwsync,
    // Load-link / store-conditional (LL/SC for atomics)
    Lwarx, Stwcx, Ldarx, Stdcx,
    // HLE trampoline
    HleCall,
    // Nop / Unknown
    Nop, Unknown,
};

struct IrInstr {
    IrOp    op;
    int     rd;    // destination register (-1 = not used)
    int     rs;    // source register (alias for store instructions)
    int     ra;    // operand A
    int     rb;    // operand B
    int     rc;    // operand C (VMX ternary)
    int64_t imm;   // immediate / branch offset / SPR number
    bool    setRecord;   // instruction sets CR0 (Rc=1)
    bool    link;        // branch with link (LK=1)
    bool    update;      // load/store with address update
    uint64_t guestPc;

    IrInstr()
        : op(IrOp::Unknown), rd(-1), rs(-1), ra(-1), rb(-1), rc(-1),
          imm(0), setRecord(false), link(false), update(false), guestPc(0) {}
};

// A basic block: straight-line sequence of IR instructions ending in a branch.
struct IrBlock {
    uint64_t guestPcStart = 0;
    uint64_t guestPcEnd   = 0;
    std::vector<IrInstr> instrs;
    bool endsWithReturn = false;
    bool endsWithBranch = false;
    uint64_t branchTarget = 0;
    uint64_t fallthrough  = 0;
};

// ─── IR builder (ir_builder.cpp) ───────────────────────────────────────────────
// Build an IR basic block starting at guestPc from guest memory.
IrBlock buildIrBlock(const uint8_t* guestMemory, uint64_t guestPc,
                     uint64_t guestBase, uint64_t guestMemorySize);

} // namespace jit
} // namespace x360
