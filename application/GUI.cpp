#include "GUI.h"
#include "Logger.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <Window.h>

GUI::GUI() 
    : initialized_(false)
    , needs_render_(true)
    , render_texture_id_(0)
    , render_texture_width_(1024)  
    , render_texture_height_(768)  
    , last_viewport_width_(0)
    , last_viewport_height_(0)
    // , loadingDialog_(std::make_unique<LoadingDialog>())
    , file_dialog_manager_(std::make_unique<FileDialogManager>())
{
    file_dialog_manager_->set_file_selected_callback([this](const std::string& path) {
        this->on_file_selected(path);
    });
    
    file_dialog_manager_->set_dialog_cancelled_callback([this]() {
        this->on_file_dialog_cancelled();
    });

    // loadingDialog_->setCancelCallback([]() {
    //     LOG_INFO("GUI: Loading cancelled by user");
    //     // The actual cancellation logic should be handled by Application
    // });
}

GUI::~GUI() {
    cleanup();
}

bool GUI::initialize(GLFWwindow* window) {
    if (initialized_) {
        std::cout << "GUI already initialized" << std::endl;
        return true;
    }
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    ImGuiIO& io = ImGui::GetIO(); 
    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui::StyleColorsLight();  
    
    // Setup Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
    setup_modern_style();
    try {
        initialize_fonts();
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize fonts: {}", e.what());
        // Fallback to default font
        ImGuiIO& io = ImGui::GetIO();
        font_regular_ = io.Fonts->AddFontDefault();
        font_subtitle_ = font_regular_;
        font_title_ = font_regular_;
        font_regular_large_ = font_regular_;
        font_subtitle_large_ = font_regular_;
        font_title_large_ = font_regular_;
        current_title_font_ = font_regular_;
        current_subtitle_font_ = font_regular_;
        current_content_font_ = font_regular_;
        LOG_INFO("GUI: Using default fonts as fallback");
    }
    


    LOG_INFO("Imgui initialized successfully");
    
    initialized_ = true;
    return true;
}

void GUI::cleanup() {
    if (!initialized_) {
        return;
    }
      
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    initialized_ = false;
    std::cout << "GUI cleanup completed" << std::endl;
}

