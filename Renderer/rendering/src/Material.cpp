#include "Material.h"
#include "Shader.h"
#include "CoroutineResourceManager.h"
#include "Texture.h"
#include <glad/glad.h>
#include <unordered_set>

Material::Material() 
    : ambient(0.1f, 0.1f, 0.1f)
    , diffuse(0.7f, 0.7f, 0.7f)
    , specular(0.5f, 0.5f, 0.5f)
    , shininess(32.0f)
    , emissive(0.0f, 0.0f, 0.0f)
    , albedo(1.0f, 1.0f, 1.0f)
    , metallic(0.0f)
    , roughness(0.5f)
    , ao(1.0f)
    , heightScale(1.0f)
    , pbrEnabled(false) {}

Material Material::create_default() {
    Material material;
    material.set_ambient(glm::vec3(0.3f, 0.3f, 0.3f));   
    material.set_diffuse(glm::vec3(1.0f, 1.0f, 1.0f));   
    material.set_specular(glm::vec3(0.5f, 0.5f, 0.5f));
    material.set_shininess(32.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_metal() {
    Material material;
    material.set_ambient(glm::vec3(0.2f, 0.2f, 0.2f));
    material.set_diffuse(glm::vec3(0.8f, 0.8f, 0.8f));
    material.set_specular(glm::vec3(1.0f, 1.0f, 1.0f));
    material.set_shininess(128.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_plastic() {
    Material material;
    material.set_ambient(glm::vec3(0.0f, 0.0f, 0.0f));
    material.set_diffuse(glm::vec3(0.55f, 0.55f, 0.55f));
    material.set_specular(glm::vec3(0.7f, 0.7f, 0.7f));
    material.set_shininess(32.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_wood() {
    Material material;
    material.set_ambient(glm::vec3(0.4f, 0.2f, 0.1f));
    material.set_diffuse(glm::vec3(0.6f, 0.3f, 0.1f));
    material.set_specular(glm::vec3(0.1f, 0.1f, 0.1f));
    material.set_shininess(8.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_stone() {
    Material material;
    material.set_ambient(glm::vec3(0.2f, 0.2f, 0.2f));
    material.set_diffuse(glm::vec3(0.4f, 0.4f, 0.4f));
    material.set_specular(glm::vec3(0.1f, 0.1f, 0.1f));
    material.set_shininess(16.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}



// Legacy texture methods
void Material::set_diffuse_texture(const std::string& texturePath) {
    diffuseTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["diffuse"] = texturePath;
    } else {
        namedTexturePaths.erase("diffuse");
    }
}

void Material::set_specular_texture(const std::string& texturePath) {
    specularTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["specular"] = texturePath;
    } else {
        namedTexturePaths.erase("specular");
    }
}

void Material::set_normal_texture(const std::string& texturePath) {
    normalTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["normal"] = texturePath;
    } else {
        namedTexturePaths.erase("normal");
    }
}

void Material::set_emissive_texture(const std::string& texturePath) {
    emissiveTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["emissive"] = texturePath;
    } else {
        namedTexturePaths.erase("emissive");
    }
}

bool Material::has_diffuse_texture() const {
    return !diffuseTexturePath.empty();
}

bool Material::has_specular_texture() const {
    return !specularTexturePath.empty();
}

bool Material::has_normal_texture() const {
    return !normalTexturePath.empty();
}

bool Material::has_emissive_texture() const {
    return !emissiveTexturePath.empty();
}

// Enhanced texture management with named slots
void Material::add_texture(const std::string& name, const std::string& texturePath) {
    if (!texturePath.empty()) {
        namedTexturePaths[name] = texturePath;
        
        // Sync with legacy texture paths if applicable
        if (name == "diffuse") {
            diffuseTexturePath = texturePath;
        } else if (name == "specular") {
            specularTexturePath = texturePath;
        } else if (name == "normal") {
            normalTexturePath = texturePath;
        } else if (name == "emissive") {
            emissiveTexturePath = texturePath;
        }
        // Sync with PBR texture paths if applicable
        else if (name == "albedo") {
            albedoTexturePath = texturePath;
        } else if (name == "metallic") {
            metallicTexturePath = texturePath;
        } else if (name == "roughness") {
            roughnessTexturePath = texturePath;
        } else if (name == "ao") {
            aoTexturePath = texturePath;
        } else if (name == "height") {
            heightTexturePath = texturePath;
        } else if (name == "metallic_roughness") {
            metallicRoughnessTexturePath = texturePath;
        }
    } else {
        namedTexturePaths.erase(name);
    }
}

void Material::remove_texture(const std::string& name) {
    auto it = namedTexturePaths.find(name);
    if (it != namedTexturePaths.end()) {
        namedTexturePaths.erase(it);
        
        // Sync with legacy texture paths if applicable
        if (name == "diffuse") {
            diffuseTexturePath.clear();
        } else if (name == "specular") {
            specularTexturePath.clear();
        } else if (name == "normal") {
            normalTexturePath.clear();
        } else if (name == "emissive") {
            emissiveTexturePath.clear();
        }
        // Sync with PBR texture paths if applicable
        else if (name == "albedo") {
            albedoTexturePath.clear();
        } else if (name == "metallic") {
            metallicTexturePath.clear();
        } else if (name == "roughness") {
            roughnessTexturePath.clear();
        } else if (name == "ao") {
            aoTexturePath.clear();
        } else if (name == "height") {
            heightTexturePath.clear();
        } else if (name == "metallic_roughness") {
            metallicRoughnessTexturePath.clear();
        }
    }
}

bool Material::has_texture(const std::string& name) const {
    return namedTexturePaths.find(name) != namedTexturePaths.end();
}

// Texture iteration and management
const std::unordered_map<std::string, std::string>& Material::get_all_texture_paths() const {
    return namedTexturePaths;
}

std::vector<std::string> Material::get_texture_names() const {
    std::vector<std::string> names;
    names.reserve(namedTexturePaths.size());
    
    for (const auto& pair : namedTexturePaths) {
        names.push_back(pair.first);
    }
    
    return names;
}

size_t Material::get_texture_count() const {
    return namedTexturePaths.size();
}

const std::string& Material::get_texture_path(const std::string& name) const {
    auto it = namedTexturePaths.find(name);
    if (it != namedTexturePaths.end()) {
        return it->second;
    }
    static const std::string empty_string;
    return empty_string;
}

void Material::clear_all_textures() {
    namedTexturePaths.clear();
    
    // Clear legacy texture paths
    diffuseTexturePath.clear();
    specularTexturePath.clear();
    normalTexturePath.clear();
    emissiveTexturePath.clear();
    
    // Clear PBR texture paths
    albedoTexturePath.clear();
    metallicTexturePath.clear();
    roughnessTexturePath.clear();
    aoTexturePath.clear();
    heightTexturePath.clear();
    metallicRoughnessTexturePath.clear();
}

// Helper method to sync legacy textures with named texture system
void Material::syncLegacyTextures() {
    if (!diffuseTexturePath.empty()) {
        namedTexturePaths["diffuse"] = diffuseTexturePath;
    }
    if (!specularTexturePath.empty()) {
        namedTexturePaths["specular"] = specularTexturePath;
    }
    if (!normalTexturePath.empty()) {
        namedTexturePaths["normal"] = normalTexturePath;
    }
    if (!emissiveTexturePath.empty()) {
        namedTexturePaths["emissive"] = emissiveTexturePath;
    }
}

// PBR texture methods (using texture paths)
void Material::set_albedo_texture(const std::string& texturePath) {
    albedoTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["albedo"] = texturePath;
    } else {
        namedTexturePaths.erase("albedo");
    }
}

void Material::set_metallic_texture(const std::string& texturePath) {
    metallicTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["metallic"] = texturePath;
    } else {
        namedTexturePaths.erase("metallic");
    }
}

void Material::set_roughness_texture(const std::string& texturePath) {
    roughnessTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["roughness"] = texturePath;
    } else {
        namedTexturePaths.erase("roughness");
    }
}

void Material::set_ao_texture(const std::string& texturePath) {
    aoTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["ao"] = texturePath;
    } else {
        namedTexturePaths.erase("ao");
    }
}

void Material::set_height_texture(const std::string& texturePath) {
    heightTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["height"] = texturePath;
    } else {
        namedTexturePaths.erase("height");
    }
}

