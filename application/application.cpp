#include <stdexcept>
#include <filesystem>
#include <Application.h>
#include <CoroutineThreadPoolScheduler.h>
#include <Model.h>
#include <Material.h>
#include <Renderer.h>
#include <InputManager.h>
#include <TransformManager.h>


Application::Application(const std::string& title) 
   : title_(title),
     load_state_(LoadState::kIdle),
     last_progress_set_(-1.0f),
     initialized_(false),
     gbuffer_debug_mode_(-1) {
}

Application::~Application() {
    shutdown();
}

bool Application::initialize(){
    try {

        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

        width_ = static_cast<int>(mode->width * 2.0 / 3.0);
        height_ = static_cast<int>(mode->height * 2.0 / 3.0);

        window_ = std::make_unique<Window>(width_, height_, title_.c_str());
        camera_ = std::make_shared<Camera>();
        // Initialize GUI
        ui_ = std::make_unique<GUI>();
        if (!ui_->initialize(window_->get_window_ptr())) {
            throw std::runtime_error("Failed to initialize GUI");
        }
        
        // Initialize InputManager
        input_manager_ = std::make_unique<InputManager>();
        if (!input_manager_->initialize(window_->get_window_ptr(), ui_)) {
            throw std::runtime_error("Failed to initialize InputManager");
        }
        
        // Enable debug logging
        //Logger::get_instance().enable_debug();

        // Calculate initial viewport size and aspect ratio
        calculate_initial_viewport();
        

        auto tempScene = std::make_unique<Scene>();
        renderer_ = std::make_unique<glRenderer::Renderer>(
            width_,
            height_
        );
        
        renderer_->initialize();
        
        // Initialize CoroutineResourceManager

        try {
            resource_manager_ = std::make_unique<CoroutineResourceManager>();
            LOG_INFO("Application: CoroutineResourceManager created successfully");
            
            // Create scene first
            scene_ = resource_manager_->create_simple_scene();
            
            // Initialize transform system now that all components are ready
            if (!input_manager_->initialize_transform_system(camera_, scene_.get(), resource_manager_.get())) {
                LOG_WARN("Application: Failed to initialize transform system - drag functionality will be disabled");
            } else {
                LOG_INFO("Application: Transform system initialized successfully");
                
                // Set up rotation animation for the default cube model
                TransformManager* transform_manager = input_manager_->get_transform_manager();
                if (transform_manager) {
                    // Note: Animation functionality will be added to TransformManager later if needed
                    LOG_INFO("Application: Transform manager available for future animation setup");
                } else {
                    LOG_WARN("Application: Could not get transform manager to set up cube rotation");
                }
            }

            // Enable deferred rendering
            renderer_->set_deferred_rendering(true);
            LOG_INFO("Application: Deferred rendering enabled");
            
            // Enable SSGI
            renderer_->set_ssgi_enabled(true);
            LOG_INFO("Application: SSGI enabled");
            

        } catch (const std::exception& e) {
            LOG_ERROR("Application: Failed to initialize ResourceManager or SimpleScene: {}", e.what());
            throw;
        }

        setup_event_handlers();
        
        // Setup input callbacks through InputManager
        if (input_manager_) {
            input_manager_->setup_input_callbacks(
                camera_,
                window_->get_window_ptr(),
                gbuffer_debug_mode_,
                [this]() { this->handle_window_close(); },
                [this](double mouseX, double mouseY) -> bool {
                    return ui_ ? ui_->is_mouse_in_viewport(mouseX, mouseY) : true;
                }
            );
        }

        // Set up GUI callbacks
        ui_->set_obj_import_callback([this](const std::string& filePath) {
            this->request_model_load(filePath);
        });
        
        // Set up viewport resize callback
        ui_->set_viewport_resize_callback([this](int width, int height) {
            this->on_viewport_resize(width, height);
        });
        
        // Set up resource cache callbacks
        ui_->set_resource_cache_callback(
            [this]() -> std::vector<std::string> { return this->get_texture_names(); },
            [this]() -> std::vector<std::string> { return this->get_model_names(); },
            [this]() -> std::vector<std::string> { return this->get_material_names(); }
        );

        initialized_ = true;
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Application initialization failed: ", e.what());
        return false;
    }

}

