#pragma once
#include "GLTFLoader.hpp"
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

    void uploadBuffersToGPU(const HalfEdgeMesh &p_meshData, renderer &p_renderer, vk::CommandBuffer cmd) {
        heVertexPositionBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.vertices.positions, vk::BufferUsageFlagBits::eStorageBuffer);
        heVertexColorBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.vertices.colors, vk::BufferUsageFlagBits::eStorageBuffer);
        heVertexNormalBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.vertices.normals, vk::BufferUsageFlagBits::eStorageBuffer);
        heVertexTexcoordBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.vertices.texCoords, vk::BufferUsageFlagBits::eStorageBuffer);
        heVertexEdgeBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.vertices.edges, vk::BufferUsageFlagBits::eStorageBuffer);

        heFaceEdgeBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.faces.edges, vk::BufferUsageFlagBits::eStorageBuffer);
        heFaceVertCountBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.faces.vertCounts, vk::BufferUsageFlagBits::eStorageBuffer);
        heFaceOffsetBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.faces.offsets, vk::BufferUsageFlagBits::eStorageBuffer);
        heFaceNormalBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.faces.normals, vk::BufferUsageFlagBits::eStorageBuffer);
        heFaceCenterBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.faces.centers, vk::BufferUsageFlagBits::eStorageBuffer);
        heFaceAreaBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.faces.faceAreas, vk::BufferUsageFlagBits::eStorageBuffer);

        heHalfEdgeVertexBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.halfEdges.vertices, vk::BufferUsageFlagBits::eStorageBuffer);
        heHalfEdgeFaceBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.halfEdges.faces, vk::BufferUsageFlagBits::eStorageBuffer);
        heHalfEdgeNextBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.halfEdges.next, vk::BufferUsageFlagBits::eStorageBuffer);
        heHalfEdgePrevBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.halfEdges.prev, vk::BufferUsageFlagBits::eStorageBuffer);
        heHalfEdgeTwinBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.halfEdges.twins, vk::BufferUsageFlagBits::eStorageBuffer);

        vertexFaceIndexBuffer = p_renderer.createAndUploadBuffer(cmd, p_meshData.vertexFaceIndices, vk::BufferUsageFlagBits::eStorageBuffer);
    }
};

struct MeshData {
    enum RenderMode {
        HALF_EDGE = 0,
        PARAMETRIC = 1,
        PEBBLE = 2,
        DEBUG = 3,
    };

    // === Mesh Data ===

    HeBufferDescSOA heMeshDescSoa;
    HalfEdgeMesh heMesh;
    mat4 modelMatrix = mat4(1.0f);

    std::vector<vec4> jointIndices;
    std::vector<vec4> jointWeights;
    Skeleton skeleton;
    std::vector<Animation> animations;
    std::string name;
    Buffer jointsIndices;
    Buffer jointsWeights;
    Buffer BoneMats;

    Buffer lutVertexBuffer;

    SampledTexture aoTexture;
    SampledTexture elementTypeTexture;
    
    vk::DescriptorSet heDescriptorSet;
    vk::DescriptorSet otherDescriptorSet;
    vk::DescriptorSetLayout heDescriptorSetLayout;
    vk::DescriptorSetLayout otherDescriptorSetLayout;
    
    RenderMode renderMode = RenderMode::PARAMETRIC;
    bool isSkeletal = false;
    bool renderBaseMesh = false;
    bool hasAOTexture = false;
    bool hasElementTypeTexture = false;
    bool hasLut = false;

