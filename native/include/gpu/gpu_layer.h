#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <vulkan/vulkan.h>
#include <android/native_window.h>

namespace x360 {
namespace gpu {

// Xenos PM4 packet types (ATI R200-derived)
enum class Pm4PacketType : uint8_t {
    Type0 = 0,  // SET_CONSTANT range
    Type1 = 1,  // not used on Xenos
    Type2 = 2,  // NOP
    Type3 = 3,  // standard Xenos commands
};

enum class Pm4OpCode : uint32_t {
    DrawIndx          = 0x2D,
    DrawIndx2         = 0x36,
    DrawAutoVisibility = 0x34,
    SetConstant       = 0x2B,
    SetConstantF      = 0x39,
    SetShaderConstants = 0x12,
    SetBin            = 0x1F,
    IndirectBuffer    = 0x3F,
    NopPacket         = 0x10,
    LoadState         = 0x30,
    EventWriteEop     = 0x26,
    EventWriteZpd     = 0x38,
    Swap              = 0x36,
};

// Xenos register set (partial — key GPU state)
struct XenosGpuRegisters {
    uint32_t rbSurfaceInfo;    // Render target surface info
    uint32_t rbColorInfo;      // Color render target
    uint32_t rbColor1Info;
    uint32_t rbColor2Info;
    uint32_t rbColor3Info;
    uint32_t rbDepthInfo;
    uint32_t rbColorMask;
    uint32_t rbBlendControl[4];
    uint32_t paClVteCntl;      // Clip control / viewport transform
    uint32_t paScWindowOffset;
    uint32_t paScWindowScissorTl;
    uint32_t paScWindowScissorBr;
    uint32_t sqProgramCntl;    // Active shader program selectors
    uint32_t sqVsConstantCf;
    uint32_t sqPsConstantCf;
    uint32_t vgtDrawInitiator;
    uint32_t vgtNumIndices;
    uint32_t vgtIndexType;
    uint32_t vgtPrimType;
    uint32_t rbModeControl;
    uint32_t rbDepthControl;
    uint32_t rbColorControl;
    uint32_t rbStencilRefMask;
    float    viewportXscale, viewportXoffset;
    float    viewportYscale, viewportYoffset;
    float    viewportZscale, viewportZoffset;
};

// Compiled shader cache entry
struct ShaderEntry {
    uint64_t microCodeHash;     // Hash of Xenos shader microcode
    VkShaderModule vkModule;    // Compiled Vulkan shader module
    std::vector<uint8_t> spirv; // SPIR-V bytecode
    bool isVertex;
};

// Texture cache entry
struct TextureEntry {
    uint64_t guestAddr;
    uint32_t width, height;
    uint32_t format;    // Xenos texture format
    VkImage  image;
    VkImageView view;
    VkDeviceMemory memory;
    bool isTiled;       // Xenos tiled layout?
};

// GPU layer state machine
class GpuLayer {
public:
    GpuLayer();
    ~GpuLayer();

    // Initialize Vulkan + window surface
    bool init(ANativeWindow* window, uint8_t* guestMemory, uint64_t guestBase);

    // Set a new Android surface (on resume / surface change)
    bool setSurface(ANativeWindow* window);
    void clearSurface();

    // Main ring buffer processing — call from the GPU thread
    void processPm4RingBuffer(const uint32_t* ringBuf, uint32_t sizeWords);

    // Called by HLE on VBlank / swap — present the frame
    void presentFrame();

    // Resize viewport
    void onSurfaceChanged(int width, int height);

    bool isInitialized() const { return m_initialized; }
    float currentFps() const;

    // Driver loading (AdrenoTools / ExynosTools)
    bool loadCustomDriver(const std::string& driverPath);
    bool loadTurnipDriver();

private:
    // ── Vulkan init ────────────────────────────────────────────────────────────
    bool createInstance();
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool createSwapchain(ANativeWindow* window);
    bool createRenderPass();
    bool createFramebuffers();
    bool createCommandPool();
    bool createSyncObjects();

    // ── PM4 dispatch ──────────────────────────────────────────────────────────
    void dispatchType3(Pm4OpCode op, const uint32_t* params, uint32_t count);
    void handleDrawIndx(const uint32_t* params);
    void handleSetConstant(const uint32_t* params, uint32_t count);
    void handleSwap();
    void handleIndirectBuffer(uint32_t ptr, uint32_t size);

    // ── Shader recompiler ─────────────────────────────────────────────────────
    ShaderEntry* getOrCompileShader(const uint8_t* microcode, uint32_t size, bool isVertex);
    bool decompileMicrocode(const uint8_t* mc, uint32_t size, bool isVertex,
                            std::vector<uint8_t>& outSpirv);

    // ── Texture management ─────────────────────────────────────────────────────
    TextureEntry* getOrUploadTexture(uint64_t guestAddr, uint32_t width,
                                     uint32_t height, uint32_t format);
    void detileTexture(const uint8_t* src, uint8_t* dst, uint32_t width,
                       uint32_t height, uint32_t format);
    bool transcodeBcToAstc(const uint8_t* src, uint8_t* dst,
                           uint32_t width, uint32_t height, uint32_t format);

    // ── eDRAM simulation ───────────────────────────────────────────────────────
    void resolveEdram();

    // Vulkan handles
    VkInstance       m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;
    VkDevice         m_device = VK_NULL_HANDLE;
    VkQueue          m_graphicsQueue = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface = VK_NULL_HANDLE;
    VkSwapchainKHR   m_swapchain = VK_NULL_HANDLE;
    VkRenderPass     m_renderPass = VK_NULL_HANDLE;
    VkCommandPool    m_cmdPool = VK_NULL_HANDLE;
    std::vector<VkImage>       m_swapImages;
    std::vector<VkImageView>   m_swapViews;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_cmdBuffers;
    VkSemaphore m_imageAvailSem = VK_NULL_HANDLE;
    VkSemaphore m_renderDoneSem = VK_NULL_HANDLE;
    VkFence     m_inFlightFence = VK_NULL_HANDLE;
    uint32_t    m_graphicsQueueFamily = 0;
    VkExtent2D  m_swapExtent{};

    // Guest memory
    uint8_t*  m_guestMemory = nullptr;
    uint64_t  m_guestBase = 0;

    // GPU register state
    XenosGpuRegisters m_regs{};

    // Caches
    std::unordered_map<uint64_t, ShaderEntry>  m_shaderCache;
    std::unordered_map<uint64_t, TextureEntry> m_textureCache;

    // Frame timing
    uint64_t m_lastFrameNs = 0;
    float    m_fps = 0.0f;
    uint32_t m_frameCount = 0;

    bool m_initialized = false;
    ANativeWindow* m_window = nullptr;
};

} // namespace gpu
} // namespace x360