void Application::run() {
    if (!initialized_) {
        LOG_ERROR("Application not initialized!");
        return;
    }
    
    last_frame_time_ = static_cast<float>(glfwGetTime());

    while (!window_->should_close()) {
        
        update_delta_time();
        glfwPollEvents();
        
        // Process main thread coroutines
        Async::CoroutineThreadPoolScheduler::get_instance().process_main_thread_coroutines();

        // Check for completed async loading
        check_pending_model_load();
        
        // Process input
        if (input_manager_) {
            input_manager_->process_input(delta_time_);
        }

        // Render 
        if (resource_manager_) {
            try {    
                if (!scene_->is_empty()) {
                    
                    // Get transform manager for rendering
                    TransformManager* transform_manager = input_manager_->get_transform_manager();
                    if (transform_manager) {
                        // Use deferred rendering if enabled, otherwise use forward rendering
                        if (renderer_->is_deferred_rendering_enabled()) {
                            LOG_DEBUG("Application: Using deferred rendering");
                            renderer_->render_deferred(*scene_, *camera_, *resource_manager_, *transform_manager);
                        } else {
                            LOG_DEBUG("Application: Using forward rendering with TransformManager");
                            renderer_->render(*scene_, *camera_, *resource_manager_, *transform_manager);
                        }
                    } else {
                        LOG_ERROR("Application: No transform manager available");
                    }

                }
            } catch (const std::exception& e) {
                LOG_ERROR("Application: Exception during rendering: {}", e.what());
            }
        } else {
            LOG_WARN("Application: ResourceManager not available, skipping rendering");
        }

        // GUI rendering
        ui_->set_render_texture(renderer_->get_color_texture(), viewport_width_, viewport_height_);
        ui_->begin_frame();
        ui_->render();
        ui_->end_frame();

        glfwSwapBuffers(window_->get_window_ptr());
    }
}

void Application::update_delta_time() {
    float currentTime = static_cast<float>(glfwGetTime());
    delta_time_ = currentTime - last_frame_time_;
    last_frame_time_ = currentTime;
}

void Application::shutdown() {
    if (!initialized_) {
        return;
    }
    
    if (ui_) {
        ui_->cleanup();
        ui_.reset();
    }
    
    if (input_manager_) {
        input_manager_->cleanup();
        input_manager_.reset();
    }
    
    renderer_.reset();
    window_.reset();
    
    glfwTerminate();
    initialized_ = false;
}

void Application::request_model_load(const std::string& filePath) {
    LOG_INFO("Application::requestModelLoad CALLED for: {}", filePath);
    
    if (!resource_manager_) {
        LOG_ERROR("Application: CoroutineResourceManager not initialized");
        if (ui_) {
            std::filesystem::path path(filePath);
            ui_->set_model_loading_error(path.filename().string(), "System not initialized");
        }
        return;
    }
    
    load_state_ = LoadState::kImportRequested;
    
    std::filesystem::path path(filePath);
    current_loading_model_name_ = path.filename().string();
    current_loading_model_path_ = filePath;  // Store the full file path
    if (ui_) {
        ui_->set_model_loading_progress(current_loading_model_name_, 0.1f, "Starting load...");
    }

    // Store the current model name for the callback closure
    std::string currentModelName = current_loading_model_name_;
    
    // Create progress callback to update GUI 
    std::function<void(float, const std::string&)> progressCallback = [this, currentModelName](float progress, const std::string& message) {
        // For debugging: log which thread this is called from
        bool isMainThread = (std::this_thread::get_id() == std::this_thread::get_id());
        
        LOG_DEBUG("Progress callback: {:.1f}% '{}' (main thread: {})", 
                  progress * 100.0f, message, isMainThread);

        if (ui_) {
            ui_->set_model_loading_progress(currentModelName, progress, message);
            LOG_DEBUG("GUI progress updated (thread: {})", isMainThread ? "main" : "worker");
        }
    };

    pending_model_task_ = resource_manager_->load_async<Mesh>(filePath, progressCallback, Async::TaskPriority::k_normal);
    load_state_ = LoadState::kLoading;
    last_progress_set_ = -1.0f;  // Reset progress tracking
}

