#pragma once

#include "pch.h"

namespace yuubi {

    using ResourceHandle = uint32_t;

    template<typename ResourceType, size_t Size>
    class ResourceManager : NonCopyable {
    public:
        virtual ~ResourceManager() = default;

        virtual ResourceHandle addResource(const std::shared_ptr<ResourceType>& resource) {
            resources_[nextAvailableHandle_] = resource;
            return nextAvailableHandle_++;
        };
        ResourceManager(ResourceManager&& rhs) noexcept :
            nextAvailableHandle_(std::exchange(rhs.nextAvailableHandle_, 0)),
            resources_(std::exchange(rhs.resources_, {})) {};
        ResourceManager& operator=(ResourceManager&& rhs) noexcept {
            if (this != &rhs) {
                std::swap(nextAvailableHandle_, rhs.nextAvailableHandle_);
                std::swap(resources_, rhs.resources_);
            }

            return *this;
        };

    protected:
        ResourceManager() = default;


        ResourceHandle nextAvailableHandle_ = 0;
        // TODO: This implementation keeps the resources in memory until the manager
        // is destroyed along with all other references. Use a slot map with
        // weak references instead to allow for resource removal.
        std::array<std::shared_ptr<ResourceType>, Size> resources_;
    };

}
