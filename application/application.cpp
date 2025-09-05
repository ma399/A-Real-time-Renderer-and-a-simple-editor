#include <stdexcept>
#include <filesystem>
#include <string>
#include <Application.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
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
     gbuffer_debug_mode_(-1),
     ssgi_exposure_(0.1f),
     ssgi_intensity_(1.0f),
     ssgi_max_steps_(32),
     ssgi_max_distance_(6.0f),
     ssgi_step_size_(0.15f),
     ssgi_thickness_(0.6f),
     ssgi_num_samples_(8) {
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
        
        // Enable OpenGL debug context
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

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
            viewport_width_,
            viewport_height_
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
        
        // Set up SSGI parameter callbacks
        ui_->set_ssgi_exposure_callback([this](float exposure) {
            this->set_ssgi_exposure(exposure);
        });
        
        ui_->set_ssgi_intensity_callback([this](float intensity) {
            this->set_ssgi_intensity(intensity);
        });
        
        // Set up SSGI compute parameter callbacks
        ui_->set_ssgi_max_steps_callback([this](int max_steps) {
            this->set_ssgi_max_steps(max_steps);
        });
        
        ui_->set_ssgi_max_distance_callback([this](float max_distance) {
            this->set_ssgi_max_distance(max_distance);
        });
        
        ui_->set_ssgi_step_size_callback([this](float step_size) {
            this->set_ssgi_step_size(step_size);
        });
        
        ui_->set_ssgi_thickness_callback([this](float thickness) {
            this->set_ssgi_thickness(thickness);
        });
        
        ui_->set_ssgi_num_samples_callback([this](int num_samples) {
            this->set_ssgi_num_samples(num_samples);
        });

        setup_opengl_debug_output();

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
    current_loading_model_path_ = filePath;
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

    // Use the new texture loading method instead of just loading mesh
    pending_model_with_textures_task_ = resource_manager_->load_model_with_textures_async(filePath, progressCallback, Async::TaskPriority::k_normal);
    load_state_ = LoadState::kLoading;
    last_progress_set_ = -1.0f;  // Reset progress tracking
}

void Application::on_viewport_resize(int width, int height) {
    if (!renderer_ || width <= 0 || height <= 0) {
        return;
    }

    viewport_width_ = width;
    viewport_height_ = height;

    // Update both framebuffer and viewport dimensions
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
    // Check for new texture-enabled model loading first
    if (pending_model_with_textures_task_.has_value()) {
        check_pending_model_with_textures_load();
        return;
    }
    
    // Fallback to legacy mesh-only loading
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
                    
                    // TODO: Convert to Renderable system
                    // scene_->add_renderable_reference(model_name);
                    LOG_INFO("Application: Model '{}' loaded (legacy system - TODO: convert to Renderable)", model_name);
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

// OpenGL debug callback function
void GLAPIENTRY opengl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                     GLsizei length, const GLchar* message, const void* userParam) {
    // Filter out non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::string source_str;
    switch (source) {
        case GL_DEBUG_SOURCE_API:             source_str = "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   source_str = "Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: source_str = "Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     source_str = "Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:     source_str = "Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           source_str = "Other"; break;
        default:                              source_str = "Unknown"; break;
    }

    std::string type_str;
    switch (type) {
        case GL_DEBUG_TYPE_ERROR:               type_str = "Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_str = "Undefined Behaviour"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         type_str = "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         type_str = "Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              type_str = "Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          type_str = "Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           type_str = "Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:               type_str = "Other"; break;
        default:                                type_str = "Unknown"; break;
    }

    // Log based on severity
    switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            LOG_ERROR("OpenGL [{}] [{}] ({}): {}", source_str, type_str, id, message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            LOG_WARN("OpenGL [{}] [{}] ({}): {}", source_str, type_str, id, message);
            break;
        case GL_DEBUG_SEVERITY_LOW:
            LOG_INFO("OpenGL [{}] [{}] ({}): {}", source_str, type_str, id, message);
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            LOG_DEBUG("OpenGL [{}] [{}] ({}): {}", source_str, type_str, id, message);
            break;
        default:
            LOG_INFO("OpenGL [{}] [{}] ({}): {}", source_str, type_str, id, message);
            break;
    }
}

