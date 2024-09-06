#pragma once

#include "core/util.h"
#include "pch.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include "renderer/vertex.h"
#include "renderer/vma/buffer.h"

namespace yuubi {

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
};

class Device;
class Mesh : NonCopyable {
public:
    Mesh() = default;
    Mesh(Device& device, std::span<Vertex> vertices, std::span<uint32_t> indices, const std::vector<GeoSurface>& surfaces);
    Mesh(Mesh&& rhs) = default;
    Mesh& operator=(Mesh&& rhs) noexcept;

    [[nodiscard]] const Buffer& vertexBuffer() const { return vertexBuffer_; }
    [[nodiscard]] const Buffer& indexBuffer() const { return indexBuffer_; }
    [[nodiscard]] const std::vector<GeoSurface>& surfaces() const { return surfaces_; }

private:
    std::string name_;
    std::vector<GeoSurface> surfaces_;
    Buffer vertexBuffer_;
    Buffer indexBuffer_;
};

std::optional<std::vector<std::shared_ptr<Mesh>>> loadGltfMeshes(
    Device& device,
    const std::filesystem::path& path
);

}
