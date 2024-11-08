#version 460

layout (location = 0) out vec2 texCoords;

const vec2 vertices[] = {
    {-1.0, -1.0},
    {3.0, -1.0},
    {-1.0,  3.0},
};

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 1.0, 1.0);
    texCoords = 0.5 * gl_Position.xy + vec2(0.5);
}
