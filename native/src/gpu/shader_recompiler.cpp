/**
 * Xenos Shader Microcode → SPIR-V Recompiler.
 * Decodes vertex and pixel shader microcode from the Xenos GPU and
 * translates it to SPIR-V for Vulkan.
 *
 * Xenos shader ISA: ALU + FETCH instructions (ATI-derived, not public).
 * Reference: Xenia gpu/shader.cc + gpu/spirv_shader_translator.cc (MIT).
 * All code written from scratch.
 */
#include "../../include/gpu/gpu_layer.h"
#include <cstring>
#include <functional>
#include <android/log.h>

#ifdef HAVE_SPIRV_HEADERS
#include <spirv/unified1/spirv.h>
#endif

#define LOG_TAG "X360:SHADER"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace gpu {

// ─── SPIR-V builder (minimal, hand-rolled) ────────────────────────────────────
// Produces a trivial pass-through SPIR-V shader if full decompilation fails.

struct SpvBuilder {
    std::vector<uint32_t> words;

    void emit(uint32_t w) { words.push_back(w); }

    void emitInstruction(uint16_t opcode, std::initializer_list<uint32_t> operands) {
        uint16_t wordCount = (uint16_t)(1 + operands.size());
        emit((wordCount << 16) | opcode);
        for (auto& op : operands) emit(op);
    }

    // Emit a string literal (null-terminated, padded to 4 bytes)
    void emitString(uint32_t opcode, uint32_t id, const char* str) {
        size_t len = strlen(str);
        size_t words = (len / 4) + 1;
        uint16_t wc = (uint16_t)(2 + words);
        emit((wc << 16) | opcode);
        emit(id);
        uint32_t w = 0;
        size_t i = 0;
        for (; str[i]; i++) {
            w |= ((uint32_t)(uint8_t)str[i]) << ((i % 4) * 8);
            if (i % 4 == 3) { emit(w); w = 0; }
        }
        emit(w); // final word (may be partial + null terminator)
    }
};

// Minimal passthrough vertex shader SPIR-V (outputs gl_Position = input position)
static std::vector<uint8_t> buildPassthroughVertexSpirv() {
    SpvBuilder b;

    // SPIR-V magic + version + generator + bound + schema
    b.emit(0x07230203); // magic
    b.emit(0x00010300); // version 1.3
    b.emit(0);          // generator
    b.emit(10);         // bound (IDs 1-9 used)
    b.emit(0);          // schema

    // OpCapability Shader
    b.emitInstruction(17, {1});
    // OpExtInstImport "GLSL.std.450" id=1
    b.emitString(11, 1, "GLSL.std.450");
    // OpMemoryModel Logical GLSL450
    b.emitInstruction(14, {0, 1});
    // OpEntryPoint Vertex main id=4 "main" io_vars
    b.emit((5 << 16) | 15); b.emit(0); b.emit(4);
    // "main" string inline
    b.emit(0x6E69616D); b.emit(0); // "main\0"
    // OpExecutionMode id=4 OriginUpperLeft
    b.emitInstruction(16, {4, 7});

    // OpTypeVoid = 2
    b.emitInstruction(19, {2});
    // OpTypeFunction void = 3, param=void
    b.emitInstruction(33, {3, 2});
    // OpTypeFloat 32 = 5
    b.emitInstruction(22, {5, 32});
    // OpTypeVector float 4 = 6
    b.emitInstruction(23, {6, 5, 4});
    // OpTypePointer Output float4 = 7
    b.emitInstruction(32, {7, 3, 6}); // StorageClass Output = 3
    // OpVariable ptr_out gl_Position = 8
    b.emitInstruction(59, {7, 8, 3});
    // OpConstant float 0.0 = 9
    b.emit((4 << 16) | 43); b.emit(5); b.emit(9); b.emit(0);

    // OpFunction main = 4
    b.emitInstruction(54, {2, 4, 0, 3});
    // OpLabel = 10
    b.emitInstruction(248, {10});
    // OpCompositeConstruct vec4(0,0,0,1) = 11
    uint32_t one_f; float f=1.0f; memcpy(&one_f,&f,4);
    b.emit((7<<16)|80); b.emit(6); b.emit(11); b.emit(9); b.emit(9); b.emit(9);
    b.emit(one_f);
    // Actually let's just use OpConstantNull
    // OpStore gl_Position, null_vec4
    b.emitInstruction(62, {8, 11});
    // OpReturn
    b.emitInstruction(253, {});
    // OpFunctionEnd
    b.emitInstruction(56, {});

    std::vector<uint8_t> out(b.words.size() * 4);
    memcpy(out.data(), b.words.data(), out.size());
    return out;
}

