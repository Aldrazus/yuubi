#version 450
#extension GL_EXT_buffer_reference : require

struct Vertex {
    vec3 position;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};


layout(push_constant) uniform constants {
    mat4 mvp;
    VertexBuffer vertexBuffer;
} PushConstants;

layout(location = 0) out vec3 fragColor;

void main() {
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];


    gl_Position = PushConstants.mvp * vec4(v.position, 1.0f);
    fragColor = v.color.xyz;
}
