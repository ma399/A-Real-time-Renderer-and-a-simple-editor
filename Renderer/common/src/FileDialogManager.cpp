#include "FileDialogManager.h"
#include "FileDialog.h"
#include "Logger.h"
#include <filesystem>

FileDialogManager::FileDialogManager() {
    LOG_INFO("FileDialogManager: Initialized");
}

FileDialogManager::~FileDialogManager() {
    LOG_DEBUG("FileDialogManager: Destroyed");
}

void FileDialogManager::open_file_dialog(const std::string& title,
                                       const std::vector<FileDialog::Filter>& filters,
                                       const std::string& defaultPath) {
    LOG_INFO("FileDialogManager: Starting async file dialog with title '{}'", title);
    
    // Use the new async FileDialog API
    FileDialog::open_file_async(title, filters, defaultPath, 
        [this](const std::string& selectedFile) {
            if (!selectedFile.empty()) {
                lastSelectedPath_ = selectedFile;
                LOG_INFO("FileDialogManager: Async dialog selected file: {}", selectedFile);
                
                if (fileSelectedCallback_) {
                    fileSelectedCallback_(selectedFile);
                }
            } else {
                LOG_DEBUG("FileDialogManager: Async dialog cancelled");
                if (dialogCancelledCallback_) {
                    dialogCancelledCallback_();
                }
            }
        });
    
    LOG_INFO("FileDialogManager: Async dialog '{}' started with path: {}", title, defaultPath);
}

bool FileDialogManager::display_dialog() {
    // With async implementation, this method is no longer needed for dialog display
    // It's kept for compatibility but doesn't do anything
    return false;
}



std::string FileDialogManager::get_selected_file_path() const {
    return lastSelectedPath_;
}

std::string FileDialogManager::get_selected_file_name() const {
    if (!lastSelectedPath_.empty()) {
        std::filesystem::path path(lastSelectedPath_);
        return path.filename().string();
    }
    return "";
}

void FileDialogManager::set_file_selected_callback(std::function<void(const std::string&)> callback) {
    fileSelectedCallback_ = callback;
}

void FileDialogManager::set_dialog_cancelled_callback(std::function<void()> callback) {
    dialogCancelledCallback_ = callback;
}

std::string FileDialogManager::get_file_size_string(const std::string& filePath) {
    try {
        if (!std::filesystem::exists(filePath)) {
            return "Unknown";
        }
        
        auto fileSize = std::filesystem::file_size(filePath);
        
        if (fileSize < 1024) {
            return std::to_string(fileSize) + " bytes";
        } else if (fileSize < 1024 * 1024) {
            return std::to_string(fileSize / 1024.0f).substr(0, 5) + " KB";
        } else {
            return std::to_string(fileSize / (1024.0f * 1024.0f)).substr(0, 5) + " MB";
        }
    } catch (const std::exception& e) {
        LOG_ERROR("FileDialogManager: Error getting file size for '{}': {}", filePath, e.what());
        return "Error";
    }
}

bool FileDialogManager::file_exists(const std::string& filePath) {
    try {
        return std::filesystem::exists(filePath);
    } catch (const std::exception&) {
        return false;
    }
}


