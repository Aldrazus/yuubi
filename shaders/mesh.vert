#version 460

#extension GL_GOOGLE_include_directive : require

#include "push_constants.glsl"

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUv;

void main() {
    Vertex vertex = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPosition = PushConstants.transform * vec4(vertex.position, 1.0f);
    outPos = worldPosition.xyz;
    outNormal = vertex.normal;
    gl_Position = PushConstants.sceneData.viewproj * worldPosition;
    outUv = vec2(vertex.uv_x, vertex.uv_y);
}
