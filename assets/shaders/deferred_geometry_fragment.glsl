#version 460 core

// G-Buffer outputs 
layout (location = 0) out vec4 gPosition;      // World Position (xyz) + Material ID (w)
layout (location = 1) out vec4 gAlbedoMetallic; // Albedo (rgb) + Metallic (a)
layout (location = 2) out vec4 gNormalRoughness; // Normal (xyz) + Roughness (a)
layout (location = 3) out vec4 gMotionAO;      // Motion Vector (xy) + AO (z) + unused (w)
layout (location = 4) out vec4 gEmissive;      // Emissive Color (rgb) + intensity (a)

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec3 WorldPos;
in vec3 Tangent;
in vec4 PrevFragPos;

// Material properties
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    vec3 emissive;
};

uniform Material material;

// Material textures
uniform sampler2D albedoTexture;
uniform sampler2D normalTexture;
uniform sampler2D metallicTexture;
uniform sampler2D roughnessTexture;
uniform sampler2D aoTexture;
uniform sampler2D emissiveTexture;

// Material parameters
uniform float materialMetallic;
uniform float materialRoughness;
uniform float materialAO;
uniform int materialID;

// Flags to indicate if textures are available
uniform bool hasAlbedoTexture;
uniform bool hasNormalTexture;
uniform bool hasMetallicTexture;
uniform bool hasRoughnessTexture;
uniform bool hasAOTexture;
uniform bool hasEmissiveTexture;

vec3 getNormalFromMap()
{
    // If no normal texture is available, return the vertex normal
    if (!hasNormalTexture) {
        return normalize(Normal);
    }
    
    // Sample the normal map and convert from [0,1] to [-1,1]
    vec3 normalMap = texture(normalTexture, TexCoords).rgb * 2.0 - 1.0;
    
    // Calculate the bitangent
    vec3 N = normalize(Normal);
    vec3 T = normalize(Tangent);
    // Re-orthogonalize tangent with respect to normal (Gram-Schmidt process)
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    
    // Create the TBN matrix
    mat3 TBN = mat3(T, B, N);
    
    // Transform the normal from tangent space to world space
    return normalize(TBN * normalMap);
}

vec2 calculateMotionVector()
{
    // Calculate current screen position
    vec4 currentPos = gl_FragCoord;
    vec2 currentScreenPos = currentPos.xy / currentPos.w;
    
    // Calculate previous screen position
    vec4 prevScreenPos = PrevFragPos;
    prevScreenPos /= prevScreenPos.w;
    prevScreenPos.xy = prevScreenPos.xy * 0.5 + 0.5; // Convert from [-1,1] to [0,1]
    
    // Current position in [0,1] range
    vec2 screenSize = textureSize(albedoTexture, 0); // Use any available texture to get screen size
    currentScreenPos = currentScreenPos / screenSize;
    
    // Motion vector is the difference between current and previous positions
    return currentScreenPos - prevScreenPos.xy;
}

void main()
{
    // RT0: World position //TODO: Add material ID
    gPosition = vec4(WorldPos, 1.0); 
    
    // RT1: Albedo (RGB) + Metallic (A) 
    vec3 albedo = material.diffuse;
    if (hasAlbedoTexture) {
        albedo *= texture(albedoTexture, TexCoords).rgb;
    }
    float metallic = materialMetallic;
    if (hasMetallicTexture) {
        metallic *= texture(metallicTexture, TexCoords).r;
    }
    metallic = clamp(metallic, 0.0, 1.0);
    gAlbedoMetallic = vec4(albedo, metallic);
    
    // RT2: Normal (RGB) + Roughness (A) 
    vec3 normal = getNormalFromMap();
    float roughness = materialRoughness;
    if (hasRoughnessTexture) {
        roughness *= texture(roughnessTexture, TexCoords).r;
    }
    roughness = clamp(roughness, 0.0, 1.0);
    gNormalRoughness = vec4(normal * 0.5 + 0.5, roughness); 
    
    // RT3: Motion Vector (XY) + AO (Z) + unused (W)
    float ao = materialAO;
    if (hasAOTexture) {
        ao *= texture(aoTexture, TexCoords).r;
    }
    ao = clamp(ao, 0.0, 1.0);
    vec2 motionVector = calculateMotionVector();
    gMotionAO = vec4(motionVector, ao, 0.0);
    
    // RT4: Emissive Color (RGB) + Intensity (A)
    vec3 emissiveColor = material.emissive;
    if (hasEmissiveTexture) {
        emissiveColor *= texture(emissiveTexture, TexCoords).rgb;
    }
    float emissiveIntensity = length(emissiveColor);
    gEmissive = vec4(emissiveColor, emissiveIntensity);
}
