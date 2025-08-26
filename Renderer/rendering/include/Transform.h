#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class Transform {
public:
    Transform();

    Transform(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale);

    Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale);

    ~Transform() = default;

    void set_position(const glm::vec3& position);

    void set_position(float x, float y, float z);

    const glm::vec3& get_position() const;


    void translate(const glm::vec3& offset);

    void translate(float x, float y, float z);

    void set_rotation(const glm::vec3& rotation);
    void set_rotation(float x, float y, float z);

    void set_rotation(const glm::quat& rotation);
    glm::vec3 get_rotation_euler() const;

    const glm::quat& get_rotation_quaternion() const;
    void rotate(const glm::vec3& rotation);

    void rotate(float x, float y, float z);


    void rotate(const glm::quat& rotation);

    void rotate_around_axis(float angle, const glm::vec3& axis);

    void set_scale(float scale);


    void set_scale(const glm::vec3& scale);

    void set_scale(float x, float y, float z);

    const glm::vec3& get_scale() const;

    void scale(float scale_factor);

    
    void scale(const glm::vec3& scale_factors);

    glm::mat4 get_model_matrix() const;

    glm::mat4 get_translation_matrix() const;

    glm::mat4 get_rotation_matrix() const;

    glm::mat4 get_scale_matrix() const;

    void reset();

    bool is_identity() const;

    glm::vec3 get_forward() const;

    glm::vec3 get_right() const;

    glm::vec3 get_up() const;

    void look_at(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));
    
    static Transform identity();

    static Transform from_position(const glm::vec3& position);

    static Transform from_rotation(const glm::vec3& rotation);

    static Transform from_scale(const glm::vec3& scale);

private:
    glm::vec3 position_;    // Position in world space
    glm::quat rotation_;    // Rotation as quaternion
    glm::vec3 scale_;       // Scale factors

    // Helper function to normalize quaternion
    void normalize_rotation();
};
