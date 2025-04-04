#pragma once

#include "renderer/vulkan_usage.h"
#include "window.h"
#include "core/util.h"
namespace yuubi {

    class Device;
    class Viewport;
    class Instance;
    class ImguiManager : NonCopyable {
    public:
        ImguiManager() = default;
        ImguiManager(const Instance& instance, Device& device, const Window& window, const Viewport& viewport);
        ImguiManager(ImguiManager&& rhs) noexcept;
        ImguiManager& operator=(ImguiManager&& rhs) noexcept;
        ~ImguiManager();

    private:
        bool initialized_ = false;
        vk::raii::DescriptorPool imguiDescriptorPool_ = nullptr;
    };

}
