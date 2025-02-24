#pragma once
#include "defines.hpp"
#ifdef NDEBUG
#define ASSERT(condition, message) \
do                                 \
{                                  \
if(!(condition))                   \
{                                  \
throw std::runtime_error(message); \
}                                  \
} while(false)
#else
#define ASSERT(condition, message) assert((condition) && (message))
#endif

#include <vulkan/vk_enum_string_helper.h>
#include <iostream>
#ifdef NDEBUG
#define VK_CHECK(vkFnc) vkFnc;
#else
#define VK_CHECK(vkFnc)                                                         \
{                                                                               \
if(const vk::Result checkResult = (vkFnc); checkResult != vk::Result::eSuccess) \
{                                                                               \
const char* errMsg = string_VkResult((VkResult)checkResult);                    \
std::cerr << "Vulkan error: "<< errMsg << std::endl;                            \
ASSERT(checkResult == vk::Result::eSuccess, errMsg);                            \
}                                                                               \
}
#endif
#include <fstream>
#include <vulkan/vulkan.hpp>

// Structs
struct Texture {
    vk::Image image;
    vk::DeviceMemory memory;
    vk::Format format;
    uvec3 dimensions = {0, 0, 0};

};

struct SampledTexture : public Texture {
    vk::Sampler sampler;
};

struct Buffer {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
};

struct QueueFamilyIndices {
    uint32 presentQueueIndex = ~0U;
    uint32 graphicsQueueIndex = ~0U;
    uint32 computeQueueIndex = ~0U;
    uint32 transferQueueIndex = ~0U;

    bool isComplete() const { return presentQueueIndex != ~0U && graphicsQueueIndex != ~0U && computeQueueIndex != ~0U && transferQueueIndex != ~0U; }
};

// Functions

// This returns the pipeline and access flags for a given layout, use for changing the image layout
static std::pair<vk::PipelineStageFlags2, vk::AccessFlags2> makePipelineStageAccessPairFromLayout(vk::ImageLayout state) {
    using PipelineStageBits = vk::PipelineStageFlagBits2;
    using AccessBits = vk::AccessFlagBits2;
    switch (state) {
    case vk::ImageLayout::eUndefined: return std::make_pair(PipelineStageBits::eTopOfPipe, AccessBits::eNone);
    case vk::ImageLayout::eColorAttachmentOptimal: return std::make_pair(PipelineStageBits::eColorAttachmentOutput, AccessBits::eColorAttachmentRead | AccessBits::eColorAttachmentWrite);
    case vk::ImageLayout::eShaderReadOnlyOptimal: return std::make_pair(PipelineStageBits::eFragmentShader | PipelineStageBits::eComputeShader | PipelineStageBits::ePreRasterizationShaders, AccessBits::eShaderRead);
    case vk::ImageLayout::eTransferDstOptimal: return std::make_pair(PipelineStageBits::eTransfer, AccessBits::eTransferWrite);
    case vk::ImageLayout::eGeneral: return std::make_pair(PipelineStageBits::eComputeShader | PipelineStageBits::eTransfer,
                                                          AccessBits::eMemoryRead | AccessBits::eMemoryWrite | AccessBits::eTransferWrite);
    case vk::ImageLayout::ePresentSrcKHR: return std::make_pair(PipelineStageBits::eColorAttachmentOutput, AccessBits::eNone);
    default: {
        ASSERT(false, "Unsupported layout transition!");
        return std::make_pair(PipelineStageBits::eAllCommands, AccessBits::eMemoryRead | AccessBits::eMemoryWrite);
    }
    }
};

// Return the barrier with the most common pair of stage and access flags for a given layout
static vk::ImageMemoryBarrier2 createImageMemoryBarrier(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageSubresourceRange subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}) {
    const auto [srcStage, srcAccess] = makePipelineStageAccessPairFromLayout(oldLayout);
    const auto [dstStage, dstAccess] = makePipelineStageAccessPairFromLayout(newLayout);

    vk::ImageMemoryBarrier2 barrier{srcStage, srcAccess, dstStage, dstAccess, oldLayout, newLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, subresourceRange};
    return barrier;
}

// Transition the image layout 
static void cmdTransitionImageLayout(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                     vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) {
    const vk::ImageMemoryBarrier2 barrier = createImageMemoryBarrier(image, oldLayout, newLayout, {aspectMask, 0, 1, 0, 1});
    cmd.pipelineBarrier2({{}, {}, {}, {}, {}, 1, &barrier});
}

static vk::AccessFlags2 inferAccessMaskFromStage(vk::PipelineStageFlags2 stage, bool src) {
    vk::AccessFlags2 access{};

    if (stage & vk::PipelineStageFlagBits2::eComputeShader)
        access |= src ? vk::AccessFlagBits2::eShaderRead : vk::AccessFlagBits2::eShaderWrite;
    if (stage & vk::PipelineStageFlagBits2::eFragmentShader)
        access |= src ? vk::AccessFlagBits2::eShaderRead : vk::AccessFlagBits2::eShaderWrite;
    if (stage & vk::PipelineStageFlagBits2::eTransfer)
        access |= src ? vk::AccessFlagBits2::eTransferRead : vk::AccessFlagBits2::eTransferWrite;
    ASSERT((VkPipelineStageFlags2)access!=0, "Unhandled stage flag");
    return access;
}

