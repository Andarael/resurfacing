#ifndef SHADERINTERFACE_H
#define SHADERINTERFACE_H
#ifdef __cplusplus
#include "imgui.h"
#include <vulkan/vulkan.hpp>
#include "vkHelper.hpp"
#include "renderer.hpp"
#include "defines.hpp"
#undef near
#undef far

#define HE_PIPELINE
#define RESURFACING_PIPELINE
#define PEBBLE_PIPELINE

namespace shaderInterface {
#else
#extension GL_EXT_scalar_block_layout : require
#endif

#ifndef __cplusplus
#define CONSTEXPR const
#else
#define CONSTEXPR constexpr
#endif

CONSTEXPR int SceneSet = 0;
CONSTEXPR int HESet = 1;
CONSTEXPR int PerObjectSet = 2;

// set 0 - UBOs
CONSTEXPR int U_viewBinding = 0;
CONSTEXPR int U_globalShadingBinding = 1;

// set 1 - Half-Edge Data
CONSTEXPR int B_heVec4TypeBinding = 0;
CONSTEXPR int B_heVec2TypeBinding = 1;
CONSTEXPR int B_heIntTypeBinding = 2;
CONSTEXPR int B_heFloatTypeBinding = 3;

// set 2 - Other Data
CONSTEXPR int U_configBinding = 0;
CONSTEXPR int U_shadingBinding = 1;
CONSTEXPR int B_lutVertexBufferBinding = 2;
CONSTEXPR int B_skinJointsIndicesBinding = 3;
CONSTEXPR int B_skinJointsWeightsBinding = 4;
CONSTEXPR int B_skinBoneMatricesBinding = 5;
CONSTEXPR int S_samplersBinding = 6;
CONSTEXPR int T_texturesBinding = 7;


// ============== Textures info ================
CONSTEXPR int linearSamplerID = 0;
CONSTEXPR int nearestSamplerID = 1;
CONSTEXPR int samplerCount = 2;

CONSTEXPR int AOTextureID = 0;
CONSTEXPR int elementTextureID = 1;
CONSTEXPR int textureCount = 2;


// ============== Half-Edge Data ================

// vec4 types data
CONSTEXPR int heVertexPositions = 0;
CONSTEXPR int heVertexColors = 1;
CONSTEXPR int heVertexNormals = 2;
CONSTEXPR int heFaceNormals = 3;
CONSTEXPR int heFaceCenters = 4;
CONSTEXPR int vec4DataCount = 5;

// vec2 types data
CONSTEXPR int heVertexTexCoords = 0;
CONSTEXPR int vec2DataCount = 1;

// int types data
CONSTEXPR int heVertexEdges = 0;
CONSTEXPR int heFaceEdges = 1;
CONSTEXPR int heFaceVertCounts = 2;
CONSTEXPR int heFaceOffsets = 3;
CONSTEXPR int heHalfEdgeVertex = 4;
CONSTEXPR int heHalfEdgeFace = 5;
CONSTEXPR int heHalfEdgeNext = 6;
CONSTEXPR int heHalfEdgePrev = 7;
CONSTEXPR int heHalfEdgeTwin = 8;
CONSTEXPR int heVertexFaceIndex = 9;
CONSTEXPR int intDataCount = 10;

// float types data
CONSTEXPR int heFaceAreas = 0;
CONSTEXPR int floatDataCount = 1;

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

layout(std430, set = PerObjectSet, binding = B_lutVertexBufferBinding) readonly buffer lutVertexBuffer { vec4 lutVertex[]; };
layout(std430, set = PerObjectSet, binding = B_skinJointsIndicesBinding) readonly buffer skinJointsIndices { vec4 jointsIndices[]; };
layout(std430, set = PerObjectSet, binding = B_skinJointsWeightsBinding) readonly buffer skinJointsWeights { vec4 jointsWeights[]; };
layout(std430, set = PerObjectSet, binding = B_skinBoneMatricesBinding) readonly buffer skinBoneMatrices { mat4 boneMatrices[]; };

layout(set = PerObjectSet, binding = S_samplersBinding) uniform sampler samplers[samplerCount];
layout(set = PerObjectSet, binding = T_texturesBinding) uniform texture2D textures[textureCount];

#endif

// ============== UBOs ================
#ifdef __cplusplus
struct Bool32 {
    uint32_t value;
    Bool32() : value(0) {}
    Bool32(bool b) : value(b ? 1 : 0) {}
    operator bool() const { return value != 0; }
    operator bool *() { return reinterpret_cast<bool *>(&value); }
    operator const bool *() const { return reinterpret_cast<const bool *>(&value); }
    bool *operator&() { return reinterpret_cast<bool *>(&value); }
    const bool *operator&() const { return reinterpret_cast<const bool *>(&value); }
};

using glm::uint;
#endif

#ifndef __cplusplus
#define PushConstantStruct layout(push_constant) uniform
#define UBOStruct(ALIGNEMENT, SET, BINDING) layout(ALIGNEMENT, set = SET, binding = BINDING) uniform
#define UBOName(NAME) NAME // we declare at the same time
#define UBODefaultVal(V) // we can't have default values in GLSL
#define BOOL bool
#else
#define PushConstantStruct struct
#define UBOStruct(ALIGNEMENT, SET, BINDING) struct
#define UBOName(NAME) // we dont want globals in cpu
#define UBODefaultVal(V) = V // default value for UBOs
#define BOOL Bool32
#endif

PushConstantStruct PushConstants {
    mat4 model;
}UBOName(constants);

UBOStruct(scalar, SceneSet, U_viewBinding) ViewUBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    float near;
    float far;
}UBOName(viewUbo);

