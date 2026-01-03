#version 330 core
out vec4 FragColor;

in vec4 ScreenPos;

uniform sampler2D portalTexture;

void main() {
    vec2 ndc = ScreenPos.xy / ScreenPos.w;

    vec2 uv = ndc * 0.5 + 0.5;

    FragColor = texture(portalTexture, uv);
}