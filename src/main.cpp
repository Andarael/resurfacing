#include "AppRessources.hpp"
#include "config.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "GLFW/glfw3.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void drawFrame(Renderer&);
void run();
void cleanup();

class App {
    GLFWwindow* m_window = nullptr;
    Renderer m_renderer{};
    Pipeline m_hePipeline{};
    Pipeline m_parametricPipline{};
    Pipeline m_pebblePipeline{};

    vk::DescriptorSetLayout m_uboDescriptorSetLayout;
    vk::DescriptorSet m_uboDescriptorSet;
    shaderInterface::PushConstants m_pushConstantData{};
    shaderInterface::ViewUBO m_viewUBOData{};
    shaderInterface::GlobalShadingUBO m_globalShadingUBOData{};
    UniformBuffer m_viewUBO;
    UniformBuffer m_globalShadingUBO;
    
    Dragon dragon{};
    Coat dragonCoat{};
    Ground ground{};

    Camera m_camera;
    bool m_animation = true;
    float m_currentTime = 0;
    float m_timeScale = 1.0f;
    vk::Extent2D m_swapChainExtent;

private:
    void updateSceneUBOs();
    void drawFrame();

public:
    void init();
    void drawUI();
    void handleEvent();
    void animate(float p_dt);
    void run();
    void cleanup();
};

int main() {
    setWorkingDirectoryToProjectRoot();
    std::cout << "Working directory set to: " << std::filesystem::current_path() << std::endl;
    App app;
    app.init();
    try {
        app.run();
    } catch (...) {
        app.cleanup();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void App::init() {
    ASSERT(glfwInit() == GLFW_TRUE, "Could not initialize GLFW!");
    ASSERT(glfwVulkanSupported() == GLFW_TRUE, "GLFW: Vulkan not supported!");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(800, 600, "Resurfacing", nullptr, nullptr);
    m_renderer.init(m_window, false);


    // allocate descriptor sets
    m_uboDescriptorSetLayout = shaderInterface::getDescriptorSetLayoutInfo(shaderInterface::SceneSet, m_renderer.m_logicalDevice);
    std::array<vk::DescriptorSetLayout, 1> layouts = {m_uboDescriptorSetLayout};
    vk::DescriptorSetAllocateInfo allocInfo(m_renderer.m_descriptorPool, layouts.size(), layouts.data());
    m_uboDescriptorSet = m_renderer.m_logicalDevice.allocateDescriptorSets(allocInfo)[0];
    // allocate uniform buffers
    m_viewUBO = m_renderer.createUniformBuffer(sizeof(shaderInterface::ViewUBO));
    m_globalShadingUBO = m_renderer.createUniformBuffer(sizeof(shaderInterface::GlobalShadingUBO));
    // update descriptor sets
    vk::DescriptorBufferInfo globalShadingUBO = vk::DescriptorBufferInfo(m_globalShadingUBO.buffer, 0, VK_WHOLE_SIZE);
    vk::DescriptorBufferInfo viewShadingUBO = vk::DescriptorBufferInfo(m_viewUBO.buffer, 0, VK_WHOLE_SIZE);
    std::vector<vk::WriteDescriptorSet> UBOWrites = {
        {m_uboDescriptorSet, shaderInterface::U_viewBinding, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &viewShadingUBO, nullptr},
        {m_uboDescriptorSet, shaderInterface::U_globalShadingBinding, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &globalShadingUBO, nullptr}
    };
    m_renderer.m_logicalDevice.updateDescriptorSets(UBOWrites, nullptr);
    memcpy(m_viewUBO.mappedMemory, &m_viewUBOData, sizeof(shaderInterface::ViewUBO));
    memcpy(m_globalShadingUBO.mappedMemory, &m_globalShadingUBOData, sizeof(shaderInterface::GlobalShadingUBO));
    m_camera.init(vec3(0, 3, 3), vec3(0));
    
    
    dragon.init(m_renderer, "assets/demo/dragon/dragon_8k.obj", "Dragon", "assets/demo/dragon/dragon_8k.gltf", "assets/parametric_luts/scale_lut.obj", "assets/demo/dragon/dargon_8k_ao.png", "assets/demo/dragon/dragon_element_type_map_2k.png");
    dragonCoat.init(m_renderer, "assets/demo/dragon/dragon_coat.obj", "Coat", "assets/demo/dragon/dragon_coat.gltf", "assets/demo/dragon/dragon_coat_ao.png");
    ground.init(m_renderer, "assets/demo/ground.obj", "Ground");

    m_hePipeline = m_renderer.createPipeline({"shaders/halfEdges/halfEdge.mesh","shaders/halfEdges/halfEdge.frag"}, PipelineDesc{});
    m_parametricPipline = m_renderer.createPipeline({"shaders/parametric/parametric.task","shaders/parametric/parametric.mesh","shaders/parametric/parametric.frag"}, PipelineDesc{});
    m_pebblePipeline = m_renderer.createPipeline({"shaders/pebble/pebble.task", "shaders/pebble/pebble.mesh", "shaders/pebble/pebble.frag"}, PipelineDesc{});
}

void App::drawUI() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit"))
                glfwSetWindowShouldClose(m_window, true);
            ImGui::EndMenu();
        }
        ImGui::Text("Total frame time (%.1f FPS/%.1fms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        ImGui::EndMainMenuBar();
    }

    m_camera.displayUI();

    ImGui::Begin("Controls"); // Renamed for clarity
    ImGui::SliderFloat("Time Scale", &m_timeScale, 0, 3);
    ImGui::Separator();
    m_globalShadingUBOData.displayUI();
    ImGui::End();
    
    ImGui::Begin("Meshes");
    ImGui::Separator();
    ImGui::Separator();
    ImGui::PushID("Dragon");
    dragon.displayUI();
    ImGui::PopID();
    ImGui::Separator();
    ImGui::Separator();
    ImGui::PushID("Coat");
    dragonCoat.displayUI();
    ImGui::PopID();
    ImGui::End();
}

