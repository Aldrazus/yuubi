#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "push_constants.glsl"
#include "bindless.glsl"

layout (location = 0) in vec2 inUv;

// Must disable early fragment tests in order to discard masked fragments
// PERF: perform early fragment tests for opaque surfaces
// layout (early_fragment_tests) in;

vec4 sampleTexture(uint index) {
    return texture(textures[nonuniformEXT(index)], inUv);
}

void main() {
    MaterialData material = PushConstants.sceneData.materials.data[PushConstants.materialId];
    float alpha = material.albedoFactor.a;
    if (material.albedoTex != 0) {
        vec4 sampledAlbedo = sampleTexture(material.albedoTex);
        alpha *= sampledAlbedo.a;
    }
    if (alpha < material.alphaCutoff) discard;
}
