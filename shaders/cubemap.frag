#version 460

#extension GL_EXT_multiview : require
layout (location = 0) in vec2 texCoords;

layout (location = 0) out vec4 outColor;

// layout (set = 0, binding = 0) uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 sampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

vec3 getWorldPosition(vec2 coord) {
    switch (gl_ViewIndex) {
        case 0U: return normalize(vec3(1.0, texCoords.y, -texCoords.x));
        case 1U: return normalize(vec3(-1.0, texCoords.yx));
        case 2U: return normalize(vec3(texCoords.x, -1.0, texCoords.y));
        case 3U: return normalize(vec3(texCoords.x, 1.0, -texCoords.y));
        case 4U: return normalize(vec3(texCoords, 1.0));
        case 5U: return normalize(vec3(-texCoords.x, texCoords.y, -1.0));
    }
    // Unreachable
    return vec3(0);
}

void main() {
    vec3 worldPos = getWorldPosition(texCoords);
    vec2 uv = sampleSphericalMap(worldPos);
    // vec3 color = texture(equirectangularMap, uv).rgb;
    //outColor = vec4(color, 1.0);
    outColor = vec4(1.0, 0.0, 0.0, 1.0);

}