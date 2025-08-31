#pragma once

#include <glad/glad.h>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>


class Shader {
public:
    Shader();
    ~Shader();

    // Disable copy constructor and assignment operator
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    
    // Enable move constructor and assignment operator
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    
    // Shader compilation and linking methods
    Shader& attach_shader(const std::string& shader_source, GLenum shader_type);
    void link_program();
    
    void use() const;
    unsigned int get_id() const;
    bool is_valid() const;

    // Uniform setters
    void set_bool(const std::string& name, bool value) const;
    void set_int(const std::string& name, int value) const;
    void set_float(const std::string& name, float value) const;
    void set_vec2(const std::string& name, const glm::vec2& value) const;
    void set_vec3(const std::string& name, const glm::vec3& value) const;
    void set_mat4(const std::string& name, const glm::mat4& value) const;
    
private:
    GLuint program_id_;
    std::unordered_map<GLenum, GLuint> attached_shaders_;
    
    // Helper methods
    void check_compile_errors(GLuint shader, const std::string& type);
    GLuint compile_shader(const std::string& shader_source, GLenum shader_type, const std::string& type_name);
}; 