void GUI::begin_frame() {
    if (!initialized_) {
        return;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GUI::end_frame() {
    if (!initialized_) {
        return;
    }
    
    // Ensure proper OpenGL state for ImGui rendering
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Clear with normal background color
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void GUI::render() {
    if (!initialized_) {
        LOG_WARN("GUI: Render called but GUI not initialized");
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    static int lastWindowWidth = 0;
    static int lastWindowHeight = 0;
    int currentWidth = static_cast<int>(io.DisplaySize.x);
    int currentHeight = static_cast<int>(io.DisplaySize.y);
    
    static bool first_render = true;
    if (first_render) {
        LOG_INFO("GUI: First render - Display size: {}x{}", currentWidth, currentHeight);
        first_render = false;
    }
    
    if (currentWidth != lastWindowWidth || currentHeight != lastWindowHeight) {
        update_fonts_for_window_size(currentWidth, currentHeight);
        lastWindowWidth = currentWidth;
        lastWindowHeight = currentHeight;
    }
    
    // Prevent covering
    render_smart_layout();
    
    render_controls();
    render_log_panel();
    render_viewport();
    render_resource_cache_panel();

    // Display file dialog using FileDialogManager
    file_dialog_manager_->display_dialog();
}

void GUI::render_smart_layout() {
    ImGuiIO& io = ImGui::GetIO();
    
    static bool layoutInitialized = false;
    static ImVec2 lastDisplaySize = ImVec2(0, 0);
    
    // Reset layout if window size changed significantly
    if (!layoutInitialized || 
        abs(io.DisplaySize.x - lastDisplaySize.x) > 50 || 
        abs(io.DisplaySize.y - lastDisplaySize.y) > 50) {
        
        layoutInitialized = true;
        lastDisplaySize = io.DisplaySize;
        
        // Calculate optimal window positions to avoid overlap
        float controlPanelWidth = io.DisplaySize.x * CONTROL_PANEL_WIDTH_RATIO;
        float resourcePanelWidth = io.DisplaySize.x * RESOURCE_PANEL_WIDTH_RATIO;
        float logPanelHeight = io.DisplaySize.y * LOG_PANEL_HEIGHT_RATIO;
        float viewportWidth = io.DisplaySize.x - controlPanelWidth - resourcePanelWidth;
        float viewportHeight = io.DisplaySize.y - logPanelHeight;
        
        // Store layout for windows - Control Panel should extend to bottom edge
        next_window_positions_["Control Panel"] = ImVec2(0, 0);
        next_window_sizes_["Control Panel"] = ImVec2(controlPanelWidth, io.DisplaySize.y); // Full height to align with bottom
    
    next_window_positions_["3D Viewport"] = ImVec2(controlPanelWidth, 0);
    next_window_sizes_["3D Viewport"] = ImVec2(viewportWidth, viewportHeight);
    
    next_window_positions_["Resource Cache Panel"] = ImVec2(controlPanelWidth + viewportWidth, 0);
    next_window_sizes_["Resource Cache Panel"] = ImVec2(resourcePanelWidth, io.DisplaySize.y);
    
    next_window_positions_["Log Panel"] = ImVec2(controlPanelWidth, viewportHeight);
    next_window_sizes_["Log Panel"] = ImVec2(viewportWidth, logPanelHeight);
    }
}

void GUI::render_controls() {
    const std::string windowName = "Control Panel";
    
    if (next_window_positions_.find(windowName) != next_window_positions_.end()) {
        ImGui::SetNextWindowPos(next_window_positions_[windowName], ImGuiCond_Always);
        ImGui::SetNextWindowSize(next_window_sizes_[windowName], ImGuiCond_Always);
    }
    
    bool window_open = ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    
    with_font(current_title_font_, [&](){
        ImGui::Text("3D Renderer");
        ImGui::Text("Real-time rendering engine");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    });

    with_font(current_subtitle_font_, [&](){
      if (ImGui::CollapsingHeader("File Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        with_font(current_content_font_, [&](){
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.9f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.8f, 1.0f));
          if (ImGui::Button("Import OBJ File", ImVec2(-1, 0))) {
            LOG_INFO("GUI: Import OBJ File button clicked");
            file_dialog_manager_->open_file_dialog("Select 3D Model File", FileDialog::get_3d_model_filters(), "./assets/models/");
          }
          ImGui::PopStyleColor(3);
          ImGui::Spacing();
        });
      }
    });
       

    with_font(current_subtitle_font_, [&](){
      if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        with_font(current_content_font_, [&](){
        static bool enableShadows = true;
        static bool enableSSAO = false;
        static float shadowBias = 0.005f;
        static bool enableSSGI = true;
        static float ssgiExposure = 1.0f;    // Higher default exposure for brighter result
        static float ssgiIntensity = 3.0f;   // Higher default intensity

        ImGui::Checkbox("Enable Shadows", &enableShadows);
        ImGui::Checkbox("Enable SSAO", &enableSSAO);
        ImGui::Checkbox("Enable SSGI", &enableSSGI);

        if (enableShadows) {
          ImGui::Text("Shadow Map Size");
          const char* sizes[] = {"512", "1024", "2048", "4096"};
          static int currentSize = 2;
          ImGui::Combo("##shadowMapSize", &currentSize, sizes, IM_ARRAYSIZE(sizes));

          ImGui::Text("Shadow Bias");
          ImGui::SliderFloat("##shadowBias", &shadowBias, 0.001f, 0.01f, "%.4f");
        }
        
        if (enableSSGI) {
          ImGui::Text("SSGI Exposure");
          if (ImGui::SliderFloat("##ssgiExposure", &ssgiExposure, 0.1f, 5.0f, "%.2f")) {
            if (ssgiExposureCallback_) {
              ssgiExposureCallback_(ssgiExposure);
            }
          }
          
          ImGui::Text("SSGI Intensity");
          if (ImGui::SliderFloat("##ssgiIntensity", &ssgiIntensity, 0.1f, 5.0f, "%.2f")) {
            if (ssgiIntensityCallback_) {
              ssgiIntensityCallback_(ssgiIntensity);
            }
          }
          
          ImGui::Separator();
          ImGui::Text("SSGI Compute Parameters");
          
          static int ssgiMaxSteps = 32;
          static float ssgiMaxDistance = 6.0f;
          static float ssgiStepSize = 0.15f;
          static float ssgiThickness = 1.2f;     // Higher for better hit detection
          static int ssgiNumSamples = 8;
          
          ImGui::Text("Max Steps");
          if (ImGui::SliderInt("##ssgiMaxSteps", &ssgiMaxSteps, 8, 64)) {
            if (ssgiMaxStepsCallback_) {
              ssgiMaxStepsCallback_(ssgiMaxSteps);
            }
          }
          
          ImGui::Text("Max Distance");
          if (ImGui::SliderFloat("##ssgiMaxDistance", &ssgiMaxDistance, 1.0f, 20.0f, "%.1f")) {
            if (ssgiMaxDistanceCallback_) {
              ssgiMaxDistanceCallback_(ssgiMaxDistance);
            }
          }
          
          ImGui::Text("Step Size");
          if (ImGui::SliderFloat("##ssgiStepSize", &ssgiStepSize, 0.05f, 0.5f, "%.3f")) {
            if (ssgiStepSizeCallback_) {
              ssgiStepSizeCallback_(ssgiStepSize);
            }
          }
          
          ImGui::Text("Thickness");
          if (ImGui::SliderFloat("##ssgiThickness", &ssgiThickness, 0.2f, 3.0f, "%.2f")) {
            if (ssgiThicknessCallback_) {
              ssgiThicknessCallback_(ssgiThickness);
            }
          }
          
          ImGui::Text("Num Samples");
          if (ImGui::SliderInt("##ssgiNumSamples", &ssgiNumSamples, 1, 16)) {
            if (ssgiNumSamplesCallback_) {
              ssgiNumSamplesCallback_(ssgiNumSamples);
            }
          }
        }
        ImGui::Spacing();
        });
      }
    });
    

    ImGuiIO& io = ImGui::GetIO();
    with_font(current_subtitle_font_, [&](){
      if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        with_font(current_content_font_, [&](){
          ImGui::Text("FPS: %.1f", io.Framerate);
          ImGui::Text("Frame Time: %.3f ms", 1000.0f / io.Framerate);

          static float values[90] = {};
          static int valuesOffset = 0;
          values[valuesOffset] = io.Framerate;
          valuesOffset = (valuesOffset + 1) % IM_ARRAYSIZE(values);

          ImGui::PlotLines("##fps", values, IM_ARRAYSIZE(values), valuesOffset,
                           "FPS", 0.0f, 120.0f, ImVec2(0, 80));
          ImGui::Spacing();
        });
      }
    });
      
    ImGui::End();
}

