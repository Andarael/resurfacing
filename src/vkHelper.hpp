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
    vk::ImageView defaultView;

};

struct SampledTexture : public Texture {
    vk::Sampler sampler;
    SampledTexture() = default;
    SampledTexture(Texture t) : Texture(t) {}
};

struct Buffer {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    vk::DeviceSize size = 0; // size in bytes
};

struct UniformBuffer : public Buffer {
    void* mappedMemory = nullptr;
    UniformBuffer() = default;
    UniformBuffer(Buffer b) : Buffer(b) {} // should check if the buffer is a UBO
};

struct QueueFamilyIndices {
    uint32 presentQueueIndex = ~0U;
    uint32 graphicsQueueIndex = ~0U;
    uint32 computeQueueIndex = ~0U;
    uint32 transferQueueIndex = ~0U;

    bool isComplete() const { return presentQueueIndex != ~0U && graphicsQueueIndex != ~0U && computeQueueIndex != ~0U && transferQueueIndex != ~0U; }
};

struct FrameResources {
    vk::Semaphore imageAvailableSemaphore; // Signals when the image is ready for rendering
    vk::Semaphore renderFinishedSemaphore; // Signals when rendering is finished
};

struct FrameData {
    vk::CommandPool commandPool;
    vk::CommandBuffer commandBuffer;
    uint64 frameNumber;
};

struct PipelineDesc {
    vk::PipelineMultisampleStateCreateInfo multisampleStateCreateInfo{{}, vk::SampleCountFlagBits::e1, VK_FALSE};
    vk::PipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{{}, VK_FALSE, VK_FALSE, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f};
    vk::PipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{{}, 0, nullptr, 0, nullptr};
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{{}, vk::PrimitiveTopology::eTriangleList, VK_FALSE};
    vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{{}, VK_TRUE, VK_TRUE, vk::CompareOp::eLess, VK_FALSE, VK_FALSE, {}, {}, 0.0f, 1.0f};
    vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{VK_FALSE, vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd , vk::BlendFactor::eZero, vk::BlendFactor::eZero, vk::BlendOp::eAdd, vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
    vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{{}, VK_FALSE, vk::LogicOp::eCopy, 1, &colorBlendAttachmentState, {0.0f, 0.0f, 0.0f, 0.0f}};
    std::array<vk::DynamicState,2> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
};

struct Pipeline {
    vk::PipelineLayout layout;
    vk::Pipeline pipeline;
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

/*-- Simple helper for the creation of a temporary command buffer, use to record the commands to upload data, or transition images. -*/
static vk::CommandBuffer beginSingleTimeCommands(vk::Device p_logicalDevice, vk::CommandPool p_commandPool) {
    const vk::CommandBufferAllocateInfo allocInfo(p_commandPool, vk::CommandBufferLevel::ePrimary, 1);
    vk::CommandBuffer commandBuffer{};
    VK_CHECK(p_logicalDevice.allocateCommandBuffers(&allocInfo, &commandBuffer));
    commandBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    return commandBuffer;
}

/*-- 
 * Submit the temporary command buffer, wait until the command is finished, and clean up. 
 * This is a blocking function and should be used only for small operations 
--*/
static void endSingleTimeCommands(vk::CommandBuffer p_commandBuffer, vk::Device p_logicalDevice, vk::CommandPool p_commandPool, vk::Queue p_queue) {
    p_commandBuffer.end();
    const vk::FenceCreateInfo fenceInfo{};
    std::array<vk::Fence, 1> fence{};
    VK_CHECK(p_logicalDevice.createFence(&fenceInfo, nullptr, fence.data()));

    const vk::CommandBufferSubmitInfo cmdBufferInfo{p_commandBuffer};
    const std::array<vk::SubmitInfo2, 1> submitInfo{vk::SubmitInfo2{{}, 0, nullptr, 0, &cmdBufferInfo, 0, nullptr}};
    VK_CHECK(p_queue.submit2(uint32(submitInfo.size()), submitInfo.data(), fence[0]));
    VK_CHECK(p_logicalDevice.waitForFences(uint32(fence.size()), fence.data(), true, UINT64_MAX));

    // Cleanup
    p_logicalDevice.destroyFence(fence[0], nullptr);
    p_logicalDevice.freeCommandBuffers(p_commandPool, 1, &p_commandBuffer);
}

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
        if (tiling == vk::ImageTiling::eOptimal && (chain.get<vk::FormatProperties3>().optimalTilingFeatures & features) == features)
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


static vk::SurfaceFormat2KHR selectSwapSurfaceFormat(const std::vector<vk::SurfaceFormat2KHR> &availableFormats) {
    if (availableFormats.size() == 1 && availableFormats[0].surfaceFormat.format == vk::Format::eUndefined) { return {{vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear}}; }

    const std::vector<vk::SurfaceFormat2KHR> preferredFormats = {
        {{vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear}},
        {{vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear}}};

    // Check available formats against the preferred formats.
    for (const auto &preferredFormat : preferredFormats) {
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.surfaceFormat.format == preferredFormat.surfaceFormat.format
                && availableFormat.surfaceFormat.colorSpace == preferredFormat.surfaceFormat.colorSpace) { return availableFormat; }
        }
    }

