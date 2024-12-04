#version 460

layout (location = 0) in vec3 worldPos;

layout (location = 0) out vec4 fragColor;

layout (set = 0, binding = 0) uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main() {
    // TODO: Figure out cubemap sampling in Vulkan's right-handed coordinate system.
    // Vulkan has a right-handed coordinate system where +X points to the right, +Z points into the screen,
    // and +Y points downward. The viewport height is negative in every pipeline, so +Y actually points upwards.
    // However, this DOESN'T change the texture coordinate space. This is why we have to do these transformations
    // here.
    vec3 normal = normalize(vec3(worldPos.x, -worldPos.y, -worldPos.z));

    vec3 irradiance = vec3(0.0);

    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = normalize(cross(normal, right));

    float sampleDelta = 0.025;
    float numSamples = 0.0;
    // Sample around the entire equator (2 * pi).
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        // Sample around the northern hemisphere (0.5 * pi).
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // Spherical to cartesian (tangent space).
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

            // Tangent space to world space.
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
            irradiance += texture(environmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            numSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(numSamples));
    fragColor = vec4(irradiance, 1.0);
    //fragColor = vec4(texture(environmentMap, normal).rgb, 1.0);
}
