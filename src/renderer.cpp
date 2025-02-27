#include "renderer.hpp"

#include "shaderInterface.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <set>

#include "config.hpp"

uint32 Renderer::findMemoryType(uint32 p_typeFilter, vk::MemoryPropertyFlags p_properties) const {
    vk::PhysicalDeviceMemoryProperties memProperties;
    m_device.getMemoryProperties(&memProperties);
    for (uint32 i = 0; i < memProperties.memoryTypeCount; i++) { if ((p_typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & p_properties) == p_properties) { return i; } }
    throw std::runtime_error("Failed to find suitable memory type.");
}

Buffer Renderer::createStagingBuffer(uint32 p_size) {
    vk::BufferCreateInfo bufferCreateInfo({}, p_size, vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive);
    Buffer staging = createBufferInternal(bufferCreateInfo, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    return staging;
}

void Renderer::freeTrackedStagingBuffers() {
    for (Buffer &buffer : m_usedStagingBuffers) {
        m_logicalDevice.destroyBuffer(buffer.buffer);
        m_logicalDevice.freeMemory(buffer.memory);
    }
}

UniformBuffer Renderer::createUniformBuffer(uint32 p_size) {
    vk::BufferCreateInfo bufferCreateInfo({}, p_size, vk::BufferUsageFlagBits::eUniformBuffer, vk::SharingMode::eExclusive);
    UniformBuffer res = UniformBuffer(createBufferInternal(bufferCreateInfo, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
    res.mappedMemory = m_logicalDevice.mapMemory(res.memory, 0, p_size);
    return res;
}

Buffer Renderer::createBufferInternal(const vk::BufferCreateInfo &p_createInfo, const vk::MemoryPropertyFlags p_memProperties) {
    Buffer res{};
    res.size = p_createInfo.size;
    VK_CHECK(m_logicalDevice.createBuffer(&p_createInfo, nullptr, &res.buffer));
    vk::StructureChain<vk::MemoryRequirements2> memRequirements = m_logicalDevice.getBufferMemoryRequirements2(vk::BufferMemoryRequirementsInfo2(res.buffer));
    vk::MemoryAllocateInfo allocInfo(memRequirements.get<vk::MemoryRequirements2>().memoryRequirements.size, findMemoryType(memRequirements.get<vk::MemoryRequirements2>().memoryRequirements.memoryTypeBits, p_memProperties));
    VK_CHECK(m_logicalDevice.allocateMemory(&allocInfo, nullptr, &res.memory));
    m_logicalDevice.bindBufferMemory(res.buffer, res.memory, 0);
    return res;
}

Texture Renderer::createTextureInternal(const vk::ImageCreateInfo &p_createInfo) {
    Texture res{{}, {}, p_createInfo.format, p_createInfo.initialLayout,uvec3(p_createInfo.extent.width, p_createInfo.extent.height, p_createInfo.extent.depth)};
    VK_CHECK(m_logicalDevice.createImage(&p_createInfo, nullptr, &res.image));
    vk::StructureChain<vk::MemoryRequirements2> memRequirements = m_logicalDevice.getImageMemoryRequirements2(vk::ImageMemoryRequirementsInfo2(res.image));
    vk::MemoryAllocateInfo allocInfo(memRequirements.get<vk::MemoryRequirements2>().memoryRequirements.size, findMemoryType(memRequirements.get<vk::MemoryRequirements2>().memoryRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));
    VK_CHECK(m_logicalDevice.allocateMemory(&allocInfo, nullptr, &res.memory));
    m_logicalDevice.bindImageMemory(res.image, res.memory, 0);
    return res;
}

void Renderer::copyToStagingMem(Buffer &p_staging, const void *p_data, uint32 p_size) {
    void *mappedData = m_logicalDevice.mapMemory(p_staging.memory, 0, p_size);
    memcpy(mappedData, p_data, p_size);
    m_logicalDevice.unmapMemory(p_staging.memory);
}

void Renderer::uploadDataToBufferInternal(Buffer &p_dstBuffer, vk::CommandBuffer p_commandBuffer,const void *p_data, uint32 p_size, uint32 p_offset) {
    Buffer stagingBuffer = createStagingBuffer(p_size);
    m_usedStagingBuffers.push_back(stagingBuffer);
    copyToStagingMem(stagingBuffer, p_data, p_size);
    vk::BufferCopy copyRegion(0, p_offset, p_size);
    p_commandBuffer.copyBuffer(stagingBuffer.buffer, p_dstBuffer.buffer, copyRegion);

}

void Renderer::uploadDataToBufferInternal(Buffer &p_stagingBuffer, Buffer &p_dstBuffer, vk::CommandBuffer p_commandBuffer,const void *p_data, uint32 p_size, uint32 p_offset) {
    copyToStagingMem(p_stagingBuffer, p_data, p_size);
    vk::BufferCopy copyRegion(0, p_offset, p_size);
    p_commandBuffer.copyBuffer(p_stagingBuffer.buffer, p_dstBuffer.buffer, copyRegion);
}

void Renderer::uploadDataToImageInternal(Texture &p_dstImage, vk::CommandBuffer p_commandBuffer,const void *p_data, uint32 p_size) {
    Buffer stagingBuffer = createStagingBuffer(p_size);
    m_usedStagingBuffers.push_back(stagingBuffer);
    copyToStagingMem(stagingBuffer, p_data, p_size);
    vk::BufferImageCopy2 copyRegion(0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0, 0, 0}, {p_dstImage.dimensions.x, p_dstImage.dimensions.y, p_dstImage.dimensions.z});
    cmdTransitionImageLayout(p_commandBuffer, p_dstImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    p_commandBuffer.copyBufferToImage2({stagingBuffer.buffer, p_dstImage.image, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion});
    cmdTransitionImageLayout(p_commandBuffer, p_dstImage, vk::ImageLayout::eTransferDstOptimal, p_dstImage.currentLayout == vk::ImageLayout::eUndefined ? vk::ImageLayout::eGeneral : p_dstImage.currentLayout);
    p_dstImage.currentLayout = p_dstImage.currentLayout == vk::ImageLayout::eUndefined ? vk::ImageLayout::eGeneral : p_dstImage.currentLayout;
}
void Renderer::uploadDataToImageInternal(Buffer &p_stagingBuffer, Texture &p_dstImage, vk::CommandBuffer p_commandBuffer,const void *p_data, uint32 p_size) {
    copyToStagingMem(p_stagingBuffer, p_data, p_size);
    vk::BufferImageCopy2 copyRegion(0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0, 0, 0}, {p_dstImage.dimensions.x, p_dstImage.dimensions.y, p_dstImage.dimensions.z});
    cmdTransitionImageLayout(p_commandBuffer, p_dstImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    p_commandBuffer.copyBufferToImage2({p_stagingBuffer.buffer, p_dstImage.image, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion});
    cmdTransitionImageLayout(p_commandBuffer, p_dstImage, vk::ImageLayout::eTransferDstOptimal, p_dstImage.currentLayout == vk::ImageLayout::eUndefined ? vk::ImageLayout::eGeneral : p_dstImage.currentLayout);
    p_dstImage.currentLayout = p_dstImage.currentLayout == vk::ImageLayout::eUndefined ? vk::ImageLayout::eGeneral : p_dstImage.currentLayout;
}


void Renderer::init(GLFWwindow *window, bool vSync) {
    m_window = window;
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
}

void Renderer::cleanup() {
    m_logicalDevice.waitIdle();
    cleanupSwapChain();
    m_logicalDevice.destroySampler(m_linearSampler);
    m_logicalDevice.destroySampler(m_nearestSampler);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_logicalDevice.destroyCommandPool(m_transientCommandPool);
    m_instance.destroySurfaceKHR(m_surface);
    m_logicalDevice.destroyDescriptorPool(m_descriptorPool);

    for (auto &frame : m_frameData) {
        m_logicalDevice.freeCommandBuffers(frame.commandPool, 1, &frame.commandBuffer);
        m_logicalDevice.destroyCommandPool(frame.commandPool);
    }
    m_logicalDevice.destroySemaphore(m_FrameTimelineSemaphore);

    m_logicalDevice.waitIdle();
    if (enableValidationLayers) { m_instance.destroyDebugUtilsMessengerEXT(m_callback); }
    m_logicalDevice.destroy();
    m_instance.destroy();
}

void Renderer::createInstance() {
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

void Renderer::createSurface() {
    // TODO: integrate error handler with GLFW
    VK_CHECK(vk::Result(glfwCreateWindowSurface(m_instance, m_window, nullptr, reinterpret_cast<VkSurfaceKHR *>(&m_surface))));
}

void Renderer::selectPhysicalDevice() {
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

void Renderer::populateQueueFamilyIndices() {
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

void Renderer::createDeviceAndQueues() {
    std::vector<vk::DeviceQueueCreateInfo> queuesCreateInfo;
    float queuePriority = 1.0f;
    std::set<uint32> uniqueQueueFamilies = {m_queueFamilyIndices.presentQueueIndex, m_queueFamilyIndices.graphicsQueueIndex, m_queueFamilyIndices.computeQueueIndex, m_queueFamilyIndices.transferQueueIndex};
    for (int queueFamilyIndex : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamilyIndex, 1, &queuePriority);
        queuesCreateInfo.push_back(queueCreateInfo);
    }

    vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceMeshShaderFeaturesEXT> deviceFeaturesChain = {};
    m_device.getFeatures2(&deviceFeaturesChain.get());
    ASSERT(deviceFeaturesChain.get<vk::PhysicalDeviceVulkan12Features>().scalarBlockLayout, "Scalar block layout required, update driver!");
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

void Renderer::createTransientCommandPool() {
    vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eTransient, m_queueFamilyIndices.graphicsQueueIndex);
    VK_CHECK(m_logicalDevice.createCommandPool(&poolInfo, nullptr, &m_transientCommandPool));
}

vk::Extent2D Renderer::createSwapchain() {
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
    vk::Format depthFormat = findDepthFormat(m_device);
    
    m_nextImages.resize(m_maxFramesInFlight);
    m_depthImages.resize(m_maxFramesInFlight);
    vk::ImageViewCreateInfo imageViewCreateInfo = {{}, nullptr, vk::ImageViewType::e2D, surfaceFormat.surfaceFormat.format, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    vk::ImageCreateInfo depthImageCreateIngo = {{}, vk::ImageType::e2D, depthFormat, {outWindowSize.width, outWindowSize.height, 1}, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::eUndefined};
    vk::ImageViewCreateInfo depthImageViewCreateInfo = {{}, nullptr, vk::ImageViewType::e2D, depthFormat, {}, {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}};
    for (uint32 i = 0; i < m_maxFramesInFlight; i++) {
        m_nextImages[i].image = swapchainImages[i];
        imageViewCreateInfo.image = m_nextImages[i].image;
        VK_CHECK(m_logicalDevice.createImageView(&imageViewCreateInfo, nullptr, &m_nextImages[i].defaultView));
        m_nextImages[i].dimensions = {outWindowSize.width, outWindowSize.height, 1};
        m_nextImages[i].format = surfaceFormat.surfaceFormat.format;

        m_depthImages[i] = createTextureInternal(depthImageCreateIngo);
        depthImageViewCreateInfo.image = m_depthImages[i].image;
        VK_CHECK(m_logicalDevice.createImageView(&depthImageViewCreateInfo, nullptr, &m_depthImages[i].defaultView));
        
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
        for (uint32 i = 0; i < m_maxFramesInFlight; i++) { cmdTransitionImageLayout(cmdBuffer, m_nextImages[i], vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR); }
        endSingleTimeCommands(cmdBuffer, m_logicalDevice, m_transientCommandPool, m_graphicsQueue);
    }
    return outWindowSize;
}

vk::Extent2D Renderer::recreateSwapChain() {
    vkQueueWaitIdle(m_graphicsQueue);
    vkQueueWaitIdle(m_presentQueue);
    m_currentFrame = 0;
    m_needRebuild = false;
    cleanupSwapChain();
    return createSwapchain();
}

void Renderer::cleanupSwapChain() {
    m_logicalDevice.destroySwapchainKHR(m_swapChain);
    for (auto &frameRes : m_frameResources) {
        m_logicalDevice.destroySemaphore(frameRes.imageAvailableSemaphore);
        m_logicalDevice.destroySemaphore(frameRes.renderFinishedSemaphore);
    }
    for (auto &image : m_nextImages) { m_logicalDevice.destroyImageView(image.defaultView); }
}

void Renderer::createFrameData() {
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

void Renderer::createDescriptorPool() {
    const std::vector<vk::DescriptorPoolSize> poolSizes{{vk::DescriptorType::eSampler, 100}, {vk::DescriptorType::eSampledImage, 100}, {vk::DescriptorType::eUniformBuffer, 100}, {vk::DescriptorType::eStorageBuffer, 100}};
    const vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1000, static_cast<uint32>(poolSizes.size()), poolSizes.data());
    VK_CHECK(m_logicalDevice.createDescriptorPool(&poolInfo, nullptr, &m_descriptorPool));
}

void Renderer::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    vk::PipelineRenderingCreateInfo dynRenderingAttachamentInfo = {{}, 1, &m_nextImages[0].format};
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

Pipeline Renderer::createPipeline(const std::vector<std::string> &p_shaderPaths, const PipelineDesc &p_pipelineDesc) {
    static const std::string autogenPath = "shaders/_autogen/";
    Pipeline pipeline;
    std::vector<vk::ShaderModule> shaderModules(p_shaderPaths.size());
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages(p_shaderPaths.size());
    uint32 i = 0;
    for (const auto &shaderPath : p_shaderPaths) {
        const vk::ShaderStageFlagBits stage = inferShaderStageFromExt(shaderPath);
        std::string shaderName = shaderPath.substr(shaderPath.find_last_of("/\\") + 1);
        const std::vector<byte> code = readFile(autogenPath + shaderName + ".spv");
        vk::ShaderModuleCreateInfo createInfo({}, code.size(), reinterpret_cast<const uint32 *>(code.data()));
        VK_CHECK(m_logicalDevice.createShaderModule(&createInfo, nullptr, &shaderModules[i]));
        shaderStages[i] = {{}, stage, shaderModules[i], "main"};
        i++;
    }
    const vk::PushConstantRange pushConstantRange = {trueAllGraphics, 0, sizeof(shaderInterface::PushConstants)};
    const std::vector<vk::DescriptorSetLayout> descriptorSetLayouts = {shaderInterface::getDescriptorSetLayoutInfo(shaderInterface::SceneSet, m_logicalDevice), shaderInterface::getDescriptorSetLayoutInfo(shaderInterface::HESet, m_logicalDevice), shaderInterface::getDescriptorSetLayoutInfo(shaderInterface::PerObjectSet, m_logicalDevice)};
    const vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, descriptorSetLayouts.size(), descriptorSetLayouts.data(), 1, &pushConstantRange);
    VK_CHECK(m_logicalDevice.createPipelineLayout(&pipelineLayoutInfo, nullptr, &pipeline.layout));

    const std::array<vk::Format, 1> imageFormats = {{ m_nextImages[0].format}}; // same as render target
    const vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo = {{}, static_cast<uint32>(imageFormats.size()), imageFormats.data(), m_depthImages[0].format};
    const vk::PipelineDynamicStateCreateInfo dynamicStateCreateInfo = {{}, static_cast<uint32>(p_pipelineDesc.dynamicStates.size()), p_pipelineDesc.dynamicStates.data()};
    const vk::PipelineViewportStateCreateInfo viewportStateCreateInfo = {{}, 1, nullptr, 1, nullptr};
    vk::GraphicsPipelineCreateInfo pipelineCreateInfo({}, shaderStages.size(), shaderStages.data(), &p_pipelineDesc.vertexInputStateCreateInfo, &p_pipelineDesc.inputAssemblyStateCreateInfo, nullptr, &viewportStateCreateInfo, &p_pipelineDesc.rasterizationStateCreateInfo, &p_pipelineDesc.multisampleStateCreateInfo, &p_pipelineDesc.depthStencilStateCreateInfo, &p_pipelineDesc.colorBlendStateCreateInfo, &dynamicStateCreateInfo, pipeline.layout);
    pipelineCreateInfo.setPNext(&pipelineRenderingCreateInfo);
    VK_CHECK(m_logicalDevice.createGraphicsPipelines(nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline.pipeline));

    for (auto &shaderModule : shaderModules) { m_logicalDevice.destroyShaderModule(shaderModule); }
    for (auto &descriptorSetLayout : descriptorSetLayouts) { m_logicalDevice.destroyDescriptorSetLayout(descriptorSetLayout); }
    return pipeline;
}

vk::CommandBuffer Renderer::beginFrame() {
    if (m_needRebuild) { m_windowSize = recreateSwapChain(); }

    FrameData &frameData = m_frameData[m_currentFrame];
    vk::SemaphoreWaitInfo waitInfo = {{}, 1, &m_FrameTimelineSemaphore, &frameData.frameNumber};
    VK_CHECK(m_logicalDevice.waitSemaphores(&waitInfo, std::numeric_limits<uint64>::max()));
    m_logicalDevice.resetCommandPool(frameData.commandPool, {});

    vk::CommandBuffer cmd = frameData.commandBuffer;
    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit}); // for now we rerecord every time
    // aquiring the next image
    ASSERT(m_needRebuild == false, "Swapbuffer need to call recreateSwapChain()");
    FrameResources &frameResources = m_frameResources[m_currentFrame];
    vk::Result result = m_logicalDevice.acquireNextImageKHR(m_swapChain, std::numeric_limits<uint64>::max(), frameResources.imageAvailableSemaphore, nullptr, &m_nextImageIndex);
    if (result == vk::Result::eErrorOutOfDateKHR) { m_needRebuild = true; } else { ASSERT(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR, "Failed to aquire next SwapChain image"); }
    return cmd;
}

void Renderer::beginRendering(vk::CommandBuffer p_cmd, bool p_clear) {
    const std::array<vk::RenderingAttachmentInfo, 1> renderingAttachments = {{{m_nextImages[m_nextImageIndex].defaultView, vk::ImageLayout::eColorAttachmentOptimal, {}, nullptr, {}, p_clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, {vk::ClearColorValue(1, 1, 1, 1)}}}};
    const vk::RenderingAttachmentInfo depthAttachment = {m_depthImages[m_nextImageIndex].defaultView, vk::ImageLayout::eDepthStencilAttachmentOptimal, {}, nullptr, {}, p_clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore, {vk::ClearDepthStencilValue(1.0f, 0)}};
    const vk::RenderingInfo renderingInfo = {{}, {{0, 0}, m_windowSize}, 1, {}, renderingAttachments.size(), renderingAttachments.data(),&depthAttachment};
    cmdTransitionImageLayout(p_cmd, m_nextImages[m_nextImageIndex], vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eColorAttachmentOptimal);
    cmdTransitionImageLayout(p_cmd, m_depthImages[m_nextImageIndex], vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageAspectFlagBits::eDepth);
    p_cmd.beginRendering(renderingInfo);
}

void Renderer::endRendering(vk::CommandBuffer p_cmd) {
    p_cmd.endRendering();
    cmdTransitionImageLayout(p_cmd, m_nextImages[m_nextImageIndex],vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
}

void Renderer::renderUI(vk::CommandBuffer p_cmd, bool p_clear) {
    const std::array<vk::RenderingAttachmentInfo, 1> renderingAttachments = {{{m_nextImages[m_nextImageIndex].defaultView, vk::ImageLayout::eColorAttachmentOptimal, {}, nullptr, {}, p_clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore,{vk::ClearColorValue(1, 1, 1, 1)}}}};
    const vk::RenderingInfo renderingInfo = {{}, {{0, 0}, m_windowSize}, 1, {}, renderingAttachments.size(), renderingAttachments.data()};
    cmdTransitionImageLayout(p_cmd, m_nextImages[m_nextImageIndex],vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eColorAttachmentOptimal);
    p_cmd.beginRendering(renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), p_cmd);
    p_cmd.endRendering();
    cmdTransitionImageLayout(p_cmd, m_nextImages[m_nextImageIndex],vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);
}

void Renderer::endFrame(vk::CommandBuffer p_cmd) {
    p_cmd.end();
    // prepare submit
    std::vector<vk::SemaphoreSubmitInfo> waitSemaphoreSubmitInfos;
    std::vector<vk::SemaphoreSubmitInfo> signalSemaphoreSubmitInfos;
    waitSemaphoreSubmitInfos.push_back({m_frameResources[m_currentFrame].imageAvailableSemaphore, 0, vk::PipelineStageFlagBits2::eColorAttachmentOutput});
    signalSemaphoreSubmitInfos.push_back({m_frameResources[m_currentFrame].renderFinishedSemaphore, 0, vk::PipelineStageFlagBits2::eColorAttachmentOutput});

    FrameData &frameData = m_frameData[m_frameRingCurrent];
    const uint64 signalValue = frameData.frameNumber + m_maxFramesInFlight;
    frameData.frameNumber = signalValue;

    signalSemaphoreSubmitInfos.push_back({m_FrameTimelineSemaphore, signalValue, vk::PipelineStageFlagBits2::eColorAttachmentOutput});

    const std::array<vk::CommandBufferSubmitInfo, 1> cmdSubmitInfo = {{{p_cmd}}};
    const std::array<vk::SubmitInfo2, 1> submitInfo = {vk::SubmitInfo2({}, waitSemaphoreSubmitInfos.size(), waitSemaphoreSubmitInfos.data(), cmdSubmitInfo.size(), cmdSubmitInfo.data(), signalSemaphoreSubmitInfos.size(), signalSemaphoreSubmitInfos.data())};

    m_graphicsQueue.submit2(submitInfo, nullptr);
    presentFrame();

    m_frameRingCurrent = (m_frameRingCurrent + 1) % m_maxFramesInFlight;
}

void Renderer::presentFrame() {
    FrameResources &frameResources = m_frameResources[m_currentFrame];
    const vk::PresentInfoKHR presentInfo(1, &frameResources.renderFinishedSemaphore, 1, &m_swapChain, &m_nextImageIndex);
    const vk::Result result = m_presentQueue.presentKHR(&presentInfo);
    if (result == vk::Result::eErrorOutOfDateKHR) { m_needRebuild = true; } else { ASSERT(result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR, "Couldn't present swapchain image"); }

    // Advance to the next frame in the swapchain
    m_currentFrame = (m_currentFrame + 1) % m_maxFramesInFlight;
}