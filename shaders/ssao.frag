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

layout(location = 0) out vec4 ambientOcclusion;

layout (push_constant, scalar) uniform constants {
    mat4 projection;
    float nearPlane;
    float farPlane;
} PushConstants;

vec3 reconstructVSPosFromDepth(vec2 uv) {
    float depth = texture(depthTex, uv).r;
    float x = uv.x * 2.0f - 1.0f;
    // y axis is flipped in Vulkan
    float y = (1.0f - uv.y) * 2.0f - 1.0f;
    vec4 pos = vec4(x, y, depth, 1.0f);
    vec4 posVS = inverse(PushConstants.projection) * pos;
    vec3 posNDC = posVS.xyz / posVS.w;
    return posNDC;
}

void main() {
    float depth = texture(depthTex, texCoords).r;
    if (depth == 0.0f) {
        ambientOcclusion = vec4(1.0f);
        return;
    }

    // TODO: this is stored as RGBA texture. Maybe try compressing it to R16G16?
    // If so, use octahedron normal encoding: 
    // https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
    // https://jcgt.org/published/0003/02/01/
    // https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
    vec3 normal = normalize(texture(normalTex, texCoords).xyz * 2.0f - 1.0f);

    vec3 posVS = reconstructVSPosFromDepth(texCoords);


    ivec2 depthTexSize = textureSize(depthTex, 0);
    ivec2 noiseTexSize = textureSize(noiseTex, 0);
    const vec2 noiseUV = vec2(float(depthTexSize.x)/float(noiseTexSize.x), float(depthTexSize.y)/(noiseTexSize.y)) * texCoords;
    vec3 randomVec = texture(noiseTex, noiseUV).xyz;

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(tangent, normal);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    const float radius = 0.5;
    const float bias = 0.025;
    for (int i = 0; i < kernelSamples.length(); i++) {
        vec3 samplePos = TBN * kernelSamples[i];
        samplePos = posVS + samplePos * radius;

        vec4 offset = vec4(samplePos, 1.0);
        offset = PushConstants.projection * offset;
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5f + 0.5f;
        offset.y = 1.0f - offset.y;


        vec3 reconstructedPos = reconstructVSPosFromDepth(offset.xy);
        vec3 sampledNormal = normalize(texture(normalTex, offset.xy).xyz * 2.0f - 1.0f);
        if (dot(sampledNormal, normal) <= 0.99) {
            float rangeCheck = smoothstep(0.0f, 1.0f, radius / abs(reconstructedPos.z - samplePos.z - bias));
            occlusion += (reconstructedPos.z <= samplePos.z - bias ? 1.0f : 0.0f) * rangeCheck;
        } 
    }

    occlusion = 1.0 - (occlusion / kernelSamples.length());

    ambientOcclusion = vec4(occlusion, occlusion, occlusion, 1.0);
}
