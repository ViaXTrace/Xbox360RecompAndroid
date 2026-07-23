/**
 * Vulkan Renderer — instance, device, swapchain, render pass, and frame presentation.
 * Targets Vulkan 1.1+ on Android (API 28+).
 */
#include "../../include/gpu/gpu_layer.h"
#include <cstring>
#include <vector>
#include <android/log.h>

#define LOG_TAG "X360:VK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace x360 {
namespace gpu {

GpuLayer::GpuLayer() = default;

GpuLayer::~GpuLayer() {
    if (m_device) {
        vkDeviceWaitIdle(m_device);
        // Cleanup in reverse order
        if (m_inFlightFence)  vkDestroyFence(m_device, m_inFlightFence, nullptr);
        if (m_renderDoneSem)  vkDestroySemaphore(m_device, m_renderDoneSem, nullptr);
        if (m_imageAvailSem)  vkDestroySemaphore(m_device, m_imageAvailSem, nullptr);
        if (m_cmdPool)        vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
        for (auto fb : m_framebuffers) vkDestroyFramebuffer(m_device, fb, nullptr);
        for (auto iv : m_swapViews)   vkDestroyImageView(m_device, iv, nullptr);
        if (m_renderPass) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        if (m_swapchain)  vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_surface)   vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_instance)  vkDestroyInstance(m_instance, nullptr);
}

bool GpuLayer::init(ANativeWindow* window, uint8_t* guestMemory, uint64_t guestBase) {
    m_window      = window;
    m_guestMemory = guestMemory;
    m_guestBase   = guestBase;

    if (!createInstance())       { LOGE("createInstance failed");      return false; }
    if (!selectPhysicalDevice()) { LOGE("selectPhysicalDevice failed");return false; }
    if (!createLogicalDevice())  { LOGE("createLogicalDevice failed"); return false; }
    if (!createSwapchain(window)){ LOGE("createSwapchain failed");     return false; }
    if (!createRenderPass())     { LOGE("createRenderPass failed");    return false; }
    if (!createFramebuffers())   { LOGE("createFramebuffers failed");  return false; }
    if (!createCommandPool())    { LOGE("createCommandPool failed");   return false; }
    if (!createSyncObjects())    { LOGE("createSyncObjects failed");   return false; }

    m_initialized = true;
    LOGI("Vulkan renderer initialized (%ux%u)", m_swapExtent.width, m_swapExtent.height);
    return true;
}

bool GpuLayer::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Xbox360RecompAndroid";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName        = "X360Recomp";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_1;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        "VK_KHR_android_surface",
    };
    const char* layers[] = {};

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = 2;
    ci.ppEnabledExtensionNames = extensions;
    ci.enabledLayerCount       = 0;

    return vkCreateInstance(&ci, nullptr, &m_instance) == VK_SUCCESS;
}

bool GpuLayer::selectPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) return false;
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    // Prefer discrete GPU, then integrated
    for (auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        LOGI("GPU: %s", props.deviceName);
        m_physDevice = dev; // Take the first (usually only one on Android)
        break;
    }

    // Find graphics queue family
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physDevice, &queueCount, qfProps.data());

    m_graphicsQueueFamily = 0;
    for (uint32_t i = 0; i < queueCount; i++) {
        if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphicsQueueFamily = i;
            break;
        }
    }
    return m_physDevice != VK_NULL_HANDLE;
}

