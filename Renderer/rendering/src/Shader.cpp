#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

Shader::Shader() : program_id_(0) {}

Shader::~Shader() {
    if (program_id_ != 0) {
        // Clean up attached shaders
        for (auto& pair : attached_shaders_) {
            glDeleteShader(pair.second);
        }
        glDeleteProgram(program_id_);
    }
}

Shader::Shader(Shader&& other) noexcept 
    : program_id_(other.program_id_), attached_shaders_(std::move(other.attached_shaders_)) {
    other.program_id_ = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (program_id_ != 0) {
            for (auto& pair : attached_shaders_) {
                glDeleteShader(pair.second);
            }
            glDeleteProgram(program_id_);
        }
        program_id_ = other.program_id_;
        attached_shaders_ = std::move(other.attached_shaders_);
        other.program_id_ = 0;
    }
    return *this;
}

Shader& Shader::attach_shader(const std::string& shader_source, GLenum shader_type) {
    if (program_id_ == 0) {
        program_id_ = glCreateProgram();
    }
    
    // Get the type name for error reporting
    std::string type_name;
    switch (shader_type) {
        case GL_VERTEX_SHADER: type_name = "VERTEX"; break;
        case GL_FRAGMENT_SHADER: type_name = "FRAGMENT"; break;
        case GL_GEOMETRY_SHADER: type_name = "GEOMETRY"; break;
        case GL_COMPUTE_SHADER: type_name = "COMPUTE"; break;
        default: type_name = "OTHERS"; break; // For tessellation or ray tracing shader in the future
    }
    
    // Compile and attach the new shader from source
    GLuint shader = compile_shader(shader_source, shader_type, type_name);
    glAttachShader(program_id_, shader);
    attached_shaders_[shader_type] = shader;
    
    return *this;
}



void Shader::link_program() {
    if (program_id_ == 0) {
        throw std::runtime_error("Cannot link program: no shader program created");
    }
    
    glLinkProgram(program_id_);
    check_compile_errors(program_id_, "PROGRAM");
}

void Shader::use() const {
    if (program_id_ != 0) {
        glUseProgram(program_id_);
    }
}

unsigned int Shader::get_id() const {
    return program_id_;
}

bool Shader::is_valid() const {
    return program_id_ != 0;
}

void Shader::set_bool(const std::string& name, bool value) const {
    glUniform1i(glGetUniformLocation(program_id_, name.c_str()), (int)value);
}

void Shader::set_int(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(program_id_, name.c_str()), value);
}

void Shader::set_float(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(program_id_, name.c_str()), value);
}

void Shader::set_vec2(const std::string& name, const glm::vec2& value) const {
    glUniform2fv(glGetUniformLocation(program_id_, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::set_vec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(glGetUniformLocation(program_id_, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::set_mat4(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(glGetUniformLocation(program_id_, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}



void Shader::check_compile_errors(unsigned int shader, const std::string& type) {
    int success;
    char infoLog[1024];
    
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            throw std::runtime_error("Shader compilation error (" + type + "): " + std::string(infoLog));
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, nullptr, infoLog);
            throw std::runtime_error("Program linking error: " + std::string(infoLog));
        }
    }
}



GLuint Shader::compile_shader(const std::string& shaderSource, GLenum shaderType, const std::string& typeName) {
    const char* code = shaderSource.c_str();
    
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &code, nullptr);
    glCompileShader(shader);
    check_compile_errors(shader, typeName);
    
    return shader;
}