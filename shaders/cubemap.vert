#version 460

#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_multiview : require

layout (buffer_reference, scalar, std430) readonly buffer ProjectionViewMatrices {
    mat4 matrices[];
};

layout (push_constant) uniform pcs {
    ProjectionViewMatrices projectionViewMatrices;
} PushConstants;

layout(location = 0) out vec3 worldPos;

// TODO: what the fuck haha
const vec3 positions[36] = {
{ -1.0f, 1.0f, -1.0f },
{ -1.0f, -1.0f, -1.0f },
{ 1.0f, -1.0f, -1.0f },
{ 1.0f, -1.0f, -1.0f },
{ 1.0f, 1.0f, -1.0f },
{ -1.0f, 1.0f, -1.0f },

{ -1.0f, -1.0f, 1.0f },
{ -1.0f, -1.0f, -1.0f },
{ -1.0f, 1.0f, -1.0f },
{ -1.0f, 1.0f, -1.0f },
{ -1.0f, 1.0f, 1.0f },
{ -1.0f, -1.0f, 1.0f },

{ 1.0f, -1.0f, -1.0f },
{ 1.0f, -1.0f, 1.0f },
{ 1.0f, 1.0f, 1.0f },
{ 1.0f, 1.0f, 1.0f },
{ 1.0f, 1.0f, -1.0f },
{ 1.0f, -1.0f, -1.0f },

{ -1.0f, -1.0f, 1.0f },
{ -1.0f, 1.0f, 1.0f },
{ 1.0f, 1.0f, 1.0f },
{ 1.0f, 1.0f, 1.0f },
{ 1.0f, -1.0f, 1.0f },
{ -1.0f, -1.0f, 1.0f },

{ -1.0f, 1.0f, -1.0f },
{ 1.0f, 1.0f, -1.0f },
{ 1.0f, 1.0f, 1.0f },
{ 1.0f, 1.0f, 1.0f },
{ -1.0f, 1.0f, 1.0f },
{ -1.0f, 1.0f, -1.0f },

{ -1.0f, -1.0f, -1.0f },
{ -1.0f, -1.0f, 1.0f },
{ 1.0f, -1.0f, -1.0f },
{ 1.0f, -1.0f, -1.0f },
{ -1.0f, -1.0f, 1.0f },
{ 1.0f, -1.0f, 1.0f }
};

void main() {
    worldPos = positions[gl_VertexIndex];
    gl_Position = PushConstants.projectionViewMatrices.matrices[gl_ViewIndex] * vec4(worldPos, 1.0f);
}