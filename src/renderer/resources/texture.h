#pragma once

#include "core/io/image.h"
#include "core/util.h"
#include "renderer/vulkan_usage.h"
#include "renderer/resources/resource.h"
#include "renderer/vma/image.h"
#include "renderer/device.h"
#include "renderer/vulkan/util.h"

namespace yuubi {

class Texture : NonCopyable {
public:
    Texture() {};
    // TODO: const ref?
    Texture(Device& device, const std::string& name);
    Texture(Texture&& other);
    Texture& operator=(Texture&& other);

    inline const vk::raii::Sampler& getSampler() const { return sampler_; }
    inline const vk::raii::ImageView& getImageView() const { return view_; }
private:
    std::string name_;
    Image image_;
    vk::raii::ImageView view_ = nullptr;
    // TODO: use immutable samplers
    vk::raii::Sampler sampler_ = nullptr;
};

}
