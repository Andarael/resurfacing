
#ifndef NOISE_GLSL
#define NOISE_GLSL

#include "common.glsl"

// ==================== RANDOM ====================

// https://www.shadertoy.com/view/Ns2XDt
// https://www.pcg-random.org/download.html
uint seed = 0;
uint pcg() {
    uint state = seed * 747796405U + 2891336453U;
    uint tmp = ((state >> ((state >> 28U) + 4U)) ^ state) * 277803737U;
    return (seed = (tmp >> 22U) ^ tmp);
}

// in [0, 1]
float rand() { return float(pcg()) / float(0xffffffffU); }

float rand(float min, float max) { return mix(min, max, rand()); }

vec2 rand2(float min, float max) { return vec2(rand(min, max), rand(min, max)); }

vec2 rand2() { return vec2(rand(), rand()); }

vec3 rand3(float min, float max) { return vec3(rand(min, max), rand(min, max), rand(min, max)); }

vec4 rand4(float min, float max) { return vec4(rand(min, max), rand(min, max), rand(min, max), rand(min, max)); }

vec4 rand4() { return rand4(0, 1); }

// ==================== VALUE NOISE ====================

vec3 quintic(vec3 p) {
    return p * p * p * (10 + p * (-15 + p * 6));
}

float whiteNoise3x1(vec3 p) {
    float random = dot(p, vec3(12.9898, 78.233, 37.719));
    random = sin(random);
    random = random * 43758.5453;
    random = fract(random);
    return random;
}

float valueNoise(vec3 pos) {
    vec3 gridPos = fract(pos);
    vec3 gridId = floor(pos);

    gridPos = quintic(gridPos);

    float bottomBackLeft = whiteNoise3x1(gridId);
    float bottomBackRight = whiteNoise3x1(gridId + vec3(1.0, 0.0, 0.0));
    float bottomFrontLeft = whiteNoise3x1(gridId + vec3(0.0, 1.0, 0.0));
    float bottomFrontRight = whiteNoise3x1(gridId + vec3(1.0, 1.0, 0.0));

    float topBackLeft = whiteNoise3x1(gridId + vec3(0.0, 0.0, 1.0));
    float topBackRight = whiteNoise3x1(gridId + vec3(1.0, 0.0, 1.0));
    float topFrontLeft = whiteNoise3x1(gridId + vec3(0.0, 1.0, 1.0));
    float topFrontRight = whiteNoise3x1(gridId + vec3(1.0, 1.0, 1.0));

    float bottomBack = lerp(bottomBackLeft, bottomBackRight, gridPos.x);
    float bottomFront = lerp(bottomFrontLeft, bottomFrontRight, gridPos.x);

    float topBack = lerp(topBackLeft, topBackRight, gridPos.x);
    float topFront = lerp(topFrontLeft, topFrontRight, gridPos.x);

    float bottom = lerp(bottomBack, bottomFront, gridPos.y);
    float top = lerp(topBack, topFront, gridPos.y);

    float noise = lerp(bottom, top, gridPos.z);

    return noise;
}

float valueNoiseOctaves(vec3 pos, uint octaves) {
    if (octaves < 1) octaves = 1;

    float noise = 0.0;
    float scale = 1.0;
    float weight = 1.0;
    float totalAmplitude = 0.0;

    for (uint i = 0; i < octaves; i++) {
        noise += valueNoise(pos * scale) * weight;
        scale *= 2.0;
        weight *= 0.5;
        totalAmplitude += weight;
    }

    return noise / totalAmplitude / 2;
}

float normalizedNoise(vec3 pos, float scale, float offset, uint octaves) {
    // return 0.5;
    return mix(offset, 1, valueNoiseOctaves(pos.xyz * scale, octaves));
}

vec3 valueNoise3D(vec3 pos) {
    return vec3(valueNoise(pos + vec3(0.0, 0.0, 0.0)), valueNoise(pos + vec3(1.0, 0.0, 0.0)), valueNoise(pos + vec3(0.0, 1.0, 0.0)));
}

// **************************************************************************/

// ==================== PERLIN NOISE 3D ====================

