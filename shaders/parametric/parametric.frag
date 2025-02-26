#version 460

#define FRAGMENT_SHADER

#include "parametric.glsl" // must be included before any other include

#include "../shaderInterface.h"
#include "../shading.glsl"
#include "../stdPerVertexMesh.glsl"


void main() {
    vec3 worldPos = getWorldPos();
    vec3 normal = normalize(getNormal());
    vec2 baseUV = getBaseUv(perPrimitive.data.x);
    baseUV.y = 1.0 - baseUV.y;
    vec2 localUV = getLocalUV();

#ifdef DISPLAY_DEBUG_DATA
    float x = uintBitsToFloat(perPrimitive.data.x);
    float y = uintBitsToFloat(perPrimitive.data.y);
    float z = uintBitsToFloat(perPrimitive.data.z);
    float w = uintBitsToFloat(perPrimitive.data.w);
    vec4 debugData = vec4(x, y, z, w);
    outColor = debugData;
#else
    outColor = vec4(shading(worldPos, normal, localUV, baseUV, shadingUbo.doAo), 1);
#endif
}