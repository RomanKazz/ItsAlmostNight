#include "graphics/Renderer.hpp"
#include "graphics/WorldTransforms.hpp"

#include "buildings/BuildingSystem.hpp"
#include "ui/UiText.hpp"
#include "world/PondDecorationLayout.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ian {
namespace {

std::uint32_t pondDecorHash(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

float pondDecorUnit(std::uint32_t value) {
    return static_cast<float>(pondDecorHash(value) & 0x00ffffffU) /
        static_cast<float>(0x01000000U);
}

} // namespace

void Renderer::rebuildTerrain(
    const TerrainHeightfield& terrain,
    std::span<const DecorationExclusion> exclusions) {
    terrainHeightfield_ = &terrain;
    grassClearAreaCells_.clear();
    indexedGrassClearAreaData_ = nullptr;
    indexedGrassClearAreaCount_ = 0U;
    grassClearAreaContentHash_ = 0U;
    grassClearAreaDimension_ = 0;
    grassInstanceCacheValid_ = false;
    decorativeInstanceCacheValid_ = false;
    for (auto& candidates : grassInstanceCandidates_) {
        candidates.clear();
    }
    for (auto& transforms : grassInstanceTransforms_) {
        transforms.clear();
    }
    for (auto& candidates : decorativeRockCandidates_) {
        candidates.clear();
    }
    for (auto& candidates : decorativeBushCandidates_) {
        candidates.clear();
    }
    boundaryForestCached_ = false;
    for (auto& transforms : boundaryForestTransforms_) {
        transforms.clear();
    }
    for (auto& transforms :
         boundaryForestRevealTransforms_) {
        transforms.clear();
    }
    decorationExclusionMap_.rebuild(
        terrain.config().terrainWorldSize * 0.5,
        exclusions);
    terrainRenderer_.rebuild(terrain);
    rebuildPondDecorInstances();
}

void Renderer::rebuildDecorationExclusions(
    std::span<const DecorationExclusion> exclusions) {
    if (terrainHeightfield_ == nullptr) {
        return;
    }
    decorationExclusionMap_.rebuild(
        terrainHeightfield_->config().terrainWorldSize * 0.5,
        exclusions);
    // Procedural candidates cover the whole map and are filtered against
    // this map while drawing. Resource/chest relocation therefore no longer
    // forces thousands of terrain/path samples to be regenerated.
    // Shore decoration is cached, so refresh only those transforms. Terrain
    // meshes and textures remain untouched during a resource relocation.
    rebuildPondDecorInstances();
}

Vector3 Renderer::terrainSurfaceNormal(
    float worldX, float worldZ) const {
    if (terrainHeightfield_ == nullptr) {
        return {0.0F, 1.0F, 0.0F};
    }
    const Vec3 sampled = terrainHeightfield_->getNormal(
        static_cast<double>(worldX),
        static_cast<double>(worldZ));
    Vector3 normal{
        static_cast<float>(sampled.x),
        static_cast<float>(sampled.y),
        static_cast<float>(sampled.z),
    };
    const float length = Vector3Length(normal);
    if (!std::isfinite(length) || length <= 0.0001F) {
        return {0.0F, 1.0F, 0.0F};
    }
    return Vector3Scale(normal, 1.0F / length);
}

Matrix Renderer::terrainAlignedRotation(
    float worldX, float worldZ, float yawRadians) const {
    return world_transforms::surfaceRotation(
        terrainSurfaceNormal(worldX, worldZ), yawRadians);
}

void Renderer::rebuildPondDecorInstances() {
    for (auto& transforms : pondDecorTransforms_) {
        transforms.clear();
    }
    for (auto& transforms : pondShoreRockTransforms_) {
        transforms.clear();
    }
    if (terrainHeightfield_ == nullptr) {
        return;
    }

    const auto pondPoint = [](const PondDefinition& pond,
                              float angle, float radial,
                              float tangentOffset) {
        const float directionX = std::cos(angle);
        const float directionZ = std::sin(angle);
        const float localX =
            directionX * static_cast<float>(pond.radiusX) * radial -
            directionZ * tangentOffset;
        const float localZ =
            directionZ * static_cast<float>(pond.radiusZ) * radial +
            directionX * tangentOffset;
        const float cosine =
            std::cos(static_cast<float>(pond.rotation));
        const float sine =
            std::sin(static_cast<float>(pond.rotation));
        return Vector2{
            static_cast<float>(pond.x) +
                localX * cosine - localZ * sine,
            static_cast<float>(pond.z) +
                localX * sine + localZ * cosine,
        };
    };

    for (const PondLilyPlacement& lily :
         generatePondLilyPlacements(*terrainHeightfield_)) {
        ModelResource& resource =
            resources_.pondDecorModel(lily.variant);
        const Matrix modelTransform = resource.valid()
            ? resource.get().transform
            : MatrixIdentity();
        const Matrix rotation = lily.variant < 2U
            // Water lilies are decals on the water plane.  Aligning them to
            // terrain normals introduced visible pitch/roll on the shore.
            ? world_transforms::surfaceRotation(
                  {0.0F, 1.0F, 0.0F}, static_cast<float>(lily.yaw))
            : terrainAlignedRotation(
                  static_cast<float>(lily.position.x),
                  static_cast<float>(lily.position.z),
                  static_cast<float>(lily.yaw));
        pondDecorTransforms_[lily.variant].push_back(
            MatrixMultiply(
                modelTransform,
                MatrixMultiply(
                    MatrixScale(
                        static_cast<float>(lily.scale),
                        static_cast<float>(lily.scale),
                        static_cast<float>(lily.scale)),
                    MatrixMultiply(
                        rotation,
                        MatrixTranslate(
                            static_cast<float>(lily.position.x),
                            static_cast<float>(lily.position.y),
                            static_cast<float>(lily.position.z))))));
    }

    std::size_t pondIndex = 0U;
    for (const PondDefinition& pond : terrainHeightfield_->ponds()) {
        const std::uint32_t pondHash = pondDecorHash(
            terrainHeightfield_->seed() ^
            static_cast<std::uint32_t>(pondIndex + 1U) * 0x9e3779b9U);
        std::vector<Vector2> plantPoints;

        constexpr int PlantClusterCandidates = 14;
        for (int cluster = 0; cluster < PlantClusterCandidates; ++cluster) {
            const std::uint32_t clusterHash = pondDecorHash(
                pondHash ^ (static_cast<std::uint32_t>(cluster + 13) *
                            0xc2b2ae35U));
            // Leave broad quiet arcs between a few dense plant groups.
            if (pondDecorUnit(clusterHash ^ 0x94d049bbU) < 0.38F) {
                continue;
            }
            const float angle =
                static_cast<float>(cluster) * PI * 2.0F /
                    static_cast<float>(PlantClusterCandidates) +
                (pondDecorUnit(clusterHash) - 0.5F) * 0.48F;
            Vector2 previousPoint =
                pondPoint(pond, angle, 0.55F, 0.0F);
            double previousDistance =
                terrainHeightfield_->waterSignedDistance(
                    previousPoint.x, previousPoint.y);
            float shoreRadial = 1.0F;
            for (int sample = 1; sample <= 32; ++sample) {
                const float radial = 0.55F +
                    static_cast<float>(sample) / 32.0F * 0.80F;
                const Vector2 point =
                    pondPoint(pond, angle, radial, 0.0F);
                const double distance =
                    terrainHeightfield_->waterSignedDistance(
                        point.x, point.y);
                shoreRadial = radial;
                if (previousDistance <= 0.0 && distance > 0.0) {
                    break;
                }
                previousDistance = distance;
            }

            const int count = 3 +
                static_cast<int>((clusterHash >> 5U) % 3U);
            for (int item = 0; item < count; ++item) {
                const std::uint32_t hash = pondDecorHash(
                    clusterHash + static_cast<std::uint32_t>(item + 1) *
                        0x165667b1U);
                const float tangent =
                    (static_cast<float>(item) -
                     static_cast<float>(count - 1) * 0.5F) * 1.16F;
                const float radial = shoreRadial - 0.010F +
                    (pondDecorUnit(hash ^ 0xfd7046c5U) - 0.5F) * 0.038F;
                const Vector2 point =
                    pondPoint(pond, angle, radial, tangent);
                if (decorationExclusionMap_.blocked(
                        point.x, point.y)) {
                    continue;
                }
                const double shoreDistance =
                    terrainHeightfield_->waterSignedDistance(
                        point.x, point.y);
                if (shoreDistance < -1.4 || shoreDistance > 1.0) {
                    continue;
                }
                constexpr float PlantSpacing = 1.02F;
                const bool overlaps = std::any_of(
                    plantPoints.begin(), plantPoints.end(),
                    [point](Vector2 other) {
                        const float x = point.x - other.x;
                        const float z = point.y - other.y;
                        return x * x + z * z <
                            PlantSpacing * PlantSpacing;
                    });
                if (overlaps) {
                    continue;
                }
                plantPoints.push_back(point);
                const std::uint32_t variantRoll = hash % 5U;
                const std::size_t variant = variantRoll < 2U
                    ? 2U
                    : variantRoll == 2U ? 3U : 4U;
                const float randomScale =
                    pondDecorUnit(hash ^ 0x68e31da4U);
                const float scale = variant == 2U
                    ? 7.2F + randomScale * 2.4F
                    : variant == 3U
                        ? 4.7F + randomScale * 1.8F
                        : 4.8F + randomScale * 1.9F;
                constexpr std::array<float, 3> MinimumY{
                    -0.0195855F, -0.0190402F, -0.0195855F};
                const float terrainY = static_cast<float>(
                    terrainHeightfield_->getHeight(point.x, point.y));
                const float yaw =
                    pondDecorUnit(hash ^ 0x9e3779b9U) * PI * 2.0F;
                ModelResource& resource =
                    resources_.pondDecorModel(variant);
                const Matrix modelTransform = resource.valid()
                    ? resource.get().transform
                    : MatrixIdentity();
                pondDecorTransforms_[variant].push_back(
                    MatrixMultiply(
                        modelTransform,
                        MatrixMultiply(
                            MatrixScale(scale, scale, scale),
                            MatrixMultiply(
                                terrainAlignedRotation(
                                    point.x, point.y, yaw),
                                MatrixTranslate(
                                    point.x,
                                    terrainY -
                                        MinimumY[variant - 2U] * scale +
                                        0.01F,
                                    point.y)))));
            }
        }

        constexpr int ShoreRockClusters = 7;
        for (int cluster = 0; cluster < ShoreRockClusters; ++cluster) {
            const std::uint32_t clusterHash = pondDecorHash(
                pondHash ^ (static_cast<std::uint32_t>(cluster + 41) *
                            0x85ebca6bU));
            if (pondDecorUnit(clusterHash ^ 0x27d4eb2fU) < 0.30F) {
                continue;
            }
            const float angle =
                static_cast<float>(cluster) * PI * 2.0F /
                    static_cast<float>(ShoreRockClusters) +
                (pondDecorUnit(clusterHash) - 0.5F) * 0.62F;
            float shoreRadial = 1.0F;
            const Vector2 innerPoint =
                pondPoint(pond, angle, 0.55F, 0.0F);
            double previousDistance =
                terrainHeightfield_->waterSignedDistance(
                    innerPoint.x, innerPoint.y);
            for (int sample = 1; sample <= 32; ++sample) {
                const float radial = 0.55F +
                    static_cast<float>(sample) / 32.0F * 0.80F;
                const Vector2 point =
                    pondPoint(pond, angle, radial, 0.0F);
                const double distance =
                    terrainHeightfield_->waterSignedDistance(
                        point.x, point.y);
                shoreRadial = radial;
                if (previousDistance <= 0.0 && distance > 0.0) {
                    break;
                }
                previousDistance = distance;
            }
            const int count = 2 +
                static_cast<int>((clusterHash >> 9U) % 3U);
            for (int item = 0; item < count; ++item) {
                const std::uint32_t hash = pondDecorHash(
                    clusterHash + static_cast<std::uint32_t>(item + 1) *
                        0x9e3779b9U);
                const float tangent =
                    (static_cast<float>(item) -
                     static_cast<float>(count - 1) * 0.5F) * 0.78F;
                const float radial = shoreRadial + 0.018F +
                    pondDecorUnit(hash ^ 0x68e31da4U) * 0.035F;
                const Vector2 point =
                    pondPoint(pond, angle, radial, tangent);
                if (decorationExclusionMap_.blocked(
                        point.x, point.y)) {
                    continue;
                }
                const double shoreDistance =
                    terrainHeightfield_->waterSignedDistance(
                        point.x, point.y);
                if (shoreDistance < 0.12 || shoreDistance > 2.5) {
                    continue;
                }
                const std::size_t variant =
                    static_cast<std::size_t>(hash % 4U);
                const float scale =
                    (item == 0 ? 0.72F : 0.48F) +
                    pondDecorUnit(hash ^ 0xb5297a4dU) * 0.28F;
                const float terrainY = static_cast<float>(
                    terrainHeightfield_->getHeight(point.x, point.y));
                const float yaw =
                    pondDecorUnit(hash ^ 0x7f4a7c15U) * PI * 2.0F;
                ModelResource& resource =
                    resources_.decorativeRockModel(variant);
                const Matrix modelTransform = resource.valid()
                    ? resource.get().transform
                    : MatrixIdentity();
                pondShoreRockTransforms_[variant].push_back(
                    MatrixMultiply(
                        modelTransform,
                        MatrixMultiply(
                            MatrixScale(scale, scale, scale),
                            MatrixMultiply(
                                terrainAlignedRotation(
                                    point.x, point.y, yaw),
                                MatrixTranslate(
                                    point.x, terrainY + 0.01F,
                                    point.y)))));
            }
        }
        ++pondIndex;
    }
}

void Renderer::drawTerrain(
    Color tint, Vector3 focusPosition,
    bool wireframe) {
    Shader shader{};
    if (selectionMaskPassOpen_ &&
        resources_.selectionMaskShader().valid()) {
        shader = resources_.selectionMaskShader().get();
    } else if (shadowPassOpen_ &&
        resources_.shadowShader().valid()) {
        shader = resources_.shadowShader().get();
    } else if (
        worldShaderActive_ &&
        resources_.worldShader().valid()) {
        shader = resources_.worldShader().get();
    }
    terrainRenderer_.draw(shader, tint, focusPosition);
    if (wireframe) {
        terrainRenderer_.drawWireframe(
            {245, 224, 154, 150});
    }
}

void Renderer::drawPondDecor() {
    drawPondDecorInstances(2U, pondDecorTransforms_.size());
}

void Renderer::drawPondShoreRocks() {
    drawPondShoreRockInstances();
}

void Renderer::drawPondSurfaceDecor() {
    drawPondDecorInstances(0U, 2U);
}

void Renderer::drawPondDecorInstances(
    std::size_t beginVariant, std::size_t endVariant) {
    if (!worldShaderActive_ || !resources_.worldShader().valid()) {
        return;
    }
    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(shader, worldInstancingEnabledLocation_,
                   &enabled, SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    endVariant = std::min(endVariant, pondDecorTransforms_.size());
    for (std::size_t variant = beginVariant;
         variant < endVariant; ++variant) {
        const auto& transforms = pondDecorTransforms_[variant];
        ModelResource& resource = resources_.pondDecorModel(variant);
        if (transforms.empty() || !resource.valid()) {
            continue;
        }
        Model& model = resource.get();
        for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
            Material material =
                model.materials[model.meshMaterial[meshIndex]];
            material.shader = shader;
            material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                transforms.data(),
                static_cast<int>(transforms.size()));
        }
    }
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(shader, worldInstancingEnabledLocation_,
                   &disabled, SHADER_UNIFORM_INT);
}

