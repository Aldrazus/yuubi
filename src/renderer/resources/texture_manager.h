#pragma once

#include "renderer/vulkan_usage.h"
#include "pch.h"
#include "renderer/vma/image.h"

namespace yuubi {

struct Texture : NonCopyable {
    Texture() = default;
    Texture(Image&& image, vk::raii::ImageView&& imageView, vk::raii::Sampler&& sampler)
        : image(std::move(image)), imageView(std::move(imageView)), sampler(std::move(sampler)) {}

    Texture(Texture&& rhs) = default;
    Texture& operator=(Texture&& rhs) noexcept {
        if (this != &rhs) {
            std::swap(image, rhs.image);
            std::swap(imageView, rhs.imageView);
            std::swap(sampler, rhs.sampler);
        }
        return *this;
    }

    Image image;
    vk::raii::ImageView imageView = nullptr;
    vk::raii::Sampler sampler = nullptr;
};

using TextureHandle = uint32_t;

class Device;
class TextureManager : NonCopyable {
public:
    TextureManager() = default;
    explicit TextureManager(std::shared_ptr<Device> device);
    TextureManager(TextureManager&&) = default;
    TextureManager& operator=(TextureManager&& rhs) {
        if (this != &rhs) {
            std::swap(device_, rhs.device_);
            std::swap(textures_, rhs.textures_);
            std::swap(textureSet_, rhs.textureSet_);
            std::swap(textureSetLayout_, rhs.textureSetLayout_);
            std::swap(pool_, rhs.pool_);
            std::swap(nextAvailableHandle_, rhs.nextAvailableHandle_);
        }
        return *this;
    }

    TextureHandle addTexture(const std::shared_ptr<Texture>& texture);

    [[nodiscard]] const vk::raii::DescriptorSetLayout& getTextureSetLayout(
    ) const {
        return textureSetLayout_;
    }

    [[nodiscard]] const vk::raii::DescriptorSet& getTextureSet() const {
        return textureSet_;
    }
private:
    void createDescriptorSet();
    void createErrorTexture();
    void createDefaultTexture();

    static const uint32_t maxTextures = 1024;

    std::shared_ptr<Device> device_;

    // TODO: This implementation keepts the textures in memory until the manager
    // is destroyed along with all other references. Use a slot map with
    // weak references instead to allow for texture removal.
    std::array<std::shared_ptr<Texture>, maxTextures> textures_;
    TextureHandle nextAvailableHandle_ = 0;

    vk::raii::DescriptorPool pool_ = nullptr;
    vk::raii::DescriptorSetLayout textureSetLayout_ = nullptr;
    vk::raii::DescriptorSet textureSet_ = nullptr;
};


}