    void init(renderer &p_renderer, std::string p_modelPath, std::string p_name,std::string p_gltfPath = "") {
        isSkeletal = p_gltfPath != "";
        name = p_name;

        vk::CommandBuffer transientCmd = beginSingleTimeCommands(p_renderer.m_logicalDevice, p_renderer.m_transientCommandPool);

        NgonLoader loader;
        NGonDataWBones data = loader.loadNgonData(p_modelPath);
        if (isSkeletal) {
            tinygltf::Model model;
            loadGltfModel(p_gltfPath, model);
            updateNgonMeshWithBoneData(model, data);
            jointIndices = std::vector(data.jointIndices);
            jointWeights = std::vector(data.jointWeights);

            extractSkeleton(model, skeleton);
            extractAnimations(model, skeleton, animations);

            std::vector<mat4> boneMatrices;
            computeBoneMatrices(skeleton, boneMatrices);
            jointsIndices = p_renderer.createAndUploadBuffer(transientCmd, jointIndices, vk::BufferUsageFlagBits::eStorageBuffer);
            jointsWeights = p_renderer.createAndUploadBuffer(transientCmd, jointWeights, vk::BufferUsageFlagBits::eStorageBuffer);
            BoneMats = p_renderer.createAndUploadBuffer(transientCmd, boneMatrices, vk::BufferUsageFlagBits::eStorageBuffer);
        }
        heMesh = convertToHalfEdgeMesh(data);
        heMeshDescSoa.uploadBuffersToGPU(heMesh, p_renderer, transientCmd);
        heDescriptorSetLayout = shaderInterface::getDescriptorSetLayoutInfo(shaderInterface::HESet, p_renderer.m_logicalDevice);
        otherDescriptorSetLayout = shaderInterface::getDescriptorSetLayoutInfo(shaderInterface::OtherSet, p_renderer.m_logicalDevice);
        // allocate descriptor sets
        std::array<vk::DescriptorSetLayout, 2> layouts = {heDescriptorSetLayout, otherDescriptorSetLayout};
        vk::DescriptorSetAllocateInfo allocInfo(p_renderer.m_descriptorPool, layouts.size(), layouts.data());
        std::array<vk::DescriptorSet, 2> descriptorSets;
        VK_CHECK(p_renderer.m_logicalDevice.allocateDescriptorSets(&allocInfo, descriptorSets.data()));
        heDescriptorSet = descriptorSets[0];
        otherDescriptorSet = descriptorSets[1];
        
        // update descriptor sets
        const std::array<vk::DescriptorBufferInfo, shaderInterface::vec4DataCount> heDescriptorBufferInfosVec4 = {
            vk::DescriptorBufferInfo(heMeshDescSoa.heVertexPositionBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heVertexColorBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heVertexNormalBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heFaceNormalBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heFaceCenterBuffer.buffer, 0, VK_WHOLE_SIZE)
        };

        const std::array<vk::DescriptorBufferInfo, shaderInterface::vec2DataCount> heDescriptorBufferInfosVec2 = {
            vk::DescriptorBufferInfo(heMeshDescSoa.heVertexTexcoordBuffer.buffer, 0, VK_WHOLE_SIZE),
        };

        const std::array<vk::DescriptorBufferInfo, shaderInterface::intDataCount> heDescriptorBufferInfosInt = {
            vk::DescriptorBufferInfo(heMeshDescSoa.heVertexEdgeBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heFaceEdgeBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heFaceVertCountBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heFaceOffsetBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heHalfEdgeVertexBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heHalfEdgeFaceBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heHalfEdgeNextBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heHalfEdgePrevBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.heHalfEdgeTwinBuffer.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(heMeshDescSoa.vertexFaceIndexBuffer.buffer, 0, VK_WHOLE_SIZE),
        };

        const std::array<vk::DescriptorBufferInfo, shaderInterface::floatDataCount> heDescriptorBufferInfosFloat = {
            vk::DescriptorBufferInfo(heMeshDescSoa.heFaceAreaBuffer.buffer, 0, VK_WHOLE_SIZE),
        };

        std::vector<vk::DescriptorBufferInfo> bufferInfos = {
            vk::DescriptorBufferInfo(jointsIndices.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(jointsWeights.buffer, 0, VK_WHOLE_SIZE),
            vk::DescriptorBufferInfo(BoneMats.buffer, 0, VK_WHOLE_SIZE)
        };

        std::vector<vk::WriteDescriptorSet> descriptorWrites = {
            {heDescriptorSet, shaderInterface::B_heVec4TypeBinding, 0, shaderInterface::vec4DataCount, vk::DescriptorType::eStorageBuffer, nullptr, heDescriptorBufferInfosVec4.data(), nullptr},
            {heDescriptorSet, shaderInterface::B_heVec2TypeBinding, 0, shaderInterface::vec2DataCount, vk::DescriptorType::eStorageBuffer, nullptr, heDescriptorBufferInfosVec2.data(), nullptr},
            {heDescriptorSet, shaderInterface::B_heIntTypeBinding, 0, shaderInterface::intDataCount, vk::DescriptorType::eStorageBuffer, nullptr, heDescriptorBufferInfosInt.data(), nullptr},
            {heDescriptorSet, shaderInterface::B_heFloatTypeBinding, 0, shaderInterface::floatDataCount, vk::DescriptorType::eStorageBuffer, nullptr, heDescriptorBufferInfosFloat.data(), nullptr},
            {otherDescriptorSet, shaderInterface::B_skinJointsIndicesBinding, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, bufferInfos.data(), nullptr},
            {otherDescriptorSet, shaderInterface::B_skinJointsWeightsBinding, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, bufferInfos.data() + 1, nullptr},
            {otherDescriptorSet, shaderInterface::B_skinBoneMatricesBinding, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, bufferInfos.data() + 2, nullptr}
        };

        // no particular synchronization needed here (only standard cpu multi-threading synchronization, if using)
        p_renderer.m_logicalDevice.updateDescriptorSets(descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
    }

    // beware descriptor update synchronization is the responsibility of the caller (cannot update while in use by the GPU)
    void loadAOTexture(std::string p_path, renderer &p_renderer, vk::CommandBuffer p_cmd) {
        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load(p_path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels) {
            std::cerr << "Failed to load AO texture file: " << p_path << std::endl;
            return;
        }
        uint32 bufferSize = sizeof(uint32) * texWidth * texHeight; // 4 uchar channels is size of uint (4 x 1byte = 32 bits)
        //p_renderer.createAndUploadImage(p_cmd, jointIndices, vk::BufferUsageFlagBits::eStorageBuffer);
        hasAOTexture = true;
        stbi_image_free(pixels);
    }

    // beware descriptor update synchronization is the responsibility of the caller (cannot update while in use by the GPU)
    void loadElementTypeTexture(std::string p_path, renderer &p_renderer, vk::CommandBuffer p_cmd) {
        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load(p_path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!pixels) {
            std::cerr << "Failed to load ElementType texture file: " << p_path << std::endl;
            return;
        }
        uint32 bufferSize = sizeof(uint32) * texWidth * texHeight; // 4 uchar channels is size of uint (4 x 1byte = 32 bits)
        //p_renderer.createAndUploadImage(p_cmd, jointIndices, vk::BufferUsageFlagBits::eStorageBuffer);
        hasElementTypeTexture = true;
        stbi_image_free(pixels);
    }

    void loadLut(std::string p_path, renderer &p_renderer, vk::CommandBuffer p_cmd) {
        if (lutVertexBuffer.buffer) {
            p_renderer.m_logicalDevice.destroyBuffer(lutVertexBuffer.buffer);
            p_renderer.m_logicalDevice.freeMemory(lutVertexBuffer.memory);
        }
        
        LutData lutData = LutLoader::loadLutData(p_path);
        lutVertexBuffer = p_renderer.createAndUploadBuffer(p_cmd, lutData.positions, vk::BufferUsageFlagBits::eStorageBuffer);
        hasLut = true;
        
        vk::DescriptorBufferInfo bufferInfo = vk::DescriptorBufferInfo(lutVertexBuffer.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet descriptorWrite = {otherDescriptorSet, shaderInterface::B_lutVertexBufferBinding, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &bufferInfo, nullptr};
        p_renderer.m_logicalDevice.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
    }
};

struct MeshRenderInterface {
    virtual ~MeshRenderInterface() = default;
    virtual void render() = 0;
};


struct Dragon : MeshData, MeshRenderInterface {
    shaderInterface::HeUBO heUBOData;
    shaderInterface::ResurfacingUBO resurfacingUBOData;
    UniformBuffer heUBO;
    UniformBuffer resurfacingUBO;

    vk::DescriptorSet otherDescriptorSetBaseMesh;

    void init(renderer &p_renderer, std::string p_modelPath, std::string p_name, std::string p_gltfPath, std::string p_lutPath, std::string p_aoPath, std::string p_elementTypePath) {
        MeshData::init(p_renderer, p_modelPath, p_name, p_gltfPath);

        renderMode = MeshData::RenderMode::PARAMETRIC;
        renderBaseMesh = true;
        heUBOData.normalOffset = 0.001f;
        resurfacingUBOData.scaling = 0.6f;
        resurfacingUBOData.normal1 = vec3(0, 1, 0.3);
        resurfacingUBOData.normal2 = vec3(0, 1, 0.3);
        resurfacingUBOData.normalPerturbation = 0.2f;
        resurfacingUBOData.MN = uvec2(16, 16);
        resurfacingUBOData.doLod = true;
        resurfacingUBOData.lodFactor = 4.0f;
        resurfacingUBOData.elementType = 9;
        resurfacingUBOData.backfaceCulling = true;
        resurfacingUBOData.cullingThreshold = 0.1f;

        //allocate descriptor sets
        std::array<vk::DescriptorSetLayout, 1> layouts = {otherDescriptorSetLayout};
        vk::DescriptorSetAllocateInfo allocInfo(p_renderer.m_descriptorPool, layouts.size(), layouts.data());
        std::array<vk::DescriptorSet, 1> descriptorSets;
        VK_CHECK(p_renderer.m_logicalDevice.allocateDescriptorSets(&allocInfo, descriptorSets.data()));
        otherDescriptorSetBaseMesh = descriptorSets[0];

        vk::CommandBuffer transientCmd = beginSingleTimeCommands(p_renderer.m_logicalDevice, p_renderer.m_transientCommandPool);
        loadLut(p_lutPath, p_renderer, transientCmd);
        loadAOTexture(p_aoPath, p_renderer, transientCmd);
        loadElementTypeTexture(p_elementTypePath, p_renderer, transientCmd);
        endSingleTimeCommands(transientCmd, p_renderer.m_logicalDevice, p_renderer.m_transientCommandPool, p_renderer.m_graphicsQueue);
        
        heUBO = p_renderer.createUniformBuffer(sizeof(shaderInterface::HeUBO));
        resurfacingUBO = p_renderer.createUniformBuffer(sizeof(shaderInterface::ResurfacingUBO));
        // update descriptor sets
        vk::DescriptorBufferInfo heUBOBufferInfo = vk::DescriptorBufferInfo(heUBO.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet heUBOWrite = {otherDescriptorSetBaseMesh, shaderInterface::U_configBinding, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &heUBOBufferInfo, nullptr};
        p_renderer.m_logicalDevice.updateDescriptorSets(1, &heUBOWrite, 0, nullptr);
        vk::DescriptorBufferInfo resurfacingUBOBufferInfo = vk::DescriptorBufferInfo(resurfacingUBO.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet resurfacingUBOWrite = {otherDescriptorSet, shaderInterface::U_configBinding, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &resurfacingUBOBufferInfo, nullptr};
        p_renderer.m_logicalDevice.updateDescriptorSets(1, &resurfacingUBOWrite, 0, nullptr);
    }
    
};

struct DragonCoat : MeshData, MeshRenderInterface {
    shaderInterface::ResurfacingUBO resurfacingUBOData;
    UniformBuffer resurfacingUBO;
    void init(renderer &p_renderer, std::string p_modelPath, std::string p_name, std::string p_gltfPath, std::string p_aoPath) {
        MeshData::init(p_renderer, p_modelPath, p_name, p_gltfPath);

        vk::CommandBuffer transientCmd = beginSingleTimeCommands(p_renderer.m_logicalDevice, p_renderer.m_transientCommandPool);
        loadAOTexture(p_aoPath, p_renderer, transientCmd);
        endSingleTimeCommands(transientCmd, p_renderer.m_logicalDevice, p_renderer.m_transientCommandPool, p_renderer.m_graphicsQueue);
        
        resurfacingUBO = p_renderer.createUniformBuffer(sizeof(shaderInterface::ResurfacingUBO));
        // update descriptor sets;
        vk::DescriptorBufferInfo resurfacingUBOBufferInfo = vk::DescriptorBufferInfo(resurfacingUBO.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet resurfacingUBOWrite = {otherDescriptorSet, shaderInterface::U_configBinding, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &resurfacingUBOBufferInfo, nullptr};
        p_renderer.m_logicalDevice.updateDescriptorSets(1, &resurfacingUBOWrite, 0, nullptr);
    }
};

struct Ground : MeshData, MeshRenderInterface {
    shaderInterface::PebbleUBO pebbleUBOData;
    UniformBuffer pebbleUBO;
    void init(renderer &p_renderer, std::string p_modelPath, std::string p_name) {
        MeshData::init(p_renderer, p_modelPath, p_name);
        renderMode = MeshData::RenderMode::PEBBLE;
        pebbleUBOData.enableRotation = true;
        pebbleUBOData.subdivisionLevel = 4;
        pebbleUBOData.extrusionAmount = 0.045f;
        pebbleUBOData.roundness = 2.0f;
        pebbleUBOData.doNoise = true;
        pebbleUBOData.noiseAmplitude = 0.01f;
        pebbleUBOData.noiseFrequency = 35.0f;
        
        pebbleUBO = p_renderer.createUniformBuffer(sizeof(shaderInterface::PebbleUBO));
        // update descriptor sets;
        vk::DescriptorBufferInfo resurfacingUBOBufferInfo = vk::DescriptorBufferInfo(pebbleUBO.buffer, 0, VK_WHOLE_SIZE);
        vk::WriteDescriptorSet resurfacingUBOWrite = {otherDescriptorSet, shaderInterface::U_configBinding, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &resurfacingUBOBufferInfo, nullptr};
        p_renderer.m_logicalDevice.updateDescriptorSets(1, &resurfacingUBOWrite, 0, nullptr);
    }
};