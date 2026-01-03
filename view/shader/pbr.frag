#version 330 core
 
// Basic PBR fragment shader:
// 1) Supports a directional light + multiple point lights (up to 4)
// 2) Uses GGX/NDF + Smith geometry + Schlick Fresnel for specular
// 3) Supports albedo mixing between texture and vertex color
// 4) Supports shadow mapping with PCF sampling
// 5) Applies Reinhard tone mapping and gamma correction at the end
 
out vec4 FragColor;          // Final color output to the framebuffer
 
// Interpolants from the vertex shader
in vec3 FragPos;              // World-space position
in vec3 Normal;               // World-space normal
in vec3 VertexColor;          // Vertex color (modulates with texture)
in vec2 TexCoord;             // Texture coordinates
in vec4 FragPosLightSpace;    // Light-space coordinates used for shadow mapping
 
// Viewer-related
uniform vec3 viewPos;         // Camera position
 
// Directional light parameters
uniform vec3 lightDir;        // Directional light direction (from light towards scene)
uniform vec3 lightColor;      // Directional light color
uniform float lightIntensity; // Directional light intensity (scalar)
 
// Ambient light
uniform vec3 ambientLight;    // Ambient light color/intensity
 
// Point light parameters (up to 4)
#define MAX_POINT_LIGHTS 4
uniform int numPointLights;                           // Number of active point lights
uniform vec3 pointLightPositions[MAX_POINT_LIGHTS];   // Point light positions
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];      // Point light colors
uniform float pointLightIntensities[MAX_POINT_LIGHTS];// Point light intensities
uniform float pointLightRadii[MAX_POINT_LIGHTS];      // Point light effective radii (linear falloff limit)
 
// PBR material parameters
uniform float metallic;       // Metalness [0,1]
uniform float roughness;      // Roughness [0,1]
uniform float ao;             // Ambient occlusion
uniform sampler2D shadowMap;  // Shadow depth map
uniform sampler2D albedoMap;  // Albedo texture
uniform bool useTexture;      // Whether to use the texture (multiplied with vertex color)
 
const float PI = 3.14159265359;
 
// Schlick Fresnel approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
 
// GGX normal distribution function (NDF)
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
 
// Geometry term: Smith-Schlick-GGX single-side term
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}
 
// Geometry term: considers both view and light occlusion
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}
 
// Simple linear attenuation (squared falloff)
float calculateAttenuation(float distance, float radius) {
    float atten = clamp(1.0 - distance / radius, 0.0, 1.0);
    return atten * atten;
}
 
// Shadow calculation: project fragment to light space and do 3x3 PCF blur
float ShadowCalculation(vec4 fragPosLightSpace, vec3 N, vec3 L) {
    // Perspective divide to get normalized coordinates
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // If outside far plane, not in shadow
    if (projCoords.z > 1.0) return 0.0;
    // Dynamic bias to reduce shadow acne
    float bias = max(0.001 * (1.0 - dot(N, L)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    // 3x3 PCF sampling
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
    // 1) Compute albedo: optional texture * vertex color
    vec3 albedo = VertexColor;
    if (useTexture) {
        vec4 texColor = texture(albedoMap, TexCoord);
        albedo = texColor.rgb * VertexColor; // Modulate with vertex color
    }

    // 2) Base vectors
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    // Metalness blending: metals use their albedo for F0, dielectrics use a constant 0.04
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);
 
    // 3) Directional light (with shadows)
    vec3 directionalL = normalize(-lightDir);
    vec3 directionalH = normalize(V + directionalL);
    float dirNDF = DistributionGGX(N, directionalH, roughness);
    float dirG = GeometrySmith(N, V, directionalL, roughness);
    vec3 dirF = fresnelSchlick(max(dot(directionalH, V), 0.0), F0);
    vec3 dirSpecular = dirNDF * dirG * dirF;
    float dirDenom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, directionalL), 0.0) + 0.0001;
    dirSpecular /= dirDenom;
    vec3 dirkS = dirF;                              // Specular energy portion
    vec3 dirkD = (vec3(1.0) - dirkS) * (1.0 - metallic); // Diffuse portion (only for non-metallic)
    float dirNdotL = max(dot(N, directionalL), 0.0);
    float shadow = ShadowCalculation(FragPosLightSpace, N, directionalL);
    vec3 directionalContribution = (dirkD * albedo / PI + dirSpecular) * lightColor * lightIntensity * dirNdotL;
    Lo += directionalContribution * (1.0 - shadow);
 
    // 4) Point lights loop
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
 
    // 5) Ambient + combine + tone mapping + gamma correction
    vec3 ambient = ambientLight * albedo * ao;
    vec3 color = ambient + Lo;
    // Reinhard tone mapping
    color = color / (color + vec3(1.0));
    // Gamma correction (gamma=2.2)
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
