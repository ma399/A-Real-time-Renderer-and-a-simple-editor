#pragma once

#include <memory>
#include <atomic>
#include <optional>

#include "GUI.h"
#include "Camera.h"
#include "InputManager.h"

#include <Logger.h>
#include <Window.h>
#include <CoroutineResourceManager.h>
#include <Scene.h>
#include <Light.h>
#include <Renderable.h>

// Forward declarations
class Model;
namespace glRenderer {
    class Renderer;
}

// GPU model handle for renderer  
using GPUModelHandle = size_t;

// Application load states
enum class LoadState {
    kIdle,
    kImportRequested,
    kLoading,
    kFinished,
    kFailed
};

class Application {
public:
    Application(const std::string& title);
    ~Application();

    // Main application lifecycle
    bool initialize();
    void run();
    void shutdown();

    LoadState get_load_state() const { return load_state_; }
    void set_load_state(LoadState state) { load_state_ = state; }

    // File operations
    void request_model_load(const std::string& file_path);
    void check_pending_model_load();
    void check_pending_model_with_textures_load();

    // Scene management operations

    bool add_light_to_scene(const std::string& light_id, 
                            const std::string& light_type,
                            float x, float y, float z,
                            float r, float g, float b);
    Scene& get_scene() { return *scene_; }
    const Scene& get_scene() const { return *scene_; }
    void clear_scene();    
    
    // Resource cache access for GUI
    std::vector<std::string> get_texture_names() const;
    std::vector<std::string> get_model_names() const;
    std::vector<std::string> get_material_names() const;
    
    // InputManager access for GLFW callbacks
    InputManager* get_input_manager() const { return input_manager_.get(); }

private:
    std::unique_ptr<Window> window_;
    std::unique_ptr<glRenderer::Renderer> renderer_;
    std::unique_ptr<GUI> ui_;
    std::shared_ptr<Camera> camera_;
    std::unique_ptr<InputManager> input_manager_;
    
    std::string title_;
    
    // Scene management
    std::unique_ptr<Scene> scene_;  

    // Resource management
    std::unique_ptr<CoroutineResourceManager> resource_manager_;    
    std::optional<Async::Task<std::shared_ptr<Mesh>>> pending_model_task_;
    std::optional<Async::Task<LoadedModelData>> pending_model_with_textures_task_;

    std::atomic<LoadState> load_state_;
    float last_progress_set_;                                   // Track last progress value to avoid redundant updates
    std::string current_loading_model_name_;                     // Track the name of the currently loading model
    std::string current_loading_model_path_;                     // Track the full path of the currently loading model

    bool initialized_;
    int width_;
    int height_;
    float delta_time_;
    float last_frame_time_;
    int viewport_width_;
    int viewport_height_;
    
    // Debug mode for G-Buffer visualization
    int gbuffer_debug_mode_;  // -1 = normal rendering, 0-6 = debug modes

    void update_delta_time();
    void setup_event_handlers();
    void setup_opengl_debug_output();
    void on_viewport_resize(int width, int height);
    void calculate_initial_viewport();
    
    // Input callback handlers
    void handle_mouse_movement(float xOffset, float yOffset);
    void handle_window_close();

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_movement_callback(GLFWwindow* window, double xpos, double ypos);
};
