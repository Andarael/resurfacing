#ifndef PARAMETRIC_GRIDS_GLSL
#define PARAMETRIC_GRIDS_GLSL

#define EDGE_MODE 0

// ================== Control Points =====================
vec3 getLutData(uvec2 idx, uvec2 gridSize) {
    idx = clamp(idx, uvec2(0), uvec2(gridSize.x - 1, gridSize.y - 1));
    return lutVertex[idx.y * gridSize.x + idx.x].xyz;
}

// ================== B-spline and Bézier Matrices =====================

const mat4 BSPLINE_MATRIX_4 = mat4(
    vec4(1 / 6., 4 / 6., 1 / 6., 0.),
    vec4(-3 / 6., 0., 3 / 6., 0.),
    vec4(3 / 6., -6 / 6., 3 / 6., 0.),
    vec4(-1 / 6., 3 / 6., -3 / 6., 1 / 6.));

const mat4 BEZIER_MATRIX_3 = mat4(1, -3, 3, -1, 0, 3, -6, 3, 0, 0, 3, -3, 0, 0, 0, 1);
const mat3 BEZIER_MATRIX_2 = mat3(1, -2, 1, 0, 2, -2, 0, 0, 1);
const mat2 BEZIER_MATRIX_1 = mat2(1, -1, 0, 1);

// ================== Utility Functions =====================

uint wrapIndex(uint idx, uint maxIdx) {
    return idx % maxIdx;
}

uvec2 cycleIndex(uvec2 idx, uvec2 gridSize, bool cyclicU, bool cyclicV) {
    if (cyclicU) {
        idx.x = wrapIndex(idx.x, gridSize.x);
    }
    if (cyclicV) {
        idx.y = wrapIndex(idx.y, gridSize.y);
    }
    return idx;
}

uvec2 applyEdgeMode(uvec2 idx, uvec2 gridSize, uint edgeMode) {
    if (edgeMode == 1) { // Duplicate edges
        idx.x = clamp(idx.x, 0, gridSize.x - 1);
        idx.y = clamp(idx.y, 0, gridSize.y - 1);
    }

    return idx;
}

vec3 getControlPoint(uvec2 idx, uvec2 gridSize, bool cyclicU, bool cyclicV) {
    idx = cycleIndex(idx, gridSize, cyclicU, cyclicV);
    idx = applyEdgeMode(idx, gridSize, EDGE_MODE);

    return getLutData(idx, gridSize);
}

void computePatchUVs(out uvec2 patchUV, out vec2 localUV, vec2 uv, uvec2 nPatches, bool cyclicU, bool cyclicV) {
    vec2 scaledUV = uv * (nPatches - 1e-5);
    scaledUV = clamp(scaledUV, vec2(0), nPatches - 1e-5);
    patchUV = uvec2(scaledUV);
    localUV = scaledUV - patchUV;
    if (!cyclicU) patchUV.x = clamp(patchUV.x, 0, nPatches.x - 1);
    if (!cyclicV) patchUV.y = clamp(patchUV.y, 0, nPatches.y - 1);
}

// ================== Fetch Patch Control Points =====================

#define FETCH_PATCH_CONTROL_POINTS(type, degree, stride)                           \
    void fetchPatchControlPoints##type##degree(out vec3 P[degree + 1][degree + 1], \
                                               uvec2 patchUV,                      \
                                               uvec2 gridSize,                     \
                                               bool cyclicU, bool cyclicV) {       \
        for (uint i = 0; i <= degree; ++i) {                                       \
            for (uint j = 0; j <= degree; ++j) {                                   \
                uvec2 idx = patchUV * stride + uvec2(i, j);                        \
                P[i][j] = getControlPoint(idx, gridSize, cyclicU, cyclicV);        \
            }                                                                      \
        }                                                                          \
    }
FETCH_PATCH_CONTROL_POINTS(Bezier, 1, 1)  // void fetchPatchControlPointsBezier1(out vec3 P[2][2], uvec2 patchUV, uvec2 gridSize, bool cyclicU, bool cyclicV)
FETCH_PATCH_CONTROL_POINTS(Bezier, 2, 2)  // void fetchPatchControlPointsBezier2(out vec3 P[3][3], uvec2 patchUV, uvec2 gridSize, bool cyclicU, bool cyclicV)
FETCH_PATCH_CONTROL_POINTS(Bezier, 3, 3)  // void fetchPatchControlPointsBezier3(out vec3 P[4][4], uvec2 patchUV, uvec2 gridSize, bool cyclicU, bool cyclicV)
FETCH_PATCH_CONTROL_POINTS(Bspline, 3, 1) // void fetchPatchControlPointsBspline3() B-spline degree 3, stride 1

// ======================== Bézier ========================

vec4 computeBezierBlendingFunctions3(float t) { return vec4(1, t, t * t, t * t * t) * BEZIER_MATRIX_3; }
vec3 computeBezierBlendingFunctions2(float t) { return vec3(1, t, t * t) * BEZIER_MATRIX_2; }
vec2 computeBezierBlendingFunctions1(float t) { return vec2(1, t) * BEZIER_MATRIX_1; }

