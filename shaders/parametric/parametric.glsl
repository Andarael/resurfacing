#ifndef PARAMETRIC_GLSL
#define PARAMETRIC_GLSL

// ============== Extensions ==============

#extension GL_EXT_mesh_shader : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_KHR_shader_subgroup_arithmetic : enable

// ============== Includes ==============
#define RESURFACING_PIPELINE // enable resurfacing config ubo
#include "../shaderInterface.h"

#include "../common.glsl"
#include "parametricSurfaces.glsl"

// ============== Constants ==============

#define MESH_GROUP_SIZE 32
#define TASK_GROUP_SIZE 1

// #define SMALL_GRID

#ifdef SMALL_GRID // optimized for small grids (4x4)
#define MAX_VERTICES 25
#define MAX_PRIMITIVES 32

#else // optimized for (8x8)
#define MAX_VERTICES 81
#define MAX_PRIMITIVES 128

#endif

#define PI 3.14159265359

// ============== Descriptor Sets ==============

// ================== Payloads =====================

struct TaskPayload {
    vec3 position;
    vec3 normal;
    float area;
    uint taskId;
    bool isVertex;
    uvec2 MN;
    uvec2 deltaUV;
    uint elementType;
#ifdef DISPLAY_DEBUG_DATA
    vec4 debug;
#endif
};

#ifndef FRAGMENT_SHADER
taskPayloadSharedEXT TaskPayload taskPayload;
#endif

// ================== Functions =====================

vec2 getBaseUv(uint taskId) {
    uint nbFaces = resurfacingUbo.nbFaces;
    bool isVertex = (taskId >= nbFaces);
    uint vertId = 0;

    if (isVertex) {
        vertId = taskId - nbFaces;
    } else {
        vertId = getVertIdFace(taskId);
    }

    return getVertexTexCoord(vertId);
}

#ifndef FRAGMENT_SHADER

void parametricPosition(vec2 uv, out vec3 position, out vec3 normal, uint elementType) {
    vec3 pos = vec3(-10.0);
    vec3 nrm = vec3(0.0);

    uvec2 Nxy = uvec2(resurfacingUbo.Nx, resurfacingUbo.Ny);
    float a = resurfacingUbo.majorRadius;
    float b = resurfacingUbo.minorRadius;

    switch (elementType) {
    case 0:
        parametricTorus(uv, pos, nrm, a, b);
        break;
    case 1:
        parametricSphere(uv, pos, nrm, a);
        break;
    case 2:
        parametricMobiusStrip(uv, pos, nrm, a);
        break;
    case 3:
        parametricKleinBottle(uv, pos, nrm, a);
        break;
    case 4:
        parametricHyperbolicParaboloid(uv, pos, nrm, a, b);
        break;
    case 5:
        parametricHelicoid(uv, pos, nrm, a);
        break;
    case 6:
        parametricCone(uv, pos, nrm, a, b);
        break;
    case 7:
        parametricCylinder(uv, pos, nrm, a, b);
        break;
    case 8:
        parametricEgg(uv, pos, nrm, a, b);
        break;
    case 9:
        evaluateBsplineSurface(uv, pos, nrm, Nxy, resurfacingUbo.cyclicU, resurfacingUbo.cyclicV);
        break;
    case 10:
        evaluateBezierSurface(uv, pos, nrm, Nxy, resurfacingUbo.cyclicU, resurfacingUbo.cyclicV, resurfacingUbo.degree);
        break;
    }

    position = pos;
    normal = nrm;
}

void parametricBoundingBox(in out LodInfos lodInfos, uint elementType) {
    float a = resurfacingUbo.majorRadius * sqrt(lodInfos.area) * resurfacingUbo.scaling;
    float b = resurfacingUbo.minorRadius * sqrt(lodInfos.area) * resurfacingUbo.scaling;

    // Use the switch-case structure to calculate the size of the selected parametric primitive
    switch (elementType) {
    case 0:
        parametricTorusScreenSpaceSize(lodInfos, a, b);
        break;
    case 1:
        parametricSphereScreenSpaceSize(lodInfos, a);
        break;
    case 2:
        parametricMobiusStripScreenSpaceSize(lodInfos, a);
        break;
    case 3:
        parametricKleinBottleScreenSpaceSize(lodInfos, a);
        break;
    case 4:
        parametricHyperbolicParaboloidScreenSpaceSize(lodInfos, a, b);
        break;
    case 5:
        parametricHelicoidScreenSpaceSize(lodInfos, a, resurfacingUbo.scaling);
        break;
    case 6:
        parametricConeScreenSpaceSize(lodInfos, a, b);
        break;
    case 7:
        parametricCylinderScreenSpaceSize(lodInfos, a, b);
        break;
    case 8:
        parametricEggScreenSpaceSize(lodInfos, a, b);
        break;
    case 9:
        lodInfos.minBound = resurfacingUbo.minLutExtent * sqrt(lodInfos.area) * resurfacingUbo.scaling;
        lodInfos.maxBound = resurfacingUbo.maxLutExtent * sqrt(lodInfos.area) * resurfacingUbo.scaling;
        break;
    case 10:
        lodInfos.minBound = resurfacingUbo.minLutExtent * sqrt(lodInfos.area) * resurfacingUbo.scaling;
        lodInfos.maxBound = resurfacingUbo.maxLutExtent * sqrt(lodInfos.area) * resurfacingUbo.scaling;
        break;
    }
}

uvec2 getLodMN(LodInfos lodInfos, uint elementType) {
    uvec2 MN = resurfacingUbo.MN;
    uvec2 minRes = min(resurfacingUbo.minResolution, MN);
    uvec2 maxRes = MN;

    if (!resurfacingUbo.doLod) { return MN; }

    float lodFactor = resurfacingUbo.lodFactor;
    parametricBoundingBox(lodInfos, elementType);
    float screenSpaceSize = boundingBoxScreenSpaceSize(lodInfos);
    uvec2 targetLodMN = uvec2(MN * sqrt(screenSpaceSize * lodFactor));

    return min(max(targetLodMN, minRes), maxRes);
}

// conform to 16x8 max
uvec2 getDeltaUVhelper(uvec2 MN, uint maxVertices, uint maxPrimitives) {

    uint maxDeltaU = min(MN.x, uint(floor(sqrt(maxPrimitives / 2.0))));

    for (uint deltaU = maxDeltaU; deltaU >= 1; deltaU--) {
        uint maxDeltaV = min(MN.y, maxPrimitives / (2 * deltaU));
        for (uint deltaV = maxDeltaV; deltaV >= 1; deltaV--) {
            uint numVertices = (deltaU + 1) * (deltaV + 1);
            uint numPrimitives = deltaU * deltaV * 2;

            if (numVertices <= maxVertices && numPrimitives <= maxPrimitives) {
                return uvec2(deltaU, deltaV);
            }
        }
    }

    return uvec2(1, 1);
}

uvec2 getDeltaUV(uvec2 MN) {
    return getDeltaUVhelper(MN, MAX_VERTICES, MAX_PRIMITIVES);
}

#endif // FRAGMENT_SHADER

#endif // PARAMETRIC_GLSL