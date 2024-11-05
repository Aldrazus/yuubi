#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#include "scene_data.glsl"
#include "vertex.glsl"

layout (push_constant, scalar) uniform constants {
    mat4 transform;
    SceneDataBuffer sceneData;
    VertexBuffer vertexBuffer;
} PushConstants;

layout(location = 0) out vec2 outUv;

void main() {
    Vertex vertex = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = PushConstants.sceneData.viewproj * PushConstants.transform * vec4(vertex.position, 1.0f);
    outUv = vec2(vertex.uv_x, vertex.uv_y);
}
