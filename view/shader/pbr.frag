#version 330 core
 
// 基础 PBR 片元着色器：
// 1) 支持方向光 + 多个点光源（最多 4 个）
// 2) 使用 GGX/NDF + Smith 几何项 + Schlick 菲涅尔计算镜面
// 3) 支持纹理与顶点色混合的反照率；
// 4) 支持阴影贴图（PCF 采样）；
// 5) 最后做 Reinhard 色调映射 + 伽马校正。
 
out vec4 FragColor;          // 输出到帧缓的最终颜色
 
// 顶点着色器插值输入
in vec3 FragPos;              // 世界空间位置
in vec3 Normal;               // 世界空间法线
in vec3 VertexColor;          // 顶点颜色（与纹理调制）
in vec2 TexCoord;             // 纹理坐标
in vec4 FragPosLightSpace;    // 光源裁剪空间坐标，用于阴影贴图
 
// 观察者相关
uniform vec3 viewPos;         // 摄像机位置
 
// 方向光参数
uniform vec3 lightDir;        // 方向光方向（从光源指向场景）
uniform vec3 lightColor;      // 方向光颜色
uniform float lightIntensity; // 方向光强度（标量）
 
// 环境光
uniform vec3 ambientLight;    // 环境光颜色/强度
 
// 点光源参数（最多 4 个）
#define MAX_POINT_LIGHTS 4
uniform int numPointLights;                           // 启用的点光源数量
uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];   // 点光源位置
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];      // 点光源颜色
uniform float pointLightIntensities[MAX_POINT_LIGHTS];// 点光源强度
uniform float pointLightRadii[MAX_POINT_LIGHTS];      // 点光源作用半径（线性衰减上限）
 
// PBR 材质参数
uniform float metallic;       // 金属度 [0,1]
uniform float roughness;      // 粗糙度 [0,1]
uniform float ao;             // 环境遮蔽 (Ambient Occlusion)
uniform sampler2D shadowMap;  // 阴影深度贴图
uniform sampler2D albedoMap;  // 反照率贴图
uniform bool useTexture;      // 是否启用纹理（与顶点色相乘）
 
const float PI = 3.14159265359;
 
// 菲涅尔 Schlick 近似
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
 
// GGX 法线分布函数 (NDF)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}
 
// 几何项：Smith-Schlick-GGX 单边项
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}
 
// 几何项：同时考虑视线与光线遮挡
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}
 
// 简单的线性衰减（平方衰减）
float calculateAttenuation(float distance, float radius) {
    float atten = clamp(1.0 - distance / radius, 0.0, 1.0);
    return atten * atten;
}
 
// 阴影计算：将片元投影到光源空间并做 3x3 PCF 软化
float ShadowCalculation(vec4 fragPosLightSpace, vec3 N, vec3 L) {
    // 透视除法得到标准化坐标
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // 转换到 [0,1] 范围
    projCoords = projCoords * 0.5 + 0.5;
    // 超出远平面则不受阴影影响
    if (projCoords.z > 1.0) return 0.0;
    // 动态偏移以减少阴影痤疮
    float bias = max(0.001 * (1.0 - dot(N, L)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    // 3x3 PCF 采样
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}
 
void main() {
    // 1) 计算反照率：可选纹理 * 顶点色
    vec3 albedo = VertexColor;
    if (useTexture) {
        vec4 texColor = texture(albedoMap, TexCoord);
        albedo = texColor.rgb * VertexColor; // 与顶点色调制
    }

    // 2) 基础向量
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    // 金属度混合：金属使用自身反射率，非金属用 0.04 的常量 F0
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);
 
    // 3) 方向光计算（含阴影）
    vec3 directionalL = normalize(-lightDir);
    vec3 directionalH = normalize(V + directionalL);
    float dirNDF = DistributionGGX(N, directionalH, roughness);
    float dirG = GeometrySmith(N, V, directionalL, roughness);
    vec3 dirF = fresnelSchlick(max(dot(directionalH, V), 0.0), F0);
    vec3 dirSpecular = dirNDF * dirG * dirF;
    float dirDenom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, directionalL), 0.0) + 0.0001;
    dirSpecular /= dirDenom;
    vec3 dirkS = dirF;                              // 镜面能量分配
    vec3 dirkD = (vec3(1.0) - dirkS) * (1.0 - metallic); // 漫反射部分（非金属才有）
    float dirNdotL = max(dot(N, directionalL), 0.0);
    float shadow = ShadowCalculation(FragPosLightSpace, N, directionalL);
    vec3 directionalContribution = (dirkD * albedo / PI + dirSpecular) * lightColor * lightIntensity * dirNdotL;
    Lo += directionalContribution * (1.0 - shadow);
 
    // 4) 点光源循环
    for (int i = 0; i < numPointLights; ++i) {
        vec3 lightVec = pointLightPositions[i] - FragPos;
        float distance = length(lightVec);
        vec3 L = normalize(lightVec);
        vec3 H = normalize(V + L);
        float attenuation = calculateAttenuation(distance, pointLightRadii[i]);
        vec3 radiance = pointLightColors[i] * pointLightIntensities[i] * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
 
    // 5) 环境光 + 合并 + 色调映射 + 伽马校正
    vec3 ambient = ambientLight * albedo * ao;
    vec3 color = ambient + Lo;
    // Reinhard tone mapping
    color = color / (color + vec3(1.0));
    // Gamma 校正 (gamma=2.2)
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
