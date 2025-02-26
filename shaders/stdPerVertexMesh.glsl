#ifndef STDVERTFRAGI_MESH_GLSL
#define STDVERTFRAGI_MESH_GLSL

#extension GL_EXT_mesh_shader : require

struct PerVertex {
    vec4 worldPosU;
    vec4 normalV;
};

struct PerPrimitive {
    uvec4 data;
};

#ifdef FRAGMENT_SHADER
layout(location = 0) flat in perprimitiveEXT PerPrimitive perPrimitive;
layout(location = 1) in PerVertex perVertex;

layout(location = 0) out vec4 outColor;
#else
layout(location = 0) flat out perprimitiveEXT PerPrimitive perPrimitive[];
layout(location = 1) out PerVertex perVertex[];
#endif

// functions
#ifdef FRAGMENT_SHADER
vec3 getWorldPos() {
    return perVertex.worldPosU.xyz;
}

vec3 getNormal() {
    return perVertex.normalV.xyz;
}

vec2 getLocalUV() {
    return vec2(perVertex.worldPosU.w, perVertex.normalV.w);
}
#endif

#endif