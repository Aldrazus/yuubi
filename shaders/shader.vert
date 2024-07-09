#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUv;

layout(push_constant) uniform constants {
    mat4 mvp;
} PushConstants;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = PushConstants.mvp * vec4(inPosition, 1.0f);
    fragColor = inColor;
}
