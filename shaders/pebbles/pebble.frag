#version 460

#define FRAGMENT_SHADER

#include "../shaderInterface.h"

#include "../common.glsl"
#include "../stdPerVertexMesh.glsl"
#include "../shading.glsl"
#include "../noise.glsl"

vec2 getBaseUv(uint faceId) {
    uint vertId = getVertIdFace(faceId);
    return getVertexTexCoord(vertId);
}

void main() {
    vec3 worldPos = getWorldPos();
    vec3 normal = normalize(getNormal());
    vec2 baseUV = getBaseUv(perPrimitive.data.x);
    baseUV.y = 1.0 - baseUV.y;
    vec2 localUV = getLocalUV();

    outColor = vec4(shading(worldPos, normal, localUV, baseUV, shadingUbo.doAo), 1);
    seed = perPrimitive.data.x;
    outColor *= rand(0.5, 1);
}
