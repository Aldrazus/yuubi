#pragma once

#include "pch.h"
#include "renderer/vulkan_usage.h"

namespace yuubi {

class Device;
class DescriptorLayoutBuilder {
public:
    DescriptorLayoutBuilder(const std::shared_ptr<Device> device);
    DescriptorLayoutBuilder& addBinding(const vk::DescriptorSetLayoutBinding& binding);
    vk::raii::DescriptorSetLayout build(const vk::DescriptorSetLayoutBindingFlagsCreateInfo& bindingFlags, const vk::DescriptorSetLayoutCreateFlags& layoutFlags);

private:
    std::shared_ptr<Device> device_;
    std::vector<vk::DescriptorSetLayoutBinding> bindings_;
};

}
