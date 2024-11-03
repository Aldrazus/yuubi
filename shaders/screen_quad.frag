#version 460

layout (input_attachment_index = 0, binding = 0) uniform subpassInput colorAttachment;

layout (location = 0) out vec4 outColor;

void main() {
    outColor = subpassLoad(colorAttachment);
}
