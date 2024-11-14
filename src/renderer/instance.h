#pragma once

#include "renderer/vulkan_usage.h"
#include "core/util.h"
#include "pch.h"

namespace yuubi {

    class Instance : NonCopyable {
    public:
        Instance() = default;
        ~Instance() = default;
        Instance(Instance&&) = default;
        Instance& operator=(Instance&&) = default;

        explicit Instance(const vk::raii::Context& context);

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData
        ) {
            std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

            return VK_FALSE;
        }

        const vk::raii::Instance& getInstance() const { return instance_; }

        explicit operator const vk::raii::Instance&() { return instance_; }

    private:
        void createInstance();
        void setupDebugMessenger();

        static const std::vector<const char*> validationLayers_;
#if UB_DEBUG
        bool enableValidationLayers_ = true;
#else
        const bool enableValidationLayers_ = false;
#endif

        vk::raii::Instance instance_ = nullptr;
        vk::raii::DebugUtilsMessengerEXT debugMessenger_ = nullptr;
    };

}
