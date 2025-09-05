#include "Renderer.h"
#include "Logger.h"
#include "Camera.h"
#include "CoroutineResourceManager.h"
#include "TransformManager.h"
#include "Light.h"
#include "Texture.h"
#include <iostream>
#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace glRenderer {

    Renderer::Renderer(
        int width, 
        int height
    ): 
       width_(width),
       height_(height),
       viewport_width_(width),
       viewport_height_(height),
       framebuffer_(0),
       color_texture_(nullptr),
       depth_texture_(nullptr),
       use_framebuffer_(false),
       g_buffer_fbo_(0),
       g_position_texture_(nullptr),
       g_albedo_metallic_texture_(nullptr),
       g_normal_roughness_texture_(nullptr),
       g_motion_ao_texture_(nullptr),
       g_emissive_texture_(nullptr),
       g_depth_texture_(nullptr),
       use_deferred_rendering_(false),
       shadow_light_pos_(-2.0f, 4.0f, -1.0f),
       shadow_light_target_(0.0f, 0.0f, 0.0f),
       last_light_space_matrix_(1.0f),
       screen_quad_mesh_(nullptr),
       skybox_vao_(0),
       skybox_vbo_(0),
       ssao_fbo_(0),
       ssao_raw_texture_(nullptr),
       ssao_final_texture_(nullptr),
       ssao_noise_texture_(nullptr),
       use_ssao_(false),
       ssgi_fbo_(0),
       ssgi_raw_texture_(nullptr),
       ssgi_final_texture_(nullptr),
       ssgi_prev_texture_(nullptr),
       lit_scene_texture_(nullptr),
       use_ssgi_(false),
       ssgi_exposure_(1.0f),    // Match GUI default - higher for brighter result
       ssgi_intensity_(3.0f),   // Match GUI default - higher intensity
       ssgi_max_steps_(32),
       ssgi_max_distance_(6.0f),
       ssgi_step_size_(0.15f),
       ssgi_thickness_(1.2f),   // Match GUI default for better hit detection
       ssgi_num_samples_(8),
       hiz_textures_{0, 0},
       final_hiz_texture_(0),
       hiz_mip_levels_(0),
       prev_view_matrix_(1.0f),
       prev_projection_matrix_(1.0f),
       first_frame_(true)
    {
    }

    Renderer::~Renderer() {
        cleanup_framebuffer();
        cleanup_g_buffer();
        cleanup_screen_quad();
        cleanup_skybox();
        cleanup_ssao();
        cleanup_ssgi();
        cleanup_hiz_buffer();
    }

    void Renderer::initialize() {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            throw std::runtime_error("Failed to initialize GLAD");
        }

        glViewport(0, 0, viewport_width_, viewport_height_);
        glEnable(GL_DEPTH_TEST);
        
        glDisable(GL_CULL_FACE);

        // All texture slot management is now handled automatically by the Texture class

        
        shadow_map = std::make_unique<ShadowMap>();
        if (shadow_map->initialize(2048, 2048)) {
            LOG_INFO("ShadowMap test passed!");
        } else {
            LOG_ERROR("ShadowMap test failed!");
        }
        
        // GUI initialization moved to Application   

        setup_framebuffer();
        setup_g_buffer();
        // setup_screen_quad(); // Moved to lazy initialization in render methods
        setup_skybox();
        setup_ssao();
        setup_ssgi();
        setup_hiz_buffer();

    }
  
        void Renderer::setup_framebuffer() {
        
        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

        // Create color texture using factory method
        color_texture_ = std::make_unique<Texture>(Texture::create_render_target(viewport_width_, viewport_height_, false));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture_->get_id(), 0);
        
        // Create depth texture using factory method
        depth_texture_ = std::make_unique<Texture>(Texture::create_depth_buffer(viewport_width_, viewport_height_));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_->get_id(), 0);

        //restore to default
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        LOG_INFO("Framebuffer setup completed: {}x{}", viewport_width_, viewport_height_);
    }

    void Renderer::resize_framebuffer(int newWidth, int newHeight) {
        if (newWidth <= 0 || newHeight <= 0) {
            return;
        }
        
        viewport_width_ = newWidth;
        viewport_height_ = newHeight;
        
        // Also update the stored width and height for consistency
        width_ = newWidth;
        height_ = newHeight;
        
        // Resize main framebuffer textures using new resize method
        if (color_texture_) {
            color_texture_->resize_texture(viewport_width_, viewport_height_, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
        }
        
        if (depth_texture_) {
            depth_texture_->resize_texture(viewport_width_, viewport_height_, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_->get_id(), 0);
        }

        // Resize G-Buffer textures using new resize method
        if (g_position_texture_) {
            g_position_texture_->resize_texture(viewport_width_, viewport_height_, GL_RGBA32F, GL_RGBA, GL_FLOAT);
            g_albedo_metallic_texture_->resize_texture(viewport_width_, viewport_height_, GL_RGBA16F, GL_RGBA, GL_FLOAT);
            g_normal_roughness_texture_->resize_texture(viewport_width_, viewport_height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
            g_motion_ao_texture_->resize_texture(viewport_width_, viewport_height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
            g_emissive_texture_->resize_texture(viewport_width_, viewport_height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
            g_depth_texture_->resize_texture(viewport_width_, viewport_height_, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT);
        }

        // Resize SSAO textures if they exist
        if (ssao_raw_texture_ || ssao_final_texture_) {
            cleanup_ssao_textures();
            setup_ssao_textures();
        }
        
        // Resize SSGI textures if they exist
        if (ssgi_raw_texture_ || ssgi_final_texture_ || lit_scene_texture_) {
            cleanup_ssgi_textures();
            setup_ssgi_textures();
        }
        
        // Resize Hi-Z buffer if it exists
        if (hiz_textures_[0] != 0) {
            cleanup_hiz_buffer();
            setup_hiz_buffer();
        }

        LOG_INFO("Framebuffer, G-Buffer, SSGI textures, and Hi-Z buffer resized to: {}x{}", viewport_width_, viewport_height_);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void Renderer::cleanup_framebuffer() {
        // Texture objects will be automatically cleaned up by their destructors
        color_texture_.reset();
        depth_texture_.reset();
        
        if (framebuffer_ != 0) {
            glDeleteFramebuffers(1, &framebuffer_);
            framebuffer_ = 0;
        }
    }
    
    void Renderer::setup_g_buffer() {
        // Generate G-Buffer framebuffer
        glGenFramebuffers(1, &g_buffer_fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_fbo_);
        
        // RT0: Position + Material ID using factory method
        g_position_texture_ = std::make_unique<Texture>(Texture::create_g_buffer_texture(viewport_width_, viewport_height_, GL_RGBA32F, GL_RGBA, GL_FLOAT));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_position_texture_->get_id(), 0);
        
        // RT1: Albedo (RGB16F) + Metallic (R16F) using factory method
        g_albedo_metallic_texture_ = std::make_unique<Texture>(Texture::create_g_buffer_texture(viewport_width_, viewport_height_, GL_RGBA16F, GL_RGBA, GL_FLOAT));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g_albedo_metallic_texture_->get_id(), 0);
        
        // RT2: Normal (RGB8) + Roughness (R8) using factory method
        g_normal_roughness_texture_ = std::make_unique<Texture>(Texture::create_g_buffer_texture(viewport_width_, viewport_height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g_normal_roughness_texture_->get_id(), 0);
        
        // RT3: Motion Vector (RG8) + AO (R8) + unused (R8) using factory method
        g_motion_ao_texture_ = std::make_unique<Texture>(Texture::create_g_buffer_texture(viewport_width_, viewport_height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, g_motion_ao_texture_->get_id(), 0);
        
        // RT4: Emissive Color (RGB8) + Intensity (R8) using factory method
        g_emissive_texture_ = std::make_unique<Texture>(Texture::create_g_buffer_texture(viewport_width_, viewport_height_, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE));
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, g_emissive_texture_->get_id(), 0);
        
        // Depth buffer using factory method
        g_depth_texture_ = std::make_unique<Texture>(Texture::create_depth_buffer(viewport_width_, viewport_height_));
        g_depth_texture_->set_filter_mode(GL_LINEAR, GL_LINEAR); // Override default nearest filtering for depth
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_depth_texture_->get_id(), 0);
        
        // Specify which color attachments we'll use for rendering
        GLenum draw_buffers[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
        glDrawBuffers(5, draw_buffers);
        
        // Check OpenGL errors after MRT setup
        GLenum gl_error = glGetError();
        if (gl_error != GL_NO_ERROR) {
            LOG_ERROR("OpenGL error after MRT setup: {}", gl_error);
        }
        
        // Check framebuffer completeness
        GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("G-Buffer framebuffer not complete! Status: {}", framebuffer_status);
        } else {
            LOG_INFO("G-Buffer setup completed: {}x{} with 4 render targets", viewport_width_, viewport_height_);
        }
        
        // Unbind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void Renderer::cleanup_g_buffer() {
        // Texture objects will be automatically cleaned up by their destructors
        g_position_texture_.reset();
        g_albedo_metallic_texture_.reset();
        g_normal_roughness_texture_.reset();
        g_motion_ao_texture_.reset();
        g_emissive_texture_.reset();
        g_depth_texture_.reset();
        
        if (g_buffer_fbo_ != 0) {
            glDeleteFramebuffers(1, &g_buffer_fbo_);
            g_buffer_fbo_ = 0;
        }
    }
    
    void Renderer::set_deferred_rendering(bool enable) {
        use_deferred_rendering_ = enable;
        LOG_INFO("Deferred rendering {}", enable ? "enabled" : "disabled");
    }
    
    void Renderer::setup_screen_quad(const CoroutineResourceManager& resource_manager) {
        // Get or create screen quad mesh from ResourceManager
        screen_quad_mesh_ = const_cast<CoroutineResourceManager&>(resource_manager).createQuad("screen_quad");
        
        if (!screen_quad_mesh_) {
            LOG_ERROR("Renderer: Failed to create screen quad mesh");
            return;
        }
        
        LOG_DEBUG("Renderer: Screen-space quad setup completed using ResourceManager");
    }
    
    void Renderer::cleanup_screen_quad() {
        // Reset the shared_ptr, mesh cleanup is handled by ResourceManager cache
        screen_quad_mesh_.reset();
        LOG_DEBUG("Renderer: Screen quad mesh reference cleared");
    }
    
    void Renderer::render_screen_quad() {
        if (!screen_quad_mesh_) {
            LOG_ERROR("Renderer: Screen quad mesh not initialized. Call setup_screen_quad() first.");
            return;
        }
        
        screen_quad_mesh_->draw();
    }
    
    void Renderer::set_render_to_framebuffer(bool enable) {
        use_framebuffer_ = enable;
        if (enable) {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glViewport(0, 0, viewport_width_, viewport_height_);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, viewport_width_, viewport_height_);
        }
    }
    
    void Renderer::bind_g_buffer_for_geometry_pass() {
      // Reset texture slot counter for geometry pass
      Texture::reset_slot_counter();
      
      glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_fbo_);
      glViewport(0, 0, viewport_width_, viewport_height_);

      // Re-specify draw buffers
      GLenum draw_buffers[5] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4};
      glDrawBuffers(5, draw_buffers);

      // Clear G-Buffer
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClearDepth(1.0f);  // Ensure depth is cleared to far plane
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // Enable depth testing and depth writing, disable face culling
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
      glDepthMask(GL_TRUE);  // Ensure depth writing is enabled
      glDisable(GL_CULL_FACE);
      
      // Disable blending for opaque geometry rendering
      glDisable(GL_BLEND);
    }

    void Renderer::bind_g_buffer_for_lighting_pass() {
      // Bind default framebuffer for final output
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
      glViewport(0, 0, viewport_width_, viewport_height_);

      // Disable depth testing for screen-space quad and ensure face culling is off
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
      
      // Enable blending 
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);

    }

    void Renderer::render_deferred(const Scene& scene, const Camera& camera, 
        const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager) {
        // Initialize screen quad if not already done
        //shadow_map = nullptr;
        if (!screen_quad_mesh_) {
            setup_screen_quad(resource_manager);
        }
        
        // Check if scene is empty
        if (scene.is_empty()) {
            LOG_ERROR("Renderer: Scene is empty, skipping deferred rendering");
            return;
        }
        
        // Unbind all textures and reset slot counter for this render pass
        
        Texture::reset_slot_counter();
        
        // Bind G-Buffer textures for reading using automatic slot management
        unsigned int g_pos_slot = Texture::bind_raw_texture(g_position_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int g_albedo_slot = Texture::bind_raw_texture(g_albedo_metallic_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int g_normal_slot = Texture::bind_raw_texture(g_normal_roughness_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int g_motion_slot = Texture::bind_raw_texture(g_motion_ao_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int g_emissive_slot = Texture::bind_raw_texture(g_emissive_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int g_depth_slot = Texture::bind_raw_texture(g_depth_texture_->get_id(), GL_TEXTURE_2D);
        
        // Shadow Pass 
        if (shadow_map) {
            //LOG_INFO("Renderer: Rendering shadow pass for deferred rendering");
            render_shadow_pass_deferred(scene, camera, resource_manager, transform_manager);
        }
        
        // Geometry Pass
        bind_g_buffer_for_geometry_pass();
        
        // Get geometry shader from ResourceManager
        auto geometry_shader = resource_manager.get_shader("deferred_geometry_shader");
        if (!geometry_shader) {
            LOG_ERROR("Renderer: Deferred geometry shader not found in ResourceManager");
            return;
        }
        
        //LOG_INFO("Renderer: Using deferred geometry shader for geometry pass");
        geometry_shader->use();
        
        // Set camera matrices
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
        glm::vec3 camera_pos = camera.get_position();
        
        //LOG_INFO("Renderer: Camera position: ({}, {}, {})", camera_pos.x, camera_pos.y, camera_pos.z);
        
        geometry_shader->set_mat4("view", view);
        geometry_shader->set_mat4("projection", projection);

        // Set previous frame MVP for motion vectors and temporal accumulation
        glm::mat4 prevMVP;
        if (first_frame_) {
            // On first frame, use current matrices to avoid artifacts
            prevMVP = projection * view;
            first_frame_ = false;
        } else {
            // Use previous frame matrices
            prevMVP = prev_projection_matrix_ * prev_view_matrix_;
        }
        geometry_shader->set_mat4("prevModelViewProjection", prevMVP);
        
        // Store current matrices for next frame
        prev_view_matrix_ = view;
        prev_projection_matrix_ = projection;
        
        // Render all renderables to G-Buffer
        const auto& renderable_refs = scene.get_renderable_references();
        
        for (const auto& renderable_id : renderable_refs) {
            auto renderable = resource_manager.get<Renderable>(renderable_id);
            if (!renderable || !renderable->is_visible() || !renderable->has_models()) {
                continue;
            }
            
            // Get transform from external transform system
            glm::mat4 renderable_matrix = transform_manager.get_model_matrix(renderable_id);
            
            // Render each model in the renderable
            for (const auto& model_id : renderable->get_model_ids()) {
                Texture::reset_slot_counter();
                auto model = resource_manager.get<Model>(model_id);
                if (!model || !model->has_mesh() || !model->has_material()) {
                    continue;
                }
                
                geometry_shader->set_mat4("model", renderable_matrix);
            
                // Set material properties
                const Material& material = *model->get_material();
                
                // Set basic material uniforms
                material.set_shader(*geometry_shader, "material");
                
                // Set PBR material parameters
                material.set_shader_pbr(*geometry_shader);
                geometry_shader->set_int("materialID", 0);
                
                // Bind material textures using automatic slot management
                material.bind_textures_auto(*geometry_shader, resource_manager);
                
                // Render the mesh
                try {
                    const Mesh& mesh = *model->get_mesh();
                    mesh.draw();
                } catch (const std::exception& e) {
                    LOG_ERROR("Renderer: Failed to render model '{}' in geometry pass: {}", model_id, e.what());
                    continue;
                }
            }
        }
        
        // Ensure G-Buffer writes are complete before generating Hi-Z pyramid
        glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
        
        // Generate Hi-Z pyramid after geometry pass for accelerated ray marching
        generate_hiz_pyramid(resource_manager);
        
        // Render skybox using G-Buffer depth information
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, viewport_width_, viewport_height_);
        
        // Clear only color buffer, keep depth from G-Buffer
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Copy depth from G-Buffer to final framebuffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_buffer_fbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_);
        glBlitFramebuffer(0, 0, viewport_width_, viewport_height_, 0, 0, viewport_width_, viewport_height_, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        
        // Render skybox with proper depth testing
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        render_skybox(camera, resource_manager);

        // SSAO pass (if enabled) - runs after G-Buffer generation
        if (use_ssao_) {
            SSAO_render(scene, camera, resource_manager);
        }

        if (use_ssgi_) {
            // SSGI-enabled pipeline: Direct lighting -> SSGI -> Skybox -> Composition
            // LOG_INFO("Renderer: Starting SSGI pipeline - Direct lighting pass");
            render_direct_lighting_pass(scene, camera, resource_manager);
            
            // LOG_INFO("Renderer: SSGI compute pass");
            SSGI_render(scene, camera, resource_manager);
            
            // Render skybox to main framebuffer before composition
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            //render_skybox(camera, resource_manager);
           
            // LOG_INFO("Renderer: SSGI composition pass");
            render_composition_pass(scene, camera, resource_manager);

        } else {
            // Traditional deferred lighting
            bind_g_buffer_for_lighting_pass();
            
            // Get lighting shader from ResourceManager
            auto lighting_shader = resource_manager.get_shader("deferred_lighting_shader");
            if (!lighting_shader) {
                LOG_ERROR("Renderer: Deferred lighting shader not found in ResourceManager");
                return;
            }
        
            lighting_shader->use();
        
            // Bind G-Buffer textures
            // Set G-Buffer texture uniforms using dynamically assigned slots
            if (g_pos_slot != Texture::INVALID_SLOT) lighting_shader->set_int("gPosition", g_pos_slot);
            if (g_albedo_slot != Texture::INVALID_SLOT) lighting_shader->set_int("gAlbedoMetallic", g_albedo_slot);
            if (g_normal_slot != Texture::INVALID_SLOT) lighting_shader->set_int("gNormalRoughness", g_normal_slot);
            if (g_motion_slot != Texture::INVALID_SLOT) lighting_shader->set_int("gMotionAO", g_motion_slot);
            if (g_emissive_slot != Texture::INVALID_SLOT) lighting_shader->set_int("gEmissive", g_emissive_slot);
            if (g_depth_slot != Texture::INVALID_SLOT) lighting_shader->set_int("gDepth", g_depth_slot);
            

        
            // Set camera uniforms
            lighting_shader->set_vec3("viewPos", camera_pos);
            lighting_shader->set_mat4("view", view);
            lighting_shader->set_mat4("projection", projection);
        
            // Set ambient lighting from scene
            lighting_shader->set_vec3("ambientLight", scene.get_ambient_light());
        
            // Set up lighting using scene lights
            auto scene_lights = resource_manager.get_scene_lights(scene);
            size_t light_size = std::min(scene_lights.size(), size_t(8)); // Limit to 8 lights
            lighting_shader->set_int("numLights", static_cast<int>(light_size));
        

        
            for (size_t i = 0; i < light_size; ++i) {
                auto light = scene_lights[i];
            
                if (light) {
                    // Use the new Light::set_shader_array method to set all light parameters
                    light->set_shader_array(*lighting_shader, static_cast<int>(i));
                } else {
                    LOG_WARN("Renderer: Light {} is null", i);
                }
            }
        
            // IBL irradiance and prefiltered mapping
            auto irradiance_map = resource_manager.get_irradiance_map("skybox_cubemap");
            auto prefiltered_map = resource_manager.get_prefiltered_map("skybox_cubemap");
        
            if (irradiance_map && prefiltered_map) {
                lighting_shader->set_bool("useIBL", true);
            
                // Bind irradiance map using automatic slot management
                unsigned int irradiance_slot = irradiance_map->bind_cubemap_auto();
                if (irradiance_slot != Texture::INVALID_SLOT) {
                    lighting_shader->set_int("irradianceMap", irradiance_slot);
                }
                
                // Bind prefiltered environment map using automatic slot management
                unsigned int prefiltered_slot = prefiltered_map->bind_cubemap_auto();
                if (prefiltered_slot != Texture::INVALID_SLOT) {
                    lighting_shader->set_int("prefilteredMap", prefiltered_slot);
                }
                
                LOG_INFO("Renderer: IBL maps bound - irradiance: slot {}, prefiltered: slot {}", irradiance_slot, prefiltered_slot);
            } else {
                lighting_shader->set_bool("useIBL", false);
                LOG_WARN("Renderer: IBL maps not available (irradiance: {}, prefiltered: {}), using fallback ambient lighting", 
                        irradiance_map ? "OK" : "missing", prefiltered_map ? "OK" : "missing");
            }
        
            // Shadow mapping (if enabled)
            if (shadow_map) {
                lighting_shader->set_bool("enableShadows", true);
            
                // Bind shadow map texture using automatic slot management
                GLuint shadow_texture_id = shadow_map->get_depth_texture();
                unsigned int shadow_slot = Texture::bind_raw_texture(shadow_texture_id, GL_TEXTURE_2D);
                if (shadow_slot != Texture::INVALID_SLOT) {
                    lighting_shader->set_int("shadowMap", shadow_slot);
                }
            

            
                // Use first light as shadow caster if available, otherwise use fixed position
                glm::vec3 shadow_light_direction = glm::normalize(shadow_light_pos_);
                if (light_size > 0 && scene_lights[0] && scene_lights[0]->get_type() == Light::Type::kDirectional) {
                    shadow_light_direction = scene_lights[0]->get_direction();
                }
            
                // For directional light shadows, center the shadow map around the scene center
                glm::vec3 shadow_center = glm::vec3(0.0f, 0.0f, 0.0f); // Use scene center as shadow map center
            
                // Set light space matrix for shadow mapping (use the same matrix from shadow pass)
                lighting_shader->set_mat4("lightSpaceMatrix", last_light_space_matrix_);
            } else {
                lighting_shader->set_bool("enableShadows", false);
            }
        
            // Render screen-space quad
            render_screen_quad();
            
            // Apply SSAO in a post-processing pass if enabled
            if (use_ssao_) {
                apply_ssao_to_framebuffer(scene, camera, resource_manager);
            }
            
            // Re-enable depth testing and disable blending for subsequent rendering
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
        }

            // Temporal function
            //render_plane_reflection(scene, camera, resource_manager, transform_manager);
        
            // Render light spheres for visualization
            //render_light_spheres(scene, camera, resource_manager);
    }
    
    void Renderer::render_gbuffer_debug(int debug_mode, const CoroutineResourceManager& resource_manager) {
        // Initialize screen quad if not already done
        if (!screen_quad_mesh_) {
            setup_screen_quad(resource_manager);
        }
        
        // Bind final framebuffer for output
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, viewport_width_, viewport_height_);
        
        // Clear framebuffer
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Disable depth testing
        glDisable(GL_DEPTH_TEST);
        
        // Get debug shader
        auto debug_shader = resource_manager.get_shader("gbuffer_debug_shader");
        if (!debug_shader) {
            LOG_ERROR("Renderer: G-Buffer debug shader not found in ResourceManager");
            return;
        }
        
        debug_shader->use();
        
        // Bind G-Buffer textures using automatic slot management
        unsigned int pos_slot = Texture::bind_raw_texture(g_position_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int albedo_slot = Texture::bind_raw_texture(g_albedo_metallic_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int normal_slot = Texture::bind_raw_texture(g_normal_roughness_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int motion_slot = Texture::bind_raw_texture(g_motion_ao_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int emissive_slot = Texture::bind_raw_texture(g_emissive_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int depth_slot = Texture::bind_raw_texture(g_depth_texture_->get_id(), GL_TEXTURE_2D);
        
        if (pos_slot != Texture::INVALID_SLOT) debug_shader->set_int("gPosition", pos_slot);
        if (albedo_slot != Texture::INVALID_SLOT) debug_shader->set_int("gAlbedoMetallic", albedo_slot);
        if (normal_slot != Texture::INVALID_SLOT) debug_shader->set_int("gNormalRoughness", normal_slot);
        if (motion_slot != Texture::INVALID_SLOT) debug_shader->set_int("gMotionAO", motion_slot);
        if (emissive_slot != Texture::INVALID_SLOT) debug_shader->set_int("gEmissive", emissive_slot);
        if (depth_slot != Texture::INVALID_SLOT) debug_shader->set_int("gDepth", depth_slot);
        
        // Set debug mode
        debug_shader->set_int("debugMode", debug_mode);
        
        // Render screen-space quad
        render_screen_quad();
        
        // Re-enable depth testing
        glEnable(GL_DEPTH_TEST);
    }
    
    
    void Renderer::render(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager) {
        // Check if scene is empty
        if (scene.is_empty()) {
            LOG_ERROR("Renderer: Scene is empty, skipping rendering");
            return;
        }
        
        // Clear buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Enable depth test and disable face culling
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        // Update camera matrices
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
        glm::vec3 camera_pos = camera.get_position();
        
        // Get main shader from ResourceManager
        auto main_shader = resource_manager.get_shader("simple_scene_main_shader");
        if (!main_shader) {
            LOG_ERROR("Renderer: Main shader not found in ResourceManager");
            return;
        }
        
        main_shader->use();
        
        // Set camera matrices
        main_shader->set_mat4("view", view);
        main_shader->set_mat4("projection", projection);
        main_shader->set_vec3("viewPos", camera_pos);
        
        // Set ambient lighting from scene
        const glm::vec3 ambient_light = scene.get_ambient_light();
        main_shader->set_vec3("ambientLight", glm::vec3(ambient_light[0], ambient_light[1], ambient_light[2]));
        
        // Set up lighting using scene lights
        auto scene_lights = resource_manager.get_scene_lights(scene);
        main_shader->set_int("numLights", static_cast<int>(scene_lights.size()));
        
        for (size_t i = 0; i < scene_lights.size() && i < 8; ++i) {  // Limit to 8 lights
            auto light = scene_lights[i];
            if (light) {
                light->set_shader(*main_shader); 
            }
        }
        
        // Render all renderables in the scene
        const auto& renderable_refs = scene.get_renderable_references();
        
        for (const auto& renderable_id : renderable_refs) {
            auto renderable = resource_manager.get<Renderable>(renderable_id);
            if (!renderable || !renderable->is_visible() || !renderable->has_models()) {
                continue;
            }
            
            // Get transform from external transform system
            glm::mat4 renderable_matrix = transform_manager.get_model_matrix(renderable_id);
            
            // Render each model in the renderable
            for (const auto& model_id : renderable->get_model_ids()) {
            // Get model from ResourceManager
            auto model = resource_manager.get<Model>(model_id);
            if (!model) {
                LOG_WARN("Renderer: Model '{}' not found in ResourceManager", model_id);
                continue;
            }
            
            // Validate model has required components
            if (!model->has_mesh()) {
                LOG_WARN("Renderer: Model '{}' has no mesh, skipping", model_id);
                continue;
            }
            
            if (!model->has_material()) {
                LOG_WARN("Renderer: Model '{}' has no material, skipping", model_id);
                continue;
            }
            
            // Check if this is the plane model and use reflection shader
            if (model_id == "simple_scene_plane_model") {
                auto plane_shader = resource_manager.get_shader("plane_reflection_shader");
                if (plane_shader) {
                    plane_shader->use();
                    
                    // Set camera matrices for plane shader
                    plane_shader->set_mat4("view", view);
                    plane_shader->set_mat4("projection", projection);
                    plane_shader->set_vec3("viewPos", camera_pos);
                    
                    // Set lighting
                    plane_shader->set_vec3("ambientLight", glm::vec3(ambient_light[0], ambient_light[1], ambient_light[2]));
                    
                    // Set up lighting using scene lights
                    auto scene_lights = resource_manager.get_scene_lights(scene);
                    plane_shader->set_int("numLights", static_cast<int>(scene_lights.size()));
                    
                    for (size_t i = 0; i < scene_lights.size() && i < 8; ++i) {
                        auto light = scene_lights[i];
                        if (light) {
                            light->set_shader(*plane_shader); 
                        }
                    }
                    
                    // Bind skybox texture for reflection using automatic slot management
                    auto skybox_texture = resource_manager.get<Texture>("skybox_cubemap");
                    if (skybox_texture) {
                        unsigned int slot = skybox_texture->bind_cubemap_auto();
                        if (slot != Texture::INVALID_SLOT) {
                            plane_shader->set_int("skybox", slot);
                        }
                    }
                    
                    // Set reflection strength (can be adjusted)
                    plane_shader->set_float("reflectionStrength", 0.4f);
                    
                    // Get transform and render
                    glm::mat4 model_matrix = transform_manager.get_model_matrix(model_id);
                    plane_shader->set_mat4("model", model_matrix);
                    
                    // Set material properties
                    const Material& material = *model->get_material();
                    material.set_shader(*plane_shader, "material");
                    
                    // Bind material textures using automatic slot management
                    material.bind_textures_auto(*plane_shader, resource_manager);
                    
                    // Render the plane mesh
                    try {
                        const Mesh& mesh = *model->get_mesh();
                        mesh.draw();
                    } catch (const std::exception& e) {
                        LOG_ERROR("Renderer: Failed to render plane model '{}': {}", model_id, e.what());
                        continue;
                    }
                    
                    // Switch back to main shader for other objects
                    main_shader->use();
                } else {
                    LOG_WARN("Renderer: Plane reflection shader not found, using default shader");
                }
            } else {
                // Use default shader for non-plane objects
                // Get transform from external transform system
                glm::mat4 model_matrix = transform_manager.get_model_matrix(model_id);
                main_shader->set_mat4("model", model_matrix);
                
                // Set material properties
                const Material& material = *model->get_material();
                material.set_shader(*main_shader, "material");
                
                // Bind material textures using automatic slot management
                material.bind_textures_auto(*main_shader, resource_manager);
                
                // Render the model's mesh
                try {
                    const Mesh& mesh = *model->get_mesh();
                    mesh.draw();
                } catch (const std::exception& e) {
                    LOG_ERROR("Renderer: Failed to render model '{}': {}", model_id, e.what());
                    continue;
                }
            }
        }
         }
         
         // Render skybox as background
         render_skybox(camera, resource_manager);
     }
     
    bool Renderer::validate_scene_resources(const Scene& scene, CoroutineResourceManager& resource_manager) const {
        LOG_DEBUG("Renderer: Validating scene resources");
        
        bool all_valid = true;
        
        // Validate renderables
        const auto& renderable_refs = scene.get_renderable_references();
        LOG_DEBUG("Renderer: Validating {} renderable references", renderable_refs.size());
        
        for (const auto& renderable_id : renderable_refs) {
            auto renderable = resource_manager.get<Renderable>(renderable_id);
            if (!renderable) {
                LOG_ERROR("Renderer: Renderable '{}' not found in ResourceManager", renderable_id);
                all_valid = false;
                continue;
            }
            
            if (!renderable->has_models()) {
                LOG_ERROR("Renderer: Renderable '{}' has no models", renderable_id);
                all_valid = false;
            }
            
            // Validate each model in the renderable
            for (const auto& model_id : renderable->get_model_ids()) {
                auto model = resource_manager.get<Model>(model_id);
                if (!model) {
                    LOG_ERROR("Renderer: Model '{}' in renderable '{}' not found", model_id, renderable_id);
                    all_valid = false;
                    continue;
                }
                
                if (!model->has_mesh()) {
                    LOG_ERROR("Renderer: Model '{}' in renderable '{}' has no mesh", model_id, renderable_id);
                    all_valid = false;
                }
                
                if (!model->has_material()) {
                    LOG_ERROR("Renderer: Model '{}' in renderable '{}' has no material", model_id, renderable_id);
                    all_valid = false;
                }
            }
            
            LOG_DEBUG("Renderer: Renderable '{}' validation passed", renderable_id);
        }
        
        // Validate lights
        const auto& light_refs = scene.get_light_references();
        LOG_DEBUG("Renderer: Validating {} light references", light_refs.size());
        
        for (const auto& light_id : light_refs) {
            auto light = resource_manager.get<Light>(light_id);
            if (!light) {
                LOG_ERROR("Renderer: Light '{}' not found in ResourceManager", light_id);
                all_valid = false;
                continue;
            }
            
            LOG_DEBUG("Renderer: Light '{}' validation passed", light_id);
        }
        
        // Validate shaders
        auto main_shader = resource_manager.get_shader("simple_scene_main_shader");
        if (!main_shader) {
            LOG_ERROR("Renderer: Main shader 'simple_scene_main_shader' not found");
            all_valid = false;
        } else {
            LOG_DEBUG("Renderer: Main shader validation passed");
        }
        
        if (all_valid) {
            LOG_INFO("Renderer: Scene validation passed - all resources are available");
        } else {
            LOG_WARN("Renderer: Scene validation failed - some resources are missing");
        }
        
        return all_valid;
    }
    
    void Renderer::render_light_spheres(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager) {
        // Get light shader from ResourceManager
        auto light_shader = resource_manager.get_shader("simple_scene_light_shader");
        if (!light_shader) {
            LOG_WARN("Renderer: Light shader not found, skipping light visualization");
            return;
        }
        
        // Enable depth testing for light spheres
        glEnable(GL_DEPTH_TEST);
        
        light_shader->use();
        
        // Set camera matrices
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
        
        light_shader->set_mat4("view", view);
        light_shader->set_mat4("projection", projection);
        
        // Render each light as a small sphere
        const auto& light_refs = scene.get_light_references();
        for (const auto& light_id : light_refs) {
            auto light = resource_manager.get<Light>(light_id);
            if (!light) {
                continue;
            }
            
            // Skip directional lights 
            if (light->get_type() == Light::Type::kDirectional) {
                continue;
            }
            
            // Create model matrix for light position
            glm::mat4 lightModel = glm::mat4(1.0f);
            lightModel = glm::translate(lightModel, light->get_position());
            lightModel = glm::scale(lightModel, glm::vec3(0.1f)); // Small sphere
            
            light_shader->set_mat4("model", lightModel);
            light_shader->set_vec3("lightColor", light->get_color());
            
            // Render the light using its built-in render method
            light->render();
        }
    }
    
    void Renderer::setup_skybox() {
        // Skybox cube vertices (positions only, no normals or texture coords needed)
        float skybox_vertices[] = {
            // positions          
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        glGenVertexArrays(1, &skybox_vao_);
        glGenBuffers(1, &skybox_vbo_);
        
        glBindVertexArray(skybox_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skybox_vertices), skybox_vertices, GL_STATIC_DRAW);
        
        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        
        glBindVertexArray(0);
        
        LOG_INFO("Skybox setup completed");
    }
    

    
    void Renderer::cleanup_skybox() {
        if (skybox_vbo_ != 0) {
            glDeleteBuffers(1, &skybox_vbo_);
            skybox_vbo_ = 0;
        }
        if (skybox_vao_ != 0) {
            glDeleteVertexArrays(1, &skybox_vao_);
            skybox_vao_ = 0;
        }
    }
    


    void Renderer::render_skybox(const Camera& camera, const CoroutineResourceManager& resource_manager) {
        // Get skybox shader from ResourceManager
        auto skybox_shader = resource_manager.get_shader("skybox_shader");
        if (!skybox_shader) {
            LOG_WARN("Renderer: Skybox shader not found, skipping skybox rendering");
            return;
        }
        
        // Disable depth writing but keep depth testing
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        
        skybox_shader->use();
        
        // Remove translation from view matrix 
        glm::mat4 view = glm::mat4(glm::mat3(camera.get_view_matrix()));
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
        
        skybox_shader->set_mat4("view", view);
        skybox_shader->set_mat4("projection", projection);
        
        // Get and bind skybox cubemap texture using automatic slot management
        auto skybox_texture = resource_manager.get<Texture>("skybox_cubemap");
        if (skybox_texture) {
            unsigned int slot = skybox_texture->bind_cubemap_auto();
            if (slot != Texture::INVALID_SLOT) {
                skybox_shader->set_int("skybox", slot);
            } else {
                LOG_WARN("Renderer: Failed to bind skybox texture");
                return;
            }
        } else {
            LOG_WARN("Renderer: Skybox texture not found");
            return;
        }
        
        // Render skybox cube
        glBindVertexArray(skybox_vao_);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        
        // Restore depth settings
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
    }
    
    void Renderer::render_shadow_pass_deferred(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager) {
        if (!shadow_map || !shadow_map->get_shadow_shader()) {
            LOG_ERROR("ShadowMap or shadow shader is null!");
            return;
        }

        shadow_map->begin_shadow_pass();
        shadow_map->get_shadow_shader()->use();

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        glm::vec3 shadow_light_direction = glm::normalize(shadow_light_pos_);
        auto scene_lights = resource_manager.get_scene_lights(scene);
        if (!scene_lights.empty() && scene_lights[0] && scene_lights[0]->get_type() == Light::Type::kDirectional) {
            shadow_light_direction = scene_lights[0]->get_direction();
        }

        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
        glm::mat4 invViewProjection = glm::inverse(projection * view);

        std::vector<glm::vec3> frustum_corners_world;
        glm::vec3 frustum_center_world(0.0f);
        const std::vector<glm::vec4> frustum_corners_ndc = {
            glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(1.0f, -1.0f, -1.0f, 1.0f),
            glm::vec4(-1.0f,  1.0f, -1.0f, 1.0f), glm::vec4(1.0f,  1.0f, -1.0f, 1.0f),
            glm::vec4(-1.0f, -1.0f,  1.0f, 1.0f), glm::vec4(1.0f, -1.0f,  1.0f, 1.0f),
            glm::vec4(-1.0f,  1.0f,  1.0f, 1.0f), glm::vec4(1.0f,  1.0f,  1.0f, 1.0f)
        };
        for (const auto& corner_ndc : frustum_corners_ndc) {
            glm::vec4 corner_world = invViewProjection * corner_ndc;
            corner_world /= corner_world.w;
            frustum_corners_world.push_back(glm::vec3(corner_world));
            frustum_center_world += glm::vec3(corner_world);
        }
        frustum_center_world /= frustum_corners_world.size();

        
        glm::mat4 lightViewMatrix = glm::lookAt(
            frustum_center_world - shadow_light_direction * 50.0f, 
            frustum_center_world,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

       
        glm::vec3 min_bounds = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 max_bounds = glm::vec3(std::numeric_limits<float>::lowest());
        for (const auto& corner_world : frustum_corners_world) {
            glm::vec4 corner_light_space = lightViewMatrix * glm::vec4(corner_world, 1.0f);
            min_bounds = glm::min(min_bounds, glm::vec3(corner_light_space));
            max_bounds = glm::max(max_bounds, glm::vec3(corner_light_space));
        }

        
        float shadow_map_width = static_cast<float>(shadow_map->get_width());
        float shadow_map_height = static_cast<float>(shadow_map->get_height());
        glm::vec2 frustum_size(max_bounds.x - min_bounds.x, max_bounds.y - min_bounds.y);
        glm::vec2 texel_size(frustum_size.x / shadow_map_width, frustum_size.y / shadow_map_height);

        min_bounds.x = std::floor(min_bounds.x / texel_size.x) * texel_size.x;
        min_bounds.y = std::floor(min_bounds.y / texel_size.y) * texel_size.y;
        max_bounds.x = min_bounds.x + frustum_size.x;
        max_bounds.y = min_bounds.y + frustum_size.y;

 
        float z_padding = 100.0f;
        min_bounds.z -= z_padding;
        max_bounds.z += z_padding;

        glm::mat4 lightProjection = glm::ortho(
            min_bounds.x, max_bounds.x,
            min_bounds.y, max_bounds.y,
            min_bounds.z, max_bounds.z
        );

       
        glm::mat4 lightSpaceMatrix = lightProjection * lightViewMatrix;
        last_light_space_matrix_ = lightSpaceMatrix;
        shadow_map->get_shadow_shader()->set_mat4("lightSpaceMatrix", lightSpaceMatrix);

 
        const auto& renderable_refs = scene.get_renderable_references();
        for (const auto& renderable_id : renderable_refs) {
            auto renderable = resource_manager.get<Renderable>(renderable_id);
            if (!renderable || !renderable->is_visible() || !renderable->has_models()) { continue; }

            glm::mat4 renderable_matrix = transform_manager.get_model_matrix(renderable_id);
            for (const auto& model_id : renderable->get_model_ids()) {
                auto model = resource_manager.get<Model>(model_id);
                if (!model || !model->has_mesh()) { continue; }

                shadow_map->get_shadow_shader()->set_mat4("model", renderable_matrix);
                try {
                    model->get_mesh()->draw();
                }
                catch (const std::exception& e) {
                    LOG_ERROR("Renderer: Failed to render model '{}' in shadow pass: {}", model_id, e.what());
                    continue;
                }
            }
        }

        glCullFace(GL_BACK);
        glDisable(GL_CULL_FACE);

        shadow_map->end_shadow_pass();
    }

    void Renderer::render_plane_reflection(const Scene& scene, const Camera& camera, 
        const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager) {
        // Find the plane renderable in the scene
        const auto& renderable_refs = scene.get_renderable_references();
        for (const auto& renderable_id : renderable_refs) {
            // Check if this is the plane renderable
            if (renderable_id != "simple_scene_plane_renderable") {
                continue; // Skip non-plane renderables
            }
            
            auto renderable = resource_manager.get<Renderable>(renderable_id);
            if (!renderable || !renderable->is_visible() || !renderable->has_models()) {
                continue;
            }
            
            // Get the first model from the plane renderable
            const auto& model_ids = renderable->get_model_ids();
            if (model_ids.empty()) {
                continue;
            }
            
            auto model = resource_manager.get<Model>(model_ids[0]);
            if (!model || !model->has_mesh() || !model->has_material()) {
                continue;
            }
            
            // Get plane reflection shader
            auto plane_shader = resource_manager.get_shader("plane_reflection_shader");
            if (!plane_shader) {
                LOG_WARN("Renderer: Plane reflection shader not found, skipping plane reflection");
                return;
            }
            
            // Enable depth testing and disable blending for opaque plane rendering
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDisable(GL_BLEND);
            
            plane_shader->use();
            
            // Set camera matrices
            glm::mat4 view = camera.get_view_matrix();
            glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
            glm::vec3 camera_pos = camera.get_position();
            
            plane_shader->set_mat4("view", view);
            plane_shader->set_mat4("projection", projection);
            plane_shader->set_vec3("viewPos", camera_pos);
            
            // Set lighting uniforms
            auto scene_lights = resource_manager.get_scene_lights(scene);
            if (!scene_lights.empty() && scene_lights[0]) {
                auto light = scene_lights[0];
                plane_shader->set_vec3("lightPos", light->get_position());
                plane_shader->set_vec3("lightColor", light->get_color());
            } else {
                // Fallback lighting
                plane_shader->set_vec3("lightPos", glm::vec3(2.0f, 4.0f, 2.0f));
                plane_shader->set_vec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
            }
            
            // Bind skybox texture for reflection using automatic slot management
            auto skybox_texture = resource_manager.get<Texture>("skybox_cubemap");
            if (skybox_texture) {
                unsigned int slot = skybox_texture->bind_cubemap_auto();
                if (slot != Texture::INVALID_SLOT) {
                    plane_shader->set_int("skybox", slot);
                    //LOG_INFO("Renderer: Skybox texture ID {} bound to slot {} for plane reflection", skybox_texture->get_id(), slot);
                } else {
                    LOG_ERROR("Renderer: Failed to bind skybox texture for plane reflection");
                }
            } else {
                LOG_ERROR("Renderer: Skybox texture not found for plane reflection");
            }
            
            // Set shadow mapping uniforms
            if (shadow_map) {
                GLuint shadow_texture_id = shadow_map->get_depth_texture();
                unsigned int shadow_slot = Texture::bind_raw_texture(shadow_texture_id, GL_TEXTURE_2D);
                if (shadow_slot != Texture::INVALID_SLOT) {
                    plane_shader->set_int("shadowMap", shadow_slot);
                }
                plane_shader->set_float("pcfRadius", 1.5f);
                plane_shader->set_float("lightSize", 5.0f);
                
                // Set light space matrix
                glm::vec3 shadow_light_direction = glm::normalize(shadow_light_pos_);
                if (!scene_lights.empty() && scene_lights[0] && scene_lights[0]->get_type() == Light::Type::kDirectional) {
                    shadow_light_direction = scene_lights[0]->get_direction();
                }
                glm::vec3 shadow_center = glm::vec3(0.0f, 0.0f, 0.0f);
                glm::mat4 lightSpaceMatrix = shadow_map->get_light_space_matrix(shadow_light_direction, shadow_center);
                plane_shader->set_mat4("lightSpaceMatrix", lightSpaceMatrix);
            }
            
            // Set reflection strength
            plane_shader->set_float("reflectionStrength", 0.5f);
            
            // Get transform and set model matrix
            glm::mat4 model_matrix = transform_manager.get_model_matrix(renderable_id);
            plane_shader->set_mat4("model", model_matrix);
            
            // Set material properties
            const Material& material = *model->get_material();
            material.set_shader(*plane_shader, "material");
            
            // Bind material textures using automatic slot management
            material.bind_textures_auto(*plane_shader, resource_manager);
            
            // Set object color (for compatibility)
            plane_shader->set_vec3("objectColor", material.get_diffuse());
            
            // Render the plane mesh
            try {
                const Mesh& mesh = *model->get_mesh();
                mesh.draw();
                LOG_DEBUG("Renderer: Successfully rendered plane with reflection");
            } catch (const std::exception& e) {
                LOG_ERROR("Renderer: Failed to render plane reflection: {}", e.what());
            }
            
            break; // Only render one plane
        }
    }

    // SSGI Implementation
    void Renderer::setup_ssgi() {
        setup_ssgi_textures();
        LOG_INFO("SSGI setup completed");
    }

    void Renderer::cleanup_ssgi() {
        cleanup_ssgi_textures();
        if (ssgi_fbo_ != 0) {
            glDeleteFramebuffers(1, &ssgi_fbo_);
            ssgi_fbo_ = 0;
        }
        LOG_INFO("SSGI cleanup completed");
    }

    void Renderer::setup_ssgi_textures() {
        // Generate framebuffer
        glGenFramebuffers(1, &ssgi_fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, ssgi_fbo_);

        // Create SSGI raw texture using factory method
        ssgi_raw_texture_ = std::make_unique<Texture>(Texture::create_framebuffer_texture(viewport_width_, viewport_height_, GL_RGBA16F, GL_RGBA, GL_FLOAT));

        // Create SSGI final texture (denoised output) using factory method
        ssgi_final_texture_ = std::make_unique<Texture>(Texture::create_framebuffer_texture(viewport_width_, viewport_height_, GL_RGBA16F, GL_RGBA, GL_FLOAT));

        // Create lit scene texture (direct lighting only) using factory method
        lit_scene_texture_ = std::make_unique<Texture>(Texture::create_framebuffer_texture(viewport_width_, viewport_height_, GL_RGBA16F, GL_RGBA, GL_FLOAT));

        // Create previous frame SSGI texture for temporal accumulation using factory method
        ssgi_prev_texture_ = std::make_unique<Texture>(Texture::create_framebuffer_texture(viewport_width_, viewport_height_, GL_RGBA16F, GL_RGBA, GL_FLOAT));

        // Attach textures to framebuffer 
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssgi_raw_texture_->get_id(), 0);

        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("SSGI framebuffer is not complete!");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        LOG_INFO("SSGI textures setup completed: {}x{}", viewport_width_, viewport_height_);
    }

    void Renderer::cleanup_ssgi_textures() {
        // Texture objects will be automatically cleaned up by their destructors
        ssgi_raw_texture_.reset();
        ssgi_final_texture_.reset();
        ssgi_prev_texture_.reset();
        lit_scene_texture_.reset();
    }

    void Renderer::set_ssgi_enabled(bool enable) {
        use_ssgi_ = enable;
        LOG_INFO("SSGI {}", enable ? "enabled" : "disabled");
    }

    void Renderer::set_ssgi_exposure(float exposure) {
        ssgi_exposure_ = exposure;
        LOG_DEBUG("Renderer: SSGI exposure set to {}", exposure);
    }

    void Renderer::set_ssgi_intensity(float intensity) {
        ssgi_intensity_ = intensity;
        LOG_DEBUG("Renderer: SSGI intensity set to {}", intensity);
    }

    void Renderer::set_ssgi_max_steps(int max_steps) {
        ssgi_max_steps_ = max_steps;
        LOG_DEBUG("Renderer: SSGI max steps set to {}", max_steps);
    }

    void Renderer::set_ssgi_max_distance(float max_distance) {
        ssgi_max_distance_ = max_distance;
        LOG_DEBUG("Renderer: SSGI max distance set to {}", max_distance);
    }

    void Renderer::set_ssgi_step_size(float step_size) {
        ssgi_step_size_ = step_size;
        LOG_DEBUG("Renderer: SSGI step size set to {}", step_size);
    }

    void Renderer::set_ssgi_thickness(float thickness) {
        ssgi_thickness_ = thickness;
        LOG_DEBUG("Renderer: SSGI thickness set to {}", thickness);
    }

    void Renderer::set_ssgi_num_samples(int num_samples) {
        ssgi_num_samples_ = num_samples;
        LOG_DEBUG("Renderer: SSGI num samples set to {}", num_samples);
    }

    // SSAO Implementation
    void Renderer::setup_ssao() {
        setup_ssao_textures();
        generate_ssao_noise_texture();
        generate_ssao_sample_kernel();
        LOG_INFO("SSAO setup completed");
    }

    void Renderer::cleanup_ssao() {
        cleanup_ssao_textures();
        if (ssao_fbo_ != 0) {
            glDeleteFramebuffers(1, &ssao_fbo_);
            ssao_fbo_ = 0;
        }
        LOG_INFO("SSAO cleanup completed");
    }

    void Renderer::setup_ssao_textures() {
        // Generate framebuffer
        glGenFramebuffers(1, &ssao_fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);

        // Create SSAO raw texture (single channel for AO values) using factory method
        ssao_raw_texture_ = std::make_unique<Texture>(Texture::create_framebuffer_texture(viewport_width_, viewport_height_, GL_R16F, GL_RED, GL_FLOAT));

        // Create SSAO final texture (blurred output) using factory method
        ssao_final_texture_ = std::make_unique<Texture>(Texture::create_framebuffer_texture(viewport_width_, viewport_height_, GL_R16F, GL_RED, GL_FLOAT));

        // Attach texture to framebuffer 
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssao_final_texture_->get_id(), 0);

        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("SSAO framebuffer is not complete!");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        LOG_INFO("SSAO textures setup completed: {}x{}", viewport_width_, viewport_height_);
    }

    void Renderer::cleanup_ssao_textures() {
        // Texture objects will be automatically cleaned up by their destructors
        ssao_raw_texture_.reset();
        ssao_final_texture_.reset();
        ssao_noise_texture_.reset();
    }

    void Renderer::generate_ssao_noise_texture() {
        // Use the new factory method to create SSAO noise texture
        ssao_noise_texture_ = std::make_unique<Texture>(Texture::create_ssao_noise_texture());
        
        LOG_DEBUG("SSAO noise texture generated using factory method");
    }

    void Renderer::generate_ssao_sample_kernel() {
        // Sample kernel is generated and uploaded as uniform in SSAO_render
        // This method is kept for consistency but actual kernel generation happens in render
        LOG_DEBUG("SSAO sample kernel generation prepared");
    }

    void Renderer::set_ssao_enabled(bool enable) {
        use_ssao_ = enable;
        LOG_INFO("SSAO {}", enable ? "enabled" : "disabled");
    }

    void Renderer::apply_ssao_to_framebuffer(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager) {
        // Initialize screen quad if not already done
        if (!screen_quad_mesh_) {
            setup_screen_quad(resource_manager);
        }
        
        // Get SSAO apply shader
        auto ssao_apply_shader = resource_manager.get_shader("ssao_apply_shader");
        if (!ssao_apply_shader) {
            LOG_ERROR("SSAO apply shader not found in ResourceManager");
            return;
        }

        // Create a temporary texture to store current framebuffer content
        GLuint temp_texture;
        glGenTextures(1, &temp_texture);
        glBindTexture(GL_TEXTURE_2D, temp_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, viewport_width_, viewport_height_, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Copy current framebuffer content to temporary texture
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
        glBindTexture(GL_TEXTURE_2D, temp_texture);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, viewport_width_, viewport_height_, 0);

        // Now render back to framebuffer with SSAO applied
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, viewport_width_, viewport_height_);
        
        // Disable depth testing for screen-space quad
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        ssao_apply_shader->use();
        
        // Bind textures
        Texture::reset_slot_counter();
        unsigned int scene_slot = Texture::bind_raw_texture(temp_texture, GL_TEXTURE_2D);
        unsigned int ssao_slot = Texture::bind_raw_texture(ssao_final_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int motion_ao_slot = Texture::bind_raw_texture(g_motion_ao_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int pos_slot = Texture::bind_raw_texture(g_position_texture_->get_id(), GL_TEXTURE_2D);
        
        if (scene_slot != Texture::INVALID_SLOT) ssao_apply_shader->set_int("sceneTexture", scene_slot);
        if (ssao_slot != Texture::INVALID_SLOT) ssao_apply_shader->set_int("ssaoTexture", ssao_slot);
        if (motion_ao_slot != Texture::INVALID_SLOT) ssao_apply_shader->set_int("gMotionAO", motion_ao_slot);
        if (pos_slot != Texture::INVALID_SLOT) ssao_apply_shader->set_int("gPosition", pos_slot);

        // Render screen-space quad
        render_screen_quad();

        // Clean up temporary texture
        glDeleteTextures(1, &temp_texture);
        
        LOG_DEBUG("SSAO applied to framebuffer");
    }

    void Renderer::SSAO_render(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager) {
        // Initialize screen quad if not already done
        if (!screen_quad_mesh_) {
            setup_screen_quad(resource_manager);
        }
        
        if (!use_ssao_) {
            return;
        }

        // Get required shaders
        auto ssao_compute_shader = resource_manager.get_shader("ssao_compute_shader");
        auto ssao_blur_shader = resource_manager.get_shader("ssao_blur_shader");
        
        if (!ssao_compute_shader || !ssao_blur_shader) {
            LOG_ERROR("SSAO shaders not found in ResourceManager");
            return;
        }

        // Camera matrices
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
        glm::mat4 invView = glm::inverse(view);
        glm::mat4 invProjection = glm::inverse(projection);
        glm::vec3 viewPos = camera.get_position();

        // Generate sample kernel
        std::vector<glm::vec3> ssaoKernel;
        std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
        std::default_random_engine generator;
        
        for (unsigned int i = 0; i < 64; ++i) {
            glm::vec3 sample(
                randomFloats(generator) * 2.0 - 1.0,
                randomFloats(generator) * 2.0 - 1.0,
                randomFloats(generator)
            );
            sample = glm::normalize(sample);
            sample *= randomFloats(generator);
            
            // Scale samples s.t. they're more aligned to center of kernel
            float scale = float(i) / 64.0f;
            scale = 0.1f + (scale * scale) * (1.0f - 0.1f);
            sample *= scale;
            ssaoKernel.push_back(sample);
        }

        // Step 1: SSAO Compute Pass
        ssao_compute_shader->use();
        
        // Bind G-Buffer textures using automatic slot management
        Texture::unbind_all_textures();
        unsigned int ssao_pos_slot = Texture::bind_raw_texture(g_position_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int ssao_normal_slot = Texture::bind_raw_texture(g_normal_roughness_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int ssao_depth_slot = Texture::bind_raw_texture(g_depth_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int ssao_noise_slot = Texture::bind_raw_texture(ssao_noise_texture_->get_id(), GL_TEXTURE_2D);
        
        if (ssao_pos_slot != Texture::INVALID_SLOT) ssao_compute_shader->set_int("gPosition", ssao_pos_slot);
        if (ssao_normal_slot != Texture::INVALID_SLOT) ssao_compute_shader->set_int("gNormalRoughness", ssao_normal_slot);
        if (ssao_depth_slot != Texture::INVALID_SLOT) ssao_compute_shader->set_int("gDepth", ssao_depth_slot);
        if (ssao_noise_slot != Texture::INVALID_SLOT) ssao_compute_shader->set_int("noiseTexture", ssao_noise_slot);

        // Set camera uniforms
        ssao_compute_shader->set_mat4("view", view);
        ssao_compute_shader->set_mat4("projection", projection);
        ssao_compute_shader->set_mat4("invView", invView);
        ssao_compute_shader->set_mat4("invProjection", invProjection);
        ssao_compute_shader->set_vec3("viewPos", viewPos);

        // Set SSAO parameters
        ssao_compute_shader->set_int("numSamples", 64);
        ssao_compute_shader->set_float("radius", 0.5f);
        ssao_compute_shader->set_float("bias", 0.025f);
        ssao_compute_shader->set_float("intensity", 1.0f);
        ssao_compute_shader->set_vec2("noiseScale", glm::vec2(viewport_width_ / 4.0f, viewport_height_ / 4.0f));

        // Upload sample kernel
        for (unsigned int i = 0; i < 64; ++i) {
            ssao_compute_shader->set_vec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
        }

        // Bind output texture
        glBindImageTexture(0, ssao_raw_texture_->get_id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16F);

        // Dispatch compute shader
        glDispatchCompute((viewport_width_ + 7) / 8, (viewport_height_ + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Step 2: Blur Pass
        glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssao_final_texture_->get_id(), 0);
        glViewport(0, 0, viewport_width_, viewport_height_);
        glClear(GL_COLOR_BUFFER_BIT);

        ssao_blur_shader->use();
        
        // Bind input texture for blurring
        Texture::reset_slot_counter();
        unsigned int blur_input_slot = Texture::bind_raw_texture(ssao_raw_texture_->get_id(), GL_TEXTURE_2D);
        
        if (blur_input_slot != Texture::INVALID_SLOT) ssao_blur_shader->set_int("ssaoInput", blur_input_slot);

        // Set blur parameters
        ssao_blur_shader->set_vec2("screenSize", glm::vec2(viewport_width_, viewport_height_));
        ssao_blur_shader->set_int("blurRadius", 2);

        // Render full-screen quad
        render_screen_quad();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        LOG_DEBUG("SSAO render pass completed");
    }



    // Hi-Z Buffer Implementation
    void Renderer::setup_hiz_buffer() {
        // Calculate number of mip levels needed
        int max_dimension = std::max(viewport_width_, viewport_height_);
        hiz_mip_levels_ = static_cast<int>(std::floor(std::log2(max_dimension))) + 1;
        
        glGenTextures(2, hiz_textures_);
        
        for (int i = 0; i < 2; ++i) {
            glBindTexture(GL_TEXTURE_2D, hiz_textures_[i]);
            glTexStorage2D(GL_TEXTURE_2D, hiz_mip_levels_, GL_R32F, viewport_width_, viewport_height_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        
        LOG_INFO("Hi-Z Ping-Pong Buffers setup completed: {}x{} with {} mip levels", 
            viewport_width_, viewport_height_, hiz_mip_levels_);
    }

    void Renderer::cleanup_hiz_buffer() {
        if (hiz_textures_[0] != 0) {
            glDeleteTextures(2, hiz_textures_);
            hiz_textures_[0] = 0;
            hiz_textures_[1] = 0;
        }
        final_hiz_texture_ = 0;
        hiz_mip_levels_ = 0;
        LOG_INFO("Hi-Z Buffer ping-pong cleanup completed");
    }

    void Renderer::generate_hiz_pyramid(const CoroutineResourceManager& resource_manager) {
        auto hiz_compute_shader = resource_manager.get_shader("hiz_generate_shader");
        if (!hiz_compute_shader) { 
            LOG_ERROR("Renderer: Hi-Z compute shader not found in ResourceManager");
            return; 
        }
        hiz_compute_shader->use();

        // Step 1: Generate Mip 0 from G-Buffer depth texture to hiz_textures_[0]
        LOG_DEBUG("Hi-Z: Generating Mip 0 from G-Buffer depth texture (ID: {}) to Hi-Z texture (ID: {})", 
                  g_depth_texture_->get_id(), hiz_textures_[0]);
        
        unsigned int depth_slot = Texture::bind_raw_texture(g_depth_texture_->get_id(), GL_TEXTURE_2D);
        if (depth_slot != Texture::INVALID_SLOT) {
            hiz_compute_shader->set_int("inputDepthTexture", depth_slot);
        }
        hiz_compute_shader->set_int("currentMipLevel", 0);
        
        glBindImageTexture(0, hiz_textures_[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        
        int mip_width = viewport_width_;
        int mip_height = viewport_height_;
        LOG_DEBUG("Hi-Z: Dispatching compute for Mip 0: {}x{}, groups: {}x{}", 
                  mip_width, mip_height, (mip_width + 7) / 8, (mip_height + 7) / 8);
        glDispatchCompute((mip_width + 7) / 8, (mip_height + 7) / 8, 1);
        
        // Memory barrier to ensure Mip 0 is complete
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        GLuint read_texture = hiz_textures_[0];
        GLuint write_texture = hiz_textures_[1];
        
        // Bind both Hi-Z textures once at the beginning
        unsigned int hiz_slot_0 = Texture::bind_raw_texture(hiz_textures_[0], GL_TEXTURE_2D);
        unsigned int hiz_slot_1 = Texture::bind_raw_texture(hiz_textures_[1], GL_TEXTURE_2D);

        // Step 2: Generate Mip 1 to N, alternating read/write
        for (int mip = 1; mip < hiz_mip_levels_; ++mip) {
            // Ensure previous write is complete, safe to read
            glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

            // Bind output texture mip
            glBindImageTexture(0, write_texture, mip, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

            // Use the appropriate slot based on which texture we're reading from
            unsigned int current_read_slot = (read_texture == hiz_textures_[0]) ? hiz_slot_0 : hiz_slot_1;
            hiz_compute_shader->set_int("inputDepthTexture", current_read_slot);
            hiz_compute_shader->set_int("inputMipLevel", mip - 1);
            hiz_compute_shader->set_int("currentMipLevel", mip);
            
            // Dispatch
            mip_width = std::max(1, viewport_width_ >> mip);
            mip_height = std::max(1, viewport_height_ >> mip);
            glDispatchCompute((mip_width + 7) / 8, (mip_height + 7) / 8, 1);
            
            // Swap roles for next iteration
            std::swap(read_texture, write_texture);
        }
        
        // Step 3: Record final result texture, no copy needed!
        final_hiz_texture_ = read_texture; 

        // Final barrier to ensure all generation operations are complete for subsequent passes
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        
        LOG_DEBUG("Hi-Z pyramid generation completed");
    }

    void Renderer::SSGI_render(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager) {
        // Initialize screen quad if not already done
        if (!screen_quad_mesh_) {
            setup_screen_quad(resource_manager);
        }
        
        if (!use_ssgi_) {
            return;
        }

        // LOG_DEBUG("Renderer: SSGI compute - processing global illumination");
        // Get required shaders
        auto ssgi_compute_shader = resource_manager.get_shader("ssgi_compute_shader");
        auto ssgi_denoise_shader = resource_manager.get_shader("ssgi_denoise_shader");
        
        if (!ssgi_compute_shader || !ssgi_denoise_shader) {
            LOG_ERROR("SSGI shaders not found in ResourceManager");
            return;
        }

        // Camera matrices
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
        glm::mat4 invView = glm::inverse(view);
        glm::mat4 invProjection = glm::inverse(projection);
        glm::vec3 viewPos = camera.get_position();

        // Step 1: SSGI Compute Pass
        ssgi_compute_shader->use();
        
        // Bind G-Buffer textures using automatic slot management
        Texture::reset_slot_counter();
        unsigned int ssgi_pos_slot = Texture::bind_raw_texture(g_position_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int ssgi_albedo_slot = Texture::bind_raw_texture(g_albedo_metallic_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int ssgi_normal_slot = Texture::bind_raw_texture(g_normal_roughness_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int ssgi_motion_slot = Texture::bind_raw_texture(g_motion_ao_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int ssgi_depth_slot = Texture::bind_raw_texture(g_depth_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int ssgi_lit_slot = Texture::bind_raw_texture(lit_scene_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int ssgi_hiz_slot = Texture::bind_raw_texture(final_hiz_texture_, GL_TEXTURE_2D);
        
        if (ssgi_pos_slot != Texture::INVALID_SLOT) ssgi_compute_shader->set_int("gPosition", ssgi_pos_slot);
        if (ssgi_albedo_slot != Texture::INVALID_SLOT) ssgi_compute_shader->set_int("gAlbedoMetallic", ssgi_albedo_slot);
        if (ssgi_normal_slot != Texture::INVALID_SLOT) ssgi_compute_shader->set_int("gNormalRoughness", ssgi_normal_slot);
        if (ssgi_motion_slot != Texture::INVALID_SLOT) ssgi_compute_shader->set_int("gMotionAO", ssgi_motion_slot);
        if (ssgi_depth_slot != Texture::INVALID_SLOT) ssgi_compute_shader->set_int("gDepth", ssgi_depth_slot);
        if (ssgi_lit_slot != Texture::INVALID_SLOT) ssgi_compute_shader->set_int("litSceneTexture", ssgi_lit_slot);
        if (ssgi_hiz_slot != Texture::INVALID_SLOT) ssgi_compute_shader->set_int("hizTexture", ssgi_hiz_slot);

        // Set camera uniforms
        ssgi_compute_shader->set_mat4("view", view);
        ssgi_compute_shader->set_mat4("projection", projection);
        ssgi_compute_shader->set_mat4("invView", invView);
        ssgi_compute_shader->set_mat4("invProjection", invProjection);
        ssgi_compute_shader->set_vec3("viewPos", viewPos);

        // Set SSGI parameters - use dynamic values from member variables
        ssgi_compute_shader->set_int("maxSteps", ssgi_max_steps_);
        ssgi_compute_shader->set_float("maxDistance", ssgi_max_distance_);
        ssgi_compute_shader->set_float("stepSize", ssgi_step_size_);
        ssgi_compute_shader->set_float("thickness", ssgi_thickness_);
        ssgi_compute_shader->set_float("intensity", ssgi_intensity_);
        ssgi_compute_shader->set_int("numSamples", ssgi_num_samples_);

        // Bind output texture
        glBindImageTexture(0, ssgi_raw_texture_->get_id(), 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        // Dispatch compute shader using actual viewport dimensions
        glDispatchCompute((viewport_width_ + 7) / 8, (viewport_height_ + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Step 2: Denoising Pass
        glBindFramebuffer(GL_FRAMEBUFFER, ssgi_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssgi_final_texture_->get_id(), 0);
        glViewport(0, 0, viewport_width_, viewport_height_);
        glClear(GL_COLOR_BUFFER_BIT);

        ssgi_denoise_shader->use();
        
        // Bind input textures for denoising using automatic slot management
        Texture::reset_slot_counter();
        unsigned int denoise_raw_slot = Texture::bind_raw_texture(ssgi_raw_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int denoise_prev_slot = Texture::bind_raw_texture(ssgi_prev_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int denoise_pos_slot = Texture::bind_raw_texture(g_position_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int denoise_normal_slot = Texture::bind_raw_texture(g_normal_roughness_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int denoise_motion_slot = Texture::bind_raw_texture(g_motion_ao_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int denoise_depth_slot = Texture::bind_raw_texture(g_depth_texture_->get_id(), GL_TEXTURE_2D);
        
        if (denoise_raw_slot != Texture::INVALID_SLOT) ssgi_denoise_shader->set_int("ssgi_raw_texture", denoise_raw_slot);
        if (denoise_prev_slot != Texture::INVALID_SLOT) ssgi_denoise_shader->set_int("ssgi_prev_texture", denoise_prev_slot);
        if (denoise_pos_slot != Texture::INVALID_SLOT) ssgi_denoise_shader->set_int("gPosition", denoise_pos_slot);
        if (denoise_normal_slot != Texture::INVALID_SLOT) ssgi_denoise_shader->set_int("gNormalRoughness", denoise_normal_slot);
        if (denoise_motion_slot != Texture::INVALID_SLOT) ssgi_denoise_shader->set_int("gMotionAO", denoise_motion_slot);
        if (denoise_depth_slot != Texture::INVALID_SLOT) ssgi_denoise_shader->set_int("gDepth", denoise_depth_slot);

        // Set denoising parameters
        ssgi_denoise_shader->set_float("spatialSigma", 2.0f);
        ssgi_denoise_shader->set_float("normalSigma", 0.1f);
        ssgi_denoise_shader->set_float("depthSigma", 0.01f);
        ssgi_denoise_shader->set_int("filterRadius", 2);
        ssgi_denoise_shader->set_bool("enableTemporalFilter", false);
        ssgi_denoise_shader->set_vec2("screenSize", glm::vec2(viewport_width_, viewport_height_));
        
        // Set temporal accumulation parameters
        ssgi_denoise_shader->set_float("temporalAlpha", 0.9f);      // High temporal weight for stability
        ssgi_denoise_shader->set_float("maxTemporalWeight", 0.95f); // Maximum temporal contribution
        ssgi_denoise_shader->set_bool("isFirstFrame", first_frame_);

        // Render full-screen quad
        render_screen_quad();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // Copy current frame SSGI result to previous frame texture for next frame temporal accumulation
        glBindFramebuffer(GL_READ_FRAMEBUFFER, ssgi_fbo_);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glBindTexture(GL_TEXTURE_2D, ssgi_prev_texture_->get_id());
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 0, 0, viewport_width_, viewport_height_, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        LOG_DEBUG("SSGI render pass completed");
    }

    void Renderer::render_direct_lighting_pass(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager) {
        // Initialize screen quad if not already done
        if (!screen_quad_mesh_) {
            setup_screen_quad(resource_manager);
        }
        
        // Render direct lighting to lit_scene_texture_
        // LOG_DEBUG("Renderer: Direct lighting pass - binding framebuffer and textures");
        glBindFramebuffer(GL_FRAMEBUFFER, ssgi_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lit_scene_texture_->get_id(), 0);
        glViewport(0, 0, viewport_width_, viewport_height_);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Disable depth testing for screen-space quad
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        
        // Get direct lighting shader
        auto direct_lighting_shader = resource_manager.get_shader("deferred_lighting_direct_shader");
        if (!direct_lighting_shader) {
            LOG_ERROR("Renderer: Direct lighting shader not found in ResourceManager");
            return;
        }
        
        direct_lighting_shader->use();
        
        // Bind G-Buffer textures using automatic slot management
        Texture::reset_slot_counter();
        unsigned int direct_pos_slot = Texture::bind_raw_texture(g_position_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int direct_albedo_slot = Texture::bind_raw_texture(g_albedo_metallic_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int direct_normal_slot = Texture::bind_raw_texture(g_normal_roughness_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int direct_motion_slot = Texture::bind_raw_texture(g_motion_ao_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int direct_emissive_slot = Texture::bind_raw_texture(g_emissive_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int direct_depth_slot = Texture::bind_raw_texture(g_depth_texture_->get_id(), GL_TEXTURE_2D);
        
        if (direct_pos_slot != Texture::INVALID_SLOT) direct_lighting_shader->set_int("gPosition", direct_pos_slot);
        if (direct_albedo_slot != Texture::INVALID_SLOT) direct_lighting_shader->set_int("gAlbedoMetallic", direct_albedo_slot);
        if (direct_normal_slot != Texture::INVALID_SLOT) direct_lighting_shader->set_int("gNormalRoughness", direct_normal_slot);
        if (direct_motion_slot != Texture::INVALID_SLOT) direct_lighting_shader->set_int("gMotionAO", direct_motion_slot);
        if (direct_emissive_slot != Texture::INVALID_SLOT) direct_lighting_shader->set_int("gEmissive", direct_emissive_slot);
        if (direct_depth_slot != Texture::INVALID_SLOT) direct_lighting_shader->set_int("gDepth", direct_depth_slot);
        

        
        // Set camera uniforms
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
        glm::vec3 camera_pos = camera.get_position();
        
        direct_lighting_shader->set_vec3("viewPos", camera_pos);
        direct_lighting_shader->set_mat4("view", view);
        direct_lighting_shader->set_mat4("projection", projection);
        
        // Set ambient lighting from scene
        direct_lighting_shader->set_vec3("ambientLight", scene.get_ambient_light());
        
        // Set up lighting using scene lights
        auto scene_lights = resource_manager.get_scene_lights(scene);
        size_t light_size = std::min(scene_lights.size(), size_t(8));
        direct_lighting_shader->set_int("numLights", static_cast<int>(light_size));
        
        for (size_t i = 0; i < light_size; ++i) {
            auto light = scene_lights[i];
            if (light) {
                light->set_shader_array(*direct_lighting_shader, static_cast<int>(i));
            }
        }
        
        // Shadow mapping setup
        if (shadow_map) {
            GLuint shadow_texture_id = shadow_map->get_depth_texture();
            unsigned int direct_shadow_slot = Texture::bind_raw_texture(shadow_texture_id, GL_TEXTURE_2D);
            if (direct_shadow_slot != Texture::INVALID_SLOT) {
                direct_lighting_shader->set_int("shadowMap", direct_shadow_slot);
            }
            direct_lighting_shader->set_bool("enableShadows", true);
            direct_lighting_shader->set_mat4("lightSpaceMatrix", last_light_space_matrix_);
            /*glm::vec3 shadow_light_direction = glm::normalize(shadow_light_target_ - shadow_light_pos_);
            glm::vec3 shadow_center = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::mat4 lightSpaceMatrix = shadow_map->get_light_space_matrix(shadow_light_direction, shadow_center);*/
            //direct_lighting_shader->set_mat4("lightSpaceMatrix", lightSpaceMatrix);
        } else {
            direct_lighting_shader->set_bool("enableShadows", false);
        }
        
        // Render screen-space quad
        render_screen_quad();
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //LOG_DEBUG("Direct lighting pass completed");
    }

    void Renderer::render_composition_pass(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager) {
        // Initialize screen quad if not already done
        if (!screen_quad_mesh_) {
            setup_screen_quad(resource_manager);
        }
        
        // Final composition pass - render to main framebuffer
        // LOG_DEBUG("Renderer: Composition pass - combining direct lighting and SSGI");
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, viewport_width_, viewport_height_);
        
        // Disable depth testing for screen-space quad
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        
        // Get composition shader
        auto composition_shader = resource_manager.get_shader("ssgi_composition_shader");
        if (!composition_shader) {
            LOG_ERROR("Renderer: SSGI composition shader not found in ResourceManager");
            return;
        }
        
        composition_shader->use();
        
        // Bind input textures using automatic slot management
        Texture::reset_slot_counter();
        unsigned int comp_lit_slot = Texture::bind_raw_texture(lit_scene_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int comp_ssgi_slot = Texture::bind_raw_texture(ssgi_final_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int comp_pos_slot = Texture::bind_raw_texture(g_position_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int comp_albedo_slot = Texture::bind_raw_texture(g_albedo_metallic_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int comp_normal_slot = Texture::bind_raw_texture(g_normal_roughness_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int comp_emissive_slot = Texture::bind_raw_texture(g_emissive_texture_->get_id(), GL_TEXTURE_2D);
        unsigned int comp_motion_slot = Texture::bind_raw_texture(g_motion_ao_texture_->get_id(), GL_TEXTURE_2D);
        
        if (comp_lit_slot != Texture::INVALID_SLOT) composition_shader->set_int("litSceneTexture", comp_lit_slot);
        if (comp_ssgi_slot != Texture::INVALID_SLOT) composition_shader->set_int("ssgi_final_texture", comp_ssgi_slot);
        if (comp_pos_slot != Texture::INVALID_SLOT) composition_shader->set_int("gPosition", comp_pos_slot);
        if (comp_albedo_slot != Texture::INVALID_SLOT) composition_shader->set_int("gAlbedoMetallic", comp_albedo_slot);
        if (comp_normal_slot != Texture::INVALID_SLOT) composition_shader->set_int("gNormalRoughness", comp_normal_slot);
        if (comp_emissive_slot != Texture::INVALID_SLOT) composition_shader->set_int("gEmissive", comp_emissive_slot);
        if (comp_motion_slot != Texture::INVALID_SLOT) composition_shader->set_int("gMotionAO", comp_motion_slot);
        
        // Bind SSAO texture if enabled
        if (use_ssao_) {
            unsigned int ssao_slot = Texture::bind_raw_texture(ssao_final_texture_->get_id(), GL_TEXTURE_2D);
            if (ssao_slot != Texture::INVALID_SLOT) composition_shader->set_int("ssaoTexture", ssao_slot);
            composition_shader->set_bool("enableSSAO", true);
        } else {
            composition_shader->set_bool("enableSSAO", false);
        }
        
        // Set camera uniforms
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(viewport_width_) / static_cast<float>(viewport_height_));
        glm::mat4 invView = glm::inverse(view);
        glm::mat4 invProjection = glm::inverse(projection);
        glm::vec3 camera_pos = camera.get_position();
        
        composition_shader->set_vec3("viewPos", camera_pos);
        composition_shader->set_mat4("invView", invView);
        composition_shader->set_mat4("invProjection", invProjection);
        
        // Set ambient lighting
        composition_shader->set_vec3("ambientLight", scene.get_ambient_light());
        
        // IBL setup
        auto irradiance_map = resource_manager.get_irradiance_map("skybox_cubemap");
        auto prefiltered_map = resource_manager.get_prefiltered_map("skybox_cubemap");
        
        if (irradiance_map && prefiltered_map) {
            unsigned int irradiance_slot = irradiance_map->bind_cubemap_auto();
            if (irradiance_slot != Texture::INVALID_SLOT) {
                composition_shader->set_int("irradianceMap", irradiance_slot);
            }
            
            unsigned int prefiltered_slot = prefiltered_map->bind_cubemap_auto();
            if (prefiltered_slot != Texture::INVALID_SLOT) {
                composition_shader->set_int("prefilteredMap", prefiltered_slot);
            }
            
            composition_shader->set_bool("useIBL", true);
        } else {
            composition_shader->set_bool("useIBL", false);
        }
        
        // SSGI controls
        composition_shader->set_bool("enableSSGI", use_ssgi_);
        composition_shader->set_float("ssgiIntensity", ssgi_intensity_);
        composition_shader->set_float("exposure", ssgi_exposure_);
        
        // Render screen-space quad
        render_screen_quad();
        
        // Re-enable depth testing for subsequent rendering
        glEnable(GL_DEPTH_TEST);
        
        //LOG_DEBUG("Composition pass completed");
    }


}
