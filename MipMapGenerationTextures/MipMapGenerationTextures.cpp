#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>

#include <string>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>

#include "ImageData.h"
#include "GPUMipMapGeneration.h"


int calculate_max_mipmap_level(const int& width, const int& height);
void print_levels(const ImageData& img);
bool resize_cpu(const ImageData& src_image, ImageData& dst_image);

int main(int argc, char* argv[]) {
    // Path of the input  image file
    const std::string image_file{ "textures/countryside.jpg" };
    std::cout << "Reading file: " << image_file << std::endl;
    // Load input image from disk
    ImageData input{ image_file };
    // Print input's info
    std::cout << "Input's info " << std::endl;
    std::cout << "width: " << input.width << std::endl;
    std::cout << "heigth: " << input.height << std::endl;
    std::cout << "size: " << input.size << std::endl;
    std::cout << "original channels: " << input.original_channels << std::endl;
    std::cout << "desired channels: " << input.desired_channels << std::endl;
    std::cout << "level: " << input.level << std::endl << std::endl;

    // How many Mipmaps do we need to generate
    const int levels_to_generate = calculate_max_mipmap_level(input.width, input.height);

    // Array of images to store the Mipmaps
    std::vector<ImageData> mip_maps(levels_to_generate);
    std::cout << "There are " << levels_to_generate << " mipmaps to generate..." << std::endl;
    // I need to do a manual copy of the first one (since the assigment operator implements a shallow copy)
    mip_maps[0] = input;
    // deep copy
    mip_maps[0].size = input.size;
    mip_maps[0].pixels = new unsigned char[mip_maps[0].size];
    std::memcpy(mip_maps[0].pixels, input.pixels, mip_maps[0].size);

    /* Calculate the mipmaps for the next levels */
    GPUMipMapGenerator gpuGen;
    const bool use_gpu = true;
    for (unsigned int i = 1; i < static_cast<unsigned int>(levels_to_generate); ++i) {
        // Calculate filename of this level
        const std::string next_level_image_name{ (use_gpu ? "GPU/" : "CPU/") + std::string("countryside_level_") + std::to_string(i) + ".jpg" };

        // Prepare the struct for the new resized image. I. e. calculate the info of the next level
        mip_maps[i].width = mip_maps[i - 1u].width > 1 ? mip_maps[i - 1u].width / 2 : 1;
        mip_maps[i].height = mip_maps[i - 1u].height > 1 ? mip_maps[i - 1u].height / 2 : 1;
        mip_maps[i].level = mip_maps[i - 1u].level + 1;
        mip_maps[i].desired_channels = mip_maps[i - 1u].desired_channels;
        mip_maps[i].original_channels = mip_maps[i - 1u].original_channels;
        // Our desired size once we are scaled
        mip_maps[i].size = mip_maps[i].width * mip_maps[i].height * mip_maps[i].desired_channels;
        // Allocate memory for the new resized image
        mip_maps[i].pixels = new unsigned char[mip_maps[i].size];

        // Resize the image
        if (use_gpu) {
            gpuGen.generateMip(mip_maps[i - 1u], mip_maps[i]);
            // Write the new image to disk
            std::cout << mip_maps[i].print() << std::endl;
            std::wstring fileName(next_level_image_name.c_str());
            std::cout << "Writing file: " << next_level_image_name << (gpuGen.saveResult(next_level_image_name) ? " sucessful!" : " failed!") << std::endl;
        }
        else {
            resize_cpu(mip_maps[i - 1u], mip_maps[i]);
            // Write the new image to disk
            std::cout << mip_maps[i].print() << std::endl;
            std::cout << "Writing file: " << next_level_image_name << (mip_maps[i].save(next_level_image_name) ? " sucessful!" : " failed!") << std::endl;
        }
        
    }

    return EXIT_SUCCESS;
}

int calculate_max_mipmap_level(const int& width, const int& height) {
    int levels{ 0 };
    using std::max;
    if (width > 0 && height > 0) {
        levels = static_cast<int>(std::floor(std::log2(max(width, height)))) + 1;
    }
    else {
        throw std::runtime_error("Invalid dimensions to calculate mipmap levels!");
    }

    return levels;
}

void print_levels(const ImageData& img) {
    int levels = calculate_max_mipmap_level(img.width, img.height);
    std::cout << "We should have " << levels << " levels..." << std::endl;

    int current_width = img.width;
    int current_height = img.height;
    for (int i = 0; i < levels; i++) {
        std::cout << "level: " << i << "\t" << current_width << " x " << current_height << std::endl;
        current_width = current_width > 1 ? current_width / 2 : 1;
        current_height = current_height > 1 ? current_height / 2 : 1;
    }
}

bool resize_cpu(const ImageData& src_image, ImageData& dst_image) {
    stbir_resize_uint8(src_image.pixels, src_image.width, src_image.height, 0,
        dst_image.pixels, dst_image.width, dst_image.height, 0,
        dst_image.desired_channels);
    return true;
}