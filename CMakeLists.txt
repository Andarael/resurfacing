cmake_minimum_required(VERSION 3.14)
include(FetchContent)

project(Resurfacing LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_PARALLEL_LEVEL 8)
set(MSVC_COMPILE_FLAGS "/MP")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MSVC_COMPILE_FLAGS}" )
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(LIBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

# ============================================================================
# Libraries
# ============================================================================

# Vulkan
find_package(Vulkan REQUIRED)

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw SOURCE_DIR ${LIBS_DIR}/glfw)
FetchContent_MakeAvailable(glfw)
set_target_properties(glfw uninstall update_mappings PROPERTIES FOLDER Libs/GLFW3)

# GLM
FetchContent_Declare(glm SOURCE_DIR ${LIBS_DIR}/glm)
FetchContent_MakeAvailable(glm)
set_target_properties(glm PROPERTIES FOLDER Libs)

# ImGui
FetchContent_Declare(imgui SOURCE_DIR ${LIBS_DIR}/imgui)
FetchContent_MakeAvailable(imgui)
set_target_properties(imgui PROPERTIES FOLDER Libs)

# stb image
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${LIBS_DIR}/stb)

# tinyGLTF
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_library(tinyGLTF INTERFACE)
target_include_directories(tinyGLTF INTERFACE ${LIBS_DIR}/tinygltf)

# ============================================================================
# Executable
# ============================================================================

file(GLOB_RECURSE SOURCE_FILES ${SOURCE_DIR}/*.cpp ${SOURCE_DIR}/*.h ${SOURCE_DIR}/*.hpp)
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${SOURCE_DIR} ${SHADER_DIR} ${Vulkan_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}  glfw glm imgui ${Vulkan_LIBRARIES} stb tinyGLTF)



# ============================================================================
# Shader Compilation
# ============================================================================

set(SHADER_EXTENSIONS vert tesc tese geom frag comp mesh task rgen rint rahit rchit rmiss rcall glsl)

foreach(EXT ${SHADER_EXTENSIONS})
    file(GLOB_RECURSE SHADERS "${SHADER_DIR}/*.${EXT}")
    list(APPEND SHADER_FILES ${SHADERS})
endforeach()

file(MAKE_DIRECTORY ${SHADER_DIR}/_autogen)

file(GLOB_RECURSE HEADERS "${SHADER_DIR}/*.h")
list(APPEND HEADER_FILES ${HEADERS})
set(ALL_SHADER_FILES ${SHADER_FILES} ${HEADER_FILES})

foreach(FILE ${ALL_SHADER_FILES})
    file(RELATIVE_PATH FILE_PATH ${SHADER_DIR} ${FILE})
    get_filename_component(FILE_FOLDER ${FILE_PATH} DIRECTORY)
    if (FILE_FOLDER)
        source_group("Shaders/${FILE_FOLDER}" FILES ${FILE})
    else()
        source_group("Shaders" FILES ${FILE})
    endif()
endforeach()

target_sources(${PROJECT_NAME} PRIVATE ${ALL_SHADER_FILES})

# Compile shaders to SPIR-V
foreach(SHADER ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    get_filename_component(SHADER_EXT ${SHADER} EXT)

    if (${SHADER_EXT} STREQUAL ".glsl")
        continue()  # Skip compilation for .glsl files
    endif()

    set(OUTPUT_FILE "${SHADER_DIR}/_autogen/${SHADER_NAME}.spv")
    set(_COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V --target-env vulkan1.3 --P "#extension GL_GOOGLE_include_directive : require" -l -o ${OUTPUT_FILE} ${SHADER})

    add_custom_command(
        PRE_BUILD
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${_COMMAND}
        MAIN_DEPENDENCY ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM
    )
endforeach()