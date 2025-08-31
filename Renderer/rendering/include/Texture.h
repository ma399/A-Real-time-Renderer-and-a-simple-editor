#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include "STBImage.h"

// Forward declarations for future graphics API abstraction
enum class GraphicsAPI {
    OpenGL,
    Vulkan
};

// Texture creation parameters structure for API abstraction
struct TextureCreateInfo {
    GLuint width;
    GLuint height;
    GLenum internal_format;
    GLenum format;
    GLenum type;
    GLenum min_filter = GL_LINEAR;
    GLenum mag_filter = GL_LINEAR;
    GLenum wrap_s = GL_CLAMP_TO_EDGE;
    GLenum wrap_t = GL_CLAMP_TO_EDGE;
    bool generate_mipmaps = false;
    const void* data = nullptr;
};

class Texture {
public:
    static const unsigned int MAX_TEXTURE_UNITS = 16;
    static const unsigned int INVALID_SLOT = MAX_TEXTURE_UNITS;
    
    Texture();
    ~Texture();

    Texture(Texture&) = delete;
    Texture(Texture&&) noexcept;
    Texture& operator=(const Texture&) = delete;
    Texture& operator=(Texture&&) noexcept;
    
    bool operator==(const Texture& other) const noexcept;
    bool operator!=(const Texture& other) const noexcept;

    // Texture loading methods
    void load_from_file(const std::string& path);
    void load_from_data(unsigned char* data, int width, int height, int channels);
    void load_cubemap_from_files(const std::vector<std::string>& faces);
    void gen_depth_texture(GLuint width, const GLuint height);
    void gen_depth_cube_map(GLuint size);
    
    // HDR/EXR loading methods
    void load_hdr_from_file(const std::string& path);
    void load_exr_from_file(const std::string& path);
    void load_equirectangular_hdr(const std::string& path);
    void load_hdr_cubemap_from_equirectangular(const std::string& path);
    void convert_equirectangular_to_cubemap(float* hdr_data, int width, int height, int channels);
    
    // Factory methods for different texture types (for Vulkan abstraction)
    static Texture create_color_texture(GLuint width, GLuint height, GLenum internal_format = GL_RGB, GLenum format = GL_RGB, GLenum type = GL_UNSIGNED_BYTE);
    static Texture create_depth_texture(GLuint width, GLuint height, GLenum internal_format = GL_DEPTH_COMPONENT24);
    static Texture create_framebuffer_texture(GLuint width, GLuint height, GLenum internal_format, GLenum format, GLenum type, bool generate_mipmaps = false);
    static Texture create_noise_texture(GLuint width, GLuint height, const std::vector<float>& noise_data);
    static Texture create_g_buffer_texture(GLuint width, GLuint height, GLenum internal_format, GLenum format, GLenum type);
    
    // Generic texture creation method using abstraction structure
    static Texture create_texture(const TextureCreateInfo& create_info);
    
    // Convenience methods for common texture types
    static Texture create_render_target(GLuint width, GLuint height, bool hdr = false);
    static Texture create_depth_buffer(GLuint width, GLuint height);
    static Texture create_shadow_depth_buffer(GLuint width, GLuint height);
    static Texture create_ssao_noise_texture();
    
    // Texture configuration methods
    void set_filter_mode(GLenum min_filter, GLenum mag_filter);
    void set_wrap_mode(GLenum wrap_s, GLenum wrap_t, GLenum wrap_r = GL_REPEAT);
    void set_border_color(const float* border_color);
    void resize_texture(GLuint new_width, GLuint new_height, GLenum internal_format, GLenum format, GLenum type);
    
    // Simple automatic texture binding
    unsigned int bind_auto();
    unsigned int bind_cubemap_auto();
    
    // Legacy binding methods (manual slot specification, no tracking)
    void bind(unsigned int slot = 0) const;
    void bind_cube_map(unsigned int slot = 0) const;
    
    // Utility methods
    unsigned int get_id() const;
    GLuint get_width() const { return width_; }
    GLuint get_height() const { return height_; }
    GLuint get_channels() const { return nr_channels_; }
    bool is_hdr() const { return is_hdr_; }
    
    // Setter methods for internal use
    void set_dimensions(GLuint width, GLuint height) { width_ = width; height_ = height; }
    void set_channels(GLuint channels) { nr_channels_ = channels; }
    void set_hdr(bool is_hdr) { is_hdr_ = is_hdr; }
    
    // Simple sequential slot allocation
    static unsigned int get_next_slot();
    static void reset_slot_counter();
    static void unbind_all_textures();
    

    
    // Static methods for binding raw OpenGL texture IDs (for renderer internal use)
    static unsigned int bind_raw_texture(GLuint texture_id, GLenum target = GL_TEXTURE_2D);

private:
    GLuint texture_id_ = 0;
    GLuint width_, height_, nr_channels_;
    bool is_hdr_ = false;
    
    // Static slot counter for sequential allocation
    static unsigned int current_slot_counter_;
    
    // Internal binding method
    void bind_internal(unsigned int slot, bool is_cubemap);

}; 