void GUI::render_viewport() {
    const std::string windowName = "3D Viewport";
    if (next_window_positions_.find(windowName) != next_window_positions_.end()) {
        ImGui::SetNextWindowPos(next_window_positions_[windowName], ImGuiCond_Always);
        ImGui::SetNextWindowSize(next_window_sizes_[windowName], ImGuiCond_Always);
    }
    
    ImGui::Begin("3D Viewport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);  
    
    // Store viewport position and size for mouse boundary checking
    viewport_position_ = ImGui::GetWindowPos();
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
    viewport_position_.x += contentMin.x;
    viewport_position_.y += contentMin.y;
    viewport_size_.x = contentMax.x - contentMin.x;
    viewport_size_.y = contentMax.y - contentMin.y;
    
    // Get the actual viewport content area
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    int currentViewportWidth = static_cast<int>(viewportSize.x);
    int currentViewportHeight = static_cast<int>(viewportSize.y);
    
    // Check if viewport size has changed
    if (currentViewportWidth != last_viewport_width_ || currentViewportHeight != last_viewport_height_) {
        last_viewport_width_ = currentViewportWidth;
        last_viewport_height_ = currentViewportHeight;
        
        // Call the resize callback if it's set
        if (viewportResizeCallback_ && currentViewportWidth > 0 && currentViewportHeight > 0) {
            viewportResizeCallback_(currentViewportWidth, currentViewportHeight);
        }
    }
    
    if (render_texture_id_ != 0) {
        // Use the full viewport size for rendering
        ImVec2 imageSize = viewportSize;
        
        // Display the 3D rendered texture
        ImGui::Image(reinterpret_cast<void*>(render_texture_id_), imageSize, ImVec2(0, 1), ImVec2(1, 0));
    } 
    
    ImGui::End();
}

bool GUI::is_mouse_in_viewport(double mouseX, double mouseY) const {
    // Check if mouse position is within the viewport content area
    return (mouseX >= viewport_position_.x && 
            mouseX <= viewport_position_.x + viewport_size_.x &&
            mouseY >= viewport_position_.y && 
            mouseY <= viewport_position_.y + viewport_size_.y);
}

void GUI::render_log_panel() {
    const std::string windowName = "Log Panel";
    if (next_window_positions_.find(windowName) != next_window_positions_.end()) {
        ImGui::SetNextWindowPos(next_window_positions_[windowName], ImGuiCond_Always);
        ImGui::SetNextWindowSize(next_window_sizes_[windowName], ImGuiCond_Always);
    }
    
    ImGui::Begin("Log Panel", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    ImGui::Text("Console Output");
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Clear", ImVec2(70, 25))) {
        Logger::get_instance().clear();
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    
    auto imguiSink = Logger::get_instance().get_imgui_sink();
    if (imguiSink) {
        bool autoScroll = imguiSink->get_auto_scroll();
        if (ImGui::Checkbox("Auto Scroll", &autoScroll)) {
            imguiSink->set_auto_scroll(autoScroll);
        }
    } else {
        ImGui::Text("Warning: ImGuiSink is null!");
    }
    
    ImGui::Separator();
    
    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    if (imguiSink) {
        const auto& entries = imguiSink->get_entries();
        
        for (const auto& entry : entries) {
            
            float color[4];
            ImGuiSink_mt::get_level_color(entry.level, color);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color[0], color[1], color[2], color[3]));
            
            ImGui::Text("[%s] %s %s", 
                       entry.timestamp.c_str(),
                       ImGuiSink_mt::getLevelString(entry.level),
                       entry.message.c_str());
            
            ImGui::PopStyleColor();
        }
        
        if (imguiSink->get_auto_scroll() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    
    ImGui::EndChild();
    ImGui::End();
}

void GUI::render_resource_cache_panel() {
    const std::string windowName = "Resource Cache Panel";
    if (next_window_positions_.find(windowName) != next_window_positions_.end()) {
        ImGui::SetNextWindowPos(next_window_positions_[windowName], ImGuiCond_Always);
        ImGui::SetNextWindowSize(next_window_sizes_[windowName], ImGuiCond_Always);
    }
    
    ImGui::Begin("Resource Cache", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    with_font(current_title_font_, [&](){
        ImGui::Text("Resource Cache");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    });

    // Textures section
    with_font(current_subtitle_font_, [&]() {
        if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            with_font(current_content_font_, [&]() {
                if (getTextureNamesCallback_) {
                    auto textureNames = getTextureNamesCallback_();
                    if (textureNames.empty()) {
                        ImGui::TextDisabled("No textures loaded");
                    } else {
                        for (const auto& name : textureNames) {
                            ImGui::Text("%s", name.c_str());
                            ImGui::SameLine();
                            ImGui::PushID(("tex_add_" + name).c_str());
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
                            if (ImGui::Button("Del")) {
                                // TODO
                                LOG_INFO("Add texture button clicked for: {}", name);
                            }
                            ImGui::PopStyleColor(3);
                            ImGui::PopID();
                        }
                    }
                } else {
                    ImGui::TextDisabled("Texture callback not set");
                }
                ImGui::Spacing();
            });
        }
    });

    // Models section
    with_font(current_subtitle_font_, [&]() {
        if (ImGui::CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            with_font(current_content_font_, [&]() {
                if (getModelNamesCallback_) {
                    auto modelNames = getModelNamesCallback_();
                    if (modelNames.empty()) {
                        ImGui::TextDisabled("No models loaded");
                    } else {
                        for (const auto& name : modelNames) {
                            // Check if this model is currently loading
                            auto loadingIt = model_loading_states_.find(name);
                            bool is_loading = (loadingIt != model_loading_states_.end() && loadingIt->second.is_loading);
                            
                            if (is_loading) {
                                // Model name
                                ImGui::Text("%s", name.c_str());
                                
                                // Progress bar below the name
                                ImGui::PushID(("progress_" + name).c_str());
                                float progress = loadingIt->second.progress;
                                std::string progressText = std::to_string(static_cast<int>(progress * 100)) + "%";
                                ImGui::ProgressBar(progress, ImVec2(-1.0f, 20.0f), progressText.c_str());
                                
                                // Status message
                                if (!loadingIt->second.status_message.empty()) {
                                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", loadingIt->second.status_message.c_str());
                                }
                                ImGui::PopID();
                            } else {
                                ImGui::Text("%s", name.c_str());
                                ImGui::SameLine();
                                ImGui::PushID(("model_add_" + name).c_str());
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.8f, 1.0f));
                                if (ImGui::Button("Del")) {
                                    LOG_INFO("Add model button clicked for: {}", name);
                                    if (modelAddCallback_) {
                                        modelAddCallback_(name);
                                    }
                                }
                                ImGui::PopStyleColor(3);
                                ImGui::PopID();
                            }
                            
                            ImGui::Spacing();
                        }
                    }
                    
                    // Show loading models that might not be in the loaded list yet
                    for (const auto& [modelName, loadingState] : model_loading_states_) {
                        // Check if this loading model is already shown above
                        auto modelIt = std::find(modelNames.begin(), modelNames.end(), modelName);
                        if (modelIt == modelNames.end() && loadingState.is_loading) {
                            // Model name
                            ImGui::Text("%s", modelName.c_str());
                            
                            // Progress bar below the name
                            ImGui::PushID(("progress_new_" + modelName).c_str());
                            float progress = loadingState.progress;
                            std::string progressText = std::to_string(static_cast<int>(progress * 100)) + "%";
                            ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), progressText.c_str());
                            
                            // Status message
                            if (!loadingState.status_message.empty()) {
                                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", loadingState.status_message.c_str());
                            }
                            ImGui::PopID();
                            ImGui::Spacing();
                        }
                    }
                } else {
                    ImGui::TextDisabled("Model callback not set");
                }
                ImGui::Spacing();
            });
        }
    });

    // Materials section
    with_font(current_subtitle_font_, [&]() {
        if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();
            with_font(current_content_font_, [&]() {
                if (getMaterialNamesCallback_) {
                    auto materialNames = getMaterialNamesCallback_();
                    if (materialNames.empty()) {
                        ImGui::TextDisabled("No materials loaded");
                    } else {
                        for (const auto& name : materialNames) {
                            ImGui::Text("%s", name.c_str());
                            ImGui::SameLine();
                            ImGui::PushID(("mat_add_" + name).c_str());
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.5f, 0.2f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.6f, 0.3f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.4f, 0.1f, 1.0f));
                            if (ImGui::Button("Del")) {
                                // TODO: Add Materials
                                LOG_INFO("Add material button clicked for: {}", name);
                            }
                            ImGui::PopStyleColor(3);
                            ImGui::PopID();
                        }
                    }
                } else {
                    ImGui::TextDisabled("Material callback not set");
                }
                ImGui::Spacing();
            });
        }
    });
    
    ImGui::End();
}

