#pragma once

#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

using TextureHandle = uint32_t;

class Device;
class Image;
class Texture;
class BindlessSetManager {
public:
    BindlessSetManager() = default;
    explicit BindlessSetManager(const std::shared_ptr<Device>& device);

    TextureHandle addTexture(const Texture& texture);

    [[nodiscard]] const vk::raii::DescriptorSetLayout& getTextureSetLayout(
    ) const {
        return textureSetLayout_;
    }

    [[nodiscard]] const vk::raii::DescriptorSet& getTextureSet() const {
        return textureSet_;
    }

private:
    std::shared_ptr<Device> device_ = nullptr;
    vk::raii::DescriptorPool pool_ = nullptr;
    vk::raii::DescriptorSetLayout textureSetLayout_ = nullptr;
    vk::raii::DescriptorSet textureSet_ = nullptr;
};

}
