#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
class Texture;
class CoroutineResourceManager;

class Material {
public:
    Material();
    ~Material() = default;

    // Basic material properties (Legacy Phong/Blinn-Phong)
    void set_ambient(const glm::vec3& ambient) { this->ambient = ambient; }
    void set_diffuse(const glm::vec3& diffuse) { this->diffuse = diffuse; }
    void set_specular(const glm::vec3& specular) { this->specular = specular; }
    void set_shininess(float shininess) { this->shininess = shininess; }
    void set_emissive(const glm::vec3& emissive) { this->emissive = emissive; }

    glm::vec3 get_ambient() const { return ambient; }
    glm::vec3 get_diffuse() const { return diffuse; }
    glm::vec3 get_specular() const { return specular; }
    float get_shininess() const { return shininess; }
    glm::vec3 get_emissive() const { return emissive; }

    // PBR material properties
    void set_albedo(const glm::vec3& albedo) { this->albedo = albedo; }
    void set_metallic(float metallic) { this->metallic = glm::clamp(metallic, 0.0f, 1.0f); }
    void set_roughness(float roughness) { this->roughness = glm::clamp(roughness, 0.0f, 1.0f); }
    void set_ao(float ao) { this->ao = glm::clamp(ao, 0.0f, 1.0f); }
    void set_height_scale(float scale) { this->heightScale = scale; }

    glm::vec3 get_albedo() const { return albedo; }
    float get_metallic() const { return metallic; }
    float get_roughness() const { return roughness; }
    float get_ao() const { return ao; }
    float get_height_scale() const { return heightScale; }

    // PBR rendering mode
    void set_pbr_enabled(bool enabled) { pbrEnabled = enabled; }
    bool is_pbr_enabled() const { return pbrEnabled; }

    // Legacy texture methods (for backward compatibility) - using texture paths
    void set_diffuse_texture(const std::string& texture_path);
    void set_specular_texture(const std::string& texture_path);
    void set_normal_texture(const std::string& texture_path);
    void set_emissive_texture(const std::string& texture_path);

    std::shared_ptr<Texture> get_diffuse_texture() const;
    std::shared_ptr<Texture> get_specular_texture() const;
    std::shared_ptr<Texture> get_normal_texture() const;
    std::shared_ptr<Texture> get_emissive_texture() const;

    const std::string& get_diffuse_texture_path() const { return diffuseTexturePath; }
    const std::string& get_specular_texture_path() const { return specularTexturePath; }
    const std::string& get_normal_texture_path() const { return normalTexturePath; }
    const std::string& get_emissive_texture_path() const { return emissiveTexturePath; }

    bool has_diffuse_texture() const;
    bool has_specular_texture() const;
    bool has_normal_texture() const;
    bool has_emissive_texture() const;

    // PBR texture methods - using texture paths
    void set_albedo_texture(const std::string& texture_path);
    void set_metallic_texture(const std::string& texture_path);
    void set_roughness_texture(const std::string& texture_path);
    void set_ao_texture(const std::string& texture_path);
    void set_height_texture(const std::string& texture_path);
    void set_metallic_roughness_texture(const std::string& texture_path);

    std::shared_ptr<Texture> get_albedo_texture() const;
    std::shared_ptr<Texture> get_metallic_texture() const;
    std::shared_ptr<Texture> get_roughness_texture() const;
    std::shared_ptr<Texture> get_ao_texture() const;
    std::shared_ptr<Texture> get_height_texture() const;
    std::shared_ptr<Texture> get_metallic_roughness_texture() const;

    bool has_albedo_texture() const;
    bool has_metallic_texture() const;
    bool has_roughness_texture() const;
    bool has_ao_texture() const;
    bool has_height_texture() const;
    bool has_metallic_roughness_texture() const;

    // Enhanced texture management with named slots
    void add_texture(const std::string& name, const std::string& texture_path);
    void remove_texture(const std::string& name);
    std::shared_ptr<Texture> get_texture(const std::string& name) const;
    const std::string& get_texture_path(const std::string& name) const;
    bool has_texture(const std::string& name) const;

    // Texture iteration and management
    const std::unordered_map<std::string, std::string>& get_all_texture_paths() const;
    std::vector<std::string> get_texture_names() const;
    size_t get_texture_count() const;
    void clear_all_textures();

    // Shader integration methods
    void set_shader(const class Shader& shader, const std::string& name = "material") const;
    void set_shader_pbr(const class Shader& shader, const std::string& prefix = "") const;
    void bind_textures(const class Shader& shader, const class CoroutineResourceManager& resource_manager) const;
    void bind_textures(const class Shader& shader, const class CoroutineResourceManager& resource_manager, 
                      const std::unordered_map<std::string, unsigned int>& texture_slots) const;

    // Texture slot operations (using texture paths)
    void set_texture_at_slot(unsigned int slot, const std::string& texture_path);
    std::shared_ptr<Texture> get_texture_at_slot(unsigned int slot) const;
    const std::string& get_texture_path_at_slot(unsigned int slot) const;
    bool has_texture_at_slot(unsigned int slot) const;

    // Material preset creation (Legacy)
    static Material create_default();
    static Material create_metal();
    static Material create_plastic();
    static Material create_wood();
    static Material create_stone();

    // PBR Material preset creation
    static Material create_pbr_default();
    static Material create_pbr_metal();
    static Material create_pbr_plastic();
    static Material create_pbr_wood();
    static Material create_pbr_stone();
    static Material create_pbr_gold();
    static Material create_pbr_chrome();
    static Material create_pbr_rubber();

private:
    // Basic material properties (Phong/Blinn-Phong)
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
    glm::vec3 emissive;

    // PBR material properties
    glm::vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    float heightScale;
    bool pbrEnabled;

    // Enhanced texture storage with named slots (using paths)
    std::unordered_map<std::string, std::string> namedTexturePaths;
    
    // Legacy texture paths (for backward compatibility)
    std::string diffuseTexturePath;
    std::string specularTexturePath;
    std::string normalTexturePath;
    std::string emissiveTexturePath;

    // PBR texture paths
    std::string albedoTexturePath;
    std::string metallicTexturePath;
    std::string roughnessTexturePath;
    std::string aoTexturePath;
    std::string heightTexturePath;
    std::string metallicRoughnessTexturePath;

    // Helper methods
    void syncLegacyTextures();
    void syncPBRTextures();
    
    // Helper method to get texture from resource manager
    std::shared_ptr<Texture> getTextureFromManager(const std::string& path) const;
}; 