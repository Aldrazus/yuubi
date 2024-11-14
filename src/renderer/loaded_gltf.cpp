#include "renderer/loaded_gltf.h"
#include "renderer/device.h"
#include "renderer/gpu_data.h"

namespace yuubi {

    Mesh::Mesh(
            std::string name, Device& device, std::span<Vertex> vertices, std::span<uint32_t> indices,
            std::vector<GeoSurface>&& surfaces
    ) : name_(std::move(name)), surfaces_(std::move(surfaces)) {
        auto& allocator = device.allocator();
        // Vertex buffer.
        {
            const vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

            // Create vertex buffer.
            vk::BufferCreateInfo vertexBufferCreateInfo{
                    .size = bufferSize,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
                             vk::BufferUsageFlagBits::eShaderDeviceAddress
            };

            VmaAllocationCreateInfo vertexBufferAllocCreateInfo{
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            };

            vertexBuffer_ = std::make_shared<Buffer>(&allocator, vertexBufferCreateInfo, vertexBufferAllocCreateInfo);
            ;

            vertexBuffer_->upload(device, vertices.data(), bufferSize, 0);
        }

        // Index buffer.
        {
            const vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

            // Create index buffer.
            vk::BufferCreateInfo indexBufferCreateInfo{
                    .size = bufferSize,
                    .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst
            };

            VmaAllocationCreateInfo indexBufferAllocCreateInfo{.usage = VMA_MEMORY_USAGE_GPU_ONLY};

            indexBuffer_ = std::make_shared<Buffer>(&allocator, indexBufferCreateInfo, indexBufferAllocCreateInfo);

            indexBuffer_->upload(device, indices.data(), bufferSize, 0);
        }
    }

    Mesh& Mesh::operator=(Mesh&& rhs) noexcept {
        if (this != &rhs) {
            std::swap(vertexBuffer_, rhs.vertexBuffer_);
            std::swap(indexBuffer_, rhs.indexBuffer_);
            std::swap(surfaces_, rhs.surfaces_);
        }

        return *this;
    }
}
