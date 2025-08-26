#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <vector>
#include <glad/glad.h>

class Shader;

class Light {
public:
    enum class Type {
        kDirectional,
        kPoint,
        kSpot
    };

    Light(Type type, const glm::vec3& position, const glm::vec3& color);
    virtual ~Light();

    Type get_type() const { return type_; }
    glm::vec3 get_position() const { return position_; }
    glm::vec3 get_color() const { return color_; }
    float get_intensity() const { return intensity_; }

    void set_position(const glm::vec3& pos) { position_ = pos; }
    void set_color(const glm::vec3& col) { color_ = col; }
    void set_intensity(float inten) { intensity_ = inten; }

    virtual glm::vec3 get_direction() const = 0;
    virtual float get_attenuation(float distance) const = 0;
    void set_shader(const Shader& shader) const;
    void set_shader_array(const Shader& shader, int index) const;
    // Rendering functions for light visualization
    void render() const;
    void setup_light_mesh();

protected:
    Type type_;
    glm::vec3 position_;
    glm::vec3 color_;
    float intensity_;
    
    virtual void set_unique_shader(const Shader& shader) const = 0;
    virtual void set_unique_shader_array(const Shader& shader, int index) const = 0;
    
    static unsigned int light_vao, light_vbo;
    static bool mesh_initialized;

private:
    void set_common_shader(const Shader& shader) const;
    void set_common_shader_array(const Shader& shader, int index) const;
};


class DirectionalLight : public Light {
public:
    DirectionalLight(const glm::vec3& direction, const glm::vec3& color);
    
    glm::vec3 get_direction() const override { return direction_; }
    float get_attenuation([[maybe_unused]]float distance) const override {return 1.0f;}
    void set_unique_shader(const Shader& shader) const override;
    void set_unique_shader_array(const Shader& shader, int index) const override;

    void set_direction(const glm::vec3& dir) { direction_ = dir; }

private:
    glm::vec3 direction_;
};


class PointLight : public Light {
public:
    PointLight(const glm::vec3& position, const glm::vec3& color, float range = 10.0f);
    
    glm::vec3 get_direction() const override { return glm::vec3(0.0f); }
    float get_attenuation(float distance) const override;
    void set_unique_shader(const Shader& shader) const override;
    void set_unique_shader_array(const Shader& shader, int index) const override;
    
    void set_range(float r) { range_ = r; }
    float get_range() const { return range_; }

private:
    float range_;
    float constant_;
    float linear_;
    float quadratic_;
};

class SpotLight : public Light {
public:
    SpotLight(const glm::vec3& position, const glm::vec3& direction, 
              const glm::vec3& color, float cutOff = 12.5f, float outerCutOff = 17.5f);
    
    glm::vec3 get_direction() const override { return direction_; }
    float get_attenuation(float distance) const override;
    void set_unique_shader(const Shader& shader) const override;
    void set_unique_shader_array(const Shader& shader, int index) const override;

    float get_spot_attenuation(const glm::vec3& lightDir) const;
    
    void set_direction(const glm::vec3& dir) { direction_ = dir; }
    void set_cut_off(float cut) { cut_off_ = cut; }
    void set_outer_cut_off(float outer) { outer_cut_off_ = outer; }

private:
    glm::vec3 direction_;
    float cut_off_;
    float outer_cut_off_;
    float constant_;
    float linear_;
    float quadratic_;
};


class LightManager {
public:
    LightManager();
    ~LightManager();

    
    void add_light(std::unique_ptr<Light> light);
    void remove_light(size_t index);
    
   
    const std::vector<std::unique_ptr<Light>>& get_lights() const { return lights_; }
    size_t get_light_count() const { return lights_.size(); }

    
    void set_ambient_light(const glm::vec3& ambient) { ambient_light_ = ambient; }
    glm::vec3 get_ambient_light() const { return ambient_light_; }

private:
    std::vector<std::unique_ptr<Light>> lights_;
    glm::vec3 ambient_light_ = glm::vec3(0.1f, 0.1f, 0.1f);
}; 