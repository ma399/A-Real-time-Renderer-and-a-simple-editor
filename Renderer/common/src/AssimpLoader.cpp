#include <algorithm>
#include <filesystem>
#include <vector>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>

#include "AssimpLoader.h"

const std::vector<std::string> AssimpLoader::supportedExtensions = {
    ".obj", ".fbx", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".stl", ".ply"
};

AssimpLoader::AssimpLoader() = default;

AssimpLoader::~AssimpLoader() = default;

bool AssimpLoader::can_load(const std::string& filePath) const {
    std::string extension = get_file_extension(filePath);
    return std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end();
}

void AssimpLoader::load_model(const std::string& filePath, std::vector<Mesh::Vertex>& vertices, std::vector<Mesh::Indices>& indices) {
    
    Assimp::Importer importer;
    
    // Set post-processing flags
    const aiScene* scene = importer.ReadFile(filePath, 
        aiProcess_Triangulate |           // Convert polygons to triangles
        aiProcess_FlipUVs |              // Flip UV coordinates (OpenGL convention)
        aiProcess_GenSmoothNormals |     // Generate smooth normals if missing
        aiProcess_CalcTangentSpace |     // Generate tangent and bitangent vectors
        aiProcess_JoinIdenticalVertices | // Remove duplicate vertices
        aiProcess_ImproveCacheLocality |  // Optimize vertex cache locality
        aiProcess_RemoveRedundantMaterials | // Remove redundant materials
        aiProcess_OptimizeMeshes |        // Optimize mesh count
        aiProcess_OptimizeGraph          // Optimize scene graph
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string error = importer.GetErrorString();
        throw std::runtime_error("Failed to load model with Assimp: " + error);
    }
    
    // Process the scene graph starting from root node
    process_node(scene->mRootNode, scene, vertices, indices);
    
    if (vertices.empty()) {
        throw std::runtime_error("No valid geometry found in file: " + filePath);
    }
    
    LOG_INFO("AssimpLoader: Successfully loaded {} vertices from {}", vertices.size(), filePath);
    LOG_INFO("  - Meshes: {}", scene->mNumMeshes);
    LOG_INFO("  - Materials: {}", scene->mNumMaterials);
    
    return;
}

LoadedModelData AssimpLoader::load_model_with_textures(const std::string& filePath) {
    LoadedModelData model_data;
    
    Assimp::Importer importer;
    
    // Set post-processing flags
    const aiScene* scene = importer.ReadFile(filePath, 
        aiProcess_Triangulate |           // Convert polygons to triangles
        aiProcess_FlipUVs |              // Flip UV coordinates (OpenGL convention)
        aiProcess_GenSmoothNormals |     // Generate smooth normals if missing
        aiProcess_CalcTangentSpace |     // Generate tangent and bitangent vectors
        aiProcess_JoinIdenticalVertices | // Remove duplicate vertices
        aiProcess_ImproveCacheLocality |  // Optimize vertex cache locality
        aiProcess_RemoveRedundantMaterials | // Remove redundant materials
        aiProcess_OptimizeMeshes |        // Optimize mesh count
        aiProcess_OptimizeGraph          // Optimize scene graph
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::string error = importer.GetErrorString();
        throw std::runtime_error("Failed to load model with Assimp: " + error);
    }
    
    // Store model directory for texture path resolution (very important!)
    model_directory_ = get_directory_from_path(filePath);
    LOG_INFO("AssimpLoader: Model directory: {}", model_directory_);
    
    // Clear texture cache for this model loading session
    loaded_textures_.clear();
    
    // Process materials first to collect texture paths
    model_data.materials.reserve(scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        Material material = process_material(scene->mMaterials[i], scene, model_directory_);
        model_data.materials.push_back(material);
        
        // Collect all texture paths from this material (only basic textures have path getters)
        if (material.has_diffuse_texture()) {
            model_data.texture_paths["diffuse_" + std::to_string(i)] = material.get_diffuse_texture_path();
        }
        if (material.has_specular_texture()) {
            model_data.texture_paths["specular_" + std::to_string(i)] = material.get_specular_texture_path();
        }
        if (material.has_normal_texture()) {
            model_data.texture_paths["normal_" + std::to_string(i)] = material.get_normal_texture_path();
        }
        if (material.has_emissive_texture()) {
            model_data.texture_paths["emissive_" + std::to_string(i)] = material.get_emissive_texture_path();
        }
        
        // For PBR textures, we need to get paths from the named texture system
        const auto& all_texture_paths = material.get_all_texture_paths();
        for (const auto& [name, path] : all_texture_paths) {
            model_data.texture_paths[name + "_" + std::to_string(i)] = path;
        }
    }
    
    // Process the scene graph starting from root node
    process_node_with_materials(scene->mRootNode, scene, model_data);
    
    if (model_data.meshes.empty()) {
        throw std::runtime_error("No valid geometry found in file: " + filePath);
    }
    
    // Calculate total vertices for logging
    size_t total_vertices = 0;
    for (const auto& mesh : model_data.meshes) {
        total_vertices += mesh.vertices.size();
    }
    
    LOG_INFO("AssimpLoader: Successfully loaded {} meshes with {} total vertices from {}", model_data.meshes.size(), total_vertices, filePath);
    LOG_INFO("  - Meshes: {}", scene->mNumMeshes);
    LOG_INFO("  - Materials: {}", scene->mNumMaterials);
    LOG_INFO("  - Unique textures found: {}", model_data.texture_paths.size());
    
    return model_data;
}

