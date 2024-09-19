#version 460

#extension GL_GOOGLE_include_directive : require

#include "push_constants.glsl"

layout(location = 0) out vec3 fragColor;

void main() {
    Vertex vertex = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = PushConstants.mvp * vec4(vertex.position, 1.0f);
    fragColor = vertex.normal;
}
