#include "renderer/gltf/thread.h"

#include <fastgltf/core.hpp>
#include <stb_image.h>
#include "pch.h"
#include "renderer/vma/image.h"
#include "renderer/resources/texture_manager.h"
#include <thread>
#include "renderer/device.h"
#include <glm/glm.hpp>
#include "renderer/vulkan/util.h"
#include <future>

namespace {
    std::unordered_set<std::size_t> getSrgbImageIndices(std::span<const fastgltf::Material> materials) {
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

    yuubi::StbImageData loadImageData(
        const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& assetDir, bool srgb
    ) {
        yuubi::StbImageData data;
        // Mostly taken from
        // https://github.com/spnda/fastgltf/blob/main/examples/gl_viewer/gl_viewer.cpp
        std::visit(
            fastgltf::visitor{
                [](auto&) {},
                [&assetDir, srgb, &data](const fastgltf::sources::URI& filePath) {
                    assert(filePath.fileByteOffset == 0); // Byte offsets are unsupported.
                    assert(filePath.uri.isLocalPath());
                    const std::string pathString = (assetDir / filePath.uri.fspath()).string();
                    data = yuubi::StbImageData(pathString, srgb);
                },
                [srgb, &data](fastgltf::sources::Array& vector) {
                    const std::span bytes{reinterpret_cast<unsigned char*>(vector.bytes.data()), vector.bytes.size()};
                    data = yuubi::StbImageData(bytes, srgb);
                },
                [&asset, srgb, &data](const fastgltf::sources::BufferView& view) {
                    auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                    auto& buffer = asset.buffers[bufferView.bufferIndex];

                    std::visit(
                        fastgltf::visitor{
                            [](auto& arg) {},
                            [srgb, &data](fastgltf::sources::Array& vector) {
                                const std::span bytes{
                                    reinterpret_cast<unsigned char*>(vector.bytes.data()), vector.bytes.size()
                                };
                                data = yuubi::StbImageData(bytes, srgb);
                            }
                        },
                        buffer.data
                    );
                }
            },
            image.data
        );

        return data;
    }

    std::vector<yuubi::StbImageData> loadImageDataChunk(
        const fastgltf::Asset& asset, std::span<std::pair<fastgltf::Texture, bool>> images,
        const std::filesystem::path& assetDir
    ) {
        return images | std::views::transform([&asset, assetDir](auto&& imageSrgbPair) {
                   const auto [texture, srgb] = imageSrgbPair;
                   const auto& image = asset.images.at(texture.imageIndex.value());
                   return loadImageData(asset, image, assetDir, srgb);
               }) |
               std::ranges::to<std::vector<yuubi::StbImageData>>();
    }

    std::vector<yuubi::StbImageData> loadAllImageData(
        const fastgltf::Asset& asset, const std::filesystem::path& assetDir
    ) {
        auto srgbImageIndices = getSrgbImageIndices(asset.materials);

        auto imageSrgbPair =
            std::views::enumerate(asset.textures) |
            std::views::transform([&asset, &srgbImageIndices](auto&& indexTexturePair) {
                const auto& [i, fastgltfTexture] = indexTexturePair;
                const auto& fastgltfImage = asset.images.at(fastgltfTexture.imageIndex.value());
                return std::pair<fastgltf::Texture, bool>{fastgltfTexture, srgbImageIndices.contains(i)};
            }) |
            std::ranges::to<std::vector<std::pair<fastgltf::Texture, bool>>>();

        const int numThreads = std::thread::hardware_concurrency();
        const int chunkSize = imageSrgbPair.size() / numThreads;
        int remainder = imageSrgbPair.size() % numThreads;

        std::vector<std::future<std::vector<yuubi::StbImageData>>> futures;

        futures.resize(numThreads);
        int start = 0;
        for (int i = 0; i < numThreads; ++i) {
            const int currentChunkSize = chunkSize + (remainder-- > 0 ? 1 : 0);

            std::span pair(imageSrgbPair.data() + start, currentChunkSize);
            futures[i] = std::async(std::launch::async, [&asset, pair, &assetDir]() {
                return loadImageDataChunk(asset, pair, assetDir);
            });
            start += currentChunkSize;
        }

        std::vector<yuubi::StbImageData> result;
        for (auto& future: futures) {
            for (auto partialResult = future.get(); auto&& data: partialResult) {
                result.emplace_back(std::move(data));
            }
        }

        return result;
    }

}

namespace yuubi {
    std::vector<StbImageData> loadTextures(const fastgltf::Asset& asset, const std::filesystem::path& assetDir) {
        return loadAllImageData(asset, assetDir);
    }
}