bool GpuLayer::createLogicalDevice() {
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = m_graphicsQueueFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &priority;

    const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = 1;
    ci.pQueueCreateInfos       = &qci;
    ci.enabledExtensionCount   = 1;
    ci.ppEnabledExtensionNames = devExts;
    ci.pEnabledFeatures        = &features;

    if (vkCreateDevice(m_physDevice, &ci, nullptr, &m_device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
    return true;
}

bool GpuLayer::createSwapchain(ANativeWindow* window) {
    // Create Android surface
    VkAndroidSurfaceCreateInfoKHR sci{};
    sci.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    sci.window = window;
    if (vkCreateAndroidSurfaceKHR(m_instance, &sci, nullptr, &m_surface) != VK_SUCCESS)
        return false;

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physDevice, m_surface, &caps);

    m_swapExtent = caps.currentExtent;
    if (m_swapExtent.width == 0xFFFFFFFF) {
        m_swapExtent = {1280, 720};
    }

    // Find surface format (prefer BGRA8_SRGB)
    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDevice, m_surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDevice, m_surface, &fmtCount, formats.data());

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }

    uint32_t imageCount = std::max(2u, caps.minImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR swci{};
    swci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swci.surface          = m_surface;
    swci.minImageCount    = imageCount;
    swci.imageFormat      = chosenFormat.format;
    swci.imageColorSpace  = chosenFormat.colorSpace;
    swci.imageExtent      = m_swapExtent;
    swci.imageArrayLayers = 1;
    swci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swci.preTransform     = caps.currentTransform;
    swci.compositeAlpha   = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    swci.presentMode      = VK_PRESENT_MODE_FIFO_KHR; // VSync
    swci.clipped          = VK_TRUE;

    if (vkCreateSwapchainKHR(m_device, &swci, nullptr, &m_swapchain) != VK_SUCCESS) return false;

    // Get swap images
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapImages.data());

    // Create image views
    m_swapViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo ivci{};
        ivci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image    = m_swapImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format   = chosenFormat.format;
        ivci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel   = 0;
        ivci.subresourceRange.levelCount     = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(m_device, &ivci, nullptr, &m_swapViews[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

bool GpuLayer::createRenderPass() {
    VkAttachmentDescription colorAttach{};
    colorAttach.format         = VK_FORMAT_B8G8R8A8_SRGB;
    colorAttach.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttach.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttach.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttach.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpci{};
    rpci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments    = &colorAttach;
    rpci.subpassCount    = 1;
    rpci.pSubpasses      = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies   = &dep;

    return vkCreateRenderPass(m_device, &rpci, nullptr, &m_renderPass) == VK_SUCCESS;
}

bool GpuLayer::createFramebuffers() {
    m_framebuffers.resize(m_swapViews.size());
    for (size_t i = 0; i < m_swapViews.size(); i++) {
        VkImageView attachments[] = { m_swapViews[i] };
        VkFramebufferCreateInfo fbci{};
        fbci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass      = m_renderPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments    = attachments;
        fbci.width           = m_swapExtent.width;
        fbci.height          = m_swapExtent.height;
        fbci.layers          = 1;
        if (vkCreateFramebuffer(m_device, &fbci, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
            return false;
    }
    return true;
}

bool GpuLayer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_graphicsQueueFamily;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_cmdPool) != VK_SUCCESS) return false;

    // Allocate one command buffer per swapchain image
    m_cmdBuffers.resize(m_swapImages.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_cmdPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_cmdBuffers.size();
    return vkAllocateCommandBuffers(m_device, &allocInfo, m_cmdBuffers.data()) == VK_SUCCESS;
}

bool GpuLayer::createSyncObjects() {
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    return vkCreateSemaphore(m_device, &sci, nullptr, &m_imageAvailSem) == VK_SUCCESS &&
           vkCreateSemaphore(m_device, &sci, nullptr, &m_renderDoneSem) == VK_SUCCESS &&
           vkCreateFence(m_device, &fci, nullptr, &m_inFlightFence) == VK_SUCCESS;
}

// ─── Frame presentation ───────────────────────────────────────────────────────

void GpuLayer::presentFrame() {
    if (!m_initialized) return;

    vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_inFlightFence);

    uint32_t imgIdx = 0;
    VkResult r = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                       m_imageAvailSem, VK_NULL_HANDLE, &imgIdx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) return; // swapchain needs recreation

    // Record a clear-color command buffer (placeholder — real rendering fills this)
    VkCommandBuffer cmd = m_cmdBuffers[imgIdx];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass        = m_renderPass;
    rpBegin.framebuffer       = m_framebuffers[imgIdx];
    rpBegin.renderArea.extent = m_swapExtent;
    rpBegin.clearValueCount   = 1;
    rpBegin.pClearValues      = &clearColor;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    // GPU draw commands are emitted here by pm4_parser dispatch
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &m_imageAvailSem;
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &m_renderDoneSem;
    vkQueueSubmit(m_graphicsQueue, 1, &submit, m_inFlightFence);

    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &m_renderDoneSem;
    present.swapchainCount     = 1;
    present.pSwapchains        = &m_swapchain;
    present.pImageIndices      = &imgIdx;
    vkQueuePresentKHR(m_graphicsQueue, &present);

    // FPS tracking
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    if (m_lastFrameNs > 0) {
        double ms = (now - (long long)m_lastFrameNs) / 1e6;
        m_fps = 1000.0f / (float)std::max(ms, 0.001);
    }
    m_lastFrameNs = (uint64_t)now;
    m_frameCount++;
}

bool GpuLayer::setSurface(ANativeWindow* window) {
    m_window = window;
    return true; // full swapchain recreation would be needed in production
}

void GpuLayer::clearSurface() {
    if (m_device) vkDeviceWaitIdle(m_device);
    m_window = nullptr;
}

void GpuLayer::onSurfaceChanged(int width, int height) {
    m_swapExtent = {(uint32_t)width, (uint32_t)height};
    LOGI("VK: surface changed %dx%d", width, height);
}

float GpuLayer::currentFps() const { return m_fps; }

bool GpuLayer::loadCustomDriver(const std::string& /*driverPath*/) {
    LOGI("VK: custom driver loading stub");
    return false;
}

bool GpuLayer::loadTurnipDriver() {
    LOGI("VK: Turnip driver loading stub (requires AdrenoTools integration)");
    return false;
}

} // namespace gpu
} // namespace x360
