#version 460 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;
uniform bool isHDR = false;
uniform float exposure = 1.0;
uniform float gamma = 2.2;

// Simple Reinhard tone mapping
vec3 toneMapReinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

// ACES tone mapping (more cinematic)
vec3 toneMapACES(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{    
    vec3 color = texture(skybox, TexCoords).rgb;
    
    if (isHDR) {
        // Apply exposure
        color *= exposure;
        
        // Tone mapping
        color = toneMapACES(color);
        
        // Gamma correction
        color = pow(color, vec3(1.0 / gamma));
    }
    
    FragColor = vec4(color, 1.0);
}
