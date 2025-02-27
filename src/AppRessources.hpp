#pragma once
#include "loaders/GLTFLoader.hpp"
#include "HalfEdge.hpp"
#include "renderer.hpp"
#include "shaderInterface.h"
#include "vkHelper.hpp"

struct HeBufferDescSOA {
    Buffer heVertexPositionBuffer;
    Buffer heVertexColorBuffer;
    Buffer heVertexNormalBuffer;
    Buffer heVertexTexcoordBuffer;
    Buffer heVertexEdgeBuffer;
    Buffer heFaceEdgeBuffer;
    Buffer heFaceVertCountBuffer;
    Buffer heFaceOffsetBuffer;
    Buffer heFaceNormalBuffer;
    Buffer heFaceCenterBuffer;
    Buffer heFaceAreaBuffer;
    Buffer heHalfEdgeVertexBuffer;
    Buffer heHalfEdgeFaceBuffer;
    Buffer heHalfEdgeNextBuffer;
    Buffer heHalfEdgePrevBuffer;
    Buffer heHalfEdgeTwinBuffer;
    Buffer vertexFaceIndexBuffer;

    void uploadBuffersToGPU(const HalfEdgeMesh &meshData, Renderer &renderer, vk::CommandBuffer cmd) {
        // helper lambda to upload buffer
        auto uploadBuffer = [&renderer, &cmd](Buffer &destBuffer, const auto &data) { destBuffer = renderer.createAndUploadBuffer(cmd, data, vk::BufferUsageFlagBits::eStorageBuffer); };

        // Upload vertex-related buffers
        uploadBuffer(heVertexPositionBuffer, meshData.vertices.positions);
        uploadBuffer(heVertexColorBuffer, meshData.vertices.colors);
        uploadBuffer(heVertexNormalBuffer, meshData.vertices.normals);
        uploadBuffer(heVertexTexcoordBuffer, meshData.vertices.texCoords);
        uploadBuffer(heVertexEdgeBuffer, meshData.vertices.edges);

        // Upload face-related buffers
        uploadBuffer(heFaceEdgeBuffer, meshData.faces.edges);
        uploadBuffer(heFaceVertCountBuffer, meshData.faces.vertCounts);
        uploadBuffer(heFaceOffsetBuffer, meshData.faces.offsets);
        uploadBuffer(heFaceNormalBuffer, meshData.faces.normals);
        uploadBuffer(heFaceCenterBuffer, meshData.faces.centers);
        uploadBuffer(heFaceAreaBuffer, meshData.faces.faceAreas);

        // Upload half-edge-related buffers
        uploadBuffer(heHalfEdgeVertexBuffer, meshData.halfEdges.vertices);
        uploadBuffer(heHalfEdgeFaceBuffer, meshData.halfEdges.faces);
        uploadBuffer(heHalfEdgeNextBuffer, meshData.halfEdges.next);
        uploadBuffer(heHalfEdgePrevBuffer, meshData.halfEdges.prev);
        uploadBuffer(heHalfEdgeTwinBuffer, meshData.halfEdges.twins);

        // Upload vertex-face index buffer
        uploadBuffer(vertexFaceIndexBuffer, meshData.vertexFaceIndices);
    }
};

struct MeshData {
    enum RenderMode {
        HALF_EDGE = 0,
        PARAMETRIC = 1,
        PEBBLE = 2,
    };

    // === Mesh Data ===
    HeBufferDescSOA heMeshDescSoa;
    HalfEdgeMesh heMesh;
    mat4 modelMatrix = mat4(1.0f);

    // === Skeletal Data ===
    Skeleton skeleton;
    std::vector<Animation> animations;
    std::string name;
    std::vector<vec4> jointIndicesData;
    std::vector<vec4> jointWeightsData;
    uint32 boneMatCount = 0;
    Buffer jointsIndices;
    Buffer jointsWeights;
    Buffer boneMats;

    Buffer lutVertexBuffer;
    SampledTexture aoTexture;
    SampledTexture elementTypeTexture;

    vk::DescriptorSet heDescriptorSet;
    vk::DescriptorSet perObjectDescriptorSet;
    vk::DescriptorSetLayout heDescriptorSetLayout;
    vk::DescriptorSetLayout perObjectDescriptorSetLayout;

    RenderMode renderMode = RenderMode::PARAMETRIC;
    bool isSkeletal = false;
    bool renderBaseMesh = false;
    bool hasAOTexture = false;
    bool hasElementTypeTexture = false;
    bool hasLut = false;

    void init(Renderer &renderer, const std::string &modelPath, const std::string &meshName, const std::string &gltfPath = "");
    LutData loadLut(const std::string &path, Renderer &renderer, vk::CommandBuffer cmd);
    void loadAOTexture(const std::string &path, Renderer &renderer, vk::CommandBuffer cmd) { aoTexture = loadAndUploadTexture(path, renderer, cmd, hasAOTexture); }
    void loadElementTypeTexture(const std::string &path, Renderer &renderer, vk::CommandBuffer cmd) { elementTypeTexture = loadAndUploadTexture(path, renderer, cmd, hasElementTypeTexture); }

protected:
    void allocateDescriptorSets(Renderer &renderer);
    void primeDescriptorSets(Renderer &renderer);
    SampledTexture loadAndUploadTexture(const std::string &path, Renderer &renderer, vk::CommandBuffer cmd, bool &flag);
};

struct Dragon : MeshData {
    shaderInterface::ShadingUBO shadingUBOData;
    shaderInterface::ShadingUBO shadingUBODataBaseMesh;
    shaderInterface::HeUBO heUBOData;
    shaderInterface::ResurfacingUBO resurfacingUBOData;

    UniformBuffer shadingUBO; // this should be a dynamic ubo
    UniformBuffer shadingUBOBaseMesh;
    UniformBuffer heUBO;
    UniformBuffer resurfacingUBO;
    Buffer boneMatStagingBuffer;

    vk::DescriptorSet perObjectDescriptorSetBaseMesh;

    void init(Renderer &renderer, const std::string &modelPath, const std::string &meshName,
              const std::string &gltfPath, const std::string &lutPath, const std::string &aoPath,
              const std::string &elementTypePath);

    void bindAndDispatch(vk::CommandBuffer &cmd, const vk::PipelineLayout &layout);
    void bindAndDispatchBaseMesh(vk::CommandBuffer &cmd, const vk::PipelineLayout &layout);
    void updateUBOs();
    void displayUI();
    void animate(float currentTime, Renderer &renderer);

    void cleanup() {
        // Implement resource cleanup as necessary
    }
};