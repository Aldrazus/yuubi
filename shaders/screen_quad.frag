#version 460
#extension GL_EXT_samplerless_texture_functions : require

layout (set = 0, binding = 0) uniform texture2D drawImage;

layout (location = 0) out vec4 outColor;

void main() {
    outColor = texelFetch(drawImage, ivec2(gl_FragCoord.xy), 0);

    // Tone mapping (no gamma correction as the swapchain image is in sRGB
    outColor.xyz = outColor.xyz / (outColor.xyz + vec3(1.0));
}
