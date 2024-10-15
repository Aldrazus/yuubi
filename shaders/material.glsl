#ifndef UB_MATERIALS
#define UB_MATERIALS

#extension GL_EXT_buffer_reference : require

struct MaterialData {
    vec4 baseColor;
    uint diffuseTex;
    uint metallicRoughnessTex;
    uint padding[2];
};

layout (buffer_reference, std430) readonly buffer MaterialsBuffer {
    MaterialData data[];
};

#endif