void Renderer::drawPondShoreRockInstances() {
    if (!worldShaderActive_ || !resources_.worldShader().valid()) {
        return;
    }
    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(shader, worldInstancingEnabledLocation_,
                   &enabled, SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    for (std::size_t variant = 0;
         variant < pondShoreRockTransforms_.size(); ++variant) {
        const auto& transforms = pondShoreRockTransforms_[variant];
        ModelResource& resource =
            resources_.decorativeRockModel(variant);
        if (transforms.empty() || !resource.valid()) {
            continue;
        }
        Model& model = resource.get();
        for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
            Material material =
                model.materials[model.meshMaterial[meshIndex]];
            material.shader = shader;
            material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                transforms.data(),
                static_cast<int>(transforms.size()));
        }
    }
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(shader, worldInstancingEnabledLocation_,
                   &disabled, SHADER_UNIFORM_INT);
}

void Renderer::drawWater(
    Vector3 cameraPosition, const WorldLighting& lighting) {
    if (terrainHeightfield_ == nullptr ||
        !resources_.waterShader().valid()) {
        return;
    }
    Shader& shader = resources_.waterShader().get();
    const auto& config = terrainHeightfield_->config();
    const Vector3 shallowColor{
        static_cast<float>(config.pondShallowColor[0]),
        static_cast<float>(config.pondShallowColor[1]),
        static_cast<float>(config.pondShallowColor[2]),
    };
    const Vector3 deepColor{
        static_cast<float>(config.pondDeepColor[0]),
        static_cast<float>(config.pondDeepColor[1]),
        static_cast<float>(config.pondDeepColor[2]),
    };
    const float timeSeconds = static_cast<float>(GetTime());
    const float waveSpeed =
        static_cast<float>(config.pondWaveSpeed);
    const float fogStart =
        settings_.fog ? lighting.fogStart : 1000000.0F;
    const float fogEnd =
        settings_.fog ? lighting.fogEnd : 1000001.0F;
    SetShaderValue(shader, waterCameraPositionLocation_,
                   &cameraPosition, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, waterShallowColorLocation_,
                   &shallowColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, waterDeepColorLocation_,
                   &deepColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, waterSkyColorLocation_,
                   &lighting.skyAmbientColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, waterSunDirectionLocation_,
                   &lighting.sunDirection, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, waterSunColorLocation_,
                   &lighting.sunColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, waterFogColorLocation_,
                   &lighting.fogColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, waterDayNightTintLocation_,
                   &lighting.dayNightTint, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, waterFogStartLocation_,
                   &fogStart, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, waterFogEndLocation_,
                   &fogEnd, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, waterExposureLocation_,
                   &lighting.exposure, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, waterTimeLocation_,
                   &timeSeconds, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, waterWaveSpeedLocation_,
                   &waveSpeed, SHADER_UNIFORM_FLOAT);
    terrainRenderer_.drawWater(shader);
}

