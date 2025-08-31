// STBImage.cpp - Implementation file for STB libraries
// This file should be the ONLY place where STB implementations are defined

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

// Include STB headers with implementations
#include <stb_image.h>
#include <stb_image_write.h>

// TinyEXR is linked as a library, no need for implementation here
#include <tinyexr.h>

#include "STBImage.h"
#include <cstdlib>

// This file provides the implementation of stb_image and TinyEXR
// Most wrapper functions are defined as inline in STBImage.h
// EXR-specific functions are implemented here

namespace glRenderer {

float* STBImage::load_exr_image(const char* filename, int* width, int* height, int* nr_channels) {
    float* image_data = nullptr;
    const char* error_msg = nullptr;
    
    int ret = LoadEXR(&image_data, width, height, filename, &error_msg);
    if (ret != TINYEXR_SUCCESS) {
        if (error_msg) {
            FreeEXRErrorMessage(error_msg);
        }
        return nullptr;
    }
    
    // EXR always loads as RGBA (4 channels)
    *nr_channels = 4;
    return image_data;
}

void STBImage::free_exr_image(float* data) {
    free(data); // TinyEXR uses malloc, so we use free
}

} // namespace glRenderer
