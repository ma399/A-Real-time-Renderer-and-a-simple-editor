#pragma once

#include <vector>
#include <string>
#include <memory>
#include <span>
#include <unordered_map>

#include "Mesh.h"
#include "Material.h"
#include "Logger.h"

// Forward declarations
struct aiScene;
struct aiMesh;
struct aiNode;
struct aiMaterial;


// Structure to hold individual mesh data
struct MeshData {
    std::vector<Mesh::Vertex> vertices;
    std::vector<Mesh::Indices> indices;
    unsigned int material_index; // Index into the materials array
    std::string name; // Mesh name for debugging/identification
};

// Structure to hold loaded model data including textures
struct LoadedModelData {
    std::vector<MeshData> meshes; // Individual meshes with their own vertices/indices
    std::vector<Material> materials;
    std::unordered_map<std::string, std::string> texture_paths; // texture name -> file path
};

// Assimp-based loader implementation
class AssimpLoader {
public:
    AssimpLoader();
    ~AssimpLoader();
    
    bool can_load(const std::string& file_path) const;
    
    // Legacy method for backward compatibility
    void load_model(const std::string& file_path, std::vector<Mesh::Vertex>& vertices, std::vector<Mesh::Indices>& indices);
    
    // Enhanced method that loads textures and materials
    LoadedModelData load_model_with_textures(const std::string& file_path);
    
    std::vector<std::string> get_supported_extensions() const;
    
private:
    void process_node(aiNode* node, const aiScene* scene, std::vector<Mesh::Vertex>& vertices, std::vector<Mesh::Indices>& indices);
    void process_mesh(aiMesh* mesh, const aiScene* scene, std::vector<Mesh::Vertex>& vertices, std::vector<Mesh::Indices>& indices);
    
    // Enhanced processing methods for texture support
    void process_node_with_materials(aiNode* node, const aiScene* scene, LoadedModelData& model_data);
    MeshData process_mesh_with_materials(aiMesh* mesh, const aiScene* scene);
    Material process_material(aiMaterial* ai_material, const aiScene* scene, const std::string& model_directory);
    
    // Helper methods for texture processing
    std::vector<std::string> load_material_textures(aiMaterial* material, unsigned int texture_type, const std::string& type_name);
    std::string get_texture_path(const std::string& model_path, const std::string& texture_filename);
    std::string get_file_extension(const std::string& file_path) const;
    std::string get_directory_from_path(const std::string& file_path) const;
    
    static const std::vector<std::string> supportedExtensions;
    
    // Model loading state
    std::string model_directory_;  // Directory of the currently loading model
    std::unordered_map<std::string, bool> loaded_textures_;  // Texture cache for current model
};
