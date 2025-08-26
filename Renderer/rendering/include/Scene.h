#pragma once

#include <vector>
#include <string>
#include <glm/vec3.hpp>

class Scene {
public:
    Scene() : ambient_light_(0.15f, 0.15f, 0.15f) {}
    
    // Model 
    void add_model_reference(const std::string& model_id);
    inline void remove_model_reference(const std::string& model_id);
    inline const std::vector<std::string>& get_model_references() const {return model_references_;}
    void clear_model_references() {model_references_.clear();}
    
    // Light 
    void add_light_reference(const std::string& light_id);
    inline void remove_light_reference(const std::string& light_id);
    inline const std::vector<std::string>& get_light_references() const {return light_references_;}
    void clear_light_references() { light_references_.clear(); }
    
    // Scene state
    inline bool is_empty() const {return model_references_.empty() && light_references_.empty();}
    inline size_t get_model_count() const {return model_references_.size();}
    inline size_t get_light_count() const {return light_references_.size();}
    
    //Ambient light
    inline const glm::vec3& get_ambient_light() const {return ambient_light_;}
    inline void set_ambient_light(const glm::vec3& ambient_light) {ambient_light_ = ambient_light;}
    
private:
    // References to resources 
    std::vector<std::string> model_references_;  
    std::vector<std::string> light_references_;  
    
    // Scene properties
    glm::vec3 ambient_light_;  
};