#include "Camera.h"
#include <Logger.h>
#include <algorithm>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : position(position), worldUp(up), yaw(yaw), pitch(pitch),
      move_speed(2.5f), mouse_sensitivity(0.1f), zoom(45.0f) {
    update_camera_vectors();
}

glm::mat4 Camera::get_view_matrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::get_projection_matrix(float aspectRatio) const {
    return glm::perspective(glm::radians(zoom), aspectRatio, 0.1f, 100.0f);
}

glm::vec3 Camera::get_position() const {
    return position;
}

glm::vec3 Camera::get_front() const {
    return front;
}

void Camera::process_keyboard(Direction dir, float deltaTime) {
    float velocity = move_speed * deltaTime;
    //LOG_INFO("velocity{}", velocity);
    switch (dir) {
        case Direction::kForward:
            position += front * velocity;
            break;
        case Direction::kBackward:
            position -= front * velocity;
            break;
        case Direction::kLeft:
            position -= right * velocity;
            break;
        case Direction::kRight:
            position += right * velocity;
            break;
        case Direction::kUp:
            position += worldUp * velocity;
            break;
        case Direction::kDown:
            position -= worldUp * velocity;
            break;
    }
}

void Camera::process_mouse_movement(float xoffset, float yoffset, bool constrainPitch) {
    LOG_DEBUG("Camera: process_mouse_movement called with offset ({}, {})", xoffset, yoffset);
    xoffset *= mouse_sensitivity;
    yoffset *= mouse_sensitivity;

    yaw += xoffset;
    pitch += yoffset;
    LOG_DEBUG("Camera: new yaw={}, pitch={}", yaw, pitch);

    if (constrainPitch) {
        pitch = std::clamp(pitch, -89.0f, 89.0f);
    }

    update_camera_vectors();
}

void Camera::process_mouse_scroll(float yoffset) {
    zoom -= yoffset;
    zoom = std::clamp(zoom, 1.0f, 45.0f);
}

void Camera::update_camera_vectors() {
    glm::vec3 f;
    f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    f.y = sin(glm::radians(pitch));
    f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(f);
    
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}