#include <stb_image.h>
#include <stb_image_write.h>
#include <tiny_gltf.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include "GLTFLoader.hpp"
#include "defines.hpp"

void extractSkeleton(const tinygltf::Model &model, Skeleton &skeleton) {
    if (model.skins.empty()) {
        std::cerr << "No skins found in the glTF model." << std::endl;
        return;
    }

    const tinygltf::Skin &skin = model.skins[0];

    // Load inverse bind matrices
    std::vector<glm::mat4> inverseBindMatrices;
    if (skin.inverseBindMatrices >= 0) {
        const tinygltf::Accessor &accessor = model.accessors[skin.inverseBindMatrices];
        const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
        const unsigned char *dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

        size_t numMatrices = accessor.count;
        inverseBindMatrices.resize(numMatrices);
        memcpy(inverseBindMatrices.data(), dataPtr, sizeof(glm::mat4) * numMatrices);
    }

    // Build the bone hierarchy
    skeleton.bones.resize(skin.joints.size());
    for (size_t i = 0; i < skin.joints.size(); ++i) {
        int nodeIndex = skin.joints[i];
        const tinygltf::Node &node = model.nodes[nodeIndex];

        Skeleton::Bone &bone = skeleton.bones[i];
        bone.nodeIndex = nodeIndex;
        bone.name = node.name;
        bone.parentIndex = -1; // Default to -1 (no parent)

        // Set inverse bind matrix
        bone.inverseBindMatrix = inverseBindMatrices.empty() ? glm::mat4(1.0f) : inverseBindMatrices[i];

        // Initialize animation data
        bone.animTranslation = getNodeTranslation(node);
        bone.animRotation = getNodeRotation(node);
        bone.animScale = getNodeScale(node);
        bone.localTransform = getNodeTransform(node);

        // Find parent bone
        for (size_t j = 0; j < skin.joints.size(); ++j) {
            const tinygltf::Node &parentNode = model.nodes[skin.joints[j]];
            if (std::find(parentNode.children.begin(), parentNode.children.end(), nodeIndex) != parentNode.children.end()) {
                bone.parentIndex = j;
                skeleton.bones[j].childrenIndices.push_back(i);
                break;
            }
        }
    }
}

void computeGlobalTransform(const Skeleton &skeleton, int boneIndex, std::vector<glm::mat4> &globalTransforms) {
    const Skeleton::Bone &bone = skeleton.bones[boneIndex];

    glm::mat4 localTransform = bone.localTransform;

    if (bone.parentIndex != -1) {
        computeGlobalTransform(skeleton, bone.parentIndex, globalTransforms);
        globalTransforms[boneIndex] = globalTransforms[bone.parentIndex] * localTransform;
    } else { globalTransforms[boneIndex] = localTransform; }
}

void computeBoneMatrices(const Skeleton &skeleton, std::vector<glm::mat4> &boneMatrices) {
    boneMatrices.resize(skeleton.bones.size());
    std::vector<glm::mat4> globalTransforms(skeleton.bones.size());

    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        computeGlobalTransform(skeleton, i, globalTransforms);
        boneMatrices[i] = globalTransforms[i] * skeleton.bones[i].inverseBindMatrix;
    }
}

