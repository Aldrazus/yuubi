#include "renderer/gltf/thread.h"

#include <fastgltf/core.hpp>
#include <stb_image.h>
#include "pch.h"
#include "renderer/vma/image.h"
#include "renderer/resources/texture_manager.h"
#include <thread>
#include "renderer/device.h"

namespace {

    std::unordered_set<std::size_t> getSrgbImageIndices(std::span<const fastgltf::Material> materials) {
        std::unordered_set<std::size_t> indices;

        for (const auto& material: materials) {
            if (const auto& baseColorTexture = material.pbrData.baseColorTexture) {
                indices.emplace(baseColorTexture->textureIndex);
            }
            if (const auto& emissiveTexture = material.emissiveTexture) {
                indices.emplace(emissiveTexture->textureIndex);
            }
        }

        return indices;
    }

    class ImageData : yuubi::NonCopyable {
    public:
        ImageData() = default;

        explicit ImageData(std::string_view path) {
            data_ = stbi_load(path.data(), &width_, &height_, &numChannels_, 0);
        }

        explicit ImageData(std::span<const unsigned char> bytes) {
            data_ = stbi_load_from_memory(bytes.data(), bytes.size(), &width_, &height_, &numChannels_, 4);
            numChannels_ = 4;
        }

        ~ImageData() { stbi_image_free(data_); }

        ImageData(ImageData&& rhs) noexcept :
            data_(std::exchange(rhs.data_, nullptr)), width_(std::exchange(rhs.width_, 0)),
            height_(std::exchange(rhs.height_, 0)), numChannels_(std::exchange(rhs.numChannels_, 0)) {}

        ImageData& operator=(ImageData&& rhs) noexcept {
            if (this != &rhs) {
                std::swap(data_, rhs.data_);
                std::swap(width_, rhs.width_);
                std::swap(height_, rhs.height_);
                std::swap(numChannels_, rhs.numChannels_);
            }

            return *this;
        }

        vk::Format getImageFormat(bool srgb) const {
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

    private:
        unsigned char* data_ = nullptr;
        int width_ = 0;
        int height_ = 0;
        int numChannels_ = 0;
    };

    struct ThreadResource {
        vk::raii::CommandPool commandPool = nullptr;
        vk::raii::CommandBuffer commandBuffer = nullptr;
    };

    std::vector<ThreadResource> createThreadResources(const yuubi::Device& device) {
        const int_fast32_t numThreads = std::thread::hardware_concurrency();
        return std::views::iota(0, numThreads) | std::views::transform([&device](auto _) {
                   ThreadResource resource;
                   resource.commandPool = vk::raii::CommandPool{
                           device.getDevice(),
                           vk::CommandPoolCreateInfo{
                                                     .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                                     .queueFamilyIndex = device.getQueue().familyIndex,
                                                     }
                   };
                   resource.commandBuffer = std::move(device.getDevice().allocateCommandBuffers(
                           vk::CommandBufferAllocateInfo{
                                   .commandPool = resource.commandPool,
                                   .level = vk::CommandBufferLevel::ePrimary,
                                   .commandBufferCount = 1
                           }
                   )[0]);
                   return resource;
               }) |
               std::ranges::to<std::vector<ThreadResource>>();
    }

    struct ImageCreateInfo {
        ImageData data;
        vk::Format format;
    };

    ImageCreateInfo loadImageData(
            const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& assetDir, bool srgb
    ) {
        ImageCreateInfo createInfo;
        // Mostly taken from
        // https://github.com/spnda/fastgltf/blob/main/examples/gl_viewer/gl_viewer.cpp
        std::visit(
                fastgltf::visitor{
                        [](auto&) {},
                        [&assetDir, srgb, &createInfo](const fastgltf::sources::URI& filePath) {
                            assert(filePath.fileByteOffset == 0); // Byte offsets are unsupported.
                            assert(filePath.uri.isLocalPath());
                            const std::string pathString = (assetDir / filePath.uri.fspath()).string();
                            createInfo.data = ImageData(pathString);
                            createInfo.format = createInfo.data.getImageFormat(srgb);
                        },
                        [srgb, &createInfo](const fastgltf::sources::Array& vector) {
                            const std::span bytes{
                                    reinterpret_cast<const unsigned char*>(vector.bytes.data()), vector.bytes.size()
                            };
                            createInfo.data = ImageData(bytes);
                            createInfo.format = createInfo.data.getImageFormat(srgb);
                        },
                        [&asset, srgb, &createInfo](const fastgltf::sources::BufferView& view) {
                            auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                            auto& buffer = asset.buffers[bufferView.bufferIndex];

                            std::visit(
                                    fastgltf::visitor{
                                            [](auto& arg) {},
                                            [srgb, &createInfo](const fastgltf::sources::Array& vector) {
                                                const std::span bytes{
                                                        reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                                                        vector.bytes.size()
                                                };
                                                createInfo.data = ImageData(bytes);
                                                createInfo.format = createInfo.data.getImageFormat(srgb);
                                            }
                                    },
                                    buffer.data
                            );
                        }
                },
                image.data
        );

        return createInfo;
    }

    std::vector<ImageCreateInfo> loadImageDataChunk(
            const fastgltf::Asset& asset, std::span<std::pair<fastgltf::Image, bool>> images,
            const std::filesystem::path& assetDir
    ) {
        return images | std::views::transform([&asset, assetDir](auto&& imageSrgbPair) {
                   auto [image, srgb] = imageSrgbPair;
                   return loadImageData(asset, image, assetDir, srgb);
               }) |
               std::ranges::to<std::vector<ImageCreateInfo>>();
    }

    std::vector<ImageCreateInfo> loadAllImageData(const fastgltf::Asset& asset, const std::filesystem::path& assetDir) {
        auto srgbImageIndices = getSrgbImageIndices(asset.materials);

        auto imageSrgbPair = std::views::enumerate(asset.textures) |
                             std::views::transform([&asset, &srgbImageIndices](auto&& indexTexturePair) {
                                 const auto& [i, fastgltfTexture] = indexTexturePair;
                                 const auto& fastgltfImage = asset.images.at(fastgltfTexture.imageIndex.value());
                                 return std::pair<fastgltf::Image, bool>{fastgltfImage, srgbImageIndices.contains(i)};
                             }) |
                             std::ranges::to<std::vector<std::pair<fastgltf::Image, bool>>>();

        return loadImageDataChunk(asset, imageSrgbPair, assetDir);
    }
}

namespace yuubi {
    std::vector<Texture> loadTextures(
            const Device& device, const fastgltf::Asset& asset, const std::filesystem::path& assetDir
    ) {
        UB_INFO("Loading textures new...");
        auto imageCreateInfos = loadAllImageData(asset, assetDir);
        UB_INFO("Done loading textures new {}", imageCreateInfos.size());
        return {};
    }
}
