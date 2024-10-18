#ifndef UB_MATERIALS
#define UB_MATERIALS

#extension GL_EXT_buffer_reference : require

struct MaterialData {
    vec4 albedo;
    uint albedoTex;

    float metallicFactor;
    float roughnessFactor;
    uint metallicRoughnessTex;
};

layout (buffer_reference, std430) readonly buffer MaterialsBuffer {
    MaterialData data[];
};

#endif
