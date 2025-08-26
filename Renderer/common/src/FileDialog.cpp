#include "FileDialog.h"
#include "Logger.h"
#include <thread>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <comdef.h>
#endif

std::string FileDialog::open_file(const std::string& title,
                                const std::vector<Filter>& filters,
                                const std::string& defaultPath) {
#ifdef _WIN32
    return open_file_windows(title, filters, defaultPath);
#else
    LOG_ERROR("File dialog not implemented for this platform");
    return "";
#endif
}

void FileDialog::open_file_async(const std::string& title,
                              const std::vector<Filter>& filters,
                              const std::string& defaultPath,
                              std::function<void(const std::string&)> onComplete) {
#ifdef _WIN32
    open_file_windows_async(title, filters, defaultPath, onComplete);
#else
    LOG_ERROR("Async file dialog not implemented for this platform");
    if (onComplete) {
        onComplete("");
    }
#endif
}


#ifdef _WIN32

void FileDialog::initialize_com() {
    static bool comInitialized = false;
    if (!comInitialized) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (SUCCEEDED(hr)) {
            comInitialized = true;
            LOG_DEBUG("FileDialog: COM initialized successfully");
        } else {
            LOG_ERROR("FileDialog: Failed to initialize COM: {:#x}", static_cast<unsigned>(hr));
        }
    }
}

std::wstring FileDialog::string_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string FileDialog::wstring_to_string(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

void FileDialog::set_dialog_filters(void* pDialog, const std::vector<Filter>& filters) {
    IFileDialog* dialog = static_cast<IFileDialog*>(pDialog);
    if (!dialog) return;
    
    std::vector<COMDLG_FILTERSPEC> filterSpecs;
    std::vector<std::wstring> descriptions;
    std::vector<std::wstring> extensions;
    
    // Add user filters
    for (const auto& filter : filters) {
        descriptions.push_back(string_to_wstring(filter.description));
        extensions.push_back(string_to_wstring(filter.extensions));
    }
    
    // Always add "All Files" filter
    descriptions.push_back(L"All Files");
    extensions.push_back(L"*.*");
    
    for (size_t i = 0; i < descriptions.size(); ++i) {
        filterSpecs.push_back({descriptions[i].c_str(), extensions[i].c_str()});
    }
    
    dialog->SetFileTypes(static_cast<UINT>(filterSpecs.size()), filterSpecs.data());
    dialog->SetFileTypeIndex(1); 
}

std::string FileDialog::open_file_windows(const std::string& title,
                                       const std::vector<Filter>& filters,
                                       const std::string& defaultPath) {
    initialize_com();
    
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                 IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
    
    if (FAILED(hr)) {
        LOG_ERROR("FileDialog: Failed to create FileOpenDialog: {:#x}", static_cast<unsigned>(hr));
        return "";
    }
    
    // Set title
    std::wstring wTitle = string_to_wstring(title);
    pFileOpen->SetTitle(wTitle.c_str());
    
    // Set filters
    set_dialog_filters(pFileOpen, filters);
    
    // Set default path if provided
    if (!defaultPath.empty()) {
        std::wstring wDefaultPath = string_to_wstring(defaultPath);
        IShellItem* pDefaultFolder = nullptr;
        hr = SHCreateItemFromParsingName(wDefaultPath.c_str(), NULL, IID_IShellItem, 
                                        reinterpret_cast<void**>(&pDefaultFolder));
        if (SUCCEEDED(hr)) {
            pFileOpen->SetDefaultFolder(pDefaultFolder);
            pDefaultFolder->Release();
        }
    }
    
    // Set options for long path support
    FILEOPENDIALOGOPTIONS options = 0;
    pFileOpen->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_SUPPORTSTREAMABLEITEMS;
    pFileOpen->SetOptions(options);
    
    // Show the dialog
    hr = pFileOpen->Show(nullptr);  // No parent window for now
    
    std::string result;
    if (SUCCEEDED(hr)) {
        IShellItem* pItem = nullptr;
        hr = pFileOpen->GetResult(&pItem);
        if (SUCCEEDED(hr)) {
            PWSTR pszFilePath = nullptr;
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            if (SUCCEEDED(hr)) {
                result = wstring_to_string(pszFilePath);
                LOG_INFO("FileDialog: Selected file: {}", result);
                CoTaskMemFree(pszFilePath);
            }
            pItem->Release();
        }
    } else if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        LOG_ERROR("FileDialog: Dialog failed: {:#x}", static_cast<unsigned>(hr));
    }
    
    pFileOpen->Release();
    return result;
}