void Renderer::drawGrassInstances(Vector3 cameraPosition,
                                  float worldLimit,
                                  float nightAmount,
                                  const WorldLighting& lighting,
                                  std::span<const GrassClearArea>
                                      clearAreas) {
    if (!settings_.grass) {
        return;
    }
    ensureGrassClearAreaIndex(clearAreas);

    constexpr std::size_t VariantCount = 3;
    constexpr std::size_t MaximumInstancesPerVariant = 1024;
    const float Spacing =
        settings_.quality == GraphicsQuality::Low
            ? 2.6F
            : settings_.quality == GraphicsQuality::Medium
                ? 2.1F
                : 1.8F;
    const float DrawRadius =
        settings_.quality == GraphicsQuality::Low
            ? 42.0F
            : settings_.quality == GraphicsQuality::Medium
                ? 52.0F
                : 60.0F;
    constexpr float ClusterSize = 11.0F;
    constexpr float CacheCellSize = 8.0F;
    constexpr float CachePadding = 10.0F;
    const int cameraCellX = static_cast<int>(std::floor(
        cameraPosition.x / CacheCellSize));
    const int cameraCellZ = static_cast<int>(std::floor(
        cameraPosition.z / CacheCellSize));
    const bool revealAnimating = worldRevealElapsed_ < 1.7F;
    const bool cacheMatches =
        !revealAnimating && grassInstanceCacheValid_ &&
        grassCacheCameraCellX_ == cameraCellX &&
        grassCacheCameraCellZ_ == cameraCellZ &&
        grassCacheQuality_ == settings_.quality &&
        grassCacheWorldLimit_ == worldLimit &&
        grassCacheTerrain_ == terrainHeightfield_ &&
        grassCacheRevealOrigin_.x == worldRevealOrigin_.x &&
        grassCacheRevealOrigin_.y == worldRevealOrigin_.y;
    std::array<std::size_t, VariantCount> counts{};

    const auto hashCell = [](int x, int z) {
        std::uint32_t value =
            static_cast<std::uint32_t>(x) * 0x8da6b343U ^
            static_cast<std::uint32_t>(z) * 0xd8163841U;
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        return value ^ (value >> 16U);
    };
    const auto unitFloat = [](std::uint32_t value) {
        return static_cast<float>(value & 0xffffU) /
               65535.0F;
    };
    const auto clusterNoise = [&](float x, float z) {
        const float sampleX = x / ClusterSize;
        const float sampleZ = z / ClusterSize;
        const int cellX = static_cast<int>(std::floor(sampleX));
        const int cellZ = static_cast<int>(std::floor(sampleZ));
        float blendX = sampleX - static_cast<float>(cellX);
        float blendZ = sampleZ - static_cast<float>(cellZ);
        blendX = blendX * blendX * (3.0F - 2.0F * blendX);
        blendZ = blendZ * blendZ * (3.0F - 2.0F * blendZ);
        const auto sample = [&](int offsetX, int offsetZ) {
            return unitFloat(
                hashCell(cellX + offsetX, cellZ + offsetZ) ^
                0x27d4eb2fU);
        };
        const float lower = sample(0, 0) +
            (sample(1, 0) - sample(0, 0)) * blendX;
        const float upper = sample(0, 1) +
            (sample(1, 1) - sample(0, 1)) * blendX;
        return lower + (upper - lower) * blendZ;
    };

    if (!cacheMatches) {
        for (auto& candidates : grassInstanceCandidates_) {
            candidates.clear();
        }
        const float generationRadius = revealAnimating
            ? DrawRadius
            : DrawRadius + CachePadding;
        const int minimumX = static_cast<int>(std::floor(
            (cameraPosition.x - generationRadius) / Spacing));
        const int maximumX = static_cast<int>(std::ceil(
            (cameraPosition.x + generationRadius) / Spacing));
        const int minimumZ = static_cast<int>(std::floor(
            (cameraPosition.z - generationRadius) / Spacing));
        const int maximumZ = static_cast<int>(std::ceil(
            (cameraPosition.z + generationRadius) / Spacing));
        const float generationRadiusSquared =
            generationRadius * generationRadius;

        for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
            for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
                const std::uint32_t hash = hashCell(cellX, cellZ);
                const float jitterX =
                    (unitFloat(hash) - 0.5F) * Spacing * 0.72F;
                const float jitterZ =
                    (unitFloat(hash >> 8U) - 0.5F) *
                    Spacing * 0.72F;
                const float x =
                    (static_cast<float>(cellX) + 0.5F) * Spacing +
                    jitterX;
                const float z =
                    (static_cast<float>(cellZ) + 0.5F) * Spacing +
                    jitterZ;
                if (std::abs(x) > worldLimit - 0.5F ||
                    std::abs(z) > worldLimit - 0.5F) {
                    continue;
                }
                const float offsetX = x - cameraPosition.x;
                const float offsetZ = z - cameraPosition.z;
                const float distanceSquared =
                    offsetX * offsetX + offsetZ * offsetZ;
                if (distanceSquared > generationRadiusSquared) {
                    continue;
                }
                const float shoreDistance = terrainHeightfield_ != nullptr
                    ? static_cast<float>(
                          terrainHeightfield_->waterSignedDistance(x, z))
                    : 100.0F;
                if (shoreDistance < 0.15F) {
                    continue;
                }
                const float pathAmount = terrainHeightfield_ != nullptr
                    ? static_cast<float>(
                          terrainHeightfield_->pathAmount(x, z))
                    : 0.0F;

                const float cluster = clusterNoise(x, z);
                float shapedCluster = std::clamp(
                    (cluster - 0.32F) / 0.52F, 0.0F, 1.0F);
                shapedCluster = shapedCluster * shapedCluster *
                    (3.0F - 2.0F * shapedCluster);
                float shoreRecovery = std::clamp(
                    (shoreDistance - 0.22F) / 2.45F, 0.0F, 1.0F);
                shoreRecovery = shoreRecovery * shoreRecovery *
                    (3.0F - 2.0F * shoreRecovery);
                float clusterDensity = std::clamp(
                    (0.025F + shapedCluster * 0.10F +
                     shapedCluster * shapedCluster * 0.84F) *
                        (0.18F + shoreRecovery * 0.82F),
                    0.008F, 0.96F);
                const float pathGrass = std::clamp(
                    (pathAmount - 0.04F) / 0.78F, 0.0F, 1.0F);
                clusterDensity *= 1.0F - pathGrass * 0.94F;
                if (unitFloat(hash ^ 0xb5297a4dU) >
                    clusterDensity) {
                    continue;
                }

                const std::size_t variant =
                    static_cast<std::size_t>(hash % VariantCount);
                const float widthScale =
                    0.55F +
                    unitFloat(hash ^ 0x63d83595U) * 0.65F;
                const float heightScale =
                    0.45F +
                    unitFloat(hash ^ 0x9e3779b9U) * 1.10F;
                const float revealScale =
                    worldRevealScaleAt({x, z});
                if (revealScale <= 0.001F) {
                    continue;
                }
                const float horizontalScale =
                    widthScale * revealScale;
                const float verticalScale =
                    heightScale * revealScale *
                    (1.0F - pathGrass * 0.42F);
                const float terrainHeight =
                    terrainHeightfield_ != nullptr
                        ? static_cast<float>(
                              terrainHeightfield_->getHeight(
                                  x, z))
                        : 0.0F;
                const float rotation =
                    unitFloat(hash ^ 0xa511e9b3U) * PI * 2.0F;
                grassInstanceCandidates_[variant].push_back({
                    .transform = MatrixMultiply(
                        MatrixScale(
                            horizontalScale,
                            verticalScale,
                            horizontalScale),
                        MatrixMultiply(
                            MatrixRotateY(rotation),
                            MatrixTranslate(
                                x,
                                terrainHeight + 0.02F,
                                z))),
                    .position = {x, z},
                });
            }
        }
        if (!revealAnimating) {
            grassInstanceCacheValid_ = true;
            grassCacheCameraCellX_ = cameraCellX;
            grassCacheCameraCellZ_ = cameraCellZ;
            grassCacheQuality_ = settings_.quality;
            grassCacheWorldLimit_ = worldLimit;
            grassCacheTerrain_ = terrainHeightfield_;
            grassCacheRevealOrigin_ = worldRevealOrigin_;
        } else {
            grassInstanceCacheValid_ = false;
        }
    }

    for (std::size_t variant = 0; variant < VariantCount;
         ++variant) {
        grassInstanceTransforms_[variant].clear();
        grassInstanceTransforms_[variant].reserve(
            std::min(
                grassInstanceCandidates_[variant].size(),
                MaximumInstancesPerVariant));
        for (const CachedInstance& instance :
             grassInstanceCandidates_[variant]) {
            const float offsetX =
                instance.position.x - cameraPosition.x;
            const float offsetZ =
                instance.position.y - cameraPosition.z;
            const float distanceSquared =
                offsetX * offsetX + offsetZ * offsetZ;
            if (distanceSquared > DrawRadius * DrawRadius) {
                continue;
            }
            const float distance = std::sqrt(distanceSquared);
            const float detailFade = std::clamp(
                (distance - DrawRadius * 0.46F) /
                    (DrawRadius * 0.54F),
                0.0F, 1.0F);
            const float stableNoise = std::fmod(std::abs(std::sin(
                instance.position.x * 12.9898F +
                instance.position.y * 78.233F) * 43758.5453F), 1.0F);
            if (stableNoise > 1.0F - detailFade * 0.48F) {
                continue;
            }
            if (decorationExclusionMap_.blocked(
                    instance.position.x, instance.position.y)) {
                continue;
            }
            if (clearAreaVisibility(
                    instance.position, clearAreas,
                    0.001F, 0.25F) < 0.999F) {
                continue;
            }
            if (grassInstanceTransforms_[variant].size() >=
                MaximumInstancesPerVariant) {
                break;
            }
            grassInstanceTransforms_[variant].push_back(
                instance.transform);
        }
        counts[variant] = grassInstanceTransforms_[variant].size();
    }

    std::array<ModelResource*, VariantCount> variants{
        &resources_.grassModelB(),
        &resources_.grassModelC(),
        &resources_.grassModelD(),
    };
    const float darkness =
        1.0F - std::clamp(nightAmount, 0.0F, 1.0F) * 0.12F;
    const Color tint{
        static_cast<unsigned char>(255.0F * darkness),
        static_cast<unsigned char>(255.0F * darkness),
        static_cast<unsigned char>(
            255.0F * std::min(darkness * 1.08F, 1.0F)),
        255,
    };
    const Vector4 shaderTint = ColorNormalize(tint);
    if (resources_.grassShader().valid()) {
        Shader& shader = resources_.grassShader().get();
        const float timeSeconds = static_cast<float>(GetTime());
        const float fogStart =
            settings_.fog ? lighting.fogStart : 1000000.0F;
        const float fogEnd =
            settings_.fog ? lighting.fogEnd : 1000001.0F;
        SetShaderValue(shader, grassTintLocation_, &shaderTint,
                       SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, grassTimeLocation_, &timeSeconds,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassCameraPositionLocation_,
                       &cameraPosition, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassSunDirectionLocation_,
                       &lighting.sunDirection, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassSunColorLocation_,
                       &lighting.sunColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassSunIntensityLocation_,
                       &lighting.sunIntensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassSkyAmbientColorLocation_,
                       &lighting.skyAmbientColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassGroundAmbientColorLocation_,
                       &lighting.groundAmbientColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassAmbientIntensityLocation_,
                       &lighting.ambientIntensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassFogColorLocation_,
                       &lighting.fogColor, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassFogStartLocation_,
                       &fogStart, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassFogEndLocation_,
                       &fogEnd, SHADER_UNIFORM_FLOAT);
        const float fogBandsEnabled =
            settings_.fogBands ? 1.0F : 0.0F;
        SetShaderValue(shader, grassFogBandsEnabledLocation_,
                       &fogBandsEnabled, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassFogBandCountLocation_,
                       &settings_.fogBandCount,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassDayNightTintLocation_,
                       &lighting.dayNightTint, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, grassExposureLocation_,
                       &lighting.exposure, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, grassSaturationLocation_,
                       &lighting.saturation, SHADER_UNIFORM_FLOAT);
    }

    // Grass should remain a soft supporting layer rather than thousands of
    // tiny ink silhouettes. RGB remains fully opaque while shader alpha
    // writes a reserved grass tag for the post-process ink mask.
    rlDrawRenderBatchActive();
    rlSetBlendFactorsSeparate(
        RL_ONE, RL_ZERO,
        RL_ONE, RL_ZERO,
        RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    for (std::size_t variant = 0; variant < VariantCount;
         ++variant) {
        ModelResource& resource = *variants[variant];
        if (!resource.valid() || counts[variant] == 0U) {
            continue;
        }
        Model& model = resource.get();
        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            const int materialIndex =
                model.meshMaterial[meshIndex];
            Material material = model.materials[materialIndex];
            if (resources_.grassShader().valid()) {
                material.shader = resources_.grassShader().get();
                material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            } else {
                material.maps[MATERIAL_MAP_DIFFUSE].color = tint;
            }
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                grassInstanceTransforms_[variant].data(),
                static_cast<int>(counts[variant]));
        }
    }
    EndBlendMode();
}

