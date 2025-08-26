#pragma once

#include <string>
#include <functional>
#include "FileDialog.h"

class FileDialogManager {
public:
    FileDialogManager();
    ~FileDialogManager();
    
    // Async file dialog operations
    void open_file_dialog(const std::string& title,
                         const std::vector<FileDialog::Filter>& filters,
                         const std::string& default_path = "");
    
    bool display_dialog();
    
    // Result access
    std::string get_selected_file_path() const;
    std::string get_selected_file_name() const;
    
    // Callbacks
    void set_file_selected_callback(std::function<void(const std::string&)> callback);
    void set_dialog_cancelled_callback(std::function<void()> callback);
    
    // File utilities for GUI display
    std::string get_file_size_string(const std::string& file_path);
    bool file_exists(const std::string& file_path);
    
private:
    std::function<void(const std::string&)> fileSelectedCallback_;
    std::function<void()> dialogCancelledCallback_;
    std::string lastSelectedPath_;
    
    struct {
        std::string title;
        std::vector<FileDialog::Filter> filters;
        std::string defaultPath;
        bool ready = false;
    } pendingDialog_;
};
