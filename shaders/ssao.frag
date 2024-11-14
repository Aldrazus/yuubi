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

float linearizeDepth(float depth) {
    float zNear = PushConstants.nearPlane;
    float zFar = PushConstants.farPlane;
    //return (2.0 * zNear * zFar) / (zFar + zNear - depth * (zFar - zNear));
    return (zNear * zFar) / (zFar - depth * (zFar - zNear));
}

// Position reconstruction code was obtained from this blog:
// https://wickedengine.net/2019/09/improved-normal-reconstruction-from-depth/
vec3 reconstructPosition() {
    float depth = texture(depthTex, texCoords).r;
    vec4 upos = inverse(PushConstants.projection) * vec4(texCoords * 2.0 - 1.0, depth, 1.0);
    return upos.xyz / upos.w;
}


void main() {
    vec3 position = reconstructPosition();

    // TODO: this is stored as RGBA texture. Maybe try compressing it to R16G16?
    // If so, use octahedron normal encoding: 
    // https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
    // https://jcgt.org/published/0003/02/01/
    // https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
    vec3 normal = normalize(texture(normalTex, texCoords).xyz * 2.0 - 1.0);

    ivec2 texDim = textureSize(depthTex, 0);
    ivec2 noiseDim = textureSize(noiseTex, 0);
    const vec2 noiseUV = vec2(float(texDim.x)/float(noiseDim.x), float(texDim.y)/(noiseDim.y)) * texCoords;
    vec3 randomVec = texture(noiseTex, noiseUV).rgb * 2.0 - 1.0;
    
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(tangent, normal);
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

        float sampleDepth = -texture(depthTex, offset.xy).r; 

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(position.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / kernelSamples.length());

    float depth = linearizeDepth(texture(depthTex, texCoords).r);
    ambientOcclusion = vec4(depth, depth, depth, 1.0);
}
