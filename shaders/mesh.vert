#version 460

#extension GL_GOOGLE_include_directive : require

#include "push_constants.glsl"

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUv;
layout(location = 3) out mat3 outTBN;

void main() {
    Vertex vertex = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPosition = PushConstants.transform * vec4(vertex.position, 1.0f);
    outPos = worldPosition.xyz;

    // PERF: expensive inverse
    mat3 normalMatrix = transpose(inverse(mat3(PushConstants.transform)));

    outNormal = normalize(normalMatrix * vertex.normal);

    gl_Position = PushConstants.sceneData.viewproj * worldPosition;
    outUv = vec2(vertex.uv_x, vertex.uv_y);

    vec3 bitangent = cross(vertex.normal, vertex.tangent.xyz) * vertex.tangent.w;
    vec3 T = normalize(mat3(PushConstants.transform) * vertex.tangent.xyz);
    vec3 N = outNormal;
    vec3 B = normalize(mat3(PushConstants.transform) * bitangent);

    outTBN = mat3(T, B, N);
}
