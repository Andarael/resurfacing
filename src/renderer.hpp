#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "defines.hpp"
#include "vkHelper.hpp"

#include <stb_image.h>

struct GLFWwindow;

class Renderer {
public:
    vk::Instance m_instance{};
    vk::DebugUtilsMessengerEXT m_callback{VK_NULL_HANDLE};
    vk::SurfaceKHR m_surface{};
    vk::PhysicalDevice m_device{};
    vk::Device m_logicalDevice{};
    vk::Queue m_graphicsQueue{};
    vk::Queue m_presentQueue{};
    vk::CommandPool m_transientCommandPool{}; // for short lived command buffers
    vk::SwapchainKHR m_swapChain;
    vk::DescriptorPool m_descriptorPool;

    vk::Sampler m_linearSampler{};
    vk::Sampler m_nearestSampler{};

public:
    void init(GLFWwindow *window, bool vSync);
    void cleanup();
    Pipeline createPipeline(const std::vector<std::string> &p_shaderPaths, const PipelineDesc &p_pipelineDesc);
    vk::Extent2D getSwapChainExtent() const { return m_windowSize; }
    vk::CommandBuffer beginFrame();
    void beginRendering(vk::CommandBuffer p_cmd, bool p_clear = false);
    void endRendering(vk::CommandBuffer p_cmd);
    void endFrame(vk::CommandBuffer p_cmd);

    UniformBuffer createUniformBuffer(uint32 p_size);
    Buffer createStagingBuffer(uint32 p_size);
    void freeTrackedStagingBuffers();

    template <typename T>
    Buffer createAndUploadBuffer(vk::CommandBuffer p_commandBuffer, const std::vector<T> &p_data, vk::BufferUsageFlags p_usage) {
        Buffer buffer = createBufferInternal({{}, p_data.size() * sizeof(T), p_usage | vk::BufferUsageFlagBits::eTransferDst});
        uploadDataToBufferInternal(buffer, p_commandBuffer, p_data.data(), p_data.size() * sizeof(T));
        return buffer;
    }

    template <typename T>
    Texture createAndUploadTexture(vk::CommandBuffer p_commandBuffer, const std::vector<T> &p_data, uvec2 p_size, vk::Format p_format) {
        Texture texture = createTextureInternal({{}, vk::ImageType::e2D, p_format, {p_size.x,p_size.y, 1}, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst});
        uploadDataToImageInternal(texture, p_commandBuffer, p_data.data(), p_data.size() * sizeof(T));
        return texture;
    }

    template <typename T>
    void uploadToBuffer(Buffer p_dstBuffer, vk::CommandBuffer p_commandBuffer, const std::vector<T> &p_data, uint32 p_offset = 0) { uploadDataToBufferInternal(p_dstBuffer, p_commandBuffer, p_data.data(), p_data.size() * sizeof(T), p_offset); }

    template <typename T>
    void uploadToBuffer(Buffer &p_stagingBuffer, Buffer &p_dstBuffer, vk::CommandBuffer p_commandBuffer, const std::vector<T> &p_data, uint32 p_offset = 0) { uploadDataToBufferInternal(p_stagingBuffer, p_dstBuffer, p_commandBuffer, p_data.data(), p_data.size() * sizeof(T), p_offset); }

    template <typename T>
    void uploadToTexture(Texture &p_dstImage, vk::CommandBuffer p_commandBuffer, const std::vector<T> &p_data) { uploadDataToImageInternal(p_dstImage, p_commandBuffer, p_data.data(), p_data.size() * sizeof(T)); }

    template <typename T>
    void uploadToTexture(Buffer &p_stagingBuffer, Texture &p_dstImage, vk::CommandBuffer p_commandBuffer, const std::vector<T> &p_data) { uploadDataToImageInternal(p_stagingBuffer, p_dstImage, p_commandBuffer, p_data.data(), p_data.size() * sizeof(T)); }


private:
    // Instance extension
    std::vector<const char *> m_instanceExtensions = {VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME};
    std::vector<const char *> m_instanceLayers = {}; // Add extra layers here

    // Device extension
    std::vector<const char *> m_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_MESH_SHADER_EXTENSION_NAME};

    GLFWwindow *m_window{};


    QueueFamilyIndices m_queueFamilyIndices{};
    vk::PhysicalDeviceLimits m_deviceLimits;
    bool m_supportMeshQueries;

    vk::Format m_imageFormat;
    vk::Extent2D m_windowSize{};
    std::vector<Texture> m_nextImages{};
    std::vector<FrameResources> m_frameResources{};
    uint32_t m_currentFrame = 0;
    uint32_t m_nextImageIndex = 0;
    bool m_needRebuild = false;


    std::vector<FrameData> m_frameData{};
    vk::Semaphore m_FrameTimelineSemaphore{};
    uint32 m_frameRingCurrent{0};

    uint32 m_maxFramesInFlight;
    bool m_vsync{false};
    uint32 m_frameIndex{0};

    std::vector<Buffer> m_usedStagingBuffers{}; // gather for later cleanup

private:
    void createInstance();
    void createSurface();
    void selectPhysicalDevice();
    void populateQueueFamilyIndices();
    void createDeviceAndQueues();
    void createTransientCommandPool();
    vk::Extent2D createSwapchain();
    vk::Extent2D recreateSwapChain();
    void cleanupSwapChain();
    void createFrameData();
    void createDescriptorPool();
    void initImGui();
    void presentFrame();


    uint32 findMemoryType(uint32 p_typeFilter, vk::MemoryPropertyFlags p_properties) const;

    Buffer createBufferInternal(const vk::BufferCreateInfo &p_createInfo, const vk::MemoryPropertyFlags p_memProperties = vk::MemoryPropertyFlagBits::eDeviceLocal);
    Texture createTextureInternal(const vk::ImageCreateInfo &p_createInfo);
    
    void copyToStagingMem(Buffer &staging, const void *p_data, uint32 p_size);
    void uploadDataToBufferInternal(Buffer &dstBuffer, vk::CommandBuffer p_commandBuffer, const void *data, uint32 size, uint32 offset = 0);
    void uploadDataToBufferInternal(Buffer &p_stagingBuffer, Buffer &p_dstBuffer, vk::CommandBuffer p_commandBuffer, const void *p_data, uint32 p_size, uint32 p_offset = 0);
    void uploadDataToImageInternal(Texture &dstImage, vk::CommandBuffer p_commandBuffer, const void *p_data, uint32 p_size);
    void uploadDataToImageInternal(Buffer &p_stagingBuffer, Texture &p_dstImage, vk::CommandBuffer p_commandBuffer, const void *p_data, uint32 p_size);

};