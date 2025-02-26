#include "renderer.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <set>

#include "config.hpp"

renderer::renderer(GLFWwindow *window, bool vSync) {
    m_window = window;
    init();
}

uint32 renderer::findMemoryType(uint32 p_typeFilter, vk::MemoryPropertyFlags p_properties) const {
    vk::PhysicalDeviceMemoryProperties memProperties;
    m_device.getMemoryProperties(&memProperties);
    for (uint32 i = 0; i < memProperties.memoryTypeCount; i++) { if ((p_typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & p_properties) == p_properties) { return i; } }
    throw std::runtime_error("Failed to find suitable memory type.");
}

Buffer renderer::createBufferInternal(const vk::BufferCreateInfo &p_createInfo, const vk::MemoryPropertyFlags p_memProperties) {
    Buffer res{};
    res.size = p_createInfo.size;
    VK_CHECK(m_logicalDevice.createBuffer(&p_createInfo, nullptr, &res.buffer));
    vk::StructureChain<vk::MemoryRequirements2> memRequirements = m_logicalDevice.getBufferMemoryRequirements2(vk::BufferMemoryRequirementsInfo2(res.buffer));
    vk::MemoryAllocateInfo allocInfo(memRequirements.get<vk::MemoryRequirements2>().memoryRequirements.size, findMemoryType(memRequirements.get<vk::MemoryRequirements2>().memoryRequirements.memoryTypeBits, p_memProperties));
    VK_CHECK(m_logicalDevice.allocateMemory(&allocInfo, nullptr, &res.memory));
    m_logicalDevice.bindBufferMemory(res.buffer, res.memory, 0);
    return res;
}

