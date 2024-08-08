#pragma once

#include "core/util.h"
#include "pch.h"
#include <stb_image.h>

namespace yuubi {

class ImageData : NonCopyable {
public:
    ImageData(std::string_view path);
    ~ImageData();
    // TODO: Default move constructor may not fully invalidate the other
    // ImageData
    ImageData(ImageData&& other) = default;
    ImageData& operator=(ImageData&& rhs);

    inline int width() const { return width_; }
    inline int height() const { return height_; }
    inline int channels() const { return channels_; }
    inline const stbi_uc* pixels() const { return pixels_; }

private:
    void destroy();

    int width_;
    int height_;
    int channels_;
    stbi_uc* pixels_ = nullptr;
};

inline ImageData loadImage(std::string_view path) { return ImageData(path); }

}
