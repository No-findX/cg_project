#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTex;

uniform mat4 view;
uniform mat4 projection;

out vec3 vColor;
out vec2 vTex;

void main() {
    vColor = aColor;
    vTex = aTex;
    gl_Position = projection * view * vec4(aPos, 1.0);
}