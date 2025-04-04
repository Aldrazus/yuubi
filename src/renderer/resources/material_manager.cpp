#include "renderer/resources/material_manager.h"
#include "renderer/device.h"

namespace yuubi {

    MaterialManager::MaterialManager(std::shared_ptr<Device> device) : device_(std::move(device)) {
        constexpr vk::DeviceSize bufferSize = maxMaterials * sizeof(MaterialData);
        vk::BufferCreateInfo bufferCreateInfo{
            .size = bufferSize,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
                     vk::BufferUsageFlagBits::eShaderDeviceAddress
        };

        vma::AllocationCreateInfo shaderDataBufferAllocInfo{
            .usage = vma::MemoryUsage::eGpuOnly,
        };

        materialBuffer_ = device_->createBuffer(bufferCreateInfo, shaderDataBufferAllocInfo);
    }

    ResourceHandle MaterialManager::addResource(const std::shared_ptr<MaterialData>& material) {
        const auto handle = ResourceManager::addResource(material);

        materialBuffer_.upload(*device_, material.get(), sizeof(MaterialData), sizeof(MaterialData) * handle);

        return handle;
    }

}
