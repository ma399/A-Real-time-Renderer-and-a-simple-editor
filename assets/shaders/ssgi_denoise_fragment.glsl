#version 460 core

out vec4 FragColor;
in vec2 TexCoords;

// Input textures
uniform sampler2D ssgi_raw_texture;    // Raw noisy SSGI
uniform sampler2D gPosition;           // World Position for depth comparison
uniform sampler2D gNormalRoughness;    // Normal for edge detection
uniform sampler2D gDepth;              // Depth buffer

// Denoising parameters
uniform float spatialSigma;
uniform float normalSigma;
uniform float depthSigma;
uniform int filterRadius;
uniform bool enableTemporalFilter;

// Screen dimensions
uniform vec2 screenSize;

// Bilateral filter weights
float calculateSpatialWeight(vec2 offset, float sigma) {
    float distance = length(offset);
    return exp(-(distance * distance) / (2.0 * sigma * sigma));
}

float calculateNormalWeight(vec3 centerNormal, vec3 sampleNormal, float sigma) {
    float normalDiff = 1.0 - max(dot(centerNormal, sampleNormal), 0.0);
    return exp(-(normalDiff * normalDiff) / (2.0 * sigma * sigma));
}

float calculateDepthWeight(float centerDepth, float sampleDepth, float sigma) {
    float depthDiff = abs(centerDepth - sampleDepth);
    return exp(-(depthDiff * depthDiff) / (2.0 * sigma * sigma));
}

// Edge-preserving bilateral filter
vec3 bilateralFilter(vec2 uv) {
    vec3 centerColor = texture(ssgi_raw_texture, uv).rgb;
    vec3 centerNormal = normalize(texture(gNormalRoughness, uv).xyz * 2.0 - 1.0);
    float centerDepth = texture(gDepth, uv).r;
    
    vec3 filteredColor = vec3(0.0);
    float totalWeight = 0.0;
    
    vec2 texelSize = 1.0 / screenSize;
    
    // Sample in a square pattern around the center pixel
    for (int x = -filterRadius; x <= filterRadius; x++) {
        for (int y = -filterRadius; y <= filterRadius; y++) {
            vec2 offset = vec2(float(x), float(y));
            vec2 sampleUV = uv + offset * texelSize;
            
            // Check bounds
            if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
                continue;
            }
            
            // Sample data
            vec3 sampleColor = texture(ssgi_raw_texture, sampleUV).rgb;
            vec3 sampleNormal = normalize(texture(gNormalRoughness, sampleUV).xyz * 2.0 - 1.0);
            float sampleDepth = texture(gDepth, sampleUV).r;
            
            // Calculate weights
            float spatialWeight = calculateSpatialWeight(offset, spatialSigma);
            float normalWeight = calculateNormalWeight(centerNormal, sampleNormal, normalSigma);
            float depthWeight = calculateDepthWeight(centerDepth, sampleDepth, depthSigma);
            
            float combinedWeight = spatialWeight * normalWeight * depthWeight;
            
            filteredColor += sampleColor * combinedWeight;
            totalWeight += combinedWeight;
        }
    }
    
    // Normalize
    if (totalWeight > 0.0) {
        return filteredColor / totalWeight;
    } else {
        return centerColor;
    }
}

// A-Trous wavelet filter for additional denoising
vec3 atrousFilter(vec2 uv, int stepSize) {
    vec3 centerColor = texture(ssgi_raw_texture, uv).rgb;
    vec3 centerNormal = normalize(texture(gNormalRoughness, uv).xyz * 2.0 - 1.0);
    float centerDepth = texture(gDepth, uv).r;
    
    // A-Trous kernel weights
    float kernel[25] = float[](
        1.0/256.0, 4.0/256.0, 6.0/256.0, 4.0/256.0, 1.0/256.0,
        4.0/256.0, 16.0/256.0, 24.0/256.0, 16.0/256.0, 4.0/256.0,
        6.0/256.0, 24.0/256.0, 36.0/256.0, 24.0/256.0, 6.0/256.0,
        4.0/256.0, 16.0/256.0, 24.0/256.0, 16.0/256.0, 4.0/256.0,
        1.0/256.0, 4.0/256.0, 6.0/256.0, 4.0/256.0, 1.0/256.0
    );
    
    vec3 filteredColor = vec3(0.0);
    float totalWeight = 0.0;
    
    vec2 texelSize = 1.0 / screenSize;
    
    // 5x5 kernel
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            vec2 offset = vec2(float(x * stepSize), float(y * stepSize));
            vec2 sampleUV = uv + offset * texelSize;
            
            // Check bounds
            if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
                continue;
            }
            
            // Sample data
            vec3 sampleColor = texture(ssgi_raw_texture, sampleUV).rgb;
            vec3 sampleNormal = normalize(texture(gNormalRoughness, sampleUV).xyz * 2.0 - 1.0);
            float sampleDepth = texture(gDepth, sampleUV).r;
            
            // Get kernel weight
            int kernelIndex = (y + 2) * 5 + (x + 2);
            float kernelWeight = kernel[kernelIndex];
            
            // Calculate edge-preserving weights
            float normalWeight = calculateNormalWeight(centerNormal, sampleNormal, normalSigma);
            float depthWeight = calculateDepthWeight(centerDepth, sampleDepth, depthSigma);
            
            float combinedWeight = kernelWeight * normalWeight * depthWeight;
            
            filteredColor += sampleColor * combinedWeight;
            totalWeight += combinedWeight;
        }
    }
    
    // Normalize
    if (totalWeight > 0.0) {
        return filteredColor / totalWeight;
    } else {
        return centerColor;
    }
}

// Temporal filter 
vec3 temporalFilter(vec2 uv, vec3 currentColor) {
    // For now, just return current color
    // In a full implementation, this would blend with previous frame
    return currentColor;
}

void main() {
    vec2 uv = TexCoords;
    
    // Check if this is a valid pixel (not background)
    vec4 positionData = texture(gPosition, uv);
    if (positionData.w < 0.5) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    // Apply bilateral filter
    vec3 denoisedColor = bilateralFilter(uv);
    
    // Apply A-Trous filter for additional smoothing
    // You can adjust the step size for different levels of denoising
    denoisedColor = atrousFilter(uv, 1);
    
    // Apply temporal filter if enabled
    if (enableTemporalFilter) {
        denoisedColor = temporalFilter(uv, denoisedColor);
    }
    
    // Clamp to prevent fireflies
    denoisedColor = clamp(denoisedColor, 0.0, 10.0);
    
    FragColor = vec4(denoisedColor, 1.0);
}