void Application::on_viewport_resize(int width, int height) {
    if (!renderer_ || width <= 0 || height <= 0) {
        return;
    }

    viewport_width_ = width;
    viewport_height_ = height;

    renderer_->resize_framebuffer(width, height);
       
    LOG_INFO("Viewport resized: {}x{}", width, height);
}

void Application::calculate_initial_viewport() {    
    // TODO: Create a constant file
    const float CONTROL_PANEL_WIDTH_RATIO = 0.25f;  // 25% of window width
    const float LOG_PANEL_HEIGHT_RATIO = 0.3f;      // 30% of window height
    
    viewport_width_ = static_cast<int>((1 - CONTROL_PANEL_WIDTH_RATIO) * static_cast<float>(width_));
    viewport_height_ = static_cast<int>((1 - LOG_PANEL_HEIGHT_RATIO) * static_cast<float>(height_));
    
    LOG_INFO("Initial viewport calculated: {}x{}", 
        viewport_width_, viewport_height_);
}


void Application::handle_window_close() {
    if (window_) {
        glfwSetWindowShouldClose(window_->get_window_ptr(), true);
    }
}

void Application::setup_event_handlers() {
    GLFWwindow* window_ptr = window_->get_window_ptr();
    
    glfwSetWindowUserPointer(window_ptr, this);
    glfwSetFramebufferSizeCallback(window_ptr, framebuffer_size_callback);
}

void Application::check_pending_model_load() {
    if (!pending_model_task_.has_value()) {
        if (load_state_ == LoadState::kLoading) {
            load_state_ = LoadState::kIdle;
        }
        return;
    }
    
    auto& task = pending_model_task_.value();
       
    // Check if the task is ready (non-blocking)
    if (task.is_ready()) {
        LOG_INFO("Application: Pending task is READY, processing result");
        try {
            auto mesh = task.try_get();
            
            if (mesh.has_value() && mesh.value()) {
                LOG_INFO("Application: Mesh loaded successfully");

                // Let ResourceManager handle everything - mesh is already cached
                std::string mesh_path = current_loading_model_path_;
                std::string model_name = current_loading_model_name_; // Use mesh name as model name
                
                // Request ResourceManager to create a model with default material
                // ResourceManager will handle material creation and model assembly
                auto assembled_model = resource_manager_->create_model_with_default_material(mesh_path, model_name);
                if (assembled_model) {
                    // CRITICAL: Force mesh setup in main thread to ensure VAO is initialized
                    auto mesh = assembled_model->get_mesh();
                    
                    // Simply get the assembled model from ResourceManager and add to scene
                    scene_->add_model_reference(model_name);
                    LOG_INFO("Application: Added model '{}' to scene", model_name);
                    // Set position to top-left corner of screen
                    TransformManager* transform_manager = input_manager_->get_transform_manager();
                    if (transform_manager) {
                        
                        glm::vec3 center_position(0.0f, 0.0f, -1.5f);
                        
                        Transform model_transform;
                        model_transform.set_position(center_position);
                        model_transform.set_scale(5.0f); // Scale UP to make it clearly visible
                        
                        transform_manager->set_transform(model_name, model_transform);
                        
                        LOG_INFO("Application: Set transform for model '{}' at position ({}, {}, {}) with scale {}", 
                                model_name, center_position.x, center_position.y, center_position.z, 5.0f);
                    } else {
                        LOG_WARN("Application: Transform manager not available, model positioned at origin");
                    }
                } else {
                    LOG_ERROR("Application: Failed to create model '{}' from mesh '{}'", model_name, mesh_path);
                }
                
                load_state_ = LoadState::kFinished;
                
                if (ui_) {
                    ui_->set_model_loading_finished(current_loading_model_name_);
                }
            } else {
                LOG_ERROR("Application: Failed to load model via coroutine or model is empty");
                load_state_ = LoadState::kFailed;
                
                // Show error in inline progress
                if (ui_) {
                    ui_->set_model_loading_error(current_loading_model_name_, "Failed to load model or model is empty");
                }
                
                // Reset to idle after error display
                load_state_ = LoadState::kIdle;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Application: Exception during coroutine model loading: {}", e.what());
            load_state_ = LoadState::kFailed;
            
            // Show exception error in inline progress
            if (ui_) {
                ui_->set_model_loading_error(current_loading_model_name_, "Exception: " + std::string(e.what()));
            }
            
            // Reset to idle after error display
            load_state_ = LoadState::kIdle;
        }
        
        // Clear the task a
        pending_model_task_.reset();
        current_loading_model_name_.clear();
        current_loading_model_path_.clear();
        last_progress_set_ = -1.0f;  // Reset progress tracking
    }
}

void Application::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->width_ = width;
        app->height_ = height;
        glViewport(0, 0, width, height);
    }
}

