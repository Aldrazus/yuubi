#include "renderer/gltf/asset.h"

#include "renderer/gpu_data.h"
#include "renderer/loaded_gltf.h"
#include "renderer/vertex.h"
#include "renderer/vma/image.h"
#include "renderer/device.h"
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include "renderer/vulkan_usage.h"
#include "renderer/vulkan/util.h"
#include <stb_image.h>

namespace {

struct ImageData {
    // TODO: use std::byte?
    unsigned char* pixels;
    uint32_t width;
    uint32_t height;
    uint32_t numChannels;
};

// PERF: Use staging buffer pool. Use dedicated transfer queue.
yuubi::Image createImageFromData(yuubi::Device& device, const ImageData& data) {
    // Create staging buffer.
    vk::DeviceSize imageSize = data.width * data.height * 4;

    vk::BufferCreateInfo stagingBufferCreateInfo{
        .size = imageSize,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
    };

    VmaAllocationCreateInfo stagingBufferAllocCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    yuubi::Buffer stagingBuffer = device.createBuffer(
        stagingBufferCreateInfo, stagingBufferAllocCreateInfo
    );

    // Copy image data onto mapped memory in staging buffer.
    std::memcpy(
        stagingBuffer.getMappedMemory(), data.pixels,
        static_cast<size_t>(imageSize)
    );

    // Create image.
    yuubi::Image image(
        &device.allocator(),
        yuubi::ImageCreateInfo{
            .width = data.width,
            .height = data.height,
            .format = vk::Format::eR8G8B8A8Srgb,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferDst,
            .properties = vk::MemoryPropertyFlagBits::eDeviceLocal
        }
    );

    // Upload data to image.
    device.submitImmediateCommands(
        [&stagingBuffer, &image,
         &data](const vk::raii::CommandBuffer& commandBuffer) {
            yuubi::transitionImage(
                commandBuffer, *image.getImage(), vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal
            );

            vk::BufferImageCopy copyRegion{
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource =
                    vk::ImageSubresourceLayers{
                                               .aspectMask = vk::ImageAspectFlagBits::eColor,
                                               .mipLevel = 0,
                                               .baseArrayLayer = 0,
                                               .layerCount = 1
                    },
                .imageOffset = {0, 0, 0},
                .imageExtent =
                    vk::Extent3D{
                                               .width = data.width, .height = data.height, .depth = 1
                    }
            };
            commandBuffer.copyBufferToImage(
                *stagingBuffer.getBuffer(), *image.getImage(),
                vk::ImageLayout::eTransferDstOptimal, {copyRegion}
            );

            yuubi::transitionImage(
                commandBuffer, *image.getImage(),
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal
            );
        }
    );

    return image;
}

// PERF: Load images in parallel with stb_image.
std::optional<yuubi::Image> loadImage(
    yuubi::Device& device, const fastgltf::Asset& asset, const fastgltf::Image& image
) {
    std::optional<yuubi::Image> newImage;

    int width, height, numChannels;

    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(
                    filePath.fileByteOffset == 0
                );  // Byte offsets are unsupported.
                assert(filePath.uri.isLocalPath()
                );  // Only local files are supported.

                const std::string pathString{filePath.uri.path()};
                unsigned char* data = stbi_load(
                    pathString.c_str(), &width, &height, &numChannels, 4
                );
                if (data != nullptr) {
                    newImage = createImageFromData(
                        device,
                        {
                            .pixels = data,
                            .width = static_cast<uint32_t>(width),
                            .height = static_cast<uint32_t>(height),
                            .numChannels = 4,
                        }
                    );

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* data = stbi_load_from_memory(
                    reinterpret_cast<unsigned char*>(vector.bytes.data()),
                    static_cast<int>(vector.bytes.size()), &width, &height,
                    &numChannels, 4
                );

                if (data != nullptr) {
                    newImage = createImageFromData(
                        device,
                        {
                            .pixels = data,
                            .width = static_cast<uint32_t>(width),
                            .height = static_cast<uint32_t>(height),
                            .numChannels = 4,
                        }
                    );

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor{
                        [](auto& arg) {},
                        [&](fastgltf::sources::Vector& vector) {
                            unsigned char* data = stbi_load_from_memory(
                                reinterpret_cast<unsigned char*>(
                                    vector.bytes.data() + bufferView.byteOffset
                                ),
                                static_cast<int>(bufferView.byteLength), &width,
                                &height, &numChannels, 4
                            );
                            if (data != nullptr) {
                                newImage = createImageFromData(
                                    device,
                                    {
                                        .pixels = data,
                                        .width = static_cast<uint32_t>(width),
                                        .height = static_cast<uint32_t>(height),
                                        .numChannels = 4,
                                    }
                                );

                                stbi_image_free(data);
                            }
                        }
                    },
                    buffer.data
                );
            }
        },
        image.data
    );

    return newImage;
}

