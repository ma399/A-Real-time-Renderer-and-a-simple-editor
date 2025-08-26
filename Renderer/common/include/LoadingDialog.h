#pragma once

#include <string>
#include <functional>
#include <imgui.h>

class LoadingDialog {
public:
    LoadingDialog();
    ~LoadingDialog();
    
    // Main control methods
    void show(const std::string& file_name);
    void hide();
    void update_progress(float progress, const std::string& message);
    void set_error(const std::string& error_message);
    void clear_error();
    
    // Rendering
    void render();
    
    // State queries
    bool is_visible() const { return visible_; }
    bool has_error() const { return has_error_; }
    float get_progress() const { return progress_; }
    
    // Callback setup
    void set_cancel_callback(std::function<void()> callback);
    
    // Style configuration
    void set_dialog_size(const ImVec2& size) { dialog_size_ = size; }
    void set_progress_bar_height(float height) { progress_bar_height_ = height; }
    
private:
    // State
    bool visible_;
    float progress_;
    std::string file_name_;
    std::string status_message_;
    std::string error_message_;
    bool has_error_;
    
    // Callbacks
    std::function<void()> cancel_callback_;
    
    // UI Configuration
    ImVec2 dialog_size_;
    float progress_bar_height_;
    
    // Internal methods
    void render_progress_dialog();
    void render_error_dialog();
    ImVec2 calculate_dialog_position();
};
