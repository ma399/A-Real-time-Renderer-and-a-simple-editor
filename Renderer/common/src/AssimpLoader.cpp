#include <algorithm>
#include <filesystem>
#include <vector>
#include <string>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <AssimpLoader.h>

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



