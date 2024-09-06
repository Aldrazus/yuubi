#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D textures;

void main() {
    // outColor = texture(textures, fragUv);
    outColor = vec4(fragColor, 1.0);
}
