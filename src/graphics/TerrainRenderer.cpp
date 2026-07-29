#include "graphics/TerrainRenderer.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstddef>
#include <limits>

namespace ian {

void TerrainRenderer::rebuild(
    const TerrainHeightfield& terrain) {
    shutdown();

    const int resolution = terrain.resolution();
    const int vertexCount = resolution * resolution;
    if (vertexCount <= 0 ||
        vertexCount >
            static_cast<int>(
                std::numeric_limits<unsigned short>::max())) {
        return;
    }
    const int cellCount =
        (resolution - 1) * (resolution - 1);
    Mesh mesh{};
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = cellCount * 2;
    mesh.vertices = static_cast<float*>(
        MemAlloc(
            static_cast<unsigned int>(
                vertexCount * 3 *
                static_cast<int>(sizeof(float)))));
    mesh.normals = static_cast<float*>(
        MemAlloc(
            static_cast<unsigned int>(
                vertexCount * 3 *
                static_cast<int>(sizeof(float)))));
    mesh.texcoords = static_cast<float*>(
        MemAlloc(
            static_cast<unsigned int>(
                vertexCount * 2 *
                static_cast<int>(sizeof(float)))));
    mesh.indices = static_cast<unsigned short*>(
        MemAlloc(
            static_cast<unsigned int>(
                mesh.triangleCount * 3 *
                static_cast<int>(
                    sizeof(unsigned short)))));
    if (mesh.vertices == nullptr ||
        mesh.normals == nullptr ||
        mesh.texcoords == nullptr ||
        mesh.indices == nullptr) {
        if (mesh.vertices != nullptr) {
            MemFree(mesh.vertices);
        }
        if (mesh.normals != nullptr) {
            MemFree(mesh.normals);
        }
        if (mesh.texcoords != nullptr) {
            MemFree(mesh.texcoords);
        }
        if (mesh.indices != nullptr) {
            MemFree(mesh.indices);
        }
        return;
    }

    const double halfSize =
        terrain.config().terrainWorldSize * 0.5;
    const double spacing = terrain.spacing();
    for (int z = 0; z < resolution; ++z) {
        for (int x = 0; x < resolution; ++x) {
            const int index = z * resolution + x;
            const double worldX =
                -halfSize +
                static_cast<double>(x) * spacing;
            const double worldZ =
                -halfSize +
                static_cast<double>(z) * spacing;
            const ian::Vec3 normal =
                terrain.getNormal(worldX, worldZ);
            mesh.vertices[index * 3] =
                static_cast<float>(worldX);
            mesh.vertices[index * 3 + 1] =
                static_cast<float>(
                    terrain.getHeight(worldX, worldZ));
            mesh.vertices[index * 3 + 2] =
                static_cast<float>(worldZ);
            mesh.normals[index * 3] =
                static_cast<float>(normal.x);
            mesh.normals[index * 3 + 1] =
                static_cast<float>(normal.y);
            mesh.normals[index * 3 + 2] =
                static_cast<float>(normal.z);
            mesh.texcoords[index * 2] =
                static_cast<float>(
                    static_cast<double>(x) /
                    static_cast<double>(
                        resolution - 1));
            mesh.texcoords[index * 2 + 1] =
                static_cast<float>(
                    static_cast<double>(z) /
                    static_cast<double>(
                        resolution - 1));
        }
    }

    int index = 0;
    for (int z = 0; z < resolution - 1; ++z) {
        for (int x = 0; x < resolution - 1; ++x) {
            const auto northWest =
                static_cast<unsigned short>(
                    z * resolution + x);
            const auto northEast =
                static_cast<unsigned short>(
                    z * resolution + x + 1);
            const auto southWest =
                static_cast<unsigned short>(
                    (z + 1) * resolution + x);
            const auto southEast =
                static_cast<unsigned short>(
                    (z + 1) * resolution + x + 1);
            mesh.indices[index++] = northWest;
            mesh.indices[index++] = southWest;
            mesh.indices[index++] = northEast;
            mesh.indices[index++] = northEast;
            mesh.indices[index++] = southWest;
            mesh.indices[index++] = southEast;
        }
    }

    UploadMesh(&mesh, false);
    model_ = LoadModelFromMesh(mesh);
    ready_ = IsModelValid(model_);
    if (!ready_) {
        UnloadModel(model_);
        model_ = {};
    }
}

void TerrainRenderer::draw(
    Shader shader, Color tint) {
    if (!ready_) {
        return;
    }
    if (shader.id != 0U) {
        for (int index = 0;
             index < model_.materialCount; ++index) {
            model_.materials[index].shader = shader;
        }
    }
    DrawModel(model_, {0.0F, 0.0F, 0.0F}, 1.0F, tint);
}

void TerrainRenderer::drawWireframe(Color tint) const {
    if (!ready_) {
        return;
    }
    DrawModelWires(
        model_, {0.0F, 0.0F, 0.0F}, 1.0F, tint);
}

void TerrainRenderer::shutdown() {
    if (!ready_) {
        return;
    }
    UnloadModel(model_);
    model_ = {};
    ready_ = false;
}

bool TerrainRenderer::ready() const {
    return ready_;
}

} // namespace ian
