#include "Texture.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <glm/glm.hpp>

// Static member definitions
unsigned int Texture::current_slot_counter_ = 0;

Texture::Texture() : texture_id_(0), width_(0), height_(0), nr_channels_(0), is_hdr_(false) {
    glGenTextures(1, &texture_id_);
}

Texture::Texture(Texture&& another) noexcept: texture_id_(another.texture_id_), width_(another.width_), height_(another.height_), nr_channels_(another.nr_channels_), is_hdr_(another.is_hdr_) {
    another.texture_id_ = 0;
    another.width_ = 0;
    another.height_ = 0;
    another.nr_channels_ = 0;
    another.is_hdr_ = false;
}

Texture& Texture::operator=(Texture&& another) noexcept {
    if (&another != this) {
        if (texture_id_ != 0) {
            glDeleteTextures(1, &texture_id_);
        }
        
        texture_id_ = another.texture_id_;
        width_ = another.width_;
        height_ = another.height_;
        nr_channels_ = another.nr_channels_;
        is_hdr_ = another.is_hdr_;
        
        another.texture_id_ = 0;
        another.width_ = 0;
        another.height_ = 0;
        another.nr_channels_ = 0;
        another.is_hdr_ = false;
    }
    return *this;
}

Texture::~Texture() {
    if (texture_id_ != 0) {
        glDeleteTextures(1, &texture_id_);
    }
}

bool Texture::operator==(const Texture& other) const noexcept {
    return texture_id_ == other.texture_id_;
}

bool Texture::operator!=(const Texture& other) const noexcept {
    return !(*this == other);
}

void Texture::load_from_file(const std::string& path) {
    glRenderer::STBImage::set_flip_vertical_on_load(true);
    
    int imgWidth, imgHeight, imgChannels;
    unsigned char* data = glRenderer::STBImage::load_image(path.c_str(), &imgWidth, &imgHeight, &imgChannels, 0);
    
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        glRenderer::STBImage::free_image(data);
        return;
    }
    
    this->width_ = static_cast<GLuint>(imgWidth);
    this->height_ = static_cast<GLuint>(imgHeight);
    this->nr_channels_ = static_cast<GLuint>(imgChannels);
    
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    
    GLenum format;
    if (nr_channels_ == 1)
        format = GL_RED;
    else if (nr_channels_ == 3)
        format = GL_RGB;
    else if (nr_channels_ == 4)
        format = GL_RGBA;
    else {
        std::cerr << "Unsupported number of channels: " << nr_channels_ << std::endl;
        glRenderer::STBImage::free_image(data);
        return;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, width_, height_, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glRenderer::STBImage::free_image(data);
    
    std::cout << "Successfully loaded texture: " << path << " (" << width_ << "x" << height_ << ", " << nr_channels_ << " channels)" << std::endl;
}

void Texture::load_from_data(unsigned char* data, int width, int height, int channels) {
    width_ = width;
    height_ = height;
    nr_channels_ = channels;

    glBindTexture(GL_TEXTURE_2D, texture_id_);

    GLenum format;
    if (channels == 1) {
        format = GL_RED;
    } else if (channels == 3) {
        format = GL_RGB;
    } else if (channels == 4) {
        format = GL_RGBA;
    } else {
        std::cerr << "Texture: Unsupported number of channels: " << channels << std::endl;
        return;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Texture::load_cubemap_from_files(const std::vector<std::string>& faces) {
    if (faces.size() != 6) {
        std::cerr << "Cubemap requires exactly 6 faces, got " << faces.size() << std::endl;
        return;
    }
    
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id_);
    
    // Don't flip images for cubemap
    glRenderer::STBImage::set_flip_vertical_on_load(false);
    
    for (unsigned int i = 0; i < faces.size(); i++) {
        int imgWidth, imgHeight, imgChannels;
        unsigned char* data = glRenderer::STBImage::load_image(faces[i].c_str(), &imgWidth, &imgHeight, &imgChannels, 0);
        
        if (data) {
            GLenum format;
            if (imgChannels == 1)
                format = GL_RED;
            else if (imgChannels == 3)
                format = GL_RGB;
            else if (imgChannels == 4)
                format = GL_RGBA;
            else {
                std::cerr << "Unsupported number of channels in " << faces[i] << ": " << imgChannels << std::endl;
                glRenderer::STBImage::free_image(data);
                continue;
            }
            
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, imgWidth, imgHeight, 0, format, GL_UNSIGNED_BYTE, data);
            glRenderer::STBImage::free_image(data);
            
            // Store dimensions from first face
            if (i == 0) {
                this->width_ = static_cast<GLuint>(imgWidth);
                this->height_ = static_cast<GLuint>(imgHeight);
                this->nr_channels_ = static_cast<GLuint>(imgChannels);
            }
            
            std::cout << "Loaded cubemap face " << i << ": " << faces[i] << " (" << imgWidth << "x" << imgHeight << ")" << std::endl;
        } else {
            std::cerr << "Failed to load cubemap texture: " << faces[i] << std::endl;
        }
    }
    
    // Set cubemap parameters
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    // Generate mipmaps for smooth filtering
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    
    std::cout << "Successfully loaded cubemap with " << faces.size() << " faces" << std::endl;
}

