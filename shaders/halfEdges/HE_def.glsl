
#ifndef HALF_EDGE_DEF_GLSL
#define HALF_EDGE_DEF_GLSL
#extension GL_EXT_shader_atomic_float : require
#define lid gl_LocalInvocationID.x

// ============== Constants ==============

#define MAX_VERTS_HE 12

// ============== Structs ================

/*
    struct Vertex {
        vec4 position;
        vec4 color;
        vec4 normal;
        vec2 texCoord;
        int edge;
    };

    struct HalfEdge {
        int vertex; // vertex at the end of the half-edge
        int face;   // face this half-edge borders
        int next;   // next half-edge in the face loop
        int prev;   // next half-edge in the face loop
        int twin;   // next half-edge in the face loop
    };

    struct HEFace {
        vec4 normal;   // face normal
        vec4 center;   // face center
        int edge;      // one half-edge bordering the face
        int vertCount; // number of vertices in the face
        int offset;    // offset in the vertexFaceIndices buffer
        float area;    // face area

    };
 */

// ============== Buffers ================

// Vertex attributes in separate buffers (Structure of Arrays)
layout(std430, set = 1, binding = 0) readonly buffer heVertexPositionBuffer { vec4 vertexPositions[]; };
layout(std430, set = 1, binding = 1) readonly buffer heVertexColorBuffer { vec4 vertexColors[]; };
layout(std430, set = 1, binding = 2) readonly buffer heVertexNormalBuffer { vec4 vertexNormals[]; };
layout(std430, set = 1, binding = 3) readonly buffer heVertexTexCoordBuffer { vec2 vertexTexCoords[]; };
layout(std430, set = 1, binding = 4) readonly buffer heVertexEdgeBuffer { int vertexEdges[]; };

// Face data in separate buffers
layout(std430, set = 2, binding = 0) readonly buffer heFaceEdgeBuffer { int faceEdges[]; };
layout(std430, set = 2, binding = 1) readonly buffer heFaceVertCountBuffer { int faceVertCounts[]; };
layout(std430, set = 2, binding = 2) readonly buffer heFaceOffsetBuffer { int faceOffsets[]; };
layout(std430, set = 2, binding = 3) readonly buffer heFaceNormalBuffer { vec4 faceNormals[]; };
layout(std430, set = 2, binding = 4) readonly buffer heFaceCenterBuffer { vec4 faceCenters[]; };
layout(std430, set = 2, binding = 5) readonly buffer heFaceAreaBuffer { float faceAreas[]; };

// Half-edge data in separate buffers
layout(std430, set = 3, binding = 0) readonly buffer heHalfEdgeVertexBuffer { int halfEdgeVertices[]; };
layout(std430, set = 3, binding = 1) readonly buffer heHalfEdgeFaceBuffer { int halfEdgeFaces[]; };
layout(std430, set = 3, binding = 2) readonly buffer heHalfEdgeNextBuffer { int halfEdgeNext[]; };
layout(std430, set = 3, binding = 3) readonly buffer heHalfEdgePrevBuffer { int halfEdgePrev[]; };
layout(std430, set = 3, binding = 4) readonly buffer heHalfEdgeTwinBuffer { int halfEdgeTwins[]; };

// Vertex-face index buffer
layout(std430, set = 4, binding = 0) readonly buffer vertexFaceIndexBuffer { int vertexFaceIndices[]; };

// ============== Getters =================

vec3 getVertexPosition(uint vertId) { return vertexPositions[vertId].xyz; }
vec3 getVertexColor(uint vertId) { return vertexColors[vertId].xyz; }
vec3 getVertexNormal(uint vertId) { return vertexNormals[vertId].xyz; }
vec2 getVertexTexCoord(uint vertId) { return vertexTexCoords[vertId]; }
uint getVertexEdge(uint vertId) { return vertexEdges[vertId]; }

uint getFaceEdge(uint faceId) { return faceEdges[faceId]; }
uint getFaceVertCount(uint faceId) { return faceVertCounts[faceId]; }
uint getFaceOffset(uint faceId) { return faceOffsets[faceId]; }
vec3 getFaceNormal(uint faceId) { return faceNormals[faceId].xyz; }
vec3 getFaceCenter(uint faceId) { return faceCenters[faceId].xyz; }
float getFaceArea(uint faceId) { return faceAreas[faceId]; }

uint getHalfEdgeVertex(uint edgeId) { return halfEdgeVertices[edgeId]; }
uint getHalfEdgeFace(uint edgeId) { return halfEdgeFaces[edgeId]; }
uint getHalfEdgeNext(uint edgeId) { return halfEdgeNext[edgeId]; }
uint getHalfEdgePrev(uint edgeId) { return halfEdgePrev[edgeId]; }
uint getHalfEdgeTwin(uint edgeId) { return halfEdgeTwins[edgeId]; }

uint getVertexFaceIndex(uint vertId) { return vertexFaceIndices[vertId]; }
uint getVertIdFace(uint faceId) { return getHalfEdgeVertex(getFaceEdge(faceId)); }

uint getFaceId(uint vertId) { return getHalfEdgeFace(getVertexEdge(vertId)); }

uint getFaceValencef(uint faceId) { return faceVertCounts[faceId]; }
uint getFaceValencev(uint vertId) { return faceVertCounts[getFaceId(vertId)]; }

