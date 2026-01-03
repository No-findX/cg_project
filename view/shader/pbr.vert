#version 330 core

// Basic PBR vertex shader:
// 1) Receive mesh vertex attributes (position/normal/color/texcoord)
// 2) Transform vertices to world space and clip space
// 3) Compute light-space coordinates for shadow mapping (FragPosLightSpace)

// Vertex attribute inputs: match layout(location) used by GameRenderer
layout(location = 0) in vec3 aPos;       // Vertex position in model space
layout(location = 1) in vec3 aNormal;    // Normal in model space
layout(location = 2) in vec3 aColor;     // Vertex color (can be used for base tint or debug)
layout(location = 3) in vec2 aTexCoord;  // Texture coordinates

// Interpolants passed to the fragment shader
out vec3 FragPos;            // World-space position
out vec3 Normal;             // World-space normal
out vec3 VertexColor;        // Vertex color
out vec2 TexCoord;           // Texture coordinates
out vec4 FragPosLightSpace;  // Light-space coordinates for shadow sampling

// Transformation matrix uniforms
uniform mat4 model;             // Model matrix: model space -> world space
uniform mat4 view;              // View matrix: world space -> view space
uniform mat4 projection;        // Projection matrix: view space -> clip space
uniform mat4 lightSpaceMatrix;  // Light projection-view matrix: world space -> light clip space (for shadows)

// For portals
uniform vec4 clipPlane0;
uniform bool enableClip0;

// For passing through portals
uniform vec4 clipPlane1;
uniform bool enableClip1;

void main() {
    // 1) Transform vertex position from model space to world space
    vec4 worldPos4dim = model * vec4(aPos, 1.0f);
    FragPos = vec3(worldPos4dim);
    
    // 2) Compute world-space normal
    //    Use the normal matrix mat3(transpose(inverse(model))) to handle non-uniform scaling correctly
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    // 3) Pass color and texture coordinates to the fragment shader
    VertexColor = aColor;
    TexCoord = aTexCoord;
    
    // 4) Compute light-space coordinates for shadow mapping
    FragPosLightSpace = lightSpaceMatrix * worldPos4dim;
    
    // 5) Compute final clip-space position for rasterization
    gl_Position = projection * view * worldPos4dim;

    // Clipping Planes
    gl_ClipDistance[0] = enableClip0 ? dot(worldPos4dim, clipPlane0) : 1.0;
    gl_ClipDistance[1] = enableClip1 ? dot(worldPos4dim, clipPlane1) : 1.0;
}