std::vector<std::string> AssimpLoader::get_supported_extensions() const {
    return supportedExtensions;
}

//TODO: multiple meshes scene
void AssimpLoader::process_node(aiNode* node, const aiScene* scene, std::vector<Mesh::Vertex>& vertices, std::vector<Mesh::Indices>& indices) {
    // Process all meshes in the current node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        process_mesh(mesh, scene, vertices, indices);
    }
    
    // Recursively process child nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        process_node(node->mChildren[i], scene, vertices, indices);
    }
}

void AssimpLoader::process_mesh(aiMesh* mesh, const aiScene*, std::vector<Mesh::Vertex>& vertices, std::vector<Mesh::Indices>& indices) {

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Mesh::Vertex vertex;
        // Position
        if (mesh->mVertices) {
            vertex.position = glm::vec3(
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            );
        } else {
            vertex.position = glm::vec3(0.0f);
        }
        
        // Normal
        if (mesh->mNormals) {
            vertex.normal = glm::vec3(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );
        } else {
            vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);  // Default normal
        }
        
        // UV coordinates
        if (mesh->mTextureCoords[0]) {
            vertex.texCoords = glm::vec2(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            );
        } else {
            // Generate basic UV coordinates based on vertex position
            // This is a simple planar mapping for models without UV
            vertex.texCoords = glm::vec2(
                (vertex.position.x + 1.0f) * 0.5f,  // Map X from [-1,1] to [0,1]
                (vertex.position.y + 1.0f) * 0.5f   // Map Y from [-1,1] to [0,1]
            );
        }
        
        // Tangent
        if (mesh->mTangents) {
            vertex.tangent = glm::vec3(
                mesh->mTangents[i].x,
                mesh->mTangents[i].y,
                mesh->mTangents[i].z
            );
        } else {
            vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);  // Default tangent
        }

        vertices.emplace_back(std::move(vertex));
    }

    auto vertex_offset = static_cast<unsigned int>(vertices.size() - mesh->mNumVertices);
    
    // Process faces to extract indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        
        if (face.mNumIndices == 3) {
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.emplace_back(static_cast<unsigned int>(face.mIndices[j] + vertex_offset));
            }
        } else {
            LOG_WARN("AssimpLoader: Face {} has {} vertices (expected 3). Skipping.", i, face.mNumIndices);
        }
    }
}

std::string AssimpLoader::get_file_extension(const std::string& filePath) const {
    std::filesystem::path path(filePath);
    std::string extension = path.extension().string();
    
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    return extension;
}

