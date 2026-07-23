/**
 * Xenos PM4 Command Buffer Parser.
 * Processes the ring buffer fed by the guest GPU driver and dispatches
 * draw calls, state changes, and shader loads to the Vulkan renderer.
 *
 * Reference: Xenia gpu/command_processor.cc (MIT), ATI R200 PM4 spec.
 */
#include "../../include/gpu/gpu_layer.h"
#include <cstring>
#include <android/log.h>

#define LOG_TAG "X360:PM4"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace gpu {

// PM4 packet header decoding
static inline Pm4PacketType pm4Type(uint32_t header) {
    return static_cast<Pm4PacketType>(header >> 30);
}
static inline uint32_t pm4Type3Opcode(uint32_t header) {
    return (header >> 8) & 0xFF;
}
static inline uint32_t pm4Type3Count(uint32_t header) {
    return (header & 0x3FFF) + 1; // dword count including the header words
}
static inline uint32_t pm4Type0Regbase(uint32_t header) {
    return header & 0x7FFF;
}
static inline uint32_t pm4Type0Count(uint32_t header) {
    return ((header >> 16) & 0x3FFF) + 1;
}

void GpuLayer::processPm4RingBuffer(const uint32_t* ringBuf, uint32_t sizeWords) {
    uint32_t i = 0;
    while (i < sizeWords) {
        uint32_t header = __builtin_bswap32(ringBuf[i]);
        Pm4PacketType type = pm4Type(header);

        if (type == Pm4PacketType::Type2) {
            // NOP — skip
            i++;
            continue;
        }

        if (type == Pm4PacketType::Type0) {
            // Set constants range
            uint32_t regBase = pm4Type0Regbase(header);
            uint32_t count   = pm4Type0Count(header);
            i++;
            uint32_t* regsPtr = reinterpret_cast<uint32_t*>(&m_regs);
            for (uint32_t j = 0; j < count && i < sizeWords; j++, i++) {
                uint32_t regIdx = regBase + j;
                if (regIdx < sizeof(m_regs) / sizeof(uint32_t)) {
                    regsPtr[regIdx] = __builtin_bswap32(ringBuf[i]);
                }
            }
            continue;
        }

        if (type == Pm4PacketType::Type3) {
            uint32_t opcode = pm4Type3Opcode(header);
            uint32_t count  = pm4Type3Count(header);
            i++; // skip header
            if (i + count - 1 > sizeWords) break; // truncated

            const uint32_t* params = ringBuf + i;
            dispatchType3(static_cast<Pm4OpCode>(opcode), params, count - 1);
            i += count - 1;
            continue;
        }

        // Unknown packet type — skip
        i++;
    }
}

void GpuLayer::dispatchType3(Pm4OpCode op, const uint32_t* params, uint32_t count) {
    switch (op) {
    case Pm4OpCode::NopPacket:
        break;

    case Pm4OpCode::DrawIndx:
    case Pm4OpCode::DrawIndx2:
        handleDrawIndx(params);
        break;

    case Pm4OpCode::SetConstant:
        handleSetConstant(params, count);
        break;

    case Pm4OpCode::SetConstantF:
        // Set floating-point shader constants (same structure, different bank)
        handleSetConstant(params, count);
        break;

    case Pm4OpCode::IndirectBuffer:
        if (count >= 2) {
            uint32_t ptr  = __builtin_bswap32(params[0]);
            uint32_t size = __builtin_bswap32(params[1]);
            handleIndirectBuffer(ptr, size);
        }
        break;

    case Pm4OpCode::Swap:
    case Pm4OpCode::EventWriteEop:
        handleSwap();
        break;

    case Pm4OpCode::LoadState:
        // Load GPU state block — parse register range
        if (count >= 2) {
            uint32_t addr = __builtin_bswap32(params[0]);
            uint32_t num  = __builtin_bswap32(params[1]) & 0xFFF;
            LOGI("PM4: LOAD_STATE addr=0x%X num=%u (stub)", addr, num);
        }
        break;

    case Pm4OpCode::SetShaderConstants:
        // SQ_PROGRAM_CNTL and shader upload
        if (count > 0) {
            m_regs.sqProgramCntl = __builtin_bswap32(params[0]);
        }
        break;

    default:
        // Unknown opcode — silently ignore
        break;
    }
}

void GpuLayer::handleDrawIndx(const uint32_t* params) {
    if (!m_initialized) return;

    // params[0] = VTX count
    // params[1] = draw initiator (prim type, index type, etc.)
    // Simplified draw dispatch — full implementation requires vertex buffer binding
    uint32_t vtxCount = __builtin_bswap32(params[0]);
    uint32_t initiator = (count_hint: 2) ? __builtin_bswap32(params[1]) : 0;

    LOGI("PM4: DRAW_INDX vtxCount=%u initiator=0x%X", vtxCount, initiator);

    // In a full implementation:
    // 1. Bind vertex/index buffers from GPU registers
    // 2. Bind shader pipeline (compiled from microcode)
    // 3. Call vkCmdDrawIndexed or vkCmdDraw
    // For now: the frame will show a cleared background (see presentFrame)
}

void GpuLayer::handleSetConstant(const uint32_t* params, uint32_t count) {
    if (count < 2) return;
    uint32_t regOffset = __builtin_bswap32(params[0]) & 0xFFFF;
    uint32_t* regsPtr = reinterpret_cast<uint32_t*>(&m_regs);
    for (uint32_t i = 1; i < count && regOffset + i - 1 < sizeof(m_regs)/4; i++) {
        regsPtr[regOffset + i - 1] = __builtin_bswap32(params[i]);
    }
}

void GpuLayer::handleSwap() {
    presentFrame();
}

void GpuLayer::handleIndirectBuffer(uint32_t ptr, uint32_t size) {
    if (!m_guestMemory || ptr < m_guestBase) return;
    uint64_t offset = ptr - m_guestBase;
    if (offset + size * 4 > 0x100000000ULL) return;
    processPm4RingBuffer(reinterpret_cast<const uint32_t*>(m_guestMemory + offset), size);
}

} // namespace gpu
} // namespace x360
