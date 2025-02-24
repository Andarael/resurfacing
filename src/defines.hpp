#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <chrono>

using nanosecondsD = std::chrono::duration<double, std::nano>;
using microsecondsD = std::chrono::duration<double, std::micro>;
using millisecondsD = std::chrono::duration<double, std::milli>;

using std::byte;

// Vectors
using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;
using glm::uvec2;
using glm::uvec3;
using glm::uvec4;

using glm::int16;
using glm::int32;
using glm::int64;
using glm::int8;

using glm::uint16;
using glm::uint32;
using glm::uint64;
using glm::uint8;

// Matrices
using glm::mat2;
using glm::mat3;
using glm::mat4;

#define VEC4F_ZERO vec4(0.0f, 0.0f, 0.0f, 0.0f)
#define VEC3F_ZERO vec3(0.0f, 0.0f, 0.0f)
#define VEC4F_ONE vec4(1.0f, 1.0f, 1.0f, 1.0f)
#define VEC3F_ONE vec3(1.0f, 1.0f, 1.0f)
#define MAT4F_ID mat4(1.0f)