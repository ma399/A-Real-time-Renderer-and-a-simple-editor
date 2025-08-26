#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

Shader::Shader() : id_(0) {}

Shader::Shader(const std::string& vertex_shader_path, const std::string& fragment_shader_path, const std::string& geometry_shader_path, const std::string& compute_shader_path) 
    : id_(0)
{
    if (vertex_shader_path.empty() && fragment_shader_path.empty() && compute_shader_path.empty()) {
        throw std::invalid_argument("At least vertex, fragment, or compute shader path must be provided");
    }
    
    load_shaders(vertex_shader_path, fragment_shader_path, geometry_shader_path, compute_shader_path);
}
Shader::~Shader() {
    if (id_ != 0) {
        glDeleteProgram(id_);
    }
}

void Shader::load_shaders(const std::string& vshader_path, const std::string& fshader_path, 
                     const std::string& gshader_path, const std::string& cshader_path) 
{
    id_ = glCreateProgram();
    
    GLuint vertex = 0, fragment = 0, geometry = 0, compute = 0;
    
    if (!vshader_path.empty()) {
        vertex = compile_shader(vshader_path, GL_VERTEX_SHADER, "VERTEX");
        glAttachShader(id_, vertex);
    }
    
    if (!fshader_path.empty()) {
        fragment = compile_shader(fshader_path, GL_FRAGMENT_SHADER, "FRAGMENT");
        glAttachShader(id_, fragment);
    }
    
    if (!gshader_path.empty()) {
        geometry = compile_shader(gshader_path, GL_GEOMETRY_SHADER, "GEOMETRY");
        glAttachShader(id_, geometry);
    }
    
    if (!cshader_path.empty()) {
        compute = compile_shader(cshader_path, GL_COMPUTE_SHADER, "COMPUTE");
        glAttachShader(id_, compute);
    }

    glLinkProgram(id_);
    check_compile_errors(id_, "PROGRAM");

    cleanup_shaders(vertex, fragment, geometry, compute, 
                   !vshader_path.empty(), !fshader_path.empty(), 
                   !gshader_path.empty(), !cshader_path.empty());
}
void Shader::use() const {
    glUseProgram(id_);
}

unsigned int Shader::get_id() const {
    return id_;
}

void Shader::set_bool(const std::string& name, bool value) const {
    glUniform1i(glGetUniformLocation(id_, name.c_str()), (int)value);
}

void Shader::set_int(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(id_, name.c_str()), value);
}

void Shader::set_float(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(id_, name.c_str()), value);
}

void Shader::set_vec2(const std::string& name, const glm::vec2& value) const {
    glUniform2fv(glGetUniformLocation(id_, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::set_vec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(glGetUniformLocation(id_, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::set_mat4(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(glGetUniformLocation(id_, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::set_material(const std::string& name, const glm::vec3& ambient, const glm::vec3& diffuse, 
                         const glm::vec3& specular, float shininess, const glm::vec3& emissive) const {
    set_vec3(name + ".ambient", ambient);
    set_vec3(name + ".diffuse", diffuse);
    set_vec3(name + ".specular", specular);
    set_float(name + ".shininess", shininess);
    set_vec3(name + ".emissive", emissive);
}

void Shader::set_light(const std::string& name, int index, int type, const glm::vec3& position, 
                      const glm::vec3& direction, const glm::vec3& color, float intensity, 
                      float range, float cutOff, float outer_cut_off) const {
    std::string lightName = name + "[" + std::to_string(index) + "]";
    set_int(lightName + ".type", type);
    set_vec3(lightName + ".position", position);
    set_vec3(lightName + ".direction", direction);
    set_vec3(lightName + ".color", color);
    set_float(lightName + ".intensity", intensity);
    set_float(lightName + ".range", range);
    set_float(lightName + ".cutOff", cutOff);
    set_float(lightName + ".outerCutOff", outer_cut_off);
}

void Shader::set_phone_point_light(const glm::vec3& pos, const glm::vec3& color) {
    set_vec3("lightPos", pos);
    set_vec3("lightColor", color);
}

std::string Shader::read_file(const std::string& filePath) {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    
    try {
        file.open(filePath);
        std::stringstream stream;
        stream << file.rdbuf();
        file.close();
        return stream.str();
    } catch (std::ifstream::failure& e) {
        throw std::runtime_error("Failed to read shader file: " + filePath);
    }
}

void Shader::check_compile_errors(unsigned int shader, const std::string& type) {
    int success;
    char infoLog[1024];
    
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            throw std::runtime_error("Shader compilation error (" + type + "): " + infoLog);
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            throw std::runtime_error("Program linking error (" + type + "): " + infoLog);
        }
    }
}

GLuint Shader::compile_shader(const std::string& shaderPath, GLenum shaderType, const std::string& typeName) {
    std::string shaderCode = read_file(shaderPath);
    const char* code = shaderCode.c_str();
    
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);
    check_compile_errors(shader, typeName);
    
    return shader;
}

void Shader::cleanup_shaders(GLuint vertex, GLuint fragment, GLuint geometry, GLuint compute,
                           bool hasVertex, bool hasFragment, bool hasGeometry, bool hasCompute) {
    if (hasVertex) {
        glDeleteShader(vertex);
    }
    if (hasFragment) {
        glDeleteShader(fragment);
    }
    if (hasGeometry) {
        glDeleteShader(geometry);
    }
    if (hasCompute) {
        glDeleteShader(compute);
    }
} 