#pragma once

#include "renderer/vulkan_usage.h"

namespace yuubi {

struct RenderAttachment {
    vk::Image image;
    vk::ImageView imageView;
};

}
