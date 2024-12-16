#include "renderer/gltf/thread.h"

#include <fastgltf/core.hpp>
#include <stb_image.h>
#include "pch.h"
#include "renderer/vma/image.h"
#include "renderer/resources/texture_manager.h"
#include <thread>
#include "renderer/device.h"
#include <glm/glm.hpp>
#include "renderer/vulkan/util.h"

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

        [[nodiscard]] const unsigned char* data() const { return data_; }
        [[nodiscard]] uint32_t width() const { return width_; }
        [[nodiscard]] uint32_t height() const { return height_; }
        [[nodiscard]] uint32_t numChannels() const { return numChannels_; }

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

    struct SamplerInfo {
        vk::Filter minFilter;
        vk::Filter magFilter;
        vk::SamplerMipmapMode mipmapMode;
    };

    struct TextureCreateInfo {
        ImageData data;
        vk::Format format;
        SamplerInfo samplerInfo;
    };

    std::pair<vk::Filter, vk::SamplerMipmapMode> getSamplerFilterInfo(fastgltf::Filter filter) {
        switch (filter) {
            case fastgltf::Filter::Nearest:
                return {vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear};
            case fastgltf::Filter::NearestMipMapNearest:
                return {vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest};
            case fastgltf::Filter::NearestMipMapLinear:
                return {vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear};

            case fastgltf::Filter::Linear:
                return {vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
            case fastgltf::Filter::LinearMipMapNearest:
                return {vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest};
            case fastgltf::Filter::LinearMipMapLinear:
                return {vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};

            default:
                return {vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear};
        }
    }

    SamplerInfo getSamplerInfo(const fastgltf::Sampler& sampler) {
        // Create sampler.
        auto [minFilter, minMipmapMode] = getSamplerFilterInfo(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
        auto [magFilter, _] = getSamplerFilterInfo(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        return SamplerInfo{.minFilter = minFilter, .magFilter = magFilter, .mipmapMode = minMipmapMode};
    }

    TextureCreateInfo loadImageData(
            const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& assetDir, bool srgb
    ) {
        TextureCreateInfo createInfo;
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

    std::vector<TextureCreateInfo> loadImageDataChunk(
            const fastgltf::Asset& asset, std::span<std::pair<fastgltf::Texture, bool>> images,
            const std::filesystem::path& assetDir
    ) {
        return images | std::views::transform([&asset, assetDir](auto&& imageSrgbPair) {
                   const auto [texture, srgb] = imageSrgbPair;
                   const auto& image = asset.images.at(texture.imageIndex.value());
                   auto createInfo = loadImageData(asset, image, assetDir, srgb);

                   const auto& sampler = asset.samplers.at(texture.samplerIndex.value());
                   createInfo.samplerInfo = getSamplerInfo(sampler);
                   return createInfo;
               }) |
               std::ranges::to<std::vector<TextureCreateInfo>>();
    }

    std::vector<TextureCreateInfo> loadAllImageData(
            const fastgltf::Asset& asset, const std::filesystem::path& assetDir
    ) {
        auto srgbImageIndices = getSrgbImageIndices(asset.materials);

        auto imageSrgbPair =
                std::views::enumerate(asset.textures) |
                std::views::transform([&asset, &srgbImageIndices](auto&& indexTexturePair) {
                    const auto& [i, fastgltfTexture] = indexTexturePair;
                    const auto& fastgltfImage = asset.images.at(fastgltfTexture.imageIndex.value());
                    return std::pair<fastgltf::Texture, bool>{fastgltfTexture, srgbImageIndices.contains(i)};
                }) |
                std::ranges::to<std::vector<std::pair<fastgltf::Texture, bool>>>();

        return loadImageDataChunk(asset, imageSrgbPair, assetDir);
    }

    void recordUploadCommands(
            const vk::raii::CommandBuffer& commandBuffer, const yuubi::Buffer& stagingBuffer, const yuubi::Image& image,
            const ImageData& data
    ) {
        commandBuffer.reset();

        commandBuffer.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

        if (data.numChannels() == 3) {
            auto pixels =
                    std::span{
                            reinterpret_cast<const glm::u8vec3*>(data.data()),
                            static_cast<size_t>(data.width() * data.height())
                    } |
                    std::views::transform([](const auto& pixel) { return glm::u8vec4{pixel, 255}; });
            std::ranges::copy(pixels, static_cast<glm::u8vec4*>(stagingBuffer.getMappedMemory()));
        } else {
            auto pixels =
                    std::span{data.data(), static_cast<size_t>(data.width() * data.height() * data.numChannels())};
            std::ranges::copy(pixels, static_cast<unsigned char*>(stagingBuffer.getMappedMemory()));
        }

        // Create image.
        const uint32_t mipLevels =
                static_cast<uint32_t>(std::floor(std::log2(std::max(data.width(), data.height())))) + 1;

        yuubi::transitionImage(
                commandBuffer, *image.getImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal
        );

        vk::BufferImageCopy copyRegion{
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource =
                        vk::ImageSubresourceLayers{
                                                   .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                   .mipLevel = 0,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = 1
                        },
                .imageOffset = {0, 0, 0},
                .imageExtent = vk::Extent3D{.width = data.width(), .height = data.height(), .depth = 1}
        };

        commandBuffer.copyBufferToImage(
                *stagingBuffer.getBuffer(), *image.getImage(), vk::ImageLayout::eTransferDstOptimal, {copyRegion}
        );

        // Generate mipmaps.
        vk::ImageMemoryBarrier2 barrier{
                .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .image = *image.getImage(),
                .subresourceRange =
                        vk::ImageSubresourceRange{
                                                  .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                  .levelCount = 1,
                                                  .baseArrayLayer = 0,
                                                  .layerCount = 1,
                                                  },
        };

        int32_t mipWidth = data.width();
        int32_t mipHeight = data.height();

        for (uint32_t i = 1; i < mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
            barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
            barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;

            vk::DependencyInfo dependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier};

            commandBuffer.pipelineBarrier2(dependencyInfo);

            std::array<vk::Offset3D, 2> srcOffsets{
                    {{0, 0, 0}, {mipWidth, mipHeight, 1}}
            };
            std::array<vk::Offset3D, 2> dstOffsets{
                    {{0, 0, 0}, {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}}
            };

            vk::ImageBlit2 blit{
                    .srcSubresource =
                            vk::ImageSubresourceLayers{
                                                       .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                       .mipLevel = i - 1,
                                                       .baseArrayLayer = 0,
                                                       .layerCount = 1,
                                                       },
                    .srcOffsets = srcOffsets,
                    .dstSubresource =
                            vk::ImageSubresourceLayers{
                                                       .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                       .mipLevel = i,
                                                       .baseArrayLayer = 0,
                                                       .layerCount = 1
                            },
                    .dstOffsets = dstOffsets,
            };

            commandBuffer.blitImage2(
                    vk::BlitImageInfo2{
                            .srcImage = *image.getImage(),
                            .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
                            .dstImage = *image.getImage(),
                            .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
                            .regionCount = 1,
                            .pRegions = &blit,
                            .filter = vk::Filter::eLinear, // TODO: fix!!
                    }
            );

            barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
            barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            barrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
            barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
            barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;

            commandBuffer.pipelineBarrier2(
                    vk::DependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier}
            );

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
        barrier.dstAccessMask = vk::AccessFlagBits2::eShaderRead;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader;

        commandBuffer.pipelineBarrier2(
                vk::DependencyInfo{.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier}
        );

        commandBuffer.end();
    }

}

namespace yuubi {
    std::vector<std::shared_ptr<Texture>> loadTextures(
            const Device& device, const fastgltf::Asset& asset, const std::filesystem::path& assetDir
    ) {
        UB_INFO("Loading textures new...");
        auto imageCreateInfos = loadAllImageData(asset, assetDir);

        const auto commandPool = vk::raii::CommandPool{
                device.getDevice(),
                vk::CommandPoolCreateInfo{
                                          .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                          .queueFamilyIndex = device.getQueue().familyIndex,
                                          }
        };

        const auto commandBuffer = std::move(device.getDevice().allocateCommandBuffers(
                vk::CommandBufferAllocateInfo{
                        .commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1
                }
        )[0]);

        struct Job {
            Texture texture;
            Buffer buffer;
            ::ImageData imageData;
        };

        auto jobs =
                imageCreateInfos | std::views::transform([&device](auto& imageCreateInfo) {
                    auto& [data, format, samplerInfo] = imageCreateInfo;

                    // Create image.
                    const uint32_t mipLevels =
                            static_cast<uint32_t>(std::floor(std::log2(std::max(data.width(), data.height())))) + 1;

                    Image image(
                            &device.allocator(),
                            ImageCreateInfo{
                                    .width = data.width(),
                                    .height = data.height(),
                                    .format = format,
                                    .tiling = vk::ImageTiling::eOptimal,
                                    .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
                                             vk::ImageUsageFlagBits::eTransferDst,
                                    .properties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                    .mipLevels = mipLevels
                            }
                    );

                    auto imageView = device.createImageView(
                            image.getImage(), image.getImageFormat(), vk::ImageAspectFlagBits::eColor,
                            image.getMipLevels()
                    );

                    // Create sampler.
                    auto sampler = device.getDevice().createSampler(
                            vk::SamplerCreateInfo{
                                    .magFilter = samplerInfo.magFilter,
                                    .minFilter = samplerInfo.minFilter,
                                    .mipmapMode = samplerInfo.mipmapMode,
                                    .anisotropyEnable = vk::True,
                                    .maxAnisotropy = device.getPhysicalDevice()
                                                             .getProperties2()
                                                             .properties.limits.maxSamplerAnisotropy,
                                    .minLod = 0,
                                    .maxLod = static_cast<float>(image.getMipLevels()),
                            }
                    );

                    // Create staging buffer.
                    const vk::DeviceSize imageSize =
                            data.width() * data.height() * (data.numChannels() == 3 ? 4 : data.numChannels());

                    const vk::BufferCreateInfo stagingBufferCreateInfo{
                            .size = imageSize,
                            .usage = vk::BufferUsageFlagBits::eTransferSrc,
                    };

                    const vma::AllocationCreateInfo stagingBufferAllocCreateInfo{
                            .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                                     vma::AllocationCreateFlagBits::eMapped,
                            .usage = vma::MemoryUsage::eAuto,
                    };

                    Buffer stagingBuffer = device.createBuffer(stagingBufferCreateInfo, stagingBufferAllocCreateInfo);

                    return Job{
                            .texture = Texture(std::move(image), std::move(imageView), std::move(sampler)),
                            .buffer = std::move(stagingBuffer),
                            .imageData = std::move(data)
                    };
                }) |
                std::ranges::to<std::vector<Job>>();


        // TODO: parallelize
        for (auto& [texture, buffer, imageData]: jobs) {
            recordUploadCommands(commandBuffer, buffer, texture.image, imageData);
        }

        vk::CommandBufferSubmitInfo const commandBufferSubmitInfo{
                .commandBuffer = *commandBuffer,
        };

        const auto fence =
                vk::raii::Fence{device.getDevice(), vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled}};

        device.getDevice().resetFences(*fence);

        device.getQueue().queue.submit2(
                {
                        vk::SubmitInfo2{.commandBufferInfoCount = 1, .pCommandBufferInfos = &commandBufferSubmitInfo}
        },
                *fence
        );

        device.getDevice().waitForFences({*fence}, vk::True, std::numeric_limits<uint64_t>::max());

        return jobs | std::views::transform([&](auto& job) {
                   return std::make_shared<Texture>(
                           std::move(job.texture.image), std::move(job.texture.imageView),
                           std::move(job.texture.sampler)
                   );
               }) |
               std::ranges::to<std::vector<std::shared_ptr<Texture>>>();

        UB_INFO("Done loading textures new {}", imageCreateInfos.size());

        return {};
    }
}