UBOStruct(scalar, SceneSet, U_globalShadingBinding) GlobalShadingUBO {
    BOOL filmic UBODefaultVal(true);
    vec3 lightPos UBODefaultVal(vec3(0));
    vec3 lightColor UBODefaultVal(vec3(1));
    vec3 viewPos UBODefaultVal(vec3(0));
    BOOL linkLight UBODefaultVal(true);
    BOOL shadingHack UBODefaultVal(true);
#ifdef __cplusplus
    void displayUI() {
        if (ImGui::CollapsingHeader("Global Shading UBO")) {
            ImGui::Text("Global Shading UBO");
            ImGui::Checkbox("Filmic", &filmic);
            ImGui::Checkbox("COS Lift", &shadingHack);
            ImGui::Checkbox("Link Light", &linkLight);
            ImGui::ColorEdit3("Light Color", &lightColor[0]);
            ImGui::DragFloat3("Light Position", &lightPos[0], -10, 10);
            ImGui::DragFloat3("View Position", &viewPos[0], -10, 10);
        }
    }
#endif
}UBOName(globalShadingUbo);

UBOStruct(scalar, PerObjectSet, U_shadingBinding) ShadingUBO {
    vec3 ambient UBODefaultVal(vec3(0));
    vec3 diffuse UBODefaultVal(vec3(1));
    vec3 specular UBODefaultVal(vec3(1));
    float shininess UBODefaultVal(32.f);
    float specularStrength UBODefaultVal(1.f);
    BOOL doAo UBODefaultVal(false);
    BOOL showElementTexture UBODefaultVal(false);
    BOOL doShading UBODefaultVal(true);
    BOOL displayNormals UBODefaultVal(false);
    BOOL displayPrimId UBODefaultVal(false);
    int colorMode UBODefaultVal(2);
    BOOL displayLocalUv UBODefaultVal(false);
    BOOL displayGlobalUv UBODefaultVal(false);
#ifdef __cplusplus
    void displayUI(std::string meshName = "") {
        float multiplier = 1.0f;
        if (ImGui::CollapsingHeader(("Shading UBO " + meshName).c_str())) {
            ImGui::PushItemWidth(100.0f);
            ImGui::ColorEdit3("Ambient", &ambient[0]);
            ImGui::ColorEdit3("Diffuse", &diffuse[0]);
            ImGui::SliderFloat("Shininess", &shininess, 1, 2048, "%.1f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Specular Strength", &specularStrength, 0, 1000, "%.1f", ImGuiSliderFlags_Logarithmic);
            ImGui::PopItemWidth();

            ImGui::Checkbox("Show Element Texture", &showElementTexture);
            ImGui::Checkbox("Do Shading", &doShading);
            ImGui::Checkbox("Display Normals", &displayNormals);
            ImGui::Checkbox("Display Prim Id", &displayPrimId);
            ImGui::SameLine();
            ImGui::PushItemWidth(150.0f);
            ImGui::Combo("Color Mode", reinterpret_cast<int *>(&colorMode), "Color Per Task\0Color Per Mesh\0Color Per Primitive\0");
            ImGui::PopItemWidth();
            ImGui::Checkbox("Display Local UV", &displayLocalUv);
            ImGui::Checkbox("Display Global UV", &displayGlobalUv);
        }
    }
#endif
}UBOName(shadingUbo);