std::pair<vk::Filter, vk::SamplerMipmapMode> getSamplerFilterInfo(
    fastgltf::Filter filter
) {
    switch (filter) {
        case fastgltf::Filter::Nearest:
            return {vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear};
        case fastgltf::Filter::NearestMipMapNearest:
            return {vk::Filter::eNearest, vk::SamplerMipmapMode::eNearest};
        case fastgltf::Filter::NearestMipMapLinear:
            return {vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear};

        case fastgltf::Filter::Linear:
            return {vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};
        case fastgltf::Filter::LinearMipMapNearest:
            return {vk::Filter::eLinear, vk::SamplerMipmapMode::eNearest};
        case fastgltf::Filter::LinearMipMapLinear:
            return {vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear};

        default:
            return {vk::Filter::eNearest, vk::SamplerMipmapMode::eLinear};
    }
}

std::vector<vk::raii::Sampler> loadSamplers(
    yuubi::Device& device, std::span<fastgltf::Sampler> samplers
) {
    return samplers | std::views::transform([&device](auto const& sampler) {
               auto [minFilter, minMipmapMode] = getSamplerFilterInfo(
                   sampler.minFilter.value_or(fastgltf::Filter::Nearest)
               );
               auto [magFilter, _] = getSamplerFilterInfo(
                   sampler.magFilter.value_or(fastgltf::Filter::Nearest)
               );

               return device.getDevice().createSampler(vk::SamplerCreateInfo{
                   .magFilter = magFilter,
                   .minFilter = minFilter,
                   .mipmapMode = minMipmapMode,
                   .minLod = 0,
                   .maxLod = vk::LodClampNone,
               });
           }) |
           std::ranges::to<std::vector>();
}

std::vector<yuubi::MaterialData> loadMaterials(const std::vector<fastgltf::Material>& materials) {
    return materials | std::views::transform([](const fastgltf::Material& material) {
        // TODO: get more texture data
        return yuubi::MaterialData{
            .baseColor = glm::vec4{material.pbrData.baseColorFactor.x(), material.pbrData.baseColorFactor.y(), material.pbrData.baseColorFactor.z(), material.pbrData.baseColorFactor.w()},
            .diffuseTex = 1,
            .metallicRoughnessTex = 1
        };
    }) | std::ranges::to<std::vector<yuubi::MaterialData>>();
}

