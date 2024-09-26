#include "renderer/loaded_gltf.h"
#include "renderer/device.h"

namespace yuubi {

Mesh::Mesh(
    Device& device,
    std::span<Vertex> vertices,
    std::span<uint32_t> indices,
    const std::vector<GeoSurface>& surfaces
)
    : surfaces_(surfaces) {
    // Vertex buffer.
    {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        // Create vertex buffer.
        vk::BufferCreateInfo vertexBufferCreateInfo{
            .size = bufferSize,
            .usage = vk::BufferUsageFlagBits::eStorageBuffer |
                     vk::BufferUsageFlagBits::eTransferDst |
                    vk::BufferUsageFlagBits::eShaderDeviceAddress
        };

        VmaAllocationCreateInfo vertexBufferAllocCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        };

        vertexBuffer_ = device.createBuffer(
            vertexBufferCreateInfo, vertexBufferAllocCreateInfo
        );

        vertexBuffer_.upload(device, vertices.data(), bufferSize, 0);
    }

    // Index buffer.
    {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        // Create index buffer.
        vk::BufferCreateInfo indexBufferCreateInfo{
            .size = bufferSize,
            .usage = vk::BufferUsageFlagBits::eIndexBuffer |
                     vk::BufferUsageFlagBits::eTransferDst
        };

        VmaAllocationCreateInfo indexBufferAllocCreateInfo{
            .usage = VMA_MEMORY_USAGE_GPU_ONLY
        };

        indexBuffer_ = device.createBuffer(
            indexBufferCreateInfo, indexBufferAllocCreateInfo
        );

        indexBuffer_.upload(device, indices.data(), bufferSize, 0);
    }
}

Mesh& Mesh::operator=(Mesh&& rhs) noexcept {
    if (this != &rhs) {
        std::swap(vertexBuffer_, rhs.vertexBuffer_);
        std::swap(indexBuffer_, rhs.indexBuffer_);
        std::swap(surfaces_, rhs.surfaces_);
    }

    return *this;
}

std::optional<std::vector<std::shared_ptr<Mesh>>> loadGltfMeshes(
    Device& device, const std::filesystem::path& path
) {
    UB_INFO("Loading GLTF file: {}", path.string());

    fastgltf::Parser parser;

    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None) {
        UB_ERROR("Unable to load file: {}", path.string());
        return {};
    }

    constexpr auto gltfOptions  = fastgltf::Options::LoadExternalBuffers;
    auto asset = parser.loadGltf(
        data.get(), path.parent_path(), gltfOptions
    );
    if (auto error = asset.error(); error != fastgltf::Error::None) {
        UB_ERROR("Unable to parse file: {}", path.string());
        return {};
    }

    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (auto&& mesh : asset->meshes) {
        std::vector<GeoSurface> surfaces;
        indices.clear();
        vertices.clear();

        for (auto&& primitive : mesh.primitives) {
            GeoSurface newSurface{};
            newSurface.startIndex = indices.size();

            size_t initialVertex = vertices.size();

            // load indices
            {
                auto& indexAccessor =
                    asset->accessors[primitive.indicesAccessor.value()];
                newSurface.count = indexAccessor.count;
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<uint32_t>(
                    asset.get(), indexAccessor,
                    [&](std::uint32_t index) {
                        indices.push_back(index + initialVertex);
                    }
                );
            }

            // load vertex positions
            {
                auto* positionIterator = primitive.findAttribute("POSITION");
                auto& positionAccessor =
                    asset->accessors[positionIterator->accessorIndex];

                vertices.resize(vertices.size() + positionAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    asset.get(), positionAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex vertex{
                            .position = v,
                            .uv_x = 0.0f,
                            .normal = {1.0f, 0.0f, 0.0f},
                            .uv_y = 0.0f,
                            .color = glm::vec4{1.0f},
                        };

                        vertices[initialVertex + index] = vertex;
                    }
                );
            }

            // Load vertex normals.
            {
                auto* normalIterator = primitive.findAttribute("NORMAL");
                if (normalIterator != primitive.attributes.end()) {
                    auto& normalAccessor =
                        asset->accessors[normalIterator->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        asset.get(), normalAccessor,
                        [&](glm::vec3 normal, size_t index) {
                            vertices[initialVertex + index].normal = normal;
                        }
                    );
                }
            }

            // Load texture coordinates.
            {
                auto* texCoordIterator = primitive.findAttribute("TEXCOORD_0");
                if (texCoordIterator != primitive.attributes.end()) {
                    auto& texCoordAccessor =
                        asset->accessors[texCoordIterator->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        asset.get(), texCoordAccessor,
                        [&](glm::vec2 texCoords, size_t index) {
                            vertices[initialVertex + index].uv_x = texCoords.x;
                            vertices[initialVertex + index].uv_y = texCoords.y;
                        }
                    );
                }
            }

            // Load vertex colors.
            {
                auto* colorIterator = primitive.findAttribute("COLOR_0");
                if (colorIterator != primitive.attributes.end()) {
                    auto& colorAccessor =
                        asset->accessors[colorIterator->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        asset.get(), colorAccessor,
                        [&](glm::vec4 color, size_t index) {
                            vertices[initialVertex + index].color = color;
                        }
                    );
                }
            }

            // Display normal as color
            for (Vertex& vertex : vertices) {
                vertex.color = glm::vec4(vertex.normal, 1.0f);
            }

            surfaces.push_back(newSurface);
        }

        meshes.emplace_back(
            std::make_shared<Mesh>(device, vertices, indices, surfaces)
        );
    }

    UB_INFO("Successfully parsed GLTF file.");
    UB_INFO("Num indices: {}, numVertices: {}", indices.size(), vertices.size());
    return meshes;
}
}
