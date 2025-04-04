#include "renderer/gltf/mikktspace.h"

#include <mikktspace.h>

#include "renderer/vertex.h"
#include "pch.h"


namespace {

    int getNumFaces(const SMikkTSpaceContext* context) {
        const auto mesh = static_cast<yuubi::MeshData*>(context->m_pUserData);
        return mesh->indices.size() / 3;
    }

    int getNumVerticesOfFace(const SMikkTSpaceContext* context, const int iFace) {
        return 3; // Only triangle primitives are supported.
    }

    void getPosition(const SMikkTSpaceContext* context, float fvPosOut[], const int iFace, const int iVert) {
        const yuubi::MeshData* mesh = static_cast<yuubi::MeshData*>(context->m_pUserData);
        const auto index = mesh->indices[iFace * 3 + iVert];
        const auto position = mesh->vertices[index].position;
        fvPosOut[0] = position.x;
        fvPosOut[1] = position.y;
        fvPosOut[2] = position.z;
    }

    void getNormal(const SMikkTSpaceContext* context, float fvNormOut[], const int iFace, const int iVert) {
        const yuubi::MeshData* mesh = static_cast<yuubi::MeshData*>(context->m_pUserData);
        const auto index = mesh->indices[iFace * 3 + iVert];
        const auto normal = mesh->vertices[index].normal;
        fvNormOut[0] = normal.x;
        fvNormOut[1] = normal.y;
        fvNormOut[2] = normal.z;
    }

    void getTexCoord(const SMikkTSpaceContext* context, float fvTexcOut[], const int iFace, const int iVert) {
        const yuubi::MeshData* mesh = static_cast<yuubi::MeshData*>(context->m_pUserData);
        const auto index = mesh->indices[iFace * 3 + iVert];
        const auto vertex = mesh->vertices[index];
        fvTexcOut[0] = vertex.uv_x;
        fvTexcOut[1] = vertex.uv_y;
    }

    void setTSSpaceBasic(
        const SMikkTSpaceContext* context, const float fvTangent[], const float fSign, const int iFace, const int iVert
    ) {
        yuubi::MeshData* mesh = static_cast<yuubi::MeshData*>(context->m_pUserData);
        const auto index = mesh->indices[iFace * 3 + iVert];
        auto& vertex = mesh->vertices[index];
        vertex.tangent = glm::vec4(fvTangent[0], fvTangent[1], fvTangent[2], fSign);
    }

    SMikkTSpaceInterface interface{
        .m_getNumFaces = getNumFaces,
        .m_getNumVerticesOfFace = getNumVerticesOfFace,
        .m_getPosition = getPosition,
        .m_getNormal = getNormal,
        .m_getTexCoord = getTexCoord,
        .m_setTSpaceBasic = setTSSpaceBasic
    };

}

namespace yuubi {

    void generateTangents(MeshData mesh) {
        const SMikkTSpaceContext context{.m_pInterface = &interface, .m_pUserData = &mesh};

        genTangSpaceDefault(&context);
    }

}
