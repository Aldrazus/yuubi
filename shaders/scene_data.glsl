#ifndef UB_SCENE_DATA
#define UB_SCENE_DATA

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

layout (buffer_reference, scalar) readonly buffer SceneDataBuffer {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec4 ambientColor;
    vec4 sunlightDirection; // w for sun power
    vec4 sunlightColor;
} sceneDataBuffer;

#endif
