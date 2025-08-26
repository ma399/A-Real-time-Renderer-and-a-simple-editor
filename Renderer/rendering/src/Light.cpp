#include "Light.h"
#include "Shader.h"
#include <memory>
#include <iostream>
#include <string>

// Static member definitions
unsigned int Light::light_vao = 0;
unsigned int Light::light_vbo = 0;
bool Light::mesh_initialized = false;

Light::Light(Type type, const glm::vec3& position, const glm::vec3& color)
    : type_(type), position_(position), color_(color), intensity_(1.0f) {
    if (!mesh_initialized) {
        setup_light_mesh();
    }
}

Light::~Light() {}


DirectionalLight::DirectionalLight(const glm::vec3& direction, const glm::vec3& color)
    : Light(Type::kDirectional, glm::vec3(0.0f), color), direction_(glm::normalize(direction)) {}


PointLight::PointLight(const glm::vec3& position, const glm::vec3& color, float range)
    : Light(Type::kPoint, position, color), range_(range) {
    
    constant_ = 1.0f;
    linear_ = 4.5f / range;
    quadratic_ = 75.0f / (range * range);
}

float PointLight::get_attenuation(float distance) const {
    if (distance > range_) return 0.0f;
    return 1.0f / (constant_ + linear_ * distance + quadratic_ * distance * distance);
}


SpotLight::SpotLight(const glm::vec3& position, const glm::vec3& direction, 
                     const glm::vec3& color, float cutOff, float outer_cut_off)
    : Light(Type::kSpot, position, color), direction_(glm::normalize(direction)),
      cut_off_(glm::cos(glm::radians(cutOff))), outer_cut_off_(glm::cos(glm::radians(outer_cut_off))) {
    
    constant_ = 1.0f;
    linear_ = 0.09f;
    quadratic_ = 0.032f;
}

float SpotLight::get_attenuation(float distance) const {
    return 1.0f / (constant_ + linear_ * distance + quadratic_ * distance * distance);
}

float SpotLight::get_spot_attenuation(const glm::vec3& lightDir) const {
    float cosTheta = glm::dot(lightDir, direction_);
    float epsilon = cut_off_ - outer_cut_off_;
    float intensity = glm::clamp((cosTheta - outer_cut_off_) / epsilon, 0.0f, 1.0f);
    return intensity;
}


LightManager::LightManager() : ambient_light_(0.5f, 0.5f, 0.5f) {}

LightManager::~LightManager() = default;

void LightManager::add_light(std::unique_ptr<Light> light) {
    lights_.push_back(std::move(light));
}

void LightManager::remove_light(size_t index) {
    if (index < lights_.size()) {
        lights_.erase(lights_.begin() + index);
    }
}

