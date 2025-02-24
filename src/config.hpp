#pragma once
#include <filesystem>
#include <iostream>

#define ROOT_IDENTIFIER ".root"
constexpr bool enableValidationLayers = true;

inline void setWorkingDirectoryToProjectRoot() {
    std::filesystem::path exePath = std::filesystem::current_path();
    unsigned int maxDepth = 10;
    while (exePath.has_parent_path() && !std::filesystem::exists(exePath / ROOT_IDENTIFIER) && maxDepth > 0) {
        exePath = exePath.parent_path();
        maxDepth--;
    }

    if (!std::filesystem::exists(exePath / ROOT_IDENTIFIER)) {
        std::cerr << "Project root not found."
                  << "Are you running the executable from the project folder?"
                  << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::filesystem::current_path(exePath);
}