void Texture::gen_depth_texture(GLuint width, const GLuint height) {
    width_ = width;
    height_ = height;
    nr_channels_ = 1;

    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
}

void Texture::gen_depth_cube_map(GLuint size) {
    width_ = size;
    height_ = size;
    nr_channels_ = 1;

    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id_);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

// Simple automatic texture binding
unsigned int Texture::bind_auto() {
    if (texture_id_ == 0) {
        std::cerr << "Texture: Cannot bind invalid texture (ID = 0)" << std::endl;
        return INVALID_SLOT;
    }
    
    unsigned int slot = get_next_slot();
    if (slot == INVALID_SLOT) {
        return INVALID_SLOT;
    }
    
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    return slot;
}

unsigned int Texture::bind_cubemap_auto() {
    if (texture_id_ == 0) {
        std::cerr << "Texture: Cannot bind invalid cubemap texture (ID = 0)" << std::endl;
        return INVALID_SLOT;
    }
    
    unsigned int slot = get_next_slot();
    if (slot == INVALID_SLOT) {
        return INVALID_SLOT;
    }
    
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id_);
    return slot;
}

// Legacy binding methods (manual slot specification, no tracking)
void Texture::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
}

void Texture::bind_cube_map(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id_);
}

unsigned int Texture::get_id() const {
    return texture_id_;
}

// Simple sequential slot allocation
unsigned int Texture::get_next_slot() {
    if (current_slot_counter_ >= MAX_TEXTURE_UNITS) {
        std::cerr << "Texture: All " << MAX_TEXTURE_UNITS << " texture slots are occupied" << std::endl;
        return INVALID_SLOT;
    }
    return current_slot_counter_++;
}

void Texture::reset_slot_counter() {
    current_slot_counter_ = 0;
}

void Texture::unbind_all_textures() {
    // Unbind all texture slots by setting them to 0
    for (unsigned int slot = 0; slot < MAX_TEXTURE_UNITS; ++slot) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }
    // Reset active texture to slot 0
    glActiveTexture(GL_TEXTURE0);
    // Reset the slot counter
    current_slot_counter_ = 0;
}

// Static methods for binding raw OpenGL texture IDs (for renderer internal use)
unsigned int Texture::bind_raw_texture(GLuint texture_id, GLenum target) {
    if (texture_id == 0) {
        std::cerr << "Texture: Cannot bind invalid texture ID (0)" << std::endl;
        return INVALID_SLOT;
    }
    
    unsigned int slot = get_next_slot();
    if (slot == INVALID_SLOT) {
        return INVALID_SLOT;
    }
    
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(target, texture_id);
    return slot;
}

