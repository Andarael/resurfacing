#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <regex>

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

static void extractIndices(const std::string &token, int &posIndex, int &texIndex, int &nIndex) {
    posIndex = texIndex = nIndex = 0; // Default values in case no indices are found

    size_t firstSlash = token.find('/');
    size_t secondSlash = token.find('/', firstSlash + 1);
    if (firstSlash != std::string::npos) { if (firstSlash > 0) { posIndex = std::stoi(token.substr(0, firstSlash)); } } else {
        // If no slashes are found, treat the whole token as the position index
        posIndex = std::stoi(token);
        return; // No texture or normal index to extract
    }

    // Extract the texture index (between the first and second slashes)
    if (secondSlash != std::string::npos) { if (secondSlash > firstSlash + 1) { texIndex = std::stoi(token.substr(firstSlash + 1, secondSlash - firstSlash - 1)); } } else if (firstSlash + 1 < token.size()) {
        // If only one slash is found and there are characters after it, it may contain a texture index
        texIndex = std::stoi(token.substr(firstSlash + 1));
        return; // No normal index to extract
    }

    // Extract the normal index (after the second slash)
    if (secondSlash + 1 < token.size()) { nIndex = std::stoi(token.substr(secondSlash + 1)); }
}

class NgonLoader {
public:
    static NgonData loadNgonData(const std::string &filename) {
        NgonData ngonData;
        std::ifstream file(filename);
        std::string line;
        // temp data
        std::vector<vec4> temp_positions;
        std::vector<vec4> temp_normals;
        std::vector<vec4> temp_texCoords;
        // open file
        if (!file.is_open()) {
            std::cerr << "Error opening the file: " << filename << std::endl;
            return ngonData;
        }
        // read file
        while (std::getline(file, line)) {
            std::istringstream ss(line);
            std::string type;
            ss >> type;
            if (type == "v") {
                vec4 position = vec4(0, 0, 0, 1);
                ss >> position.x >> position.y >> position.z;
                temp_positions.push_back(position);
            } else if (type == "vn") {
                vec4 normal = vec4(0);
                ss >> normal.x >> normal.y >> normal.z;
                temp_normals.push_back(normal);
            } else if (type == "vt") {
                vec4 texCoord = vec4(0);
                ss >> texCoord.x >> texCoord.y;
                temp_texCoords.push_back(texCoord);
            } else if (type == "f") {
                NGonFace face{};
                std::string token;
                face.count = 0;
                face.offset = unsigned(ngonData.indices.size());
                face.normal = VEC4F_ONE;
                while (ss >> token) {
                    int posIndex, texIndex, nIndex;
                    extractIndices(token, posIndex, texIndex, nIndex);
                    Vertex vertex{};
                    // OBJ file uses 1-based indexing
                    // set vertex data (position, normal, texCoord
                    if (temp_positions.size() >= posIndex && posIndex > 0) {
                        vec3 pos = temp_positions[posIndex - 1];
                        vertex.position = vec4(pos.x, pos.y, pos.z, 1.0f);
                    }
                    if (temp_normals.size() >= nIndex && nIndex > 0)
                        vertex.normal = temp_normals[nIndex - 1];
                    if (temp_texCoords.size() >= texIndex && texIndex > 0)
                        vertex.texCoord = temp_texCoords[texIndex - 1];
                    // set vertex at pos (posIndex - 1) in _vertices
                    // but first allocate space for it up to posIndex
                    if (posIndex > ngonData.vertices.size())
                        ngonData.vertices.resize(posIndex);
                    ngonData.vertices.at(posIndex - 1) = vertex;
                    ngonData.indices.push_back(posIndex - 1);
                    face.count++;
                }
                ngonData.faces.push_back(face);
            }
        }

        file.close();

        // compute face normals
        for (auto &face : ngonData.faces) {
            vec4 v0 = ngonData.vertices[ngonData.indices[face.offset]].position;
            vec4 v1 = ngonData.vertices[ngonData.indices[face.offset + 1]].position;
            vec4 v2 = ngonData.vertices[ngonData.indices[face.offset + 2]].position;
            vec3 normal = glm::normalize(glm::cross(vec3(v1.x, v1.y, v1.z) - vec3(v0.x, v0.y, v0.z),
                                                    vec3(v2.x, v2.y, v2.z) - vec3(v0.x, v0.y, v0.z)));
            face.normal = vec4(normal.x, normal.y, normal.z, 1.0f);
        }

        // compute face areas
        for (auto &face : ngonData.faces) {
            vec4 center = VEC4F_ZERO;
            for (uint32 i = 0; i < face.count; i++) { center += ngonData.vertices[ngonData.indices[face.offset + i]].position; }
            center /= float(face.count);
            face.faceArea = 0.0f;
            for (uint32 i = 0; i < face.count; i++) {
                vec4 v0 = ngonData.vertices[ngonData.indices[face.offset + i]].position;
                vec4 v1 = ngonData.vertices[ngonData.indices[face.offset + (i + 1) % face.count]].position;
                face.faceArea += glm::length(glm::cross(vec3(v1.x, v1.y, v1.z) - vec3(v0.x, v0.y, v0.z),
                                                        vec3(center.x, center.y, center.z) - vec3(v0.x, v0.y, v0.z)));
            }
            face.faceArea *= 0.5f;
        }

        // compute face centers
        for (auto &face : ngonData.faces) {
            face.center = VEC4F_ZERO;
            for (uint32 i = 0; i < face.count; i++) { face.center += ngonData.vertices[ngonData.indices[face.offset + i]].position; }
            face.center /= float(face.count);
        }

        // set ngonData
        ngonData.vertices.shrink_to_fit();
        ngonData.indices.shrink_to_fit();
        ngonData.faces.shrink_to_fit();
        return ngonData;
    }
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

    static LutData loadLutData(const std::string &filename) {
        NgonData ngonData = NgonLoader::loadNgonData(filename);

        // Compute Nx and Ny from the UV layout
        uint32 Nx = 0;
        uint32 Ny = 0;

        // Compute grid dimensions
        // Example pseudo-code to compute Nx and Ny
        std::unordered_set<float> uniqueU, uniqueV;
        for (const auto &vertex : ngonData.vertices) {
            uniqueU.insert(vertex.texCoord.x); // assuming texCoord.x is U
            uniqueV.insert(vertex.texCoord.y); // assuming texCoord.y is V
        }
        Nx = uniqueU.size();
        Ny = uniqueV.size();

        // Reorder vertices based on grid dimensions
        // Example pseudo-code to sort by UVs
        std::sort(ngonData.vertices.begin(), ngonData.vertices.end(),
                  [](const Vertex &a, const Vertex &b) {
                      if (a.texCoord.y == b.texCoord.y) { return a.texCoord.x < b.texCoord.x; }
                      return a.texCoord.y < b.texCoord.y;
                  });

        // Populate LutData
        LutData lutData;
        lutData.positions.reserve(ngonData.vertices.size());

        for (const auto &vertex : ngonData.vertices) { lutData.positions.push_back(vertex.position); }

        // Set Nx and Ny in the LutData
        lutData.Nx = Nx;
        lutData.Ny = Ny;

        // compute min and max
        vec3 min = vec3(std::numeric_limits<float>::max());
        vec3 max = vec3(std::numeric_limits<float>::min());

        for (const auto &vertex : lutData.positions) {
            min = glm::min(min, vec3(vertex.x, vertex.y, vertex.z));
            max = glm::max(max, vec3(vertex.x, vertex.y, vertex.z));
        }

        lutData.min = min;
        lutData.max = max;

        return lutData;
    }
};