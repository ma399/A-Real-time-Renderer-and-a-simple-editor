#version 460 core

out vec4 FragColor;
in vec2 TexCoords;

// Input textures
uniform sampler2D litSceneTexture;     // Direct lighting only
uniform sampler2D ssgi_final_texture;  // Denoised SSGI (indirect lighting)
uniform sampler2D gPosition;           // World Position for background check
uniform sampler2D gAlbedoMetallic;     // Albedo for background
uniform sampler2D gNormalRoughness;    // Normal for environment reflection
uniform sampler2D gEmissive;           // Emissive color
uniform sampler2D gMotionAO;           // AO factor

// Environment lighting
uniform samplerCube irradianceMap;
uniform samplerCube skyboxTexture;
uniform bool useIBL;
uniform bool useSkybox;
uniform vec3 ambientLight;
uniform vec3 viewPos;

// Camera matrices for proper skybox sampling
uniform mat4 invView;
uniform mat4 invProjection;

// SSGI controls
uniform bool enableSSGI;
uniform float ssgiIntensity;

// PBR functions for environment lighting
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec2 uv = TexCoords;
    
    // Sample G-Buffer data
    vec4 positionData = texture(gPosition, uv);
    vec4 albedoMetallic = texture(gAlbedoMetallic, uv);
    vec4 normalRoughness = texture(gNormalRoughness, uv);
    vec4 emissiveData = texture(gEmissive, uv);
    vec4 motionAO = texture(gMotionAO, uv);
    
    // Check if this is a background pixel - skip composition for background
    // (skybox is already rendered to framebuffer)
    if (positionData.w < 0.5) {
        discard; // Let the skybox show through
    }
    
    // Extract material properties
    vec3 worldPos = positionData.xyz;
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 normal = normalize(normalRoughness.xyz * 2.0 - 1.0);
    float roughness = normalRoughness.a;
    vec3 emissiveColor = emissiveData.rgb * emissiveData.a;
    float ao = motionAO.z;
    
    // Sample lighting textures
    vec3 directLighting = texture(litSceneTexture, uv).rgb;
    vec3 indirectLighting = vec3(0.0);
    
    if (enableSSGI) {
        indirectLighting = texture(ssgi_final_texture, uv).rgb * ssgiIntensity;
    }
    
    // Calculate environment lighting (ambient/IBL)
    vec3 N = normal;
    vec3 V = normalize(viewPos - worldPos);
    
    // Calculate reflectance at normal incidence
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    vec3 F_ambient = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 kS_ambient = F_ambient;
    vec3 kD_ambient = 1.0 - kS_ambient;
    kD_ambient *= 1.0 - metallic;
    
    vec3 environmentLighting = vec3(0.0);
    if (useIBL) {
        // Use precomputed irradiance map for diffuse IBL
        vec3 irradiance = texture(irradianceMap, N).rgb;
        
        // Ensure irradiance has some minimum value
        if (length(irradiance) < 0.01) {
            irradiance = vec3(0.1, 0.1, 0.1); // Fallback to gray
        }
        
        vec3 diffuse_ambient = irradiance * albedo;
        
        // Simplified specular ambient (could be enhanced with prefiltered environment map)
        vec3 specular_ambient = irradiance * F_ambient * (1.0 - roughness) * 0.5;
        
        environmentLighting = (kD_ambient * diffuse_ambient + specular_ambient) * ao;
    } else {
        // Fallback to simple ambient lighting
        vec3 irradiance = ambientLight;
        vec3 diffuse_ambient = irradiance * albedo;
        vec3 specular_ambient = irradiance * F_ambient * (1.0 - roughness);
        environmentLighting = (kD_ambient * diffuse_ambient + specular_ambient) * ao;
    }
    
    // Combine all lighting components
    vec3 finalColor = directLighting + indirectLighting + environmentLighting + emissiveColor;
    
    // Ensure minimum visibility
    if (length(finalColor) < 0.1) {
        finalColor = albedo * 0.3;
    }
    
    // HDR tone mapping and gamma correction
    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0/2.2));
    
    FragColor = vec4(finalColor, 1.0);
}