void FileDialog::open_file_windows_async(const std::string& title,
                                     const std::vector<Filter>& filters,
                                     const std::string& defaultPath,
                                     std::function<void(const std::string&)> onComplete) {
    LOG_INFO("FileDialog: Starting async file dialog with title '{}'", title);
    
    // Create a detached thread with STA COM apartment
    std::thread([title, filters, defaultPath, onComplete]() {
        // Initialize COM in STA mode for this thread
        HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        
        if (FAILED(hrInit)) {
            LOG_ERROR("FileDialog: Failed to initialize COM in worker thread: {:#x}", static_cast<unsigned>(hrInit));
            if (onComplete) {
                onComplete("");
            }
            return;
        }
        
        LOG_DEBUG("FileDialog: COM initialized successfully in worker thread");
        
        // Create and show dialog in this STA thread
        IFileOpenDialog* pFileOpen = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                     IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        
        std::string result;
        
        if (SUCCEEDED(hr)) {
            // Set title
            std::wstring wTitle = string_to_wstring(title);
            pFileOpen->SetTitle(wTitle.c_str());
            
            // Set filters
            set_dialog_filters(pFileOpen, filters);
            
            // Set default path if provided
            if (!defaultPath.empty()) {
                std::wstring wDefaultPath = string_to_wstring(defaultPath);
                IShellItem* pDefaultFolder = nullptr;
                hr = SHCreateItemFromParsingName(wDefaultPath.c_str(), NULL, IID_IShellItem, 
                                               reinterpret_cast<void**>(&pDefaultFolder));
                if (SUCCEEDED(hr)) {
                    pFileOpen->SetDefaultFolder(pDefaultFolder);
                    pDefaultFolder->Release();
                }
            }
            
            // Set options for long path support
            FILEOPENDIALOGOPTIONS options = 0;
            pFileOpen->GetOptions(&options);
            options |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_SUPPORTSTREAMABLEITEMS;
            pFileOpen->SetOptions(options);
            
            // Show the dialog (this blocks only this worker thread)
            hr = pFileOpen->Show(nullptr);
            
            if (SUCCEEDED(hr)) {
                IShellItem* pItem = nullptr;
                hr = pFileOpen->GetResult(&pItem);
                if (SUCCEEDED(hr)) {
                    PWSTR pszFilePath = nullptr;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr)) {
                        result = wstring_to_string(pszFilePath);
                        LOG_INFO("FileDialog: Async dialog selected file: {}", result);
                        CoTaskMemFree(pszFilePath);
                    }
                    pItem->Release();
                }
            } else if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                LOG_ERROR("FileDialog: Async dialog failed: {:#x}", static_cast<unsigned>(hr));
            } else {
                LOG_DEBUG("FileDialog: Async dialog cancelled by user");
            }
            
            pFileOpen->Release();
        } else {
            LOG_ERROR("FileDialog: Failed to create FileOpenDialog in worker thread: {:#x}", static_cast<unsigned>(hr));
        }
        
        // Cleanup COM
        CoUninitialize();
        
        // Call completion callback
        if (onComplete) {
            onComplete(result);
        }
        
        LOG_DEBUG("FileDialog: Worker thread finished, result: '{}'", result.empty() ? "(cancelled)" : result);
        
    }).detach();
}

// Predefined filter sets
std::vector<FileDialog::Filter> FileDialog::get_3d_model_filters() {
    return {
        {"3D Models", "*.obj;*.fbx;*.gltf;*.ply;*.dae"},
        {"OBJ Files", "*.obj"},
        {"FBX Files", "*.fbx"},
        {"GLTF Files", "*.gltf;*.glb"},
        {"PLY Files", "*.ply"},
        {"DAE Files", "*.dae"}
    };
}

std::vector<FileDialog::Filter> FileDialog::get_texture_filters() {
    return {
        {"Image Files", "*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.hdr"},
        {"PNG Files", "*.png"},
        {"JPEG Files", "*.jpg;*.jpeg"},
        {"BMP Files", "*.bmp"},
        {"TGA Files", "*.tga"},
        {"HDR Files", "*.hdr"}
    };
}

std::vector<FileDialog::Filter> FileDialog::get_all_files_filter() {
    return {
        {"All Files", "*.*"}
    };
}

#endif
