#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>

class Shader;

class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap();

    bool initialize(int width, int height);
    
    void cleanup();    
    
    GLuint get_depth_texture() const { return depth_texture_; }
    
    GLuint get_framebuffer() const { return framebuffer_; }
    
    
    int get_width() const { return shadow_width_; }
    int get_height() const { return shadow_height_; }
    
    void begin_shadow_pass();
    void end_shadow_pass();
     
    Shader* get_shadow_shader() const { return shadow_shader_.get(); }
    
    glm::mat4 get_light_space_matrix(const glm::vec3& light_direction, const glm::vec3& shadow_center) const;
    

private:
    GLuint framebuffer_;
    GLuint depth_texture_;
    int shadow_width_;
    int shadow_height_;
    bool initialized_;
    
    int saved_viewport_[4];
    GLint saved_framebuffer_;

    std::unique_ptr<Shader> shadow_shader_;
};