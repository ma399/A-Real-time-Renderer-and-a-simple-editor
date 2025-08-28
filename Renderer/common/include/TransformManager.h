#pragma once

#include "Transform.h"
#include "RaycastUtils.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include <string>

// Forward declarations
class Camera;
class Scene;
class CoroutineResourceManager;

// Transform modes
enum class TransformMode {
    kTranslate,
    kRotate,
    kScale
};

// Drag state
enum class DragState {
    kNone,
    kDragging
};

struct DragInfo {
    std::string model_id;
    glm::vec3 initial_hit_point;
    glm::vec3 drag_offset;
    DragState state = DragState::kNone;
    TransformMode mode = TransformMode::kTranslate;
    
    void reset() {
        model_id.clear();
        state = DragState::kNone;
        initial_hit_point = glm::vec3(0.0f);
        drag_offset = glm::vec3(0.0f);
    }
};

class TransformManager {
public:
    TransformManager();
    ~TransformManager() = default;

    // Transform management
    Transform& get_transform(const std::string& model_id);
    const Transform& get_transform(const std::string& model_id) const;
    void set_transform(const std::string& model_id, const Transform& transform);
    glm::mat4 get_model_matrix(const std::string& model_id) const;

    // Drag operations
    bool start_drag(float screen_x, float screen_y,
                   float screen_width, float screen_height,
                   const Camera& camera,
                   const Scene& scene,
                   CoroutineResourceManager& resource_manager);
    
    bool update_drag(float screen_x, float screen_y,
                    float screen_width, float screen_height,
                    const Camera& camera);
    
    void end_drag();
    
    bool is_dragging() const { return drag_info_.state == DragState::kDragging; }
    const DragInfo& get_drag_info() const { return drag_info_; }

    // Settings
    void set_transform_mode(TransformMode mode) { current_mode_ = mode; }
    TransformMode get_transform_mode() const { return current_mode_; }

private:
    std::unordered_map<std::string, Transform> transforms_;
    DragInfo drag_info_;
    TransformMode current_mode_ = TransformMode::kTranslate;
    
    static const Transform identity_transform_;
    
    glm::vec3 calculate_drag_world_position(float screen_x, float screen_y,
                                           float screen_width, float screen_height,
                                           const Camera& camera) const;
};
