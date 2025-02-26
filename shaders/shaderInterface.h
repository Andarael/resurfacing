#ifndef SHADERINTERFACE_H
#define SHADERINTERFACE_H
#ifdef __cplusplus
namespace shaderInterface {
#else
#extension GL_EXT_scalar_block_layout : require
#endif


const int UBOSet = 0;
const int HESet = 1;
const int OtherSet = 2;

// set 0 - UBOs
const int U_viewBinding = 0;
const int U_configBinding = 1;
const int U_globalShadingBinding = 2;
const int U_shadingBinding = 3;

// set 1 - Half-Edge Data
const int B_heVec4TypeBinding = 0;
const int B_heVec2TypeBinding = 1;
const int B_heIntTypeBinding = 2;
const int B_heFloatTypeBinding = 3;

// set 2 - Other Data
const int B_lutVertexBufferBinding = 0;
const int B_skinJointsIndicesBinding = 1;
const int B_skinJointsWeightsBinding = 2;
const int B_skinBoneMatricesBinding = 3;
const int S_samplersBinding = 4;
const int T_texturesBinding = 5;


// ============== Textures info ================
const int linearSamplerID = 0;
const int nearestSamplerID = 1;
const int samplerCount = 2;

const int AOTextureID = 0;
const int elementTextureID = 1;
const int textureCount = 2;


// ============== Half-Edge Data ================

// vec4 types data
const int heVertexPositions = 0;
const int heVertexColors = 1;
const int heVertexNormals = 2;
const int heFaceNormals = 3;
const int heFaceCenters = 4;
const int vec4DataCount = 5;

// vec2 types data
const int heVertexTexCoords = 0;
const int vec2DataCount = 1;

// int types data
const int heVertexEdges = 0;
const int heFaceEdges = 1;
const int heFaceVertCounts = 2;
const int heFaceOffsets = 3;
const int heHalfEdgeVertex = 4;
const int heHalfEdgeFace = 5;
const int heHalfEdgeNext = 6;
const int heHalfEdgePrev = 7;
const int heHalfEdgeTwin = 8;
const int heVertexFaceIndex = 9;
const int intDataCount = 10;

// float types data
const int heFaceAreas = 0;
const int floatDataCount = 1;

#ifndef __cplusplus
#define lid gl_LocalInvocationID.x       // local thread ID
#define gid gl_GlobalInvocationID.x      // global thread ID
#define groupId gl_WorkGroupID.x         // workgroup ID
#define workgroupSize gl_WorkGroupSize.x // workgroup size

#define MAX_VERTS_HE 12
// Vertex attributes in separate buffers (Structure of Arrays)
layout(std430, set = HESet, binding = B_heVec4TypeBinding) readonly buffer heVertexPositionBuffer { vec4 data[]; }heVec4Buffer[vec4DataCount];
layout(std430, set = HESet, binding = B_heVec2TypeBinding) readonly buffer heVertexColorBuffer { vec2 data[]; }heVec2Buffer[vec2DataCount];
layout(std430, set = HESet, binding = B_heIntTypeBinding) readonly buffer heVertexNormalBuffer { int data[]; }heIntBuffer[intDataCount];
layout(std430, set = HESet, binding = B_heFloatTypeBinding) readonly buffer heVertexTexCoordBuffer { float data[]; }heFloatBuffer[floatDataCount];

// ============== Getters =================

vec3 getVertexPosition(uint vertId) { return heVec4Buffer[heVertexPositions].data[vertId].xyz; }
vec3 getVertexColor(uint vertId) { return heVec4Buffer[heVertexColors].data[vertId].xyz; }
vec3 getVertexNormal(uint vertId) { return heVec4Buffer[heVertexNormals].data[vertId].xyz; }
vec2 getVertexTexCoord(uint vertId) { return heVec2Buffer[heVertexTexCoords].data[vertId]; }
uint getVertexEdge(uint vertId) { return heIntBuffer[heVertexEdges].data[vertId]; }

uint getFaceEdge(uint faceId) { return heIntBuffer[heFaceEdges].data[faceId]; }
uint getFaceVertCount(uint faceId) { return heIntBuffer[heFaceVertCounts].data[faceId]; }
uint getFaceOffset(uint faceId) { return heIntBuffer[heFaceOffsets].data[faceId]; }
vec3 getFaceNormal(uint faceId) { return heVec4Buffer[heFaceNormals].data[faceId].xyz; }
vec3 getFaceCenter(uint faceId) { return heVec4Buffer[heFaceCenters].data[faceId].xyz; }
float getFaceArea(uint faceId) { return heFloatBuffer[heFaceAreas].data[faceId]; }

uint getHalfEdgeVertex(uint edgeId) { return heIntBuffer[heHalfEdgeVertex].data[edgeId]; }
uint getHalfEdgeFace(uint edgeId) { return heIntBuffer[heHalfEdgeFace].data[edgeId]; }
uint getHalfEdgeNext(uint edgeId) { return heIntBuffer[heHalfEdgeNext].data[edgeId]; }
uint getHalfEdgePrev(uint edgeId) { return heIntBuffer[heHalfEdgePrev].data[edgeId]; }
uint getHalfEdgeTwin(uint edgeId) { return heIntBuffer[heHalfEdgeTwin].data[edgeId]; }

uint getVertexFaceIndex(uint vertId) { return heIntBuffer[heVertexFaceIndex].data[vertId]; }
uint getVertIdFace(uint faceId) { return getHalfEdgeVertex(getFaceEdge(faceId)); }

uint getFaceId(uint vertId) { return getHalfEdgeFace(getVertexEdge(vertId)); }

uint getFaceValencef(uint faceId) { return heIntBuffer[heFaceVertCounts].data[faceId]; }
uint getFaceValencev(uint vertId) { return heIntBuffer[heFaceVertCounts].data[getFaceId(vertId)]; }

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


// ============== Other Data ================

layout(push_constant) uniform PushConstants {
    mat4 model;
}
constants;

layout(std430, set = UBOSet, binding = 0) uniform ViewUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    float near;
    float far;
}
viewUbo;

layout(std430, set = UBOSet, binding = 2) uniform GlobalShadingUbo {
    vec3 lightPos;
    bool linkLight;
    vec3 lightColor;
    bool filmic;
    vec3 viewPos;
    bool shadingHack;
}
globalShadingUbo;

layout(std430, set = UBOSet, binding = 1) uniform ShadingUbo {
    vec3 ambient;
    float shininess;
    vec3 diffuse;
    float specularStrength;
    vec3 specular;
    int colorMode;
    bool showElementTexture;
    bool doAo;
    bool doShading;
    bool displayNormals;
    bool displayPrimId;
    bool displayLocalUv;
    bool displayGlobalUv;
}
shadingUbo;


// ============== Other Data ================

layout(std430, set = OtherSet, binding = B_lutVertexBufferBinding) readonly buffer lutVertexBuffer { vec4 lutVertex[]; };
layout(std430, set = OtherSet, binding = B_skinJointsIndicesBinding) readonly buffer skinJointsIndices { vec4 jointsIndices[]; };
layout(std430, set = OtherSet, binding = B_skinJointsWeightsBinding) readonly buffer skinJointsWeights { vec4 jointsWeights[]; };
layout(std430, set = OtherSet, binding = B_skinBoneMatricesBinding) readonly buffer skinBoneMatrices { mat4 boneMatrices[]; };

layout(set = OtherSet, binding = S_samplersBinding) uniform sampler samplers[samplerCount];
layout(set = OtherSet, binding = T_texturesBinding) uniform texture2D textures[textureCount];


#endif


#ifdef __cplusplus
}
#endif
#endif
