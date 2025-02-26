#include "config.hpp"
#include "ObjLoader.hpp"
#include "renderer.hpp"
#include "GLFW/glfw3.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

void drawFrame(renderer&);

int main() {
    setWorkingDirectoryToProjectRoot();
    std::cout << "Working directory set to: " << std::filesystem::current_path() << std::endl;
    NgonLoader loader;
    NgonData data = loader.loadNgonData("assets/icosphere.obj");

    ASSERT(glfwInit() == GLFW_TRUE, "Could not initialize GLFW!");
    ASSERT(glfwVulkanSupported() == GLFW_TRUE, "GLFW: Vulkan not supported!");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Resurfacing", nullptr, nullptr);
    renderer renderer(window, true);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE) {
            ImGui_ImplGlfw_Sleep(10); // Do nothing when minimized
            continue;
        }
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit"))
                    glfwSetWindowShouldClose(window, true);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        // ImGui::ShowDemoWindow();
        drawFrame(renderer);
        ImGui::EndFrame();
    }

    return EXIT_SUCCESS;
}

void drawFrame(renderer &renderer) {
    vk::CommandBuffer cmd = renderer.beginRendering();
    renderer.renderUI(cmd);
    renderer.endRendering(cmd);
}