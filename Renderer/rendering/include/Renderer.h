#pragma once

#include <memory>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"
#include "Model.h"
#include "Light.h"
#include "Material.h"
#include "Texture.h"
#include "ShadowMap.h"
#include <Scene.h>

// Forward declarations
class TransformManager;

namespace glRenderer {
    class Renderer {
    public:
        Renderer(
            int width,
            int height
        );
        ~Renderer();

        void initialize();
        //void render();
        void process_input();
        void update_camera(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& camera_pos);
        GLuint get_color_texture() const { return color_texture_; }
   
        void set_render_to_framebuffer(bool enable);
        void resize_framebuffer(int width, int height);
        
        // Methods for Application to control rendering phases
        void begin_render();
        void end_render();
        
        // New Scene-based rendering method
        //void render(const Scene& scene, const class Camera& camera);

        
        // Render with external transform system
        void render(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager);
        
        // Scene validation and debugging
        bool validate_scene_resources(const Scene& scene, CoroutineResourceManager& resource_manager) const;
        
        // Deferred rendering control
        void set_deferred_rendering(bool enable);
        bool is_deferred_rendering_enabled() const { return use_deferred_rendering_; }
        void render_deferred(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager);
        void render_deferred(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager);
        
        // G-Buffer debug visualization
        void render_gbuffer_debug(int debug_mode, const CoroutineResourceManager& resource_manager);
        
        // Light visualization
        void render_light_spheres(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager);
        
        // Skybox rendering
        void render_skybox(const Camera& camera, const CoroutineResourceManager& resource_manager);
        

        
        // Plane reflection rendering
        void render_plane_reflection(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager);
        
        // SSGI rendering
        void SSGI_render(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager);
        void set_ssgi_enabled(bool enable);
        bool is_ssgi_enabled() const { return use_ssgi_; }
        
        // SSGI pipeline functions
        void render_direct_lighting_pass(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager);
        void render_composition_pass(const Scene& scene, const Camera& camera, const CoroutineResourceManager& resource_manager);

    private:
        
        std::unique_ptr<ShadowMap> shadow_map;
        
        int width_;
        int height_;
        
        // Forward rendering framebuffer
        GLuint framebuffer_;
        GLuint color_texture_;
        GLuint depth_texture_;
        bool use_framebuffer_;
        
        // G-Buffer for deferred rendering
        GLuint g_buffer_fbo_;
        GLuint g_position_texture_;     // RT0: World Position (xyz) + Material ID (w)
        GLuint g_albedo_metallic_texture_;  // RT1: Albedo (rgb) + Metallic (a)
        GLuint g_normal_roughness_texture_; // RT2: Normal (xyz) + Roughness (a)
        GLuint g_motion_ao_texture_;    // RT3: Motion Vector (xy) + AO (z) + unused (w)
        GLuint g_emissive_texture_;     // RT4: Emissive Color (rgb) + intensity (a)
        GLuint g_depth_texture_;        // Depth buffer for G-Buffer
        bool use_deferred_rendering_;
        
        // Shadow light configuration - consistent across shadow pass and lighting pass
        glm::vec3 shadow_light_pos_;
        glm::vec3 shadow_light_target_;
        
        // Screen-space quad for deferred lighting
        GLuint screen_quad_vao_;
        GLuint screen_quad_vbo_;
        
        // Skybox rendering
        GLuint skybox_vao_;
        GLuint skybox_vbo_;
        
        // SSGI framebuffers and textures
        GLuint ssgi_fbo_;
        GLuint ssgi_raw_texture_;       // Raw noisy SSGI output
        GLuint ssgi_final_texture_;     // Denoised SSGI output
        GLuint lit_scene_texture_;      // Direct lighting only 
        bool use_ssgi_;
        
        // Shadow mapping
        void render_shadow_pass();
        void render_shadow_pass_deferred(const Scene& scene, const CoroutineResourceManager& resource_manager, const TransformManager& transform_manager);
                
        // Framebuffer methods
        void setup_framebuffer();
        void cleanup_framebuffer();
        
        // G-Buffer methods
        void setup_g_buffer();
        void cleanup_g_buffer();
        void bind_g_buffer_for_geometry_pass();
        void bind_g_buffer_for_lighting_pass();
        
        // Screen-space quad for lighting pass
        void setup_screen_quad();
        void cleanup_screen_quad();
        void render_screen_quad();
        
        // Skybox methods
        void setup_skybox();
        void cleanup_skybox();
        
        // SSGI methods
        void setup_ssgi();
        void cleanup_ssgi();
        void setup_ssgi_textures();
        void cleanup_ssgi_textures();
        
    };
}