#pragma once

#include "renderer/vulkan/device.h"
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace yuubi {
class SwapChain {
public:
    SwapChain(vk::Device device);
    ~SwapChain();
private:
    Device device_;
    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> swapChainImages_;
    vk::Format imageFormat_;
    vk::Extent2D swapExtent_;
};
}