void extractAnimations(const tinygltf::Model &model, const Skeleton &skeleton, std::vector<Animation> &animations) {
    for (const auto &gltfAnimation : model.animations) {
        Animation animation;
        animation.name = gltfAnimation.name;
        animation.duration = 0.0f;

        for (const auto &channel : gltfAnimation.channels) {
            const tinygltf::AnimationSampler &sampler = gltfAnimation.samplers[channel.sampler];

            // Get interpolation method
            std::string interpolation = sampler.interpolation.empty() ? "LINEAR" : sampler.interpolation;

            // Load keyframe times
            std::vector<float> times;
            {
                const tinygltf::Accessor &accessor = model.accessors[sampler.input];
                const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
                const unsigned char *dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                size_t count = accessor.count;
                times.resize(count);
                memcpy(times.data(), dataPtr, sizeof(float) * count);
            }

            // Load keyframe values
            std::vector<float> values;
            {
                const tinygltf::Accessor &accessor = model.accessors[sampler.output];
                const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];
                const unsigned char *dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                size_t count = accessor.count * tinygltf::GetNumComponentsInType(accessor.type);
                values.resize(count);
                memcpy(values.data(), dataPtr, sizeof(float) * count);
            }

            // Find the corresponding bone
            int boneIndex = -1;
            for (size_t i = 0; i < skeleton.bones.size(); ++i) {
                if (skeleton.bones[i].nodeIndex == channel.target_node) {
                    boneIndex = static_cast<int>(i);
                    break;
                }
            }
            if (boneIndex == -1) continue;

            AnimationChannel animChannel;
            animChannel.boneIndex = boneIndex;
            animChannel.path = channel.target_path;
            animChannel.interpolation = interpolation;

            // Create keyframes
            size_t numKeyframes = times.size();
            size_t valueComponents = 0;
            if (channel.target_path == "translation" || channel.target_path == "scale") { valueComponents = 3; } else if (channel.target_path == "rotation") { valueComponents = 4; } else {
                continue; // Unsupported path
            }

            size_t stride = valueComponents;
            if (interpolation == "CUBICSPLINE") {
                stride = valueComponents * 3; // in-tangent, value, out-tangent
            }

            for (size_t i = 0; i < numKeyframes; ++i) {
                KeyFrame keyframe;
                keyframe.time = times[i];
                if (keyframe.time > animation.duration) { animation.duration = keyframe.time; }

                size_t offset = i * stride;

                if (interpolation == "CUBICSPLINE") {
                    // Extract in-tangent, value, and out-tangent
                    size_t inOffset = offset;
                    size_t valueOffset = offset + valueComponents;
                    size_t outOffset = offset + valueComponents * 2;

                    if (channel.target_path == "translation") {
                        keyframe.inTangentTranslation = glm::make_vec3(&values[inOffset]);
                        keyframe.translation = glm::make_vec3(&values[valueOffset]);
                        keyframe.outTangentTranslation = glm::make_vec3(&values[outOffset]);
                    } else if (channel.target_path == "rotation") {
                        keyframe.inTangentRotation = glm::make_quat(&values[inOffset]);
                        keyframe.rotation = glm::make_quat(&values[valueOffset]);
                        keyframe.outTangentRotation = glm::make_quat(&values[outOffset]);
                    } else if (channel.target_path == "scale") {
                        keyframe.inTangentScale = glm::make_vec3(&values[inOffset]);
                        keyframe.scale = glm::make_vec3(&values[valueOffset]);
                        keyframe.outTangentScale = glm::make_vec3(&values[outOffset]);
                    }
                } else {
                    // LINEAR or STEP interpolation
                    if (channel.target_path == "translation") { keyframe.translation = glm::make_vec3(&values[offset]); } else if (channel.target_path == "rotation") { keyframe.rotation = glm::make_quat(&values[offset]); } else if (channel.target_path == "scale") { keyframe.scale = glm::make_vec3(&values[offset]); }
                }

                animChannel.keyframes.push_back(keyframe);
            }

            animation.channels.push_back(animChannel);
        }

        animations.push_back(animation);
    }
}

void updateSkeleton(const Animation &animation, float time, Skeleton &skeleton) {
    for (const auto &channel : animation.channels) {
        int boneIndex = channel.boneIndex;
        Skeleton::Bone &bone = skeleton.bones[boneIndex];

        const auto &keyframes = channel.keyframes;
        size_t numKeyframes = keyframes.size();

        // Handle time outside the animation duration
        if (time < keyframes.front().time) { time = keyframes.front().time; } else if (time > keyframes.back().time) { time = fmod(time, animation.duration); }

        // Find the current keyframe index
        size_t kfIndex = 0;
        for (; kfIndex < numKeyframes - 1; ++kfIndex) { if (time < keyframes[kfIndex + 1].time) { break; } }

        const KeyFrame &kf0 = keyframes[kfIndex];
        const KeyFrame &kf1 = keyframes[std::min(kfIndex + 1, numKeyframes - 1)];

        float t = 0.0f;
        if (kf0.time != kf1.time) { t = (time - kf0.time) / (kf1.time - kf0.time); }

        // Interpolation based on the method
        if (channel.interpolation == "LINEAR") { if (channel.path == "translation") { bone.animTranslation = glm::mix(kf0.translation, kf1.translation, t); } else if (channel.path == "rotation") { bone.animRotation = glm::slerp(kf0.rotation, kf1.rotation, t); } else if (channel.path == "scale") { bone.animScale = glm::mix(kf0.scale, kf1.scale, t); } } else if (channel.interpolation == "STEP") { if (channel.path == "translation") { bone.animTranslation = kf0.translation; } else if (channel.path == "rotation") { bone.animRotation = kf0.rotation; } else if (channel.path == "scale") { bone.animScale = kf0.scale; } } else if (channel.interpolation == "CUBICSPLINE") { std::cout << "Cubic interpolation is not supported" << std::endl; }

        // Update the bone's local transform
        bone.localTransform = glm::translate(glm::mat4(1.0f), bone.animTranslation) *
                              glm::mat4_cast(bone.animRotation) *
                              glm::scale(glm::mat4(1.0f), bone.animScale);
    }
}

vec3 getNodeTranslation(const tinygltf::Node &node) {
    if (!node.translation.empty()) { return glm::vec3(node.translation[0], node.translation[1], node.translation[2]); }
    return glm::vec3(0.0f);
}

