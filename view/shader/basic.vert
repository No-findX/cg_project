#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// For portals
uniform vec4 clipPlane;
uniform bool enableClip;

out vec3 vColor;

void main() {
    vec4 worldPos4dim = model * vec4(aPos, 1.0f);
    
    vColor = aColor;
    gl_Position = projection * view * worldPos4dim;

    if (enableClip) {
        gl_ClipDistance[0] = dot(worldPos4dim, clipPlane);
    }
}