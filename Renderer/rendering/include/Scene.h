#pragma once

#include <vector>
#include <string>
#include <glm/vec3.hpp>

class Scene {
public:
    Scene() : ambient_light_(0.15f, 0.15f, 0.15f) {}
    
    // Renderable management
    void add_renderable_reference(const std::string& renderable_id);
    void remove_renderable_reference(const std::string& renderable_id);
    const std::vector<std::string>& get_renderable_references() const { return renderable_references_; }
    void clear_renderable_references() { renderable_references_.clear(); }
    
    // Light 
    void add_light_reference(const std::string& light_id);
    void remove_light_reference(const std::string& light_id);
    const std::vector<std::string>& get_light_references() const { return light_references_; }
    void clear_light_references() { light_references_.clear(); }
    
    // Scene state
    bool is_empty() const { return renderable_references_.empty() && light_references_.empty(); }
    size_t get_renderable_count() const { return renderable_references_.size(); }
    size_t get_light_count() const { return light_references_.size(); }
    
    // Ambient light
    const glm::vec3& get_ambient_light() const { return ambient_light_; }
    void set_ambient_light(const glm::vec3& ambient_light) { ambient_light_ = ambient_light; }
    
private:
    // References to resources 
    std::vector<std::string> renderable_references_;
    std::vector<std::string> light_references_;  
    
    // Scene properties
    glm::vec3 ambient_light_;  
};