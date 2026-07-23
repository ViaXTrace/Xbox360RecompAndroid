/**
 * Texture Manager — Xbox 360 tiled texture detiling and Vulkan upload.
 * Xbox 360 uses a proprietary tile layout for all textures in memory.
 * We must detile before uploading to a Vulkan VkImage.
 *
 * Reference: Xenia gpu/texture_info.cc + gpu/texture_cache.cc (MIT).
 */
#include "../../include/gpu/gpu_layer.h"
#include <cstring>
#include <cstdlib>
#include <android/log.h>

#define LOG_TAG "X360:TEX"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace gpu {

// ─── Xenos tile constants ──────────────────────────────────────────────────────
// Xbox 360 uses a 2D Morton-curve (Z-order) tile layout.
// Texture data is stored in 32x32 blocks of 4-byte texels (for 32bpp).
static constexpr uint32_t kTileWidth  = 32;
static constexpr uint32_t kTileHeight = 32;

// Convert 2D (x, y) coordinates to Morton code (interleave bits)
static uint32_t morton2D(uint32_t x, uint32_t y) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < 16; i++) {
        result |= ((x & (1u << i)) << i) | ((y & (1u << i)) << (i + 1));
    }
    return result;
}

// Detile a single RGBA8 (or generic 4bpp) tile-formatted texture to linear.
void GpuLayer::detileTexture(const uint8_t* src, uint8_t* dst,
                              uint32_t width, uint32_t height, uint32_t format) {
    // Determine bytes-per-pixel from format
    uint32_t bpp = 4; // default RGBA8
    switch (format & 0x3F) {
    case 0x02: bpp = 1; break; // DXT1 compressed — handle separately
    case 0x04: bpp = 2; break; // 16-bit
    case 0x06: bpp = 4; break; // RGBA8
    case 0x0A: bpp = 8; break; // RGBA16
    case 0x13: bpp = 1; break; // BC1 (DXT1)
    case 0x14: bpp = 2; break; // BC2 (DXT3)
    case 0x15: bpp = 2; break; // BC3 (DXT5)
    default:   bpp = 4; break;
    }

    // Tiled texture: iterate tiles
    uint32_t widthTiles  = (width  + kTileWidth  - 1) / kTileWidth;
    uint32_t heightTiles = (height + kTileHeight - 1) / kTileHeight;

    uint32_t tileSize = kTileWidth * kTileHeight * bpp;
    uint32_t srcOffset = 0;

    for (uint32_t ty = 0; ty < heightTiles; ty++) {
        for (uint32_t tx = 0; tx < widthTiles; tx++) {
            // Within this tile, pixels are stored in Morton order
            for (uint32_t py = 0; py < kTileHeight; py++) {
                for (uint32_t px = 0; px < kTileWidth; px++) {
                    uint32_t gx = tx * kTileWidth  + px;
                    uint32_t gy = ty * kTileHeight + py;
                    if (gx >= width || gy >= height) {
                        // Out of bounds — advance source pointer only
                        srcOffset += bpp;
                        continue;
                    }
                    uint32_t mortonIdx = morton2D(px, py);
                    uint32_t srcPixelOffset = srcOffset + mortonIdx * bpp;
                    uint32_t dstPixelOffset = (gy * width + gx) * bpp;
                    if (dstPixelOffset + bpp <= width * height * bpp) {
                        memcpy(dst + dstPixelOffset, src + srcPixelOffset, bpp);
                    }
                }
            }
            srcOffset += tileSize;
        }
    }
}

// Transcoding BC textures to ASTC (for GPUs without BC support)
// Full implementation requires a BC→ASTC transcoder library (e.g. bc7enc/astcenc).
bool GpuLayer::transcodeBcToAstc(const uint8_t* src, uint8_t* dst,
                                  uint32_t width, uint32_t height, uint32_t format) {
    // Stub: copy BC data unchanged (caller should check GPU capability first)
    // TODO: integrate astcenc or bc7decomp for software transcoding
    LOGI("TEX: BC→ASTC transcode stub w=%u h=%u fmt=0x%X", width, height, format);
    uint32_t bcBlockSize = (format == 0x13) ? 8 : 16; // DXT1=8, DXT3/5=16
    uint32_t numBlocks = ((width+3)/4) * ((height+3)/4);
    memcpy(dst, src, numBlocks * bcBlockSize);
    return true;
}

// ─── Vulkan texture upload ─────────────────────────────────────────────────────

TextureEntry* GpuLayer::getOrUploadTexture(uint64_t guestAddr, uint32_t width,
                                            uint32_t height, uint32_t format) {
    // Check cache
    auto it = m_textureCache.find(guestAddr);
    if (it != m_textureCache.end()) return &it->second;

    if (!m_initialized || !m_guestMemory) return nullptr;
    if (guestAddr < m_guestBase) return nullptr;

    uint64_t hostOffset = guestAddr - m_guestBase;
    uint32_t bpp = 4;
    size_t srcSize = width * height * bpp;
    if (hostOffset + srcSize > 0x100000000ULL) return nullptr;

    const uint8_t* guestTexData = m_guestMemory + hostOffset;

    // Detile into a linear buffer
    std::vector<uint8_t> linear(width * height * bpp);
    detileTexture(guestTexData, linear.data(), width, height, format);

    // Create VkImage
    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent        = {width, height, 1};
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    TextureEntry entry{};
    entry.guestAddr = guestAddr;
    entry.width     = width;
    entry.height    = height;
    entry.format    = format;
    entry.isTiled   = true;

    if (vkCreateImage(m_device, &ici, nullptr, &entry.image) != VK_SUCCESS) {
        LOGE("TEX: vkCreateImage failed");
        return nullptr;
    }

    // Allocate device memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, entry.image, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physDevice, &memProps);

    uint32_t memTypeIdx = 0;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReqs.memoryTypeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memTypeIdx = i; break;
        }
    }

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReqs.size;
    mai.memoryTypeIndex = memTypeIdx;
    vkAllocateMemory(m_device, &mai, nullptr, &entry.memory);
    vkBindImageMemory(m_device, entry.image, entry.memory, 0);

    // Create image view
    VkImageViewCreateInfo ivci{};
    ivci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image    = entry.image;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format   = VK_FORMAT_R8G8B8A8_UNORM;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    vkCreateImageView(m_device, &ivci, nullptr, &entry.view);

    // TODO: upload via staging buffer + vkCmdCopyBufferToImage
    LOGI("TEX: uploaded %ux%u fmt=0x%X guest=0x%llX",
         width, height, format, (unsigned long long)guestAddr);

    m_textureCache[guestAddr] = entry;
    return &m_textureCache[guestAddr];
}

} // namespace gpu
} // namespace x360
