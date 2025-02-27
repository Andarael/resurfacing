#include "AppRessources.hpp"
#include <stb_image.h>

void MeshData::init(Renderer &renderer, const std::string &modelPath, const std::string &meshName, const std::string &gltfPath) {
    isSkeletal = !gltfPath.empty();
    name = meshName;

    vk::CommandBuffer cmdBuffer = beginSingleTimeCommands(renderer.m_logicalDevice, renderer.m_transientCommandPool);

    // Load NGon mesh data
    NgonLoader loader;
    NGonDataWBones data = loader.loadNgonData(modelPath);

    if (isSkeletal) {
        data.jointIndices.resize(data.vertices.size());
        data.jointWeights.resize(data.vertices.size());
        GLTFLoader gltfLoader;
        tinygltf::Model model = gltfLoader.loadGltfModel(gltfPath);

        extractSkeleton(model, skeleton);
        extractAnimations(model, skeleton, animations);
        updateNgonMeshWithBoneData(model, data);

        std::vector<mat4> boneMatrices;
        computeBoneMatrices(skeleton, boneMatrices);
        boneMatCount = static_cast<uint32>(boneMatrices.size());
        jointsIndices = renderer.createAndUploadBuffer(cmdBuffer, data.jointIndices, vk::BufferUsageFlagBits::eStorageBuffer);
        jointsWeights = renderer.createAndUploadBuffer(cmdBuffer, data.jointWeights, vk::BufferUsageFlagBits::eStorageBuffer);
        boneMats = renderer.createAndUploadBuffer(cmdBuffer, boneMatrices, vk::BufferUsageFlagBits::eStorageBuffer);
        jointIndicesData = data.jointIndices;
        jointWeightsData = data.jointWeights;
    }

    heMesh = convertToHalfEdgeMesh(data);
    heMeshDescSoa.uploadBuffersToGPU(heMesh, renderer, cmdBuffer);
    endSingleTimeCommands(cmdBuffer, renderer.m_logicalDevice, renderer.m_transientCommandPool, renderer.m_graphicsQueue);

    // Setup and update descriptor sets
    heDescriptorSetLayout = shaderInterface::getDescriptorSetLayoutInfo(shaderInterface::HESet, renderer.m_logicalDevice);
    perObjectDescriptorSetLayout = shaderInterface::getDescriptorSetLayoutInfo(shaderInterface::PerObjectSet, renderer.m_logicalDevice);
        
    allocateDescriptorSets(renderer);
    primeDescriptorSets(renderer);
}

void MeshData::allocateDescriptorSets(Renderer &renderer) {
    std::array<vk::DescriptorSetLayout, 2> layouts = { heDescriptorSetLayout, perObjectDescriptorSetLayout };
    vk::DescriptorSetAllocateInfo allocInfo(renderer.m_descriptorPool, static_cast<uint32_t>(layouts.size()), layouts.data());
    std::array<vk::DescriptorSet, 2> descriptorSets;
    VK_CHECK(renderer.m_logicalDevice.allocateDescriptorSets(&allocInfo, descriptorSets.data()));
    heDescriptorSet = descriptorSets[0];
    perObjectDescriptorSet = descriptorSets[1];
}

