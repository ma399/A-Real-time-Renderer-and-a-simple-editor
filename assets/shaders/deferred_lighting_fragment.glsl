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

// IBL textures
uniform samplerCube irradianceMap;
uniform samplerCube prefilteredMap;
uniform bool useIBL;

// Camera
uniform vec3 viewPos;
uniform mat4 view;
uniform mat4 projection;

// Lighting
uniform vec3 ambientLight;
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
    
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float blockerDepthSum = 0.0;
    int blockerCount = 0;
    
    for(int i = 0; i < 16; ++i)
    {
        vec2 offset = poissonDisk[i] * texelSize * searchRadius;
        float shadowDepth = texture(shadowMap, uv + offset).r;
        
        if(shadowDepth < zReceiver)
        {
            blockerDepthSum += shadowDepth;
            blockerCount++;
        }
    }
    
    if(blockerCount == 0)
        return -1.0;  
    
    return blockerDepthSum / float(blockerCount);
}

// Calculate penumbra size for PCSS
float calculatePenumbraSize(float zReceiver, float zBlocker, float lightSize)
{
    if(zBlocker < 0.0) return 0.0;  
 
    return (zReceiver - zBlocker) * lightSize / zBlocker;
}

// PCSS Shadow calculation
float PCSSShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
 
    if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0)
        return 0.0;
    
    float currentDepth = projCoords.z;
    
    // Blocker search
    float searchRadius = 10.0;  
    float blockerDepth = findBlockerDepth(projCoords.xy, currentDepth, searchRadius);
    
    if(blockerDepth < 0.0)
        return 0.0;  
    
    // Penumbra size calculation
    float penumbraSize = calculatePenumbraSize(currentDepth, blockerDepth, 5.0);
    float bias = 0.005;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    // 25 samples for higher quality
    vec2 poissonDisk[25] = vec2[](
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
        vec2(0.14383161, -0.14100790),
        vec2(-0.65465631, 0.77613958),
        vec2(0.87912464, 0.81544232),
        vec2(-0.47373420, -0.53742981),
        vec2(0.41893023, 0.26496911),
        vec2(-0.19090188, -0.79197514),
        vec2(0.99706507, 0.24188840),
        vec2(-0.91437590, -0.81409955),
        vec2(0.78641367, -0.19984126),
        vec2(0.14100790, 0.14383161)
    );
    
    for(int i = 0; i < 25; ++i)
    {
        vec2 offset = poissonDisk[i] * texelSize * penumbraSize;
        float pcfDepth = texture(shadowMap, projCoords.xy + offset).r; 
        shadow += currentDepth - bias > pcfDepth ? 0.0 : 1.0;
    }
    shadow /= 25.0;
    
    return shadow;
}

// Legacy shadow calculation (kept for compatibility)
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    return PCSSShadowCalculation(fragPosLightSpace);
}

// Light attenuation calculation
float calculateAttenuation(int lightType, vec3 lightPos, vec3 fragPos, float lightRange)
{
    if (lightType == 0) { // Directional light
        return 1.0;
    }
    
    float distance = length(lightPos - fragPos);
    
    if (lightType == 1) { // Point light
        return 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    }
    
    // Spot light (type 2) - simplified
    return max(0.0, 1.0 - distance / lightRange);
}

