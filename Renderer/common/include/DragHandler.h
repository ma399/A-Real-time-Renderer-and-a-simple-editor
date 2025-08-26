#pragma once

#include <memory>
#include <string>
#include <functional>

// Forward declarations
class Transform;

// Forward declarations
class Camera;
class Scene;
class CoroutineResourceManager;
class RaycastSystem;
class ObjectTransformSystem;

// Drag operation callback types
using DragStartCallback = std::function<void(const std::string& model_id, float screen_x, float screen_y)>;
using DragUpdateCallback = std::function<void(const std::string& model_id, float screen_x, float screen_y)>;
using DragEndCallback = std::function<void(const std::string& model_id)>;

/**
 * @brief Handles drag operations for 3D objects
 * 
 * This class encapsulates the drag logic to avoid header conflicts
 * in InputManager. It manages raycast detection and object transformation.
 */
class DragHandler {
public:
    DragHandler();
    ~DragHandler();

    /**
     * @brief Initialize the drag handler with required systems
     * @param camera Shared pointer to camera for coordinate conversion
     * @param scene Pointer to scene containing models
     * @param resource_manager Pointer to resource manager
     * @return true if initialization successful
     */
    bool initialize(std::shared_ptr<Camera> camera,
                   Scene* scene,
                   CoroutineResourceManager* resource_manager);

    /**
     * @brief Clean up resources
     */
    void cleanup();

    /**
     * @brief Set drag callbacks
     */
    void set_drag_start_callback(DragStartCallback callback);
    void set_drag_update_callback(DragUpdateCallback callback);
    void set_drag_end_callback(DragEndCallback callback);

    /**
     * @brief Start drag operation at screen coordinates
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @param viewport_width Viewport width
     * @param viewport_height Viewport height
     * @return true if drag started successfully
     */
    bool start_drag(float screen_x, float screen_y, 
                   float viewport_width, float viewport_height);

    /**
     * @brief Update drag operation
     * @param screen_x Screen X coordinate
     * @param screen_y Screen Y coordinate
     * @param viewport_width Viewport width
     * @param viewport_height Viewport height
     * @return true if drag updated successfully
     */
    bool update_drag(float screen_x, float screen_y,
                    float viewport_width, float viewport_height);

    /**
     * @brief End drag operation
     */
    void end_drag();

    /**
     * @brief Check if currently dragging
     * @return true if dragging
     */
    bool is_dragging() const;

    /**
     * @brief Enable/disable drag functionality
     */
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

    /**
     * @brief Get current transform for a model
     * @param model_id Model ID
     * @return Transform object, or identity if not found
     */
    Transform get_model_transform(const std::string& model_id) const;

    /**
     * @brief Get access to the ObjectTransformSystem
     * @return Pointer to ObjectTransformSystem, nullptr if not available
     */
    ObjectTransformSystem* get_transform_system() const;

private:
    // System components
    std::shared_ptr<Camera> camera_;
    Scene* scene_;
    CoroutineResourceManager* resource_manager_;
    
    // Drag systems
    std::shared_ptr<RaycastSystem> raycast_system_;
    std::shared_ptr<ObjectTransformSystem> transform_system_;
    
    // State
    bool enabled_;
    bool is_dragging_;
    
    // Callbacks
    DragStartCallback drag_start_callback_;
    DragUpdateCallback drag_update_callback_;
    DragEndCallback drag_end_callback_;
};
