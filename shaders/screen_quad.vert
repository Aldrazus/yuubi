#version 460

layout (location = 0) out vec2 texCoords;

void main() {
    texCoords = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(texCoords * 2.0 - 1.0, 0.0, 1.0);
    texCoords.y = 1 - texCoords.y;
}
