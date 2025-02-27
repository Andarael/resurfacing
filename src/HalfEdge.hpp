#pragma once

#include <iostream>
#include <map>
#include <vector>

#include "defines.hpp"
#include "loaders/objLoader.hpp"

// Structs now use Structure of Arrays (SoA)
struct HEVertices {
    std::vector<vec4> positions;
    std::vector<vec4> colors;
    std::vector<vec4> normals;
    std::vector<vec2> texCoords;
    std::vector<int> edges; // one outgoing half-edge
};

struct HEFaces {
    std::vector<int> edges;      // one half-edge bordering the face
    std::vector<int> vertCounts; // number of vertices in the face
    std::vector<int> offsets;    // offset in the vertexFaceIndices buffer
    std::vector<vec4> normals;
    std::vector<vec4> centers;
    std::vector<float> faceAreas;
};

struct HalfEdges {
    std::vector<int> vertices; // vertex at the end of the half-edge
    std::vector<int> faces;    // face this half-edge borders
    std::vector<int> next;     // next half-edge in the face loop
    std::vector<int> prev;     // previous half-edge in the face loop
    std::vector<int> twins;    // twin half-edge
};

// Structure of arrays for the entire HalfEdgeMesh
struct HalfEdgeMesh {
    uint32 nbVertices = 0;
    uint32 nbFaces = 0;
    HEVertices vertices; // All vertex data split into SoA
    HEFaces faces;       // All face data split into SoA
    HalfEdges halfEdges; // All half-edge data split into SoA

    std::vector<int> vertexFaceIndices; // Buffer of vertex indices for each face
};

// Main function to convert NgonMesh to HalfEdgeMesh using SoA and vertexFaceIndices
inline HalfEdgeMesh convertToHalfEdgeMesh(const NgonData &ngonMesh) {
    HalfEdgeMesh heMesh;

    std::map<std::pair<int, int>, int> edgeToHalfEdgeMap; // Maps edges to half-edge indices
    std::vector<std::pair<int, int>> halfEdgeToEdge;      // For finding twins

    // Initialize vertices
    for (const auto &v : ngonMesh.vertices) {
        heMesh.vertices.positions.push_back(v.position);
        heMesh.vertices.colors.push_back(v.color);
        heMesh.vertices.normals.push_back(v.normal);
        heMesh.vertices.texCoords.push_back(v.texCoord);
        heMesh.vertices.edges.push_back(-1); // Initialize with no edge
    }

    int vertexFaceOffset = 0; // Tracks offset for vertexFaceIndices

    // Initialize faces and half-edges
    for (const auto &f : ngonMesh.faces) {
        int firstHalfEdgeIndex = heMesh.halfEdges.vertices.size();
        int lastHalfEdgeIndex = -1;

        // Store the offset to the vertex indices of this face
        heMesh.faces.offsets.push_back(vertexFaceOffset);

        for (unsigned i = 0; i < f.count; ++i) {
            int startVertexIndex = ngonMesh.indices[f.offset + i];
            int endVertexIndex = ngonMesh.indices[f.offset + (i + 1) % f.count];

            HalfEdges he;
            heMesh.halfEdges.vertices.push_back(endVertexIndex);
            heMesh.halfEdges.faces.push_back(heMesh.faces.edges.size());           // Current face index
            heMesh.halfEdges.next.push_back(heMesh.halfEdges.vertices.size() + 1); // Point to the next half-edge
            heMesh.halfEdges.prev.push_back(lastHalfEdgeIndex);                    // Point to the previous half-edge
            heMesh.halfEdges.twins.push_back(-1);                                  // To be determined later

            // Add to vertexFaceIndices for the current face
            heMesh.vertexFaceIndices.push_back(startVertexIndex);
            ++vertexFaceOffset; // Increment offset for next vertex

            // Store the mapping from edge to half-edge index
            edgeToHalfEdgeMap[{startVertexIndex, endVertexIndex}] = heMesh.halfEdges.vertices.size() - 1;
            halfEdgeToEdge.push_back({startVertexIndex, endVertexIndex});

            if (lastHalfEdgeIndex != -1) {
                heMesh.halfEdges.next[lastHalfEdgeIndex] = heMesh.halfEdges.vertices.size() - 1;
            }

            lastHalfEdgeIndex = heMesh.halfEdges.vertices.size() - 1;
        }

        // Connect the last half-edge to the first
        heMesh.halfEdges.next[lastHalfEdgeIndex] = firstHalfEdgeIndex;
        heMesh.halfEdges.prev[firstHalfEdgeIndex] = lastHalfEdgeIndex;

        // Create the face
        heMesh.faces.edges.push_back(firstHalfEdgeIndex);
        heMesh.faces.normals.push_back(f.normal);
        heMesh.faces.centers.push_back(f.center);
        heMesh.faces.faceAreas.push_back(f.faceArea);
        heMesh.faces.vertCounts.push_back(f.count); // Set the vertex count for the face
    }

    // Link twin half-edges
    for (int i = 0; i < halfEdgeToEdge.size(); ++i) {
        int start = halfEdgeToEdge[i].first;
        int end = halfEdgeToEdge[i].second;
        auto it = edgeToHalfEdgeMap.find({end, start}); // Find the opposite half-edge
        if (it != edgeToHalfEdgeMap.end()) {
            heMesh.halfEdges.twins[i] = it->second;
            heMesh.halfEdges.twins[it->second] = i;
        }
    }

    // Set the vertex edge index
    for (int i = 0; i < heMesh.halfEdges.vertices.size(); ++i) {
        if (heMesh.vertices.edges[heMesh.halfEdges.vertices[i]] == -1) {
            heMesh.vertices.edges[heMesh.halfEdges.vertices[i]] = i;
        }
    }

    heMesh.nbVertices = ngonMesh.vertices.size();
    heMesh.nbFaces = ngonMesh.faces.size();

    return heMesh;
}
