#include "renderer/immediate_command_executor.h"
#include "renderer/device.h"

namespace yuubi {
ImmediateCommandExecutor::ImmediateCommandExecutor(
    std::shared_ptr<Device> device
)
    : device_(device) {
    immediateCommandPool_ = vk::raii::CommandPool{
        device_->getDevice(),
        {.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                       .queueFamilyIndex = device_->getQueue().familyIndex}
    };

    // TODO: wtf?
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = *immediateCommandPool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };
    immediateCommandBuffer_ =
        std::move(device_->getDevice().allocateCommandBuffers(allocInfo)[0]);

    immediateCommandFence_ = vk::raii::Fence{
        device_->getDevice(),
        vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled}
    };
}

void ImmediateCommandExecutor::submitImmediateCommands(
    std::function<void(const vk::raii::CommandBuffer& commandBuffer)>&& function
) {
    device_->getDevice().resetFences(*immediateCommandFence_);
    immediateCommandBuffer_.reset();

    immediateCommandBuffer_.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });

    function(immediateCommandBuffer_);

    immediateCommandBuffer_.end();

    vk::CommandBufferSubmitInfo commandBufferSubmitInfo{
        .commandBuffer = *immediateCommandBuffer_,
    };
    device_->getQueue().queue.submit2(
        {
            vk::SubmitInfo2{
                            .commandBufferInfoCount = 1,
                            .pCommandBufferInfos = &commandBufferSubmitInfo,
                            }
    },
        *immediateCommandFence_
    );

    device_->getDevice().waitForFences(
        {*immediateCommandFence_}, vk::True,
        std::numeric_limits<uint64_t>::max()
    );
}
}