void Material::set_metallic_roughness_texture(const std::string& texturePath) {
    metallicRoughnessTexturePath = texturePath;
    if (!texturePath.empty()) {
        namedTexturePaths["metallic_roughness"] = texturePath;
    } else {
        namedTexturePaths.erase("metallic_roughness");
    }
}

bool Material::has_albedo_texture() const {
    return !albedoTexturePath.empty();
}

bool Material::has_metallic_texture() const {
    return !metallicTexturePath.empty();
}

bool Material::has_roughness_texture() const {
    return !roughnessTexturePath.empty();
}

bool Material::has_ao_texture() const {
    return !aoTexturePath.empty();
}

bool Material::has_height_texture() const {
    return !heightTexturePath.empty();
}

bool Material::has_metallic_roughness_texture() const {
    return !metallicRoughnessTexturePath.empty();
}

// Helper method to sync PBR textures with named texture system
void Material::syncPBRTextures() {
    if (!albedoTexturePath.empty()) {
        namedTexturePaths["albedo"] = albedoTexturePath;
    }
    if (!metallicTexturePath.empty()) {
        namedTexturePaths["metallic"] = metallicTexturePath;
    }
    if (!roughnessTexturePath.empty()) {
        namedTexturePaths["roughness"] = roughnessTexturePath;
    }
    if (!aoTexturePath.empty()) {
        namedTexturePaths["ao"] = aoTexturePath;
    }
    if (!heightTexturePath.empty()) {
        namedTexturePaths["height"] = heightTexturePath;
    }
    if (!metallicRoughnessTexturePath.empty()) {
        namedTexturePaths["metallic_roughness"] = metallicRoughnessTexturePath;
    }
}

