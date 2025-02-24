#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "defines.hpp"
#include "vkHelper.hpp"

struct GLFWwindow;


class renderer {
public:

public:
    renderer() = default;
    renderer(GLFWwindow *window, bool vSync);
    void init();

private:
    // Instance extension
    std::vector<const char *> m_instanceExtensions = {VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME};
    std::vector<const char *> m_instanceLayers = {}; // Add extra layers here

    // Device extension
    std::vector<const char *> m_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_MESH_SHADER_EXTENSION_NAME};

    GLFWwindow *m_window{};

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
};