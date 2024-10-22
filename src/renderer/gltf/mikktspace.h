#pragma once

#include <mikktspace.h>
#include "renderer/loaded_gltf.h"
#include "pch.h"

namespace yuubi {

struct MeshData {
    std::span<Vertex> vertices;
    std::span<uint32_t> indices;
};

void generateTangents(MeshData mesh);

}
