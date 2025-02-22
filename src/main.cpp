#include "config.hpp"
#include "ObjLoader.hpp"

int main() {
    setWorkingDirectoryToProjectRoot();
    std::cout << "Working directory set to: " << std::filesystem::current_path() << std::endl;
    NgonLoader loader;
    NgonData data = loader.loadNgonData("assets/icosphere.obj");
    return EXIT_SUCCESS;
}