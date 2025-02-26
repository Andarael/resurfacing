#version 450
#extension GL_EXT_fragment_shader_barycentric : require
#extension GL_EXT_mesh_shader : require

#include "../common.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in PerVertexData {
    vec4 position;
    vec4 color;
    vec4 normal;
    vec2 texCoords;
    vec4 tangent;
    vec4 bitangent;
    vec4 patchcolor;
    vec4 randoms;
}
frag_in;

layout(location = 8) in perprimitiveEXT PerPrimData{
    vec4 color;
}prim_in;

layout(push_constant) uniform PushConstants{
    mat4 MVP;
    vec4[3] ModelMatrix;
    vec4 cameraPos;
} constants;

void main() {
    vec3 color = IDtoColor(gl_PrimitiveID);

    // Assign the color to your output variable
    outColor = vec4(color,1.0);
    //outColor = vec4(gl_BaryCoordEXT, 1.0);
    //outColor = prim_in.color;
    //outColor = vec4(0.,0,1,1);
    //outColor = frag_in.color;
}