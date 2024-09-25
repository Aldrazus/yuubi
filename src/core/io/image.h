#pragma once

#include "core/util.h"
#include "pch.h"
#include <stb_image.h>

namespace yuubi {

struct ImageData {
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    std::vector<std::byte> pixels;
};

ImageData loadImage(std::string_view path);

}
