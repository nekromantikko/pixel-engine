#version 330 core

uniform vec4 quad; // x, y, w, h

out vec2 texCoord;

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
    vec2 pos = positions[gl_VertexID] * vec2(quad.z, quad.w) + vec2(quad.x, quad.y);
    gl_Position = vec4(pos, 0.0, 1.0);
    texCoord = uv[gl_VertexID];
}