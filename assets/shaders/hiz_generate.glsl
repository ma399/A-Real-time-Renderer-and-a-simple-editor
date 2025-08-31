#version 460 core
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Input: previous mip level (or original depth texture for mip 0)
uniform sampler2D inputDepthTexture;

// Output: current mip level being generated
layout(r32f, binding = 0) uniform writeonly image2D outputDepthMip;

// Mip level being generated (0 = copy from original, 1+ = downsample)
uniform int currentMipLevel;

// Input mip level to sample from (for mip > 0)
uniform int inputMipLevel;

void main() {
    ivec2 outputCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 outputSize = imageSize(outputDepthMip);
    
    // Check bounds
    if (outputCoord.x >= outputSize.x || outputCoord.y >= outputSize.y) {
        return;
    }
    
    if (currentMipLevel == 0) {
        // Mip level 0: Direct copy from original depth texture
        vec2 uv = (vec2(outputCoord) + 0.5) / vec2(outputSize);
        float depth = texture(inputDepthTexture, uv).r;
        
        // Debug: Ensure we're getting valid depth values
        // Depth should be in [0,1] range, with 1.0 being far plane
        if (depth <= 0.0 || depth > 1.0) {
            depth = 1.0; // Default to far plane if invalid
        }
        
        imageStore(outputDepthMip, outputCoord, vec4(depth));
    } else {
        // Mip level 1+: Downsample by taking maximum depth from 2x2 block        
        ivec2 inputCoord = outputCoord * 2;  // Map to input coordinates
        
        // Sample 2x2 block from previous mip level
        float depth00 = texelFetch(inputDepthTexture, inputCoord, inputMipLevel).r;
        float depth10 = texelFetch(inputDepthTexture, inputCoord + ivec2(1, 0), inputMipLevel).r;
        float depth01 = texelFetch(inputDepthTexture, inputCoord + ivec2(0, 1), inputMipLevel).r;
        float depth11 = texelFetch(inputDepthTexture, inputCoord + ivec2(1, 1), inputMipLevel).r;
        
        // Take maximum depth (farthest point) for conservative occlusion testing
        float maxDepth = max(max(depth00, depth10), max(depth01, depth11));
        
        imageStore(outputDepthMip, outputCoord, vec4(maxDepth));
    }
}