// PBR Material preset creation
Material Material::create_pbr_default() {
    Material material;
    material.set_pbr_enabled(true);
    material.set_albedo(glm::vec3(0.5f, 0.8f, 0.1f));
    material.set_metallic(0.0f);
    material.set_roughness(0.7f);
    material.set_ao(1.0f);
    material.set_emissive(glm::vec3(0.1f, 0.3f, 0.1f));
    return material;
}

Material Material::create_pbr_metal() {
    Material material;
    material.set_pbr_enabled(true);
    material.set_albedo(glm::vec3(0.7f, 0.7f, 0.7f));
    material.set_metallic(1.0f);
    material.set_roughness(0.2f);
    material.set_ao(1.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_pbr_plastic() {
    Material material;
    material.set_pbr_enabled(true);
    material.set_albedo(glm::vec3(0.6f, 0.6f, 0.6f));
    material.set_metallic(0.0f);
    material.set_roughness(0.4f);
    material.set_ao(1.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_pbr_wood() {
    Material material;
    material.set_pbr_enabled(true);
    material.set_albedo(glm::vec3(0.6f, 0.3f, 0.1f));
    material.set_metallic(0.0f);
    material.set_roughness(0.8f);
    material.set_ao(1.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_pbr_stone() {
    Material material;
    material.set_pbr_enabled(true);
    material.set_albedo(glm::vec3(0.4f, 0.4f, 0.4f));
    material.set_metallic(0.0f);
    material.set_roughness(0.9f);
    material.set_ao(1.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_pbr_gold() {
    Material material;
    material.set_pbr_enabled(true);
    material.set_albedo(glm::vec3(1.0f, 0.766f, 0.336f));
    material.set_metallic(1.0f);
    material.set_roughness(0.1f);
    material.set_ao(1.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_pbr_chrome() {
    Material material;
    material.set_pbr_enabled(true);
    material.set_albedo(glm::vec3(0.55f, 0.55f, 0.55f));
    material.set_metallic(1.0f);
    material.set_roughness(0.05f);
    material.set_ao(1.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

Material Material::create_pbr_rubber() {
    Material material;
    material.set_pbr_enabled(true);
    material.set_albedo(glm::vec3(0.1f, 0.1f, 0.1f));
    material.set_metallic(0.0f);
    material.set_roughness(0.9f);
    material.set_ao(1.0f);
    material.set_emissive(glm::vec3(0.0f, 0.0f, 0.0f));
    return material;
}

// Shader integration methods
void Material::set_shader(const Shader& shader, const std::string& name) const {
    // Set basic material properties (Phong/Blinn-Phong)
    shader.set_vec3(name + ".ambient", ambient);
    shader.set_vec3(name + ".diffuse", diffuse);
    shader.set_vec3(name + ".specular", specular);
    shader.set_float(name + ".shininess", shininess);
    shader.set_vec3(name + ".emissive", emissive);
    
    // Set texture flags
    shader.set_bool(name + ".hasDiffuseTexture", has_diffuse_texture());
    shader.set_bool(name + ".hasSpecularTexture", has_specular_texture());
    shader.set_bool(name + ".hasNormalTexture", has_normal_texture());
    shader.set_bool(name + ".hasEmissiveTexture", has_emissive_texture());
}

void Material::set_shader_pbr(const Shader& shader, const std::string& prefix) const {
    // Set PBR material properties
    std::string metallic_name = prefix.empty() ? "materialMetallic" : prefix + "Metallic";
    std::string roughness_name = prefix.empty() ? "materialRoughness" : prefix + "Roughness";
    std::string ao_name = prefix.empty() ? "materialAO" : prefix + "AO";
    
    shader.set_float(metallic_name, metallic);
    shader.set_float(roughness_name, roughness);
    shader.set_float(ao_name, ao);
    
    // Set PBR texture flags
    shader.set_bool("hasAlbedoTexture", has_albedo_texture());
    shader.set_bool("hasMetallicTexture", has_metallic_texture());
    shader.set_bool("hasRoughnessTexture", has_roughness_texture());
    shader.set_bool("hasAOTexture", has_ao_texture());
    shader.set_bool("hasHeightTexture", has_height_texture());
    shader.set_bool("hasEmissiveTexture", has_emissive_texture());
}

// Simplified automatic texture binding using Texture's built-in slot management
void Material::bind_textures_auto(const Shader& shader, const CoroutineResourceManager& resource_manager) const {
    // Get material textures from resource manager
    auto material_textures = const_cast<CoroutineResourceManager&>(resource_manager).get_material_textures(*this);
    
    // Define standard texture binding configuration
    struct TextureBinding {
        std::string texture_name;
        std::vector<std::string> uniform_names;
        bool (Material::*has_texture_func)() const;
    };
    
    // Configure standard texture bindings - each texture can have multiple uniform names
    const std::vector<TextureBinding> bindings = {
        {"diffuse", {"diffuseTexture", "albedoTexture"}, &Material::has_diffuse_texture},
        {"normal", {"normalTexture"}, &Material::has_normal_texture},
        {"metallic", {"metallicTexture"}, &Material::has_metallic_texture},
        {"roughness", {"roughnessTexture"}, &Material::has_roughness_texture},
        {"ao", {"aoTexture"}, &Material::has_ao_texture},
        {"emissive", {"emissiveTexture"}, &Material::has_emissive_texture},
        {"specular", {"specularTexture"}, &Material::has_specular_texture}
    };
    
    // Bind textures using automatic slot allocation
    for (const auto& binding : bindings) {
        if ((this->*binding.has_texture_func)()) {
            auto texture_it = material_textures.find(binding.texture_name);
            if (texture_it != material_textures.end()) {
                // Use automatic slot allocation
                unsigned int slot = texture_it->second->bind_auto();
                if (slot != Texture::INVALID_SLOT) {
                    // Set all uniform names to the same slot
                    for (const auto& uniform_name : binding.uniform_names) {
                        shader.set_int(uniform_name, slot);
                    }
                }
            }
        }
    }
}

 