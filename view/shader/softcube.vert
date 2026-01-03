#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aNormal; // Normal within XZ plane (top/bottom are 0,0)
layout(location = 3) in vec3 aTex;    // aTex.xy: relative offset from center [-1,1]; aTex.z: halfW (world units)

uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

uniform vec2 direction; // Movement direction in XZ plane (unit vector or (0,0))
uniform float moveT;    // 0..1
uniform float idleT;    // 0..1

// For portals
uniform vec4 clipPlane0;
uniform bool enableClip0;

// For passing through portals
uniform vec4 clipPlane1;
uniform bool enableClip1;

out vec3 FragPos;
out vec3 Normal;
out vec3 VertexColor;
out vec4 FragPosLightSpace;
out vec2 TexCoord;

// Idle Squash & Stretch: radial expansion/contraction in XZ, inverse small scale in Y
vec3 idlePos(vec3 p, vec3 tex, float t) {
    // Phase: sin(2πt)
    const float TWO_PI = 6.28318530718;
    float phase = sin(TWO_PI * t);

    // Amplitude (kept consistent with art damping)
    const float amp = 0.05;    // Horizontal expansion strength
    const float yRatio = 0.5;  // Relative strength for Y scale

    float halfW = tex.z;
    if (halfW < 1e-6) return p;

    // Reconstruct vertex center using aTex.xy and current aPos.xz
    vec2 rel = tex.xy * halfW;   // Vertex offset from center in XZ
    vec2 centerXZ = p.xz - rel;  // Push back to center

    // Radial weight (more pronounced at edges)
    float r = clamp(length(tex.xy), 0.0, 1.0);
    float w = r * r;

    // Horizontal scale factor
    float s = 1.0 + amp * phase * w;

    vec3 outPos = p;
    outPos.x  = centerXZ.x + rel.x * s;
    outPos.z  = centerXZ.y + rel.y * s;
    outPos.y  = p.y + (-amp * yRatio * phase) * w * halfW * 0.2; // Slight up & down bobbing
    return outPos;
}

// Movement deformation: compress during first half
// (Expand on sides / top, compress towards back)
// Recover during the second half
vec3 movingPos(vec3 p, vec2 n2, vec3 tex, vec2 dir, float t) {
    // Stationary / No direction: no deformation
    if (t <= 0.0 || t >= 1.0) return p;
    float dlen = length(dir);
    if (dlen < 1e-5) return p;

    vec2 ndir = dir / dlen;

    // Symmetrical triangular wave 0->1->0 (compress -> recover)
    float tri = 1.0 - abs(1.0 - 2.0 * t);
    float k = smoothstep(0.0, 1.0, tri);

    float nlen = length(n2);
    vec2 fn = (nlen > 1e-5) ? (n2 / nlen) : vec2(0.0);
    float backw  = max(0.0, -dot(ndir, fn));                   // Back
    float sidew  = (nlen > 1e-5) ? max(0.0, 1.0 - abs(dot(ndir, fn))) : 0.0;
    float topw   = (nlen < 1e-5) ? 1.0 : 0.0;                  // Top / Bottom (no normal in X/Z directions)

    float halfW = tex.z;
    if (halfW < 1e-6) return p;

    vec2 rel = tex.xy * halfW;
    vec2 centerXZ = p.xz - rel;

    // Relative position along moving direction (0: back -> 1: front)
    float along = clamp((dot(ndir, tex.xy) + 1.0) * 0.5, 0.0, 1.0);
    float distFromFront = 1.0 - along;

    // Compress backwards, keep front face stationary
    float compAmount = 0.15 * k; // At max 25% halfW
    vec2 shiftBack = ndir * (compAmount * halfW * distFromFront * backw);

    // Widen width for sides / front (related to compression)
    float widenAmount = 0.10 * k;
    float widenFactor = 1.0 + widenAmount * (sidew + topw);
    vec2 widened = centerXZ + rel * widenFactor;

    vec3 outPos = p;
    outPos.xz = widened + shiftBack;
    return outPos;
}

void main() {
    // Apply idle deformation first
    vec3 pIdle = idlePos(aPos, aTex, idleT);
    // Then apply movement deformation (if moving)
    vec3 p = movingPos(pIdle, aNormal, aTex, direction, moveT);

    FragPos = p;
    
    // Simple normal estimation: use aNormal for sides and default upwards for top / bottom
    vec3 N = vec3(0.0, 1.0, 0.0);
    if (length(aNormal) > 0.001) {
        N = vec3(aNormal.x, 0.0, aNormal.y);
    }
    Normal = normalize(N);
    
    VertexColor = aColor;
    TexCoord = vec2(0.0); // No TexCoord for soft cube yet
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

    gl_Position = projection * view * vec4(p, 1.0);

    // Clipping for portals
    vec4 worldPos4dim = vec4(p, 1.0);
    gl_ClipDistance[0] = enableClip0 ? dot(worldPos4dim, clipPlane0) : 1.0;
    gl_ClipDistance[1] = enableClip1 ? dot(worldPos4dim, clipPlane1) : 1.0;
}