// Helper functions
vec4 permute(vec4 x) { return mod(((x * 34.0) + 1.0) * x, 289.0); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }
vec3 fade(vec3 t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
vec3 dFade(vec3 t) { return 30.0 * t * t * (t * (t - 2.0) + 1.0); } // Derivative of fade

// Improved Perlin noise with derivative (gradient)
struct PerlinNoise3D {
    float value;
    vec3 gradient;
};

PerlinNoise3D perlinNoise3D(vec3 P) {
    vec3 Pi0 = floor(P);        // Integer part for indexing
    vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
    Pi0 = mod(Pi0, 289.0);
    Pi1 = mod(Pi1, 289.0);
    vec3 Pf0 = fract(P); // Fractional part for interpolation
    vec3 Pf1 = Pf0 - vec3(1.0);

    vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    vec4 iy = vec4(Pi0.y, Pi0.y, Pi1.y, Pi1.y);
    vec4 iz0 = vec4(Pi0.z);
    vec4 iz1 = vec4(Pi1.z);

    vec4 ixy = permute(permute(ix) + iy);
    vec4 ixy0 = permute(ixy + iz0);
    vec4 ixy1 = permute(ixy + iz1);

    vec4 gx0 = ixy0 * (1.0 / 7.0);
    vec4 gy0 = fract(floor(gx0) * (1.0 / 7.0)) - 0.5;
    gx0 = fract(gx0);
    vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
    vec4 sz0 = step(gz0, vec4(0.0));
    gx0 -= sz0 * (step(0.0, gx0) - 0.5);
    gy0 -= sz0 * (step(0.0, gy0) - 0.5);

    vec4 gx1 = ixy1 * (1.0 / 7.0);
    vec4 gy1 = fract(floor(gx1) * (1.0 / 7.0)) - 0.5;
    gx1 = fract(gx1);
    vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
    vec4 sz1 = step(gz1, vec4(0.0));
    gx1 -= sz1 * (step(0.0, gx1) - 0.5);
    gy1 -= sz1 * (step(0.0, gy1) - 0.5);

    vec3 g000 = vec3(gx0.x, gy0.x, gz0.x);
    vec3 g100 = vec3(gx0.y, gy0.y, gz0.y);
    vec3 g010 = vec3(gx0.z, gy0.z, gz0.z);
    vec3 g110 = vec3(gx0.w, gy0.w, gz0.w);
    vec3 g001 = vec3(gx1.x, gy1.x, gz1.x);
    vec3 g101 = vec3(gx1.y, gy1.y, gz1.y);
    vec3 g011 = vec3(gx1.z, gy1.z, gz1.z);
    vec3 g111 = vec3(gx1.w, gy1.w, gz1.w);

    vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
    g000 *= norm0.x;
    g010 *= norm0.y;
    g100 *= norm0.z;
    g110 *= norm0.w;

    vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
    g001 *= norm1.x;
    g011 *= norm1.y;
    g101 *= norm1.z;
    g111 *= norm1.w;

    vec4 n000 = vec4(dot(g000, Pf0));
    vec4 n100 = vec4(dot(g100, vec3(Pf1.x, Pf0.yz)));
    vec4 n010 = vec4(dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z)));
    vec4 n110 = vec4(dot(g110, vec3(Pf1.xy, Pf0.z)));
    vec4 n001 = vec4(dot(g001, vec3(Pf0.xy, Pf1.z)));
    vec4 n101 = vec4(dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z)));
    vec4 n011 = vec4(dot(g011, vec3(Pf0.x, Pf1.yz)));
    vec4 n111 = vec4(dot(g111, Pf1));

    vec3 fade_xyz = fade(Pf0);
    vec3 dfade_xyz = dFade(Pf0); // Derivative of the fade function

    // Interpolating over the z-axis
    vec4 n_z = mix(vec4(n000.x, n100.x, n010.x, n110.x), vec4(n001.x, n101.x, n011.x, n111.x), fade_xyz.z);
    vec4 n_y = mix(vec4(n000.y, n100.y, n010.y, n110.y), vec4(n001.y, n101.y, n011.y, n111.y), fade_xyz.z);
    vec4 n_w = mix(vec4(n000.w, n100.w, n010.w, n110.w), vec4(n001.w, n101.w, n011.w, n111.w), fade_xyz.z);

    vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
    float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x);

    // Gradient computation (component-wise interpolation)
    vec3 gradient = dfade_xyz * vec3(
                                    mix(mix(dot(g000, Pf0), dot(g100, Pf1), fade_xyz.x),
                                        mix(dot(g010, Pf0), dot(g110, Pf1), fade_xyz.x), fade_xyz.y),
                                    mix(mix(dot(g001, Pf0), dot(g101, Pf1), fade_xyz.x),
                                        mix(dot(g011, Pf0), dot(g111, Pf1), fade_xyz.x), fade_xyz.y),
                                    mix(mix(dot(g000, Pf0), dot(g001, Pf1), fade_xyz.x),
                                        mix(dot(g100, Pf0), dot(g101, Pf1), fade_xyz.x), fade_xyz.y));

    PerlinNoise3D result;
    result.value = 2.2 * n_xyz; // Scale to match typical Perlin noise range
    result.gradient = gradient; // Return the computed gradient
    return result;
}

#endif