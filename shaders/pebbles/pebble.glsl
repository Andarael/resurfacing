#ifndef PEBBLE_HELPER_GLSL
#define PEBBLE_HELPER_GLSL

// ================== Constants =====================
#define PEBBLE_PIPELINE // enable pebble config ubo
#include "../shaderInterface.h"
#include "../common.glsl"

#define MAX_NGON_VERTICES 12

// todo find better names
#define FACE_PER_PATCH 10 // number of faces per patch
#define VERT_PER_PATCH 12 // number of vertices per patch
#define NB_RINGS 6        // number of patch rings

#define MAX_FACES MAX_NGON_VERTICES *FACE_PER_PATCH    // total number of faces for all patches
#define MAX_VERTICES MAX_NGON_VERTICES *VERT_PER_PATCH // total number of vertices for all patches
#define MAX_PATCHES MAX_NGON_VERTICES *NB_RINGS        // total number of patches

#define MAX_SUBDIV_PER_WORKGROUP 3 // we can't fit more than 3 subdivisions in a single mesh shader
#define MAX_SUBDIVISION_LEVEL 9    // max 65k patches (hardware limit)

// ================== Payloads =====================

struct Task {               // per workgroup output
    uint baseID;            // Taks ID
    uint targetSubdivLevel; // target subdivision level
    float scale;            // random value
};

shared uint[MAX_NGON_VERTICES] sharedVertIndices;
shared uint vertCount;
shared uint faceOffset;

void fetchFaceData(uint faceId) {
    uint count = getFaceVertCount(faceId);
    uint off = getFaceOffset(faceId);
    if (lid < MAX_NGON_VERTICES || lid < count) {
        sharedVertIndices[lid] = getVertexFaceIndex(off + lid);
    }
    if (lid == 0) {
        vertCount = count;
        faceOffset = off;
    }
    barrier();
}

vec3 getVertexPosShared(uint id) { return getVertexPosition(sharedVertIndices[id]); }

#endif