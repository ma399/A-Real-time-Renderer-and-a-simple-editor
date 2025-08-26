#include "Texture.h"
#include <iostream>
#include <vector>

Texture::Texture() : texture_id_(0), width_(0), height_(0), nr_channels_(0) {
    glGenTextures(1, &texture_id_);
}

Texture::Texture(Texture&& another) noexcept: texture_id_(another.texture_id_), width_(another.width_), height_(another.height_), nr_channels_(another.nr_channels_){
    another.texture_id_ = 0;
    another.width_ = 0;
    another.height_ = 0;
    another.nr_channels_ = 0;
}

Texture& Texture::operator=(Texture&& another) noexcept {
    if (another != *this) {
        if (texture_id_ != 0) {
            glDeleteTextures(1, &texture_id_);
        }
        this->texture_id_ = another.texture_id_;
        this->width_ = another.width_;
        this->height_ = another.height_;
        this->nr_channels_ = another.nr_channels_;

        another.texture_id_ = 0;
        another.width_ = 0;
        another.height_ = 0;
        another.nr_channels_ = 0;
    }

    return *this;
}

bool Texture::operator==(const Texture& another) const noexcept {
    return this->texture_id_ == another.texture_id_;
}

bool Texture::operator!=(const Texture& another) const noexcept {
    return this->texture_id_ != another.texture_id_;
}

Texture::~Texture() {
    if (texture_id_ != 0) {
        glDeleteTextures(1, &texture_id_);
    }
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
    if (!data) {
        std::cerr << "Invalid texture data provided" << std::endl;
        return;
    }
    
    this->width_ = static_cast<GLuint>(width);
    this->height_ = static_cast<GLuint>(height);
    this->nr_channels_ = static_cast<GLuint>(channels);
    
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    
    GLenum format;
    if (channels == 1)
        format = GL_RED;
    else if (channels == 3)
        format = GL_RGB;
    else if (channels == 4)
        format = GL_RGBA;
    else {
        std::cerr << "Unsupported number of channels: " << channels << std::endl;
        return;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    std::cout << "Successfully loaded texture from data: " << width << "x" << height << ", " << channels << " channels" << std::endl;
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
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    std::cout << "Successfully loaded cubemap with " << faces.size() << " faces" << std::endl;
}

void Texture::gen_depth_texture(GLuint width, const GLuint height) {
    glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    this->width_ = width;
    this->height_ = height;
    this->nr_channels_ = 1;
}

void Texture::gen_depth_cube_map(GLuint size) {
    glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id_);
    
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                     size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    this->width_ = size;
    this->height_ = size;
    this->nr_channels_ = 1;
}

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