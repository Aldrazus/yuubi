#pragma once

#include "renderer/vulkan_usage.h"
#include "pch.h"

namespace yuubi {

class Device;
class BindlessSetManager {
public:
    BindlessSetManager() {};
    BindlessSetManager(const std::shared_ptr<Device>& device);

    uint32_t addImage(const vk::raii::Sampler& sampler, const vk::raii::ImageView& imageView);

    const vk::raii::DescriptorSetLayout& getDescriptorSetLayout() const { return layout_; }
    const vk::raii::DescriptorSet& getDescriptorSet() const { return set_; }
    
private:
    std::shared_ptr<Device> device_ = nullptr;
    vk::raii::DescriptorPool pool_ = nullptr;
    vk::raii::DescriptorSetLayout layout_ = nullptr;
    vk::raii::DescriptorSet set_ = nullptr;
};

}