Buffer renderer::createStagingBuffer(const void *p_data, uint32 p_size) {
    vk::BufferCreateInfo bufferCreateInfo({}, p_size, vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive);
    Buffer staging = createBufferInternal(bufferCreateInfo, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    m_usedStagingBuffers.push_back(staging); // track the buffer

    // map the buffer and copy the data
    void* mappedData;
    vk::MemoryMapInfo mappedInfo = {{}, staging.memory, 0, p_size};
    VK_CHECK(m_logicalDevice.mapMemory2(&mappedInfo, &mappedData));
    memcpy(mappedData, p_data, p_size);
    vk::MemoryUnmapInfo unmapInfo = {{}, staging.memory};
    VK_CHECK(m_logicalDevice.unmapMemory2(&unmapInfo));
    return staging;
}

void renderer::freeStagingBuffers() {
    for(const Buffer &buffer : m_usedStagingBuffers)
    {
        m_logicalDevice.destroy(buffer.buffer);
        m_logicalDevice.free(buffer.memory);
    }
    m_usedStagingBuffers.clear();
}

Texture renderer::createTextureInternal(const vk::ImageCreateInfo &p_createInfo) {
    Texture res{};
    VK_CHECK(m_logicalDevice.createImage(&p_createInfo, nullptr, &res.image));
    vk::StructureChain<vk::MemoryRequirements2> memRequirements = m_logicalDevice.getImageMemoryRequirements2(vk::ImageMemoryRequirementsInfo2(res.image));
    vk::MemoryAllocateInfo allocInfo(memRequirements.get<vk::MemoryRequirements2>().memoryRequirements.size, findMemoryType(memRequirements.get<vk::MemoryRequirements2>().memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    VK_CHECK(m_logicalDevice.allocateMemory(&allocInfo, nullptr, &res.memory));
    m_logicalDevice.bindImageMemory(res.image, res.memory, 0);
    return res;
}


void renderer::init() {
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
    createInstance();
    createSurface();
    selectPhysicalDevice();
    createDeviceAndQueues();
    createTransientCommandPool();
    m_windowSize = createSwapchain();
    createFrameData();
    createDescriptorPool();
    initImGui();
    {
        vk::SamplerCreateInfo samplerCreateInfo = {{}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
        VK_CHECK(m_logicalDevice.createSampler(&samplerCreateInfo, nullptr, &m_linearSampler));
        samplerCreateInfo = {{}, vk::Filter::eNearest, vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest};
        VK_CHECK(m_logicalDevice.createSampler(&samplerCreateInfo, nullptr, &m_nearestSampler));
    }
    buildRenderTargets();
}

void renderer::createInstance() {
    uint32 glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<vk::ExtensionProperties> availableInstanceExtentions = getAvailableInstanceExtensions();

    const vk::ApplicationInfo appInfo("Resurfacing", VK_MAKE_VERSION(1, 0, 0), "Resurfacing", VK_MAKE_VERSION(1, 0, 0),
                                      VK_API_VERSION_1_3);

    m_instanceExtensions.insert(m_instanceExtensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);
    bool debugUtilsAvailable = extensionIsAvailable(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, availableInstanceExtentions);
    if (debugUtilsAvailable)
        m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (extensionIsAvailable(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME, availableInstanceExtentions))
        m_instanceExtensions.push_back(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);

    if (enableValidationLayers) { m_instanceLayers.push_back("VK_LAYER_KHRONOS_validation"); }
    vk::InstanceCreateInfo createInfo({}, &appInfo, m_instanceLayers, m_instanceExtensions);
    VK_CHECK(vk::createInstance(&createInfo, nullptr, &m_instance));
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);

    if (enableValidationLayers && debugUtilsAvailable)
        setupDebugMessenger(m_instance, m_callback);
}

void renderer::createSurface() {
    // TODO: integrate error handler with GLFW
    VK_CHECK(vk::Result(glfwCreateWindowSurface(m_instance, m_window, nullptr, reinterpret_cast<VkSurfaceKHR *>(&m_surface))));
}

void renderer::selectPhysicalDevice() {
    size_t chosenDevice = 0;
    uint32 deviceCount = 0;

    VK_CHECK(m_instance.enumeratePhysicalDevices(&deviceCount, nullptr));
    ASSERT(deviceCount != 0, "failed to find GPUs with Vulkan support!");

    std::vector<vk::PhysicalDevice> physicalDevices(deviceCount);
    VK_CHECK(m_instance.enumeratePhysicalDevices(&deviceCount, physicalDevices.data()));

    vk::PhysicalDeviceProperties2 properties2{};
    for (uint32 i = 0; i < physicalDevices.size(); i++) {
        if (isDeviceSuitable(physicalDevices[i], m_deviceExtensions)) {
            chosenDevice = i;
            break;
        }
    }

    m_device = physicalDevices[chosenDevice];
    populateQueueFamilyIndices();
    m_device.getProperties2(&properties2);
    m_deviceLimits = properties2.properties.limits;
    std::cout << "Selected GPU: " << properties2.properties.deviceName << "\n";
    std::cout << "Driver: " << VK_VERSION_MAJOR(properties2.properties.driverVersion) << "." <<
        VK_VERSION_MINOR(properties2.properties.driverVersion) << "." << VK_VERSION_PATCH(
            properties2.properties.driverVersion) << "\n";
    std::cout << "Vulkan API: " << VK_VERSION_MAJOR(properties2.properties.apiVersion) << "." <<
        VK_VERSION_MINOR(properties2.properties.apiVersion) << "." << VK_VERSION_PATCH(
            properties2.properties.apiVersion) << "\n";
}

void renderer::populateQueueFamilyIndices() {
    // find a graphics, compute and present queue (can be de same)
    std::vector<vk::QueueFamilyProperties2> queueFamilies = m_device.getQueueFamilyProperties2();
    uint32 i = 0;
    while (!m_queueFamilyIndices.isComplete() && i < queueFamilies.size()) {
        // very simple policy of selecting the first queue that supports the required operations, not ideal but works for now
        if (m_device.getSurfaceSupportKHR(i, m_surface)) { m_queueFamilyIndices.presentQueueIndex = i; }
        if ((queueFamilies[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) && (queueFamilies[i].queueFamilyProperties.timestampValidBits != 0)) { m_queueFamilyIndices.graphicsQueueIndex = i; } // ensure the queue supports timestamp queries
        if ((queueFamilies[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eCompute) && (queueFamilies[i].queueFamilyProperties.timestampValidBits != 0)) { m_queueFamilyIndices.computeQueueIndex = i; } // ensure the queue supports timestamp queries
        if (queueFamilies[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eTransfer) { m_queueFamilyIndices.transferQueueIndex = i; }
        i++;
    }
}

void renderer::createDeviceAndQueues() {
    std::vector<vk::DeviceQueueCreateInfo> queuesCreateInfo;
    float queuePriority = 1.0f;
    std::set<uint32> uniqueQueueFamilies = {m_queueFamilyIndices.presentQueueIndex, m_queueFamilyIndices.graphicsQueueIndex, m_queueFamilyIndices.computeQueueIndex, m_queueFamilyIndices.transferQueueIndex};
    for (int queueFamilyIndex : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamilyIndex, 1, &queuePriority);
        queuesCreateInfo.push_back(queueCreateInfo);
    }

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceMeshShaderFeaturesEXT> deviceFeaturesChain = {};
    m_device.getFeatures2(&deviceFeaturesChain.get());
    ASSERT(deviceFeaturesChain.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering, "Dynamic rendering required, update driver!");
    vk::PhysicalDeviceMeshShaderFeaturesEXT &meshShaderFeatures = deviceFeaturesChain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>();
    ASSERT(meshShaderFeatures.meshShader && meshShaderFeatures.taskShader, "Mesh shader required, update driver!");
    m_supportMeshQueries = meshShaderFeatures.meshShaderQueries;
    deviceFeaturesChain.get<vk::PhysicalDeviceMeshShaderFeaturesEXT>().primitiveFragmentShadingRateMeshShader = false;

    vk::DeviceCreateInfo deviceLogicalCreateInfo = {{}, static_cast<uint32>(queuesCreateInfo.size()), queuesCreateInfo.data(), 0, nullptr, static_cast<uint32>(m_deviceExtensions.size()), m_deviceExtensions.data()};
    deviceLogicalCreateInfo.pNext = &deviceFeaturesChain.get();
    VK_CHECK(m_device.createDevice(&deviceLogicalCreateInfo, nullptr, &m_logicalDevice));
    VULKAN_HPP_DEFAULT_DISPATCHER.init(m_logicalDevice);

    m_graphicsQueue = m_logicalDevice.getQueue(m_queueFamilyIndices.graphicsQueueIndex, 0);
    m_presentQueue = m_logicalDevice.getQueue(m_queueFamilyIndices.presentQueueIndex, 0);
}

void renderer::createTransientCommandPool() {
    vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eTransient, m_queueFamilyIndices.graphicsQueueIndex);
    VK_CHECK(m_logicalDevice.createCommandPool(&poolInfo, nullptr, &m_transientCommandPool));
}

vk::Extent2D renderer::createSwapchain() {
    vk::Extent2D outWindowSize;

    const vk::PhysicalDeviceSurfaceInfo2KHR surfaceInfo2{m_surface};
    vk::SurfaceCapabilities2KHR capabilities2{};
    VK_CHECK(m_device.getSurfaceCapabilities2KHR(&surfaceInfo2, &capabilities2));

    std::vector<vk::SurfaceFormat2KHR> surfaceFormats = m_device.getSurfaceFormats2KHR(surfaceInfo2);
    std::vector<vk::PresentModeKHR> presentModes = m_device.getSurfacePresentModesKHR(m_surface);

    const vk::SurfaceFormat2KHR surfaceFormat = selectSwapSurfaceFormat(surfaceFormats);
    const vk::PresentModeKHR presentMode = selectSwapPresentMode(presentModes, m_vsync);

    outWindowSize = capabilities2.surfaceCapabilities.currentExtent;
    uint32 minImageCount = capabilities2.surfaceCapabilities.minImageCount;
    uint32 preferredImageCount = std::max(3u, minImageCount);
    // Handle the maxImageCount case where 0 means "no upper limit"
    uint32 maxImageCount = (capabilities2.surfaceCapabilities.maxImageCount == 0)
                                 ? preferredImageCount
                                 : // No upper limit, use preferred
                                 capabilities2.surfaceCapabilities.maxImageCount;

    // Clamp preferredImageCount to valid range [minImageCount, maxImageCount]
    m_maxFramesInFlight = std::clamp(preferredImageCount, minImageCount, maxImageCount);
    m_imageFormat = surfaceFormat.surfaceFormat.format;

    vk::SwapchainCreateInfoKHR ci = {{}, m_surface, m_maxFramesInFlight, surfaceFormat.surfaceFormat.format, surfaceFormat.surfaceFormat.colorSpace, outWindowSize, 1, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive, 0, nullptr, capabilities2.surfaceCapabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque, presentMode, VK_TRUE};
    uint32 queueFamilyIndices[] = {(uint32)m_queueFamilyIndices.graphicsQueueIndex, (uint32)m_queueFamilyIndices.presentQueueIndex};
    if (m_queueFamilyIndices.graphicsQueueIndex != m_queueFamilyIndices.presentQueueIndex) {
        ci.imageSharingMode = vk::SharingMode::eConcurrent;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = queueFamilyIndices;
    }

    VK_CHECK(m_logicalDevice.createSwapchainKHR(&ci, nullptr, &m_swapChain));
    std::vector<vk::Image> swapchainImages = m_logicalDevice.getSwapchainImagesKHR(m_swapChain);
    ASSERT(m_maxFramesInFlight == swapchainImages.size(), "Wrong swapchain setup");

    m_nextImages.resize(m_maxFramesInFlight);

    vk::ImageViewCreateInfo imageViewCreateInfo{{}, nullptr, vk::ImageViewType::e2D, m_imageFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    for (uint32 i = 0; i < m_maxFramesInFlight; i++) {
        m_nextImages[i].image = swapchainImages[i];
        imageViewCreateInfo.image = m_nextImages[i].image;
        VK_CHECK(m_logicalDevice.createImageView(&imageViewCreateInfo, nullptr, &m_nextImages[i].defaultView));
    }

    m_frameResources.resize(m_maxFramesInFlight);
    // initialize sync objects for each frame
    for (uint32 i = 0; i < m_maxFramesInFlight; i++) {
        const vk::SemaphoreCreateInfo semaphoreCreateInfo{};
        VK_CHECK(m_logicalDevice.createSemaphore(&semaphoreCreateInfo, nullptr,&m_frameResources[i].imageAvailableSemaphore));
        VK_CHECK(m_logicalDevice.createSemaphore(&semaphoreCreateInfo, nullptr, &m_frameResources[i].renderFinishedSemaphore));
    }

    {
        // transition every image to present layout
        vk::CommandBuffer cmdBuffer = beginSingleTimeCommands(m_logicalDevice, m_transientCommandPool);
        for (uint32 i = 0; i < m_maxFramesInFlight; i++) { cmdTransitionImageLayout(cmdBuffer, m_nextImages[i].image, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR); }
        endSingleTimeCommands(cmdBuffer, m_logicalDevice, m_transientCommandPool, m_graphicsQueue);
    }
    return outWindowSize;
}

vk::Extent2D renderer::recreateSwapChain() {
    vkQueueWaitIdle(m_graphicsQueue);
    vkQueueWaitIdle(m_presentQueue);
    m_currentFrame = 0;
    m_needRebuild = false;
    cleanupSwapChain();
    return createSwapchain();
}

void renderer::cleanupSwapChain() {
    m_logicalDevice.destroySwapchainKHR(m_swapChain);
    for (auto &frameRes : m_frameResources) {
        m_logicalDevice.destroySemaphore(frameRes.imageAvailableSemaphore);
        m_logicalDevice.destroySemaphore(frameRes.renderFinishedSemaphore);
    }
    for (auto &image : m_nextImages) { m_logicalDevice.destroyImageView(image.defaultView); }
}

void renderer::createFrameData() {
    m_frameData.resize(m_maxFramesInFlight);
    const uint64 initialValue = (m_maxFramesInFlight - 1);
    vk::SemaphoreTypeCreateInfo timelineCreateInfo(vk::SemaphoreType::eTimeline, initialValue);

    vk::SemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.pNext = &timelineCreateInfo;
    VK_CHECK(m_logicalDevice.createSemaphore(&semaphoreCreateInfo, nullptr, &m_FrameTimelineSemaphore));

    vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eTransient, m_queueFamilyIndices.graphicsQueueIndex);
    for (uint32 i = 0; i < m_maxFramesInFlight; i++) {
        m_frameData[i].frameNumber = i;
        VK_CHECK(m_logicalDevice.createCommandPool(&poolInfo, nullptr, &m_frameData[i].commandPool));
        vk::CommandBufferAllocateInfo allocInfo = {m_frameData[i].commandPool, vk::CommandBufferLevel::ePrimary, 1};
        VK_CHECK(m_logicalDevice.allocateCommandBuffers(&allocInfo, &m_frameData[i].commandBuffer));
    }
}

void renderer::createDescriptorPool() {
    const std::vector<vk::DescriptorPoolSize> poolSizes{{vk::DescriptorType::eSampler, 100}, {vk::DescriptorType::eSampledImage, 100}, {vk::DescriptorType::eUniformBuffer, 100}, {vk::DescriptorType::eStorageBuffer, 100}};
    const vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000, static_cast<uint32>(poolSizes.size()), poolSizes.data());
    VK_CHECK(m_logicalDevice.createDescriptorPool(&poolInfo, nullptr, &m_descriptorPool));
}

void renderer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    vk::PipelineRenderingCreateInfo dynRenderingAttachamentInfo = {{}, 1, &m_imageFormat};
    ImGui_ImplGlfw_InitForVulkan(m_window, true);
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = m_instance;
    initInfo.PhysicalDevice = m_device;
    initInfo.Device = m_logicalDevice;
    initInfo.QueueFamily = m_queueFamilyIndices.graphicsQueueIndex;
    initInfo.Queue = m_graphicsQueue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.Allocator = nullptr;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = m_maxFramesInFlight;
    initInfo.CheckVkResultFn = nullptr;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineRenderingCreateInfo = dynRenderingAttachamentInfo;
    ImGui_ImplVulkan_Init(&initInfo);
}

void renderer::buildRenderTargets() {
    const vk::ImageLayout layout = vk::ImageLayout::eGeneral;
    {
        const vk::Format format = vk::Format::eR8G8B8A8Unorm;
        const vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
        m_colorTexture = createTextureInternal({{}, vk::ImageType::e2D, format, {m_windowSize.width, m_windowSize.height, 1}, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, usage, vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::eUndefined});

        vk::ImageViewCreateInfo viewCreateInfo = {{}, m_colorTexture.image, vk::ImageViewType::e2D, format, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
        VK_CHECK(m_logicalDevice.createImageView(&viewCreateInfo, nullptr, &m_colorTexture.defaultView));
        m_colorTexture.sampler = m_linearSampler;
    }
    {
        const vk::Format format = findDepthFormat(m_device);
        m_depthTexture = createTextureInternal({{}, vk::ImageType::e2D, format, {m_windowSize.width, m_windowSize.height, 1}, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled, vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::eUndefined});

        vk::ImageViewCreateInfo viewCreateInfo = {{}, m_depthTexture.image, vk::ImageViewType::e2D, format, {}, {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}};
        VK_CHECK(m_logicalDevice.createImageView(&viewCreateInfo, nullptr, &m_depthTexture.defaultView));
    }

    // transition layout and clear
    vk::CommandBuffer transientCommandBuffer = beginSingleTimeCommands(m_logicalDevice, m_transientCommandPool);
    cmdTransitionImageLayout(transientCommandBuffer, m_colorTexture.image, vk::ImageLayout::eUndefined, layout);
    transientCommandBuffer.clearColorImage(m_colorTexture.image, layout, {0.f, 0.f, 0.f, 0.f}, {vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)});
    // TODO: shoud we clear depth to ?
    endSingleTimeCommands(transientCommandBuffer, m_logicalDevice, m_transientCommandPool, m_graphicsQueue);

    /* TODO: Check again
    // Descriptor Set for ImGUI
    if((ImGui::GetCurrentContext() != nullptr) && ImGui::GetIO().BackendPlatformUserData != nullptr)
    {
      for(size_t d = 0; d < m_res.descriptor.size(); ++d)
      {
        m_descriptorSet[d] = ImGui_ImplVulkan_AddTexture(m_createInfo.linearSampler, m_res.uiImageViews[d], layout);
      }
    }*/
}