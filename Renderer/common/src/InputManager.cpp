#include "InputManager.h"
#include "TransformManager.h"
#include "Transform.h"
#include "Camera.h"
#include "Scene.h"
#include "CoroutineResourceManager.h"
#include <Logger.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

// Forward declaration to avoid header conflicts
class Application;

InputManager::InputManager()
    : window_(nullptr),
      gui_(nullptr),
      first_mouse_(true),
      last_mouse_x_(400.0f),
      last_mouse_y_(300.0f),
      right_mouse_pressed_(false),
      left_mouse_pressed_(false),
      last_drag_x_(0.0f),
      last_drag_y_(0.0f),
      drag_mouse_moved_(false),
      scene_(nullptr),
      resource_manager_(nullptr),
      drag_enabled_(true),
      is_dragging_(false) {
}

InputManager::~InputManager() {
    cleanup();
}

bool InputManager::initialize(GLFWwindow* window, std::unique_ptr<GUI>& gui) {
    if (!window) {
        LOG_ERROR("InputManager: Invalid window pointer");
        return false;
    }

    window_ = window;
    gui_ = gui.get();

    // Note: Mouse callback will be set by Application to avoid header conflicts

    LOG_INFO("InputManager: Initialized successfully");
    return true;
}

bool InputManager::initialize_transform_system(std::shared_ptr<Camera> camera,
                                              Scene* scene,
                                              CoroutineResourceManager* resource_manager) {
    if (!camera || !scene || !resource_manager) {
        LOG_ERROR("InputManager: Invalid parameters for transform system initialization");
        return false;
    }

    // Store references for drag operations
    camera_ = camera;
    scene_ = scene;
    resource_manager_ = resource_manager;

    // Create transform manager
    transform_manager_ = std::make_unique<TransformManager>();
    
    LOG_INFO("InputManager: Transform system initialized successfully");
    return true;
}

void InputManager::setup_input_callbacks(std::shared_ptr<Camera> camera, 
                                        GLFWwindow* window,
                                        int& gbuffer_debug_mode,
                                        WindowCloseHandler window_close_handler,
                                        /*DragStartHandler drag_start_handler,
                                        DragUpdateHandler drag_update_handler,
                                        DragEndHandler drag_end_handler,*/
                                        ViewportCheckHandler viewport_check_handler) {
    if (!camera || !window) {
        LOG_ERROR("InputManager: Invalid parameters for callback setup");
        return;
    }

    // Set up keyboard callback for camera movement
    set_keyboard_callback([camera, window, &gbuffer_debug_mode, window_close_handler](int key, float deltaTime) {
        if (!camera) return;
        
        LOG_DEBUG("InputManager: Key {} pressed", key);
        const float moveSpeed = 5.0f;
        switch (key) {
            case GLFW_KEY_W:
                camera->process_keyboard(Camera::Direction::kForward, deltaTime * moveSpeed);
                break;
            case GLFW_KEY_S:
                camera->process_keyboard(Camera::Direction::kBackward, deltaTime * moveSpeed);
                break;
            case GLFW_KEY_A:
                camera->process_keyboard(Camera::Direction::kLeft, deltaTime * moveSpeed);
                break;
            case GLFW_KEY_D:
                camera->process_keyboard(Camera::Direction::kRight, deltaTime * moveSpeed);
                break;
            case GLFW_KEY_SPACE:
                camera->process_keyboard(Camera::Direction::kUp, deltaTime * moveSpeed);
                break;
            case GLFW_KEY_C:
                camera->process_keyboard(Camera::Direction::kDown, deltaTime * moveSpeed);
                break;
            case GLFW_KEY_ESCAPE:
                if (window) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                if (window_close_handler) {
                    window_close_handler();
                }
                break;
            //case GLFW_KEY_1:
            //case GLFW_KEY_2:
            //case GLFW_KEY_3:
            //case GLFW_KEY_4:
            //case GLFW_KEY_5:
            //case GLFW_KEY_6:
            //case GLFW_KEY_7:
            //    // G-Buffer debug modes: 1=Position(RT1.rgb), 2=Albedo(RT3.rgb), 3=Normal(RT2.rgb), 4=Metallic(RT1.a), 5=Roughness(RT3.a), 6=AO(RT2.a), 7=Depth
            //    gbuffer_debug_mode = key - GLFW_KEY_1;
            //    LOG_INFO("InputManager: Key {} pressed, switched to G-Buffer debug mode {}", key, gbuffer_debug_mode);
            //    break;
            //case GLFW_KEY_0:
            //    // Return to normal rendering
            //    gbuffer_debug_mode = -1;
            //    LOG_INFO("InputManager: Switched to normal rendering");
            //    break;
        }
    });

    // Set up mouse movement callback for camera rotation
    set_mouse_move_callback([camera](float xOffset, float yOffset) {
        if (!camera) return;
        
        LOG_DEBUG("InputManager: Mouse callback called with offset ({}, {})", xOffset, yOffset);
        const float sensitivity = 1.0f;  // Increased sensitivity since Camera also applies its own
        camera->process_mouse_movement(xOffset * sensitivity, yOffset * sensitivity);
    });

    // Set up mouse button callback
    set_mouse_button_callback([](int /* button */, int /* action */) {
        // Mouse button handling is already done in InputManager
    });

    // Set up window close callback
    set_window_close_callback(window_close_handler);
    
    // Set up drag callbacks
    //set_drag_start_callback(drag_start_handler);
    //set_drag_update_callback(drag_update_handler);
    //set_drag_end_callback(drag_end_handler);
    
    // Set up viewport check callback
    viewport_check_handler_ = viewport_check_handler;
    
    LOG_INFO("InputManager: Input callbacks setup completed");
}