void Application::mouse_movement_callback(GLFWwindow* window, double xpos, double ypos) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app && app->input_manager_) {
        app->input_manager_->handle_mouse_movement_callback(static_cast<float>(xpos), static_cast<float>(ypos));
    }
}

std::vector<std::string> Application::get_texture_names() const {
    if (!resource_manager_) {
        return {};
    }
    return resource_manager_->get_cached_resource_names<Texture>();
}

std::vector<std::string> Application::get_model_names() const {
    if (!resource_manager_) {
        return {};
    }
    return resource_manager_->get_cached_resource_names<Mesh>();
}

std::vector<std::string> Application::get_material_names() const {
    if (!resource_manager_) {
        return {};
    }
    return resource_manager_->get_cached_resource_names<Material>();
}



bool Application::add_light_to_scene(const std::string& lightId, 
                                 const std::string& lightType,
                                 float x, float y, float z,
                                 float r, float g, float b) {
    if (!resource_manager_) {
        LOG_ERROR("Application: CoroutineResourceManager not initialized");
        return false;
    }
    
    LOG_INFO("Application: Adding {} light '{}' to scene at ({}, {}, {}) with color ({}, {}, {})", 
             lightType, lightId, x, y, z, r, g, b);
    
    try {
        std::shared_ptr<Light> light;
        
        if (lightType == "directional") {
            light = std::make_shared<DirectionalLight>(glm::vec3(x, y, z), glm::vec3(r, g, b));
        } else if (lightType == "point") {
            light = std::make_shared<PointLight>(glm::vec3(x, y, z), glm::vec3(r, g, b));
        } else if (lightType == "spot") {
            light = std::make_shared<SpotLight>(glm::vec3(x, y, z), glm::vec3(0, -1, 0), glm::vec3(r, g, b));
        } else {
            LOG_ERROR("Application: Unknown light type '{}'", lightType);
            return false;
        }
        
        // Store light in ResourceManager's cache
        resource_manager_->store_light_in_cache(lightId, light);
        
        // Add light reference to scene
        scene_->add_light_reference(lightId);
        LOG_INFO("Application: Light '{}' successfully added to scene", lightId);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("Application: Exception while adding light '{}': {}", lightId, e.what());
        return false;
    }
}



void Application::clear_scene() {
    scene_->clear_model_references();
    scene_->clear_light_references();
    LOG_INFO("Application: Scene cleared");
}