void MeshData::primeDescriptorSets(Renderer &renderer) {
    // Prepare buffer infos for the heDescriptorSet
    const std::array<vk::DescriptorBufferInfo, shaderInterface::vec4DataCount> heDescriptorBufferInfosVec4 = {
        vk::DescriptorBufferInfo(heMeshDescSoa.heVertexPositionBuffer.buffer, 0, VK_WHOLE_SIZE),
        vk::DescriptorBufferInfo(heMeshDescSoa.heVertexColorBuffer.buffer, 0, VK_WHOLE_SIZE),
        vk::DescriptorBufferInfo(heMeshDescSoa.heVertexNormalBuffer.buffer, 0, VK_WHOLE_SIZE),
        vk::DescriptorBufferInfo(heMeshDescSoa.heFaceNormalBuffer.buffer, 0, VK_WHOLE_SIZE),
        vk::DescriptorBufferInfo(heMeshDescSoa.heFaceCenterBuffer.buffer, 0, VK_WHOLE_SIZE)
    };

    const std::array<vk::DescriptorBufferInfo, shaderInterface::vec2DataCount> heDescriptorBufferInfosVec2 = {
        vk::DescriptorBufferInfo(heMeshDescSoa.heVertexTexcoordBuffer.buffer, 0, VK_WHOLE_SIZE)
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
        vk::DescriptorBufferInfo(heMeshDescSoa.vertexFaceIndexBuffer.buffer, 0, VK_WHOLE_SIZE)
    };

    const std::array<vk::DescriptorBufferInfo, shaderInterface::floatDataCount> heDescriptorBufferInfosFloat = {
        vk::DescriptorBufferInfo(heMeshDescSoa.heFaceAreaBuffer.buffer, 0, VK_WHOLE_SIZE)
    };

    std::vector<vk::DescriptorBufferInfo> skinBufferInfos = {
        vk::DescriptorBufferInfo(jointsIndices.buffer, 0, VK_WHOLE_SIZE),
        vk::DescriptorBufferInfo(jointsWeights.buffer, 0, VK_WHOLE_SIZE),
        vk::DescriptorBufferInfo(boneMats.buffer, 0, VK_WHOLE_SIZE)
    };

    std::vector<vk::WriteDescriptorSet> descriptorWrites = {
        { heDescriptorSet, shaderInterface::B_heVec4TypeBinding, 0, shaderInterface::vec4DataCount,
          vk::DescriptorType::eStorageBuffer, nullptr, heDescriptorBufferInfosVec4.data(), nullptr },
        { heDescriptorSet, shaderInterface::B_heVec2TypeBinding, 0, shaderInterface::vec2DataCount,
          vk::DescriptorType::eStorageBuffer, nullptr, heDescriptorBufferInfosVec2.data(), nullptr },
        { heDescriptorSet, shaderInterface::B_heIntTypeBinding, 0, shaderInterface::intDataCount,
          vk::DescriptorType::eStorageBuffer, nullptr, heDescriptorBufferInfosInt.data(), nullptr },
        { heDescriptorSet, shaderInterface::B_heFloatTypeBinding, 0, shaderInterface::floatDataCount,
          vk::DescriptorType::eStorageBuffer, nullptr, heDescriptorBufferInfosFloat.data(), nullptr },
        { perObjectDescriptorSet, shaderInterface::B_skinJointsIndicesBinding, 0, 1,
          vk::DescriptorType::eStorageBuffer, nullptr, skinBufferInfos.data(), nullptr },
        { perObjectDescriptorSet, shaderInterface::B_skinJointsWeightsBinding, 0, 1,
          vk::DescriptorType::eStorageBuffer, nullptr, skinBufferInfos.data() + 1, nullptr },
        { perObjectDescriptorSet, shaderInterface::B_skinBoneMatricesBinding, 0, 1,
          vk::DescriptorType::eStorageBuffer, nullptr, skinBufferInfos.data() + 2, nullptr }
    };

    renderer.m_logicalDevice.updateDescriptorSets(descriptorWrites, nullptr);
}

void MeshData::loadLut(const std::string &path, Renderer &renderer, vk::CommandBuffer cmd) {
    if (lutVertexBuffer.buffer) {
        renderer.m_logicalDevice.destroyBuffer(lutVertexBuffer.buffer);
        renderer.m_logicalDevice.freeMemory(lutVertexBuffer.memory);
    }
        
    LutData lutData = LutLoader::loadLutData(path);
    lutVertexBuffer = renderer.createAndUploadBuffer(cmd, lutData.positions, vk::BufferUsageFlagBits::eStorageBuffer);
    hasLut = true;
        
    vk::DescriptorBufferInfo bufferInfo(lutVertexBuffer.buffer, 0, VK_WHOLE_SIZE);
    vk::WriteDescriptorSet descriptorWrite(perObjectDescriptorSet, shaderInterface::B_lutVertexBufferBinding,
                                           0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &bufferInfo, nullptr);
    renderer.m_logicalDevice.updateDescriptorSets(descriptorWrite, nullptr);
}

SampledTexture MeshData::loadAndUploadTexture(const std::string &path, Renderer &renderer, vk::CommandBuffer cmd, bool &flag) {
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!pixels) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        flag = false;
        return SampledTexture{};
    }
    std::vector<stbi_uc> pixelsVec(pixels, pixels + (texWidth * texHeight * sizeof(uint32)));
    SampledTexture texture = renderer.createAndUploadTexture(cmd, pixelsVec, uvec2(texWidth, texHeight), vk::Format::eR8G8B8A8Srgb);
    flag = true;
    stbi_image_free(pixels);
    return texture;
}