void InputManager::cleanup() {
    transform_manager_.reset();
    
    window_ = nullptr;
    gui_ = nullptr;
    camera_.reset();
    scene_ = nullptr;
    resource_manager_ = nullptr;
    
    // Clear callbacks
    keyboard_callback_ = nullptr;
    mouse_move_callback_ = nullptr;
    mouse_button_callback_ = nullptr;
    window_close_callback_ = nullptr;
}

void InputManager::process_input(float deltaTime) {
    if (!window_) {
        return;
    }

    // Always process keyboard input (not limited by cursor position)
    process_keyboard_input(deltaTime);
    
    // Only process mouse input if cursor is in viewport
    if (is_cursor_in_viewport()) {
        process_mouse_input();
    }
}

void InputManager::set_keyboard_callback(KeyboardCallback callback) {
    keyboard_callback_ = callback;
}

void InputManager::set_mouse_move_callback(MouseMoveCallback callback) {
    mouse_move_callback_ = callback;
}

void InputManager::set_mouse_button_callback(MouseButtonCallback callback) {
    mouse_button_callback_ = callback;
}

void InputManager::set_window_close_callback(WindowCloseCallback callback) {
    window_close_callback_ = callback;
}

void InputManager::set_drag_start_callback(DragStartCallback callback) {
    drag_start_callback_ = callback;
}

void InputManager::set_drag_update_callback(DragUpdateCallback callback) {
    drag_update_callback_ = callback;
}

void InputManager::set_drag_end_callback(DragEndCallback callback) {
    drag_end_callback_ = callback;
}

bool InputManager::is_key_pressed(int key) const {
    if (!window_) {
        return false;
    }
    return glfwGetKey(window_, key) == GLFW_PRESS;
}

bool InputManager::is_mouse_button_pressed(int button) const {
    if (!window_) {
        return false;
    }
    return glfwGetMouseButton(window_, button) == GLFW_PRESS;
}

void InputManager::get_cursor_position(double& x, double& y) const {
    if (!window_) {
        x = 0.0;
        y = 0.0;
        return;
    }
    glfwGetCursorPos(window_, &x, &y);
}



bool InputManager::is_dragging() const {
    return transform_manager_ ? transform_manager_->is_dragging() : false;
}

Transform InputManager::get_model_transform(const std::string& model_id) const {
    if (transform_manager_) {
        return transform_manager_->get_transform(model_id);
    }
    return Transform::identity();
}

TransformManager* InputManager::get_transform_manager() const {
    return transform_manager_.get();
}

bool InputManager::is_cursor_in_viewport() const {
    if (!viewport_check_handler_) {
        return true; 
    }

    double mouseX, mouseY;
    get_cursor_position(mouseX, mouseY);
    
    return viewport_check_handler_(mouseX, mouseY);
}

KeyboardInput InputManager::map_glfw_key_to_input(int glfwKey) {
    switch (glfwKey) {
        case GLFW_KEY_W: return KeyboardInput::kMoveForward;
        case GLFW_KEY_S: return KeyboardInput::kMoveBackward;
        case GLFW_KEY_A: return KeyboardInput::kMoveLeft;
        case GLFW_KEY_D: return KeyboardInput::kMoveRight;
        case GLFW_KEY_SPACE: return KeyboardInput::kMoveUp;
        case GLFW_KEY_C: return KeyboardInput::kMoveDown;
        case GLFW_KEY_ESCAPE: return KeyboardInput::kEscape;
        default: return KeyboardInput::kUnknown;
    }
}

MouseInput InputManager::map_glfw_button_to_input(int glfwButton) {
    switch (glfwButton) {
        case GLFW_MOUSE_BUTTON_LEFT: return MouseInput::kLeftButton;
        case GLFW_MOUSE_BUTTON_RIGHT: return MouseInput::kRightButton;
        case GLFW_MOUSE_BUTTON_MIDDLE: return MouseInput::kMiddleButton;
        default: return MouseInput::kUnknown;
    }
}

