#include "renderer/gltf/thread.h"

#include <fastgltf/core.hpp>
#include <stb_image.h>

namespace {

std::unordered_set<std::size_t> getSrgbImageIndices(std::span<fastgltf::Material> materials) {
    std::unordered_set<std::size_t> indices;

    for (const auto& material: materials) {
        if (const auto& baseColorTexture = material.pbrData.baseColorTexture) {
            indices.emplace(baseColorTexture->textureIndex);
        }
        if (const auto& emissiveTexture = material.emissiveTexture) {
            indices.emplace(emissiveTexture->textureIndex);
        }
    }

    return indices;
}

class ImageData {
public:
    ImageData() = default;

    ImageData(std::string_view path) {
        data_ = stbi_load(path.data(), &width_, &height_, &numChannels_, 0);
    }

    ImageData(std::span<unsigned char> bytes) {
        data_ = stbi_load_from_memory(bytes.data(), bytes.size(), &width_, &height_, &numChannels_, 4);
    }

    ~ImageData() {
        stbi_image_free(data_);
    }
private:
    unsigned char* data_;
    int width_ = 0;
    int height_ = 0;
    int numChannels_ = 0;
};


}

namespace yuubi {}
