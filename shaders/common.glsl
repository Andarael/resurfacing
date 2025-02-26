#ifndef COMMONS_GLSL
#define COMMONS_GLSL

vec4 lerp(vec4 a, vec4 b, float t) {
    return a * (1 - t) + b * t;
}

vec3 lerp(vec3 a, vec3 b, float t) {
    return a * (1 - t) + b * t;
}

vec4 lerp(vec2 uv, vec4 a, vec4 b, vec4 c, vec4 d) {
    vec4 ab = lerp(a, b, uv.x);
    vec4 cd = lerp(c, d, uv.x);
    return lerp(ab, cd, uv.y);
}

vec3 lerp(vec2 uv, vec3 a, vec3 b, vec3 c, vec3 d) {
    vec3 ab = lerp(a, b, uv.x);
    vec3 cd = lerp(c, d, uv.x);
    return lerp(ab, cd, uv.y);
}

float lerp(float a, float b, float t) {
    return a * (1 - t) + b * t;
}

vec3 HSVtoRGB(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 IDtoColor(int index) {
    uint state = uint(index) * 747796405U + 2891336453U;
    state = ((state >> ((state >> 28U) + 4U)) ^ state) * 277803737U;
    float tmp = float(state) / float(0xffffffffU);
    tmp = fract(sin(tmp) * 43758.5453123);

    float hue = mix(0., 1., tmp);
    float saturation = sin(tmp * 6.28318530718) * 0.1 + 0.9;
    float value = cos(tmp * 6.28318530718) * 0.1 + 0.9;

    return HSVtoRGB(vec3(hue, value, saturation));
}

vec3 IDtoColor(uint index) {
    return IDtoColor(int(index));
}

bool isVisible(vec3 position, mat4 MVP, float threshold) {
    vec4 clipSpaceCenter = MVP * vec4(position.xyz, 1);
    clipSpaceCenter.xyz /= clipSpaceCenter.w;

    float nearPlaceDistance = clipSpaceCenter.z;

    bool isInsideX = (clipSpaceCenter.x >= -threshold) && (clipSpaceCenter.x <= threshold);
    bool isInsideY = (clipSpaceCenter.y >= -threshold) && (clipSpaceCenter.y <= threshold);

    return isInsideX && isInsideY;
}

/** A filmic transfor curve to lessen the bad colors calculations done in standard sRGB space */
vec3 filmicTransform(const vec3 x) {
    vec3 col = max(vec3(0), x);
    return clamp((col * (2.51 * col + 0.03) / (col * (2.43 * col + 0.59) + 0.14)), 0.0, 1.0); // filmic curve (http://filmicworlds.com/blog/filmic-tonemapping-with-piecewise-power-curves/)
}

mat3 align_rotation_to_vector(vec3 vector, vec3 preferred_axis) {
    vec3 new_axis = normalize(vector);

    if (length(new_axis) == 0) {
        return mat3(1);
    }

    vec3 rotation_axis = cross(preferred_axis, new_axis);

    // Handle case where vectors are linearly dependent
    if (length(rotation_axis) == 0) {
        rotation_axis = cross(preferred_axis, vec3(1, 0, 0));
        if (length(rotation_axis) == 0) {
            rotation_axis = cross(preferred_axis, vec3(0, 1, 0));
        }
    }

    rotation_axis = normalize(rotation_axis);

    float cos_angle = dot(preferred_axis, new_axis);
    float angle = acos(clamp(cos_angle, -1, 1));

    float s = sin(angle);
    float c = cos(angle);
    float t = 1 - c;

    // Construct the rotation matrix
    mat3 rotation_matrix = mat3(
        t * rotation_axis.x * rotation_axis.x + c,
        t * rotation_axis.x * rotation_axis.y - s * rotation_axis.z,
        t * rotation_axis.x * rotation_axis.z + s * rotation_axis.y,

        t * rotation_axis.x * rotation_axis.y + s * rotation_axis.z,
        t * rotation_axis.y * rotation_axis.y + c,
        t * rotation_axis.y * rotation_axis.z - s * rotation_axis.x,

        t * rotation_axis.x * rotation_axis.z - s * rotation_axis.y,
        t * rotation_axis.y * rotation_axis.z + s * rotation_axis.x,
        t * rotation_axis.z * rotation_axis.z + c);

    return transpose(rotation_matrix);
}

#endif