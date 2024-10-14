#pragma once

#include "renderer/resources/resource_manager.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"
#include "renderer/vma/image.h"

namespace yuubi {

// TODO: Find right limit.
const uint32_t maxTextures = 1024;

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
class TextureManager : ResourceManager<Texture, maxTextures>, NonCopyable {
public:
    TextureManager() = default;
    explicit TextureManager(std::shared_ptr<Device> device);
    TextureManager(TextureManager&&) = default;
    TextureManager& operator=(TextureManager&& rhs) noexcept {
        if (this != &rhs) {
            std::swap(device_, rhs.device_);
            std::swap(textureSet_, rhs.textureSet_);
            std::swap(textureSetLayout_, rhs.textureSetLayout_);
            std::swap(pool_, rhs.pool_);
            std::swap(nextAvailableHandle_, rhs.nextAvailableHandle_);
        }
        return *this;
    }

    virtual ResourceHandle addResource(const std::shared_ptr<Texture>& texture) override;

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

    std::shared_ptr<Device> device_;

    vk::raii::DescriptorPool pool_ = nullptr;
    vk::raii::DescriptorSetLayout textureSetLayout_ = nullptr;
    vk::raii::DescriptorSet textureSet_ = nullptr;
};


}
