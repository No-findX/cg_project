#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

// For passing through portals
uniform vec4 clipPlane1;
uniform bool enableClip1;

void main()
{
    vec4 worldPos4dim = model * vec4(aPos, 1.0);
    gl_Position = lightSpaceMatrix * worldPos4dim;

    // Clipping for portals
    gl_ClipDistance[1] = enableClip1 ? dot(worldPos4dim, clipPlane1) : 1.0;
}
