#include "core/io/image.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace yuubi {

ImageData loadImage(std::string_view path) { 
    int width, height, channels;
    auto* imageChars = 
        stbi_load(path.data(), &width, &height, &channels, STBI_rgb_alpha);

    // Alpha channel is being forced due to STBI_rbg_alpha flag.
    channels = 4;

    const uint32_t imageSizeBytes = width * height * channels;
    
    ImageData data{
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .channels = static_cast<uint32_t>(channels),
        .pixels = std::vector<std::byte>{imageSizeBytes}
    };

    std::memcpy(data.pixels.data(), imageChars, imageSizeBytes);

    stbi_image_free(imageChars);

    return data;
}

}
