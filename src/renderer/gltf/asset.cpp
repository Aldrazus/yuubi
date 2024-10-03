#include "renderer/gltf/asset.h"

#include "renderer/vma/image.h"
#include "renderer/device.h"
#include "fastgltf/types.hpp"
#include "renderer/vulkan_usage.h"
#include "renderer/vulkan/util.h"
#include <stb_image.h>

namespace {

struct ImageData {
    // TODO: use std::byte?
    unsigned char* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t numChannels;
};

// PERF: Use staging buffer pool. Use dedicated transfer queue.
yuubi::Image createImageFromData(yuubi::Device& device, const ImageData& data) {
    // Create staging buffer.
    vk::DeviceSize imageSize = data.width * data.height * 4;

    vk::BufferCreateInfo stagingBufferCreateInfo{
        .size = imageSize,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    };

    VmaAllocationCreateInfo stagingBufferAllocCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    yuubi::Buffer stagingBuffer = device.createBuffer(
        stagingBufferCreateInfo, stagingBufferAllocCreateInfo
    );

    // Copy image data onto mapped memory in staging buffer.
    std::memcpy(
        stagingBuffer.getMappedMemory(), data.pixels,
        static_cast<size_t>(imageSize)
    );

    // Create image.
    yuubi::Image image(
        &device.allocator(),
        yuubi::ImageCreateInfo{
            .width = data.width,
            .height = data.height,
            .format = vk::Format::eR8G8B8A8Srgb,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferDst,
            .properties = vk::MemoryPropertyFlagBits::eDeviceLocal
        }
    );

    // Upload data to image.
    device.submitImmediateCommands(
        [&stagingBuffer, &image,
         &data](const vk::raii::CommandBuffer& commandBuffer) {
            yuubi::transitionImage(
                commandBuffer, *image.getImage(), vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal
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
                .imageExtent =
                    vk::Extent3D{
                                               .width = data.width, .height = data.height, .depth = 1
                    }
            };
            commandBuffer.copyBufferToImage(
                *stagingBuffer.getBuffer(), *image.getImage(),
                vk::ImageLayout::eTransferDstOptimal, {copyRegion}
            );

            yuubi::transitionImage(
                commandBuffer, *image.getImage(),
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }
    );

    return image;
}

// PERF: Load images in parallel with stb_image.
std::optional<yuubi::Image> loadImage(
    yuubi::Device& device, fastgltf::Asset& asset, fastgltf::Image& image
) {
    std::optional<yuubi::Image> newImage;

    int width, height, numChannels;

    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(
                    filePath.fileByteOffset == 0
                );  // Byte offsets are unsupported.
                assert(filePath.uri.isLocalPath()
                );  // Only local files are supported.

                const std::string pathString{filePath.uri.path()};
                unsigned char* data = stbi_load(
                    pathString.c_str(), &width, &height, &numChannels, 4
                );
                if (data != nullptr) {
                    newImage = createImageFromData(
                        device,
                        {
                            .pixels = data,
                            .width = static_cast<uint32_t>(width),
                            .height = static_cast<uint32_t>(height),
                            .numChannels = 4,
                        }
                    );

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* data = stbi_load_from_memory(
                    reinterpret_cast<unsigned char*>(vector.bytes.data()),
                    static_cast<int>(vector.bytes.size()), &width, &height,
                    &numChannels, 4
                );

                if (data != nullptr) {
                    newImage = createImageFromData(
                        device,
                        {
                            .pixels = data,
                            .width = static_cast<uint32_t>(width),
                            .height = static_cast<uint32_t>(height),
                            .numChannels = 4,
                        }
                    );

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor{
                        [](auto& arg) {},
                        [&](fastgltf::sources::Vector& vector) {
                            unsigned char* data = stbi_load_from_memory(
                                reinterpret_cast<unsigned char*>(
                                    vector.bytes.data() + bufferView.byteOffset
                                ),
                                static_cast<int>(bufferView.byteLength), &width,
                                &height, &numChannels, 4
                            );
                            if (data != nullptr) {
                                newImage = createImageFromData(
                                    device,
                                    {
                                        .pixels = data,
                                        .width = static_cast<uint32_t>(width),
                                        .height = static_cast<uint32_t>(height),
                                        .numChannels = 4,
                                    }
                                );

                                stbi_image_free(data);
                            }
                        }
                    },
                    buffer.data
                );
            }
        },
        image.data
    );
}
}

namespace yuubi {

GLTFAsset::GLTFAsset(Device& device, const std::filesystem::path& filePath) {}

}