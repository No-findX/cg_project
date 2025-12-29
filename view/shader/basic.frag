#version 330 core

in vec3 vColor;
in vec2 vTex;

out vec4 FragColor;

uniform sampler2D Texture;

void main() {
    float alpha = texture(Texture, vTex).w;
    //FragColor = mix(vec4(vColor, 1.0), vec4(1.0, 1.0, 1.0, 1.0), alpha);
    FragColor = vec4(vColor, 1.0) * texture(Texture, vTex);
}