#include "graphics/Renderer.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace ian {
namespace {

constexpr float DecorativeRockClusterSize = 18.0F;

std::uint32_t decorativeRockClusterHash(int x, int z) {
    std::uint32_t value =
        static_cast<std::uint32_t>(x) * 0x8da6b343U ^
        static_cast<std::uint32_t>(z) * 0xd8163841U ^
        0x27d4eb2fU;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    return value ^ (value >> 16U);
}

float decorativeRockClusterDensity(float x, float z) {
    const float sampleX = x / DecorativeRockClusterSize;
    const float sampleZ = z / DecorativeRockClusterSize;
    const int cellX = static_cast<int>(std::floor(sampleX));
    const int cellZ = static_cast<int>(std::floor(sampleZ));
    float blendX = sampleX - static_cast<float>(cellX);
    float blendZ = sampleZ - static_cast<float>(cellZ);
    blendX = blendX * blendX * (3.0F - 2.0F * blendX);
    blendZ = blendZ * blendZ * (3.0F - 2.0F * blendZ);
    const auto sample = [](int sampleCellX, int sampleCellZ) {
        return static_cast<float>(
                   decorativeRockClusterHash(
                       sampleCellX, sampleCellZ) &
                   0xffffU) /
               65535.0F;
    };
    const float lower = sample(cellX, cellZ) +
        (sample(cellX + 1, cellZ) - sample(cellX, cellZ)) *
            blendX;
    const float upper = sample(cellX, cellZ + 1) +
        (sample(cellX + 1, cellZ + 1) -
         sample(cellX, cellZ + 1)) *
            blendX;
    const float cluster = lower + (upper - lower) * blendZ;
    float shaped = std::clamp(
        (cluster - 0.34F) / 0.46F, 0.0F, 1.0F);
    shaped = shaped * shaped * (3.0F - 2.0F * shaped);
    // Large quiet gaps, soft outskirts, then a genuinely dense core.
    return std::clamp(
        0.012F + shaped * 0.10F + shaped * shaped * 0.78F,
        0.012F, 0.89F);
}

float pathEdgeRockAmount(
    const TerrainHeightfield* terrain, float x, float z) {
    if (terrain == nullptr) {
        return 0.0F;
    }
    const float center = static_cast<float>(
        terrain->pathAmount(x, z));
    constexpr float Probe = 1.55F;
    float nearby = center;
    nearby = std::max(nearby, static_cast<float>(
        terrain->pathAmount(x + Probe, z)));
    nearby = std::max(nearby, static_cast<float>(
        terrain->pathAmount(x - Probe, z)));
    nearby = std::max(nearby, static_cast<float>(
        terrain->pathAmount(x, z + Probe)));
    nearby = std::max(nearby, static_cast<float>(
        terrain->pathAmount(x, z - Probe)));
    const auto smoothstep = [](float from, float to, float value) {
        float amount = std::clamp(
            (value - from) / (to - from), 0.0F, 1.0F);
        return amount * amount * (3.0F - 2.0F * amount);
    };
    const float closeToPath = smoothstep(0.10F, 0.72F, nearby);
    const float outsideCore =
        1.0F - smoothstep(0.28F, 0.82F, center);
    return closeToPath * outsideCore;
}


} // namespace

