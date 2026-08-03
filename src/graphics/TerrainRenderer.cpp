#include "graphics/TerrainRenderer.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace ian {

TerrainRenderer::~TerrainRenderer() {
    shutdown();
}

void TerrainRenderer::rebuild(
    const TerrainHeightfield& terrain) {
    shutdown();
    terrain_ = &terrain;
    ready_ = true;
}

Model TerrainRenderer::buildChunk(
    int chunkX, int chunkZ) const {
    if (terrain_ == nullptr) {
        return {};
    }
    const auto& config = terrain_->config();
    const double halfSize = config.terrainWorldSize * 0.5;
    const double minimumX = -halfSize +
        static_cast<double>(chunkX) *
            config.terrainChunkWorldSize;
    const double minimumZ = -halfSize +
        static_cast<double>(chunkZ) *
            config.terrainChunkWorldSize;
    const double maximumX = std::min(
        minimumX + config.terrainChunkWorldSize, halfSize);
    const double maximumZ = std::min(
        minimumZ + config.terrainChunkWorldSize, halfSize);
    const double width = maximumX - minimumX;
    const double depth = maximumZ - minimumZ;
    if (width <= 0.0 || depth <= 0.0) {
        return {};
    }
    const int cellsX = std::clamp(
        static_cast<int>(std::ceil(width / terrain_->spacing())),
        2, 128);
    const int cellsZ = std::clamp(
        static_cast<int>(std::ceil(depth / terrain_->spacing())),
        2, 128);
    const int resolutionX = cellsX + 1;
    const int resolutionZ = cellsZ + 1;
    const int vertexCount = resolutionX * resolutionZ;
    if (vertexCount <= 0 ||
        vertexCount >
            static_cast<int>(
                std::numeric_limits<unsigned short>::max())) {
        return {};
    }
    const int cellCount = cellsX * cellsZ;
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
        return {};
    }

    for (int z = 0; z < resolutionZ; ++z) {
        for (int x = 0; x < resolutionX; ++x) {
            const int index = z * resolutionX + x;
            const double worldX =
                minimumX + width *
                    static_cast<double>(x) /
                    static_cast<double>(cellsX);
            const double worldZ =
                minimumZ + depth *
                    static_cast<double>(z) /
                    static_cast<double>(cellsZ);
            const ian::Vec3 normal =
                terrain_->getNormal(worldX, worldZ);
            mesh.vertices[index * 3] =
                static_cast<float>(worldX);
            mesh.vertices[index * 3 + 1] =
                static_cast<float>(
                    terrain_->getHeight(worldX, worldZ));
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
                    (worldX + halfSize) /
                    config.terrainWorldSize);
            mesh.texcoords[index * 2 + 1] =
                static_cast<float>(
                    (worldZ + halfSize) /
                    config.terrainWorldSize);
        }
    }

    int index = 0;
    for (int z = 0; z < cellsZ; ++z) {
        for (int x = 0; x < cellsX; ++x) {
            const auto northWest =
                static_cast<unsigned short>(
                    z * resolutionX + x);
            const auto northEast =
                static_cast<unsigned short>(
                    z * resolutionX + x + 1);
            const auto southWest =
                static_cast<unsigned short>(
                    (z + 1) * resolutionX + x);
            const auto southEast =
                static_cast<unsigned short>(
                    (z + 1) * resolutionX + x + 1);
            mesh.indices[index++] = northWest;
            mesh.indices[index++] = southWest;
            mesh.indices[index++] = northEast;
            mesh.indices[index++] = northEast;
            mesh.indices[index++] = southWest;
            mesh.indices[index++] = southEast;
        }
    }

    UploadMesh(&mesh, false);
    Model model = LoadModelFromMesh(mesh);
    if (!IsModelValid(model)) {
        UnloadModel(model);
        return {};
    }
    return model;
}

void TerrainRenderer::updateVisibleChunks(
    Vector3 focusPosition) {
    if (terrain_ == nullptr) {
        return;
    }
    const auto& config = terrain_->config();
    const double chunkSize = config.terrainChunkWorldSize;
    const int chunkCount = std::max(
        1, static_cast<int>(
               std::ceil(config.terrainWorldSize / chunkSize)));
    (void)focusPosition;
    for (int z = 0; z < chunkCount; ++z) {
        for (int x = 0; x < chunkCount; ++x) {
            const auto existing = std::find_if(
                chunks_.begin(), chunks_.end(),
                [x, z](const TerrainChunk& chunk) {
                    return chunk.x == x && chunk.z == z;
                });
            if (existing != chunks_.end()) {
                continue;
            }
            Model model = buildChunk(x, z);
            if (IsModelValid(model)) {
                chunks_.push_back({x, z, model});
            }
        }
    }

}

void TerrainRenderer::draw(
    Shader shader, Color tint,
    Vector3 focusPosition) {
    if (!ready_) {
        return;
    }
    updateVisibleChunks(focusPosition);
    for (auto& chunk : chunks_) {
        if (shader.id != 0U) {
            for (int index = 0;
                 index < chunk.model.materialCount; ++index) {
                chunk.model.materials[index].shader = shader;
            }
        }
        DrawModel(
            chunk.model, {0.0F, 0.0F, 0.0F},
            1.0F, tint);
    }
}

void TerrainRenderer::drawWireframe(Color tint) const {
    if (!ready_) {
        return;
    }
    for (const auto& chunk : chunks_) {
        DrawModelWires(
            chunk.model, {0.0F, 0.0F, 0.0F},
            1.0F, tint);
    }
}

void TerrainRenderer::shutdown() {
    for (auto& chunk : chunks_) {
        if (IsModelValid(chunk.model)) {
            UnloadModel(chunk.model);
        }
    }
    chunks_.clear();
    terrain_ = nullptr;
    ready_ = false;
}

bool TerrainRenderer::ready() const {
    return ready_;
}

} // namespace ian
