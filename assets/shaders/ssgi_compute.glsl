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

// Hi-Z Buffer (Depth Pyramid) for accelerated ray marching
uniform sampler2D hizTexture;     // Hi-Z pyramid texture

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

// Hi-Z accelerated screen space ray marching
vec3 screenSpaceRayMarchHiZ(vec3 rayOrigin, vec3 rayDir, out bool hit) {
    hit = false;
    
    // Step 1: Begin stepping - Transform to view space
    vec4 viewOrigin = view * vec4(rayOrigin, 1.0);
    vec4 viewDir = view * vec4(rayDir, 0.0);
    
    vec3 rayDirection = normalize(viewDir.xyz);
    vec3 currentPos = viewOrigin.xyz;
    float t = 0.0;
    const float tMax = maxDistance;
    const int maxMipLevel = 4;
    
    // Ray marching loop
    for (int i = 0; i < maxSteps && t < tMax; i++) {
        // Step 2: Determine current region - Project current ray position to screen space
        vec4 projPos = projection * vec4(currentPos, 1.0);
        if (projPos.w <= 0.0) break;  // Behind camera
        
        projPos.xyz /= projPos.w;
        vec2 screenUV = projPos.xy * 0.5 + 0.5;
        
        // Check bounds
        if (screenUV.x < 0.0 || screenUV.x > 1.0 || screenUV.y < 0.0 || screenUV.y > 1.0) {
            break;
        }
        
        // Step 3: Query pyramid - Select appropriate mip level based on step distance
        // Farther distances can use lower mip levels (larger sampling range)
        int mipLevel = clamp(int(log2(max(1.0, t / (stepSize * 2.0)))), 0, maxMipLevel);
        
        // Sample Hi-Z pyramid at selected mip level
        float hizDepth = textureLod(hizTexture, screenUV, float(mipLevel)).r;
        
        // Convert current ray position to depth for comparison
        float rayDepth = projPos.z * 0.5 + 0.5;  // Convert NDC to [0,1] depth
        
        // Step 4: Perform comparison - Compare ray depth with Hi-Z sampled depth
        // Step 5: Make decision based on comparison result
        if (rayDepth < hizDepth) {
            // Ray depth < region max depth: entire region is "empty"
            // Safe to perform large step jump, skip empty space efficiently
            float jumpDistance = stepSize * pow(2.0, float(mipLevel));
            t += jumpDistance;
            currentPos = viewOrigin.xyz + rayDirection * t;
        } else {
            // Ray depth > region max depth: ray has entered region with potential geometry
            if (mipLevel == 0) {
                // Already at highest resolution, perform precise detection
                float actualDepth = texture(gDepth, screenUV).r;
                float ndcDepth = actualDepth * 2.0 - 1.0;
                float viewSampledZ = -projection[3][2] / (ndcDepth + projection[2][2]);
                
                float depthDiff = currentPos.z - viewSampledZ;
                if (depthDiff > 0.0 && depthDiff < thickness) {
                    // Found intersection
                    hit = true;
                    return vec3(screenUV, actualDepth);
                } else {
                    // No intersection, take small step forward
                    t += stepSize * 0.5;
                    currentPos = viewOrigin.xyz + rayDirection * t;
                }
            } else {
                // Lower mip level (switch to higher resolution depth map) or switch back to original depth map (Mip0)
                // Use very fine small steps to precisely find intersection points
                t += stepSize * 0.25;  // Very small step for precision
                currentPos = viewOrigin.xyz + rayDirection * t;
            }
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
    // float ao = motionAO.z;
    float ao = 1.0;
    
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
        
        // Perform Hi-Z accelerated screen space ray march
        bool hit;
        vec3 hitResult = screenSpaceRayMarchHiZ(worldPos, sampleDir, hit);
        
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
    } else {
        // Fallback: small ambient contribution if no indirect lighting found
        indirectColor = albedo * 0.02 * ao; // Very small ambient
    }
    
    // Store result
    imageStore(ssgi_raw_texture, texelCoord, vec4(indirectColor, 1.0));
}
