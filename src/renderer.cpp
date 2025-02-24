#include "renderer.hpp"

#include <set>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <vulkan/vulkan.hpp>

#include "config.hpp"
#include "GLFW/glfw3.h"


renderer::renderer(GLFWwindow *window, bool vSync) {
    m_window = window;
    init();
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
    uint32_t i = 0;
    while (!m_queueFamilyIndices.isComplete() && i < queueFamilies.size()) { // very simple policy of selecting the first queue that supports the required operations, not ideal but works for now
        if (m_device.getSurfaceSupportKHR(i, m_surface)) { m_queueFamilyIndices.presentQueueIndex = i; }
        if ((queueFamilies[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics) && (queueFamilies[i].queueFamilyProperties.timestampValidBits !=0)) { m_queueFamilyIndices.graphicsQueueIndex = i; } // ensure the queue supports timestamp queries
        if ((queueFamilies[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eCompute) && (queueFamilies[i].queueFamilyProperties.timestampValidBits !=0)) { m_queueFamilyIndices.computeQueueIndex = i; } // ensure the queue supports timestamp queries
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

void renderer::init() {
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
    createInstance();
    createSurface();
    selectPhysicalDevice();
    createDeviceAndQueues();
}