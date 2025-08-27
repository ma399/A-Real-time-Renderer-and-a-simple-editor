#include "ShadowMap.h"
#include "Shader.h"
#include <Logger.h>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

ShadowMap::ShadowMap() 
    : framebuffer_(0), depth_texture_(0), shadow_width_(0), shadow_height_(0), initialized_(false)
{
}

ShadowMap::~ShadowMap() {
    cleanup();
}

bool ShadowMap::initialize(int width, int height) {
    if (initialized_) {
        std::cout << "ShadowMap already initialized, cleaning up first..." << std::endl;
        cleanup();
    }
    
    shadow_width_ = width;
    shadow_height_ = height;
    
    LOG_INFO("ShadowMap::initialize({},{}))", width, height);

    glGenTextures(1, &depth_texture_);
    glBindTexture(GL_TEXTURE_2D, depth_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    
    glGenFramebuffers(1, &framebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR("ERROR: Framebuffer not complete!");
        cleanup();
        return false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    try {
        shadow_shader_ = std::make_unique<Shader>("../assets/shaders/shadow_map_vertex.glsl", 
                                               "../assets/shaders/shadow_map_fragment.glsl");
        LOG_INFO("Shadow shader created successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create shadow shader: {}");
        cleanup();
        return false;
    }
    
    LOG_INFO("ShadowMap initialized successfully");
    initialized_ = true;
    return true;
}

void ShadowMap::cleanup() {
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("ShadowMap::cleanup()");
    
    if (depth_texture_ != 0) {
        glDeleteTextures(1, &depth_texture_);
        depth_texture_ = 0;
    }
    
    if (framebuffer_ != 0) {
        glDeleteFramebuffers(1, &framebuffer_);
        framebuffer_ = 0;
    }
    
    initialized_ = false;
}

void ShadowMap::begin_shadow_pass() {
    if (!initialized_) {
        std::cout << "Warning: ShadowMap not initialized" << std::endl;
        return;
    }
    
    // save viewport and framebuffer
    glGetIntegerv(GL_VIEWPORT, saved_viewport_);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved_framebuffer_);
    
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(0, 0, shadow_width_, shadow_height_);
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);
    

}

void ShadowMap::end_shadow_pass() {
    if (!initialized_) {
        return;
    }
    
    // restore viewport and framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, saved_framebuffer_);
    glViewport(saved_viewport_[0], saved_viewport_[1], saved_viewport_[2], saved_viewport_[3]);
}

glm::mat4 ShadowMap::get_light_space_matrix(const glm::vec3& lightDirection, const glm::vec3& shadowCenter) const {
    // For directional light shadow mapping
    float near_plane = 1.0f, far_plane = 50.0f;  // Increased far plane
    float orthoSize = 20.0f;  // Increased coverage area
    
    glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, near_plane, far_plane);
    
    // For directional light, position the light camera far away in the opposite direction
    glm::vec3 normalizedLightDir = glm::normalize(lightDirection);
    glm::vec3 lightPosition = shadowCenter - normalizedLightDir * 15.0f; // Position light 15 units away from center
    
    // Look from light position towards the shadow center
    glm::mat4 lightView = glm::lookAt(lightPosition, shadowCenter, glm::vec3(0.0f, 1.0f, 0.0f));
    
    return lightProjection * lightView;
}
