#version 460 core

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D sceneTexture;    // Current framebuffer content
uniform sampler2D ssaoTexture;     // SSAO texture
uniform sampler2D gMotionAO;       // G-Buffer AO for material AO
uniform sampler2D gPosition;       // Position to check for background

void main() {
    vec4 positionData = texture(gPosition, TexCoord);
    
    // Skip background pixels
    if (positionData.w < 0.5) {
        FragColor = texture(sceneTexture, TexCoord);
        return;
    }
    
    vec3 sceneColor = texture(sceneTexture, TexCoord).rgb;
    float ssao = texture(ssaoTexture, TexCoord).r;
    float materialAO = texture(gMotionAO, TexCoord).z;
    
    // Multiply material AO with SSAO
    float combinedAO = materialAO * ssao;
    
    // Apply combined AO to scene color
    vec3 finalColor = sceneColor * combinedAO;
    
    FragColor = vec4(finalColor, 1.0);
}