void Renderer::setWorldReveal(
    Vector2 origin, float elapsedSeconds) {
    const float nextElapsed =
        std::max(elapsedSeconds, 0.0F);
    if (worldRevealOrigin_.x != origin.x ||
        worldRevealOrigin_.y != origin.y ||
        worldRevealElapsed_ < 1.7F ||
        nextElapsed < 1.7F) {
        grassInstanceCacheValid_ = false;
        decorativeInstanceCacheValid_ = false;
    }
    worldRevealOrigin_ = origin;
    worldRevealElapsed_ = nextElapsed;
}

float Renderer::worldRevealScaleAt(
    Vector2 position) const {
    constexpr float InitialRadius = 9.0F;
    constexpr float WaveSpeed = 150.0F;
    constexpr float AnimationDuration = 0.14F;
    constexpr float BackStrength = 1.15F;
    constexpr float BackCurve = BackStrength + 1.0F;
    if (worldRevealElapsed_ >= 1.7F) {
        return 1.0F;
    }
    const float offsetX =
        position.x - worldRevealOrigin_.x;
    const float offsetZ =
        position.y - worldRevealOrigin_.y;
    const float distance =
        std::hypot(offsetX, offsetZ);
    const float jitter =
        std::sin(position.x * 12.9898F +
                 position.y * 78.233F) *
        0.008F;
    const float arrivalTime = std::max(
        0.0F,
        (distance - InitialRadius) / WaveSpeed + jitter);
    const float progress = std::clamp(
        (worldRevealElapsed_ - arrivalTime) /
            AnimationDuration,
        0.0F, 1.0F);
    const float shifted = progress - 1.0F;
    const float scale =
        1.0F +
        BackCurve * shifted * shifted * shifted +
        BackStrength * shifted * shifted;
    return std::clamp(scale, 0.0F, 1.06F);
}