void InputManager::process_keyboard_input(float deltaTime) {
    // Check for ESC key to close window
    if (is_key_pressed(GLFW_KEY_ESCAPE)) {
        handle_key_input(KeyboardInput::kEscape, deltaTime);
    }

    // Check movement keys
    if (is_key_pressed(GLFW_KEY_W)) {
        handle_key_input(KeyboardInput::kMoveForward, deltaTime);
    }
    if (is_key_pressed(GLFW_KEY_S)) {
        handle_key_input(KeyboardInput::kMoveBackward, deltaTime);
    }
    if (is_key_pressed(GLFW_KEY_A)) {
        handle_key_input(KeyboardInput::kMoveLeft, deltaTime);
    }
    if (is_key_pressed(GLFW_KEY_D)) {
        handle_key_input(KeyboardInput::kMoveRight, deltaTime);
    }
    if (is_key_pressed(GLFW_KEY_SPACE)) {
        handle_key_input(KeyboardInput::kMoveUp, deltaTime);
    }
    if (is_key_pressed(GLFW_KEY_C)) {
        handle_key_input(KeyboardInput::kMoveDown, deltaTime);
    }
    
    // Check for number keys 0-7 for debug modes
    for (int i = GLFW_KEY_0; i <= GLFW_KEY_7; ++i) {
        if (is_key_pressed(i)) {
            if (keyboard_callback_) {
                keyboard_callback_(i, deltaTime);
            }
        }
    }
}

void InputManager::process_mouse_input() {
    // Check for mouse button state changes
    bool current_right_pressed = is_mouse_button_pressed(GLFW_MOUSE_BUTTON_RIGHT);
    
    if (current_right_pressed != right_mouse_pressed_) {
        right_mouse_pressed_ = current_right_pressed;
        LOG_DEBUG("Right mouse button state changed: {}", current_right_pressed ? "PRESSED" : "RELEASED");
        if (current_right_pressed) {
            // Reset first mouse when starting to drag
            first_mouse_ = true;
        }
        handle_mouse_button(MouseInput::kRightButton, current_right_pressed ? GLFW_PRESS : GLFW_RELEASE);
    }
    
    // Poll mouse position for movement 
    if (right_mouse_pressed_) {
        double xpos, ypos;
        get_cursor_position(xpos, ypos);
        
        LOG_DEBUG("Mouse polling: pos=({}, {}), last=({}, {}), first={}", 
                 xpos, ypos, last_mouse_x_, last_mouse_y_, first_mouse_);
        
        // Only process if mouse actually moved
        if (first_mouse_ || xpos != last_mouse_x_ || ypos != last_mouse_y_) {
            LOG_DEBUG("Mouse moved, calling handle_mouse_movement_callback");
            handle_mouse_movement_callback(static_cast<float>(xpos), static_cast<float>(ypos));
        } else {
            LOG_DEBUG("Mouse position unchanged, skipping movement processing");
        }
    }
    
    // Process left mouse button for dragging
    process_left_mouse_button();
}

void InputManager::handle_key_input(KeyboardInput input, float deltaTime) {
    if (!keyboard_callback_) {
        return;
    }

    // Convert KeyboardInput back to GLFW key for callback
    int glfwKey = GLFW_KEY_UNKNOWN;
    switch (input) {
        case KeyboardInput::kMoveForward: glfwKey = GLFW_KEY_W; break;
        case KeyboardInput::kMoveBackward: glfwKey = GLFW_KEY_S; break;
        case KeyboardInput::kMoveLeft: glfwKey = GLFW_KEY_A; break;
        case KeyboardInput::kMoveRight: glfwKey = GLFW_KEY_D; break;
        case KeyboardInput::kMoveUp: glfwKey = GLFW_KEY_SPACE; break;
        case KeyboardInput::kMoveDown: glfwKey = GLFW_KEY_C; break;
        case KeyboardInput::kEscape: 
            glfwKey = GLFW_KEY_ESCAPE;
            // Special handling for ESC - close window
            if (window_close_callback_) {
                window_close_callback_();
            }
            break;
        default: return;
    }

    keyboard_callback_(glfwKey, deltaTime);
}

void InputManager::handle_mouse_movement(float xPos, float yPos) {
    if (!mouse_move_callback_) {
        return;
    }

    if (first_mouse_) {
        last_mouse_x_ = xPos;
        last_mouse_y_ = yPos;
        first_mouse_ = false;
    }

    float xOffset = xPos - last_mouse_x_;
    float yOffset = last_mouse_y_ - yPos; 
    last_mouse_x_ = xPos;
    last_mouse_y_ = yPos;

    LOG_DEBUG("Mouse offset: ({}, {}), calling camera callback", xOffset, yOffset);
    mouse_move_callback_(xOffset, yOffset);
}

