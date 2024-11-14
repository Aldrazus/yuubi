#include "renderer/descriptor_layout_builder.h"

#include "renderer/device.h"

namespace yuubi {

    DescriptorLayoutBuilder::DescriptorLayoutBuilder(const std::shared_ptr<Device>& device) : device_(device) {}

    DescriptorLayoutBuilder& DescriptorLayoutBuilder::addBinding(const vk::DescriptorSetLayoutBinding& binding) {
        bindings_.push_back(binding);
        return *this;
    }
    vk::raii::DescriptorSetLayout DescriptorLayoutBuilder::build(
            const vk::DescriptorSetLayoutBindingFlagsCreateInfo& bindingFlags,
            const vk::DescriptorSetLayoutCreateFlags& layoutFlags
    ) const {
        const vk::StructureChain<vk::DescriptorSetLayoutCreateInfo, vk::DescriptorSetLayoutBindingFlagsCreateInfo>
                createInfo{
                        vk::DescriptorSetLayoutCreateInfo{
                                                          .pNext = &bindingFlags,
                                                          .flags = layoutFlags,
                                                          .bindingCount = static_cast<uint32_t>(bindings_.size()),
                                                          .pBindings = bindings_.data()
                        },
                        bindingFlags
        };

        return device_->getDevice().createDescriptorSetLayout(createInfo.get());
    }

}
