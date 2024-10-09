#pragma once

#include "pch.h"

namespace yuubi {

using ResourceHandle = uint32_t;

template <typename ResourceType, size_t Size>
class ResourceManager : NonCopyable {
public:
    virtual ~ResourceManager() = default;

    virtual ResourceHandle addResource(const std::shared_ptr<ResourceType>& resource) {
        resources_[nextAvailableHandle_] = resource;
        return nextAvailableHandle_++;
    };
protected:
    ResourceManager(ResourceManager&& rhs) = default;
    ResourceManager& operator=(ResourceManager&& rhs) = default;
    ResourceManager() = default;


    ResourceHandle nextAvailableHandle_ = 0;
    // TODO: This implementation keeps the resources in memory until the manager
    // is destroyed along with all other references. Use a slot map with
    // weak references instead to allow for resource removal.
    std::array<std::shared_ptr<ResourceType>, Size> resources_;
};

}
