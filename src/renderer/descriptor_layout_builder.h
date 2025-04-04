#pragma once

#include "pch.h"
#include "renderer/vulkan_usage.h"

namespace yuubi {

    class Device;
    class DescriptorLayoutBuilder {
    public:
        explicit DescriptorLayoutBuilder(const std::shared_ptr<Device>& device);
        DescriptorLayoutBuilder& addBinding(const vk::DescriptorSetLayoutBinding& binding);
        [[nodiscard]] vk::raii::DescriptorSetLayout build(
            const vk::DescriptorSetLayoutBindingFlagsCreateInfo& bindingFlags,
            const vk::DescriptorSetLayoutCreateFlags& layoutFlags
        ) const;

    private:
        std::shared_ptr<Device> device_;
        std::vector<vk::DescriptorSetLayoutBinding> bindings_;
    };

}