glm::quat getNodeRotation(const tinygltf::Node &node) {
    if (!node.rotation.empty()) { return glm::quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]); }
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

vec3 getNodeScale(const tinygltf::Node &node) {
    if (!node.scale.empty()) { return glm::vec3(node.scale[0], node.scale[1], node.scale[2]); }
    return glm::vec3(1.0f);
}

mat4 getNodeTransform(const tinygltf::Node &node) {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), getNodeTranslation(node));
    glm::mat4 rotation = glm::mat4_cast(getNodeRotation(node));
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), getNodeScale(node));
    return translation * rotation * scale;
}

tinygltf::Model GLTFLoader::loadGltfModel(const std::string &filepath) {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string error, warning;

    bool loaded = loader.LoadASCIIFromFile(&model, &error, &warning, filepath);
    if (!loaded) { ASSERT(false, "Failed to load glTF model"); }

    if (!warning.empty()) { std::cerr << "glTF Warning: " << warning << std::endl; }

    return model;
}

bool arePositionsEqual(const vec3 &posA, const vec3 &posB, float epsilon) { return glm::distance(posA, posB) < epsilon; }

void updateNgonMeshWithBoneData(const tinygltf::Model &model, NGonDataWBones &ngonMesh) {
    // VERY SLOW, VERY NAIVE
    for (const auto &mesh : model.meshes) {
        for (const auto &primitive : mesh.primitives) {
            const auto &attributes = primitive.attributes;

            // Ensure JOINTS_0 and WEIGHTS_0 are present in the glTF model
            if (attributes.find("JOINTS_0") != attributes.end() && attributes.find("WEIGHTS_0") != attributes.end()) {
                const tinygltf::Accessor &jointAccessor = model.accessors[attributes.at("JOINTS_0")];
                const tinygltf::Accessor &weightAccessor = model.accessors[attributes.at("WEIGHTS_0")];
                const tinygltf::Accessor &positionAccessor = model.accessors[attributes.at("POSITION")];

                const tinygltf::BufferView &jointBufferView = model.bufferViews[jointAccessor.bufferView];
                const tinygltf::BufferView &weightBufferView = model.bufferViews[weightAccessor.bufferView];
                const tinygltf::BufferView &positionBufferView = model.bufferViews[positionAccessor.bufferView];

                const uint8_t *jointData = reinterpret_cast<const uint8_t *>(&model.buffers[jointBufferView.buffer].data[jointAccessor.byteOffset + jointBufferView.byteOffset]);
                const float *weightData = reinterpret_cast<const float *>(&model.buffers[weightBufferView.buffer].data[weightAccessor.byteOffset + weightBufferView.byteOffset]);
                const float *positionData = reinterpret_cast<const float *>(&model.buffers[positionBufferView.buffer].data[positionAccessor.byteOffset + positionBufferView.byteOffset]);

                size_t vertexCount = positionAccessor.count;

                // Iterate through each vertex in the glTF model
                for (size_t i = 0; i < vertexCount; ++i) {
                    // Extract position from glTF
                    vec3 gltfPosition(positionData[i * 3 + 0], positionData[i * 3 + 1], positionData[i * 3 + 2]);

                    // Now directly compare the position with the vertices in NgonMesh
                    for (size_t j = 0; j < ngonMesh.vertices.size(); ++j) {
                        const vec3 &ngonPosition = ngonMesh.vertices[j].position;

                        // Directly compare vertex positions
                        if (arePositionsEqual(gltfPosition, ngonPosition)) {
                            // If positions match, update the NgonMesh vertex with joint indices and weights
                            vec4 jointIndices;
                            vec4 jointWeights;

                            // Extract joint indices (4 per vertex, stored as unsigned bytes)
                            jointIndices.x = static_cast<float>(jointData[i * 4 + 0]);
                            jointIndices.y = static_cast<float>(jointData[i * 4 + 1]);
                            jointIndices.z = static_cast<float>(jointData[i * 4 + 2]);
                            jointIndices.w = static_cast<float>(jointData[i * 4 + 3]);

                            // Extract joint weights (4 per vertex, stored as floats)
                            jointWeights.x = weightData[i * 4 + 0];
                            jointWeights.y = weightData[i * 4 + 1];
                            jointWeights.z = weightData[i * 4 + 2];
                            jointWeights.w = weightData[i * 4 + 3];

                            // Update the corresponding NgonMesh vertex with the bone data
                            ngonMesh.jointIndices[j] = jointIndices;
                            ngonMesh.jointWeights[j] = jointWeights;
                            break; // Move on to the next vertex in the glTF model
                        }
                    }
                }
            } else { std::cerr << "JOINTS_0 or WEIGHTS_0 not found in the glTF model." << std::endl; }
        }
    }
}