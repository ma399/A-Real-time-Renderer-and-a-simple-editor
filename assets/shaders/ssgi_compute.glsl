#version 460 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Output texture for raw SSGI
layout(rgba16f, binding = 0) uniform writeonly image2D ssgi_raw_texture;

// G-Buffer textures
uniform sampler2D gPosition;      // World Position (xyz) + Material ID (w)
uniform sampler2D gAlbedoMetallic; // Albedo (rgb) + Metallic (a)
uniform sampler2D gNormalRoughness; // Normal (xyz) + Roughness (a)
uniform sampler2D gMotionAO;      // Motion Vector (xy) + AO (z) + unused (w)
uniform sampler2D gDepth;         // Depth buffer

// Direct lighting texture (from lighting pass)
uniform sampler2D litSceneTexture;

// Camera matrices
uniform mat4 view;
uniform mat4 projection;
uniform mat4 invView;
uniform mat4 invProjection;
uniform vec3 viewPos;

// SSGI parameters
uniform int maxSteps;
uniform float maxDistance;
uniform float stepSize;
uniform float thickness;
uniform float intensity;
uniform int numSamples;

// Random number generation
float random(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// Generate hemisphere sample
vec3 generateHemisphereSample(vec2 xi, vec3 normal) {
    float cosTheta = sqrt(1.0 - xi.x);
    float sinTheta = sqrt(xi.x);
    float phi = 2.0 * 3.14159265359 * xi.y;
    
    vec3 localSample = vec3(
        sinTheta * cos(phi),
        sinTheta * sin(phi),
        cosTheta
    );
    
    // Create tangent space
    vec3 up = abs(normal.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    
    return tangent * localSample.x + bitangent * localSample.y + normal * localSample.z;
}

// Screen space ray marching
vec3 screenSpaceRayMarch(vec3 rayOrigin, vec3 rayDir, out bool hit) {
    hit = false;
    
    // Transform to view space
    vec4 viewOrigin = view * vec4(rayOrigin, 1.0);
    vec4 viewDir = view * vec4(rayDir, 0.0);
    
    vec3 rayStep = normalize(viewDir.xyz) * stepSize;
    vec3 currentPos = viewOrigin.xyz;
    
    for (int i = 0; i < maxSteps; i++) {
        currentPos += rayStep;
        
        // Project to screen space
        vec4 projPos = projection * vec4(currentPos, 1.0);
        projPos.xyz /= projPos.w;
        
        // Convert to texture coordinates
        vec2 screenUV = projPos.xy * 0.5 + 0.5;
        
        // Check bounds
        if (screenUV.x < 0.0 || screenUV.x > 1.0 || screenUV.y < 0.0 || screenUV.y > 1.0) {
            break;
        }
        
        // Sample depth buffer
        float sampledDepth = texture(gDepth, screenUV).r;
        
        // Convert to view space depth
        vec4 ndcPos = vec4(projPos.xy, sampledDepth * 2.0 - 1.0, 1.0);
        vec4 viewSampledPos = invProjection * ndcPos;
        viewSampledPos /= viewSampledPos.w;
        
        // Check intersection
        float depthDiff = currentPos.z - viewSampledPos.z;
        if (depthDiff > 0.0 && depthDiff < thickness) {
            hit = true;
            return vec3(screenUV, sampledDepth);
        }
        
        // Early exit if too far
        if (length(currentPos - viewOrigin.xyz) > maxDistance) {
            break;
        }
    }
    
    return vec3(0.0);
}

void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
    vec2 screenSize = vec2(imageSize(ssgi_raw_texture));
    
    // Check bounds
    if (texelCoord.x >= int(screenSize.x) || texelCoord.y >= int(screenSize.y)) {
        return;
    }
    
    vec2 uv = (vec2(texelCoord) + 0.5) / screenSize;
    
    // Sample G-Buffer
    vec4 positionData = texture(gPosition, uv);
    vec4 albedoMetallic = texture(gAlbedoMetallic, uv);
    vec4 normalRoughness = texture(gNormalRoughness, uv);
    vec4 motionAO = texture(gMotionAO, uv);
    
    vec3 worldPos = positionData.xyz;
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 normal = normalize(normalRoughness.xyz * 2.0 - 1.0);
    float roughness = normalRoughness.a;
    float ao = motionAO.z;
    
    // Skip background pixels
    if (positionData.w < 0.5) {
        imageStore(ssgi_raw_texture, texelCoord, vec4(0.0));
        return;
    }
    
    // Initialize accumulated color
    vec3 indirectColor = vec3(0.0);
    float totalWeight = 0.0;
    
    // Generate random seed for this pixel
    vec2 seed = uv + fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453);
    
    // Sample multiple directions for indirect lighting
    for (int i = 0; i < numSamples; i++) {
        // Generate random sample in hemisphere
        vec2 xi = vec2(
            random(seed + vec2(float(i) * 0.1, 0.0)),
            random(seed + vec2(0.0, float(i) * 0.1))
        );
        
        vec3 sampleDir = generateHemisphereSample(xi, normal);
        
        // Perform screen space ray march
        bool hit;
        vec3 hitResult = screenSpaceRayMarch(worldPos, sampleDir, hit);
        
        if (hit) {
            vec2 hitUV = hitResult.xy;
            
            // Sample indirect lighting from hit point
            vec3 hitLighting = texture(litSceneTexture, hitUV).rgb;
            vec4 hitAlbedo = texture(gAlbedoMetallic, hitUV);
            
            // Simple Lambert BRDF for indirect lighting
            float NdotL = max(dot(normal, sampleDir), 0.0);
            vec3 brdf = albedo / 3.14159265359; // Lambertian BRDF
            
            indirectColor += hitLighting * hitAlbedo.rgb * brdf * NdotL;
            totalWeight += NdotL;
        }
    }
    
    // Normalize and apply intensity
    if (totalWeight > 0.0) {
        indirectColor = (indirectColor / totalWeight) * intensity * ao;
    }
    
    // Store result
    imageStore(ssgi_raw_texture, texelCoord, vec4(indirectColor, 1.0));
}
