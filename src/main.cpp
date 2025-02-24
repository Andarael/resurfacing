#include "config.hpp"
#include "ObjLoader.hpp"
#include "renderer.hpp"
#include "GLFW/glfw3.h"

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
    
    return EXIT_SUCCESS;
}