#ifndef UB_MATERIALS
#define UB_MATERIALS

#extension GL_EXT_buffer_reference : require

struct MaterialData {
    uint normalTex;
    float normalScale;

    uint albedoTex;
    uint pad0;
    vec4 albedoFactor;

    uint metallicRoughnessTex;
    float metallicFactor;
    float roughnessFactor;

    uint pad1;
};

layout (buffer_reference, std430) readonly buffer MaterialsBuffer {
    MaterialData data[];
};

#endif
