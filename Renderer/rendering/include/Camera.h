#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    enum class Direction {
        kForward,
        kBackward,
        kLeft,
        kRight,
        kUp,
        kDown
    };

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = -90.0f, float pitch = 0.0f);

    glm::mat4 get_view_matrix() const;
    glm::mat4 get_projection_matrix(float aspectRatio) const;
    glm::vec3 get_position() const;
    glm::vec3 get_front() const;

    void process_keyboard(Direction dir, float deltaTime);
    void process_mouse_movement(float xoffset, float yoffset, bool constrainPitch = true);
    void process_mouse_scroll(float yoffset);

private:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float move_speed;
    float mouse_sensitivity;
    float zoom;

    void update_camera_vectors();
}; 