// Modified from https://github.com/GraphicsProgramming/dear-imgui-styles
void GUI::setup_modern_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    style.WindowPadding        = ImVec2(15, 15);
    style.WindowRounding       = 5.0f;
    style.FramePadding         = ImVec2(5, 5);
    style.FrameRounding        = 4.0f;
    style.ItemSpacing          = ImVec2(12, 8);
    style.ItemInnerSpacing     = ImVec2(8, 6);
    style.IndentSpacing        = 25.0f;
    style.ScrollbarSize        = 15.0f;
    style.ScrollbarRounding    = 9.0f;
    style.GrabMinSize          = 5.0f;
    style.GrabRounding         = 3.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.40f, 0.39f, 0.38f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.40f, 0.39f, 0.38f, 0.77f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.92f, 0.91f, 0.88f, 0.70f);
    colors[ImGuiCol_ChildBg]                = ImVec4(1.00f, 0.98f, 0.95f, 0.58f); 
    colors[ImGuiCol_PopupBg]                = ImVec4(0.92f, 0.91f, 0.88f, 0.92f);
    colors[ImGuiCol_Border]                 = ImVec4(0.84f, 0.83f, 0.80f, 0.65f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.99f, 1.00f, 0.40f, 0.78f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 1.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(1.00f, 0.98f, 0.95f, 0.47f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(1.00f, 0.98f, 0.95f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.00f, 0.00f, 0.00f, 0.21f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.90f, 0.91f, 0.00f, 0.78f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.25f, 1.00f, 0.00f, 0.80f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 0.00f, 0.00f, 0.14f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.14f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.99f, 1.00f, 0.22f, 0.86f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.25f, 1.00f, 0.00f, 0.76f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.25f, 1.00f, 0.00f, 0.86f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.00f, 0.00f, 0.00f, 0.32f); 
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.25f, 1.00f, 0.00f, 0.78f); 
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.25f, 1.00f, 0.00f, 1.00f); 
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.00f, 0.00f, 0.04f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.25f, 1.00f, 0.00f, 0.78f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(1.00f, 0.98f, 0.95f, 0.73f); 
}

