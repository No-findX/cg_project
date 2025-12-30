#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec4 ScreenPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// For portals
uniform vec4 clipPlane;
uniform bool enableClip;

void main() {
	vec4 worldPos4dim = model * vec4(aPos, 1.0f);

	gl_Position = projection * view * worldPos4dim;
	ScreenPos = gl_Position;

	if (enableClip) {
        gl_ClipDistance[0] = dot(worldPos4dim, clipPlane);
    }
}