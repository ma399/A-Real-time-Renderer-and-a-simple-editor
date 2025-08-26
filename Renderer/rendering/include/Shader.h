#pragma once

#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>


class Shader {
public:
    Shader();
    Shader(const std::string& vertex_shader_path, const std::string& fragment_shader_path,
        const std::string& geometry_shader_path = "", const std::string& compute_shader_path = "");
    ~Shader();

    void load_shaders(const std::string& vertex_path, const std::string& fragment_path, 
                     const std::string& geometry_path = "", const std::string& compute_path = "");
    
    void use() const;
    unsigned int get_id() const;

    // Uniform setters
    void set_bool(const std::string& name, bool value) const;
    void set_int(const std::string& name, int value) const;
    void set_float(const std::string& name, float value) const;
    void set_vec2(const std::string& name, const glm::vec2& value) const;
    void set_vec3(const std::string& name, const glm::vec3& value) const;
    void set_mat4(const std::string& name, const glm::mat4& value) const;
    
    void set_material(const std::string& name, const glm::vec3& ambient, const glm::vec3& diffuse, 
                     const glm::vec3& specular, float shininess, const glm::vec3& emissive) const;
    void set_light(const std::string& name, int index, int type, const glm::vec3& position, 
                  const glm::vec3& direction, const glm::vec3& color, float intensity, 
                  float range, float cut_off, float outer_cut_off) const;
    void set_phone_point_light(const glm::vec3& direction, const glm::vec3& color);
private:
    unsigned int id_;
    std::string read_file(const std::string& file_path);
    void check_compile_errors(unsigned int shader, const std::string& type);
    
    GLuint compile_shader(const std::string& shader_path, GLenum shader_type, const std::string& type_name);
    void cleanup_shaders(GLuint vertex, GLuint fragment, GLuint geometry, GLuint compute,
                       bool has_vertex, bool has_fragment, bool has_geometry, bool has_compute);
}; 