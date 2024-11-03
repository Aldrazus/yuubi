#version 460

const vec2 vertices[] = {
    {-1.0, -3.0},
    {-1.0, 1.0},
    {3.0,  1.0},
};

void main() {
    gl_Position = vec4(vertices[gl_VertexIndex], 1.0, 1.0);
}
