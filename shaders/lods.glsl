#ifndef LODS_GLSL
#define LODS_GLSL

struct LodInfos {
    mat4 MVP;
    vec3 position;
    vec3 normal;
    vec3 minBound;
    vec3 maxBound;
    float area;

    // Control
    vec3 controlNormal;
};

// ================== Helper Functions =====================

// Function to project a 3D point into screen space using MVP matrix
vec2 projectToScreenSpace(vec3 worldPos, LodInfos lodInfos) {
    vec4 clipSpacePos = lodInfos.MVP * vec4(worldPos, 1.0);
    // Convert from clip space to normalized device coordinates (NDC)
    vec3 ndcPos = clipSpacePos.xyz / clipSpacePos.w;
    // Convert from NDC to screen space (range: [-1, 1] in both dimensions)
    return ndcPos.xy;
}

// Function to compute the screen space size based on the bounding box approach
float boundingBoxScreenSpaceSize(LodInfos lodinfos) {
    vec3 minBound = lodinfos.minBound;
    vec3 maxBound = lodinfos.maxBound;
    mat4 MVP = lodinfos.MVP;

    mat3 rotation = align_rotation_to_vector(lodinfos.controlNormal, lodinfos.normal);

    vec3 corners[8];
    corners[0] = vec3(minBound.x, minBound.y, minBound.z);
    corners[1] = vec3(minBound.x, minBound.y, maxBound.z);
    corners[2] = vec3(minBound.x, maxBound.y, minBound.z);
    corners[3] = vec3(minBound.x, maxBound.y, maxBound.z);
    corners[4] = vec3(maxBound.x, minBound.y, minBound.z);
    corners[5] = vec3(maxBound.x, minBound.y, maxBound.z);
    corners[6] = vec3(maxBound.x, maxBound.y, minBound.z);
    corners[7] = vec3(maxBound.x, maxBound.y, maxBound.z);
    // Project all corners to screen space
    vec2 projectedCorners[8];
    for (int i = 0; i < 8; i++) {
        projectedCorners[i] = projectToScreenSpace(corners[i] * rotation + lodinfos.position, lodinfos);
    }

    // Compute the maximum screen space distance between any two corners
    float maxDistance = 0.0;
    for (int i = 0; i < 8; i++) {
        for (int j = i + 1; j < 8; j++) {
            float dist = distance(projectedCorners[i], projectedCorners[j]);
            maxDistance = max(maxDistance, dist);
        }
    }

    return maxDistance;
}

#endif // LODS_GLSL