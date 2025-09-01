#pragma once

#include <string>
#include <functional>
#include <map>
#include <memory>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "FileDialogManager.h"

class Window;

class GUI {
public:
    GUI();
    ~GUI();
    
    bool initialize(GLFWwindow* window);
    void cleanup();
    void begin_frame();
    void end_frame();
    void render();
    void set_render_texture(GLuint texture_id, int width, int height);
    void set_obj_import_callback(std::function<void(const std::string&)> callback);
    void set_viewport_resize_callback(std::function<void(int, int)> callback);
    void set_model_add_callback(std::function<void(const std::string&)> callback);
    void update_fonts_for_window_size(int window_width, int window_height);
    bool needs_render() const { return needs_render_; }
    void reset_render_flag() { needs_render_ = false; }
    
    // Resource cache callbacks
    void set_resource_cache_callback(std::function<std::vector<std::string>()> get_texture_names,
                                    std::function<std::vector<std::string>()> get_model_names,
                                    std::function<std::vector<std::string>()> get_material_names);
    
    bool is_mouse_in_viewport(double mouse_x, double mouse_y) const;
    
    // Model loading progress management
    void set_model_loading_progress(const std::string& model_name, float progress, const std::string& message);
    void set_model_loading_finished(const std::string& model_name);
    void set_model_loading_error(const std::string& model_name, const std::string& error_message);

private:
    bool initialized_;
    bool needs_render_;
    
    GLuint render_texture_id_;
    int render_texture_width_;
    int render_texture_height_;
    
    // Layout percentages
    static constexpr float CONTROL_PANEL_WIDTH_RATIO = 0.25f;  // 25% of window width
    static constexpr float RESOURCE_PANEL_WIDTH_RATIO = 0.25f; // 25% of window width
    static constexpr float LOG_PANEL_HEIGHT_RATIO = 0.3f;      // 30% of window height
    
    std::function<void(const std::string&)> importCallback_;
    std::function<void(int, int)> viewportResizeCallback_;
    std::function<void(const std::string&)> modelAddCallback_;
    
    // Resource cache callbacks
    std::function<std::vector<std::string>()> getTextureNamesCallback_;
    std::function<std::vector<std::string>()> getModelNamesCallback_;
    std::function<std::vector<std::string>()> getMaterialNamesCallback_;
    
    // Loading state tracking for individual models
    struct ModelLoadingState {
        bool is_loading = false;
        float progress = 0.0f;
        std::string status_message;
    };
    std::map<std::string, ModelLoadingState> model_loading_states_;
    
    // viewport size tracking
    int last_viewport_width_;
    int last_viewport_height_;    
   
    // render
    void render_viewport();
    void render_controls();
    void render_log_panel();
    void render_resource_cache_panel();
    void setup_modern_style();
    void render_smart_layout();
    
    // file operations (now managed by FileDialogManager)
    std::unique_ptr<FileDialogManager> file_dialog_manager_;
    void on_file_selected(const std::string& file_path);
    void on_file_dialog_cancelled();

    // font
    ImFont* font_regular_;
    ImFont* font_subtitle_;
    ImFont* font_title_;
    ImFont* font_regular_large_;
    ImFont* font_subtitle_large_;
    ImFont* font_title_large_;

    ImFont* current_title_font_;
    ImFont* current_content_font_;
    ImFont* current_subtitle_font_;

    void initialize_fonts();
    void with_font(ImFont* font, std::function<void()> func);
    
    // Smart layout system
    std::map<std::string, ImVec2> next_window_positions_;
    std::map<std::string, ImVec2> next_window_sizes_;
    
    // Viewport boundary tracking
    mutable ImVec2 viewport_position_;
    mutable ImVec2 viewport_size_;
    
};
