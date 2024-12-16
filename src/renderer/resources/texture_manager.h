#pragma once

#include "renderer/resources/resource_manager.h"
#include "renderer/vulkan_usage.h"
#include "pch.h"
#include "renderer/vma/image.h"

namespace yuubi {

    // TODO: Find right limit.
    constexpr uint32_t maxTextures = 1024;

    struct Texture : NonCopyable {
        Texture() = default;
        Texture(Image&& image, vk::raii::ImageView&& imageView, vk::raii::Sampler&& sampler) :
            image(std::move(image)), imageView(std::move(imageView)), sampler(std::move(sampler)) {}

        Texture(Texture&& rhs) noexcept :
            image(std::exchange(rhs.image, {})), imageView(std::exchange(rhs.imageView, nullptr)),
            sampler(std::exchange(rhs.sampler, nullptr)) {};
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
    class TextureManager final : ResourceManager<Texture, maxTextures>, NonCopyable {
    public:
        TextureManager() = default;
        TextureManager(std::shared_ptr<Device> device, std::shared_ptr<vk::raii::DescriptorSet> textureSet);
        TextureManager(TextureManager&& rhs) noexcept :
            ResourceManager(std::move(rhs)), device_(std::exchange(rhs.device_, nullptr)),
            textureSet_(std::exchange(rhs.textureSet_, nullptr)) {};

        TextureManager& operator=(TextureManager&& rhs) noexcept {
            if (this != &rhs) {
                ResourceManager::operator=(std::move(rhs));
                std::swap(device_, rhs.device_);
                std::swap(textureSet_, rhs.textureSet_);
            }
            return *this;
        }

        virtual ResourceHandle addResource(const std::shared_ptr<Texture>& texture) override;

    private:
        void createErrorTexture();

        std::shared_ptr<Device> device_;
        std::shared_ptr<vk::raii::DescriptorSet> textureSet_;
    };

}
