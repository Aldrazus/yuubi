#include "core/io/image.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace yuubi {

ImageData::ImageData(std::string_view path)
{
    pixels_ = stbi_load(path.data(), &width_, &height_, &channels_, STBI_rgb_alpha);
}
ImageData::~ImageData()
{
    destroy();
}

ImageData& ImageData::operator=(ImageData&& rhs)
{
    if (this == &rhs) {
        return *this;
    }

    destroy();
    width_ = rhs.width_;
    rhs.width_ = 0;
    height_ = rhs.height_;
    rhs.height_ = 0;
    channels_ = rhs.channels_;
    rhs.channels_ = 0;
    pixels_ = rhs.pixels_;
    rhs.pixels_ = nullptr;

    return *this;
}

void ImageData::destroy()
{
    if (pixels_ != nullptr) {
        stbi_image_free(pixels_);
    }
}

}
