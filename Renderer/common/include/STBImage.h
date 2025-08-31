#pragma once

#include <cstring>
#include <cstdlib>

// Forward declarations for STB functions to avoid including headers
extern "C" {
    typedef unsigned char stbi_uc;
    
    // STB Image loading functions
    stbi_uc *stbi_load(char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);
    void stbi_image_free(void *retval_from_stbi_load);
    
    // STB Image writing functions  
    int stbi_write_png(char const *filename, int w, int h, int comp, const void *data, int stride_in_bytes);
    int stbi_write_jpg(char const *filename, int w, int h, int comp, const void *data, int quality);
    int stbi_write_bmp(char const *filename, int w, int h, int comp, const void *data);
    int stbi_write_tga(char const *filename, int w, int h, int comp, const void *data);
    
    // STB Image utility functions
    void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);
    float *stbi_loadf(char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);
    int stbi_is_hdr(char const *filename);
}

namespace glRenderer {
    
class STBImage {
public:
    // Standard LDR image loading
    static unsigned char* load_image(const char* filename, int* width, int* height, int* nr_channels, int desired_channels = 0);
    static void free_image(unsigned char* data);
    static bool write_image(const char* filename, int width, int height, int components, const void* data);
    static void set_flip_vertical_on_load(bool flip);
    
    // HDR/EXR image loading
    static float* load_hdr_image(const char* filename, int* width, int* height, int* nr_channels, int desired_channels = 0);
    static float* load_exr_image(const char* filename, int* width, int* height, int* nr_channels);
    static void free_hdr_image(float* data);
    static void free_exr_image(float* data);
    static bool is_hdr_file(const char* filename);
    static bool is_exr_file(const char* filename);
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

// HDR/EXR image loading implementations
inline float* STBImage::load_hdr_image(const char* filename, int* width, int* height, int* nr_channels, int desired_channels) {
    return stbi_loadf(filename, width, height, nr_channels, desired_channels);
}

inline void STBImage::free_hdr_image(float* data) {
    stbi_image_free(data);
}

inline bool STBImage::is_hdr_file(const char* filename) {
    return stbi_is_hdr(filename);
}

inline bool STBImage::is_exr_file(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcmp(ext, ".exr") == 0 || strcmp(ext, ".EXR") == 0);
}

} // namespace glRenderer