static vk::Format findSupportedFormat(vk::PhysicalDevice physicalDevice, const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags2 features) {
    for (const vk::Format format : candidates) {
        vk::FormatProperties2 props{};
        vk::StructureChain<vk::FormatProperties2, vk::FormatProperties3> chain;
        physicalDevice.getFormatProperties2(format, &chain.get<vk::FormatProperties2>());

        if (tiling == vk::ImageTiling::eLinear && (chain.get<vk::FormatProperties3>().linearTilingFeatures & features) == features)
            return format;
        if (tiling == vk::ImageTiling::eLinear && (chain.get<vk::FormatProperties3>().linearTilingFeatures & features) == features)
            return format;
    }
    ASSERT(false, "failed to find supported format!");
    return vk::Format::eUndefined;
}

static vk::Format findDepthFormat(vk::PhysicalDevice p_device) { return findSupportedFormat(p_device, {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint}, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits2::eDepthStencilAttachment); }

static std::vector<vk::ExtensionProperties> getAvailableInstanceExtensions() {
    uint32_t count = 0;
    VK_CHECK(vk::enumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    std::vector<vk::ExtensionProperties> instanceExtensions(count);
    VK_CHECK(vk::enumerateInstanceExtensionProperties(nullptr, &count, instanceExtensions.data()));
    return instanceExtensions;
}

static std::vector<vk::ExtensionProperties> getAvailableDeviceExtensions(const vk::PhysicalDevice &p_device) {
    uint32_t count = 0;
    VK_CHECK(p_device.enumerateDeviceExtensionProperties(nullptr, &count, nullptr));
    std::vector<vk::ExtensionProperties> deviceExtensions(count);
    VK_CHECK(p_device.enumerateDeviceExtensionProperties(nullptr, &count, deviceExtensions.data()));
    return deviceExtensions;
}

static bool extensionIsAvailable(const std::string &name, const std::vector<vk::ExtensionProperties> &availableExtensions) {
    for (auto &ext : availableExtensions) {
        if (name == ext.extensionName)
            return true;
    }
    return false;
}

VKAPI_ATTR static VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
    std::ofstream perfLog("performance.log", std::ios_base::app);
    std::ofstream clutterLog("clutter.log", std::ios_base::app);

    std::string message(pCallbackData->pMessage);

    if (message.find("ReShade") != std::string::npos ||
        message.find("obs-studio-hook") != std::string::npos ||
        message.find("GalaxyOverlayVkLayer") != std::string::npos) { if (clutterLog) { clutterLog << message << std::endl; } } else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: std::cout << "debug: " << message << std::endl;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: std::cout << "info: " << message << std::endl;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: std::cout << "warning: " << message << std::endl;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: std::cout << "error: " << message << std::endl;
            throw std::runtime_error("Validation error.");
            break;
        default: std::cout << "unknown: " << message << std::endl;
            break;
        }
    } else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) { std::cout << "general: " << message << std::endl; } else if (messageType == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) { if (perfLog) { perfLog << "performance: " << message << std::endl; } } else { std::cout << "unknown: " << message << std::endl; }

    return VK_FALSE;
}

static void setupDebugMessenger(vk::Instance p_instance, vk::DebugUtilsMessengerEXT &p_debugMessenger) {
    using MessageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MessageType = vk::DebugUtilsMessageTypeFlagBitsEXT;
    vk::DebugUtilsMessengerCreateInfoEXT createInfoDebugMessenger{};
    createInfoDebugMessenger = vk::DebugUtilsMessengerCreateInfoEXT({}, MessageSeverity::eVerbose | MessageSeverity::eWarning | MessageSeverity::eError, MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance, debugCallback);
    p_debugMessenger = p_instance.createDebugUtilsMessengerEXT(createInfoDebugMessenger);
}


static bool isDeviceSuitable(vk::PhysicalDevice p_device, const std::vector<const char *> &p_requiredDeviceExtentions) {
    vk::PhysicalDeviceProperties2 prop = p_device.getProperties2();
    std::string deviceName = prop.properties.deviceName;

    // check for discrete GPU
    if (prop.properties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu) {
        std::cout << "Device " << deviceName << " is not a discrete GPU." << '\n';
        return false;
    }

    std::vector<vk::ExtensionProperties> availableExtentions = getAvailableDeviceExtensions(p_device);
    bool allExtensionsAvailable = true;
    for (const char *requiredExtension : p_requiredDeviceExtentions) {
        allExtensionsAvailable &= extensionIsAvailable(requiredExtension, availableExtentions);
        if (allExtensionsAvailable == false) {
            std::cout << "Device " << deviceName << " is missing required extensions." << '\n';
            return false;
        }
    }

    std::cout << "Device " << deviceName << " is suitable." << '\n';
    return true;
}