#ifdef HE_PIPELINE
UBOStruct(scalar, PerObjectSet, U_configBinding) HeUBO {
    int nbFaces UBODefaultVal(0);
    BOOL colorPerPrimitive UBODefaultVal(0);
    float normalOffset UBODefaultVal(0.0f);
    BOOL doSkinning UBODefaultVal(0);
#ifdef __cplusplus
    void displayUI(std::string meshName = "") {
        if (ImGui::CollapsingHeader(("HE UBO " + meshName).c_str())) {
            ImGui::PushItemWidth(100.0f);
            ImGui::InputInt("Nb Faces", &nbFaces, 1, 100);
            ImGui::Checkbox("Color Per Primitive", &colorPerPrimitive);
            ImGui::SliderFloat("Normal Offset", &normalOffset, 0.001f, 0.1f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::Separator();
        }
    }
#endif
}UBOName(heUbo);
#endif

#ifdef RESURFACING_PIPELINE
UBOStruct(scalar, PerObjectSet, U_configBinding) ResurfacingUBO {
    int nbFaces UBODefaultVal(0);
    int nbVertices UBODefaultVal(0);
    int elementType UBODefaultVal(0);
    float scaling UBODefaultVal(.5f);
    BOOL backfaceCulling UBODefaultVal(false);
    float cullingThreshold UBODefaultVal(.5f);
    BOOL doLod UBODefaultVal(false);
    float lodFactor UBODefaultVal(1.f);
    BOOL renderMesh UBODefaultVal(true);
    BOOL hasElementTypeTexture UBODefaultVal(false);

    float minorRadius UBODefaultVal(.4f);
    float majorRadius UBODefaultVal(1.f);
    vec3 normal1 UBODefaultVal(vec3(0,0, 1));
    vec3 normal2 UBODefaultVal(vec3(0,0, 1));
    float normalPerturbation UBODefaultVal(0.f);
    uvec2 MN UBODefaultVal(uvec2(16, 16));
    uvec2 minResolution UBODefaultVal(uvec2(4, 4));
    int edgeMode UBODefaultVal(0);
    BOOL cyclicU UBODefaultVal(false);
    BOOL cyclicV UBODefaultVal(false);
    uint Nx UBODefaultVal(0);
    uint Ny UBODefaultVal(0);
    uint degree UBODefaultVal(3);
    vec3 minLutExtent UBODefaultVal(vec3(0));
    vec3 maxLutExtent UBODefaultVal(vec3(0));
    BOOL doSkinning UBODefaultVal(false);
#ifdef __cplusplus
    void displayUI(std::string meshName = "") {
        if (ImGui::CollapsingHeader(("Resurfacing UBO " + meshName).c_str())) {
            ImGui::PushItemWidth(200.0f);

            ImGui::Checkbox("Render Mesh", &renderMesh);

            ImGui::Text("Base Mesh Nb faces: %d", nbFaces);
            ImGui::Text("Base Mesh Nb vertices: %d", nbVertices);
            ImGui::Text("Parametric patch : Vertices: %d, Quads %d, Tris%d", (MN.x + 1) * (MN.y + 1), MN.x * MN.y, MN.x * MN.y * 2);

            ImGui::Separator();

            const char *elementTypesNames[] = {"torus", "sphere", "Mobius", "Klein", "Hyperbolic", "Helicoid", "Cone", "Cylinder", "Egg", "B-Spline", "Bezier"};
            ImGui::Combo("Element Type", &elementType, elementTypesNames, IM_ARRAYSIZE(elementTypesNames));
            // ImGui::SliderInt("elementType", &elementType, 0, 10);

            ImGui::SliderFloat("Scaling", &scaling, 0.01f, 10, "%.2f", ImGuiSliderFlags_Logarithmic);

            ImGui::Checkbox("Culling", &backfaceCulling);
            if (backfaceCulling) {
                ImGui::SameLine();
                ImGui::SliderFloat("Threshold", &cullingThreshold, 0, 1, "%.2f");
            }

            ImGui::Checkbox("Do LOD", &doLod);
            if (doLod) {
                ImGui::PushItemWidth(50.0f);
                ImGui::SameLine();
                ImGui::SliderFloat("lodFactor", &lodFactor, 0, 100, "%.1f", ImGuiSliderFlags_Logarithmic);
                ImGui::SameLine();
                ImGui::PopItemWidth();
                ImGui::PushItemWidth(100.0f);
                ImGui::SliderInt2("Min Resolution", reinterpret_cast<int *>(&minResolution), 1, 16);
                ImGui::PopItemWidth();
            }

            ImGui::Separator();

            ImGui::SliderFloat3("Normal 1", &normal1[0], -1, 1, "%.2f");
            ImGui::SliderFloat3("Normal 2", &normal2[0], -1, 1, "%.2f");
            ImGui::SliderFloat("Normal Perturbation", &normalPerturbation, 0, 1, "%.2f");

            ImGui::SliderInt2("Resolution MN", reinterpret_cast<int *>(&MN), 3, 64);

            if (elementType < 9) {
                ImGui::SliderFloat("Minor Radius", &minorRadius, 0.01f, 10, "%.2f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Major Radius", &majorRadius, 0.01f, 10, "%.2f", ImGuiSliderFlags_Logarithmic);
            }

            if (elementType >= 9) {
                ImGui::Text("Nx: %d, Ny: %d", Nx, Ny);
                ImGui::Text("MinLutExtent: (%.1f, %.1f, %.1f)", minLutExtent[0], minLutExtent[1], minLutExtent[2]);
                ImGui::Text("MaxLutExtent: (%.1f, %.1f, %.1f)", maxLutExtent[0], maxLutExtent[1], maxLutExtent[2]);
                ImGui::Checkbox("Cyclic U", &cyclicU);
                ImGui::Checkbox("Cyclic V", &cyclicV);
                if (elementType == 10)
                    ImGui::SliderInt("Degree", reinterpret_cast<int *>(&degree), 1, 3);
            }

            ImGui::PopItemWidth();
            ImGui::Separator();
        }
    }
#endif
}UBOName(resurfacingUbo);
#endif

#ifdef PEBBLE_PIPELINE
UBOStruct(scalar, PerObjectSet, U_configBinding) PebbleUBO {
    uint subdivisionLevel UBODefaultVal(3);
    uint subdivOffset UBODefaultVal(0);
    float extrusionAmount UBODefaultVal(0.1f);
    float extrusionVariation UBODefaultVal(0.5f);
    float roundness UBODefaultVal(2.0f);
    uint normalCalculationMethod UBODefaultVal(1);

    float fillradius UBODefaultVal(0.f);
    float ringoffset UBODefaultVal(0.3f);

    BOOL useLod UBODefaultVal(false);
    float lodFactor UBODefaultVal(1.f);
    BOOL allowLowLod UBODefaultVal(false);
    uint BoundingBoxType UBODefaultVal(0);

    BOOL useCulling UBODefaultVal(false);
    float cullingThreshold UBODefaultVal(0.1f);

    float time UBODefaultVal(0.f);

    BOOL enableRotation UBODefaultVal(false);
    float rotationSpeed UBODefaultVal(0.1f);
    float scalingThreshold UBODefaultVal(0.1f);

    BOOL doNoise UBODefaultVal(false);
    float noiseAmplitude UBODefaultVal(0.01f);
    float noiseFrequency UBODefaultVal(50.0f);
    float normalOffset UBODefaultVal(0.2f);
#ifdef __cplusplus
    void displayUI(std::string meshName = "") {
        if (ImGui::CollapsingHeader(("Pebble UBO " + meshName).c_str())) {
            if (ImGui::SliderInt("Subdivision Level", (int *)&subdivisionLevel, 0, 8)) { subdivOffset = glm::min(subdivOffset, subdivisionLevel); }

            ImGui::SliderInt("Sudivision Offset", (int *)&subdivOffset, 0, glm::min(subdivisionLevel, 3u));
            ImGui::SliderFloat("Extrusion", &extrusionAmount, 0, 1, "%.3f");
            ImGui::SliderFloat("Variation", &extrusionVariation, 0, 1, "%.3f");
            ImGui::SliderFloat("Roundness", &roundness, 0, 2, "%.2f");

            // if (ImGui::CollapsingHeader("Top Face")) {
            //     ImGui::SliderFloat("Fill Radius", &fillradius, 0, 1, "%.3f");
            //     ImGui::SliderFloat("Rings offset", &ringoffset, 0, 1, "%.3f");
            // }

            ImGui::Separator();

            // ImGui::DragFloat("Time", &time, 0.01f, 0, 1000, "%.3f");

            ImGui::Checkbox("Enable Demo", &enableRotation);
            if (enableRotation) {
                // ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0, 10, "%.3f");
                // ImGui::SliderFloat("Scaling Threshold", &scalingThreshold, 0, 1, "%.3f");
            }

            ImGui::Checkbox("Do Noise", &doNoise);
            if (doNoise) {
                ImGui::SliderFloat("Noise Amplitude", &noiseAmplitude, 0, 1, "%.3f", ImGuiSliderFlags_Logarithmic);
                ImGui::SliderFloat("Noise Frequency", &noiseFrequency, 0, 100, "%.3f");
                // ImGui::SliderFloat("Normal Offset", &normalOffset, 0, 1, "%.3f");
            }

            ImGui::Separator();

            ImGui::Checkbox("Culling", &useCulling);
            if (useCulling) {
                ImGui::SameLine();
                ImGui::SliderFloat("Threshold", &cullingThreshold, 0, 1, "%.2f");
            }

            ImGui::Checkbox("Use Lod", &useLod);
            if (useLod) {
                ImGui::SliderInt("BoundingBoxType", (int *)&BoundingBoxType, 0, 3);
                ImGui::SliderFloat("LodFactor", &lodFactor, 0, 10, "%.3f");
                ImGui::Checkbox("Allow Low Lod", &allowLowLod);
            }
        }
    }
#endif
}UBOName(pebbleUbo);
#endif

#ifdef __cplusplus
static vk::DescriptorSetLayout getDescriptorSetLayoutInfo(const int p_set, const vk::Device &p_logicalDevice) {
    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    std::vector<vk::DescriptorBindingFlags> bindingFlags;
    switch (p_set) {
    case SceneSet: {
        bindings = {
            {U_viewBinding, vk::DescriptorType::eUniformBuffer, 1, trueAllGraphics},
            {U_globalShadingBinding, vk::DescriptorType::eUniformBuffer, 1, trueAllGraphics},
        };
        bindingFlags = {
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind},
        };
        break;
    }

    case HESet: {
        bindings = {
            {B_heVec4TypeBinding, vk::DescriptorType::eStorageBuffer, vec4DataCount, trueAllGraphics},
            {B_heVec2TypeBinding, vk::DescriptorType::eStorageBuffer, vec2DataCount, trueAllGraphics},
            {B_heIntTypeBinding, vk::DescriptorType::eStorageBuffer, intDataCount, trueAllGraphics},
            {B_heFloatTypeBinding, vk::DescriptorType::eStorageBuffer, floatDataCount, trueAllGraphics},
        };
        bindingFlags = {
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind},
        };
        break;
    }
    case PerObjectSet: {
        bindings = {
            {U_configBinding, vk::DescriptorType::eUniformBuffer, 1, trueAllGraphics},
            {U_shadingBinding, vk::DescriptorType::eUniformBuffer, 1, trueAllGraphics},
            {B_lutVertexBufferBinding, vk::DescriptorType::eStorageBuffer, 1, trueAllGraphics},
            {B_skinJointsIndicesBinding, vk::DescriptorType::eStorageBuffer, 1, trueAllGraphics},
            {B_skinJointsWeightsBinding, vk::DescriptorType::eStorageBuffer, 1, trueAllGraphics},
            {B_skinBoneMatricesBinding, vk::DescriptorType::eStorageBuffer, 1, trueAllGraphics},
            {S_samplersBinding, vk::DescriptorType::eSampler, samplerCount, trueAllGraphics},
            {T_texturesBinding, vk::DescriptorType::eSampledImage, textureCount, trueAllGraphics},
        };
        bindingFlags = {
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound},
            {vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound},
        };
        break;
    }
    default: ASSERT(false, "Invalid set");
    }
    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo;
    bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    bindingFlagsInfo.pBindingFlags = bindingFlags.data();
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    layoutInfo.pNext = &bindingFlagsInfo;
    layoutInfo.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
    vk::DescriptorSetLayout res{};
    VK_CHECK(p_logicalDevice.createDescriptorSetLayout(&layoutInfo, nullptr, &res));
    return res;
}
}
#endif


#endif