void Renderer::drawDecorativeRocks(
    Vector3 cameraPosition, float worldLimit,
    std::span<const GrassClearArea> clearAreas) {
    ensureGrassClearAreaIndex(clearAreas);
    constexpr std::size_t VariantCount = 4U;
    constexpr float Spacing = 4.45F;
    const float DrawRadius =
        settings_.quality == GraphicsQuality::Low
            ? 25.0F
            : settings_.quality == GraphicsQuality::Medium
                ? 30.0F
                : 34.0F;
    constexpr float CoreClearRadius = 10.5F;
    constexpr float CacheCellSize = 8.0F;
    constexpr float CacheCellOffsetX = CacheCellSize * 0.5F;
    constexpr float CacheCellOffsetZ = CacheCellSize * 0.25F;
    constexpr float CachePadding = 10.0F;
    const int cameraCellX = static_cast<int>(std::floor(
        (cameraPosition.x + CacheCellOffsetX) / CacheCellSize));
    const int cameraCellZ = static_cast<int>(std::floor(
        (cameraPosition.z + CacheCellOffsetZ) / CacheCellSize));
    const bool revealAnimating = worldRevealElapsed_ < 1.7F;
    const auto hashCell = [](int x, int z) {
        std::uint32_t value =
            static_cast<std::uint32_t>(x) * 0x8da6b343U ^
            static_cast<std::uint32_t>(z) * 0xd8163841U ^
            0x6c8e9cf5U;
        value ^= value >> 16U;
        value *= 0x7feb352dU;
        value ^= value >> 15U;
        value *= 0x846ca68bU;
        return value ^ (value >> 16U);
    };
    const auto unitFloat = [](std::uint32_t value) {
        return static_cast<float>(value & 0xffffU) / 65535.0F;
    };

    Shader* decorativeShader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        decorativeShader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ &&
               resources_.worldShader().valid()) {
        decorativeShader = &resources_.worldShader().get();
    }
    const bool useInstancing =
        worldShaderActive_ && !shadowPassOpen_ &&
        !selectionMaskPassOpen_ && resources_.worldShader().valid();
    const bool cacheMatches =
        useInstancing && !revealAnimating &&
        decorativeInstanceCacheValid_ &&
        decorativeCacheCameraCellX_ == cameraCellX &&
        decorativeCacheCameraCellZ_ == cameraCellZ &&
        decorativeCacheQuality_ == settings_.quality &&
        decorativeCacheWorldLimit_ == worldLimit &&
        decorativeCacheTerrain_ == terrainHeightfield_ &&
        decorativeCacheRevealOrigin_.x == worldRevealOrigin_.x &&
        decorativeCacheRevealOrigin_.y == worldRevealOrigin_.y;
    if (useInstancing) {
        for (auto& transforms : decorativeRockTransforms_) {
            transforms.clear();
            transforms.reserve(96U);
        }
        for (auto& transforms : decorativeBushTransforms_) {
            transforms.clear();
            transforms.reserve(96U);
        }
        if (!cacheMatches) {
            for (auto& candidates : decorativeRockCandidates_) {
                candidates.clear();
            }
            for (auto& candidates : decorativeBushCandidates_) {
                candidates.clear();
            }
        }
    }
    for (std::size_t variant = 0; variant < VariantCount; ++variant) {
        ModelResource& resource =
            resources_.decorativeRockModel(variant);
        if (!resource.valid()) {
            continue;
        }
        if (decorativeShader != nullptr) {
            Model& model = resource.get();
            for (int material = 0; material < model.materialCount;
                 ++material) {
                model.materials[material].shader = *decorativeShader;
            }
        }
    }
    constexpr std::size_t BushVariantCount = 9U;
    for (std::size_t variant = 0;
         variant < BushVariantCount; ++variant) {
        ModelResource& resource =
            resources_.decorativeBushModel(variant);
        if (!resource.valid()) {
            continue;
        }
        if (decorativeShader != nullptr) {
            Model& model = resource.get();
            for (int material = 0; material < model.materialCount;
                 ++material) {
                model.materials[material].shader = *decorativeShader;
            }
        }
    }

    const float traversalRadius =
        useInstancing && !revealAnimating
            ? DrawRadius + CachePadding
            : DrawRadius;
    const int minimumX = static_cast<int>(std::floor(
        (cameraPosition.x - traversalRadius) / Spacing));
    const int maximumX = static_cast<int>(std::ceil(
        (cameraPosition.x + traversalRadius) / Spacing));
    const int minimumZ = static_cast<int>(std::floor(
        (cameraPosition.z - traversalRadius) / Spacing));
    const int maximumZ = static_cast<int>(std::ceil(
        (cameraPosition.z + traversalRadius) / Spacing));
    const float BushDrawRadius =
        settings_.quality == GraphicsQuality::Low
            ? 27.0F
            : settings_.quality == GraphicsQuality::Medium
                ? 32.0F
                : 36.0F;
    if (!cacheMatches) {
    for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
        for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
            const std::uint32_t hash = hashCell(cellX, cellZ);
            const float jitterX =
                (unitFloat(hash >> 7U) - 0.5F) * Spacing * 0.78F;
            const float jitterZ =
                (unitFloat(hash >> 15U) - 0.5F) * Spacing * 0.78F;
            const float x =
                (static_cast<float>(cellX) + 0.5F) * Spacing +
                jitterX;
            const float z =
                (static_cast<float>(cellZ) + 0.5F) * Spacing +
                jitterZ;
            const float pathEdge = pathEdgeRockAmount(
                terrainHeightfield_, x, z);
            const float rockDensity = std::max(
                decorativeRockClusterDensity(x, z),
                pathEdge * 0.46F);
            if (unitFloat(hash) > rockDensity) {
                continue;
            }
            if (std::abs(x) > worldLimit - 0.7F ||
                std::abs(z) > worldLimit - 0.7F ||
                x * x + z * z < CoreClearRadius * CoreClearRadius) {
                continue;
            }
            if (!useInstancing &&
                decorationExclusionMap_.blocked(x, z)) {
                continue;
            }
            if (terrainHeightfield_ != nullptr &&
                terrainHeightfield_->waterSignedDistance(x, z) < 0.7) {
                continue;
            }
            const float pathAmount = terrainHeightfield_ != nullptr
                ? static_cast<float>(
                      terrainHeightfield_->pathAmount(x, z))
                : 0.0F;
            if (pathAmount > 0.70F && pathEdge < 0.08F) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                traversalRadius * traversalRadius) {
                continue;
            }
            if (!useInstancing && clearAreaVisibility(
                    {x, z}, clearAreas,
                    0.001F, 0.65F) < 0.999F) {
                continue;
            }
            const std::size_t variant =
                static_cast<std::size_t>((hash >> 4U) % VariantCount);
            ModelResource& resource =
                resources_.decorativeRockModel(variant);
            if (!resource.valid()) {
                continue;
            }
            const float baseScale =
                0.55F + unitFloat(hash ^ 0xa511e9b3U) * 0.45F;
            const float clusterStrength =
                decorativeRockClusterDensity(x, z);
            const float revealScale =
                worldRevealScaleAt({x, z});
            if (revealScale <= 0.001F) {
                continue;
            }
            const float scale =
                baseScale * (0.88F + clusterStrength * 0.28F) *
                revealScale * (1.0F - pathEdge * 0.62F);
            const float terrainHeight =
                terrainHeightfield_ != nullptr
                    ? static_cast<float>(
                          terrainHeightfield_->getHeight(x, z))
                    : 0.0F;
            const float rotation =
                unitFloat(hash ^ 0x63d83595U) * PI * 2.0F;
            const Vector3 position{
                x, terrainHeight, z,
            };
            if (useInstancing) {
                Model& model = resource.get();
                const Matrix transform = MatrixMultiply(
                    model.transform,
                    MatrixMultiply(
                        MatrixScale(scale, scale, scale),
                        MatrixMultiply(
                            terrainAlignedRotation(
                                position.x, position.z, rotation),
                            MatrixTranslate(
                                position.x, position.y,
                                position.z))));
                decorativeRockCandidates_[variant].push_back({
                    .transform = transform,
                    .position = {position.x, position.z},
                    .scale = scale,
                    .groundHeight = terrainHeight,
                });
            } else {
                drawTerrainAlignedModel(
                    resource.get(), position, rotation,
                    {scale, scale, scale}, WHITE);
            }
        }
    }

    constexpr float BushSpacing = 4.35F;
    constexpr std::array<float, BushVariantCount> BushVariantScales{
        1.35F, 0.62F, 0.82F, 0.70F, 0.90F, 1.00F,
        1.596F, 1.512F, 1.722F,
    };
    const float bushTraversalRadius =
        useInstancing && !revealAnimating
            ? BushDrawRadius + CachePadding
            : BushDrawRadius;
    const int minimumBushX = static_cast<int>(std::floor(
        (cameraPosition.x - bushTraversalRadius) / BushSpacing));
    const int maximumBushX = static_cast<int>(std::ceil(
        (cameraPosition.x + bushTraversalRadius) / BushSpacing));
    const int minimumBushZ = static_cast<int>(std::floor(
        (cameraPosition.z - bushTraversalRadius) / BushSpacing));
    const int maximumBushZ = static_cast<int>(std::ceil(
        (cameraPosition.z + bushTraversalRadius) / BushSpacing));
    for (int cellZ = minimumBushZ;
         cellZ <= maximumBushZ; ++cellZ) {
        for (int cellX = minimumBushX;
             cellX <= maximumBushX; ++cellX) {
            const std::uint32_t hash =
                std::rotl(hashCell(cellX, cellZ), 13) ^ 0xb5297a4dU;
            const float x =
                (static_cast<float>(cellX) + 0.5F) * BushSpacing +
                (unitFloat(hash >> 6U) - 0.5F) * BushSpacing * 0.76F;
            const float z =
                (static_cast<float>(cellZ) + 0.5F) * BushSpacing +
                (unitFloat(hash >> 14U) - 0.5F) * BushSpacing * 0.76F;
            const float sharedCluster =
                decorativeRockClusterDensity(x, z);
            const float secondaryCluster =
                decorativeRockClusterDensity(x + 31.0F, z - 23.0F);
            const float clusterDensity = std::clamp(
                sharedCluster * 0.74F + secondaryCluster * 0.34F,
                0.0F, 0.88F);
            const std::size_t selector = static_cast<std::size_t>(
                (hash ^ 0x7f4a7c15U) % 14U);
            const std::size_t variant = selector < 6U
                ? selector
                : selector < 10U ? 6U : selector < 13U ? 7U : 8U;
            const bool flora = variant >= 6U;
            const float placementDensity = flora
                ? std::clamp(0.28F + clusterDensity * 0.62F,
                             0.28F, 0.82F)
                : clusterDensity;
            if (unitFloat(hash ^ 0x68e31da4U) > placementDensity ||
                std::abs(x) > worldLimit - 0.9F ||
                std::abs(z) > worldLimit - 0.9F ||
                x * x + z * z < CoreClearRadius * CoreClearRadius) {
                continue;
            }
            if (!useInstancing &&
                decorationExclusionMap_.blocked(x, z)) {
                continue;
            }
            if (terrainHeightfield_ != nullptr &&
                terrainHeightfield_->waterSignedDistance(x, z) < 1.0) {
                continue;
            }
            if (terrainHeightfield_ != nullptr &&
                terrainHeightfield_->pathAmount(x, z) > 0.06) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                bushTraversalRadius * bushTraversalRadius) {
                continue;
            }
            if (!useInstancing && clearAreaVisibility(
                    {x, z}, clearAreas,
                    0.001F, 0.72F) < 0.999F) {
                continue;
            }
            ModelResource& resource =
                resources_.decorativeBushModel(variant);
            if (!resource.valid()) {
                continue;
            }
            const float revealScale = worldRevealScaleAt({x, z});
            const float scale =
                BushVariantScales[variant] *
                (0.82F + unitFloat(hash ^ 0x68e31da4U) * 0.43F) *
                (0.90F + sharedCluster * 0.22F) *
                revealScale;
            if (scale <= 0.001F) {
                continue;
            }
            const float terrainHeight =
                terrainHeightfield_ != nullptr
                    ? static_cast<float>(
                          terrainHeightfield_->getHeight(x, z))
                    : 0.0F;
            const float rotation =
                unitFloat(hash ^ 0x1b56c4e9U) * PI * 2.0F;
            const Vector3 position{
                x, terrainHeight, z,
            };
            if (useInstancing) {
                Model& model = resource.get();
                const Matrix transform = MatrixMultiply(
                    model.transform,
                    MatrixMultiply(
                        MatrixScale(scale, scale, scale),
                        MatrixMultiply(
                            terrainAlignedRotation(
                                position.x, position.z, rotation),
                            MatrixTranslate(
                                position.x, position.y,
                                position.z))));
                decorativeBushCandidates_[variant].push_back({
                    .transform = transform,
                    .position = {position.x, position.z},
                    .scale = scale,
                    .groundHeight = terrainHeight,
                });
            } else {
                drawTerrainAlignedModel(
                    resource.get(), position, rotation,
                    {scale, scale, scale}, WHITE);
            }
        }
    }
    }

    if (!useInstancing) {
        return;
    }
    for (std::size_t variant = 0; variant < VariantCount;
         ++variant) {
        for (const CachedInstance& instance :
             decorativeRockCandidates_[variant]) {
            const float offsetX =
                instance.position.x - cameraPosition.x;
            const float offsetZ =
                instance.position.y - cameraPosition.z;
            if (offsetX * offsetX + offsetZ * offsetZ <=
                    DrawRadius * DrawRadius &&
                !decorationExclusionMap_.blocked(
                    instance.position.x, instance.position.y) &&
                clearAreaVisibility(
                    instance.position, clearAreas,
                    0.001F, 0.65F) >= 0.999F) {
                decorativeRockTransforms_[variant].push_back(
                    instance.transform);
            }
        }
    }
    for (std::size_t variant = 0; variant < BushVariantCount;
         ++variant) {
        for (const CachedInstance& instance :
             decorativeBushCandidates_[variant]) {
            const float offsetX =
                instance.position.x - cameraPosition.x;
            const float offsetZ =
                instance.position.y - cameraPosition.z;
            const float distanceSquared =
                offsetX * offsetX + offsetZ * offsetZ;
            const float distance = std::sqrt(distanceSquared);
            const float detailFade = std::clamp(
                (distance - BushDrawRadius * 0.58F) /
                    (BushDrawRadius * 0.42F),
                0.0F, 1.0F);
            const float stableNoise = std::fmod(std::abs(std::sin(
                instance.position.x * 12.9898F +
                instance.position.y * 78.233F) * 43758.5453F), 1.0F);
            const bool keepDistantDetail =
                stableNoise <= 1.0F - detailFade * 0.42F;
            if (distanceSquared <= BushDrawRadius * BushDrawRadius &&
                keepDistantDetail &&
                !decorationExclusionMap_.blocked(
                    instance.position.x, instance.position.y) &&
                clearAreaVisibility(
                    instance.position, clearAreas,
                    0.001F, 0.72F) >= 0.999F) {
                decorativeBushTransforms_[variant].push_back(
                    instance.transform);
            }
        }
    }
    if (!revealAnimating) {
        decorativeInstanceCacheValid_ = true;
        decorativeCacheCameraCellX_ = cameraCellX;
        decorativeCacheCameraCellZ_ = cameraCellZ;
        decorativeCacheQuality_ = settings_.quality;
        decorativeCacheWorldLimit_ = worldLimit;
        decorativeCacheTerrain_ = terrainHeightfield_;
        decorativeCacheRevealOrigin_ = worldRevealOrigin_;
    } else {
        decorativeInstanceCacheValid_ = false;
    }
    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &enabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    const auto drawInstanced = [&shader](
                                   Model& model,
                                   const std::vector<Matrix>& transforms) {
        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            const int materialIndex = model.meshMaterial[meshIndex];
            Material material = model.materials[materialIndex];
            material.shader = shader;
            material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                transforms.data(),
                static_cast<int>(transforms.size()));
        }
    };
    for (std::size_t variant = 0; variant < VariantCount; ++variant) {
        if (decorativeRockTransforms_[variant].empty() ||
            !resources_.decorativeRockModel(variant).valid()) {
            continue;
        }
        drawInstanced(
            resources_.decorativeRockModel(variant).get(),
            decorativeRockTransforms_[variant]);
    }
    constexpr std::size_t FirstFlowerVariant = 6U;
    constexpr std::size_t LastFlowerVariant = 7U;
    rlDrawRenderBatchActive();
    WorldMaterialState bushMaterial = worldMaterial_;
    bushMaterial.distantFadeAmount = 1.0F;
    bushMaterial.vegetationAmount = 1.0F;
    uploadWorldMaterial(bushMaterial);
    for (std::size_t variant = 0; variant < BushVariantCount; ++variant) {
        if (variant >= FirstFlowerVariant &&
            variant <= LastFlowerVariant) {
            continue;
        }
        if (decorativeBushTransforms_[variant].empty() ||
            !resources_.decorativeBushModel(variant).valid()) {
            continue;
        }
        drawInstanced(
            resources_.decorativeBushModel(variant).get(),
            decorativeBushTransforms_[variant]);
    }
    rlDrawRenderBatchActive();
    // Small flower clusters share the same reserved material tag as grass.
    // Their RGB stays opaque, while post-process ink ignores both sides of
    // the flower/terrain boundary. Bushes and the larger plant keep ink.
    WorldMaterialState flowerMaterial = worldMaterial_;
    flowerMaterial.baseColor.w = 0.125F;
    uploadWorldMaterial(flowerMaterial);
    rlSetBlendFactorsSeparate(
        RL_ONE, RL_ZERO,
        RL_ONE, RL_ZERO,
        RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    for (std::size_t variant = FirstFlowerVariant;
         variant <= LastFlowerVariant; ++variant) {
        if (decorativeBushTransforms_[variant].empty() ||
            !resources_.decorativeBushModel(variant).valid()) {
            continue;
        }
        drawInstanced(
            resources_.decorativeBushModel(variant).get(),
            decorativeBushTransforms_[variant]);
    }
    EndBlendMode();
    uploadWorldMaterial(worldMaterial_);
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &disabled,
        SHADER_UNIFORM_INT);
}