std::string AssimpLoader::get_directory_from_path(const std::string& filePath) const {
    std::filesystem::path path(filePath);
    return path.parent_path().string();
}

void AssimpLoader::process_node_with_materials(aiNode* node, const aiScene* scene, LoadedModelData& model_data) {
    // Process all meshes in the current node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        MeshData mesh_data = process_mesh_with_materials(mesh, scene);
        
        // Set mesh name for debugging (combine node name and mesh index)
        mesh_data.name = std::string(node->mName.C_Str()) + "_mesh_" + std::to_string(i);
        
        LOG_DEBUG("Processed mesh '{}' with {} vertices and material index {}", 
                  mesh_data.name, mesh_data.vertices.size(), mesh_data.material_index);
        
        model_data.meshes.push_back(std::move(mesh_data));
    }
    
    // Recursively process child nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        process_node_with_materials(node->mChildren[i], scene, model_data);
    }
}

MeshData AssimpLoader::process_mesh_with_materials(aiMesh* mesh, const aiScene* scene) {
    MeshData mesh_data;
    
    // Store material index
    mesh_data.material_index = mesh->mMaterialIndex;
    
    // Process vertices
    mesh_data.vertices.reserve(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Mesh::Vertex vertex;
        // Position
        if (mesh->mVertices) {
            vertex.position = glm::vec3(
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            );
        } else {
            vertex.position = glm::vec3(0.0f);
        }
        
        // Normal
        if (mesh->mNormals) {
            vertex.normal = glm::vec3(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );
        } else {
            vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f);  // Default normal
        }
        
        // UV coordinates
        if (mesh->mTextureCoords[0]) {
            vertex.texCoords = glm::vec2(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            );
        } else {
            // Generate basic UV coordinates based on vertex position
            vertex.texCoords = glm::vec2(
                (vertex.position.x + 1.0f) * 0.5f,  // Map X from [-1,1] to [0,1]
                (vertex.position.y + 1.0f) * 0.5f   // Map Y from [-1,1] to [0,1]
            );
        }
        
        // Tangent
        if (mesh->mTangents) {
            vertex.tangent = glm::vec3(
                mesh->mTangents[i].x,
                mesh->mTangents[i].y,
                mesh->mTangents[i].z
            );
        } else {
            vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);  // Default tangent
        }

        mesh_data.vertices.emplace_back(std::move(vertex));
    }
    
    // Process faces to extract indices
    // Estimate indices count (assuming mostly triangles)
    mesh_data.indices.reserve(mesh->mNumFaces * 3);
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        
        if (face.mNumIndices == 3) {
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                mesh_data.indices.emplace_back(static_cast<unsigned int>(face.mIndices[j]));
            }
        } else {
            LOG_WARN("AssimpLoader: Face {} has {} vertices (expected 3). Skipping.", i, face.mNumIndices);
        }
    }
    
    return mesh_data;
}

