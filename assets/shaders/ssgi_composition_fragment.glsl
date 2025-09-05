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

// SSAO texture
uniform sampler2D ssaoTexture;         // Screen-space ambient occlusion
uniform bool enableSSAO;

// Environment lighting
uniform samplerCube irradianceMap;
uniform samplerCube prefilteredMap;
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
uniform float exposure;

// PBR functions for environment lighting
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 ACESFitted(vec3 color) {
    // sRGB to ACEScg
    const mat3 sRGB_to_ACEScg = mat3(
        0.59719, 0.35458, 0.04823,
        0.07600, 0.90834, 0.01566,
        0.02840, 0.13383, 0.83777
    );

    // ACEScg to sRGB
    const mat3 ACEScg_to_sRGB = mat3(
        1.60475, -0.53108, -0.07367,
        -0.10208,  1.10813, -0.00605,
        -0.00327, -0.07276,  1.07602
    );

    // 1. Convert from sRGB linear to ACEScg
    vec3 aces_color = sRGB_to_ACEScg * color;

    // 2. Apply the RRT + ODT curve
    aces_color = (aces_color * (aces_color + 0.0245786) - 0.000090537) / (aces_color * (aces_color * 0.983729 + 0.4329510) + 0.238081);

    // 3. Convert from ACEScg to sRGB linear
    vec3 result = ACEScg_to_sRGB * aces_color; 
    // Clamp the result to [0, 1] range
    return clamp(result, 0.0, 1.0);
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
    
    // Apply SSAO if enabled
    if (enableSSAO) {
        float ssao = texture(ssaoTexture, uv).r;
        // Multiply material AO with SSAO
        ao = ao * ssao;
    }
    
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
        
        vec3 diffuse_ambient = irradiance * albedo;
        
        // Use prefiltered environment map for specular IBL
        vec3 R = reflect(-V, N);
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        
        // Approximate BRDF integration (simplified Schlick approximation)
        vec3 F_schlick = F_ambient + (max(vec3(1.0 - roughness), F_ambient) - F_ambient) * pow(clamp(1.0 - max(dot(N, V), 0.0), 0.0, 1.0), 5.0);
        vec3 specular_ambient = prefilteredColor * F_schlick;
        
        environmentLighting = (kD_ambient * diffuse_ambient + specular_ambient);
    } else {
        // Fallback to simple ambient lighting
        vec3 irradiance = ambientLight;
        vec3 diffuse_ambient = irradiance * albedo;
        vec3 specular_ambient = irradiance * F_ambient * (1.0 - roughness);
        environmentLighting = (kD_ambient * diffuse_ambient + specular_ambient);
    }
    
    // Apply AO to lighting components
    vec3 finalColor = directLighting + (indirectLighting + environmentLighting) * ao + emissiveColor;

    // vec3 finalColor = (directLighting + indirectLighting + environmentLighting * 0.01) + emissiveColor;
    

    // HDR tone mapping and gamma correction
    // finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = ACESFitted(finalColor * exposure);
    finalColor = pow(finalColor, vec3(1.0/2.2));

    FragColor = vec4(finalColor, 1.0);
}