void main()
{
    
    // Sample G-Buffer
    vec4 gPos = texture(gPosition, TexCoords);
    vec4 gAlbedoMetal = texture(gAlbedoMetallic, TexCoords);
    vec4 gNormRough = texture(gNormalRoughness, TexCoords);
    vec4 gMotionAOData = texture(gMotionAO, TexCoords);
    vec4 gEmissiveData = texture(gEmissive, TexCoords);

    // Extract data from G-Buffer 
    vec3 WorldPos = gPos.xyz;          // Position from RT0
    vec3 albedo = gAlbedoMetal.rgb;    // Albedo from RT1.rgb
    float metallic = gAlbedoMetal.a;   // Metallic from RT1.a
    
    // Decode normal from [0,1] to [-1,1]
    vec3 normal = gNormRough.xyz * 2.0 - 1.0;  // Normal from RT2.rgb
    float roughness = gNormRough.a;    // Roughness from RT2.a
    
    float ao = gMotionAOData.z;        // AO from RT3.z
    vec3 emissiveColor = gEmissiveData.rgb;  // Emissive color from RT4.rgb
    float emissiveIntensity = gEmissiveData.a;  // Emissive intensity from RT4.a
    

    
    // View direction
    vec3 V = normalize(viewPos - WorldPos);
    vec3 N = normalize(normal);
    
    // Calculate base reflectance F0
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Outgoing radiance
    vec3 Lo = vec3(0.0);
    

    
    // Calculate lighting for each light
    for(int i = 0; i < min(numLights, 8); ++i)
    {
        vec3 L;
        float attenuation = 1.0;
        
        if (lightTypes[i] == 0) { // Directional light
            L = normalize(-lightDirections[i]);
            attenuation = 1.0;
        } else if (lightTypes[i] == 1) { // Point light
            L = normalize(lightPositions[i] - WorldPos);
            float distance = length(lightPositions[i] - WorldPos);
            attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
        } else if (lightTypes[i] == 2) { // Spot light
            L = normalize(lightPositions[i] - WorldPos);
            float distance = length(lightPositions[i] - WorldPos);
            attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
            
            // Spot light cone calculation
            float theta = dot(L, normalize(-lightDirections[i]));
            float epsilon = lightInnerCones[i] - lightOuterCones[i];
            float intensity = clamp((theta - lightOuterCones[i]) / epsilon, 0.0, 1.0);
            attenuation *= intensity;
        }
        
        vec3 H = normalize(V + L);
        
        vec3 radiance = lightColors[i] * lightIntensities[i] * attenuation; 
        
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
        float shadow = 1.0;  // Default to fully lit (no shadow)
        
        if (i == 0 && lightTypes[i] == 0) {  // Apply shadows to first directional light
            vec4 fragPosLightSpace = lightSpaceMatrix * vec4(WorldPos, 1.0);
            shadow = ShadowCalculation(fragPosLightSpace, N, L);
        }
        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
    }
    
    // Ambient lighting 
    vec3 F_ambient = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 kS_ambient = F_ambient;
    vec3 kD_ambient = 1.0 - kS_ambient;
    kD_ambient *= 1.0 - metallic;
    
    vec3 ambient;
    if (useIBL) {
        // Use precomputed irradiance map for diffuse IBL
        vec3 irradiance = texture(irradianceMap, N).rgb;
        
        vec3 diffuse_ambient = irradiance * albedo;
        
        // Use prefiltered environment map for specular IBL
        vec3 R = reflect(-V, N);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        
        // Approximate BRDF integration (simplified Schlick approximation)
        vec3 F_schlick = F_ambient + (max(vec3(1.0 - roughness), F_ambient) - F_ambient) * pow(clamp(1.0 - max(dot(N, V), 0.0), 0.0, 1.0), 5.0);
        vec3 specular_ambient = prefilteredColor * F_schlick;
        
        ambient = (kD_ambient * diffuse_ambient + specular_ambient) * ao;
    } else {
        //Phong 
        vec3 irradiance = ambientLight;
        vec3 diffuse_ambient = irradiance * albedo;
        vec3 specular_ambient = irradiance * F_ambient * (1.0 - roughness);
        ambient = (kD_ambient * diffuse_ambient + specular_ambient) * ao;
    }
    
    vec3 color = ambient + Lo + emissiveColor; 
    
    // Ensure minimum visibility
    if (length(color) < 0.1) {
        color = albedo * 0.3; 
    }
    
    // HDR tone mapping and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    
    FragColor = vec4(color, 1.0);
}
