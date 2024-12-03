#version 460

#extension GL_EXT_multiview : require
layout (location = 0) in vec3 worldPos;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 sampleSphericalMap(vec3 v) {
    return invAtan * vec2(atan(v.z, v.x), asin(v.y)) + 0.5;
}

void main() {
    vec2 uv = sampleSphericalMap(normalize(worldPos));
    vec3 color = texture(equirectangularMap, uv).rgb;
    outColor = vec4(color, 1.0);
    //outColor = vec4(worldPos, 1.0);

}