void Light::setup_light_mesh() {
    if (mesh_initialized) return;
    
    // Simple cube vertices for light visualization
    float vertices[] = {
        // Front face
        -0.1f, -0.1f,  0.1f,
         0.1f, -0.1f,  0.1f,
         0.1f,  0.1f,  0.1f,
         0.1f,  0.1f,  0.1f,
        -0.1f,  0.1f,  0.1f,
        -0.1f, -0.1f,  0.1f,

        // Back face
        -0.1f, -0.1f, -0.1f,
         0.1f, -0.1f, -0.1f,
         0.1f,  0.1f, -0.1f,
         0.1f,  0.1f, -0.1f,
        -0.1f,  0.1f, -0.1f,
        -0.1f, -0.1f, -0.1f,

        // Left face
        -0.1f,  0.1f,  0.1f,
        -0.1f,  0.1f, -0.1f,
        -0.1f, -0.1f, -0.1f,
        -0.1f, -0.1f, -0.1f,
        -0.1f, -0.1f,  0.1f,
        -0.1f,  0.1f,  0.1f,

        // Right face
         0.1f,  0.1f,  0.1f,
         0.1f,  0.1f, -0.1f,
         0.1f, -0.1f, -0.1f,
         0.1f, -0.1f, -0.1f,
         0.1f, -0.1f,  0.1f,
         0.1f,  0.1f,  0.1f,

        // Bottom face
        -0.1f, -0.1f, -0.1f,
         0.1f, -0.1f, -0.1f,
         0.1f, -0.1f,  0.1f,
         0.1f, -0.1f,  0.1f,
        -0.1f, -0.1f,  0.1f,
        -0.1f, -0.1f, -0.1f,

        // Top face
        -0.1f,  0.1f, -0.1f,
         0.1f,  0.1f, -0.1f,
         0.1f,  0.1f,  0.1f,
         0.1f,  0.1f,  0.1f,
        -0.1f,  0.1f,  0.1f,
        -0.1f,  0.1f, -0.1f
    };

    glGenVertexArrays(1, &light_vao);
    glGenBuffers(1, &light_vbo);

    glBindVertexArray(light_vao);

    glBindBuffer(GL_ARRAY_BUFFER, light_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    
    mesh_initialized = true;
    std::cout << "Light mesh initialized with VAO: " << light_vao << std::endl;
}

void Light::render() const {
    if (!mesh_initialized || type_ == Type::kDirectional) {
        // Don't render directional lights as they don't have a position
        return;
    }

    glBindVertexArray(light_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void Light::set_shader(const Shader& shader) const {
    set_common_shader(shader);
    set_unique_shader(shader);
}

void Light::set_shader_array(const Shader& shader, int index) const {
    set_common_shader_array(shader, index);
    set_unique_shader_array(shader, index);
}

void Light::set_common_shader(const Shader& shader) const {
    shader.set_vec3("light.position", position_);
    shader.set_vec3("light.color", color_);
    shader.set_float("light.intensity", intensity_);
    shader.set_int("light.type", static_cast<int>(type_));
}

void Light::set_common_shader_array(const Shader& shader, int index) const {
    std::string base = "lightPositions[" + std::to_string(index) + "]";
    shader.set_vec3(base, position_);
    
    base = "lightColors[" + std::to_string(index) + "]";
    shader.set_vec3(base, color_);
    
    base = "lightIntensities[" + std::to_string(index) + "]";
    shader.set_float(base, intensity_);
    
    base = "lightTypes[" + std::to_string(index) + "]";
    shader.set_int(base, static_cast<int>(type_));
}

void DirectionalLight::set_unique_shader(const Shader& shader) const {
    shader.set_vec3("light.direction", direction_);
}

void DirectionalLight::set_unique_shader_array(const Shader& shader, int index) const {
    std::string base = "lightDirections[" + std::to_string(index) + "]";
    shader.set_vec3(base, direction_);
}

void PointLight::set_unique_shader(const Shader& shader) const {
    shader.set_float("light.range", range_);
    shader.set_float("light.constant", constant_);
    shader.set_float("light.linear", linear_);
    shader.set_float("light.quadratic", quadratic_);
}

void PointLight::set_unique_shader_array(const Shader& shader, int index) const {
    std::string base = "lightRanges[" + std::to_string(index) + "]";
    shader.set_float(base, range_);
    
    // Direction is not used for point lights, but set to zero vector
    base = "lightDirections[" + std::to_string(index) + "]";
    shader.set_vec3(base, glm::vec3(0.0f));
}

void SpotLight::set_unique_shader(const Shader& shader) const {
    shader.set_vec3("light.direction", direction_);
    shader.set_float("light.cutOff", cut_off_);
    shader.set_float("light.outerCutOff", outer_cut_off_);
    shader.set_float("light.constant", constant_);
    shader.set_float("light.linear", linear_);
    shader.set_float("light.quadratic", quadratic_);
}

void SpotLight::set_unique_shader_array(const Shader& shader, int index) const {
    std::string base = "lightDirections[" + std::to_string(index) + "]";
    shader.set_vec3(base, direction_);
    
    base = "lightRanges[" + std::to_string(index) + "]";
    shader.set_float(base, 25.0f); // Default spot light range
    
    base = "lightInnerCones[" + std::to_string(index) + "]";
    shader.set_float(base, cut_off_);
    
    base = "lightOuterCones[" + std::to_string(index) + "]";
    shader.set_float(base, outer_cut_off_);
} 