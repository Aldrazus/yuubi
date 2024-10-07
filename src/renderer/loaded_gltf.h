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
    uint32_t materialIndex = 0;
};

class Device;
class Mesh : NonCopyable {
public:
    Mesh() = default;
    Mesh(std::string name, Device& device, std::span<Vertex> vertices, std::span<uint32_t> indices, std::vector<GeoSurface>&& surfaces);
    Mesh(Mesh&& rhs) = default;
    Mesh& operator=(Mesh&& rhs) noexcept;

    [[nodiscard]] std::shared_ptr<Buffer> vertexBuffer() const { return vertexBuffer_; }
    [[nodiscard]] std::shared_ptr<Buffer> indexBuffer() const { return indexBuffer_; }
    [[nodiscard]] const std::vector<GeoSurface>& surfaces() const { return surfaces_; }

private:
    std::string name_;
    std::vector<GeoSurface> surfaces_;
    std::shared_ptr<Buffer> vertexBuffer_;
    std::shared_ptr<Buffer> indexBuffer_;
};

}
