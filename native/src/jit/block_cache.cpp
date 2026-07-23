/**
 * Block cache management.
 * The main cache lives in JitEngine (unordered_map); this file provides
 * hash helpers and cache-entry lifecycle utilities used by jit_engine.cpp.
 */
#include "../../include/jit/jit_engine.h"
#include <android/log.h>
#include <cstring>

#define LOG_TAG "X360:CACHE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace jit {

// FNV-1a 32-bit hash of guest instruction bytes — used for block invalidation check
uint32_t hashGuestInstructions(const uint8_t* guestCode, size_t byteLen) {
    uint32_t hash = 0x811c9dc5u;
    for (size_t i = 0; i < byteLen; i++) {
        hash ^= guestCode[i];
        hash *= 0x01000193u;
    }
    return hash;
}

// Check if a compiled block is still valid (guest code unchanged)
bool isBlockValid(const CompiledBlock& block, const uint8_t* guestMemory,
                  uint64_t guestBase, uint64_t guestMemorySize) {
    if (!block.isValid) return false;
    uint64_t offset = block.guestPc - guestBase;
    if (offset + block.nativeSize / 4 * 4 > guestMemorySize) return false;
    uint32_t currentHash = hashGuestInstructions(
        guestMemory + offset,
        std::min((size_t)block.nativeSize / 4 * 4, (size_t)256)); // check up to 64 PPC instrs
    return currentHash == block.guestHash;
}

} // namespace jit
} // namespace x360