void GUI::set_render_texture(GLuint textureId, int width, int height) {
    render_texture_id_ = textureId;
    render_texture_width_ = width;
    render_texture_height_ = height;
}

void GUI::set_obj_import_callback(std::function<void(const std::string&)> callback) {
    importCallback_ = callback;
}

void GUI::set_viewport_resize_callback(std::function<void(int, int)> callback) {
    viewportResizeCallback_ = callback;
}

void GUI::set_resource_cache_callback(std::function<std::vector<std::string>()> getTextureNames,
                                      std::function<std::vector<std::string>()> getModelNames,
                                      std::function<std::vector<std::string>()> getMaterialNames) {
    getTextureNamesCallback_ = getTextureNames;
    getModelNamesCallback_ = getModelNames;
    getMaterialNamesCallback_ = getMaterialNames;
}

void GUI::on_file_selected(const std::string& filePath) {
    LOG_DEBUG("GUI: File selected via FileDialogManager: {}", filePath);
    
    // Use callback to external system (ResourceManager through Application)
    if (importCallback_) {
        importCallback_(filePath);
        LOG_DEBUG("GUI: File import callback executed successfully");
    } else {
        LOG_ERROR("GUI: objImportCallback_ is null, cannot process file");
    }
}

void GUI::on_file_dialog_cancelled() {
    LOG_DEBUG("GUI: File dialog cancelled by user");
}

