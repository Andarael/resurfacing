#ifndef COMPRESSION_GLSL
#define COMPRESSION_GLSL

// from A Survey of Efficient Representations for Independent Unit Vectors (https://jcgt.org/published/0003/02/01/)
float signNotZero(in float k) {
    return k >= 0.0 ? 1.0 : -1.0;
}

vec2 signNotZero(in vec2 v) {
    return vec2( signNotZero(v.x), signNotZero(v.y) );
}

vec2 octEncode(vec3 normal) {
    return normal.xy / (1.0 + normal.z);  // Map to a 2D plane
}

vec3 octDecode(vec2 enc) {
    float z = 1.0 - dot(enc, enc);  // Reconstruct z using the length of the xy projection
    return normalize(vec3(enc.x, enc.y, z));  // Reconstruct the full 3D vector
}
#endif