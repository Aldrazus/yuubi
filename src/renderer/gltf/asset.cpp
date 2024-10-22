#include "renderer/gltf/asset.h"

#include "renderer/gltf/mikktspace.h"
#include "renderer/gpu_data.h"
#include "renderer/loaded_gltf.h"
#include "renderer/resources/resource_manager.h"
#include "renderer/vertex.h"
#include "renderer/vma/image.h"
#include "renderer/device.h"
#include "renderer/resources/texture_manager.h"
#include "renderer/resources/material_manager.h"
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include "renderer/vulkan_usage.h"
#include "renderer/vulkan/util.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace {

std::unordered_set<std::size_t> getSrgbImageIndices(std::span<fastgltf::Material> materials) {
    std::unordered_set<std::size_t> indices;

    for (const auto& material : materials) {
        if (const auto& baseColorTexture = material.pbrData.baseColorTexture) {
            indices.emplace(baseColorTexture->textureIndex);
        }
        if (const auto& emissiveTexture = material.emissiveTexture) {
            indices.emplace(emissiveTexture->textureIndex);
        }
    }

    return indices;
}

vk::Format getImageFormat(int channels, bool srgb) {
    switch (channels) {
        case 1:
            return vk::Format::eR8Unorm;
        case 2:
        return vk::Format::eR8G8Unorm;
        case 3:
        case 4:
            return srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
        default:
            throw std::runtime_error{"Unsupported image channel"};
    }
}

// PERF: Load images in parallel with stb_image.
std::optional<yuubi::Image> loadImage(
    yuubi::Device& device,
    const fastgltf::Asset& asset,
    const fastgltf::Image& image,
    const std::filesystem::path& assetDir,
    bool srgb
) {
    std::optional<yuubi::Image> newImage;

    int width, height, numChannels;

    // Mostly taken from
    // https://github.com/spnda/fastgltf/blob/main/examples/gl_viewer/gl_viewer.cpp
    std::visit(
        fastgltf::visitor{
            [](auto&) {},
            [&](const fastgltf::sources::URI& filePath) {
                assert(
                    filePath.fileByteOffset == 0
                );  // Byte offsets are unsupported.
                assert(filePath.uri.isLocalPath()
                );  // Only local files are supported.

                const std::string pathString =
                    (assetDir / filePath.uri.fspath()).string();
                unsigned char* data = stbi_load(
                    pathString.c_str(), &width, &height, &numChannels, 0
                );
                if (data != nullptr) {
                    newImage = createImageFromData(
                        device,
                        {
                            .pixels = data,
                            .width = static_cast<uint32_t>(width),
                            .height = static_cast<uint32_t>(height),
                            .numChannels = static_cast<uint32_t>(numChannels),
                            .format = getImageFormat(numChannels, srgb)
                        }
                    );

                    stbi_image_free(data);
                }
            },
            [&](const fastgltf::sources::Array& vector) {
                unsigned char* data = stbi_load_from_memory(
                    reinterpret_cast<const unsigned char*>(vector.bytes.data()),
                    static_cast<int>(vector.bytes.size()), &width, &height,
                    &numChannels, 0
                );

                if (data != nullptr) {
                    newImage = createImageFromData(
                        device,
                        {
                            .pixels = data,
                            .width = static_cast<uint32_t>(width),
                            .height = static_cast<uint32_t>(height),
                            .numChannels = 4,
                            .format = getImageFormat(numChannels, srgb)
                        }
                    );

                    stbi_image_free(data);
                }
            },
            [&](const fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor{
                        [](auto& arg) {},
                        [&](const fastgltf::sources::Array& vector) {
                            unsigned char* data = stbi_load_from_memory(
                                reinterpret_cast<const unsigned char*>(
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
                                        .format = getImageFormat(numChannels, srgb)
                                    }
                                );

                                stbi_image_free(data);
                            }
                        }
                    },
                    buffer.data
                );
            },
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

}