#define EVALUATE_BEZIER_PATCH(degree, T)                               \
    vec3 evaluateBezierPatch##degree(vec2 uv,                          \
                                     vec3 P[degree + 1][degree + 1]) { \
        T Bu = computeBezierBlendingFunctions##degree(uv.x);           \
        T Bv = computeBezierBlendingFunctions##degree(uv.y);           \
        vec3 point = vec3(0);                                          \
        for (int i = 0; i <= degree; i++) {                            \
            for (int j = 0; j <= degree; j++) {                        \
                point += Bu[i] * Bv[j] * P[i][j];                      \
            }                                                          \
        }                                                              \
        return point;                                                  \
    }
EVALUATE_BEZIER_PATCH(1, vec2) // vec2 evaluateBezierPatch1(vec2 uv, vec3 P[2][2])
EVALUATE_BEZIER_PATCH(2, vec3) // vec3 evaluateBezierPatch2(vec2 uv, vec3 P[3][3])
EVALUATE_BEZIER_PATCH(3, vec4) // vec4 evaluateBezierPatch3(vec2 uv, vec3 P[4][4])

void evaluateBezierSurface(vec2 uv, out vec3 pos, out vec3 normal, uvec2 gridSize, bool cyclicU, bool cyclicV, uint degree) {
    uvec2 nbPatches = uvec2(0);
    nbPatches.x = cyclicU ? gridSize.x : (gridSize.x - 1) / degree;
    nbPatches.y = cyclicV ? gridSize.y : (gridSize.y - 1) / degree;

    uvec2 patchUV;
    vec2 localUV;
    computePatchUVs(patchUV, localUV, uv, nbPatches, cyclicU, cyclicV);

    if (degree == 1) {
        vec3 P[2][2];
        fetchPatchControlPointsBezier1(P, patchUV, gridSize, cyclicU, cyclicV);
        pos = evaluateBezierPatch1(localUV, P);
        normal = vec3(0, 1, 0);
    }

    else if (degree == 2) {
        vec3 P[3][3];
        fetchPatchControlPointsBezier2(P, patchUV, gridSize, cyclicU, cyclicV);
        pos = evaluateBezierPatch2(localUV, P);
        normal = vec3(0, 1, 0);
    }

    else if (degree == 3) {
        vec3 P[4][4];
        fetchPatchControlPointsBezier3(P, patchUV, gridSize, cyclicU, cyclicV);
        pos = evaluateBezierPatch3(localUV, P);
        normal = vec3(0, 1, 0);
    }
}

// ================== B-spline Surface =====================

vec3 computeBSplinePoint(float t, vec3 P0, vec3 P1, vec3 P2, vec3 P3) {
    vec4 T = vec4(1.0, t, t * t, t * t * t);
    vec4 basis = BSPLINE_MATRIX_4 * T;
    return basis.x * P0 + basis.y * P1 + basis.z * P2 + basis.w * P3;
}

vec3 evaluateBSplinePatch(vec2 uv, vec3 P[4][4]) {
    uint degree = 3;
    vec3 Cu[4];
    for (uint j = 0; j <= degree; ++j) {
        Cu[j] = computeBSplinePoint(uv.x, P[0][j], P[1][j], P[2][j], P[3][j]);
    }
    return computeBSplinePoint(uv.y, Cu[0], Cu[1], Cu[2], Cu[3]);
}

// ================== Normal Calculation =====================

#define NORMAL_OFFSET 0.001

vec3 computeFiniteDifferenceBspline(vec2 uv, vec3 P[4][4]) {
    vec2 offset = vec2(NORMAL_OFFSET);
    vec3 dPdu = evaluateBSplinePatch(uv + vec2(offset.x, 0.0), P) - evaluateBSplinePatch(uv - vec2(offset.x, 0.0), P);
    vec3 dPdv = evaluateBSplinePatch(uv + vec2(0.0, offset.y), P) - evaluateBSplinePatch(uv - vec2(0.0, offset.y), P);

    return normalize(cross(dPdu, dPdv));
}

// ================== Surface Evaluation Function =====================

void evaluateBsplineSurface(vec2 uv, out vec3 pos, out vec3 normal, uvec2 gridSize, bool cyclicU, bool cyclicV) {
    uint degree = 3;
    vec2 offset = vec2(0.001); // Offset for finite difference normal calculation

    uvec2 nbPatches;
    nbPatches.x = cyclicU ? gridSize.x : gridSize.x - degree + (EDGE_MODE != 0 ? 0 : 0);
    nbPatches.y = cyclicV ? gridSize.y : gridSize.y - degree + (EDGE_MODE != 0 ? 0 : 0);

    uvec2 patchUV;
    vec2 localUV;
    computePatchUVs(patchUV, localUV, uv, nbPatches, cyclicU, cyclicV);

    vec3 P[4][4]; // Control points for the patch
    fetchPatchControlPointsBspline3(P, patchUV, gridSize, cyclicU, cyclicV);

    pos = evaluateBSplinePatch(localUV, P);
    normal = computeFiniteDifferenceBspline(localUV, P);
}

#endif // PARAMETRIC_GRIDS_GLSL