void Application::setup_opengl_debug_output() {
    // Make sure we have a valid OpenGL context
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_ERROR("Application: Failed to initialize GLAD for debug output");
        return;
    }

    // Check if debug output is supported
    GLint flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        // Enable debug output
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // Makes sure errors are displayed synchronously
        glDebugMessageCallback(opengl_debug_callback, nullptr);
        
        // Control which messages are displayed
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        
        // Optionally disable notifications for less spam
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        
        LOG_INFO("Application: OpenGL debug output enabled");
    } else {
        LOG_WARN("Application: OpenGL debug context not available");
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
    scene_->clear_renderable_references();
    scene_->clear_light_references();
    LOG_INFO("Application: Scene cleared");
}

void Application::check_pending_model_with_textures_load() {
    if (!pending_model_with_textures_task_.has_value()) {
        if (load_state_ == LoadState::kLoading) {
            load_state_ = LoadState::kIdle;
        }
        return;
    }
    
    auto& task = pending_model_with_textures_task_.value();
       
    // Check if the task is ready (non-blocking)
    if (task.is_ready()) {
        LOG_INFO("Application: Pending model with textures task is READY, processing result");
        try {
            auto model_data = task.try_get();
            
            if (model_data.has_value() && !model_data.value().meshes.empty()) {
                const LoadedModelData& data = model_data.value();
                
                // Calculate total vertices for logging
                size_t total_vertices = 0;
                for (const auto& mesh_data : data.meshes) {
                    total_vertices += mesh_data.vertices.size();
                }
                
                LOG_INFO("Application: Model with textures loaded successfully - {} meshes with {} total vertices, {} materials, {} textures", 
                        data.meshes.size(), total_vertices, data.materials.size(), data.texture_paths.size());

                // Create Renderable with multiple Models
                auto renderable = std::make_shared<Renderable>(current_loading_model_name_);
                
                // Let ResourceManager handle all texture loading first
                resource_manager_->load_model_textures(data.texture_paths);
                
                // Create individual Models for each mesh
                for (size_t i = 0; i < data.meshes.size(); ++i) {
                    const auto& mesh_data = data.meshes[i];
                    
                    // Create mesh
                    auto mesh = std::make_shared<Mesh>(mesh_data.vertices, mesh_data.indices);
                    std::string mesh_id = current_loading_model_name_ + "_mesh_" + std::to_string(i);
                    resource_manager_->store_mesh_in_cache(mesh_id, mesh);
                    
                    // Get corresponding material
                    std::shared_ptr<Material> material;
                    if (mesh_data.material_index < data.materials.size()) {
                        material = std::make_shared<Material>(data.materials[mesh_data.material_index]);
                        std::string material_id = current_loading_model_name_ + "_material_" + std::to_string(mesh_data.material_index);
                        resource_manager_->store_material_in_cache(material_id, material);
                    } else {
                        material = std::make_shared<Material>(Material::create_pbr_default());
                        std::string material_id = current_loading_model_name_ + "_default_material_" + std::to_string(i);
                        resource_manager_->store_material_in_cache(material_id, material);
                    }
                    
                    // Create Model
                    auto model = std::make_shared<Model>(mesh.get(), material.get());
                    std::string model_id = current_loading_model_name_ + "_model_" + std::to_string(i);
                    resource_manager_->store_model_in_cache(model_id, model);
                    
                    // Add Model to Renderable
                    renderable->add_model(model_id);
                    
                    LOG_DEBUG("Application: Created model '{}' for mesh '{}' with material index {}", 
                             model_id, mesh_data.name, mesh_data.material_index);
                }
                
                LOG_INFO("Application: Created Renderable '{}' with {} models from {} meshes", 
                        current_loading_model_name_, data.meshes.size(), data.meshes.size());
                
                // Store Renderable in cache
                resource_manager_->store_renderable_in_cache(current_loading_model_name_, renderable);
                
                // Add Renderable to scene
                scene_->add_renderable_reference(current_loading_model_name_);
                LOG_INFO("Application: Added Renderable '{}' to scene", current_loading_model_name_);
                
                // Set position and transform for the Renderable
                TransformManager* transform_manager = input_manager_->get_transform_manager();
                if (transform_manager) {
                    glm::vec3 center_position(0.0f, 0.0f, -1.5f);
                    
                    Transform renderable_transform;
                    renderable_transform.set_position(center_position);
                    renderable_transform.set_scale(0.003f); 
                    
                    transform_manager->set_transform(current_loading_model_name_, renderable_transform);
                    
                    LOG_INFO("Application: Set transform for Renderable '{}' at position ({}, {}, {}) with scale {}", 
                            current_loading_model_name_, center_position.x, center_position.y, center_position.z, 0.1f);
                } else {
                    LOG_WARN("Application: Transform manager not available, Renderable positioned at origin");
                }
                
                load_state_ = LoadState::kFinished;
                
                if (ui_) {
                    ui_->set_model_loading_finished(current_loading_model_name_);
                }
            } else {
                LOG_ERROR("Application: Failed to load model with textures or model is empty");
                load_state_ = LoadState::kFailed;
                
                // Show error in inline progress
                if (ui_) {
                    ui_->set_model_loading_error(current_loading_model_name_, "Failed to load model with textures or model is empty");
                }
                
                // Reset to idle after error display
                load_state_ = LoadState::kIdle;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Application: Exception during model with textures loading: {}", e.what());
            load_state_ = LoadState::kFailed;
            
            // Show exception error in inline progress
            if (ui_) {
                ui_->set_model_loading_error(current_loading_model_name_, "Exception: " + std::string(e.what()));
            }
            
            // Reset to idle after error display
            load_state_ = LoadState::kIdle;
        }
        
        // Clear the task
        pending_model_with_textures_task_.reset();
        current_loading_model_name_.clear();
        current_loading_model_path_.clear();
        last_progress_set_ = -1.0f;  // Reset progress tracking
    }
}

void Application::set_ssgi_exposure(float exposure) {
    ssgi_exposure_ = exposure;
    if (renderer_) {
        renderer_->set_ssgi_exposure(exposure);
    }
    LOG_DEBUG("Application: SSGI exposure set to {}", exposure);
}

void Application::set_ssgi_intensity(float intensity) {
    ssgi_intensity_ = intensity;
    if (renderer_) {
        renderer_->set_ssgi_intensity(intensity);
    }
    LOG_DEBUG("Application: SSGI intensity set to {}", intensity);
}

void Application::set_ssgi_max_steps(int max_steps) {
    ssgi_max_steps_ = max_steps;
    if (renderer_) {
        renderer_->set_ssgi_max_steps(max_steps);
    }
    LOG_DEBUG("Application: SSGI max steps set to {}", max_steps);
}

void Application::set_ssgi_max_distance(float max_distance) {
    ssgi_max_distance_ = max_distance;
    if (renderer_) {
        renderer_->set_ssgi_max_distance(max_distance);
    }
    LOG_DEBUG("Application: SSGI max distance set to {}", max_distance);
}

void Application::set_ssgi_step_size(float step_size) {
    ssgi_step_size_ = step_size;
    if (renderer_) {
        renderer_->set_ssgi_step_size(step_size);
    }
    LOG_DEBUG("Application: SSGI step size set to {}", step_size);
}

void Application::set_ssgi_thickness(float thickness) {
    ssgi_thickness_ = thickness;
    if (renderer_) {
        renderer_->set_ssgi_thickness(thickness);
    }
    LOG_DEBUG("Application: SSGI thickness set to {}", thickness);
}

void Application::set_ssgi_num_samples(int num_samples) {
    ssgi_num_samples_ = num_samples;
    if (renderer_) {
        renderer_->set_ssgi_num_samples(num_samples);
    }
    LOG_DEBUG("Application: SSGI num samples set to {}", num_samples);
}