    return availableFormats[0];
}

static vk::PresentModeKHR selectSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes, bool vsync) {
    // Fifo is the only mode guaranteed to be available
    if (vsync) { return vk::PresentModeKHR::eFifo; }

    bool mailboxSupported = false, immediateSupported = false;

    for (vk::PresentModeKHR mode : availablePresentModes) {
        if (mode == vk::PresentModeKHR::eMailbox)
            mailboxSupported = true;
        if (mode == vk::PresentModeKHR::eImmediate)
            immediateSupported = true;
    }

    if (mailboxSupported) {
        return vk::PresentModeKHR::eMailbox; // vsync presentation, render as fast as possible
    }

    if (immediateSupported) {
        return vk::PresentModeKHR::eImmediate; // presentation as soon as possible
    }

    return vk::PresentModeKHR::eFifo; // Only mode guaranteed to be available
}

// function to read file as binary blob
static std::vector<byte> readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate); // Open at end to get file size
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    
    std::streamsize fileSize = file.tellg(); // Get size
    file.seekg(0, std::ios::beg); // Move back to beginning
    
    std::vector<byte> buffer(fileSize);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        throw std::runtime_error("Failed to read file: " + filename);
    }
    return buffer;
}

static vk::ShaderStageFlagBits inferShaderStageFromExt(std::string filename) {
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    if (ext == "vert")
        return vk::ShaderStageFlagBits::eVertex;
    if (ext == "frag")
        return vk::ShaderStageFlagBits::eFragment;
    if (ext == "comp")
        return vk::ShaderStageFlagBits::eCompute;
    if (ext == "tesc")
        return vk::ShaderStageFlagBits::eTessellationControl;
    if (ext == "tese")
        return vk::ShaderStageFlagBits::eTessellationEvaluation;
    if (ext == "geom")
        return vk::ShaderStageFlagBits::eGeometry;
    if (ext == "mesg")
        return vk::ShaderStageFlagBits::eMeshEXT;
    if (ext == "task") 
        return vk::ShaderStageFlagBits::eTaskEXT;
    if (ext == "rgen") 
        return vk::ShaderStageFlagBits::eRaygenKHR;
    if (ext == "rint") 
        return vk::ShaderStageFlagBits::eIntersectionKHR;
    if (ext == "rahit") 
        return vk::ShaderStageFlagBits::eAnyHitKHR;
    if (ext == "rchit") 
        return vk::ShaderStageFlagBits::eClosestHitKHR;
    if (ext == "rmiss") 
        return vk::ShaderStageFlagBits::eMissKHR;
    if (ext == "rcall") 
        return vk::ShaderStageFlagBits::eCallableKHR;
    
    ASSERT(false, "Unsupported shader stage");
    return vk::ShaderStageFlagBits::eAll;
}

    