void GUI::set_model_loading_progress(const std::string& modelName, float progress, const std::string& message) {
    auto& state = model_loading_states_[modelName];
    state.is_loading = true;
    state.progress = std::clamp(progress, 0.0f, 1.0f);
    state.status_message = message;
    needs_render_ = true;
    
    LOG_DEBUG("GUI: Model loading progress updated - {}: {:.1f}% - {}", modelName, progress * 100.0f, message);
}

void GUI::set_model_loading_finished(const std::string& modelName) {
    auto it = model_loading_states_.find(modelName);
    if (it != model_loading_states_.end()) {
        model_loading_states_.erase(it);
        needs_render_ = true;
        LOG_INFO("GUI: Model loading finished - {}", modelName);
    }
}

void GUI::set_model_loading_error(const std::string& modelName, const std::string& errorMessage) {
    auto& state = model_loading_states_[modelName];
    state.is_loading = false;
    state.progress = 0.0f;
    state.status_message = "Error: " + errorMessage;
    needs_render_ = true;
    
    LOG_ERROR("GUI: Model loading error - {}: {}", modelName, errorMessage);
}

void GUI::initialize_fonts() {
    ImGuiIO& io = ImGui::GetIO(); 
    io.Fonts->Clear();
    
    LOG_INFO("GUI: Initializing fonts...");
    
    font_regular_ = io.Fonts->AddFontFromFileTTF(
        "../assets/fonts/Inter/static/Inter_24pt-Regular.ttf", 20.0f);
    if (!font_regular_) {
        LOG_WARN("GUI: Failed to load regular font, using default");
        font_regular_ = io.Fonts->AddFontDefault();
    }
    
    font_subtitle_ = io.Fonts->AddFontFromFileTTF(
        "../assets/fonts/Inter/static/Inter_24pt-SemiBold.ttf", 20.0f);
    if (!font_subtitle_) {
        LOG_WARN("GUI: Failed to load subtitle font, using regular font");
        font_subtitle_ = font_regular_;
    }
    
    font_title_ = io.Fonts->AddFontFromFileTTF(
        "../assets/fonts/Inter/static/Inter_28pt-Bold.ttf", 24.0f);
    if (!font_title_) {
        LOG_WARN("GUI: Failed to load title font, using regular font");
        font_title_ = font_regular_;
    }
    
    font_regular_large_ = io.Fonts->AddFontFromFileTTF(
        "../assets/fonts/Inter/static/Inter_28pt-Regular.ttf", 40.0f);
    if (!font_regular_large_) {
        LOG_WARN("GUI: Failed to load large regular font, using regular font");
        font_regular_large_ = font_regular_;
    }
    
    font_subtitle_large_ = io.Fonts->AddFontFromFileTTF(
        "../assets/fonts/Inter/static/Inter_28pt-SemiBold.ttf", 40.0f);
    if (!font_subtitle_large_) {
        LOG_WARN("GUI: Failed to load large subtitle font, using subtitle font");
        font_subtitle_large_ = font_subtitle_;
    }
    
    font_title_large_ = io.Fonts->AddFontFromFileTTF(
        "../assets/fonts/Inter/static/Inter_28pt-Bold.ttf", 48.0f);
    if (!font_title_large_) {
        LOG_WARN("GUI: Failed to load large title font, using title font");
        font_title_large_ = font_title_;
    }
    
    // Build font atlas
    io.Fonts->Build();
    LOG_INFO("GUI: Font atlas built successfully");
    
    // Initialize current fonts with default small sizes
    current_title_font_ = font_title_;
    current_subtitle_font_ = font_subtitle_;
    current_content_font_ = font_regular_;
    
    LOG_INFO("GUI: Fonts initialized successfully");
}

