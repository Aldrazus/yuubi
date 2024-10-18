#version 460

#extension GL_GOOGLE_include_directive : require

#include "push_constants.glsl"

layout(location = 0) out vec2 fragUv;

void main() {
    Vertex vertex = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = PushConstants.sceneData.viewproj * PushConstants.transform * vec4(vertex.position, 1.0f);
    fragUv = vec2(vertex.uv_x, vertex.uv_y);
}
