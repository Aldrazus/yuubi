#pragma once

#include "renderer/resources/resource_manager.h"
#include "renderer/vma/buffer.h"
#include "renderer/gpu_data.h"
#include "pch.h"

namespace yuubi {

    // TODO: Find right limit.
    constexpr uint32_t maxMaterials = 1024;

    class Device;
    class MaterialManager final : ResourceManager<MaterialData, maxMaterials>, NonCopyable {
    public:
        MaterialManager() = default;
        explicit MaterialManager(std::shared_ptr<Device> device);
        MaterialManager(MaterialManager&& rhs) noexcept :
            ResourceManager(std::move(rhs)), device_(std::exchange(rhs.device_, {})),
            materialBuffer_(std::exchange(rhs.materialBuffer_, {})) {};
        MaterialManager& operator=(MaterialManager&& rhs) noexcept {
            if (this != &rhs) {
                ResourceManager::operator=(std::move(rhs));
                std::swap(device_, rhs.device_);
                std::swap(materialBuffer_, rhs.materialBuffer_);
            }

            return *this;
        }

        virtual ResourceHandle addResource(const std::shared_ptr<MaterialData>& material) override;

        [[nodiscard]] inline vk::DeviceAddress getBufferAddress() const { return materialBuffer_.getAddress(); }

    private:
        std::shared_ptr<Device> device_;
        Buffer materialBuffer_;
    };

}