void GUI::update_fonts_for_window_size(int windowWidth, int windowHeight) {
    // Calculate window area for scaling decision
    int windowArea = windowWidth * windowHeight;
    
    // Define thresholds for font scaling
    // Small: < 1280x720 (921,600 pixels)
    // Medium: 1280x720 to 1920x1080 (2,073,600 pixels)  
    // Large: > 1920x1080
    const int SMALL_THRESHOLD = 921600;
    const int LARGE_THRESHOLD = 2073600;
    
    if (windowArea < SMALL_THRESHOLD) {
        // Use small fonts for small windows
        current_title_font_ = font_title_;
        current_subtitle_font_ = font_subtitle_;
        current_content_font_ = font_regular_;
    } else if (windowArea > LARGE_THRESHOLD) {
        // Use large fonts for large windows
        current_title_font_ = font_title_large_;
        current_subtitle_font_ = font_subtitle_large_;
        current_content_font_ = font_regular_large_;
    } else {
        // Use medium fonts for medium windows (mix of small and large)
        current_title_font_ = font_title_large_;
        current_subtitle_font_ = font_subtitle_;
        current_content_font_ = font_regular_;
    }
}

//font helper
void GUI::with_font(ImFont* font, std::function<void()> func){
    if (font){
        ImGui::PushFont(font);
    }
    func();
    if (font){
        ImGui::PopFont();
    }
}

void GUI::set_model_add_callback(std::function<void(const std::string&)> callback) {
    modelAddCallback_ = callback;
}

void GUI::set_ssgi_exposure_callback(std::function<void(float)> callback) {
    ssgiExposureCallback_ = callback;
}

void GUI::set_ssgi_intensity_callback(std::function<void(float)> callback) {
    ssgiIntensityCallback_ = callback;
}

void GUI::set_ssgi_max_steps_callback(std::function<void(int)> callback) {
    ssgiMaxStepsCallback_ = callback;
}

void GUI::set_ssgi_max_distance_callback(std::function<void(float)> callback) {
    ssgiMaxDistanceCallback_ = callback;
}

void GUI::set_ssgi_step_size_callback(std::function<void(float)> callback) {
    ssgiStepSizeCallback_ = callback;
}

void GUI::set_ssgi_thickness_callback(std::function<void(float)> callback) {
    ssgiThicknessCallback_ = callback;
}

void GUI::set_ssgi_num_samples_callback(std::function<void(int)> callback) {
    ssgiNumSamplesCallback_ = callback;
}
