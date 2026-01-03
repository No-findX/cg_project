#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aTex;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// For portals
uniform vec4 clipPlane0;
uniform bool enableClip0;

// For passing through portals
uniform vec4 clipPlane1;
uniform bool enableClip1;

out vec3 vColor;
out vec2 vTex;

void main() {
    vec4 worldPos4dim = model * vec4(aPos, 1.0f);
    
    vColor = aColor;
    vTex = aTex;
    gl_Position = projection * view * worldPos4dim;

    gl_ClipDistance[0] = enableClip0 ? dot(worldPos4dim, clipPlane0) : 1.0;
    gl_ClipDistance[1] = enableClip1 ? dot(worldPos4dim, clipPlane1) : 1.0;
}