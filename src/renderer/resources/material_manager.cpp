#include "renderer/resources/material_manager.h"
#include "renderer/device.h"

namespace yuubi {

MaterialManager::MaterialManager(std::shared_ptr<Device> device)
    : device_(std::move(device)) {
    const vk::DeviceSize bufferSize = maxMaterials * sizeof(MaterialData);
    vk::BufferCreateInfo bufferCreateInfo{
        .size = bufferSize,
        .usage = vk::BufferUsageFlagBits::eStorageBuffer |
                 vk::BufferUsageFlagBits::eTransferDst |
                 vk::BufferUsageFlagBits::eShaderDeviceAddress
    };

    VmaAllocationCreateInfo shaderDataBufferAllocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    materialBuffer_ =
        device_->createBuffer(bufferCreateInfo, shaderDataBufferAllocInfo);
}

ResourceHandle MaterialManager::addResource(
    const std::shared_ptr<MaterialData>& material
) {
    auto handle = ResourceManager::addResource(material);

    materialBuffer_.upload(
        *device_, material.get(), sizeof(MaterialData),
        sizeof(MaterialData) * handle
    );

    UB_INFO("Handle {}", handle);
    return handle;
}

}
