#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : require

layout (location = 0) in vec2 texCoords;

layout(set = 0, binding = 0) uniform sampler2D depthTex;
layout(set = 0, binding = 1) uniform sampler2D normalTex;

layout(location = 0) out float ambientOcclusion;

layout (push_constant, scalar) uniform constants {
    mat4 inverseProjection;
} PushConstants;

// Position reconstruction code was obtained from this blog:
// https://wickedengine.net/2019/09/improved-normal-reconstruction-from-depth/
vec3 reconstructPosition() {
    float depth = texture(depthTex, texCoords).r;
    float x = texCoords.x * 2.0 - 1.0;
    float y = (1.0 - texCoords.y) * 2.0 - 1.0;
    vec4 pos = vec4(x, y, depth, 1.0);
    vec4 viewSpacePos = PushConstants.inverseProjection * pos;
    return viewSpacePos.xyz / viewSpacePos.w;
}

void main() {
    vec3 position = reconstructPosition();

    // TODO: this is stored as RGBA texture. Maybe try compressing it to R16G16?
    // If so, use octahedron normal encoding: 
    // https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
    // https://jcgt.org/published/0003/02/01/
    // https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
    vec3 normal = texture(normalTex, texCoords).xyz;

    ambientOcclusion = 1.0f;
}
