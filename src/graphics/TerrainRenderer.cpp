#include "graphics/TerrainRenderer.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <cstring>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace ian {
namespace {

[[nodiscard]] Model buildTriangleModel(
    const std::vector<float>& vertices,
    const std::vector<float>& normals,
    const std::vector<float>& texcoords,
    const std::vector<unsigned char>& colors = {}) {
    if (vertices.empty() ||
        vertices.size() % 9U != 0U ||
        normals.size() != vertices.size() ||
        texcoords.size() * 3U != vertices.size() * 2U) {
        return {};
    }
    Mesh mesh{};
    mesh.vertexCount = static_cast<int>(vertices.size() / 3U);
    mesh.triangleCount = mesh.vertexCount / 3;
    const auto allocateCopy = [](const auto& source) {
        using Value = typename std::decay_t<decltype(source)>::value_type;
        Value* destination = static_cast<Value*>(MemAlloc(
            static_cast<unsigned int>(source.size() * sizeof(Value))));
        if (destination != nullptr) {
            std::memcpy(destination, source.data(),
                        source.size() * sizeof(Value));
        }
        return destination;
    };
    mesh.vertices = allocateCopy(vertices);
    mesh.normals = allocateCopy(normals);
    mesh.texcoords = allocateCopy(texcoords);
    if (!colors.empty()) {
        mesh.colors = allocateCopy(colors);
    }
    if (mesh.vertices == nullptr || mesh.normals == nullptr ||
        mesh.texcoords == nullptr ||
        (!colors.empty() && mesh.colors == nullptr)) {
        if (mesh.vertices != nullptr) MemFree(mesh.vertices);
        if (mesh.normals != nullptr) MemFree(mesh.normals);
        if (mesh.texcoords != nullptr) MemFree(mesh.texcoords);
        if (mesh.colors != nullptr) MemFree(mesh.colors);
        return {};
    }
    UploadMesh(&mesh, false);
    Model model = LoadModelFromMesh(mesh);
    if (!IsModelValid(model)) {
        UnloadModel(model);
        return {};
    }
    return model;
}

} // namespace

TerrainRenderer::~TerrainRenderer() {
    shutdown();
}

void TerrainRenderer::rebuild(
    const TerrainHeightfield& terrain) {
    shutdown();
    terrain_ = &terrain;
    waterModel_ = buildWaterModel();
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

Model TerrainRenderer::buildWaterModel() const {
    if (terrain_ == nullptr || terrain_->ponds().empty()) {
        return {};
    }
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    const double step = std::max(1.35, terrain_->spacing() * 1.6);
    const double shorelineWidth =
        terrain_->config().pondShorelineWidth;
    const auto appendVertex = [&](const PondDefinition& pond,
                                  double x, double z) {
        const double signedDistance =
            terrain_->waterSignedDistance(x, z);
        const double depth = std::clamp(
            (pond.waterLevel - terrain_->getHeight(x, z)) /
                std::max(pond.depth, 0.01),
            0.0, 1.0);
        vertices.insert(vertices.end(), {
            static_cast<float>(x),
            static_cast<float>(pond.waterLevel + 0.035),
            static_cast<float>(z),
        });
        normals.insert(normals.end(), {0.0F, 1.0F, 0.0F});
        texcoords.push_back(static_cast<float>(depth));
        texcoords.push_back(static_cast<float>(
            signedDistance / std::max(shorelineWidth, 0.01)));
    };
    const auto appendCell = [&](const PondDefinition& pond,
                                double x0, double z0,
                                double x1, double z1) {
        appendVertex(pond, x0, z0);
        appendVertex(pond, x0, z1);
        appendVertex(pond, x1, z0);
        appendVertex(pond, x1, z0);
        appendVertex(pond, x0, z1);
        appendVertex(pond, x1, z1);
    };
    for (const PondDefinition& pond : terrain_->ponds()) {
        const double extent =
            std::max(pond.radiusX, pond.radiusZ) +
            pond.bayRadius + step * 2.0;
        const int cells = std::max(
            2, static_cast<int>(std::ceil(extent * 2.0 / step)));
        const double minimumX = pond.x - extent;
        const double minimumZ = pond.z - extent;
        for (int zCell = 0; zCell < cells; ++zCell) {
            const double z0 = minimumZ + zCell * step;
            const double z1 = z0 + step;
            for (int xCell = 0; xCell < cells; ++xCell) {
                const double x0 = minimumX + xCell * step;
                const double x1 = x0 + step;
                const std::array<double, 4> distances{{
                    terrain_->waterSignedDistance(x0, z0),
                    terrain_->waterSignedDistance(x1, z0),
                    terrain_->waterSignedDistance(x0, z1),
                    terrain_->waterSignedDistance(x1, z1),
                }};
                if (*std::min_element(
                        distances.begin(), distances.end()) > step * 1.5) {
                    continue;
                }
                const auto [minimumDistance, maximumDistance] =
                    std::minmax_element(
                        distances.begin(), distances.end());
                constexpr int ShoreSubdivisions = 4;
                const bool nearShore =
                    *minimumDistance < shorelineWidth * 0.35 &&
                    *maximumDistance > -shorelineWidth * 1.15;
                if (!nearShore) {
                    appendCell(pond, x0, z0, x1, z1);
                    continue;
                }
                const double fineStep = step /
                    static_cast<double>(ShoreSubdivisions);
                for (int fineZ = 0; fineZ < ShoreSubdivisions; ++fineZ) {
                    const double fineZ0 = z0 + fineZ * fineStep;
                    const double fineZ1 = fineZ0 + fineStep;
                    for (int fineX = 0;
                         fineX < ShoreSubdivisions; ++fineX) {
                        const double fineX0 = x0 + fineX * fineStep;
                        const double fineX1 = fineX0 + fineStep;
                        appendCell(
                            pond, fineX0, fineZ0, fineX1, fineZ1);
                    }
                }
            }
        }
    }
    return buildTriangleModel(vertices, normals, texcoords);
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

void TerrainRenderer::drawWater(Shader shader) const {
    if (!ready_ || !IsModelValid(waterModel_)) {
        return;
    }
    Model& model = const_cast<Model&>(waterModel_);
    for (int index = 0; index < model.materialCount; ++index) {
        model.materials[index].shader = shader;
        model.materials[index].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    }
    rlDrawRenderBatchActive();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();
    DrawModel(model, {0.0F, 0.0F, 0.0F}, 1.0F, WHITE);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    EndBlendMode();
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
    if (IsModelValid(waterModel_)) {
        UnloadModel(waterModel_);
    }
    waterModel_ = {};
    terrain_ = nullptr;
    ready_ = false;
}

bool TerrainRenderer::ready() const {
    return ready_;
}

} // namespace ian