void Renderer::ensureGrassClearAreaIndex(
    std::span<const GrassClearArea> clearAreas) {
    std::size_t contentHash =
        static_cast<std::size_t>(1469598103934665603ULL);
    const auto mixHash = [&contentHash](std::uint32_t value) {
        contentHash ^= static_cast<std::size_t>(value);
        contentHash *= static_cast<std::size_t>(1099511628211ULL);
    };
    for (const GrassClearArea& area : clearAreas) {
        mixHash(std::bit_cast<std::uint32_t>(area.center.x));
        mixHash(std::bit_cast<std::uint32_t>(area.center.y));
        mixHash(std::bit_cast<std::uint32_t>(area.innerRadius));
        mixHash(std::bit_cast<std::uint32_t>(area.amount));
    }
    if (indexedGrassClearAreaCount_ == clearAreas.size() &&
        grassClearAreaContentHash_ == contentHash) {
        indexedGrassClearAreaData_ = clearAreas.data();
        return;
    }
    indexedGrassClearAreaData_ = clearAreas.data();
    indexedGrassClearAreaCount_ = clearAreas.size();
    grassClearAreaContentHash_ = contentHash;
    grassClearAreaCells_.clear();
    grassClearAreaDimension_ = 0;
    if (clearAreas.empty() || terrainHeightfield_ == nullptr) {
        return;
    }

    const float halfExtent = static_cast<float>(
        terrainHeightfield_->config().terrainWorldSize * 0.5);
    grassClearAreaMinimum_ = -halfExtent;
    grassClearAreaDimension_ = std::max(
        1, static_cast<int>(std::ceil(
               halfExtent * 2.0F / grassClearAreaCellSize_)));
    grassClearAreaCells_.resize(
        static_cast<std::size_t>(grassClearAreaDimension_) *
        static_cast<std::size_t>(grassClearAreaDimension_));

    const auto coordinate = [this](float value) {
        return std::clamp(
            static_cast<int>(std::floor(
                (value - grassClearAreaMinimum_) /
                grassClearAreaCellSize_)),
            0, grassClearAreaDimension_ - 1);
    };
    constexpr float MaximumFeatherAndPadding = 3.0F;
    for (std::size_t index = 0; index < clearAreas.size(); ++index) {
        const GrassClearArea& area = clearAreas[index];
        const float extent =
            std::max(area.innerRadius, 0.0F) +
            MaximumFeatherAndPadding;
        const int minimumX = coordinate(area.center.x - extent);
        const int maximumX = coordinate(area.center.x + extent);
        const int minimumZ = coordinate(area.center.y - extent);
        const int maximumZ = coordinate(area.center.y + extent);
        for (int z = minimumZ; z <= maximumZ; ++z) {
            for (int x = minimumX; x <= maximumX; ++x) {
                grassClearAreaCells_[
                    static_cast<std::size_t>(z) *
                        static_cast<std::size_t>(grassClearAreaDimension_) +
                    static_cast<std::size_t>(x)]
                    .push_back(static_cast<std::uint32_t>(index));
            }
        }
    }
}

