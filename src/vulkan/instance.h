#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

class Instance {
public:
   Instance();

   ~Instance();

private:
   vk::UniqueHandle<vk::Instance, vk::DispatchLoaderStatic> instance_;
};
