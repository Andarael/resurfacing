#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <chrono>

using nanosecondsD = std::chrono::duration<double, std::nano>;
using microsecondsD = std::chrono::duration<double, std::micro>;
using millisecondsD = std::chrono::duration<double, std::milli>;

using glm::uint;
using std::byte;
// Vectors
using Vec2f = glm::vec2;
using Vec3f = glm::vec3;
using Vec4f = glm::vec4;
using Vec2i = glm::ivec2;
using Vec3i = glm::ivec3;
using Vec4i = glm::ivec4;
using Vec2u = glm::uvec2;
using Vec3u = glm::uvec3;
using Vec4u = glm::uvec4;

using glm::int16;
using glm::int32;
using glm::int64;
using glm::int8;

using glm::uint16;
using glm::uint32;
using glm::uint64;
using glm::uint8;

// Matrices
using Mat2f = glm::mat2;
using Mat3f = glm::mat3;
using Mat4f = glm::mat4;

#define VEC4F_ZERO Vec4f(0.0f, 0.0f, 0.0f, 0.0f)
#define VEC3F_ZERO Vec3f(0.0f, 0.0f, 0.0f)
#define VEC4F_ONE Vec4f(1.0f, 1.0f, 1.0f, 1.0f)
#define VEC3F_ONE Vec3f(1.0f, 1.0f, 1.0f)
#define MAT4F_ID Mat4f(1.0f)