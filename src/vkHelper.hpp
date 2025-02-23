#pragma once
#include "defines.hpp"
#ifdef NDEBUG
#define ASSERT(condition, message)                                                                                     \
do                                                                                                                   \
{                                                                                                                    \
if(!(condition))                                                                                                   \
{                                                                                                                  \
throw std::runtime_error(message);                                                                               \
}                                                                                                                  \
} while(false)
#else
#define ASSERT(condition, message) assert((condition) && (message))
#endif

#include <vulkan/vk_enum_string_helper.h>
#include <iostream>
#ifdef NDEBUG
#define VK_CHECK(vkFnc) vkFnc
#else
#define VK_CHECK(vkFnc)                                             \
{                                                                   \
if(const VkResult checkResult = (vkFnc); checkResult != VK_SUCCESS) \
{                                                                   \
const char* errMsg = string_VkResult(checkResult);                  \
std::cerr << "Vulkan error: "<< errMsg << std::endl;                \
ASSERT(checkResult == VK_SUCCESS, errMsg);                          \
}                                                                   \
}
#endif
#include <vulkan/vulkan.hpp>

// Structs
struct Texture {
    vk::Image image;
    vk::DeviceMemory memory;
    vk::Format format;
    uvec3 dimensions = {0, 0, 0};
};

struct SampledTexture: public Texture {
    vk::Sampler sampler;
};

struct Buffer {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
};

struct Queue
{
    uint32 familyIndex = ~0U;  // Family index of the queue (graphic, compute, transfer, ...)
    uint32 queueIndex  = ~0U;  // Index of the queue in the family
    vk::Queue queue;
};



// Functions

// This returns the pipeline and access flags for a given layout, use for changing the image layout
static std::pair<vk::PipelineStageFlags2, vk::AccessFlags2> makePipelineStageAccessPairFromLayout(vk::ImageLayout state)
{
    using PipelineStageBits = vk::PipelineStageFlagBits2;
    using AccessBits = vk::AccessFlagBits2;
    switch(state)
    {
    case vk::ImageLayout::eUndefined:
        return std::make_pair(PipelineStageBits::eTopOfPipe, AccessBits::eNone);
    case vk::ImageLayout::eColorAttachmentOptimal:
        return std::make_pair(PipelineStageBits::eColorAttachmentOutput,  AccessBits::eColorAttachmentRead | AccessBits::eColorAttachmentWrite);
    case vk::ImageLayout::eShaderReadOnlyOptimal:
        return std::make_pair(PipelineStageBits::eFragmentShader | PipelineStageBits::eComputeShader | PipelineStageBits::ePreRasterizationShaders, AccessBits::eShaderRead);
    case vk::ImageLayout::eTransferDstOptimal:
        return std::make_pair(PipelineStageBits::eTransfer, AccessBits::eTransferWrite);
    case vk::ImageLayout::eGeneral:
        return std::make_pair(PipelineStageBits::eComputeShader | PipelineStageBits::eTransfer,
                               AccessBits::eMemoryRead | AccessBits::eMemoryWrite | AccessBits::eTransferWrite);
    case vk::ImageLayout::ePresentSrcKHR:
        return std::make_pair(PipelineStageBits::eColorAttachmentOutput, AccessBits::eNone);
    default: {
            ASSERT(false, "Unsupported layout transition!");
            return std::make_pair(PipelineStageBits::eAllCommands, AccessBits::eMemoryRead | AccessBits::eMemoryWrite);
    }
    }
};

// Return the barrier with the most common pair of stage and access flags for a given layout
static vk::ImageMemoryBarrier2 createImageMemoryBarrier(vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::ImageSubresourceRange subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})
{
    const auto [srcStage, srcAccess] = makePipelineStageAccessPairFromLayout(oldLayout);
    const auto [dstStage, dstAccess] = makePipelineStageAccessPairFromLayout(newLayout);

    vk::ImageMemoryBarrier2 barrier{srcStage, srcAccess, dstStage, dstAccess, oldLayout, newLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, subresourceRange};
    return barrier;
}

// Transition the image layout 
static void cmdTransitionImageLayout(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                                     vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor)
{
    const vk::ImageMemoryBarrier2 barrier = createImageMemoryBarrier(image, oldLayout, newLayout, {aspectMask, 0, 1, 0, 1});
    cmd.pipelineBarrier2({{}, {}, {}, {}, {},  1, &barrier});
}

static vk::AccessFlags2 inferAccessMaskFromStage(vk::PipelineStageFlags2 stage, bool src)
{
    vk::AccessFlags2 access{};
    
    if(stage & vk::PipelineStageFlagBits2::eComputeShader)
        access |= src ? vk::AccessFlagBits2::eShaderRead : vk::AccessFlagBits2::eShaderWrite;
    if(stage & vk::PipelineStageFlagBits2::eFragmentShader)
        access |= src ? vk::AccessFlagBits2::eShaderRead : vk::AccessFlagBits2::eShaderWrite;
    if(stage & vk::PipelineStageFlagBits2::eTransfer)
        access |= src ? vk::AccessFlagBits2::eTransferRead : vk::AccessFlagBits2::eTransferWrite;
    ASSERT((VkPipelineStageFlags2)access!=0, "Unhandled stage flag");
    return access;
}

static vk::Format findSupportedFormat(vk::PhysicalDevice physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags2 features)
{
    for(const vk::Format format : candidates)
    {
        vk::FormatProperties2 props{};
        vk::StructureChain<vk::FormatProperties2, vk::FormatProperties3> chain;
        physicalDevice.getFormatProperties2(format, &chain.get<vk::FormatProperties2>());

        if(tiling == vk::ImageTiling::eLinear && (chain.get<vk::FormatProperties3>().linearTilingFeatures & features) == features)
            return format;
        if(tiling == vk::ImageTiling::eLinear && (chain.get<vk::FormatProperties3>().linearTilingFeatures & features) == features)
            return format;
    }
    ASSERT(false, "failed to find supported format!");
    return vk::Format::eUndefined;
}

static vk::Format findDepthFormat(vk::PhysicalDevice physicalDevice)
{
    return findSupportedFormat(physicalDevice, {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint}, vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits2::eDepthStencilAttachment);
}




