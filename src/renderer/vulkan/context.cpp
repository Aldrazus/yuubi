#include "renderer/vulkan/context.h"

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace yuubi {

Context::Context() {
    
}

Context::~Context() {
    instance_.destroySurfaceKHR(surface_);
    if (enableValidationLayers_) {
        instance_.destroyDebugUtilsMessengerEXT(debugMessenger_);
    }
    instance_.destroy();
}

}
