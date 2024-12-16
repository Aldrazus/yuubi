#pragma once

#include "pch.h"
#include <fastgltf/types.hpp>
#include "renderer/resources/texture_manager.h"
#include <stb_image.h>


namespace yuubi {
    class Device;

    class StbImageData : NonCopyable {
    public:
        StbImageData() = default;

        explicit StbImageData(std::string_view path, bool srgb) {
            data_ = stbi_load(path.data(), &width_, &height_, &numChannels_, 0);
            format_ = getImageFormat(srgb);
        }

        explicit StbImageData(std::span<unsigned char> bytes, bool srgb) {
            data_ = stbi_load_from_memory(bytes.data(), bytes.size(), &width_, &height_, &numChannels_, 4);
            numChannels_ = 4;
            format_ = getImageFormat(srgb);
        }

        [[nodiscard]] unsigned char* data() const { return data_; }
        [[nodiscard]] uint32_t width() const { return width_; }
        [[nodiscard]] uint32_t height() const { return height_; }
        [[nodiscard]] uint32_t numChannels() const { return numChannels_; }
        [[nodiscard]] vk::Format format() const { return format_; }

        ~StbImageData() { stbi_image_free(data_); }

        StbImageData(StbImageData&& rhs) noexcept :
            data_(std::exchange(rhs.data_, nullptr)), width_(std::exchange(rhs.width_, 0)),
            height_(std::exchange(rhs.height_, 0)), numChannels_(std::exchange(rhs.numChannels_, 0)),
            format_(std::exchange(rhs.format_, {})) {}

        StbImageData& operator=(StbImageData&& rhs) noexcept {
            if (this != &rhs) {
                std::swap(data_, rhs.data_);
                std::swap(width_, rhs.width_);
                std::swap(height_, rhs.height_);
                std::swap(numChannels_, rhs.numChannels_);
                std::swap(format_, rhs.format_);
            }

            return *this;
        }

    private:
        [[nodiscard]] vk::Format getImageFormat(bool srgb) const {
            switch (numChannels_) {
                case 1:
                    return vk::Format::eR8Unorm;
                case 2:
                    return vk::Format::eR8G8Unorm;
                case 3:
                case 4:
                    return srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
                default:
                    throw std::runtime_error{"Unsupported image channel"};
            }
        }

        unsigned char* data_ = nullptr;
        int width_ = 0;
        int height_ = 0;
        int numChannels_ = 0;
        vk::Format format_;
    };

    std::vector<StbImageData> loadTextures(const fastgltf::Asset& asset, const std::filesystem::path& assetDir);

}
