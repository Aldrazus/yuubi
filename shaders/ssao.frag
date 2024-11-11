#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_samplerless_texture_functions : require

layout (location = 0) in vec2 texCoords;

layout(set = 0, binding = 0) uniform sampler2D depthTex;
layout(set = 0, binding = 1) uniform sampler2D normalTex;
layout(set = 0, binding = 2) uniform sampler2D noiseTex;

vec3 kernelSamples[16] = {
    vec3(-0.09999844, -0.07369244, 0.07556053), vec3(-0.008269972, 0.0065534473, 0.021895919),
    vec3(-0.09079013, 0.035851546, 0.068078905), vec3(0.08712961, -0.023350783, 0.05205577),
    vec3(0.066774845, -0.09390371, 0.005393151), vec3(0.0059922514, 0.034530725, 0.0007765846),
    vec3(-0.023777973, -0.08834473, 0.042574193), vec3(0.038093243, 0.018147234, 0.09488363),
    vec3(0.071667366, 0.0055750995, 0.009519803), vec3(0.03186604, -0.01739076, 0.07258418),
    vec3(0.08657208, 0.0553202, 0.027686996), vec3(-0.0954788, 0.049810052, 0.03462647),
    vec3(0.0286261, 0.05533859, 0.106943), vec3(-0.029062647, -0.054594144, 0.106027156),
    vec3(0.049326677, 0.056126736, 0.0721665), vec3(-0.09466426, 0.02916146, 0.097996004),
};

layout(location = 0) out float ambientOcclusion;

layout (push_constant, scalar) uniform constants {
    mat4 projection;
} PushConstants;

// Position reconstruction code was obtained from this blog:
// https://wickedengine.net/2019/09/improved-normal-reconstruction-from-depth/
vec3 reconstructPosition() {
    float depth = texture(depthTex, texCoords).r;
    float x = texCoords.x * 2.0 - 1.0;
    float y = (1.0 - texCoords.y) * 2.0 - 1.0;
    vec4 pos = vec4(x, y, depth, 1.0);
    vec4 viewSpacePos = inverse(PushConstants.projection) * pos;
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
    vec3 randomVec = normalize(texture(noiseTex, texCoords).rgb);
    
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    
    const float radius = 0.5;
    const float bias = 0.025;
    for (int i = 0; i < kernelSamples.length(); i++) {
        vec3 samplePos = TBN * kernelSamples[i];
        samplePos = position + samplePos * radius;

        vec4 offset = vec4(samplePos, 1.0);
        offset = PushConstants.projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sampleDepth = texture(depthTex, offset.xy).r; 

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(position.z - sampleDepth));
    }

    occlusion = 1.0 - (occlusion / kernelSamples.length());
    ambientOcclusion = occlusion;
}
