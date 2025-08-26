#pragma once

#include "Transform.h"
#include "RaycastSystem.h"
#include <unordered_map>
#include <string>
#include <memory>

// Forward declarations
class Camera;
class Scene;
class CoroutineResourceManager;


enum class TransformMode {
    kTranslate,  // Move objects
    kRotate,     // Rotate objects
    kScale       // Scale objects
};


enum class DragState {
    kNone,       // No drag operation
    kStarting,   // Drag just started
    kDragging,   // Currently dragging
    kEnding      // Drag ending
};


struct DragInfo {
    std::string model_id;                    // ID of the model being dragged
    glm::vec3 initial_hit_point;            // Initial world hit point
    glm::vec3 initial_model_position;       // Initial model position
    glm::vec3 drag_offset;                  // Offset from model center to hit point
    glm::vec3 current_world_position;       // Current drag position in world space
    DragState state = DragState::kNone;     // Current drag state
    TransformMode mode = TransformMode::kTranslate;  // Current transform mode
    
    // Screen space information
    float initial_screen_x = 0.0f;
    float initial_screen_y = 0.0f;
    
    void reset() {
        model_id.clear();
        state = DragState::kNone;
        initial_hit_point = glm::vec3(0.0f);
        initial_model_position = glm::vec3(0.0f);
        drag_offset = glm::vec3(0.0f);
        current_world_position = glm::vec3(0.0f);
    }
};


class ObjectTransformSystem {
public:
    ObjectTransformSystem();
    ~ObjectTransformSystem();

 
    bool initialize(std::shared_ptr<RaycastSystem> raycast_system);

    void set_transform_mode(TransformMode mode) { current_mode_ = mode; }
  
    TransformMode get_transform_mode() const { return current_mode_; }

    
    bool start_drag(float screen_x, float screen_y,
                   float screen_width, float screen_height,
                   const Camera& camera,
                   const Scene& scene,
                   CoroutineResourceManager& resource_manager);

   
    bool update_drag(float screen_x, float screen_y,
                    float screen_width, float screen_height,
                    const Camera& camera);

    bool end_drag();

    bool is_dragging() const { return drag_info_.state == DragState::kDragging; }


    const DragInfo& get_drag_info() const { return drag_info_; }

    Transform& get_transform(const std::string& model_id);

    const Transform& get_transform(const std::string& model_id) const;

    void set_transform(const std::string& model_id, const Transform& transform);

    void remove_transform(const std::string& model_id);

    void clear_transforms();

    glm::mat4 get_model_matrix(const std::string& model_id) const;

    void set_drag_sensitivity(float sensitivity) { drag_sensitivity_ = sensitivity; }

    float get_drag_sensitivity() const { return drag_sensitivity_; }

    void set_rotation_animation(const std::string& model_id, bool enable, const glm::vec3& rotation_speed = glm::vec3(0.5f, 0.3f, 0.0f));

    void update_animations(float current_time);

    bool has_animation(const std::string& model_id) const;

private:
    // Core systems
    std::shared_ptr<RaycastSystem> raycast_system_;
    
    // Transform storage
    std::unordered_map<std::string, Transform> transforms_;
    
    // Drag state
    DragInfo drag_info_;
    TransformMode current_mode_;
    
    // Configuration
    float drag_sensitivity_;
    bool snap_to_grid_;
    float grid_size_;
    
    // Animation data
    struct AnimationData {
        bool enabled = false;
        glm::vec3 rotation_speed = glm::vec3(0.5f, 0.3f, 0.0f);
        glm::vec3 base_position = glm::vec3(0.0f);
        glm::vec3 base_scale = glm::vec3(1.0f);
    };
    std::unordered_map<std::string, AnimationData> animations_;
    
    // Static identity transform for const access
    static const Transform identity_transform_;

    glm::vec3 calculate_drag_world_position(float screen_x, float screen_y,
                                          float screen_width, float screen_height,
                                          const Camera& camera) const;

    glm::vec3 apply_constraints(const glm::vec3& position) const;
};
