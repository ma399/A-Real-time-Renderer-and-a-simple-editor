#version 460 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform sampler2D shadowMap;
uniform samplerCube skybox;  // Skybox cubemap texture
uniform float pcfRadius;
uniform float lightSize;
uniform float reflectionStrength;  // Control reflection intensity

// Material properties
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    vec3 emissive;
};

uniform Material material;

// Shadow calculation functions 
float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
 
    if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0)
        return 0.0;
    
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    float currentDepth = projCoords.z;
    
    float bias = 0;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    return shadow;
}


float findBlockerDepth(vec2 uv, float zReceiver, float searchRadius)
{
    vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );
    
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float blockerDepthSum = 0.0;
    int blockerCount = 0;
    
    for(int i = 0; i < 16; ++i)
    {
        vec2 offset = poissonDisk[i] * texelSize * searchRadius;
        float shadowDepth = texture(shadowMap, uv + offset).r;
        
        if(shadowDepth < zReceiver)
        {
            blockerDepthSum += shadowDepth;
            blockerCount++;
        }
    }
    
    if(blockerCount == 0)
        return -1.0;  
    
    return blockerDepthSum / float(blockerCount);
}

float calculatePenumbraSize(float zReceiver, float zBlocker, float lightSize)
{
    if(zBlocker < 0.0) return 0.0;  
    
    return (zReceiver - zBlocker) * lightSize / zBlocker;
}

float PCSShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
 
    if(projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0)
        return 0.0;
    
    float currentDepth = projCoords.z;
    
    float searchRadius = 10.0;  
    float blockerDepth = findBlockerDepth(projCoords.xy, currentDepth, searchRadius);
    
    if(blockerDepth < 0.0)
        return 0.0;  
    
    float penumbraSize = calculatePenumbraSize(currentDepth, blockerDepth, lightSize);
    float bias = 0.005;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    vec2 poissonDisk[25] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790),
        vec2(-0.65465631, 0.77613958),
        vec2(0.87912464, 0.81544232),
        vec2(-0.47373420, -0.53742981),
        vec2(0.41893023, 0.26496911),
        vec2(-0.19090188, -0.79197514),
        vec2(0.99706507, 0.24188840),
        vec2(-0.91437590, -0.81409955),
        vec2(0.78641367, -0.19984126),
        vec2(0.14100790, 0.14383161)
    );
    
    for(int i = 0; i < 25; ++i)
    {
        vec2 offset = poissonDisk[i] * texelSize * penumbraSize;
        float pcfDepth = texture(shadowMap, projCoords.xy + offset).r; 
        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
    }
    shadow /= 25.0;
    
    return shadow;
}

void main()
{
    // Use material properties for lighting calculation
    vec3 ambient = material.ambient * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * material.diffuse * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = spec * material.specular * lightColor;

    // Shadow calculation
    float shadow = PCSShadowCalculation(FragPosLightSpace);
    
    // Calculate skybox reflection
    vec3 I = normalize(FragPos - viewPos);
    vec3 R = reflect(I, normalize(Normal));
    vec3 skyboxColor = texture(skybox, R).rgb;
    
    // Fresnel effect 
    float fresnel = pow(1.0 - max(dot(normalize(-I), norm), 0.0), 2.0);
    float finalReflectionStrength = mix(reflectionStrength * 0.2, reflectionStrength, fresnel);
    
    // Combine lighting and reflection
    vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specular);
    vec3 baseColor = lighting + material.emissive;
    
    // Mix base color with skybox reflection
    vec3 finalColor = mix(baseColor, skyboxColor, finalReflectionStrength);
    
    FragColor = vec4(finalColor, 1.0);
}