std::vector<yuubi::Mesh> loadMeshes(const fastgltf::Asset& asset) {
    std::vector<uint32_t> indices;
    std::vector<yuubi::Vertex> vertices;

    for (const fastgltf::Mesh& mesh : asset.meshes) {
        indices.clear();
        vertices.clear();

        for (auto&& primitive : mesh.primitives) {
            yuubi::GeoSurface newPrimitive;
            newPrimitive.startIndex = static_cast<uint32_t>(indices.size());
            newPrimitive.count = static_cast<uint32_t>(asset.accessors[primitive.indicesAccessor.value()].count);
            size_t initial_vertex = vertices.size();

            // Load indices.
            {
                const fastgltf::Accessor& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                for (uint32_t index : fastgltf::iterateAccessor<uint32_t>(asset, indexAccessor)) {
                    indices.push_back(initial_vertex + index);
                }
            }

            // Load vertex positions.
            {
                const auto* positionIter = primitive.findAttribute("POSITION");
                const auto& positionAccessor = asset.accessors[positionIter->accessorIndex];

                vertices.resize(vertices.size() + positionAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor, [&](glm::vec3 vertex, size_t index) {
                    vertices[initial_vertex + index] = yuubi::Vertex{
                        .position = vertex,
                        .uv_x = 0,
                        .normal = {1.0f, 0.0f, 0.0f},
                        .uv_y = 0,
                        .color = glm::vec4{1.0f},
                    };
                });
            }

            // Load normals.
            {
                const auto* normalIter = primitive.findAttribute("NORMAL");
                if (normalIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[(*normalIter).accessorIndex], [&](glm::vec3 normal, size_t index) {
                        vertices[initial_vertex + index].normal = normal;
                    });
                }
            }

            // Load UVs.
            // TODO: support all texcoords
            {
                const auto* texCoordIter = primitive.findAttribute("TEXCOORD_0");
                if (texCoordIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[(*texCoordIter).accessorIndex], [&](glm::vec2 uv, size_t index) {
                        vertices[initial_vertex + index].uv_x = uv.x;
                        vertices[initial_vertex + index].uv_y = uv.y;
                    });
                }
            }

            // Load colors.
            {
                const auto* colorIter = primitive.findAttribute("COLOR_0");
                if (colorIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[(*colorIter).accessorIndex], [&](glm::vec4 color, size_t index){
                        vertices[initial_vertex + index].color = color;
                    });
                }
            }

            // Load material index
            newPrimitive.materialIndex = primitive.materialIndex.value_or(0);

        }
    }

    return {};
}

}

