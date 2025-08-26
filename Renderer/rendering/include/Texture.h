#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>
#include "STBImage.h"

class Texture {
public:
    Texture();
 
    ~Texture();

    Texture(Texture&) = delete;
    Texture(Texture&&) noexcept;
    Texture& operator=(const Texture&) = delete;
    Texture& operator=(Texture&&) noexcept;
    
    bool operator==(const Texture& other) const noexcept;
    bool operator!=(const Texture& other) const noexcept;

    void load_from_file(const std::string& path);
    void load_from_data(unsigned char* data, int width, int height, int channels);
    void load_cubemap_from_files(const std::vector<std::string>& faces);
    void gen_depth_texture(GLuint width, const GLuint height);
    void gen_depth_cube_map(GLuint size);
    void bind(unsigned int slot = 0) const;
    void bind_cube_map(unsigned int slot = 0) const;
    unsigned int get_id() const;

private:
    GLuint texture_id_ = 0;
    GLuint width_, height_, nr_channels_;

}; 