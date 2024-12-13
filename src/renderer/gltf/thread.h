#pragma once

#include "pch.h"
#include <fastgltf/types.hpp>
#include "renderer/resources/texture_manager.h"


namespace yuubi {
    class Device;
    std::vector<Texture> loadTextures(
            const Device& device, const fastgltf::Asset& asset, const std::filesystem::path& assetDir
    );
}