void Renderer::drawDecorativeRockAo(
    Vector3 cameraPosition, float worldLimit,
    std::span<const GrassClearArea> clearAreas) {
    ensureGrassClearAreaIndex(clearAreas);
    if (!blobShadowBatchOpen_ ||
        terrainHeightfield_ == nullptr) {
        return;
    }
    constexpr float Spacing = 4.45F;
    constexpr float DrawRadius = 34.0F;
    constexpr float CoreClearRadius = 10.5F;
    constexpr std::size_t BushVariantCount = 9U;
    constexpr std::array<float, BushVariantCount> BushAoRadii{
        0.42F, 0.78F, 0.55F, 0.68F, 0.60F, 0.50F,
        0.0F, 0.34F, 0.38F,
    };
    constexpr float BushDrawRadius = 36.0F;
    const int cameraCellX = static_cast<int>(std::floor(
        (cameraPosition.x + 4.0F) / 8.0F));
    const int cameraCellZ = static_cast<int>(std::floor(
        (cameraPosition.z + 2.0F) / 8.0F));
    const bool cachedCandidates =
        decorativeInstanceCacheValid_ &&
        decorativeCacheCameraCellX_ == cameraCellX &&
        decorativeCacheCameraCellZ_ == cameraCellZ &&
        decorativeCacheQuality_ == settings_.quality &&
        decorativeCacheWorldLimit_ == worldLimit &&
        decorativeCacheTerrain_ == terrainHeightfield_ &&
        decorativeCacheRevealOrigin_.x == worldRevealOrigin_.x &&
        decorativeCacheRevealOrigin_.y == worldRevealOrigin_.y &&
        worldRevealElapsed_ >= 1.7F;
    if (cachedCandidates) {
        for (const auto& candidates : decorativeRockCandidates_) {
            for (const CachedInstance& instance : candidates) {
                const float offsetX =
                    instance.position.x - cameraPosition.x;
                const float offsetZ =
                    instance.position.y - cameraPosition.z;
                if (offsetX * offsetX + offsetZ * offsetZ >
                    DrawRadius * DrawRadius) {
                    continue;
                }
                if (decorationExclusionMap_.blocked(
                        instance.position.x, instance.position.y)) {
                    continue;
                }
                if (clearAreaVisibility(
                        instance.position, clearAreas,
                        0.001F, 0.65F) < 0.999F) {
                    continue;
                }
                drawBlobShadow(
                    {instance.position.x,
                     instance.groundHeight + 0.018F,
                     instance.position.y},
                    instance.scale * 0.62F,
                    instance.scale * 0.52F, 0.24F);
                drawBlobShadow(
                    {instance.position.x,
                     instance.groundHeight + 0.02F,
                     instance.position.y},
                    instance.scale * 0.34F,
                    instance.scale * 0.28F, 0.42F);
            }
        }
        for (std::size_t variant = 0;
             variant < BushVariantCount; ++variant) {
            for (const CachedInstance& instance :
                 decorativeBushCandidates_[variant]) {
                const float offsetX =
                    instance.position.x - cameraPosition.x;
                const float offsetZ =
                    instance.position.y - cameraPosition.z;
                if (offsetX * offsetX + offsetZ * offsetZ >
                    BushDrawRadius * BushDrawRadius) {
                    continue;
                }
                if (decorationExclusionMap_.blocked(
                        instance.position.x, instance.position.y)) {
                    continue;
                }
                if (clearAreaVisibility(
                        instance.position, clearAreas,
                        0.001F, 0.72F) < 0.999F) {
                    continue;
                }
                const float radius =
                    BushAoRadii[variant] * instance.scale;
                if (radius <= 0.001F) {
                    continue;
                }
                const bool largePlant = variant >= 7U;
                drawBlobShadow(
                    {instance.position.x,
                     instance.groundHeight + 0.018F,
                     instance.position.y},
                    radius, radius * 0.82F,
                    largePlant ? 0.07F : 0.11F);
                drawBlobShadow(
                    {instance.position.x,
                     instance.groundHeight + 0.02F,
                     instance.position.y},
                    radius * 0.52F, radius * 0.42F,
                    largePlant ? 0.12F : 0.19F);
            }
        }
        return;
    }
    const int minimumX = static_cast<int>(
        std::floor((cameraPosition.x - DrawRadius) / Spacing));
    const int maximumX = static_cast<int>(
        std::ceil((cameraPosition.x + DrawRadius) / Spacing));
    const int minimumZ = static_cast<int>(
        std::floor((cameraPosition.z - DrawRadius) / Spacing));
    const int maximumZ = static_cast<int>(
        std::ceil((cameraPosition.z + DrawRadius) / Spacing));
    const auto hashCell = [](int x, int z) {
        std::uint32_t value =
            static_cast<std::uint32_t>(x) * 0x8da6b343U ^
            static_cast<std::uint32_t>(z) * 0xd8163841U ^
            0x6c8e9cf5U;
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

    for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
        for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
            const std::uint32_t hash = hashCell(cellX, cellZ);
            const float x =
                (static_cast<float>(cellX) + 0.5F) * Spacing +
                (unitFloat(hash >> 7U) - 0.5F) * Spacing * 0.78F;
            const float z =
                (static_cast<float>(cellZ) + 0.5F) * Spacing +
                (unitFloat(hash >> 15U) - 0.5F) * Spacing * 0.78F;
            const float pathEdge = pathEdgeRockAmount(
                terrainHeightfield_, x, z);
            const float rockDensity = std::max(
                decorativeRockClusterDensity(x, z),
                pathEdge * 0.46F);
            if (unitFloat(hash) > rockDensity) {
                continue;
            }
            if (std::abs(x) > worldLimit - 0.7F ||
                std::abs(z) > worldLimit - 0.7F ||
                x * x + z * z <
                    CoreClearRadius * CoreClearRadius) {
                continue;
            }
            if (decorationExclusionMap_.blocked(x, z)) {
                continue;
            }
            if (terrainHeightfield_->waterSignedDistance(x, z) < 0.7) {
                continue;
            }
            const float pathAmount = static_cast<float>(
                terrainHeightfield_->pathAmount(x, z));
            if (pathAmount > 0.70F && pathEdge < 0.08F) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                DrawRadius * DrawRadius) {
                continue;
            }
            const float visibility = clearAreaVisibility(
                {x, z}, clearAreas, 0.001F, 0.65F);
            if (visibility < 0.999F) {
                continue;
            }
            const float scale =
                (0.55F +
                 unitFloat(hash ^ 0xa511e9b3U) * 0.45F) *
                (0.88F + decorativeRockClusterDensity(x, z) * 0.28F) *
                worldRevealScaleAt({x, z}) *
                (1.0F - pathEdge * 0.62F);
            if (scale <= 0.001F) {
                continue;
            }
            const float groundY = static_cast<float>(
                terrainHeightfield_->getHeight(x, z));
            drawBlobShadow(
                {x, groundY + 0.018F, z},
                scale * 0.62F, scale * 0.52F, 0.24F);
            drawBlobShadow(
                {x, groundY + 0.02F, z},
                scale * 0.34F, scale * 0.28F, 0.42F);
        }
    }

    constexpr float BushSpacing = 4.35F;
    constexpr std::array<float, BushVariantCount> BushVariantScales{
        1.35F, 0.62F, 0.82F, 0.70F, 0.90F, 1.00F,
        1.596F, 1.512F, 1.722F,
    };
    const int minimumBushX = static_cast<int>(std::floor(
        (cameraPosition.x - BushDrawRadius) / BushSpacing));
    const int maximumBushX = static_cast<int>(std::ceil(
        (cameraPosition.x + BushDrawRadius) / BushSpacing));
    const int minimumBushZ = static_cast<int>(std::floor(
        (cameraPosition.z - BushDrawRadius) / BushSpacing));
    const int maximumBushZ = static_cast<int>(std::ceil(
        (cameraPosition.z + BushDrawRadius) / BushSpacing));
    for (int cellZ = minimumBushZ;
         cellZ <= maximumBushZ; ++cellZ) {
        for (int cellX = minimumBushX;
             cellX <= maximumBushX; ++cellX) {
            const std::uint32_t hash =
                std::rotl(hashCell(cellX, cellZ), 13) ^ 0xb5297a4dU;
            const float x =
                (static_cast<float>(cellX) + 0.5F) * BushSpacing +
                (unitFloat(hash >> 6U) - 0.5F) * BushSpacing * 0.76F;
            const float z =
                (static_cast<float>(cellZ) + 0.5F) * BushSpacing +
                (unitFloat(hash >> 14U) - 0.5F) * BushSpacing * 0.76F;
            const float sharedCluster =
                decorativeRockClusterDensity(x, z);
            const float secondaryCluster =
                decorativeRockClusterDensity(x + 31.0F, z - 23.0F);
            const float clusterDensity = std::clamp(
                sharedCluster * 0.74F + secondaryCluster * 0.34F,
                0.0F, 0.88F);
            const std::size_t selector = static_cast<std::size_t>(
                (hash ^ 0x7f4a7c15U) % 14U);
            const std::size_t variant = selector < 6U
                ? selector
                : selector < 10U ? 6U : selector < 13U ? 7U : 8U;
            const bool flora = variant >= 6U;
            const float placementDensity = flora
                ? std::clamp(0.28F + clusterDensity * 0.62F,
                             0.28F, 0.82F)
                : clusterDensity;
            if (unitFloat(hash ^ 0x68e31da4U) > placementDensity ||
                std::abs(x) > worldLimit - 0.9F ||
                std::abs(z) > worldLimit - 0.9F ||
                x * x + z * z < CoreClearRadius * CoreClearRadius) {
                continue;
            }
            if (decorationExclusionMap_.blocked(x, z)) {
                continue;
            }
            if (terrainHeightfield_->waterSignedDistance(x, z) < 1.0) {
                continue;
            }
            if (terrainHeightfield_->pathAmount(x, z) > 0.06) {
                continue;
            }
            const float cameraX = x - cameraPosition.x;
            const float cameraZ = z - cameraPosition.z;
            if (cameraX * cameraX + cameraZ * cameraZ >
                BushDrawRadius * BushDrawRadius) {
                continue;
            }
            const float visibility = clearAreaVisibility(
                {x, z}, clearAreas, 0.001F, 0.72F);
            if (visibility < 0.999F) {
                continue;
            }
            const float scale =
                BushVariantScales[variant] *
                (0.82F + unitFloat(hash ^ 0x68e31da4U) * 0.43F) *
                (0.90F + sharedCluster * 0.22F) *
                worldRevealScaleAt({x, z});
            if (scale <= 0.001F) {
                continue;
            }
            const float groundY = static_cast<float>(
                terrainHeightfield_->getHeight(x, z));
            const float radius = BushAoRadii[variant] * scale;
            if (radius <= 0.001F) {
                continue;
            }
            const bool largePlant = variant >= 7U;
            drawBlobShadow(
                {x, groundY + 0.018F, z},
                radius, radius * 0.82F,
                largePlant ? 0.07F : 0.11F);
            drawBlobShadow(
                {x, groundY + 0.02F, z},
                radius * 0.52F, radius * 0.42F,
                largePlant ? 0.12F : 0.19F);
        }
    }
}