void App::handleEvent() {
    
    m_camera.handleEvents();
}

void App::animate(float p_deltaTime) {
    m_currentTime += p_deltaTime * m_timeScale;
    m_camera.animate(p_deltaTime);
    m_globalShadingUBOData.viewPos = m_camera.getPosition();
    if (m_globalShadingUBOData.linkLight) {
        m_globalShadingUBOData.lightPos = m_camera.getPosition();
    }

    // update time
    dragon.animate(m_currentTime, m_renderer);
    dragonCoat.animate(m_currentTime, m_renderer);
    ground.animate(m_currentTime, m_renderer);
}

void App::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) == GLFW_TRUE) {
            ImGui_ImplGlfw_Sleep(10); // Do nothing when minimized
            continue;
        }
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        handleEvent();
        float dt = ImGui::GetIO().DeltaTime;
        if (m_animation) {
            animate(dt);
        }
        drawUI();


        const vk::Extent2D extent = m_renderer.getSwapChainExtent();
        if (extent.width != m_swapChainExtent.width || extent.height != m_swapChainExtent.height) {
            m_camera.resize(extent.width, extent.height);
            m_swapChainExtent = extent;
        }
        drawFrame();
        ImGui::EndFrame();
    }
    cleanup();
}

void App::updateSceneUBOs() {
    mat4 projection = m_camera.getProjectionMatrix();
    projection[1][1] *= -1; // flip y coordinate
    m_viewUBOData.view = m_camera.getViewMatrix();
    m_viewUBOData.projection = projection;
    m_viewUBOData.cameraPosition = vec4(m_camera.getPosition(), 1);
    m_viewUBOData.near = m_camera.getZNear();
    m_viewUBOData.far = m_camera.getZFar();
    
    memcpy(m_viewUBO.mappedMemory, &m_viewUBOData, sizeof(shaderInterface::ViewUBO));
    memcpy(m_globalShadingUBO.mappedMemory, &m_globalShadingUBOData, sizeof(shaderInterface::GlobalShadingUBO));
    dragon.updateUBOs();
    dragonCoat.updateUBOs();
    ground.updateUBOs();
}

void App::drawFrame() {
    updateSceneUBOs();
    
    vk::CommandBuffer cmd = m_renderer.beginFrame();
    m_renderer.beginRendering(cmd, true);
    vk::Extent2D extent = m_renderer.getSwapChainExtent();
    // dragon
    cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, extent.width, extent.height, 0.0f, 1.0f));
    cmd.setScissor(0, vk::Rect2D({0, 0}, extent));
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_parametricPipline.pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_parametricPipline.layout, 0, 1, &m_uboDescriptorSet, 0, nullptr);
    dragon.bindAndDispatch(cmd, m_parametricPipline.layout);
    dragonCoat.bindAndDispatch(cmd, m_parametricPipline.layout);
    
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_hePipeline.pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_hePipeline.layout, 0, 1, &m_uboDescriptorSet, 0, nullptr);
    dragon.bindAndDispatchBaseMesh(cmd, m_hePipeline.layout);

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pebblePipeline.pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pebblePipeline.layout, 0, 1, &m_uboDescriptorSet, 0, nullptr);
    ground.bindAndDispatch(cmd, m_pebblePipeline.layout);
    
    m_renderer.endRendering(cmd);
    
    // UI pass
    ImGui::Render(); // finalize ImGui draw data
    m_renderer.renderUI(cmd);
    m_renderer.endFrame(cmd);
}

void App::cleanup() {
    m_renderer.cleanup();
    glfwDestroyWindow(m_window);
    glfwTerminate();
}