uint getVertValence(uint vertId) {
    uint edgeId = getVertexEdge(vertId);
    uint startEdgeId = edgeId;
    uint valence = 0;
    do {
        valence++;
        edgeId = getHalfEdgeNext(edgeId);
    } while (edgeId != startEdgeId);
    return valence;
}

vec3 getVertexPosRelative(uint faceId, uint vertIndexRelative) {
    uint offset = getFaceOffset(faceId);
    return getVertexPosition(getVertexFaceIndex(offset + vertIndexRelative));
}

vec2 getVertexTexCoordRelative(uint faceId, uint vertIndexRelative) {
    uint offset = getFaceOffset(faceId);
    return getVertexTexCoord(getVertexFaceIndex(offset + vertIndexRelative));
}

vec3 getVertexNormalRelative(uint faceId, uint vertIndexRelative) {
    uint offset = getFaceOffset(faceId);
    return getVertexNormal(getVertexFaceIndex(offset + vertIndexRelative));
}

uint getVertexIDRelative(uint faceId, uint vertIndexRelative) {
    uint offset = getFaceOffset(faceId);
    return getVertexFaceIndex(offset + vertIndexRelative);
}

// ============== Globals ================

// shared uint[MAX_VERTS_HE] sharedVertIndices; // vertex indices of the current face
// shared uint vertCount;                       // number of vertices in the current face
// shared uint faceId;                          // current face id
// shared uint offset;                          // offset in the vertexFaceIndices buffer

// vec3 getVertexPosShared(uint id) { return getVertexPosition(sharedVertIndices[id]); }

// ============== Functions ================

// void initSharedMemory(uint faceId) {
//     if (lid == 0) {
//         vertCount = getFaceVertCount(faceId);
//         offset = getFaceOffset(faceId);
//     }
//     barrier();
// }

// void fetchFaceData(uint faceId) {
//     initSharedMemory(faceId);

//     if (lid == 0) {
//         uint startEdgeId = getFaceEdge(faceId);
//         uint currentEdgeId = startEdgeId;
//         for (uint i = 0; i < MAX_VERTS_HE; i++) {
//             sharedVertIndices[i] = getHalfEdgeVertex(currentEdgeId);
//             currentEdgeId = getHalfEdgeNext(currentEdgeId);
//             if (currentEdgeId == startEdgeId)
//                 break;
//         }
//     }
//     barrier();
// }

// void fetchFaceDataFast(uint faceId) {
//     initSharedMemory(faceId);
//     if (lid >= MAX_VERTS_HE || lid >= vertCount) { return; }
//     sharedVertIndices[lid] = getVertexFaceIndex(offset + lid);
//     barrier();
// }

// vec3 computeFaceCenter(uint faceId) {
//     if (lid != 0) { return vec3(0); }

//     vec3 centerSum = vec3(0);
//     for (int i = 0; i < vertCount; i++) {
//         vec3 vertexPosition = getVertexPosShared(i).xyz;
//         centerSum += vertexPosition;
//     }
//     return centerSum / float(vertCount);
// }

// vec3 computeFaceNormal(uint faceId) {
//     if (lid != 0) { return vec3(0); }

//     vec3 v0 = getVertexPosShared(0).xyz;
//     vec3 v1 = getVertexPosShared(1).xyz;
//     vec3 v2 = getVertexPosShared(2).xyz;
//     return normalize(cross(v1 - v0, v2 - v0));
// }

// vec3 computeVertexNormal(uint vertId) {
//     if (lid != 0) { return vec3(0); }

//     int nextVertex = getHalfEdgeVertex(getHalfEdgeNext(getVertexEdge(vertId)));
//     int prevVertex = getHalfEdgeVertex(getHalfEdgePrev(getVertexEdge(vertId)));

//     vec3 v0 = getVertexPosition(vertId);
//     vec3 v1 = getVertexPosition(nextVertex);
//     vec3 v2 = getVertexPosition(prevVertex);

//     return normalize(cross(v1 - v0, v2 - v0));
// }

// float computeFaceArea(uint faceId) {
//     if (lid != 0) { return 0; }

//     vec3 center = getFaceCenter(faceId);
//     float area = 0.0;
//     for (int i = 0; i < vertCount; i++) {
//         vec3 v0 = getVertexPosShared(i).xyz;
//         vec3 v1 = getVertexPosShared((i + 1) % vertCount).xyz;
//         area += length(cross(v1 - v0, center - v0));
//     }

//     return area * 0.5;
// }



#endif // HALF_EDGE_DEF_GLSL

/*
    shared vec3 totalCenterSum;
    vec3 computeFaceCenterParallel(uint vertCount) {
        if (vertCount < 3 || vertCount > MAX_VERTS_HE)
            return vec3(0);

        vec3 centerSum = vec3(0.0);

        vec3 vertexPosition = vertices[sharedVertIndices[lid]].position.xyz;
        centerSum = vertexPosition;

        if (lid == 0) {
            totalCenterSum = vec3(0.0);
        }

        barrier();

        if (lid < vertCount) {
            atomicAdd(totalCenterSum.x, centerSum.x);
            atomicAdd(totalCenterSum.y, centerSum.y);
            atomicAdd(totalCenterSum.z, centerSum.z);
        }

        barrier();

        vec3 faceCenter = totalCenterSum / float(vertCount);

        return faceCenter;
    }
 */