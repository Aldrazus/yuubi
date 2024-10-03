#pragma once

#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Image;
class Device;
class GLTFAsset {
public:
    GLTFAsset(Device& device, const std::filesystem::path& filePath);

private:
    std::vector<Image> images_;
    std::vector<vk::raii::ImageView> imageViews_;
    std::vector<vk::raii::Sampler> samplers_;
};

}
