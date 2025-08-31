#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Output texture for raw SSAO
layout(r16f, binding = 0) uniform writeonly image2D ssao_raw_texture;

// G-Buffer textures
uniform sampler2D gPosition;      // World Position (xyz) + Material ID (w)
uniform sampler2D gNormalRoughness; // Normal (xyz) + Roughness (a)
uniform sampler2D gDepth;         // Depth buffer

// Camera matrices
uniform mat4 view;
uniform mat4 projection;
uniform mat4 invView;
uniform mat4 invProjection;
uniform vec3 viewPos;

// SSAO parameters
uniform int numSamples;
uniform float radius;
uniform float bias;
uniform float intensity;
uniform vec2 noiseScale;

// Noise texture for random sampling
uniform sampler2D noiseTexture;

// Sample kernel (uploaded as uniform)
uniform vec3 samples[64];

void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 screenSize = vec2(imageSize(ssao_raw_texture));
    
    // Check bounds
    if (texelCoord.x >= int(screenSize.x) || texelCoord.y >= int(screenSize.y)) {
        return;
    }
    
    vec2 uv = (vec2(texelCoord) + 0.5) / screenSize;
    
    // Sample G-Buffer
    vec4 positionData = texture(gPosition, uv);
    vec4 normalRoughness = texture(gNormalRoughness, uv);
    
    vec3 worldPos = positionData.xyz;
    vec3 normal = normalize(normalRoughness.xyz * 2.0 - 1.0);
    
    // Skip background pixels
    if (positionData.w < 0.5) {
        imageStore(ssao_raw_texture, texelCoord, vec4(1.0));
        return;
    }
    
    // Transform to view space
    vec4 viewPos4 = view * vec4(worldPos, 1.0);
    vec3 fragPos = viewPos4.xyz;
    vec3 viewNormal = normalize((view * vec4(normal, 0.0)).xyz);
    
    // Get noise vector for random rotation
    vec3 randomVec = normalize(texture(noiseTexture, uv * noiseScale).xyz);
    
    // Create TBN matrix for tangent space to view space transformation
    vec3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    vec3 bitangent = cross(viewNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, viewNormal);
    
    // Calculate occlusion
    float occlusion = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        // Get sample position in view space
        vec3 samplePos = TBN * samples[i]; // From tangent to view space
        samplePos = fragPos + samplePos * radius;
        
        // Project sample position to screen space
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset;    // From view to clip space
        offset.xyz /= offset.w;          // Perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // Transform to [0,1] range
        
        // Check if sample is within screen bounds
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
            continue;
        }
        
        // Get sample depth from G-Buffer
        float sampleDepth = texture(gDepth, offset.xy).r;
        
        // Convert depth to view space Z
        vec4 sampleWorldPos = texture(gPosition, offset.xy);
        vec4 sampleViewPos = view * vec4(sampleWorldPos.xyz, 1.0);
        float sampleZ = sampleViewPos.z;
        
        // Range check and accumulate occlusion
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleZ));
        occlusion += (sampleZ >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    
    occlusion = 1.0 - (occlusion / float(numSamples));
    occlusion = pow(occlusion, intensity);
    
    // Store result
    imageStore(ssao_raw_texture, texelCoord, vec4(occlusion));
}
