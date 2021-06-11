#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>


#include "ImageData.h"

ImageData::ImageData(const std::string& filename) : ImageData() {
    // since we read as RGBA
    desired_channels = STBI_rgb_alpha;
    pixels = stbi_load(filename.c_str(), &width, &height, &original_channels, desired_channels);
    size = width * height * desired_channels;

    if (!pixels) {
        throw std::runtime_error("Failed to load image: " + filename + "!\n");
    }
};

ImageData::ImageData() : width(0), height(0), original_channels(0), desired_channels(0), level(0), pixels(nullptr), size(0) {
   
};

ImageData::ImageData(const ImageData& to_copy) : width(to_copy.width), height(to_copy.height), original_channels(to_copy.original_channels), 
    desired_channels(to_copy.desired_channels), level(to_copy.level), pixels(nullptr), size(0) {

}

bool ImageData::save(const std::string& filename) {
    int bytes_written = stbi_write_jpg(filename.c_str(), width, height, desired_channels, pixels, /*quality=*/100);
    return bytes_written != 0;
};

std::string ImageData::print() const {
    std::string info{ "" };
    info.append("level: " + std::to_string(level) + "\tsize: " + std::to_string(width) + " x " + std::to_string(height));
    return info;
}

ImageData& ImageData::operator= (const ImageData& rhs) {
    // Prevent self assignation
    if (this == &rhs) {
        return *this;
    }
    
    width = rhs.width; 
    height = rhs.height; 
    original_channels = rhs.original_channels;
    desired_channels = rhs.desired_channels;
    level = rhs.level; 
    // In the remote case we had previous memmory
    if (pixels != nullptr) {
        stbi_image_free(pixels);
    }
    pixels = nullptr;
    size = 0;

    return *this;
}

ImageData::~ImageData() { 
    if (pixels != nullptr) {
        stbi_image_free(pixels);
    }
};