namespace yuubi {

GLTFAsset::GLTFAsset(
    Device& device,
    TextureManager& textureManager,
    MaterialManager& materialManager,
    const std::filesystem::path& filePath
) {
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

    // Check sRGB preference.
    auto srgbImageIndices = getSrgbImageIndices(asset.materials);
    for (auto& i : srgbImageIndices) {
        std::println("i: {}", i);
    }

    // TODO: handle missing images by replacing with error checkerboard
    // PERF: Horrendously slow. MUST FIX.
    UB_INFO("Loading textures...");
    for (const auto& [i, fastgltfTexture]: std::views::enumerate(asset.textures)) {
        // Create image.
        const auto& fastgltfImage =
            asset.images.at(fastgltfTexture.imageIndex.value());
        auto image =
            *loadImage(device, asset, fastgltfImage, filePath.parent_path(), srgbImageIndices.contains(i));

        // Create image view.
        auto imageView = device.createImageView(
            *image.getImage(), image.getImageFormat(),
            vk::ImageAspectFlagBits::eColor
        );

        // Create sampler.
        const auto& gltfSampler =
            asset.samplers.at(fastgltfTexture.samplerIndex.value());
        auto [minFilter, minMipmapMode] = getSamplerFilterInfo(
            gltfSampler.minFilter.value_or(fastgltf::Filter::Nearest)
        );
        auto [magFilter, _] = getSamplerFilterInfo(
            gltfSampler.magFilter.value_or(fastgltf::Filter::Nearest)
        );

        auto sampler = device.getDevice().createSampler(vk::SamplerCreateInfo{
            .magFilter = magFilter,
            .minFilter = minFilter,
            .mipmapMode = minMipmapMode,
            .minLod = 0,
            .maxLod = vk::LodClampNone,
        });

        auto texture = std::make_shared<Texture>(
            std::move(image), std::move(imageView), std::move(sampler)
        );
        UB_INFO("Adding texture {}", textureManager.addResource(texture));
    }
    UB_INFO("Done loading textures...");

    UB_INFO("Loading materials...");
    auto materials =
        asset.materials |
        std::views::transform([](const fastgltf::Material& fastgltfMaterial) {
            // TODO: get more texture data

            auto getTextureIndex =
                [](const std::optional<fastgltf::TextureInfo>& textureInfo
                ) -> uint32_t {
                if (!textureInfo.has_value()) {
                    return 0;
                }

                return textureInfo->textureIndex + 1;
            };

            auto normalTextureIndex = fastgltfMaterial.normalTexture
                                          .transform([](const auto& texture) {
                                              return texture.textureIndex;
                                          })
                                          .value_or(-1) +
                                      1;

            auto normalScale = fastgltfMaterial.normalTexture.transform([](const auto& texture) {
                return texture.scale;
            }).value_or(1);

            return std::make_shared<yuubi::MaterialData>(
                normalTextureIndex,
                normalScale,

                getTextureIndex(fastgltfMaterial.pbrData.baseColorTexture),
                0,
                glm::vec4{
                    fastgltfMaterial.pbrData.baseColorFactor.x(),
                    fastgltfMaterial.pbrData.baseColorFactor.y(),
                    fastgltfMaterial.pbrData.baseColorFactor.z(),
                    fastgltfMaterial.pbrData.baseColorFactor.w()
                },

                getTextureIndex(
                    fastgltfMaterial.pbrData.metallicRoughnessTexture
                ),
                fastgltfMaterial.pbrData.metallicFactor,
                fastgltfMaterial.pbrData.roughnessFactor,
                0
            );
        });

    for (auto&& material : materials) {
        UB_INFO("Adding material {}", materialManager.addResource(material));
    }
    UB_INFO("Done loading materials...");

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
                .count = static_cast<uint32_t>(
                    asset.accessors[primitive.indicesAccessor.value()].count
                )
            };

            size_t initial_vertex = vertices.size();

