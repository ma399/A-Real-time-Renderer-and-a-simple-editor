#pragma once

#include <vector>
#include <string>
#include <memory>
#include <span>

#include <Mesh.h>
#include <Logger.h>

// Forward declarations
struct aiScene;
struct aiMesh;
struct aiNode;


// Assimp-based loader implementation
class AssimpLoader {
public:
    AssimpLoader();
    ~AssimpLoader();
    
    bool can_load(const std::string& file_path) const;
    void load_model(const std::string& file_path, std::vector<Mesh::Vertex>& vertices, std::vector<Mesh::Indices>& indices);
    std::vector<std::string> get_supported_extensions() const;
    
private:
    void process_node(aiNode* node, const aiScene* scene, std::vector<Mesh::Vertex>& vertices, std::vector<Mesh::Indices>& indices);
    void process_mesh(aiMesh* mesh, const aiScene* scene, std::vector<Mesh::Vertex>& vertices, std::vector<Mesh::Indices>& indices);
    
    std::string get_file_extension(const std::string& file_path) const;
    
    static const std::vector<std::string> supportedExtensions;
};
