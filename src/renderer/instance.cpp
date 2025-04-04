#include "renderer/instance.h"

#include <GLFW/glfw3.h>

namespace yuubi {

    Instance::Instance(const vk::raii::Context& context) {
        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName = "Yuubi",
            .applicationVersion = vk::makeApiVersion(0, 1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = vk::makeApiVersion(0, 1, 0, 0),
            .apiVersion = vk::ApiVersion13
        };

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers_) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> createInfo;
        createInfo.get<vk::InstanceCreateInfo>().setPApplicationInfo(&appInfo).setPEnabledExtensionNames(extensions);

        if (enableValidationLayers_) {
            createInfo.get<vk::InstanceCreateInfo>().setPEnabledLayerNames(validationLayers_);
        } else {
            createInfo.unlink<vk::DebugUtilsMessengerCreateInfoEXT>();
        }

        createInfo.get<vk::DebugUtilsMessengerCreateInfoEXT>()
            .setMessageSeverity(
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
            )
            .setMessageType(
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
            )
            .setPfnUserCallback(debugCallback);

        instance_ = vk::raii::Instance{context, createInfo.get()};

        if (enableValidationLayers_) {
            debugMessenger_ =
                instance_.createDebugUtilsMessengerEXT(createInfo.get<vk::DebugUtilsMessengerCreateInfoEXT>());
        }
    }

    const std::vector<const char*> Instance::validationLayers_ = {"VK_LAYER_KHRONOS_validation"};

}