// HDR/EXR loading implementations
void Texture::load_hdr_from_file(const std::string& path) {
    int imgWidth, imgHeight, imgChannels;
    float* data = glRenderer::STBImage::load_hdr_image(path.c_str(), &imgWidth, &imgHeight, &imgChannels, 0);
    
    if (!data) {
        std::cerr << "Failed to load HDR texture: " << path << std::endl;
        return;
    }
    
    this->width_ = static_cast<GLuint>(imgWidth);
    this->height_ = static_cast<GLuint>(imgHeight);
    this->nr_channels_ = static_cast<GLuint>(imgChannels);
    this->is_hdr_ = true;
    
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    
    // Use floating-point internal format for HDR
    GLenum internal_format, format;
    if (nr_channels_ == 1) {
        internal_format = GL_R16F;
        format = GL_RED;
    } else if (nr_channels_ == 3) {
        internal_format = GL_RGB16F;
        format = GL_RGB;
    } else if (nr_channels_ == 4) {
        internal_format = GL_RGBA16F;
        format = GL_RGBA;
    } else {
        std::cerr << "Unsupported number of channels for HDR: " << nr_channels_ << std::endl;
        glRenderer::STBImage::free_hdr_image(data);
        return;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width_, height_, 0, format, GL_FLOAT, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // Set texture parameters suitable for HDR
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glRenderer::STBImage::free_hdr_image(data);
    
    std::cout << "Successfully loaded HDR texture: " << path << " (" << width_ << "x" << height_ << ", " << nr_channels_ << " channels)" << std::endl;
}

void Texture::load_exr_from_file(const std::string& path) {
    int imgWidth, imgHeight, imgChannels;
    float* data = glRenderer::STBImage::load_exr_image(path.c_str(), &imgWidth, &imgHeight, &imgChannels);
    
    if (!data) {
        std::cerr << "Failed to load EXR texture: " << path << std::endl;
        return;
    }
    
    this->width_ = static_cast<GLuint>(imgWidth);
    this->height_ = static_cast<GLuint>(imgHeight);
    this->nr_channels_ = static_cast<GLuint>(imgChannels);
    this->is_hdr_ = true;
    
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    
    // EXR can have different channel counts, handle accordingly
    GLenum format, internal_format;
    if (nr_channels_ == 1) {
        format = GL_RED;
        internal_format = GL_R16F;
    } else if (nr_channels_ == 3) {
        format = GL_RGB;
        internal_format = GL_RGB16F;
    } else if (nr_channels_ == 4) {
        format = GL_RGBA;
        internal_format = GL_RGBA16F;
    } else {
        std::cerr << "Unsupported EXR channel count: " << nr_channels_ << std::endl;
        glRenderer::STBImage::free_exr_image(data);
        return;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width_, height_, 0, format, GL_FLOAT, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // Set texture parameters suitable for HDR
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glRenderer::STBImage::free_exr_image(data);
    
    std::cout << "Successfully loaded EXR texture: " << path << " (" << width_ << "x" << height_ << ", " << nr_channels_ << " channels)" << std::endl;
}

void Texture::load_equirectangular_hdr(const std::string& path) {
    // Determine file type and load accordingly
    if (glRenderer::STBImage::is_exr_file(path.c_str())) {
        load_exr_from_file(path);
    } else if (glRenderer::STBImage::is_hdr_file(path.c_str())) {
        load_hdr_from_file(path);
    } else {
        std::cerr << "Unsupported HDR file format: " << path << std::endl;
    }
}

void Texture::convert_equirectangular_to_cubemap(float* hdr_data, int width, int height, int channels) {
    // Create temporary texture for equirectangular map
    GLuint equirectTexture;
    glGenTextures(1, &equirectTexture);
    glBindTexture(GL_TEXTURE_2D, equirectTexture);
    
    GLenum format = (channels == 3) ? GL_RGB : GL_RGBA;
    GLenum internal_format = (channels == 3) ? GL_RGB16F : GL_RGBA16F;
    
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_FLOAT, hdr_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Set up cubemap dimensions 
    const int cubemap_size = 512;
    
    // Configure this texture as a cubemap
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id_);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 
                     cubemap_size, cubemap_size, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Generate mipmaps for smooth filtering
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    
    // Store cubemap dimensions
    width_ = cubemap_size;
    height_ = cubemap_size;
    nr_channels_ = 3; // RGB for cubemap
    is_hdr_ = true;   // Cubemap converted from HDR
    
    // Clean up temporary texture
    glDeleteTextures(1, &equirectTexture);
    
    std::cout << "Converted equirectangular HDR to cubemap: " << cubemap_size << "x" << cubemap_size << std::endl;
}

void Texture::load_hdr_cubemap_from_equirectangular(const std::string& path) {
    // Load HDR/EXR data first
    int imgWidth, imgHeight, imgChannels;
    float* data = nullptr;
    
    if (glRenderer::STBImage::is_exr_file(path.c_str())) {
        data = glRenderer::STBImage::load_exr_image(path.c_str(), &imgWidth, &imgHeight, &imgChannels);
    } else if (glRenderer::STBImage::is_hdr_file(path.c_str())) {
        data = glRenderer::STBImage::load_hdr_image(path.c_str(), &imgWidth, &imgHeight, &imgChannels, 0);
    } else {
        std::cerr << "Unsupported HDR file format for cubemap conversion: " << path << std::endl;
        return;
    }
    
    if (!data) {
        std::cerr << "Failed to load HDR data for cubemap conversion: " << path << std::endl;
        return;
    }
    
    // Convert to cubemap
    convert_equirectangular_to_cubemap(data, imgWidth, imgHeight, imgChannels);
    
    // Free the loaded data
    if (glRenderer::STBImage::is_exr_file(path.c_str())) {
        glRenderer::STBImage::free_exr_image(data);
    } else {
        glRenderer::STBImage::free_hdr_image(data);
    }
    
    std::cout << "Successfully loaded HDR cubemap from equirectangular: " << path << std::endl;
}

// Factory methods for different texture types
Texture Texture::create_color_texture(GLuint width, GLuint height, GLenum internal_format, GLenum format, GLenum type) {
    Texture texture;
    texture.width_ = width;
    texture.height_ = height;
    texture.nr_channels_ = (format == GL_RGB) ? 3 : (format == GL_RGBA) ? 4 : 1;
    
    glBindTexture(GL_TEXTURE_2D, texture.texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);
    
    // Set default parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return texture;
}

Texture Texture::create_depth_texture(GLuint width, GLuint height, GLenum internal_format) {
    Texture texture;
    texture.width_ = width;
    texture.height_ = height;
    texture.nr_channels_ = 1;
    
    glBindTexture(GL_TEXTURE_2D, texture.texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    
    // Set depth texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    
    float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
    
    return texture;
}

Texture Texture::create_framebuffer_texture(GLuint width, GLuint height, GLenum internal_format, GLenum format, GLenum type, bool generate_mipmaps) {
    Texture texture;
    texture.width_ = width;
    texture.height_ = height;
    texture.nr_channels_ = (format == GL_RGB) ? 3 : (format == GL_RGBA) ? 4 : 1;
    
    glBindTexture(GL_TEXTURE_2D, texture.texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);
    
    if (generate_mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return texture;
}

Texture Texture::create_noise_texture(GLuint width, GLuint height, const std::vector<float>& noise_data) {
    Texture texture;
    texture.width_ = width;
    texture.height_ = height;
    texture.nr_channels_ = 3; // Assuming RGB noise data
    
    glBindTexture(GL_TEXTURE_2D, texture.texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGB, GL_FLOAT, noise_data.data());
    
    // Set noise texture parameters (no filtering, repeat wrapping)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    return texture;
}

Texture Texture::create_g_buffer_texture(GLuint width, GLuint height, GLenum internal_format, GLenum format, GLenum type) {
    Texture texture;
    texture.width_ = width;
    texture.height_ = height;
    texture.nr_channels_ = (format == GL_RGB) ? 3 : (format == GL_RGBA) ? 4 : 1;
    
    glBindTexture(GL_TEXTURE_2D, texture.texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, type, nullptr);
    
    // G-Buffer textures typically use nearest filtering for precision
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return texture;
}

// Texture configuration methods
void Texture::set_filter_mode(GLenum min_filter, GLenum mag_filter) {
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
}

void Texture::set_wrap_mode(GLenum wrap_s, GLenum wrap_t, GLenum wrap_r) {
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
    // Note: GL_TEXTURE_WRAP_R is only applicable to 3D textures and cubemaps
    // For 2D textures, we don't set WRAP_R parameter
}

void Texture::set_border_color(const float* border_color) {
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);
}

void Texture::resize_texture(GLuint new_width, GLuint new_height, GLenum internal_format, GLenum format, GLenum type) {
    width_ = new_width;
    height_ = new_height;
    
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, new_width, new_height, 0, format, type, nullptr);
}

// Generic texture creation method using abstraction structure
Texture Texture::create_texture(const TextureCreateInfo& create_info) {
    Texture texture;
    texture.width_ = create_info.width;
    texture.height_ = create_info.height;
    // Determine number of channels based on format
    if (create_info.format == GL_RED || create_info.format == GL_DEPTH_COMPONENT) {
        texture.nr_channels_ = 1;
    } else if (create_info.format == GL_RG) {
        texture.nr_channels_ = 2;
    } else if (create_info.format == GL_RGB) {
        texture.nr_channels_ = 3;
    } else if (create_info.format == GL_RGBA) {
        texture.nr_channels_ = 4;
    } else {
        texture.nr_channels_ = 1; // Default fallback
    }
    
    glBindTexture(GL_TEXTURE_2D, texture.texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, create_info.internal_format, create_info.width, create_info.height, 
                 0, create_info.format, create_info.type, create_info.data);
    
    if (create_info.generate_mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, create_info.min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, create_info.mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, create_info.wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, create_info.wrap_t);
    
    return texture;
}

// Convenience methods for common texture types
Texture Texture::create_render_target(GLuint width, GLuint height, bool hdr) {
    TextureCreateInfo create_info{};
    create_info.width = width;
    create_info.height = height;
    
    if (hdr) {
        create_info.internal_format = GL_RGBA16F;
        create_info.format = GL_RGBA;
        create_info.type = GL_FLOAT;
    } else {
        create_info.internal_format = GL_RGBA8;
        create_info.format = GL_RGBA;
        create_info.type = GL_UNSIGNED_BYTE;
    }
    
    create_info.min_filter = GL_LINEAR;
    create_info.mag_filter = GL_LINEAR;
    create_info.wrap_s = GL_CLAMP_TO_EDGE;
    create_info.wrap_t = GL_CLAMP_TO_EDGE;
    
    Texture texture = create_texture(create_info);
    texture.set_hdr(hdr);
    return texture;
}

Texture Texture::create_depth_buffer(GLuint width, GLuint height) {
    TextureCreateInfo create_info{};
    create_info.width = width;
    create_info.height = height;
    create_info.internal_format = GL_DEPTH_COMPONENT24;
    create_info.format = GL_DEPTH_COMPONENT;
    create_info.type = GL_FLOAT;
    create_info.min_filter = GL_NEAREST;
    create_info.mag_filter = GL_NEAREST;
    create_info.wrap_s = GL_CLAMP_TO_EDGE;
    create_info.wrap_t = GL_CLAMP_TO_EDGE;
    
    return create_texture(create_info);
}

Texture Texture::create_shadow_depth_buffer(GLuint width, GLuint height) {
    TextureCreateInfo create_info{};
    create_info.width = width;
    create_info.height = height;
    create_info.internal_format = GL_DEPTH_COMPONENT24;
    create_info.format = GL_DEPTH_COMPONENT;
    create_info.type = GL_FLOAT;
    create_info.min_filter = GL_NEAREST;
    create_info.mag_filter = GL_NEAREST;
    create_info.wrap_s = GL_CLAMP_TO_BORDER;
    create_info.wrap_t = GL_CLAMP_TO_BORDER;
    
    Texture texture = create_texture(create_info);
    
    // Set border color for shadow depth texture
    float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
    texture.set_border_color(border_color);
    
    return texture;
}

Texture Texture::create_ssao_noise_texture() {
    // Generate 4x4 noise texture with random vectors
    std::vector<float> ssao_noise_data;
    std::uniform_real_distribution<float> random_floats(0.0, 1.0);
    std::default_random_engine generator;
    
    for (unsigned int i = 0; i < 16; i++) {
        // Generate noise vector and store as separate float values
        ssao_noise_data.push_back(random_floats(generator) * 2.0 - 1.0); // x
        ssao_noise_data.push_back(random_floats(generator) * 2.0 - 1.0); // y
        ssao_noise_data.push_back(0.0f); // z
    }
    
    TextureCreateInfo create_info{};
    create_info.width = 4;
    create_info.height = 4;
    create_info.internal_format = GL_RGBA16F;
    create_info.format = GL_RGB;
    create_info.type = GL_FLOAT;
    create_info.min_filter = GL_NEAREST;
    create_info.mag_filter = GL_NEAREST;
    create_info.wrap_s = GL_REPEAT;
    create_info.wrap_t = GL_REPEAT;
    create_info.data = ssao_noise_data.data();
    
    return create_texture(create_info);
}