void Renderer::drawBoundaryForest() {
    if (!worldShaderActive_ ||
        terrainHeightfield_ == nullptr ||
        !resources_.worldShader().valid()) {
        return;
    }
    constexpr std::size_t VariantCount = 2U;
    constexpr float Spacing = 8.0F;
    constexpr std::size_t MaximumInstancesPerVariant = 4096U;
    const auto& terrain = *terrainHeightfield_;
    const auto& config = terrain.config();
    if (config.terrainBoundaryRiseWidth <= 0.0F ||
        config.terrainBoundaryRiseHeight <= 0.0F) {
        return;
    }
    const float terrainLimit = static_cast<float>(
        config.terrainWorldSize * 0.5);
    const float forestInnerLimit = static_cast<float>(
        config.terrainWorldSize * 0.5 -
        config.terrainBoundaryRiseWidth + 0.5);
    const float forestOuterLimit = terrainLimit - 0.5F;
    const int minimumX = static_cast<int>(std::floor(
        -terrainLimit / Spacing));
    const int maximumX = static_cast<int>(std::ceil(
        terrainLimit / Spacing));
    const int minimumZ = static_cast<int>(std::floor(
        -terrainLimit / Spacing));
    const int maximumZ = static_cast<int>(std::ceil(
        terrainLimit / Spacing));
    const auto hashCell = [](int x, int z) {
        std::uint32_t value =
            static_cast<std::uint32_t>(x) * 0x8da6b343U ^
            static_cast<std::uint32_t>(z) * 0xd8163841U ^
            0xa511e9b3U;
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

    if (!boundaryForestCached_) {
        for (auto& variant : boundaryForestTransforms_) {
            variant.clear();
            variant.reserve(1024U);
        }
        for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
            for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
                const std::uint32_t hash = hashCell(cellX, cellZ);
                const float x =
                    (static_cast<float>(cellX) + 0.5F) * Spacing +
                    (unitFloat(hash >> 7U) - 0.5F) * Spacing * 0.34F;
                const float z =
                    (static_cast<float>(cellZ) + 0.5F) * Spacing +
                    (unitFloat(hash >> 15U) - 0.5F) * Spacing * 0.34F;
                const float edgeDistance =
                    std::max(std::abs(x), std::abs(z));
                if (edgeDistance < forestInnerLimit ||
                    edgeDistance > forestOuterLimit) {
                    continue;
                }
                const float slopeProgress = std::clamp(
                    (edgeDistance - forestInnerLimit) /
                        std::max(
                            forestOuterLimit - forestInnerLimit,
                            0.001F),
                    0.0F, 1.0F);
                const float treeLine = 1.0F - std::clamp(
                    (slopeProgress - 0.48F) / 0.28F,
                    0.0F, 1.0F);
                if (unitFloat(hash ^ 0x51f15e1dU) > treeLine) {
                    continue;
                }
                const std::size_t variant =
                    static_cast<std::size_t>(
                        (hash >> 4U) % VariantCount);
                if (boundaryForestTransforms_[variant].size() >=
                    MaximumInstancesPerVariant) {
                    continue;
                }
                ModelResource& resource =
                    resources_.boundaryTreeModel(variant);
                if (!resource.valid()) {
                    continue;
                }
                const float sizeRoll =
                    unitFloat(hash ^ 0x63d83595U);
                const float scale =
                    5.0F + sizeRoll * sizeRoll * 6.0F +
                    (1.0F - slopeProgress) * 0.8F;
                const float yaw =
                    unitFloat(hash ^ 0x9e3779b9U) * PI * 2.0F;
                const float height = static_cast<float>(
                    terrain.getHeight(x, z));
                boundaryForestTransforms_[variant].push_back(
                    MatrixMultiply(
                        resource.get().transform,
                        MatrixMultiply(
                            MatrixMultiply(
                                MatrixScale(scale, scale, scale),
                                MatrixRotateY(yaw)),
                            MatrixTranslate(x, height, z))));
            }
        }
        boundaryForestCached_ = true;
    }

    Shader& shader = resources_.worldShader().get();
    const int enabled = 1;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &enabled,
        SHADER_UNIFORM_INT);
    setSkinningEnabled(shader, false);
    for (std::size_t variant = 0; variant < VariantCount;
         ++variant) {
        ModelResource& resource =
            resources_.boundaryTreeModel(variant);
        if (!resource.valid() ||
            boundaryForestTransforms_[variant].empty()) {
            continue;
        }
        Model& model = resource.get();
        const Matrix* transforms =
            boundaryForestTransforms_[variant].data();
        int transformCount = static_cast<int>(
            boundaryForestTransforms_[variant].size());
        if (worldRevealElapsed_ < 1.7F) {
            auto& revealTransforms =
                boundaryForestRevealTransforms_[variant];
            revealTransforms.clear();
            revealTransforms.reserve(
                boundaryForestTransforms_[variant].size());
            for (const Matrix transform :
                 boundaryForestTransforms_[variant]) {
                const float revealScale =
                    worldRevealScaleAt(
                        {transform.m12, transform.m14});
                if (revealScale <= 0.001F) {
                    continue;
                }
                revealTransforms.push_back(
                    MatrixMultiply(
                        MatrixScale(
                            revealScale,
                            revealScale,
                            revealScale),
                        transform));
            }
            transforms = revealTransforms.data();
            transformCount = static_cast<int>(
                revealTransforms.size());
            if (transformCount == 0) {
                continue;
            }
        }
        for (int meshIndex = 0; meshIndex < model.meshCount;
             ++meshIndex) {
            const int materialIndex =
                model.meshMaterial[meshIndex];
            Material material = model.materials[materialIndex];
            material.shader = shader;
            material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
            DrawMeshInstanced(
                model.meshes[meshIndex], material,
                transforms, transformCount);
        }
    }
    const int disabled = 0;
    rlDrawRenderBatchActive();
    SetShaderValue(
        shader, worldInstancingEnabledLocation_, &disabled,
        SHADER_UNIFORM_INT);
}


} // namespace ian
