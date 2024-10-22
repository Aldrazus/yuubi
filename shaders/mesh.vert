#version 460

#extension GL_GOOGLE_include_directive : require

#include "push_constants.glsl"

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUv;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec3 outBitangent;

void main() {
    Vertex vertex = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    vec4 worldPosition = PushConstants.transform * vec4(vertex.position, 1.0f);
    outPos = worldPosition.xyz;
    gl_Position = PushConstants.sceneData.viewproj * worldPosition;
    outUv = vec2(vertex.uv_x, vertex.uv_y);

    // PERF: expensive inverse
    mat3 normalMatrix = transpose(inverse(mat3(PushConstants.transform)));


    outNormal = normalMatrix * normalize(vertex.normal);
    outTangent = normalMatrix * normalize(vertex.tangent.xyz);
    outTangent = vertex.tangent.xyz;
    outBitangent = cross(vertex.normal, vertex.tangent.xyz) * vertex.tangent.w;
}
