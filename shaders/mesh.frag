#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "push_constants.glsl"
#include "bindless.glsl"

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUv;

layout(location = 0) out vec4 outColor;

void main() {
    MaterialData material = PushConstants.sceneData.materials.data[PushConstants.materialId];

    outColor = material.albedoFactor;
    if (material.albedoTex != 0) {
        outColor *= texture(textures[nonuniformEXT(material.albedoTex)], inUv);
    }
}
