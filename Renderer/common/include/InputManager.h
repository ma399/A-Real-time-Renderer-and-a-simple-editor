#pragma once

#include <functional>
#include <memory>
#include <string>
#include <GLFW/glfw3.h>

// Forward declarations
class Camera;
class GUI;
class DragHandler;
class Scene;
class CoroutineResourceManager;
class Transform;

// Input callback types
using KeyboardCallback = std::function<void(int key, float deltaTime)>;
using MouseMoveCallback = std::function<void(float xOffset, float yOffset)>;
using MouseButtonCallback = std::function<void(int button, int action)>;
using WindowCloseCallback = std::function<void()>;

// Drag operation callback types
using DragStartCallback = std::function<void(const std::string& model_id, float screen_x, float screen_y)>;
using DragUpdateCallback = std::function<void(const std::string& model_id, float screen_x, float screen_y)>;
using DragEndCallback = std::function<void(const std::string& model_id)>;

// Application callback types for setup
using WindowCloseHandler = std::function<void()>;
using DragStartHandler = std::function<void(const std::string&, float, float)>;
using DragUpdateHandler = std::function<void(const std::string&, float, float)>;
using DragEndHandler = std::function<void(const std::string&)>;
using ViewportCheckHandler = std::function<bool(double, double)>;

// Keyboard input types
enum class KeyboardInput {
    kMoveForward,
    kMoveBackward,
    kMoveLeft,
    kMoveRight,
    kMoveUp,
    kMoveDown,
    kEscape,
    kUnknown
};

// Mouse input types
enum class MouseInput {
    kRightButton,
    kLeftButton,
    kMiddleButton,
    kUnknown
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    // Initialization and cleanup
    bool initialize(GLFWwindow* window, std::unique_ptr<GUI>& gui);
    void cleanup();
    
    // Drag system initialization
    bool initialize_drag_system(std::shared_ptr<Camera> camera,
                               Scene* scene,
                               CoroutineResourceManager* resource_manager);

    // Input callbacks setup
    void setup_input_callbacks(std::shared_ptr<Camera> camera, 
                              GLFWwindow* window,
                              int& gbuffer_debug_mode,
                              WindowCloseHandler window_close_handler,
                              /*DragStartHandler drag_start_handler,
                              DragUpdateHandler drag_update_handler,
                              DragEndHandler drag_end_handler,*/
                              ViewportCheckHandler viewport_check_handler);

    // Main input processing
    void process_input(float deltaTime);

    // Callback registration
    void set_keyboard_callback(KeyboardCallback callback);
    void set_mouse_move_callback(MouseMoveCallback callback);
    void set_mouse_button_callback(MouseButtonCallback callback);
    void set_window_close_callback(WindowCloseCallback callback);
    
    // Drag callback registration
    void set_drag_start_callback(DragStartCallback callback);
    void set_drag_update_callback(DragUpdateCallback callback);
    void set_drag_end_callback(DragEndCallback callback);

    // Input state queries
    bool is_key_pressed(int key) const;
    bool is_mouse_button_pressed(int button) const;
    void get_cursor_position(double& x, double& y) const;

    // Viewport management
    bool is_cursor_in_viewport() const;
    
    // Drag system control
    bool is_dragging() const;
    void set_drag_enabled(bool enabled) { drag_enabled_ = enabled; }
    bool is_drag_enabled() const { return drag_enabled_; }
    
    // Transform access
    Transform get_model_transform(const std::string& model_id) const;
    

    class ObjectTransformSystem* get_transform_system() const;
    
    // Public method for GLFW callback
    void handle_mouse_movement_callback(float xPos, float yPos);

    // Input mapping
    static KeyboardInput map_glfw_key_to_input(int glfwKey);
    static MouseInput map_glfw_button_to_input(int glfwButton);

private:
    GLFWwindow* window_;
    GUI* gui_;

    // Mouse state tracking
    bool first_mouse_;
    float last_mouse_x_;
    float last_mouse_y_;
    bool right_mouse_pressed_;
    bool left_mouse_pressed_;
    
    // Drag mouse tracking
    float last_drag_x_;
    float last_drag_y_;
    bool drag_mouse_moved_;
    
    // Drag system
    std::unique_ptr<DragHandler> drag_handler_;
    
    // Drag state
    bool drag_enabled_;
    bool is_dragging_;

    // Callbacks
    KeyboardCallback keyboard_callback_;
    MouseMoveCallback mouse_move_callback_;
    MouseButtonCallback mouse_button_callback_;
    WindowCloseCallback window_close_callback_;
    
    // Drag callbacks
    DragStartCallback drag_start_callback_;
    DragUpdateCallback drag_update_callback_;
    DragEndCallback drag_end_callback_;
    
    // Viewport check callback
    ViewportCheckHandler viewport_check_handler_;

    // Internal processing methods
    void process_keyboard_input(float deltaTime);
    void process_mouse_input();
    void handle_key_input(KeyboardInput input, float deltaTime);
    void handle_mouse_movement(float xPos, float yPos);
    void handle_mouse_button(MouseInput button, int action);
    
    // Drag processing methods
    void process_left_mouse_button();
    void handle_drag_start(float screen_x, float screen_y);
    void handle_drag_update(float screen_x, float screen_y);
    void handle_drag_end();

    // Utility methods
};
