#pragma once

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tiny_gltf.h>

#include <iostream>
#include "defines.hpp"
#include "ObjLoader.hpp"

// Data structures


struct Skeleton {
    struct Bone {
        int nodeIndex; // Index of the node in the glTF model
        std::string name; // Name of the bone
        int parentIndex; // Index of the parent bone (-1 if root)
        std::vector<int> childrenIndices; // Indices of child bones
        mat4 inverseBindMatrix; // Inverse bind matrix
        mat4 localTransform; // Local transform (animation applied)
        vec3 animTranslation; // Animated translation
        glm::quat animRotation; // Animated rotation
        vec3 animScale; // Animated scale
    };

    std::vector<Bone> bones;
};

// Helper functions
mat4 getNodeTransform(const tinygltf::Node &node);
vec3 getNodeTranslation(const tinygltf::Node &node);
glm::quat getNodeRotation(const tinygltf::Node &node);
vec3 getNodeScale(const tinygltf::Node &node);

// Function to extract skeleton
void extractSkeleton(const tinygltf::Model &model, Skeleton &skeleton);

// Recursive function to compute global transform
void computeGlobalTransform(const Skeleton &skeleton, int boneIndex, std::vector<mat4> &globalTransforms);

void computeBoneMatrices(const Skeleton &skeleton, std::vector<mat4> &boneMatrices);

struct KeyFrame {
    float time;
    // For LINEAR and STEP
    vec3 translation;
    glm::quat rotation;
    vec3 scale;

    // For CUBICSPLINE (additional data)
    vec3 inTangentTranslation;
    vec3 outTangentTranslation;
    glm::quat inTangentRotation;
    glm::quat outTangentRotation;
    vec3 inTangentScale;
    vec3 outTangentScale;
};


struct AnimationChannel {
    int boneIndex;
    std::string path; // "translation", "rotation", or "scale"
    std::string interpolation; // "LINEAR", "STEP", or "CUBICSPLINE"
    std::vector<KeyFrame> keyframes;
};

struct Animation {
    std::string name;
    float duration;
    std::vector<AnimationChannel> channels;
};

void extractAnimations(const tinygltf::Model &model, const Skeleton &skeleton, std::vector<Animation> &animations);

void updateSkeleton(const Animation &animation, float time, Skeleton &skeleton);

void updateNgonMeshWithBoneData(const tinygltf::Model &model, NGonDataWBones &ngonMesh);

class GLTFLoader {
public:
    static tinygltf::Model loadGltfModel(const std::string &filepath);

};

// Helper functions to get node transformations
bool arePositionsEqual(const vec3 &posA, const vec3 &posB, float epsilon = 1e-5f);