void InputManager::handle_mouse_movement_callback(float xPos, float yPos) {
    if (!right_mouse_pressed_) {
        LOG_DEBUG("Right mouse not pressed, skipping movement processing");
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    LOG_DEBUG("handle_mouse_movement_callback: pos=({}, {}), WantCaptureMouse={}, right_pressed={}", 
             xPos, yPos, io.WantCaptureMouse, right_mouse_pressed_);
    
    // Force ImGui to not capture mouse when we're doing camera rotation
    bool original_want_capture = io.WantCaptureMouse;
    io.WantCaptureMouse = false;
    
    LOG_DEBUG("Mouse movement: ({}, {}), right_pressed: {}", xPos, yPos, right_mouse_pressed_);
    handle_mouse_movement(xPos, yPos);
    
    // Restore original ImGui mouse capture state
    io.WantCaptureMouse = original_want_capture;
}

void InputManager::handle_mouse_button(MouseInput button, int action) {
    // Check if ImGui wants to capture mouse input
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return; // Let ImGui handle the mouse input
    }
    
    if (!mouse_button_callback_) {
        return;
    }

    // Convert MouseInput back to GLFW button for callback
    int glfwButton = GLFW_MOUSE_BUTTON_1;
    switch (button) {
        case MouseInput::kLeftButton: glfwButton = GLFW_MOUSE_BUTTON_LEFT; break;
        case MouseInput::kRightButton: glfwButton = GLFW_MOUSE_BUTTON_RIGHT; break;
        case MouseInput::kMiddleButton: glfwButton = GLFW_MOUSE_BUTTON_MIDDLE; break;
        default: return;
    }

    mouse_button_callback_(glfwButton, action);
}



void InputManager::process_left_mouse_button() {
    if (!drag_enabled_) {
        return;
    }

    bool current_left_pressed = is_mouse_button_pressed(GLFW_MOUSE_BUTTON_LEFT);
    
    if (current_left_pressed != left_mouse_pressed_) {
        left_mouse_pressed_ = current_left_pressed;
        
        if (current_left_pressed) {
            // Left mouse button pressed - start drag
            double mouse_x, mouse_y;
            get_cursor_position(mouse_x, mouse_y);
            handle_drag_start(static_cast<float>(mouse_x), static_cast<float>(mouse_y));
        } else {
            // Left mouse button released - end drag
            handle_drag_end();
        }
    } else if (current_left_pressed && is_dragging_) {
        // Left mouse button held and dragging
        double mouse_x, mouse_y;
        get_cursor_position(mouse_x, mouse_y);
        handle_drag_update(static_cast<float>(mouse_x), static_cast<float>(mouse_y));
    }
}

void InputManager::handle_drag_start(float screen_x, float screen_y) {
    if (!transform_manager_ || !drag_enabled_ || !camera_ || !scene_ || !resource_manager_) {
        return;
    }

    // Calculate viewport dimensions
    int window_width, window_height;
    glfwGetWindowSize(window_, &window_width, &window_height);
    
    bool started = transform_manager_->start_drag(
        screen_x, screen_y,
        static_cast<float>(window_width), static_cast<float>(window_height),
        *camera_, *scene_, *resource_manager_
    );
    
    is_dragging_ = started;
    
    // Call drag start callback if drag started successfully
    if (started && drag_start_callback_) {
        const auto& drag_info = transform_manager_->get_drag_info();
        drag_start_callback_(drag_info.model_id, screen_x, screen_y);
    }
}

void InputManager::handle_drag_update(float screen_x, float screen_y) {
    if (!is_dragging_ || !transform_manager_ || !camera_) {
        return;
    }

    // Calculate viewport dimensions
    int window_width, window_height;
    glfwGetWindowSize(window_, &window_width, &window_height);
    
    bool updated = transform_manager_->update_drag(
        screen_x, screen_y,
        static_cast<float>(window_width), static_cast<float>(window_height),
        *camera_
    );
    
    // Call drag update callback if drag updated successfully
    if (updated && drag_update_callback_) {
        const auto& drag_info = transform_manager_->get_drag_info();
        drag_update_callback_(drag_info.model_id, screen_x, screen_y);
    }
}

void InputManager::handle_drag_end() {
    if (!is_dragging_ || !transform_manager_) {
        return;
    }

    // Get drag info before ending drag for callback
    const auto& drag_info = transform_manager_->get_drag_info();
    std::string model_id = drag_info.model_id;
    
    transform_manager_->end_drag();
    is_dragging_ = false;
    
    // Call drag end callback
    if (drag_end_callback_) {
        drag_end_callback_(model_id);
    }
}


