/**
 * IR Optimizer — DCE, constant folding, endian-swap fusion.
 * Operates on IrBlock in-place before ARM64 codegen.
 */
#include "../../include/jit/ir.h"
#include <unordered_map>
#include <bitset>
#include <android/log.h>

#define LOG_TAG "X360:OPT"

namespace x360 {
namespace jit {

// ─── Dead Code Elimination ───────────────────────────────────────────────────
// Mark instructions whose destination register is never read after them as dead.
static void dce(IrBlock& block) {
    // Use liveness: walk backwards, track which regs are live
    std::bitset<256> liveRegs; // GPR 0-31, FPR 32-63, VMX 64-191
    liveRegs.set(); // conservative: all live initially

    for (int i = (int)block.instrs.size() - 1; i >= 0; i--) {
        IrInstr& ir = block.instrs[i];
        // Check if destination is live
        int dst = ir.rd;
        if (dst >= 0 && !liveRegs.test((size_t)dst)) {
            // Result is dead — but only eliminate if no side effects
            if (ir.op == IrOp::Add || ir.op == IrOp::Sub || ir.op == IrOp::Mul ||
                ir.op == IrOp::And || ir.op == IrOp::Or  || ir.op == IrOp::Xor ||
                ir.op == IrOp::ShiftL || ir.op == IrOp::ShiftR) {
                ir.op = IrOp::Nop;
                continue;
            }
        }
        // Mark source registers as live
        if (ir.ra >= 0) liveRegs.set((size_t)ir.ra);
        if (ir.rb >= 0) liveRegs.set((size_t)ir.rb);
        if (ir.rc >= 0) liveRegs.set((size_t)ir.rc);
        if (ir.rs >= 0) liveRegs.set((size_t)ir.rs);
        // Mark destination as defined (not live before this point)
        if (dst >= 0 && !ir.setRecord) liveRegs.reset((size_t)dst);
    }
}

// ─── Constant Folding ─────────────────────────────────────────────────────────
// Propagate constant values through pure arithmetic.
static void constantFold(IrBlock& block) {
    std::unordered_map<int, int64_t> constMap; // reg → known constant value

    for (auto& ir : block.instrs) {
        // Propagate known constants into immediates
        if (ir.ra >= 0 && constMap.count(ir.ra) && ir.rb < 0) {
            // ra is a known constant — fold it
        }

        // ADDI r0, 0, #imm → r0 = imm (li pseudo)
        if (ir.op == IrOp::Add && ir.ra == 0 && ir.rb < 0) {
            constMap[ir.rd] = ir.imm;
        } else if (ir.op == IrOp::Add && ir.ra >= 0 && ir.rb < 0
                   && constMap.count(ir.ra)) {
            constMap[ir.rd] = constMap[ir.ra] + ir.imm;
        } else if (ir.rd >= 0) {
            constMap.erase(ir.rd); // destination redefined with unknown value
        }

        // Fold Add(ra, 0) → mov (rb==0 or imm==0)
        if (ir.op == IrOp::Add && ir.imm == 0 && ir.rb < 0 && ir.ra == ir.rd) {
            ir.op = IrOp::Nop; // self-add with zero is NOP
        }
    }
}

// ─── Endian swap fusion ───────────────────────────────────────────────────────
// If we load then immediately store with no modification, fuse the pair.
// (Very common pattern in memcpy-like game code.)
static void fuseEndianSwaps(IrBlock& block) {
    for (size_t i = 0; i + 1 < block.instrs.size(); i++) {
        auto& a = block.instrs[i];
        auto& b = block.instrs[i + 1];
        // Load followed by store of same register to same base+offset:
        // lwz rX, N(rA) + stw rX, N(rA) → nop + nop (if same location)
        if ((a.op == IrOp::Lwz || a.op == IrOp::Lhz || a.op == IrOp::Ld) &&
            (b.op == IrOp::Stw || b.op == IrOp::Sth || b.op == IrOp::Std) &&
            a.rd == b.rs && a.ra == b.ra && a.imm == b.imm) {
            // round-trip: no net change — eliminate both
            a.op = IrOp::Nop;
            b.op = IrOp::Nop;
            i++; // skip b
        }
    }
}

// Remove Nop instructions from the block
static void removeNops(IrBlock& block) {
    auto it = std::remove_if(block.instrs.begin(), block.instrs.end(),
        [](const IrInstr& ir) { return ir.op == IrOp::Nop; });
    block.instrs.erase(it, block.instrs.end());
}

void optimizeBlock(IrBlock& block) {
    constantFold(block);
    dce(block);
    fuseEndianSwaps(block);
    removeNops(block);
}

} // namespace jit
} // namespace x360