float Renderer::clearAreaVisibility(
    Vector2 position,
    std::span<const GrassClearArea> clearAreas,
    float feather, float innerPadding) const {
    float visibility = 1.0F;
    feather = std::max(feather, 0.001F);
    const auto applyArea = [&](const GrassClearArea& area) {
        const float deltaX = position.x - area.center.x;
        const float deltaZ = position.y - area.center.y;
        const float maximumDistance =
            area.innerRadius + std::max(innerPadding, 0.0F) +
            feather;
        if (std::abs(deltaX) > maximumDistance ||
            std::abs(deltaZ) > maximumDistance) {
            return;
        }
        const float distanceSquared =
            deltaX * deltaX + deltaZ * deltaZ;
        if (distanceSquared >= maximumDistance * maximumDistance) {
            return;
        }
        const float distance = std::sqrt(distanceSquared);
        const float proximity =
            1.0F - std::clamp(
                       (distance - area.innerRadius -
                        std::max(innerPadding, 0.0F)) /
                           feather,
                       0.0F, 1.0F);
        const float clearing =
            std::clamp(area.amount, 0.0F, 1.0F) *
            proximity * proximity *
            (3.0F - 2.0F * proximity);
        visibility *= 1.0F - clearing;
    };

    if (!grassClearAreaCells_.empty() &&
        clearAreas.data() == indexedGrassClearAreaData_ &&
        clearAreas.size() == indexedGrassClearAreaCount_) {
        const int x = static_cast<int>(std::floor(
            (position.x - grassClearAreaMinimum_) /
            grassClearAreaCellSize_));
        const int z = static_cast<int>(std::floor(
            (position.y - grassClearAreaMinimum_) /
            grassClearAreaCellSize_));
        if (x < 0 || z < 0 || x >= grassClearAreaDimension_ ||
            z >= grassClearAreaDimension_) {
            return visibility;
        }
        const auto& candidates = grassClearAreaCells_[
            static_cast<std::size_t>(z) *
                static_cast<std::size_t>(grassClearAreaDimension_) +
            static_cast<std::size_t>(x)];
        for (const std::uint32_t index : candidates) {
            applyArea(clearAreas[index]);
        }
        return visibility;
    }
    for (const GrassClearArea& area : clearAreas) {
        applyArea(area);
    }
    return visibility;
}

} // namespace ian
