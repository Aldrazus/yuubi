#pragma once

#include "core/util.h"
#include "renderer/vma/image.h"
#include "renderer/device.h"
#include "renderer/immediate_command_executor.h"
#include "core/io/image.h"
#include "renderer/vulkan/util.h"
#include "pch.h"

namespace yuubi {

class ImageManager : NonCopyableOrMovable {
public:
    ImageManager();

    ImageManager(
        std::shared_ptr<Device> device,
        std::shared_ptr<ImmediateCommandExecutor> immCmdExec
    );

    std::shared_ptr<Image> load(const std::string& filename);

private:
    std::shared_ptr<Device> device_;
    std::shared_ptr<ImmediateCommandExecutor> immediateCommandExecutor_;
    std::unordered_map<std::string, std::weak_ptr<Image>> cache_;
};

ImageManager::ImageManager(){};

ImageManager::ImageManager(
    std::shared_ptr<Device> device,
    std::shared_ptr<ImmediateCommandExecutor> immCmdExec
)
    : device_(device), immediateCommandExecutor_(immCmdExec) {}

std::shared_ptr<Image> ImageManager::load(const std::string& filename) {
    if (!cache_.contains(std::string(filename))) {
        auto textureData = loadImage("textures/texture.jpg");

        vk::DeviceSize imageSize =
            textureData.width() * textureData.height() * 4;

        // Create staging buffer.
        vk::BufferCreateInfo stagingBufferCreateInfo{
            .size = imageSize,
            .usage = vk::BufferUsageFlagBits::eTransferSrc,
        };

        VmaAllocationCreateInfo stagingBufferAllocCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        Buffer stagingBuffer = device_->createBuffer(
            stagingBufferCreateInfo, stagingBufferAllocCreateInfo
        );

        // Copy image data onto mapped memory in staging buffer.
        std::memcpy(
            stagingBuffer.getMappedMemory(), textureData.pixels(),
            static_cast<size_t>(imageSize)
        );

        auto image = std::make_unique<Image>(&device_->allocator(), ImageCreateInfo{
            .width = static_cast<uint32_t>(textureData.width()),
            .height = static_cast<uint32_t>(textureData.height()),
            .format = vk::Format::eR8G8B8A8Srgb, 
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled |
                vk::ImageUsageFlagBits::eTransferDst,
            .properties = vk::MemoryPropertyFlagBits::eDeviceLocal
        });

        immediateCommandExecutor_->submitImmediateCommands(
            [&stagingBuffer, &textureData,
             &image](const vk::raii::CommandBuffer& commandBuffer) {
                transitionImage(
                    commandBuffer, *image->getImage(),
                    vk::ImageLayout::eUndefined,
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
                                                   .width = static_cast<uint32_t>(textureData.width()),
                                                   .height =
                                static_cast<uint32_t>(textureData.height()),
                                                   .depth = 1
                        }
                };
                commandBuffer.copyBufferToImage(
                    *stagingBuffer.getBuffer(), *image->getImage(),
                    vk::ImageLayout::eTransferDstOptimal, {copyRegion}
                );

                transitionImage(
                    commandBuffer, *image->getImage(),
                    vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal
                );
            }
        );

        std::shared_ptr<Image> spImage(image.release(), [this](Image* p) {
            delete p;
        });
    }

    return cache_.at(filename).lock();
}
}
