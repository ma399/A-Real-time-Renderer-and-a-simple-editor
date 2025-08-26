#version 460 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec3 WorldPos;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    vec3 emissive;
    bool hasDiffuseTexture;
    bool hasSpecularTexture;
    bool hasNormalTexture;
    bool hasEmissiveTexture;
};

struct Light {
    int type; // 0: directional, 1: point, 2: spot
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float range;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
};

uniform Material material;
uniform Light lights[16]; 
uniform int numLights;
uniform vec3 viewPos;
uniform vec3 ambientLight;

uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
uniform sampler2D normalTexture;
uniform sampler2D emissiveTexture;

vec3 calculateDirectionalLight(Light light, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specularColor) {
    vec3 lightDir = normalize(-light.direction);
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.intensity * diff * light.color * diffuseColor;
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.intensity * spec * light.color * specularColor;
    
    return diffuse + specular;
}

vec3 calculatePointLight(Light light, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specularColor) {
    vec3 lightDir = normalize(light.position - FragPos);
    float distance = length(light.position - FragPos);
    
    if (distance > light.range) return vec3(0.0);
    
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.intensity * attenuation * diff * light.color * diffuseColor;
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.intensity * attenuation * spec * light.color * specularColor;
    
    return diffuse + specular;
}

vec3 calculateSpotLight(Light light, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specularColor) {
    vec3 lightDir = normalize(light.position - FragPos);
    float distance = length(light.position - FragPos);
    
    if (distance > light.range) return vec3(0.0);
    
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    
    float cosTheta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((cosTheta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = light.intensity * attenuation * intensity * diff * light.color * diffuseColor;
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.intensity * attenuation * intensity * spec * light.color * specularColor;
    
    return diffuse + specular;
}

void main() {
    vec3 diffuseColor = material.diffuse;
    vec3 specularColor = material.specular;
    
    if (material.hasDiffuseTexture) {
        diffuseColor *= texture(diffuseTexture, TexCoords).rgb;
    }
    
    if (material.hasSpecularTexture) {
        specularColor *= texture(specularTexture, TexCoords).rgb;
    }
    
    vec3 normal = normalize(Normal);
    if (material.hasNormalTexture) {
        normal = normalize(normal + texture(normalTexture, TexCoords).rgb * 2.0 - 1.0);
    }
    
    vec3 viewDir = normalize(viewPos - FragPos);
    
    vec3 ambient = ambientLight * material.ambient * diffuseColor;
    
    vec3 result = ambient;
    
    for (int i = 0; i < numLights; i++) {
        if (lights[i].type == 0) {
            result += calculateDirectionalLight(lights[i], normal, viewDir, diffuseColor, specularColor);
        } else if (lights[i].type == 1) {
            result += calculatePointLight(lights[i], normal, viewDir, diffuseColor, specularColor);
        } else if (lights[i].type == 2) {
            result += calculateSpotLight(lights[i], normal, viewDir, diffuseColor, specularColor);
        }
    }
    
    if (material.hasEmissiveTexture) {
        result += texture(emissiveTexture, TexCoords).rgb * material.emissive;
    } else {
        result += material.emissive;
    }
    
    FragColor = vec4(result, 1.0);
} 