Material AssimpLoader::process_material(aiMaterial* ai_material, const aiScene* scene, const std::string& model_directory) {
    Material material;
    
    // Get material name
    aiString name;
    if (ai_material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
        LOG_DEBUG("Processing material: {}", name.C_Str());
    }
    
    // Load basic material properties
    aiColor3D color(0.0f, 0.0f, 0.0f);
    float value = 0.0f;
    
    // Ambient color
    if (ai_material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
        material.set_ambient(glm::vec3(color.r, color.g, color.b));
    }
    
    // Diffuse color
    if (ai_material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        material.set_diffuse(glm::vec3(color.r, color.g, color.b));
        material.set_albedo(glm::vec3(color.r, color.g, color.b)); // For PBR
    }
    
    // Specular color
    if (ai_material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
        material.set_specular(glm::vec3(color.r, color.g, color.b));
    }
    
    // Emissive color
    if (ai_material->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) {
        material.set_emissive(glm::vec3(color.r, color.g, color.b));
    }
    
    // Shininess
    if (ai_material->Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS) {
        material.set_shininess(value);
    }
    
    // PBR properties
    if (ai_material->Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS) {
        material.set_metallic(value);
        material.set_pbr_enabled(true);
    }
    
    if (ai_material->Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS) {
        material.set_roughness(value);
        material.set_pbr_enabled(true);
    }
    
    // Load textures
    std::vector<std::string> diffuse_textures = load_material_textures(ai_material, aiTextureType_DIFFUSE, "diffuse");
    for (const auto& texture_path : diffuse_textures) {
        material.set_diffuse_texture(texture_path);
        material.set_albedo_texture(texture_path); // For PBR
        break; // Use first diffuse texture
    }
    
    std::vector<std::string> specular_textures = load_material_textures(ai_material, aiTextureType_SPECULAR, "specular");
    for (const auto& texture_path : specular_textures) {
        material.set_specular_texture(texture_path);
        break; // Use first specular texture
    }
    
    std::vector<std::string> normal_textures = load_material_textures(ai_material, aiTextureType_NORMALS, "normal");
    for (const auto& texture_path : normal_textures) {
        material.set_normal_texture(texture_path);
        break; // Use first normal texture
    }
    
    std::vector<std::string> height_textures = load_material_textures(ai_material, aiTextureType_HEIGHT, "height");
    for (const auto& texture_path : height_textures) {
        material.set_height_texture(texture_path);
        break; // Use first height texture
    }
    
    // PBR textures
    std::vector<std::string> metallic_textures = load_material_textures(ai_material, aiTextureType_METALNESS, "metallic");
    for (const auto& texture_path : metallic_textures) {
        material.set_metallic_texture(texture_path);
        material.set_pbr_enabled(true);
        break;
    }
    
    std::vector<std::string> roughness_textures = load_material_textures(ai_material, aiTextureType_DIFFUSE_ROUGHNESS, "roughness");
    for (const auto& texture_path : roughness_textures) {
        material.set_roughness_texture(texture_path);
        material.set_pbr_enabled(true);
        break;
    }
    
    std::vector<std::string> ao_textures = load_material_textures(ai_material, aiTextureType_AMBIENT_OCCLUSION, "ao");
    for (const auto& texture_path : ao_textures) {
        material.set_ao_texture(texture_path);
        material.set_pbr_enabled(true);
        break;
    }
    
    return material;
}

std::vector<std::string> AssimpLoader::load_material_textures(aiMaterial* material, unsigned int texture_type, const std::string& type_name) {
    std::vector<std::string> texture_paths;
    
    unsigned int texture_count = material->GetTextureCount(static_cast<aiTextureType>(texture_type));
    
    for (unsigned int i = 0; i < texture_count; i++) {
        aiString texture_filename;
        if (material->GetTexture(static_cast<aiTextureType>(texture_type), i, &texture_filename) == AI_SUCCESS) {
            std::string relative_path = texture_filename.C_Str();
            
            // Construct full path: model_directory + '/' + relative_path
            std::string full_path = model_directory_ + "/" + relative_path;
            
            if (loaded_textures_.find(full_path) == loaded_textures_.end()) {
                // Check if texture file exists
                if (std::filesystem::exists(full_path)) {
                    texture_paths.push_back(full_path);
                    loaded_textures_[full_path] = true;  // Mark as loaded
                    LOG_INFO("Found {} texture: {} -> {}", type_name, relative_path, full_path);
                } else {
                    LOG_WARN("Texture file not found: {} (referenced as {})", full_path, relative_path);
                }
            }
        }
    }
    
    return texture_paths;
}

std::string AssimpLoader::get_texture_path(const std::string& model_directory, const std::string& texture_filename) {
    // Handle different path formats
    std::string filename = texture_filename;
    
    // Remove any leading path separators or drive letters from texture filename
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        filename = filename.substr(last_slash + 1);
    }
    
    // Construct full path
    std::filesystem::path full_path = std::filesystem::path(model_directory) / filename;
    return full_path.string();
}



