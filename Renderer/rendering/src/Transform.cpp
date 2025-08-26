#include "Transform.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <cmath>

// ===== Constructors =====

Transform::Transform() 
    : position_(0.0f, 0.0f, 0.0f)
    , rotation_(1.0f, 0.0f, 0.0f, 0.0f)  // Identity quaternion (w, x, y, z)
    , scale_(1.0f, 1.0f, 1.0f) {
}

Transform::Transform(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
    : position_(position)
    , rotation_(glm::quat(rotation))  // Convert Euler angles to quaternion
    , scale_(scale) {
    normalize_rotation();
}

Transform::Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
    : position_(position)
    , rotation_(rotation)
    , scale_(scale) {
    normalize_rotation();
}

void Transform::set_position(const glm::vec3& position) {
    position_ = position;
}

void Transform::set_position(float x, float y, float z) {
    position_ = glm::vec3(x, y, z);
}

const glm::vec3& Transform::get_position() const {
    return position_;
}

void Transform::translate(const glm::vec3& offset) {
    position_ += offset;
}

void Transform::translate(float x, float y, float z) {
    position_ += glm::vec3(x, y, z);
}

// Rotation Operations =====

void Transform::set_rotation(const glm::vec3& rotation) {
    rotation_ = glm::quat(rotation);
    normalize_rotation();
}

void Transform::set_rotation(float x, float y, float z) {
    rotation_ = glm::quat(glm::vec3(x, y, z));
    normalize_rotation();
}

void Transform::set_rotation(const glm::quat& rotation) {
    rotation_ = rotation;
    normalize_rotation();
}

glm::vec3 Transform::get_rotation_euler() const {
    return glm::eulerAngles(rotation_);
}

const glm::quat& Transform::get_rotation_quaternion() const {
    return rotation_;
}

void Transform::rotate(const glm::vec3& rotation) {
    glm::quat additional_rotation(rotation);
    rotation_ = rotation_ * additional_rotation;
    normalize_rotation();
}

void Transform::rotate(float x, float y, float z) {
    rotate(glm::vec3(x, y, z));
}

void Transform::rotate(const glm::quat& rotation) {
    rotation_ = rotation_ * rotation;
    normalize_rotation();
}

void Transform::rotate_around_axis(float angle, const glm::vec3& axis) {
    glm::quat axis_rotation = glm::angleAxis(angle, glm::normalize(axis));
    rotation_ = rotation_ * axis_rotation;
    normalize_rotation();
}

// ===== Scale Operations =====

void Transform::set_scale(float scale) {
    scale_ = glm::vec3(scale, scale, scale);
}

void Transform::set_scale(const glm::vec3& scale) {
    scale_ = scale;
}

void Transform::set_scale(float x, float y, float z) {
    scale_ = glm::vec3(x, y, z);
}

const glm::vec3& Transform::get_scale() const {
    return scale_;
}

void Transform::scale(float scale_factor) {
    scale_ *= scale_factor;
}

void Transform::scale(const glm::vec3& scale_factors) {
    scale_ *= scale_factors;
}

// ===== Matrix Operations =====

glm::mat4 Transform::get_model_matrix() const {
    // Create transformation matrix: T * R * S
    glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), position_);
    glm::mat4 rotation_matrix = glm::mat4_cast(rotation_);
    glm::mat4 scale_matrix = glm::scale(glm::mat4(1.0f), scale_);
    
    return translation_matrix * rotation_matrix * scale_matrix;
}

glm::mat4 Transform::get_translation_matrix() const {
    return glm::translate(glm::mat4(1.0f), position_);
}

glm::mat4 Transform::get_rotation_matrix() const {
    return glm::mat4_cast(rotation_);
}

glm::mat4 Transform::get_scale_matrix() const {
    return glm::scale(glm::mat4(1.0f), scale_);
}

// ===== Utility Operations =====

void Transform::reset() {
    position_ = glm::vec3(0.0f, 0.0f, 0.0f);
    rotation_ = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Identity quaternion
    scale_ = glm::vec3(1.0f, 1.0f, 1.0f);
}

bool Transform::is_identity() const {
    const float epsilon = 1e-6f;
    
    // Check position
    if (glm::length(position_) > epsilon) {
        return false;
    }
    
    // Check rotation (should be identity quaternion)
    glm::quat identity_quat(1.0f, 0.0f, 0.0f, 0.0f);
    float dot_product = glm::dot(rotation_, identity_quat);
    if (std::abs(dot_product) < (1.0f - epsilon)) {
        return false;
    }
    
    // Check scale
    glm::vec3 unit_scale(1.0f, 1.0f, 1.0f);
    if (glm::length(scale_ - unit_scale) > epsilon) {
        return false;
    }
    
    return true;
}

glm::vec3 Transform::get_forward() const {
    // Forward is negative Z in OpenGL coordinate system
    return glm::normalize(rotation_ * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Transform::get_right() const {
    // Right is positive X
    return glm::normalize(rotation_ * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Transform::get_up() const {
    // Up is positive Y
    return glm::normalize(rotation_ * glm::vec3(0.0f, 1.0f, 0.0f));
}

void Transform::look_at(const glm::vec3& target, const glm::vec3& up) {
    glm::vec3 forward = glm::normalize(target - position_);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 actual_up = glm::cross(right, forward);
    
    // Create rotation matrix from basis vectors
    glm::mat3 rotation_matrix;
    rotation_matrix[0] = right;
    rotation_matrix[1] = actual_up;
    rotation_matrix[2] = -forward;  // Negative because OpenGL uses right-handed system
    
    // Convert to quaternion
    rotation_ = glm::quat_cast(rotation_matrix);
    normalize_rotation();
}

// ===== Static Utility Functions =====

Transform Transform::identity() {
    return Transform();
}

Transform Transform::from_position(const glm::vec3& position) {
    Transform transform;
    transform.set_position(position);
    return transform;
}

Transform Transform::from_rotation(const glm::vec3& rotation) {
    Transform transform;
    transform.set_rotation(rotation);
    return transform;
}

Transform Transform::from_scale(const glm::vec3& scale) {
    Transform transform;
    transform.set_scale(scale);
    return transform;
}

// ===== Private Helper Functions =====

void Transform::normalize_rotation() {
    rotation_ = glm::normalize(rotation_);
}
