#pragma once

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>

class Mesh {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
        glm::vec3 tangent;
    };

    using Indices = unsigned int;

    Mesh() = default;
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    void draw() const;
    void setup_mesh();
    void ensure_setup() const; // Ensure OpenGL buffers are initialized
    inline bool empty() const { return vertices.empty(); };
    inline bool is_setup() const { return gl_initialized_; };
    
    // Accessors 
    const std::vector<Vertex>& get_vertices() const { return vertices; }
    const std::vector<unsigned int>& get_indices() const { return indices; }
    size_t get_vertex_count() const { return vertices.size(); }
    size_t get_triangle_count() const { return indices.size() / 3; }

private:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    
    mutable unsigned int vao_ = 0, vbo_ = 0, ebo_ = 0;
    mutable bool gl_initialized_ = false;
}; 