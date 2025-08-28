#include "Renderer.h"
#include "Logger.h"
#include "Camera.h"
#include "CoroutineResourceManager.h"
#include "TransformManager.h"
#include "Light.h"
#include <iostream>
#include <vector>
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
       framebuffer_(0),
       color_texture_(0),
       depth_texture_(0),
       use_framebuffer_(false),
       g_buffer_fbo_(0),
       g_position_texture_(0),
       g_albedo_metallic_texture_(0),
       g_normal_roughness_texture_(0),
       g_motion_ao_texture_(0),
       g_emissive_texture_(0),
       g_depth_texture_(0),
       use_deferred_rendering_(false),
       shadow_light_pos_(-2.0f, 4.0f, -1.0f),
       shadow_light_target_(0.0f, 0.0f, 0.0f),
       screen_quad_vao_(0),
       screen_quad_vbo_(0),
       skybox_vao_(0),
       skybox_vbo_(0),
       ssgi_fbo_(0),
       ssgi_raw_texture_(0),
       ssgi_final_texture_(0),
       lit_scene_texture_(0),
       use_ssgi_(false)
    {
    }

    Renderer::~Renderer() {
        cleanup_framebuffer();
        cleanup_g_buffer();
        cleanup_screen_quad();
        cleanup_skybox();
        cleanup_ssgi();
    }

    void Renderer::initialize() {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            throw std::runtime_error("Failed to initialize GLAD");
        }

        glViewport(0, 0, width_, height_);
        glEnable(GL_DEPTH_TEST);
        
        glDisable(GL_CULL_FACE);



        
        shadow_map = std::make_unique<ShadowMap>();
        if (shadow_map->initialize(2048, 2048)) {
            LOG_INFO("ShadowMap test passed!");
        } else {
            LOG_ERROR("ShadowMap test failed!");
        }
        
        // GUI initialization moved to Application   

        setup_framebuffer();
        setup_g_buffer();
        setup_screen_quad();
        setup_skybox();
        setup_ssgi();

    }
  
    void Renderer::setup_framebuffer() {
        
        glGenFramebuffers(1, &framebuffer_);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);

        //color texture
        glGenTextures(1, &color_texture_);
        glBindTexture(GL_TEXTURE_2D, color_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture_, 0);
        
        //depth texture
        glGenTextures(1, &depth_texture_);
        glBindTexture(GL_TEXTURE_2D, depth_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_, 0);

        GLenum framebuffer_Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (framebuffer_Status != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("ERROR: Framebuffer not complete! Status: {}", framebuffer_Status);
            switch(framebuffer_Status) {
                case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                    LOG_ERROR("  - INCOMPLETE_ATTACHMENT");
                    break;
                case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                    LOG_ERROR("  - MISSING_ATTACHMENT");
                    break;
                case GL_FRAMEBUFFER_UNSUPPORTED:
                    LOG_ERROR("  - UNSUPPORTED");
                    break;
                default:
                    LOG_ERROR("  - Unknown error: {}", framebuffer_Status);
            }
        } else {
            LOG_INFO("Framebuffer is complete! Color texture ID: {}, Depth texture ID: {}", 
                     color_texture_, depth_texture_);
        }
        
        //restore to default
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        LOG_INFO("Framebuffer setup completed: {}x{}", width_, height_);
    }

    void Renderer::resize_framebuffer(int newWidth, int newHeight) {
        if (newWidth <= 0 || newHeight <= 0) {
            return;
        }
        
        width_ = newWidth;
        height_ = newHeight;
        
        // Resize main framebuffer textures
        glBindTexture(GL_TEXTURE_2D, color_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        glBindTexture(GL_TEXTURE_2D, depth_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_, 0);

        // Resize G-Buffer textures
        if (g_position_texture_ != 0) {
            glBindTexture(GL_TEXTURE_2D, g_position_texture_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            
            glBindTexture(GL_TEXTURE_2D, g_albedo_metallic_texture_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
            
            glBindTexture(GL_TEXTURE_2D, g_normal_roughness_texture_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            
            glBindTexture(GL_TEXTURE_2D, g_motion_ao_texture_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            
            glBindTexture(GL_TEXTURE_2D, g_emissive_texture_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            
            glBindTexture(GL_TEXTURE_2D, g_depth_texture_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }

        LOG_INFO("Framebuffer and G-Buffer resized to: {}x{}", width_, height_);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void Renderer::cleanup_framebuffer() {
        if (color_texture_ != 0) {
            glDeleteTextures(1, &color_texture_);
            color_texture_ = 0;
        }
        if (depth_texture_ != 0) {
            glDeleteTextures(1, &depth_texture_);
            depth_texture_ = 0;
        }
        if (framebuffer_ != 0) {
            glDeleteFramebuffers(1, &framebuffer_);
            framebuffer_ = 0;
        }
    }
    
    void Renderer::setup_g_buffer() {
        // Generate G-Buffer framebuffer
        glGenFramebuffers(1, &g_buffer_fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_fbo_);
        
        // RT0: Position  + Material ID
        glGenTextures(1, &g_position_texture_);
        glBindTexture(GL_TEXTURE_2D, g_position_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_position_texture_, 0);
        
        // RT1: Position (RGB16F) + Metallic (R16F) - needs float format for position data
        glGenTextures(1, &g_albedo_metallic_texture_);
        glBindTexture(GL_TEXTURE_2D, g_albedo_metallic_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g_albedo_metallic_texture_, 0);
        
        // RT2: Normal (RGB8) + Roughness (R8)
        glGenTextures(1, &g_normal_roughness_texture_);
        glBindTexture(GL_TEXTURE_2D, g_normal_roughness_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g_normal_roughness_texture_, 0);
        
        // RT3: Motion Vector (RG8) + AO (R8) + unused (R8)
        glGenTextures(1, &g_motion_ao_texture_);
        glBindTexture(GL_TEXTURE_2D, g_motion_ao_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, g_motion_ao_texture_, 0);
        
        // RT4: Emissive Color (RGB8) + Intensity (R8)
        glGenTextures(1, &g_emissive_texture_);
        glBindTexture(GL_TEXTURE_2D, g_emissive_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, g_emissive_texture_, 0);
        
        // Depth buffer
        glGenTextures(1, &g_depth_texture_);
        glBindTexture(GL_TEXTURE_2D, g_depth_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_depth_texture_, 0);
        
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
            LOG_INFO("G-Buffer setup completed: {}x{} with 4 render targets", width_, height_);
        }
        
        // Unbind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void Renderer::cleanup_g_buffer() {
        if (g_position_texture_ != 0) {
            glDeleteTextures(1, &g_position_texture_);
            g_position_texture_ = 0;
        }
        if (g_albedo_metallic_texture_ != 0) {
            glDeleteTextures(1, &g_albedo_metallic_texture_);
            g_albedo_metallic_texture_ = 0;
        }
        if (g_normal_roughness_texture_ != 0) {
            glDeleteTextures(1, &g_normal_roughness_texture_);
            g_normal_roughness_texture_ = 0;
        }
        if (g_motion_ao_texture_ != 0) {
            glDeleteTextures(1, &g_motion_ao_texture_);
            g_motion_ao_texture_ = 0;
        }
        if (g_emissive_texture_ != 0) {
            glDeleteTextures(1, &g_emissive_texture_);
            g_emissive_texture_ = 0;
        }
        if (g_depth_texture_ != 0) {
            glDeleteTextures(1, &g_depth_texture_);
            g_depth_texture_ = 0;
        }
        if (g_buffer_fbo_ != 0) {
            glDeleteFramebuffers(1, &g_buffer_fbo_);
            g_buffer_fbo_ = 0;
        }
    }
    
    void Renderer::set_deferred_rendering(bool enable) {
        use_deferred_rendering_ = enable;
        LOG_INFO("Deferred rendering {}", enable ? "enabled" : "disabled");
    }
    
    void Renderer::setup_screen_quad() {
        // Screen-space quad vertices (NDC coordinates)
        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        
        glGenVertexArrays(1, &screen_quad_vao_);
        glGenBuffers(1, &screen_quad_vbo_);
        
        glBindVertexArray(screen_quad_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, screen_quad_vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        
        // Position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        
        // Texture coordinate attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        
        glBindVertexArray(0);
        
        // LOG_INFO("Screen-space quad setup completed");
    }
    
    void Renderer::cleanup_screen_quad() {
        if (screen_quad_vbo_ != 0) {
            glDeleteBuffers(1, &screen_quad_vbo_);
            screen_quad_vbo_ = 0;
        }
        if (screen_quad_vao_ != 0) {
            glDeleteVertexArrays(1, &screen_quad_vao_);
            screen_quad_vao_ = 0;
        }
    }
    
    void Renderer::render_screen_quad() {
        glBindVertexArray(screen_quad_vao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
    
    void Renderer::set_render_to_framebuffer(bool enable) {
        use_framebuffer_ = enable;
        if (enable) {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glViewport(0, 0, width_, height_);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width_, height_);
        }
    }
    
    void Renderer::bind_g_buffer_for_geometry_pass() {
      glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_fbo_);
      glViewport(0, 0, width_, height_);

      // Re-specify draw buffers
      GLenum draw_buffers[5] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4};
      glDrawBuffers(5, draw_buffers);

      // Clear G-Buffer
      glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // Enable depth testing and disable face culling
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
      glDisable(GL_CULL_FACE);
      
      // Disable blending for opaque geometry rendering
      glDisable(GL_BLEND);
    }

    void Renderer::bind_g_buffer_for_lighting_pass() {
      // Bind default framebuffer for final output
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
      glViewport(0, 0, width_, height_);

      // Disable depth testing for screen-space quad and ensure face culling is off
      glDisable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
      
      // Enable blending 
      glEnable(GL_BLEND);
      glBlendFunc(GL_ONE, GL_ONE);

      // Bind G-Buffer textures for reading
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, g_position_texture_);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, g_albedo_metallic_texture_);
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, g_normal_roughness_texture_);
      glActiveTexture(GL_TEXTURE3);
      glBindTexture(GL_TEXTURE_2D, g_motion_ao_texture_);
      glActiveTexture(GL_TEXTURE4);
      glBindTexture(GL_TEXTURE_2D, g_emissive_texture_);
      glActiveTexture(GL_TEXTURE5);
      glBindTexture(GL_TEXTURE_2D, g_depth_texture_);
    }

    void Renderer::render_deferred(const Scene& scene, const Camera& camera, 
        const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager) {
        // Check if scene is empty
        if (scene.is_empty()) {
            LOG_ERROR("Renderer: Scene is empty, skipping deferred rendering");
            return;
        }
        
        // Shadow Pass 
        if (shadow_map) {
            //LOG_INFO("Renderer: Rendering shadow pass for deferred rendering");
            render_shadow_pass_deferred(scene, resource_manager, transform_manager);
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
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(width_) / static_cast<float>(height_));
        glm::vec3 camera_pos = camera.get_position();
        
        geometry_shader->set_mat4("view", view);
        geometry_shader->set_mat4("projection", projection);

        // TODO: Implement previous frame MVP for motion vectors
        glm::mat4 prevMVP = projection * view; // Simplified for now
        geometry_shader->set_mat4("prevModelViewProjection", prevMVP);
        
        // Render all models to G-Buffer
        const auto& model_refs = scene.get_model_references();
        //LOG_INFO("Renderer: Geometry pass rendering {} models", model_refs.size());
        
        for (const auto& model_id : model_refs) {
            // Skip plane model in geometry pass - it will be rendered separately with reflection
            if (model_id == "simple_scene_plane_model") {
                continue;
            }
            
            auto model = resource_manager.get<Model>(model_id);
            if (!model || !model->has_mesh() || !model->has_material()) {
                continue;
            }
            
            // Get transform from external transform system
            glm::mat4 model_matrix = transform_manager.get_model_matrix(model_id);
            geometry_shader->set_mat4("model", model_matrix);
            
            // Set material properties
            const Material& material = *model->get_material();
            
            // Set basic material uniforms
            material.set_shader(*geometry_shader, "material");
            
            // Set PBR material parameters
            material.set_shader_pbr(*geometry_shader);
            geometry_shader->set_int("materialID", 0);
            
            // Bind material textures
            material.bind_textures(*geometry_shader, resource_manager);
            
            // Render the mesh
            try {
                const Mesh& mesh = *model->get_mesh();
                //LOG_INFO("Renderer: Drawing model '{}' with {} vertices", model_id, mesh.get_vertex_count());
                mesh.draw();
                //LOG_INFO("Renderer: Successfully drew model '{}'", model_id);
            } catch (const std::exception& e) {
                LOG_ERROR("Renderer: Failed to render model '{}' in geometry pass: {}", model_id, e.what());
                continue;
            }
        }
        
        // Render skybox using G-Buffer depth information
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, width_, height_);
        
        // Clear only color buffer, keep depth from G-Buffer
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Copy depth from G-Buffer to final framebuffer
        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_buffer_fbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_);
        glBlitFramebuffer(0, 0, width_, height_, 0, 0, width_, height_, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        
        // Render skybox with proper depth testing
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        render_skybox(camera, resource_manager);

        if (use_ssgi_) {
            // SSGI-enabled pipeline: Direct lighting -> SSGI -> Skybox -> Composition
            // LOG_INFO("Renderer: Starting SSGI pipeline - Direct lighting pass");
            render_direct_lighting_pass(scene, camera, resource_manager);
            
            // LOG_INFO("Renderer: SSGI compute pass");
            SSGI_render(scene, camera, resource_manager);
            
            // Render skybox to main framebuffer before composition
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
           
            // LOG_INFO("Renderer: SSGI composition pass");
            render_composition_pass(scene, camera, resource_manager);
            
            // LOG_INFO("Renderer: Final skybox render");
            render_skybox(camera, resource_manager);
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
            lighting_shader->set_int("gPosition", 0);
            lighting_shader->set_int("gAlbedoMetallic", 1);
            lighting_shader->set_int("gNormalRoughness", 2);
            lighting_shader->set_int("gMotionAO", 3);
            lighting_shader->set_int("gEmissive", 4);
            lighting_shader->set_int("gDepth", 5);
        
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
        
            // IBL irradiance mapping
            auto irradiance_map = resource_manager.get_irradiance_map("skybox_cubemap");
        
            if (irradiance_map) {
                lighting_shader->set_bool("useIBL", true);
                lighting_shader->set_int("irradianceMap", 7);
            
                // Bind irradiance map
                glActiveTexture(GL_TEXTURE7);
                irradiance_map->bind_cube_map(7);
            
                LOG_INFO("Renderer: IBL irradiance map bound to texture unit 7 (ID: {})", irradiance_map->get_id());
            } else {
                lighting_shader->set_bool("useIBL", false);
                LOG_WARN("Renderer: No irradiance map found, using fallback ambient lighting");
            }
        
            // Shadow mapping (if enabled)
            if (shadow_map) {
                lighting_shader->set_bool("enableShadows", true);
                lighting_shader->set_int("shadowMap", 6);
            
                // Bind shadow map texture
                glActiveTexture(GL_TEXTURE6);
                GLuint shadow_texture_id = shadow_map->get_depth_texture();
                glBindTexture(GL_TEXTURE_2D, shadow_texture_id);
            

            
                // Use first light as shadow caster if available, otherwise use fixed position
                glm::vec3 shadow_light_direction = glm::normalize(shadow_light_pos_);
                if (light_size > 0 && scene_lights[0] && scene_lights[0]->get_type() == Light::Type::kDirectional) {
                    shadow_light_direction = scene_lights[0]->get_direction();
                }
            
                // For directional light shadows, center the shadow map around the scene center
                glm::vec3 shadow_center = glm::vec3(0.0f, 0.0f, 0.0f); // Use scene center as shadow map center
            
                // Set light space matrix for shadow mapping
                glm::mat4 lightSpaceMatrix = shadow_map->get_light_space_matrix(shadow_light_direction, shadow_center);
                lighting_shader->set_mat4("lightSpaceMatrix", lightSpaceMatrix);
            }
        
            // Render screen-space quad
            render_screen_quad();
            
            // Re-enable depth testing and disable blending for subsequent rendering
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
        }

            // Temporal function
            render_plane_reflection(scene, camera, resource_manager, transform_manager);
        
            // Render light spheres for visualization
            render_light_spheres(scene, camera, resource_manager);
    }
    
    void Renderer::render_gbuffer_debug(int debug_mode, const CoroutineResourceManager& resource_manager) {
        // Bind final framebuffer for output
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, width_, height_);
        
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
        
        // Bind G-Buffer textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_position_texture_);
        debug_shader->set_int("gPosition", 0);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_albedo_metallic_texture_);
        debug_shader->set_int("gAlbedoMetallic", 1);
        
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, g_normal_roughness_texture_);
        debug_shader->set_int("gNormalRoughness", 2);
        
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, g_motion_ao_texture_);
        debug_shader->set_int("gMotionAO", 3);
        
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, g_emissive_texture_);
        debug_shader->set_int("gEmissive", 4);
        
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, g_depth_texture_);
        debug_shader->set_int("gDepth", 5);
        
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
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(width_) / static_cast<float>(height_));
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
        
        // Render all models in the scene
        const auto& model_refs = scene.get_model_references();
        
        for (const auto& model_id : model_refs) {
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
                    
                    // Bind skybox texture for reflection
                    auto skybox_texture = resource_manager.get<Texture>("skybox_cubemap");
                    if (skybox_texture) {
                        skybox_texture->bind_cube_map(1);
                        plane_shader->set_int("skybox", 1);
                    }
                    
                    // Set reflection strength (can be adjusted)
                    plane_shader->set_float("reflectionStrength", 0.4f);
                    
                    // Get transform and render
                    glm::mat4 model_matrix = transform_manager.get_model_matrix(model_id);
                    plane_shader->set_mat4("model", model_matrix);
                    
                    // Set material properties
                    const Material& material = *model->get_material();
                    material.set_shader(*plane_shader, "material");
                    
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
         
         // Render skybox as background
         render_skybox(camera, resource_manager);
     }
     
    bool Renderer::validate_scene_resources(const Scene& scene, CoroutineResourceManager& resource_manager) const {
        LOG_DEBUG("Renderer: Validating scene resources");
        
        bool all_valid = true;
        
        // Validate models
        const auto& model_refs = scene.get_model_references();
        LOG_DEBUG("Renderer: Validating {} model references", model_refs.size());
        
        for (const auto& model_id : model_refs) {
            auto model = resource_manager.get<Model>(model_id);
            if (!model) {
                LOG_ERROR("Renderer: Model '{}' not found in ResourceManager", model_id);
                all_valid = false;
                continue;
            }
            
            if (!model->has_mesh()) {
                LOG_ERROR("Renderer: Model '{}' has no mesh", model_id);
                all_valid = false;
            }
            
            if (!model->has_material()) {
                LOG_ERROR("Renderer: Model '{}' has no material", model_id);
                all_valid = false;
            }
            
            LOG_DEBUG("Renderer: Model '{}' validation passed", model_id);
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
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(width_) / static_cast<float>(height_));
        
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
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(width_) / static_cast<float>(height_));
        
        skybox_shader->set_mat4("view", view);
        skybox_shader->set_mat4("projection", projection);
        
        // Get and bind skybox cubemap texture
        auto skybox_texture = resource_manager.get<Texture>("skybox_cubemap");
        if (skybox_texture) {
            skybox_texture->bind_cube_map(0);
            skybox_shader->set_int("skybox", 0);
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
    
    void Renderer::render_shadow_pass_deferred(const Scene& scene, const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager) {
        if (!shadow_map || !shadow_map->get_shadow_shader()) {
            LOG_ERROR("ShadowMap or shadow shader is null!");
            return;
        }
        
        shadow_map->begin_shadow_pass();
        shadow_map->get_shadow_shader()->use();
        
        // Use first light as shadow caster if available, otherwise use fixed position
        glm::vec3 shadow_light_direction = glm::normalize(shadow_light_pos_);
        auto scene_lights = resource_manager.get_scene_lights(scene);
        if (!scene_lights.empty() && scene_lights[0] && scene_lights[0]->get_type() == Light::Type::kDirectional) {
            shadow_light_direction = scene_lights[0]->get_direction();
        }
        
        // For directional light shadows, center the shadow map around the scene center
        // TODO: This should ideally use camera position, but we need to pass it to this function
        glm::vec3 shadow_center = glm::vec3(0.0f, 0.0f, 0.0f); // Scene center for now
        
        // Set light space matrix for shadow mapping
        glm::mat4 lightSpaceMatrix = shadow_map->get_light_space_matrix(shadow_light_direction, shadow_center);
        shadow_map->get_shadow_shader()->set_mat4("lightSpaceMatrix", lightSpaceMatrix);
        
        // Render all models in the scene for shadow mapping
        const auto& model_refs = scene.get_model_references();
        
        for (const auto& model_id : model_refs) {
            auto model = resource_manager.get<Model>(model_id);
            if (!model || !model->has_mesh()) {
                continue;
            }
            
            // Get transform from external transform system
            glm::mat4 model_matrix = transform_manager.get_model_matrix(model_id);
            shadow_map->get_shadow_shader()->set_mat4("model", model_matrix);
            
            // Render the mesh for shadow mapping
            try {
                const Mesh& mesh = *model->get_mesh();
                mesh.draw();
            } catch (const std::exception& e) {
                LOG_ERROR("Renderer: Failed to render model '{}' in shadow pass: {}", model_id, e.what());
                continue;
            }
        }
        shadow_map->end_shadow_pass();
    }

    void Renderer::render_plane_reflection(const Scene& scene, const Camera& camera, 
        const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager) {
        
        // Find the plane model in the scene
        const auto& model_refs = scene.get_model_references();
        for (const auto& model_id : model_refs) {
            if (model_id != "simple_scene_plane_model") {
                continue; // Skip non-plane models
            }
            
            auto model = resource_manager.get<Model>(model_id);
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
            glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(width_) / static_cast<float>(height_));
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
            
            // Bind skybox texture for reflection FIRST
            auto skybox_texture = resource_manager.get<Texture>("skybox_cubemap");
            if (skybox_texture) {
                skybox_texture->bind_cube_map(0);  // Use texture unit 0
                plane_shader->set_int("skybox", 0);
                //LOG_INFO("Renderer: Skybox texture ID {} bound to unit 0 for plane reflection", skybox_texture->get_id());
            } else {
                LOG_ERROR("Renderer: Skybox texture not found for plane reflection");
            }
            
            // Set shadow mapping uniforms
            if (shadow_map) {
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, shadow_map->get_depth_texture());
                plane_shader->set_int("shadowMap", 1);
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
            glm::mat4 model_matrix = transform_manager.get_model_matrix(model_id);
            plane_shader->set_mat4("model", model_matrix);
            
            // Set material properties
            const Material& material = *model->get_material();
            material.set_shader(*plane_shader, "material");
            
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

        // Create SSGI raw texture 
        glGenTextures(1, &ssgi_raw_texture_);
        glBindTexture(GL_TEXTURE_2D, ssgi_raw_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Create SSGI final texture (denoised output)
        glGenTextures(1, &ssgi_final_texture_);
        glBindTexture(GL_TEXTURE_2D, ssgi_final_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Create lit scene texture (direct lighting only)
        glGenTextures(1, &lit_scene_texture_);
        glBindTexture(GL_TEXTURE_2D, lit_scene_texture_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Attach textures to framebuffer 
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssgi_raw_texture_, 0);

        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("SSGI framebuffer is not complete!");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        LOG_INFO("SSGI textures setup completed: {}x{}", width_, height_);
    }

    void Renderer::cleanup_ssgi_textures() {
        if (ssgi_raw_texture_ != 0) {
            glDeleteTextures(1, &ssgi_raw_texture_);
            ssgi_raw_texture_ = 0;
        }
        if (ssgi_final_texture_ != 0) {
            glDeleteTextures(1, &ssgi_final_texture_);
            ssgi_final_texture_ = 0;
        }
        if (lit_scene_texture_ != 0) {
            glDeleteTextures(1, &lit_scene_texture_);
            lit_scene_texture_ = 0;
        }
    }

    void Renderer::set_ssgi_enabled(bool enable) {
        use_ssgi_ = enable;
        LOG_INFO("SSGI {}", enable ? "enabled" : "disabled");
    }

    void Renderer::SSGI_render(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager) {
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
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(width_) / static_cast<float>(height_));
        glm::mat4 invView = glm::inverse(view);
        glm::mat4 invProjection = glm::inverse(projection);
        glm::vec3 viewPos = camera.get_position();

        // Step 1: SSGI Compute Pass
        ssgi_compute_shader->use();
        
        // Bind G-Buffer textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_position_texture_);
        ssgi_compute_shader->set_int("gPosition", 0);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_albedo_metallic_texture_);
        ssgi_compute_shader->set_int("gAlbedoMetallic", 1);
        
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, g_normal_roughness_texture_);
        ssgi_compute_shader->set_int("gNormalRoughness", 2);
        
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, g_motion_ao_texture_);
        ssgi_compute_shader->set_int("gMotionAO", 3);
        
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, g_depth_texture_);
        ssgi_compute_shader->set_int("gDepth", 4);
        
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, lit_scene_texture_);
        ssgi_compute_shader->set_int("litSceneTexture", 5);

        // Set camera uniforms
        ssgi_compute_shader->set_mat4("view", view);
        ssgi_compute_shader->set_mat4("projection", projection);
        ssgi_compute_shader->set_mat4("invView", invView);
        ssgi_compute_shader->set_mat4("invProjection", invProjection);
        ssgi_compute_shader->set_vec3("viewPos", viewPos);

        // Set SSGI parameters
        ssgi_compute_shader->set_int("maxSteps", 32);
        ssgi_compute_shader->set_float("maxDistance", 10.0f);
        ssgi_compute_shader->set_float("stepSize", 0.1f);
        ssgi_compute_shader->set_float("thickness", 0.5f);
        ssgi_compute_shader->set_float("intensity", 1.0f);
        ssgi_compute_shader->set_int("numSamples", 8);

        // Bind output texture
        glBindImageTexture(0, ssgi_raw_texture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

        // Dispatch compute shader
        glDispatchCompute((width_ + 7) / 8, (height_ + 7) / 8, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        // Step 2: Denoising Pass
        glBindFramebuffer(GL_FRAMEBUFFER, ssgi_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssgi_final_texture_, 0);
        glViewport(0, 0, width_, height_);
        glClear(GL_COLOR_BUFFER_BIT);

        ssgi_denoise_shader->use();
        
        // Bind input textures for denoising
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ssgi_raw_texture_);
        ssgi_denoise_shader->set_int("ssgi_raw_texture", 0);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_position_texture_);
        ssgi_denoise_shader->set_int("gPosition", 1);
        
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, g_normal_roughness_texture_);
        ssgi_denoise_shader->set_int("gNormalRoughness", 2);
        
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, g_depth_texture_);
        ssgi_denoise_shader->set_int("gDepth", 3);

        // Set denoising parameters
        ssgi_denoise_shader->set_float("spatialSigma", 2.0f);
        ssgi_denoise_shader->set_float("normalSigma", 0.1f);
        ssgi_denoise_shader->set_float("depthSigma", 0.01f);
        ssgi_denoise_shader->set_int("filterRadius", 2);
        ssgi_denoise_shader->set_bool("enableTemporalFilter", false);
        ssgi_denoise_shader->set_vec2("screenSize", glm::vec2(width_, height_));

        // Render full-screen quad
        render_screen_quad();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        LOG_DEBUG("SSGI render pass completed");
    }

    void Renderer::render_direct_lighting_pass(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager) {
        // Render direct lighting to lit_scene_texture_
        // LOG_DEBUG("Renderer: Direct lighting pass - binding framebuffer and textures");
        glBindFramebuffer(GL_FRAMEBUFFER, ssgi_fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lit_scene_texture_, 0);
        glViewport(0, 0, width_, height_);
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
        
        // Bind G-Buffer textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_position_texture_);
        direct_lighting_shader->set_int("gPosition", 0);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_albedo_metallic_texture_);
        direct_lighting_shader->set_int("gAlbedoMetallic", 1);
        
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, g_normal_roughness_texture_);
        direct_lighting_shader->set_int("gNormalRoughness", 2);
        
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, g_motion_ao_texture_);
        direct_lighting_shader->set_int("gMotionAO", 3);
        
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, g_emissive_texture_);
        direct_lighting_shader->set_int("gEmissive", 4);
        
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, g_depth_texture_);
        direct_lighting_shader->set_int("gDepth", 5);
        
        // Set camera uniforms
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(width_) / static_cast<float>(height_));
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
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, shadow_map->get_depth_texture());
            direct_lighting_shader->set_int("shadowMap", 6);
            direct_lighting_shader->set_bool("enableShadows", true);
            
            glm::vec3 shadow_light_direction = glm::normalize(shadow_light_target_ - shadow_light_pos_);
            glm::vec3 shadow_center = glm::vec3(0.0f, 0.0f, 0.0f);
            glm::mat4 lightSpaceMatrix = shadow_map->get_light_space_matrix(shadow_light_direction, shadow_center);
            direct_lighting_shader->set_mat4("lightSpaceMatrix", lightSpaceMatrix);
        } else {
            direct_lighting_shader->set_bool("enableShadows", false);
        }
        
        // Render screen-space quad
        render_screen_quad();
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        //LOG_DEBUG("Direct lighting pass completed");
    }

    void Renderer::render_composition_pass(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager) {
        // Final composition pass - render to main framebuffer
        // LOG_DEBUG("Renderer: Composition pass - combining direct lighting and SSGI");
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, width_, height_);
        // Don't clear - skybox is already rendered
        
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
        
        // Bind input textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, lit_scene_texture_);
        composition_shader->set_int("litSceneTexture", 0);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, ssgi_final_texture_);
        composition_shader->set_int("ssgi_final_texture", 1);
        
        // Bind G-Buffer textures for background detection and environment lighting
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, g_position_texture_);
        composition_shader->set_int("gPosition", 2);
        
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, g_albedo_metallic_texture_);
        composition_shader->set_int("gAlbedoMetallic", 3);
        
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, g_normal_roughness_texture_);
        composition_shader->set_int("gNormalRoughness", 4);
        
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, g_emissive_texture_);
        composition_shader->set_int("gEmissive", 5);
        
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, g_motion_ao_texture_);
        composition_shader->set_int("gMotionAO", 6);
        
        // Set camera uniforms
        glm::mat4 view = camera.get_view_matrix();
        glm::mat4 projection = camera.get_projection_matrix(static_cast<float>(width_) / static_cast<float>(height_));
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
        if (irradiance_map) {
            glActiveTexture(GL_TEXTURE7);
            irradiance_map->bind_cube_map(7);
            composition_shader->set_int("irradianceMap", 7);
            composition_shader->set_bool("useIBL", true);
        } else {
            composition_shader->set_bool("useIBL", false);
        }
        
        // SSGI controls
        composition_shader->set_bool("enableSSGI", use_ssgi_);
        composition_shader->set_float("ssgiIntensity", 1.0f);
        
        // Render screen-space quad
        render_screen_quad();
        
        // Re-enable depth testing for subsequent rendering
        glEnable(GL_DEPTH_TEST);
        
        //LOG_DEBUG("Composition pass completed");
    }


}
