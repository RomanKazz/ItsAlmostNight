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

[[nodiscard]] std::uint32_t mountainHash(
    int x, int z, std::uint32_t seed) {
    std::uint32_t value = seed ^
        static_cast<std::uint32_t>(x) * 0x9e3779b9U ^
        static_cast<std::uint32_t>(z) * 0x85ebca6bU;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    return value ^ (value >> 16U);
}

[[nodiscard]] double mountainValueNoise(
    double x, double z, std::uint32_t seed) {
    const int cellX = static_cast<int>(std::floor(x));
    const int cellZ = static_cast<int>(std::floor(z));
    double blendX = x - static_cast<double>(cellX);
    double blendZ = z - static_cast<double>(cellZ);
    blendX = blendX * blendX * (3.0 - 2.0 * blendX);
    blendZ = blendZ * blendZ * (3.0 - 2.0 * blendZ);
    const auto sample = [seed](int sampleX, int sampleZ) {
        return static_cast<double>(
                   mountainHash(sampleX, sampleZ, seed)) /
                   static_cast<double>(
                       std::numeric_limits<std::uint32_t>::max()) *
                   2.0 - 1.0;
    };
    const double north = sample(cellX, cellZ) +
        (sample(cellX + 1, cellZ) - sample(cellX, cellZ)) * blendX;
    const double south = sample(cellX, cellZ + 1) +
        (sample(cellX + 1, cellZ + 1) -
         sample(cellX, cellZ + 1)) * blendX;
    return north + (south - north) * blendZ;
}

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
    buildPathMask();
    mountainBackdropModel_ = buildMountainBackdrop();
    waterModel_ = buildWaterModel();
    ready_ = true;
    // Build and upload the finite terrain during loading. Doing this lazily
    // from the first shadow/world pass creates a large single-frame stall.
    buildChunks();
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
    mesh.colors = static_cast<unsigned char*>(
        MemAlloc(
            static_cast<unsigned int>(
                vertexCount * 4 *
                static_cast<int>(sizeof(unsigned char)))));
    mesh.indices = static_cast<unsigned short*>(
        MemAlloc(
            static_cast<unsigned int>(
                mesh.triangleCount * 3 *
                static_cast<int>(
                    sizeof(unsigned short)))));
    if (mesh.vertices == nullptr ||
        mesh.normals == nullptr ||
        mesh.texcoords == nullptr ||
        mesh.colors == nullptr ||
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
        if (mesh.colors != nullptr) {
            MemFree(mesh.colors);
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
            const double height =
                terrain_->getHeight(worldX, worldZ);
            mesh.vertices[index * 3] =
                static_cast<float>(worldX);
            mesh.vertices[index * 3 + 1] =
                static_cast<float>(height);
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
            const double shoreDistance =
                terrain_->waterSignedDistance(worldX, worldZ);
            const double wetShore = std::clamp(
                1.0 - std::max(shoreDistance, 0.0) / 3.6,
                0.0, 1.0);
            const auto shade = static_cast<unsigned char>(
                std::lround(255.0 - wetShore * 36.0));
            const double boundaryStart =
                halfSize - config.terrainBoundaryRiseWidth;
            const double edgeDistance =
                std::max(std::abs(worldX), std::abs(worldZ));
            const double boundaryProgress = std::clamp(
                (edgeDistance - boundaryStart) /
                    std::max(config.terrainBoundaryRiseWidth, 0.001),
                0.0, 1.0);
            const double mountainInput = std::clamp(
                (boundaryProgress - 0.22) / 0.58, 0.0, 1.0);
            const double mountainAmount =
                mountainInput * mountainInput *
                (3.0 - 2.0 * mountainInput);
            const double snowHeight = std::clamp(
                (height - 32.0) / 15.0, 0.0, 1.0);
            const double snowSurface = std::clamp(
                (normal.y - 0.38) / 0.30, 0.0, 1.0);
            const double snowAmount =
                mountainAmount * snowHeight * snowSurface;
            if (mountainAmount > 0.001) {
                mesh.colors[index * 4] = 255U;
                mesh.colors[index * 4 + 1] =
                    static_cast<unsigned char>(std::lround(
                        255.0 * (1.0 - snowAmount)));
                mesh.colors[index * 4 + 2] =
                    static_cast<unsigned char>(std::lround(
                        255.0 * (1.0 - mountainAmount)));
            } else {
                mesh.colors[index * 4] = shade;
                mesh.colors[index * 4 + 1] = shade;
                mesh.colors[index * 4 + 2] = shade;
            }
            mesh.colors[index * 4 + 3] = 255U;
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

Model TerrainRenderer::buildMountainBackdrop() const {
    if (terrain_ == nullptr) {
        return {};
    }
    const auto& config = terrain_->config();
    if (config.terrainBoundaryRiseHeight <= 0.0) {
        return {};
    }
    const double inner = config.terrainWorldSize * 0.5;
    const double width = std::max(
        config.terrainBoundaryRiseWidth * 2.25, 96.0);
    if (width <= 0.0) {
        return {};
    }
    const double outer = inner + width;
    // Large triangles are intentional: the previous 2.25 m grid contained
    // tens of thousands of smoothly shaded faces and read as one soft wall.
    const double step = std::max(
        terrain_->spacing() * 6.0, 6.0);
    const int longCells = std::max(
        2, static_cast<int>(std::ceil(outer * 2.0 / step)));
    const int sideCells = std::max(
        2, static_cast<int>(std::ceil(width / step)));
    const double normalStep = std::max(step * 0.55, 2.5);

    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned char> colors;
    const std::size_t cellEstimate = static_cast<std::size_t>(
        longCells * sideCells * 4);
    vertices.reserve(cellEstimate * 18U);
    normals.reserve(cellEstimate * 18U);
    texcoords.reserve(cellEstimate * 12U);
    colors.reserve(cellEstimate * 24U);

    const auto normalAt = [&](double x, double z) {
        const double left = terrain_->getBackdropHeight(
            x - normalStep, z);
        const double right = terrain_->getBackdropHeight(
            x + normalStep, z);
        const double north = terrain_->getBackdropHeight(
            x, z - normalStep);
        const double south = terrain_->getBackdropHeight(
            x, z + normalStep);
        Vec3 normal{
            left - right,
            normalStep * 2.0,
            north - south,
        };
        const double length = std::sqrt(
            normal.x * normal.x +
            normal.y * normal.y +
            normal.z * normal.z);
        if (length <= 1e-9) {
            return Vec3{0.0, 1.0, 0.0};
        }
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
        return normal;
    };
    const auto appendVertex = [&](const Vec3& point,
                                  const Vec3& normal) {
        const double x = point.x;
        const double y = point.y;
        const double z = point.z;
        const double edgeDistance = std::max(
            std::abs(x), std::abs(z)) - inner;
        const double mountainProgress = std::clamp(
            edgeDistance / width, 0.0, 1.0);
        const double smoothMountainProgress =
            mountainProgress * mountainProgress * mountainProgress *
            (mountainProgress *
                 (mountainProgress * 6.0 - 15.0) +
             10.0);
        const double mountainMaterialAmount =
            0.88 + smoothMountainProgress * 0.12;
        const auto mountainBlue = static_cast<unsigned char>(
            std::lround(255.0 *
                        (1.0 - mountainMaterialAmount)));
        const double edgeX = std::clamp(x, -inner, inner);
        const double edgeZ = std::clamp(z, -inner, inner);
        const double relativeMountainHeight =
            y - terrain_->getHeight(edgeX, edgeZ);
        // Snow uses a continuous world-space field and the smooth terrain
        // normal even though lighting keeps the faceted normal. Shared
        // vertices therefore agree across triangles and cannot form seams.
        const double snowLineVariation = mountainValueNoise(
            x * 0.018,
            z * 0.018,
            terrain_->seed() ^ 0xd3a2646cU);
        const double snowLine = 34.0 + snowLineVariation * 3.0;
        const double heightSnow = std::clamp(
            (relativeMountainHeight - snowLine) / 9.5,
            0.0, 1.0);
        const double smoothHeightSnow =
            heightSnow * heightSnow *
            (3.0 - 2.0 * heightSnow);
        const Vec3 snowNormal = normalAt(x, z);
        const double topSurfaceSnow =
            std::clamp(
                (snowNormal.y - 0.32) / 0.32,
                0.0, 1.0);
        const double snowCoverage = std::clamp(
            0.88 + mountainValueNoise(
                x * 0.012 + 14.0,
                z * 0.012 - 8.0,
                terrain_->seed() ^ 0x165667b1U) * 0.14,
            0.72, 1.0);
        const double snowAmount =
            smoothHeightSnow * topSurfaceSnow *
            snowCoverage * mountainMaterialAmount;
        const auto snowGreen = static_cast<unsigned char>(
            std::lround(255.0 * (1.0 - snowAmount)));
        vertices.insert(vertices.end(), {
            static_cast<float>(x),
            static_cast<float>(y),
            static_cast<float>(z),
        });
        normals.insert(normals.end(), {
            static_cast<float>(normal.x),
            static_cast<float>(normal.y),
            static_cast<float>(normal.z),
        });
        texcoords.insert(texcoords.end(), {
            static_cast<float>(x * 0.08),
            static_cast<float>(z * 0.08),
        });
        // In-map terrain keeps R=G=B. On the backdrop B fades with mountain
        // distance and G fades on snowy caps, giving the world shader two
        // channel-safe material masks without affecting wet-shore shading.
        colors.insert(colors.end(), {
            255U, snowGreen, mountainBlue, 255U});
    };
    const auto appendTriangle = [&](double x0, double z0,
                                    double x1, double z1,
                                    double x2, double z2) {
        const Vec3 point0{x0, terrain_->getBackdropHeight(x0, z0), z0};
        const Vec3 point1{x1, terrain_->getBackdropHeight(x1, z1), z1};
        const Vec3 point2{x2, terrain_->getBackdropHeight(x2, z2), z2};
        const Vec3 edge1{
            point1.x - point0.x,
            point1.y - point0.y,
            point1.z - point0.z,
        };
        const Vec3 edge2{
            point2.x - point0.x,
            point2.y - point0.y,
            point2.z - point0.z,
        };
        Vec3 faceNormal{
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x,
        };
        double faceLength = std::sqrt(
            faceNormal.x * faceNormal.x +
            faceNormal.y * faceNormal.y +
            faceNormal.z * faceNormal.z);
        if (faceLength <= 1e-9) {
            faceNormal = {0.0, 1.0, 0.0};
        } else {
            faceNormal.x /= faceLength;
            faceNormal.y /= faceLength;
            faceNormal.z /= faceLength;
        }
        if (faceNormal.y < 0.0) {
            faceNormal.x = -faceNormal.x;
            faceNormal.y = -faceNormal.y;
            faceNormal.z = -faceNormal.z;
        }
        const double centerX = (x0 + x1 + x2) / 3.0;
        const double centerZ = (z0 + z1 + z2) / 3.0;
        const Vec3 smoothNormal = normalAt(centerX, centerZ);
        constexpr double FaceNormalWeight = 0.78;
        Vec3 facetNormal{
            faceNormal.x * FaceNormalWeight +
                smoothNormal.x * (1.0 - FaceNormalWeight),
            faceNormal.y * FaceNormalWeight +
                smoothNormal.y * (1.0 - FaceNormalWeight),
            faceNormal.z * FaceNormalWeight +
                smoothNormal.z * (1.0 - FaceNormalWeight),
        };
        const double facetLength = std::sqrt(
            facetNormal.x * facetNormal.x +
            facetNormal.y * facetNormal.y +
            facetNormal.z * facetNormal.z);
        if (facetLength > 1e-9) {
            facetNormal.x /= facetLength;
            facetNormal.y /= facetLength;
            facetNormal.z /= facetLength;
        }
        appendVertex(point0, facetNormal);
        appendVertex(point1, facetNormal);
        appendVertex(point2, facetNormal);
    };
    const auto appendCell = [&](double x0, double z0,
                                double x1, double z1) {
        const std::int64_t diagonal =
            static_cast<std::int64_t>(std::llround(x0 / step)) +
            static_cast<std::int64_t>(std::llround(z0 / step));
        if ((diagonal & 1) == 0) {
            appendTriangle(x0, z0, x0, z1, x1, z0);
            appendTriangle(x1, z0, x0, z1, x1, z1);
        } else {
            appendTriangle(x0, z0, x0, z1, x1, z1);
            appendTriangle(x0, z0, x1, z1, x1, z0);
        }
    };
    const auto appendGrid = [&](double minimumX, double maximumX,
                                double minimumZ, double maximumZ,
                                int cellsX, int cellsZ) {
        for (int z = 0; z < cellsZ; ++z) {
            const double z0 = minimumZ +
                (maximumZ - minimumZ) *
                    static_cast<double>(z) /
                    static_cast<double>(cellsZ);
            const double z1 = minimumZ +
                (maximumZ - minimumZ) *
                    static_cast<double>(z + 1) /
                    static_cast<double>(cellsZ);
            for (int x = 0; x < cellsX; ++x) {
                const double x0 = minimumX +
                    (maximumX - minimumX) *
                        static_cast<double>(x) /
                        static_cast<double>(cellsX);
                const double x1 = minimumX +
                    (maximumX - minimumX) *
                        static_cast<double>(x + 1) /
                        static_cast<double>(cellsX);
                appendCell(x0, z0, x1, z1);
            }
        }
    };

    // Four non-overlapping strips form a square annulus. Keeping the inner
    // vertices exactly on the map edge avoids a visible crack or z-fighting
    // with the final in-map terrain row.
    appendGrid(-outer, outer, inner, outer,
               longCells, sideCells);
    appendGrid(-outer, outer, -outer, -inner,
               longCells, sideCells);
    appendGrid(inner, outer, -inner, inner,
               sideCells, longCells);
    appendGrid(-outer, -inner, -inner, inner,
               sideCells, longCells);
    return buildTriangleModel(
        vertices, normals, texcoords, colors);
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
            pond.signedDistance(x, z);
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
    const auto appendTriangle = [&](const PondDefinition& pond,
                                    double x0, double z0,
                                    double x1, double z1,
                                    double x2, double z2) {
        appendVertex(pond, x0, z0);
        appendVertex(pond, x1, z1);
        appendVertex(pond, x2, z2);
    };
    for (const PondDefinition& pond : terrain_->ponds()) {
        const double extent =
            std::max(pond.radiusX, pond.radiusZ) +
            pond.bayRadius + step * 2.0;
        const int cells = std::max(
            2, static_cast<int>(std::ceil(extent * 2.0 / step)));
        const double minimumX = pond.x - extent;
        const double minimumZ = pond.z - extent;
        constexpr int ShoreSubdivisions = 4;
        std::vector<unsigned char> activeCells(
            static_cast<std::size_t>(cells * cells));
        std::vector<unsigned char> fineCells(
            static_cast<std::size_t>(cells * cells));
        const auto cellIndex = [cells](int x, int z) {
            return static_cast<std::size_t>(z * cells + x);
        };
        const auto coordinate = [step](double minimum, int index) {
            return minimum + static_cast<double>(index) * step;
        };
        for (int zCell = 0; zCell < cells; ++zCell) {
            const double z0 = coordinate(minimumZ, zCell);
            const double z1 = coordinate(minimumZ, zCell + 1);
            for (int xCell = 0; xCell < cells; ++xCell) {
                const double x0 = coordinate(minimumX, xCell);
                const double x1 = coordinate(minimumX, xCell + 1);
                const std::array<double, 4> distances{{
                    pond.signedDistance(x0, z0),
                    pond.signedDistance(x1, z0),
                    pond.signedDistance(x0, z1),
                    pond.signedDistance(x1, z1),
                }};
                const auto [minimumDistance, maximumDistance] =
                    std::minmax_element(
                        distances.begin(), distances.end());
                const std::size_t index = cellIndex(xCell, zCell);
                activeCells[index] = static_cast<unsigned char>(
                    *minimumDistance <= step * 1.5);
                fineCells[index] = static_cast<unsigned char>(
                    activeCells[index] != 0U &&
                    *minimumDistance < shorelineWidth * 0.35 &&
                    *maximumDistance > -shorelineWidth * 1.15);
            }
        }
        const auto isFine = [&](int x, int z) {
            return x >= 0 && x < cells && z >= 0 && z < cells &&
                fineCells[cellIndex(x, z)] != 0U;
        };
        const auto subdivide = [](double from, double to, int index) {
            return from + (to - from) *
                static_cast<double>(index) /
                static_cast<double>(ShoreSubdivisions);
        };
        for (int zCell = 0; zCell < cells; ++zCell) {
            const double z0 = coordinate(minimumZ, zCell);
            const double z1 = coordinate(minimumZ, zCell + 1);
            for (int xCell = 0; xCell < cells; ++xCell) {
                const std::size_t index = cellIndex(xCell, zCell);
                if (activeCells[index] == 0U) {
                    continue;
                }
                const double x0 = coordinate(minimumX, xCell);
                const double x1 = coordinate(minimumX, xCell + 1);
                if (fineCells[index] == 0U) {
                    const bool westFine = isFine(xCell - 1, zCell);
                    const bool southFine = isFine(xCell, zCell + 1);
                    const bool eastFine = isFine(xCell + 1, zCell);
                    const bool northFine = isFine(xCell, zCell - 1);
                    if (!westFine && !southFine &&
                        !eastFine && !northFine) {
                        appendCell(pond, x0, z0, x1, z1);
                        continue;
                    }

                    // A fan adds the same intermediate edge vertices as an
                    // adjacent fine cell. This removes T-junctions whose
                    // independently displaced edges can expose dark cracks.
                    std::vector<std::array<double, 2>> boundary;
                    boundary.reserve(ShoreSubdivisions * 4U);
                    boundary.push_back({x0, z0});
                    if (westFine) {
                        for (int part = 1;
                             part < ShoreSubdivisions; ++part) {
                            boundary.push_back(
                                {x0, subdivide(z0, z1, part)});
                        }
                    }
                    boundary.push_back({x0, z1});
                    if (southFine) {
                        for (int part = 1;
                             part < ShoreSubdivisions; ++part) {
                            boundary.push_back(
                                {subdivide(x0, x1, part), z1});
                        }
                    }
                    boundary.push_back({x1, z1});
                    if (eastFine) {
                        for (int part = 1;
                            part < ShoreSubdivisions; ++part) {
                            boundary.push_back(
                                {x1, subdivide(
                                    z0, z1,
                                    ShoreSubdivisions - part)});
                        }
                    }
                    boundary.push_back({x1, z0});
                    if (northFine) {
                        for (int part = 1;
                            part < ShoreSubdivisions; ++part) {
                            boundary.push_back(
                                {subdivide(
                                    x0, x1,
                                    ShoreSubdivisions - part), z0});
                        }
                    }
                    const double centerX = (x0 + x1) * 0.5;
                    const double centerZ = (z0 + z1) * 0.5;
                    for (std::size_t part = 0;
                         part < boundary.size(); ++part) {
                        const auto& from = boundary[part];
                        const auto& to =
                            boundary[(part + 1U) % boundary.size()];
                        appendTriangle(
                            pond, centerX, centerZ,
                            from[0], from[1], to[0], to[1]);
                    }
                    continue;
                }
                for (int fineZ = 0; fineZ < ShoreSubdivisions; ++fineZ) {
                    const double fineZ0 = subdivide(z0, z1, fineZ);
                    const double fineZ1 = subdivide(z0, z1, fineZ + 1);
                    for (int fineX = 0;
                         fineX < ShoreSubdivisions; ++fineX) {
                        const double fineX0 = subdivide(x0, x1, fineX);
                        const double fineX1 = subdivide(x0, x1, fineX + 1);
                        appendCell(
                            pond, fineX0, fineZ0, fineX1, fineZ1);
                    }
                }
            }
        }
    }
    return buildTriangleModel(vertices, normals, texcoords);
}

void TerrainRenderer::buildPathMask() {
    if (terrain_ == nullptr) {
        return;
    }
    const int resolution = std::clamp(
        terrain_->resolution(), 257, 1025);
    Image image = GenImageColor(resolution, resolution, BLACK);
    if (!IsImageValid(image) || image.data == nullptr) {
        UnloadImage(image);
        return;
    }
    auto* pixels = static_cast<Color*>(image.data);
    const double worldSize = terrain_->config().terrainWorldSize;
    const double halfSize = worldSize * 0.5;
    for (int z = 0; z < resolution; ++z) {
        const double worldZ = -halfSize + worldSize *
            static_cast<double>(z) /
            static_cast<double>(resolution - 1);
        for (int x = 0; x < resolution; ++x) {
            const double worldX = -halfSize + worldSize *
                static_cast<double>(x) /
                static_cast<double>(resolution - 1);
            const auto mask = static_cast<unsigned char>(std::lround(
                terrain_->pathAmount(worldX, worldZ) * 255.0));
            pixels[static_cast<std::size_t>(z) *
                       static_cast<std::size_t>(resolution) +
                   static_cast<std::size_t>(x)] =
                {mask, mask, mask, 255U};
        }
    }
    pathMaskTexture_ = LoadTextureFromImage(image);
    UnloadImage(image);
    if (IsTextureValid(pathMaskTexture_)) {
        SetTextureFilter(pathMaskTexture_, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(pathMaskTexture_, TEXTURE_WRAP_CLAMP);
    }
}

void TerrainRenderer::buildChunks() {
    if (terrain_ == nullptr) {
        return;
    }
    const auto& config = terrain_->config();
    const double chunkSize = config.terrainChunkWorldSize;
    const int chunkCount = std::max(
        1, static_cast<int>(
               std::ceil(config.terrainWorldSize / chunkSize)));
    const std::size_t cellCount = static_cast<std::size_t>(chunkCount) *
        static_cast<std::size_t>(chunkCount);
    chunks_.reserve(cellCount);
    for (int z = 0; z < chunkCount; ++z) {
        for (int x = 0; x < chunkCount; ++x) {
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
    constexpr int PathMaskTextureSlot = 11;
    const float pathMaskEnabled =
        IsTextureValid(pathMaskTexture_) ? 1.0F : 0.0F;
    if (!pathMaskLocationsResolved_ ||
        pathMaskShaderId_ != shader.id) {
        pathMaskShaderId_ = shader.id;
        pathMaskLocation_ =
            GetShaderLocation(shader, "terrainPathMask");
        pathMaskEnabledLocation_ =
            GetShaderLocation(shader, "terrainPathMaskEnabled");
        pathMaskLocationsResolved_ = true;
    }
    if (pathMaskLocation_ >= 0 && pathMaskEnabledLocation_ >= 0) {
        SetShaderValue(shader, pathMaskLocation_,
                       &PathMaskTextureSlot, SHADER_UNIFORM_INT);
        SetShaderValue(shader, pathMaskEnabledLocation_,
                       &pathMaskEnabled, SHADER_UNIFORM_FLOAT);
    }
    if (pathMaskEnabled > 0.5F && pathMaskLocation_ >= 0) {
        rlActiveTextureSlot(PathMaskTextureSlot);
        rlEnableTexture(pathMaskTexture_.id);
        rlActiveTextureSlot(0);
    }
    const auto& config = terrain_->config();
    const float halfSize = static_cast<float>(
        config.terrainWorldSize * 0.5);
    const float chunkSize = static_cast<float>(
        config.terrainChunkWorldSize);
    const float chunkRadius =
        std::sqrt(2.0F) * chunkSize * 0.5F;
    const float renderDistance = std::max(
        static_cast<float>(config.terrainRenderDistance),
        chunkRadius);
    const float renderDistanceWithRadius =
        renderDistance + chunkRadius;
    const float renderDistanceSquared =
        renderDistanceWithRadius * renderDistanceWithRadius;
    // The outer terrain ring is the visual foundation for the boundary
    // forest and the mountain silhouette. Keep those chunks alive even when
    // the gameplay terrain render distance is reduced; otherwise the trees
    // remain visible while their ground is culled and appear to float.
    const float boundaryStart = static_cast<float>(
        halfSize - config.terrainBoundaryRiseWidth);
    const float boundaryKeepDistance = std::max(
        boundaryStart - chunkRadius, 0.0F);
    for (auto& chunk : chunks_) {
        const float centerX =
            -halfSize +
            (static_cast<float>(chunk.x) + 0.5F) * chunkSize;
        const float centerZ =
            -halfSize +
            (static_cast<float>(chunk.z) + 0.5F) * chunkSize;
        const float offsetX = centerX - focusPosition.x;
        const float offsetZ = centerZ - focusPosition.z;
        const float edgeDistance = std::max(
            std::abs(centerX), std::abs(centerZ));
        const bool boundaryChunk =
            config.terrainBoundaryRiseWidth > 0.0 &&
            config.terrainBoundaryRiseHeight > 0.0 &&
            edgeDistance >= boundaryKeepDistance;
        if (!boundaryChunk &&
            offsetX * offsetX + offsetZ * offsetZ >
                renderDistanceSquared) {
            continue;
        }
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
    if (IsModelValid(mountainBackdropModel_)) {
        for (int index = 0;
             index < mountainBackdropModel_.materialCount; ++index) {
            mountainBackdropModel_.materials[index].shader = shader;
        }
        DrawModel(
            mountainBackdropModel_, {0.0F, 0.0F, 0.0F},
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
    if (IsModelValid(mountainBackdropModel_)) {
        DrawModelWires(
            mountainBackdropModel_, {0.0F, 0.0F, 0.0F},
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
    if (IsModelValid(mountainBackdropModel_)) {
        UnloadModel(mountainBackdropModel_);
    }
    mountainBackdropModel_ = {};
    if (IsModelValid(waterModel_)) {
        UnloadModel(waterModel_);
    }
    waterModel_ = {};
    if (IsTextureValid(pathMaskTexture_)) {
        UnloadTexture(pathMaskTexture_);
    }
    pathMaskTexture_ = {};
    pathMaskShaderId_ = 0U;
    pathMaskLocation_ = -1;
    pathMaskEnabledLocation_ = -1;
    pathMaskLocationsResolved_ = false;
    terrain_ = nullptr;
    ready_ = false;
}

bool TerrainRenderer::ready() const {
    return ready_;
}

} // namespace ian
