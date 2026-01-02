#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec2 aNormal; // XZ 平面内的面法线（顶/底为 0,0）
layout(location = 3) in vec3 aTex;    // aTex.xy: 相对中心偏移[-1,1]；aTex.z: halfW(世界单位)

uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

uniform vec2 direction; // XZ 运动方向（单位向量或(0,0)）
uniform float moveT;    // 0..1
uniform float idleT;    // 0..1

out vec3 FragPos;
out vec3 Normal;
out vec3 VertexColor;
out vec4 FragPosLightSpace;
out vec2 TexCoord;

// 待机 Squash & Stretch：XZ 径向膨胀/收缩，Y 反向微缩放
vec3 idlePos(vec3 p, vec3 tex, float t) {
    // 相位：sin(2πt)
    const float TWO_PI = 6.28318530718;
    float phase = sin(TWO_PI * t);

    // 振幅（可与美术调参一致）
    const float amp = 0.05;    // 横向膨胀强度
    const float yRatio = 0.5;  // Y 缩放相对强度

    float halfW = tex.z;
    if (halfW < 1e-6) return p;

    // 顶点的中心坐标（通过 aTex.xy 和当前 aPos.xz 重建）
    vec2 rel = tex.xy * halfW;   // 顶点相对中心的 XZ 偏移
    vec2 centerXZ = p.xz - rel;  // 推回中心

    // 径向权重（边缘更明显）
    float r = clamp(length(tex.xy), 0.0, 1.0);
    float w = r * r;

    // 横向缩放因子
    float s = 1.0 + amp * phase * w;

    vec3 outPos = p;
    outPos.x  = centerXZ.x + rel.x * s;
    outPos.z  = centerXZ.y + rel.y * s;
    outPos.y  = p.y + (-amp * yRatio * phase) * w * halfW * 0.2; // 轻微上下起伏
    return outPos;
}

// 运动形变：前半程压缩、侧/顶展宽、后向挤压；后半程恢复
vec3 movingPos(vec3 p, vec2 n2, vec3 tex, vec2 dir, float t) {
    // 静止或无方向：不形变
    if (t <= 0.0 || t >= 1.0) return p;
    float dlen = length(dir);
    if (dlen < 1e-5) return p;

    vec2 ndir = dir / dlen;

    // 对称三角波 0->1->0（压缩/恢复）
    float tri = 1.0 - abs(1.0 - 2.0 * t);
    float k = smoothstep(0.0, 1.0, tri);

    float nlen = length(n2);
    vec2 fn = (nlen > 1e-5) ? (n2 / nlen) : vec2(0.0);
    float backw  = max(0.0, -dot(ndir, fn));                   // 后面
    float sidew  = (nlen > 1e-5) ? max(0.0, 1.0 - abs(dot(ndir, fn))) : 0.0;
    float topw   = (nlen < 1e-5) ? 1.0 : 0.0;                  // 顶/底（无XZ法线）

    float halfW = tex.z;
    if (halfW < 1e-6) return p;

    vec2 rel = tex.xy * halfW;
    vec2 centerXZ = p.xz - rel;

    // 顶点沿运动方向的相对位置（0:后 -> 1:前）
    float along = clamp((dot(ndir, tex.xy) + 1.0) * 0.5, 0.0, 1.0);
    float distFromFront = 1.0 - along;

    // 后向压缩，前脸保持不动
    float compAmount = 0.15 * k; // 最大 25% halfW
    vec2 shiftBack = ndir * (compAmount * halfW * distFromFront * backw);

    // 侧/顶展宽（与压缩量相关）
    float widenAmount = 0.10 * k;
    float widenFactor = 1.0 + widenAmount * (sidew + topw);
    vec2 widened = centerXZ + rel * widenFactor;

    vec3 outPos = p;
    outPos.xz = widened + shiftBack;
    return outPos;
}

void main() {
    // 先应用待机形变
    vec3 pIdle = idlePos(aPos, aTex, idleT);
    // 再应用运动形变（若运动中）
    vec3 p = movingPos(pIdle, aNormal, aTex, direction, moveT);

    FragPos = p;
    
    // 简单法线估算：侧面使用 aNormal，顶底面默认向上
    vec3 N = vec3(0.0, 1.0, 0.0);
    if (length(aNormal) > 0.001) {
        N = vec3(aNormal.x, 0.0, aNormal.y);
    }
    Normal = normalize(N);
    
    VertexColor = aColor;
    TexCoord = vec2(0.0); // 软立方体暂无纹理坐标
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

    gl_Position = projection * view * vec4(p, 1.0);
}