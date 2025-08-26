#pragma once

#include <stb_image.h>
#include <stb_image_write.h>
#include <cstring>

namespace glRenderer {
    
class STBImage {
public:
    static unsigned char* load_image(const char* filename, int* width, int* height, int* nr_channels, int desired_channels = 0);
    static void free_image(unsigned char* data);
    static bool write_image(const char* filename, int width, int height, int components, const void* data);
    static void set_flip_vertical_on_load(bool flip);
};

inline unsigned char* STBImage::load_image(const char* filename, int* width, int* height, int* nr_channels, int desired_channels) {
    return stbi_load(filename, width, height, nr_channels, desired_channels);
}

inline void STBImage::free_image(unsigned char* data) {
    stbi_image_free(data);
}

inline bool STBImage::write_image(const char* filename, int width, int height, int components, const void* data) {
    // Determine file format from extension
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    if (strcmp(ext, ".png") == 0) {
        return stbi_write_png(filename, width, height, components, data, width * components);
    } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
        return stbi_write_jpg(filename, width, height, components, data, 90);
    } else if (strcmp(ext, ".bmp") == 0) {
        return stbi_write_bmp(filename, width, height, components, data);
    } else if (strcmp(ext, ".tga") == 0) {
        return stbi_write_tga(filename, width, height, components, data);
    }
    
    return false;
}

inline void STBImage::set_flip_vertical_on_load(bool flip) {
    stbi_set_flip_vertically_on_load(flip);
}

} // namespace glRenderer
