#version 460 core
out vec4 FragColor;

in vec2 TexCoords;

// G-Buffer textures
uniform sampler2D gPosition;      // World Position (xyz) + Material ID (w)
uniform sampler2D gAlbedoMetallic; // Albedo (rgb) + Metallic (a)
uniform sampler2D gNormalRoughness; // Normal (xyz) + Roughness (a)
uniform sampler2D gMotionAO;      // Motion Vector (xy) + AO (z) + unused (w)
uniform sampler2D gEmissive;      // Emissive Color (rgb) + intensity (a)
uniform sampler2D gDepth;         // Depth buffer



// Shadow map
uniform sampler2D shadowMap;
uniform bool enableShadows;
uniform mat4 lightSpaceMatrix;

// Camera
uniform vec3 viewPos;
uniform mat4 view;
uniform mat4 projection;

// Lighting
uniform int numLights;

// Light arrays (max 8 lights)
uniform vec3 lightPositions[8];
uniform vec3 lightColors[8];
uniform vec3 lightDirections[8];
uniform int lightTypes[8];  // 0=directional, 1=point, 2=spot
uniform float lightIntensities[8];
uniform float lightRanges[8];
uniform float lightInnerCones[8];
uniform float lightOuterCones[8];

// PBR constants
const float PI = 3.14159265359;

// PBR functions
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Blocker search for PCSS
float findBlockerDepth(vec2 uv, float zReceiver, float searchRadius)
{
    vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );
    
    float blockerDepthSum = 0.0;
    int numBlockers = 0;
    
    for (int i = 0; i < 16; i++) {
        vec2 sampleUV = uv + poissonDisk[i] * searchRadius;
        float sampleDepth = texture(shadowMap, sampleUV).r;
        
        if (sampleDepth < zReceiver) {
            blockerDepthSum += sampleDepth;
            numBlockers++;
        }
    }
    
    if (numBlockers == 0) {
        return -1.0; // No blockers found
    }
    
    return blockerDepthSum / float(numBlockers);
}

// PCSS penumbra estimation
float penumbraSize(float zReceiver, float zBlocker)
{
    return (zReceiver - zBlocker) / zBlocker;
}

// PCF with variable kernel size
float PCF(vec2 uv, float zReceiver, float filterRadius)
{
    vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );
    
    float shadow = 0.0;
    for (int i = 0; i < 16; i++) {
        vec2 sampleUV = uv + poissonDisk[i] * filterRadius;
        float sampleDepth = texture(shadowMap, sampleUV).r;
        shadow += (sampleDepth >= zReceiver) ? 1.0 : 0.0;
    }
    return shadow / 16.0;
}

// PCSS shadow calculation
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || 
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0; // Outside shadow map bounds
    }
    
    float currentDepth = projCoords.z;
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    float zReceiver = currentDepth - bias;
    
    // Step 1: Blocker search
    float searchRadius = 0.05; // Light size parameter
    float avgBlockerDepth = findBlockerDepth(projCoords.xy, zReceiver, searchRadius);
    
    if (avgBlockerDepth == -1.0) {
        return 1.0; // No blockers, fully lit
    }
    
    // Step 2: Penumbra size estimation
    float penumbraRatio = penumbraSize(zReceiver, avgBlockerDepth);
    float filterRadius = penumbraRatio * searchRadius;
    
    // Step 3: PCF with estimated filter size
    return PCF(projCoords.xy, zReceiver, filterRadius);
}

void main()
{
    // Sample G-Buffer
    vec4 positionData = texture(gPosition, TexCoords);
    vec4 albedoMetallic = texture(gAlbedoMetallic, TexCoords);
    vec4 normalRoughness = texture(gNormalRoughness, TexCoords);
    vec4 motionAO = texture(gMotionAO, TexCoords);
    vec4 emissiveData = texture(gEmissive, TexCoords);
    
    // Skip background pixels
    // if (positionData.w < 0.5) {
    //     FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    //     return;
    // }
    
    // Extract data
    vec3 WorldPos = positionData.xyz;
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 normal = normalize(normalRoughness.xyz * 2.0 - 1.0);
    float roughness = normalRoughness.a;
    float ao = motionAO.z;
    vec3 emissiveColor = emissiveData.rgb * emissiveData.a;
    

    
    vec3 N = normal;
    vec3 V = normalize(viewPos - WorldPos);
    
    // Calculate reflectance at normal incidence
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // Direct lighting calculation (ONLY direct lights, NO ambient/IBL)
    vec3 Lo = vec3(0.0);
    
    for (int i = 0; i < numLights && i < 8; i++) {
        vec3 L;
        float attenuation = 1.0;
        
        if (lightTypes[i] == 0) { // Directional light
            L = normalize(-lightDirections[i]);
        } else if (lightTypes[i] == 1) { // Point light
            L = normalize(lightPositions[i] - WorldPos);
            float distance = length(lightPositions[i] - WorldPos);
            attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
            if (distance > lightRanges[i]) {
                attenuation = 0.0;
            }
        } else if (lightTypes[i] == 2) { // Spot light
            L = normalize(lightPositions[i] - WorldPos);
            float distance = length(lightPositions[i] - WorldPos);
            attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
            
            float theta = dot(L, normalize(-lightDirections[i]));
            float epsilon = lightInnerCones[i] - lightOuterCones[i];
            float intensity = clamp((theta - lightOuterCones[i]) / epsilon, 0.0, 1.0);
            attenuation *= intensity;
            
            if (distance > lightRanges[i]) {
                attenuation = 0.0;
            }
        }
        
        vec3 H = normalize(V + L);
        vec3 radiance = lightColors[i] * lightIntensities[i] * attenuation;
        
        // PBR BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        float NdotL = max(dot(N, L), 0.0);
        
        // Shadow calculation 
        float shadow = 1.0; 
        
        if (i == 0 && lightTypes[i] == 0) {  // Apply shadows to first directional light
            vec4 fragPosLightSpace = lightSpaceMatrix * vec4(WorldPos, 1.0);
            shadow = ShadowCalculation(fragPosLightSpace, N, L);
        }
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
    }
    
    // Output direct lighting 
    vec3 color = Lo + emissiveColor;
    
    // Output direct lighting 
    FragColor = vec4(color, 1.0);
}