namespace yuubi {

GLTFAsset::GLTFAsset(Device& device, const std::filesystem::path& filePath) {
    UB_INFO("Loading GLTF file: {}", filePath.string());

    fastgltf::Parser parser;

    auto data = fastgltf::GltfDataBuffer::FromPath(filePath.string());
    if (data.error() != fastgltf::Error::None) {
        UB_ERROR("Unable to load file: {}", filePath.string());
        // TODO: throw
    }

    constexpr auto gltfOptions =
        fastgltf::Options::DontRequireValidAssetMember |
        fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;
    auto loadedGltf =
        parser.loadGltf(data.get(), filePath.parent_path(), gltfOptions);
    if (auto error = loadedGltf.error(); error != fastgltf::Error::None) {
        UB_ERROR("Unable to parse file: {}", filePath.string());
        // TODO: throw
    }
    auto asset = std::move(loadedGltf.get());

    // TODO: handle missing images by replacing with error checkerboard
    auto images = asset.images | std::views::transform([&device, &asset](const fastgltf::Image& image){
        return loadImage(device, asset, image);
    }) | std::views::filter([](auto&& image){ return image.has_value(); }) | std::views::transform([](auto&& image){ return std::move(*image); }) | std::ranges::to<std::vector<Image>>();

    auto samplers = loadSamplers(device, asset.samplers);

    // TODO: add textures to bindless descriptor set manager

    std::vector<std::shared_ptr<Mesh>> meshes;
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    
    for (const fastgltf::Mesh& mesh : asset.meshes) {
        indices.clear();
        vertices.clear();

        std::vector<GeoSurface> primitives;

        for (auto&& primitive : mesh.primitives) {
            yuubi::GeoSurface newPrimitive{
                .startIndex = static_cast<uint32_t>(indices.size()),
                .count = static_cast<uint32_t>(asset.accessors[primitive.indicesAccessor.value()].count)
            };

            size_t initial_vertex = vertices.size();

            // Load indices.
            {
                const fastgltf::Accessor& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                for (uint32_t index : fastgltf::iterateAccessor<uint32_t>(asset, indexAccessor)) {
                    indices.push_back(initial_vertex + index);
                }
            }

            // Load vertex positions.
            {
                const auto* positionIter = primitive.findAttribute("POSITION");
                const auto& positionAccessor = asset.accessors[positionIter->accessorIndex];

                vertices.resize(vertices.size() + positionAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor, [&](glm::vec3 vertex, size_t index) {
                    vertices[initial_vertex + index] = yuubi::Vertex{
                        .position = vertex,
                        .uv_x = 0,
                        .normal = {1.0f, 0.0f, 0.0f},
                        .uv_y = 0,
                        .color = glm::vec4{1.0f},
                    };
                });
            }

            // Load normals.
            {
                const auto* normalIter = primitive.findAttribute("NORMAL");
                if (normalIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[(*normalIter).accessorIndex], [&](glm::vec3 normal, size_t index) {
                        vertices[initial_vertex + index].normal = normal;
                    });
                }
            }

            // Load UVs.
            // TODO: support all texcoords
            {
                const auto* texCoordIter = primitive.findAttribute("TEXCOORD_0");
                if (texCoordIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[(*texCoordIter).accessorIndex], [&](glm::vec2 uv, size_t index) {
                        vertices[initial_vertex + index].uv_x = uv.x;
                        vertices[initial_vertex + index].uv_y = uv.y;
                    });
                }
            }

            // Load colors.
            {
                const auto* colorIter = primitive.findAttribute("COLOR_0");
                if (colorIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[(*colorIter).accessorIndex], [&](glm::vec4 color, size_t index){
                        vertices[initial_vertex + index].color = color;
                    });
                }
            }

            // Load material index
            newPrimitive.materialIndex = primitive.materialIndex.value_or(0);

            primitives.push_back(newPrimitive);
        }

        auto newMesh = std::make_shared<Mesh>(mesh.name.c_str(), device, vertices, indices, std::move(primitives));
        meshes.push_back(newMesh);
        meshes_[mesh.name.c_str()] = newMesh;
    }

    std::vector<std::shared_ptr<Node>> nodes;

    for (fastgltf::Node& node : asset.nodes) {
        std::shared_ptr<Node> newNode;

        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>(meshes[*node.meshIndex]);
        } else {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        nodes_[node.name.c_str()] = newNode;

        std::visit(fastgltf::visitor {
            [&](fastgltf::math::fmat4x4 matrix) {
                std::memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
            },
            [&](fastgltf::TRS transform){
                glm::vec3 translation{
                    transform.translation[0], transform.translation[1], transform.translation[2]
                };
                glm::quat rotation{
                    transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]
                };
                glm::vec3 scale{
                    transform.scale[0], transform.scale[1], transform.scale[2]
                };

                glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), translation);
                glm::mat4 rotationMatrix = glm::toMat4(rotation);
                glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

                newNode->localTransform = translationMatrix * rotationMatrix * scaleMatrix;
            }
        }, node.transform);
    }

    for (const auto& [assetNode, sceneNode] : std::views::zip(asset.nodes, nodes)) {
        for (auto& childIndex : assetNode.children) {
            auto& childNode = nodes[childIndex];
            sceneNode->children.push_back(childNode);
            childNode->parent = sceneNode;
        }
    }

    for (auto& node : nodes) {
        if (node->parent.lock() == nullptr) {
            topNodes_.push_back(node);
            node->refreshTransform(glm::mat4{1.0f});
        }
    }
}

void GLTFAsset::draw(const glm::mat4& topMatrix, DrawContext& context)
{
    for (const auto& node : topNodes_) {
        node->draw(topMatrix, context);
    }
}

} 
