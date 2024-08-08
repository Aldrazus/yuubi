#pragma once

#include "pch.h"
#include "core/util.h"
#include "renderer/vulkan_usage.h"

namespace yuubi {

class Device;

class ImmediateCommandExecutor : NonCopyable {
public:
    ImmediateCommandExecutor() {}
    ImmediateCommandExecutor(std::shared_ptr<Device> device);
    void submitImmediateCommands(std::function<void(const vk::raii::CommandBuffer& commandBuffer)>&& function);
private:
    void createImmediateCommandBuffer();

    std::shared_ptr<Device> device_ = nullptr;

    vk::raii::CommandPool immediateCommandPool_ = nullptr;
    vk::raii::CommandBuffer immediateCommandBuffer_ = nullptr;
    vk::raii::Fence immediateCommandFence_ = nullptr;
};
}
