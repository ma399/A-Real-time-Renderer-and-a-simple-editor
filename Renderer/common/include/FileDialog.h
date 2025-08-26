#pragma once

#include <string>
#include <vector>
#include <functional>

class FileDialog {
public:
    struct Filter {
        std::string description;
        std::string extensions;  
        
        Filter(const std::string& desc, const std::string& ext) 
            : description(desc), extensions(ext) {}
    };

    // Synchronous version (original)
    static std::string open_file(const std::string& title = "Open File",
                                const std::vector<Filter>& filters = {},
                                const std::string& default_path = "");

    // Asynchronous version with callback
    static void open_file_async(const std::string& title,
                               const std::vector<Filter>& filters,
                               const std::string& default_path,
                               std::function<void(const std::string&)> on_complete);

    // Predefined filter sets
    static std::vector<Filter> get_3d_model_filters();
    static std::vector<Filter> get_texture_filters();
    static std::vector<Filter> get_all_files_filter();

private:
#ifdef _WIN32
    static std::string open_file_windows(const std::string& title,
                                        const std::vector<Filter>& filters,
                                        const std::string& default_path);
    
    // Async version that runs in STA thread
    static void open_file_windows_async(const std::string& title,
                                       const std::vector<Filter>& filters,
                                       const std::string& default_path,
                                       std::function<void(const std::string&)> on_complete);
    
    static void initialize_com();
    static std::wstring string_to_wstring(const std::string& str);
    static std::string wstring_to_string(const std::wstring& wstr);
    static void set_dialog_filters(void* p_dialog, const std::vector<Filter>& filters);
#endif
};