void Dragon::init(Renderer &renderer, const std::string &modelPath, const std::string &meshName, const std::string &gltfPath, const std::string &lutPath, const std::string &aoPath, const std::string &elementTypePath) {
    MeshData::init(renderer, modelPath, meshName, gltfPath);

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

    // Allocate extra descriptor set for the base mesh
    std::array<vk::DescriptorSetLayout, 1> layouts = { perObjectDescriptorSetLayout };
    vk::DescriptorSetAllocateInfo allocInfo(renderer.m_descriptorPool, static_cast<uint32_t>(layouts.size()), layouts.data());
    std::array<vk::DescriptorSet, 1> descriptorSets;
    VK_CHECK(renderer.m_logicalDevice.allocateDescriptorSets(&allocInfo, descriptorSets.data()));
    perObjectDescriptorSetBaseMesh = descriptorSets[0];

    vk::CommandBuffer cmdBuffer = beginSingleTimeCommands(renderer.m_logicalDevice, renderer.m_transientCommandPool);
    loadLut(lutPath, renderer, cmdBuffer);
    loadAOTexture(aoPath, renderer, cmdBuffer);
    loadElementTypeTexture(elementTypePath, renderer, cmdBuffer);
    endSingleTimeCommands(cmdBuffer, renderer.m_logicalDevice, renderer.m_transientCommandPool, renderer.m_graphicsQueue);

    shadingUBO = renderer.createUniformBuffer(sizeof(shaderInterface::ShadingUBO));
    heUBO = renderer.createUniformBuffer(sizeof(shaderInterface::HeUBO));
    resurfacingUBO = renderer.createUniformBuffer(sizeof(shaderInterface::ResurfacingUBO));
    boneMatStagingBuffer = renderer.createStagingBuffer(sizeof(mat4) * boneMatCount);

    // Update descriptor sets for uniform buffers (base mesh)
    vk::DescriptorBufferInfo shadingUBOBufferInfo(shadingUBO.buffer, 0, VK_WHOLE_SIZE);
    vk::DescriptorBufferInfo heUBOBufferInfo(heUBO.buffer, 0, VK_WHOLE_SIZE);
    std::vector<vk::WriteDescriptorSet> heUBOWrite = {
        { perObjectDescriptorSetBaseMesh, shaderInterface::U_configBinding, 0, 1,
          vk::DescriptorType::eUniformBuffer, nullptr, &heUBOBufferInfo, nullptr },
        { perObjectDescriptorSetBaseMesh, shaderInterface::U_shadingBinding, 0, 1,
          vk::DescriptorType::eUniformBuffer, nullptr, &shadingUBOBufferInfo, nullptr }
    };
    renderer.m_logicalDevice.updateDescriptorSets(heUBOWrite, nullptr);

    vk::DescriptorBufferInfo resurfacingUBOBufferInfo(resurfacingUBO.buffer, 0, VK_WHOLE_SIZE);
    std::vector<vk::WriteDescriptorSet> resurfacingUBOWrite = {
        { perObjectDescriptorSet, shaderInterface::U_configBinding, 0, 1,
          vk::DescriptorType::eUniformBuffer, nullptr, &resurfacingUBOBufferInfo, nullptr },
        { perObjectDescriptorSet, shaderInterface::U_shadingBinding, 0, 1,
          vk::DescriptorType::eUniformBuffer, nullptr, &shadingUBOBufferInfo, nullptr }
    };
    renderer.m_logicalDevice.updateDescriptorSets(resurfacingUBOWrite, nullptr);

    heUBOData.nbFaces = heMesh.nbFaces;
    resurfacingUBOData.nbFaces = heMesh.nbFaces;
    resurfacingUBOData.nbVertices = heMesh.nbVertices;
    heUBOData.doSkinning = false;
    resurfacingUBOData.doSkinning = false;
    updateUBOs();
}

void Dragon::bindAndDispatch(vk::CommandBuffer &cmd, const vk::PipelineLayout &layout) {
    std::array<vk::DescriptorSet, 2> sets = { heDescriptorSet, perObjectDescriptorSet };
    cmd.pushConstants(layout, trueAllGraphics, 0, sizeof(mat4), &modelMatrix);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, shaderInterface::HESet, sets, {});
    cmd.drawMeshTasksEXT(heMesh.nbFaces + heMesh.nbVertices, 1, 1);
}

void Dragon::bindAndDispatchBaseMesh(vk::CommandBuffer &cmd, const vk::PipelineLayout &layout) {
    std::array<vk::DescriptorSet, 2> sets = { heDescriptorSet, perObjectDescriptorSetBaseMesh };
    cmd.pushConstants(layout, trueAllGraphics, 0, sizeof(mat4), &modelMatrix);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, shaderInterface::HESet, sets, {});
    cmd.drawMeshTasksEXT(heMesh.nbFaces, 1, 1);
}

void Dragon::updateUBOs() {
    memcpy(shadingUBO.mappedMemory, &m_shadingUBOData, sizeof(shaderInterface::ShadingUBO));
    memcpy(heUBO.mappedMemory, &heUBOData, sizeof(shaderInterface::HeUBO));
    memcpy(resurfacingUBO.mappedMemory, &resurfacingUBOData, sizeof(shaderInterface::ResurfacingUBO));
}

void Dragon::animate(float currentTime, Renderer &renderer) {
    if (!animations.empty()) {
        updateSkeleton(animations[0], currentTime, skeleton);
        std::vector<mat4> boneMatrices;
        computeBoneMatrices(skeleton, boneMatrices);
        vk::CommandBuffer cmd = beginSingleTimeCommands(renderer.m_logicalDevice, renderer.m_transientCommandPool);
        renderer.uploadToBuffer(boneMatStagingBuffer, boneMats, cmd, boneMatrices);
        endSingleTimeCommands(cmd, renderer.m_logicalDevice, renderer.m_transientCommandPool, renderer.m_graphicsQueue);
    }
}