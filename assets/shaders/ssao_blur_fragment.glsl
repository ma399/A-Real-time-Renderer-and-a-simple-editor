#version 460 core

out float FragColor;

in vec2 TexCoord;

uniform sampler2D ssaoInput;
uniform vec2 screenSize;
uniform int blurRadius;

void main() {
    vec2 texelSize = 1.0 / screenSize;
    float result = 0.0;
    float totalWeight = 0.0;
    
    // Bilateral blur to preserve edges
    float centerValue = texture(ssaoInput, TexCoord).r;
    
    for (int x = -blurRadius; x <= blurRadius; ++x) {
        for (int y = -blurRadius; y <= blurRadius; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleCoord = TexCoord + offset;
            
            // Check bounds
            if (sampleCoord.x < 0.0 || sampleCoord.x > 1.0 || 
                sampleCoord.y < 0.0 || sampleCoord.y > 1.0) {
                continue;
            }
            
            float sampleValue = texture(ssaoInput, sampleCoord).r;
            
            // Simple Gaussian-like weight based on distance
            float distance = length(vec2(x, y));
            float spatialWeight = exp(-distance * distance / (2.0 * float(blurRadius) * float(blurRadius)));
            
            // Bilateral weight to preserve edges
            float valueDiff = abs(centerValue - sampleValue);
            float bilateralWeight = exp(-valueDiff * valueDiff / (2.0 * 0.1 * 0.1));
            
            float weight = spatialWeight * bilateralWeight;
            
            result += sampleValue * weight;
            totalWeight += weight;
        }
    }
    
    FragColor = totalWeight > 0.0 ? result / totalWeight : centerValue;
}
