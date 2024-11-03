#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "push_constants.glsl"
#include "bindless.glsl"

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUv;
layout (location = 3) in mat3 inTBN;

layout(location = 0) out vec4 _;
layout(location = 1) out vec4 outColor;

const float PI = 3.14159265359;

// PERF: Turn Array of Structures (AoS) to Structures of Arrays (SoA)
struct Light {
    vec3 color;
    vec3 position;
};

const Light lights[1] = Light[](
    Light(
        vec3(23.7, 21.31, 20.79),
        vec3(1.0, 3.0, 1.0)
    )
);


vec4 sampleTexture(uint index) {
    return texture(textures[nonuniformEXT(index)], inUv);
}

vec3 getNormalFromMap() {
    MaterialData material = PushConstants.sceneData.materials.data[PushConstants.materialId];

    vec3 tangentNormal = sampleTexture(material.normalTex).xyz * 2.0 - 1.0;

    return inTBN * normalize(tangentNormal);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

void main() {
    MaterialData material = PushConstants.sceneData.materials.data[PushConstants.materialId];

    vec3 cameraPosition = PushConstants.sceneData.cameraPosition.xyz;

    vec3 normal = inNormal;
    if (material.normalTex != 0) {
        vec3 n = getNormalFromMap();
        normal = vec3(n.xy * material.normalScale, n.z);
    }

    float alpha = material.albedoFactor.a;
    vec3 albedo = material.albedoFactor.rgb;
    if (material.albedoTex != 0) {
        vec4 sampledAlbedo = sampleTexture(material.albedoTex);
        albedo *= sampledAlbedo.rgb;
        alpha *= sampledAlbedo.a;
    }

    if (alpha < material.alphaCutoff) discard;

    float metallic = material.metallicFactor;
    float roughness = material.roughnessFactor;
    if (material.metallicRoughnessTex != 0) {
        vec2 metallicRoughness = sampleTexture(material.metallicRoughnessTex).bg;
        metallic *= metallicRoughness.x;
        roughness *= metallicRoughness.y;
    }

    vec3 N = normalize(normal);
    vec3 V = normalize(cameraPosition - inPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lights.length(); i++) {
        Light light = lights[i];
        vec3 L = normalize(light.position - inPos);
        vec3 H = normalize(V + L);
        float distance = length(light.position - inPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = light.color * attenuation;

        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;


        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.001) * albedo; // * ao
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
    
    // Just to stop annoying validation errors
    _ = vec4(0.0);

    // outColor = texture(textures[nonuniformEXT(material.normalTex)], inUv);
}
