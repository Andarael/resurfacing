#pragma once
#include "defines.hpp"

struct Vertex {
    vec4 position = VEC4F_ZERO;
    vec4 color = VEC4F_ONE;
    vec4 normal = VEC4F_ZERO;
    vec4 tangent = VEC4F_ZERO;
    vec4 bitangent = VEC4F_ZERO;
    vec4 texCoord = VEC4F_ZERO;
};

struct NGonFace {
    vec4 normal; // face normal
    vec4 center; // face center
    uint32 offset; // offset into the index indices array
    uint32 count; // number of vertices in the face
    float faceArea; // face area
    int32 padding = 0; // to align the struct to 16 bytes
};

struct NgonData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<NGonFace> faces;

    size_t getDataSize() const { return vertices.size() * sizeof(Vertex) + indices.size() * sizeof(uint32) + faces.size() * sizeof(NGonFace); }
};

struct NGonDataWBones : public NgonData {
    std::vector<vec4> jointIndices;
    std::vector<vec4> jointWeights;
    NGonDataWBones() = default;
    NGonDataWBones(const NgonData &data) : NgonData(data) {}
};

void extractIndices(const std::string &token, int &posIndex, int &texIndex, int &nIndex);

class NgonLoader {
public:
    static NgonData loadNgonData(const std::string &filename);
};

struct LutData {
    std::vector<vec4> positions;
    unsigned int Nx = 0;
    unsigned int Ny = 0;
    vec3 min = vec3(0);
    vec3 max = vec3(0);
};

class LutLoader {
public:
    // create a grid from any grid mesh with UVs :
    // 1. load the ngon mesh (it is guaranteed to have uvs, and be composed only of quads)
    // 2. compute grid dimentions
    // 3. sort vertices by u
    // cluster vertices by v while keeping the u order
    // 4. flatten the grid into a 1D array

    static LutData loadLutData(const std::string &filename);
};