#version 450

layout(push_constant) uniform Quad
{
    float x, y, w, h;
} quad;

layout(location = 0) out vec2 texCoord;

vec2 positions[4] = vec2[](
    vec2(-1.0, 1.0),
    vec2(-1.0, -1.0),
    vec2(1.0, 1.0),
    vec2(1.0, -1.0)
);

vec2 uv[4] = vec2[](
    vec2(0.0, 1.0),
    vec2(0.0, 0.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);

void main() {
    vec2 pos = positions[gl_VertexIndex] * vec2(quad.w, quad.h) + vec2(quad.x, quad.y);
    gl_Position = vec4(pos, 0.0, 1.0);
    texCoord = uv[gl_VertexIndex];
}