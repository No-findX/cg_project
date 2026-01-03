#version 330 core
 
out vec4 FragColor;
 
in vec3 FragPos;
in vec3 Normal;
in vec3 VertexColor;
in vec2 TexCoord;
in vec4 FragPosLightSpace;
 
uniform vec3 viewPos;
 
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform float lightIntensity;
 
uniform vec3 ambientLight;
 
#define MAX_POINT_LIGHTS 4
uniform int numPointLights;
uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];
uniform float pointLightIntensities[MAX_POINT_LIGHTS];
uniform float pointLightRadii[MAX_POINT_LIGHTS];
 
uniform float metallic;
uniform float roughness;
uniform float ao;
uniform sampler2D shadowMap;
uniform sampler2D albedoMap;
uniform bool useTexture;
 
const float PI = 3.14159265359;
 
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
 
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
 
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}
 
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}
 
float calculateAttenuation(float distance, float radius) {
    float atten = clamp(1.0 - distance / radius, 0.0, 1.0);
    return atten * atten;
}
 
float ShadowCalculation(vec4 fragPosLightSpace, vec3 N, vec3 L) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    float bias = max(0.001 * (1.0 - dot(N, L)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
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
    vec3 albedo = VertexColor;
    if (useTexture) {
        vec4 texColor = texture(albedoMap, TexCoord);
        albedo = texColor.rgb * VertexColor;
    }

    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);
 
    vec3 directionalL = normalize(-lightDir);
    vec3 directionalH = normalize(V + directionalL);
    float dirNDF = DistributionGGX(N, directionalH, roughness);
    float dirG = GeometrySmith(N, V, directionalL, roughness);
    vec3 dirF = fresnelSchlick(max(dot(directionalH, V), 0.0), F0);
    vec3 dirSpecular = dirNDF * dirG * dirF;
    float dirDenom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, directionalL), 0.0) + 0.0001;
    dirSpecular /= dirDenom;
    vec3 dirkS = dirF;
    vec3 dirkD = (vec3(1.0) - dirkS) * (1.0 - metallic);
    float dirNdotL = max(dot(N, directionalL), 0.0);
    float shadow = ShadowCalculation(FragPosLightSpace, N, directionalL);
    vec3 directionalContribution = (dirkD * albedo / PI + dirSpecular) * lightColor * lightIntensity * dirNdotL;
    Lo += directionalContribution * (1.0 - shadow);
 
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
 
    vec3 ambient = ambientLight * albedo * ao;
    vec3 color = ambient + Lo;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}