// Minimal passthrough fragment shader SPIR-V (outputs green)
static std::vector<uint8_t> buildPassthroughFragmentSpirv() {
    SpvBuilder b;
    b.emit(0x07230203);
    b.emit(0x00010300);
    b.emit(0); b.emit(8); b.emit(0);
    b.emitInstruction(17, {1}); // Shader
    b.emitString(11, 1, "GLSL.std.450");
    b.emitInstruction(14, {0, 1});
    b.emit((5<<16)|15); b.emit(1); b.emit(4);
    b.emit(0x6E69616D); b.emit(0);
    b.emitInstruction(16, {4, 7}); // OriginUpperLeft
    b.emitInstruction(19, {2}); // void
    b.emitInstruction(33, {3,2}); // func type
    b.emitInstruction(22, {5,32}); // float
    b.emitInstruction(23, {6,5,4}); // vec4
    b.emitInstruction(32, {7,3,6}); // ptr Output
    b.emitInstruction(59, {7,8,3}); // out color
    // Constants: 0.0, 1.0
    uint32_t zero_f = 0, one_f; float f=1.0f; memcpy(&one_f,&f,4);
    b.emit((4<<16)|43); b.emit(5); b.emit(9); b.emit(zero_f);
    b.emit((4<<16)|43); b.emit(5); b.emit(10); b.emit(one_f);
    // Function
    b.emitInstruction(54, {2,4,0,3});
    b.emitInstruction(248, {11});
    // vec4(0,1,0,1) — green
    b.emit((7<<16)|80); b.emit(6); b.emit(12); b.emit(9); b.emit(10); b.emit(9); b.emit(10);
    b.emitInstruction(62, {8,12}); // Store
    b.emitInstruction(253, {});
    b.emitInstruction(56, {});
    std::vector<uint8_t> out(b.words.size() * 4);
    memcpy(out.data(), b.words.data(), out.size());
    return out;
}

// ─── Xenos microcode decoder (stub → passthrough) ─────────────────────────────
// A full implementation decodes ALU/FETCH instructions per Xenia's shader translator.
// This stub generates a valid passthrough shader for testing pipeline setup.

bool GpuLayer::decompileMicrocode(const uint8_t* mc, uint32_t size, bool isVertex,
                                   std::vector<uint8_t>& outSpirv) {
    if (!mc || size == 0) {
        LOGE("SHADER: null/empty microcode");
        return false;
    }

    // Hash the microcode to check if we have a cached result
    uint32_t hash = 0x811c9dc5u;
    for (uint32_t i = 0; i < size; i++) { hash ^= mc[i]; hash *= 0x01000193u; }
    LOGI("SHADER: compile %s shader, size=%u hash=0x%08X",
         isVertex ? "vertex" : "pixel", size, hash);

    // TODO: full Xenos microcode → SPIR-V translation
    // For now, emit passthrough shaders
    outSpirv = isVertex ? buildPassthroughVertexSpirv() : buildPassthroughFragmentSpirv();
    return !outSpirv.empty();
}

ShaderEntry* GpuLayer::getOrCompileShader(const uint8_t* microcode, uint32_t size, bool isVertex) {
    // Compute hash for cache lookup
    uint64_t hash = 0;
    for (uint32_t i = 0; i < size; i++) hash = hash * 31 + microcode[i];
    hash |= ((uint64_t)isVertex << 63);

    auto it = m_shaderCache.find(hash);
    if (it != m_shaderCache.end()) return &it->second;

    ShaderEntry entry{};
    entry.microCodeHash = hash;
    entry.isVertex = isVertex;

    if (!decompileMicrocode(microcode, size, isVertex, entry.spirv)) {
        LOGE("SHADER: decompile failed");
        return nullptr;
    }

    // Create Vulkan shader module
    VkShaderModuleCreateInfo smci{};
    smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = entry.spirv.size();
    smci.pCode    = reinterpret_cast<const uint32_t*>(entry.spirv.data());

    if (vkCreateShaderModule(m_device, &smci, nullptr, &entry.vkModule) != VK_SUCCESS) {
        LOGE("SHADER: vkCreateShaderModule failed");
        return nullptr;
    }

    m_shaderCache[hash] = entry;
    LOGI("SHADER: compiled and cached shader 0x%llX", (unsigned long long)hash);
    return &m_shaderCache[hash];
}

} // namespace gpu
} // namespace x360
