#version 330 core

uniform sampler2D colorSampler;
in vec2 texCoord;

out vec4 fragColor;

void main() {
    fragColor = texture(colorSampler, texCoord);
}