            // Load indices.
            {
                const fastgltf::Accessor& indexAccessor =
                    asset.accessors[primitive.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                for (uint32_t index : fastgltf::iterateAccessor<uint32_t>(
                         asset, indexAccessor
                     )) {
                    indices.push_back(initial_vertex + index);
                }
            }

            // Load vertex positions.
            {
                const auto* positionIter = primitive.findAttribute("POSITION");
                const auto& positionAccessor =
                    asset.accessors[positionIter->accessorIndex];

                vertices.resize(vertices.size() + positionAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    asset, positionAccessor,
                    [&](glm::vec3 vertex, size_t index) {
                        vertices[initial_vertex + index] = yuubi::Vertex{
                            .position = vertex,
                            .uv_x = 0,
                            .normal = {1.0f, 0.0f, 0.0f},
                            .uv_y = 0,
                            .color = glm::vec4{1.0f},
                        };
                    }
                );
            }

            // Load normals.
            {
                const auto* normalIter = primitive.findAttribute("NORMAL");
                if (normalIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        asset, asset.accessors[(*normalIter).accessorIndex],
                        [&](glm::vec3 normal, size_t index) {
                            vertices[initial_vertex + index].normal = normal;
                        }
                    );
                }
            }

            // Load UVs.
            // TODO: support all texcoords
            {
                const auto* texCoordIter =
                    primitive.findAttribute("TEXCOORD_0");
                if (texCoordIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        asset, asset.accessors[(*texCoordIter).accessorIndex],
                        [&](glm::vec2 uv, size_t index) {
                            vertices[initial_vertex + index].uv_x = uv.x;
                            vertices[initial_vertex + index].uv_y = uv.y;
                        }
                    );
                }
            }

            // Load colors.
            {
                const auto* colorIter = primitive.findAttribute("COLOR_0");
                if (colorIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        asset, asset.accessors[(*colorIter).accessorIndex],
                        [&](glm::vec4 color, size_t index) {
                            vertices[initial_vertex + index].color = color;
                        }
                    );
                }
            }

            {
                const auto* tangentIter = primitive.findAttribute("TANGENT");
                if (tangentIter != primitive.attributes.end()) {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        asset, asset.accessors[(*tangentIter).accessorIndex],
                        [&](glm::vec4 tangent, size_t index) {
                            vertices[initial_vertex + index].tangent = tangent;
                        }
                    );
                }
            }

            // Load material index
            newPrimitive.materialIndex = primitive.materialIndex.value_or(0);

            primitives.push_back(newPrimitive);
        }

        /*
        // Generate tangents.
        generateTangents(MeshData{
            .vertices = vertices,
            .indices = indices
        });
        */

        auto newMesh = std::make_shared<Mesh>(
            mesh.name.c_str(), device, vertices, indices, std::move(primitives)
        );
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

        std::visit(
            fastgltf::visitor{
                [&](fastgltf::math::fmat4x4 matrix) {
                    std::memcpy(
                        &newNode->localTransform, matrix.data(), sizeof(matrix)
                    );
                },
                [&](fastgltf::TRS transform) {
                    glm::vec3 translation{
                        transform.translation[0], transform.translation[1],
                        transform.translation[2]
                    };
                    glm::quat rotation{
                        transform.rotation[3], transform.rotation[0],
                        transform.rotation[1], transform.rotation[2]
                    };
                    glm::vec3 scale{
                        transform.scale[0], transform.scale[1],
                        transform.scale[2]
                    };

                    glm::mat4 translationMatrix =
                        glm::translate(glm::mat4(1.0f), translation);
                    glm::mat4 rotationMatrix = glm::toMat4(rotation);
                    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

                    newNode->localTransform =
                        translationMatrix * rotationMatrix * scaleMatrix;
                }
            },
            node.transform
        );
    }

    for (const auto& [assetNode, sceneNode] :
         std::views::zip(asset.nodes, nodes)) {
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

void GLTFAsset::draw(const glm::mat4& topMatrix, DrawContext& context) {
    for (const auto& node : topNodes_) {
        node->draw(topMatrix, context);
    }
}

}
