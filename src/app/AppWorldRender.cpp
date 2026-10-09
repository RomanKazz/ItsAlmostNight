#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "app/AppProjectileDrawing.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/BuildingModifiers.hpp"
#include "buildings/CatapultRig.hpp"
#include "game/ChallengeArena.hpp"
#include "graphics/CameraCulling.hpp"
#include "presentation/PresentationEffectQueries.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace ian {

using namespace app_detail;
using namespace app_projectile_drawing;

namespace {

constexpr int ChallengeFencePegCount = 36;
constexpr float ChallengeFencePegHeight = 2.25F;
constexpr float ChallengeFenceRopeHeight = 1.56F;
constexpr int ChallengeFenceRopeSegments = 5;

float challengeFencePegProgress(float fenceProgress, int index) {
    const float phase =
        static_cast<float>(index) /
        static_cast<float>(ChallengeFencePegCount) * 0.34F;
    const float amount = std::clamp(
        (fenceProgress - phase) / 0.66F, 0.0F, 1.0F);
    return amount * amount * (3.0F - 2.0F * amount);
}

float challengeFenceAngle(int index) {
    return 2.0F * PI * static_cast<float>(index) /
        static_cast<float>(ChallengeFencePegCount);
}
















} // namespace

std::span<const GrassClearArea>
App::activeDecorationClearAreas(
    const SimulationSnapshot& snapshot) const {
    const double cellSize =
        simulation_.terrain().config().cellSize;
    const auto add = [](
                         std::vector<GrassClearArea>& areas,
                         double x, double z, double radius) {
        areas.push_back({
            .center = {
                static_cast<float>(x),
                static_cast<float>(z),
            },
            .innerRadius =
                static_cast<float>(radius),
            .amount = 1.0F,
        });
    };
    const auto addPlatform = [cellSize, &add](
                                 std::vector<GrassClearArea>& areas,
                                 const auto& placement) {
        add(
            areas,
            (placement.anchor.x + 1.0) * cellSize,
            (placement.anchor.z + 1.0) * cellSize,
            cellSize * 1.35);
    };
    const auto addWall = [cellSize, &add](
                             std::vector<GrassClearArea>& areas,
                             const auto& placement) {
        add(
            areas,
            (placement.anchor.x + 0.5) * cellSize,
            (placement.anchor.z + 0.5) * cellSize,
            cellSize * 0.72);
    };
    const auto addRamp = [cellSize, &add](
                             std::vector<GrassClearArea>& areas,
                             const auto& placement) {
        const bool alongZ =
            placement.rotation == Rotation::Deg0 ||
            placement.rotation == Rotation::Deg180;
        const int widthCells =
            alongZ ? ModularRampWidthCells
                   : ModularRampRunCells;
        const int depthCells =
            alongZ ? ModularRampRunCells
                   : ModularRampWidthCells;
        add(
            areas,
            (placement.anchor.x + widthCells * 0.5) *
                cellSize,
            (placement.anchor.z + depthCells * 0.5) *
                cellSize,
            cellSize * 0.5 *
                std::hypot(widthCells, depthCells));
    };

    // The simulation owns stable vectors. Their address, size and newest
    // entity id form an O(1) structural key that changes on placement,
    // removal and save restoration without scanning every item each frame.
    std::uint64_t fingerprint = 0xcbf29ce484222325ULL;
    const auto mix = [&fingerprint](std::uint64_t value) {
        fingerprint ^= value + 0x9e3779b97f4a7c15ULL +
            (fingerprint << 6U) + (fingerprint >> 2U);
    };
    const auto mixCollection = [&mix](const auto& items) {
        mix(static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(items.data())));
        mix(static_cast<std::uint64_t>(items.size()));
        if (!items.empty()) {
            mix(static_cast<std::uint64_t>(items.back().id.index));
            mix(static_cast<std::uint64_t>(items.back().id.generation));
        }
    };
    mixCollection(snapshot.buildings);
    mixCollection(snapshot.platformFrames);
    mixCollection(snapshot.modularWalls);
    mixCollection(snapshot.ramps);

    std::vector<GrassClearArea>& persistent =
        persistentDecorationClearAreaBuffer_;
    if (fingerprint != grassClearAreaFingerprint_) {
        grassClearAreaFingerprint_ = fingerprint;
        persistent.clear();
        persistent.reserve(
            snapshot.buildings.size() +
            snapshot.platformFrames.size() +
            snapshot.modularWalls.size() +
            snapshot.ramps.size());
        for (const BuildingInstance& building : snapshot.buildings) {
            const Vec3 center = buildingWorldPosition(building);
            add(
                persistent, center.x, center.z,
                buildingFootprintHalfExtent(building.type) + 0.18);
        }
        for (const PlatformFrameInstance& frame :
             snapshot.platformFrames) {
            addPlatform(persistent, frame);
        }
        for (const WallInstance& wall : snapshot.modularWalls) {
            addWall(persistent, wall);
        }
        for (const RampInstance& ramp : snapshot.ramps) {
            addRamp(persistent, ramp);
        }
    }

    const bool hasPortal = std::any_of(
        snapshot.attackPortalPositions.begin(),
        snapshot.attackPortalPositions.end(),
        [](const auto& portal) { return portal.has_value(); });
    const bool hasDynamicAreas =
        platformFramePreview_ || wallPreview_ || rampPreview_ ||
        !modularPlatformDragPreviews_.empty() ||
        !modularWallDragPreviews_.empty() ||
        !modularRampDragPreviews_.empty() ||
        snapshot.buildingPreview || hasPortal;
    if (!hasDynamicAreas) {
        return std::span<const GrassClearArea>{persistent};
    }

    std::vector<GrassClearArea>& result =
        activeDecorationClearAreaBuffer_;
    result.assign(persistent.begin(), persistent.end());
    result.reserve(
        persistent.size() +
        modularPlatformDragPreviews_.size() +
        modularWallDragPreviews_.size() +
        modularRampDragPreviews_.size() + 16U);

    if (platformFramePreview_ &&
        !modularDragPiece_) {
        addPlatform(result, *platformFramePreview_);
    }
    for (const auto& placement :
         modularPlatformDragPreviews_) {
        addPlatform(result, placement);
    }
    if (wallPreview_ && !modularDragPiece_) {
        addWall(result, *wallPreview_);
    }
    for (const auto& placement :
         modularWallDragPreviews_) {
        addWall(result, placement);
    }
    if (rampPreview_ && !modularDragPiece_) {
        addRamp(result, *rampPreview_);
    }
    for (const auto& placement :
         modularRampDragPreviews_) {
        addRamp(result, placement);
    }

    if (snapshot.buildingPreview) {
        const BuildingPreview& preview =
            *snapshot.buildingPreview;
        std::vector<GridPosition> cells{
            preview.gridPosition};
        if (wallDragStart_ && wallDragEnd_ &&
            placementDragType_ &&
            *placementDragType_ == preview.type) {
            cells = placementLine(
                preview.type, *wallDragStart_,
                *wallDragEnd_, placementDragAxis_);
        }
        const double radius =
            buildingFootprintHalfExtent(preview.type) +
            0.18;
        for (const GridPosition cell : cells) {
            const Vec3 center =
                buildingWorldPosition(preview.type, cell);
            add(result, center.x, center.z, radius);
        }
    }
    for (const auto& portal : snapshot.attackPortalPositions) {
        if (portal) {
            add(result, portal->x, portal->z, 2.6);
        }
    }
    return std::span<const GrassClearArea>{result};
}

void App::drawChallengeFence(
    const ChallengeColumnInstance& column, bool drawRopes) {
    const float fenceProgress = std::clamp(
        static_cast<float>(column.fenceProgress), 0.0F, 1.0F);
    if (fenceProgress <= 0.001F) return;

    std::array<Vector3, ChallengeFencePegCount> ropeAnchors{};
    std::array<float, ChallengeFencePegCount> pegProgress{};
    for (int index = 0; index < ChallengeFencePegCount; ++index) {
        const float angle = challengeFenceAngle(index);
        const float x = static_cast<float>(column.position.x) +
            std::cos(angle) *
                static_cast<float>(challenge_arena::FenceRadius);
        const float z = static_cast<float>(column.position.z) +
            std::sin(angle) *
                static_cast<float>(challenge_arena::FenceRadius);
        const float groundY = static_cast<float>(
            simulation_.terrain().getHeight(x, z));
        const float amount = challengeFencePegProgress(
            fenceProgress, index);
        pegProgress[static_cast<std::size_t>(index)] = amount;
        const float bounce = std::sin(amount * PI) *
            (1.0F - amount) * 0.16F;
        const float buried = (1.0F - amount) *
            ChallengeFencePegHeight * 0.42F;
        const float baseY = groundY - buried + bounce;
        ropeAnchors[static_cast<std::size_t>(index)] = {
            x,
            baseY + ChallengeFenceRopeHeight * amount,
            z,
        };
        if (amount <= 0.002F) continue;
        // Broad face is tangent to the circle; its normal points radially.
        const float yaw = PI * 0.5F - angle;
        static_cast<void>(renderer_->drawChallengeArenaPeg(
            {x, baseY, z}, yaw, WHITE, amount));
    }

    if (!drawRopes) return;
    const Color ropeShadow{145, 94, 58, 255};
    const Color ropeHighlight{228, 170, 108, 255};
    for (int index = 0; index < ChallengeFencePegCount; ++index) {
        const int nextIndex = (index + 1) % ChallengeFencePegCount;
        const float visibility = std::min(
            pegProgress[static_cast<std::size_t>(index)],
            pegProgress[static_cast<std::size_t>(nextIndex)]);
        if (visibility <= 0.04F) continue;
        const Vector3 start =
            ropeAnchors[static_cast<std::size_t>(index)];
        const Vector3 end =
            ropeAnchors[static_cast<std::size_t>(nextIndex)];
        Vector3 previous = start;
        for (int segment = 1;
             segment <= ChallengeFenceRopeSegments; ++segment) {
            const float t = static_cast<float>(segment) /
                static_cast<float>(ChallengeFenceRopeSegments);
            Vector3 point{
                start.x + (end.x - start.x) * t,
                start.y + (end.y - start.y) * t,
                start.z + (end.z - start.z) * t,
            };
            point.y -= 0.52F * 4.0F * t * (1.0F - t) * visibility;
            const float outerRadius = 0.192F * visibility;
            const float innerRadius = 0.114F * visibility;
            const Vector3 segmentDelta = Vector3Subtract(point, previous);
            const float segmentLength = Vector3Length(segmentDelta);
            const Vector3 segmentDirection = segmentLength > 0.0001F
                ? Vector3Scale(segmentDelta, 1.0F / segmentLength)
                : Vector3{};
            const float overlap = std::min(
                outerRadius * 0.82F, segmentLength * 0.18F);
            const Vector3 joinedStart = Vector3Subtract(
                previous, Vector3Scale(segmentDirection, overlap));
            const Vector3 joinedEnd = Vector3Add(
                point, Vector3Scale(segmentDirection, overlap));
            DrawCylinderEx(
                joinedStart, joinedEnd,
                outerRadius, outerRadius,
                6, ropeShadow);
            Vector3 highlightStart = joinedStart;
            Vector3 highlightEnd = joinedEnd;
            highlightStart.y += 0.096F * visibility;
            highlightEnd.y += 0.096F * visibility;
            DrawCylinderEx(
                highlightStart, highlightEnd,
                innerRadius, innerRadius,
                6, ropeHighlight);
            previous = point;
        }
    }
}

void App::drawWorldEntities(
    const SimulationSnapshot& snapshot, const Camera3D& camera,
    float nightAmount, const WorldLighting& lighting,
    float interpolationAlpha) {
    const auto decorationsStart = PerformanceClock::now();
    const auto clearAreas =
        activeDecorationClearAreas(snapshot);
    const auto horizontalView =
        camera_culling::horizontalView(camera);
    const float worldDrawDistance = static_cast<float>(
        simulation_.terrain().config().terrainRenderDistance + 20.0);
    const auto worldObjectVisible = [camera, horizontalView](
                                        Vec3 position, float radius,
                                        float maximumDistance) {
        const float offsetX =
            static_cast<float>(position.x) - camera.position.x;
        const float offsetZ =
            static_cast<float>(position.z) - camera.position.z;
        const float distanceLimit = maximumDistance + radius;
        return camera_culling::visibleInHorizontalRange(
            offsetX, offsetZ, horizontalView,
            distanceLimit, radius);
    };
    if constexpr (!MinimalWorldVisuals) {
        WorldMaterialState obstacleMaterial{};
        obstacleMaterial.bakedAo = 0.74F;
        renderer_->setWorldMaterial(obstacleMaterial);
        for (const auto& obstacle : snapshot.mapObstacles) {
            const float width = static_cast<float>(
                obstacle.collision.maxX - obstacle.collision.minX);
            const float depth = static_cast<float>(
                obstacle.collision.maxZ - obstacle.collision.minZ);
            const Vector3 center{
                static_cast<float>(
                    (obstacle.collision.minX + obstacle.collision.maxX) *
                    0.5),
                static_cast<float>(obstacle.height * 0.5),
                static_cast<float>(
                    (obstacle.collision.minZ + obstacle.collision.maxZ) *
                    0.5),
            };
            if (!worldObjectVisible(
                    {center.x, center.y, center.z},
                    0.5F * std::hypot(width, depth),
                    worldDrawDistance)) {
                continue;
            }
            DrawCube(center, width,
                     static_cast<float>(obstacle.height), depth,
                     {99, 111, 122, 255});
        }
    }
    if constexpr (!MinimalWorldVisuals) {
        WorldMaterialState boundaryForestMaterial{};
        boundaryForestMaterial.baseColor = {
            0.45F, 0.53F, 0.43F, 1.0F};
        boundaryForestMaterial.bakedAo = 0.72F;
        boundaryForestMaterial.screenAoAmount = 0.0F;
        boundaryForestMaterial.windAmount = 0.18F;
        boundaryForestMaterial.distantFadeAmount = 1.0F;
        boundaryForestMaterial.vegetationAmount = 1.0F;
        boundaryForestMaterial.treeAmount = 1.0F;
        boundaryForestMaterial.coniferAmount = 1.0F;
        renderer_->setWorldMaterial(boundaryForestMaterial);
        renderer_->drawBoundaryForest(camera);
    }
    WorldMaterialState decorativeRockMaterial{};
    decorativeRockMaterial.bakedAo = 0.82F;
    decorativeRockMaterial.screenAoAmount = 0.0F;
    renderer_->setWorldMaterial(decorativeRockMaterial);
    renderer_->drawDecorativeRocks(
        camera,
        static_cast<float>(snapshot.worldLimit),
        clearAreas);
    performanceStats_.decorationsRender.sample(
        performanceMilliseconds(decorationsStart));
    const float resourceDrawDistance = static_cast<float>(
        simulation_.terrain().config().terrainRenderDistance +
        12.0);
    resourceTreeDrawInstances_.clear();
    resourceTreeDrawInstances_.reserve(snapshot.resourceNodes.size());
    resourceRockDrawInstances_.clear();
    resourceRockDrawInstances_.reserve(snapshot.resourceNodes.size());
    for (const auto& node : snapshot.resourceNodes) {
        if (!node.active ||
            (MinimalWorldVisuals &&
             node.type != ResourceType::Wood &&
             node.type != ResourceType::Stone)) {
            continue;
        }
        if (!worldObjectVisible(
                node.position, 3.5F, resourceDrawDistance)) {
            continue;
        }

        const Vec3 hitOffset =
            presentation::resourceHitOffset(
                effects_, node.id, node.position);
        const Vector3 nodePosition = {
            static_cast<float>(
                node.position.x + hitOffset.x),
            static_cast<float>(
                node.position.y - node.groundOffset),
            static_cast<float>(
                node.position.z + hitOffset.z),
        };
        WorldMaterialState material{};
        material.bakedAo = 0.78F;
        material.windAmount =
            node.type == ResourceType::Wood ? 1.0F : 0.0F;
        if (node.type == ResourceType::Wood) {
            material.vegetationAmount = 1.0F;
            material.treeAmount = 1.0F;
        }
        material.hitFlashAmount =
            presentation::resourceHitFlash(
                effects_, node.id);
        if (isHarvestableResource(node.type) &&
            material.hitFlashAmount > 0.001F) {
            if (const auto hitPosition =
                    presentation::resourceHitPosition(
                        effects_, node.id)) {
                material.hitFlashPosition = {
                    static_cast<float>(hitPosition->x),
                    static_cast<float>(hitPosition->y),
                    static_cast<float>(hitPosition->z),
                };
                material.hitFlashRadius = 0.92F;
            }
        }
        const float hitScale =
            presentation::resourceHitScale(
                effects_, node.id);
        if (node.type == ResourceType::Wood) {
            if (material.hitFlashAmount <= 0.001F) {
                resourceTreeDrawInstances_.push_back({
                    .position = nodePosition,
                    .yawRadians =
                        static_cast<float>(node.visualYaw),
                    .scale = hitScale *
                        static_cast<float>(node.visualScale),
                    .visualVariant = node.visualVariant,
                });
                continue;
            }
            renderer_->setWorldMaterial(material);
            if (!renderer_->drawTree(
                    nodePosition,
                    WHITE,
                    hitScale * static_cast<float>(node.visualScale),
                    node.visualVariant,
                    static_cast<float>(node.visualYaw))) {
                DrawCylinder(
                    {nodePosition.x,
                     nodePosition.y + 0.9F,
                     nodePosition.z}, 0.32F,
                    0.42F, 1.8F, 8, {112, 74, 42, 255});
                DrawSphere(
                    {nodePosition.x,
                     nodePosition.y + 2.2F,
                     nodePosition.z}, 1.15F,
                    {58, 124, 67, 255});
            }
        } else if (node.type == ResourceType::Stone) {
            if (material.hitFlashAmount <= 0.001F) {
                resourceRockDrawInstances_.push_back({
                    .position = nodePosition,
                    .yawRadians =
                        static_cast<float>(node.visualYaw),
                    .scale = hitScale,
                    .visualVariant = node.visualVariant,
                });
                continue;
            }
            renderer_->setWorldMaterial(material);
            if (!renderer_->drawRock(
                    nodePosition,
                    WHITE, hitScale, node.visualVariant,
                    static_cast<float>(node.visualYaw))) {
                DrawSphere(
                    nodePosition, 0.9F, {104, 116, 128, 255});
            }
        } else if (node.type == ResourceType::Crystal) {
            renderer_->setWorldMaterial(material);
            static_cast<void>(renderer_->drawCrystalResource(
                nodePosition, WHITE,
                hitScale * static_cast<float>(node.visualScale),
                static_cast<float>(node.visualYaw),
                [&]() {
                    const Vec3 normal = simulation_.terrain().getNormal(
                        node.position.x, node.position.z);
                    return Vector3{
                        static_cast<float>(normal.x),
                        static_cast<float>(normal.y),
                        static_cast<float>(normal.z)};
                }()));
        } else {
            renderer_->setWorldMaterial(material);
            static_cast<void>(renderer_->drawDestructibleProp(
                node.type, nodePosition,
                static_cast<float>(node.visualYaw), WHITE,
                hitScale * static_cast<float>(node.visualScale)));
        }
    }
    if (!resourceRockDrawInstances_.empty()) {
        WorldMaterialState rockMaterial{};
        rockMaterial.bakedAo = 0.78F;
        renderer_->setWorldMaterial(rockMaterial);
        if (!renderer_->drawRocksInstanced(
                resourceRockDrawInstances_)) {
            for (const RockDrawInstance& rock :
                 resourceRockDrawInstances_) {
                static_cast<void>(renderer_->drawRock(
                    rock.position, WHITE, rock.scale,
                    rock.visualVariant, rock.yawRadians));
            }
        }
    }
    if (!resourceTreeDrawInstances_.empty()) {
        WorldMaterialState treeMaterial{};
        treeMaterial.bakedAo = 0.78F;
        treeMaterial.windAmount = 1.0F;
        treeMaterial.vegetationAmount = 1.0F;
        treeMaterial.treeAmount = 1.0F;
        renderer_->setWorldMaterial(treeMaterial);
        if (!renderer_->drawTreesInstanced(
                resourceTreeDrawInstances_)) {
            for (const TreeDrawInstance& tree :
                 resourceTreeDrawInstances_) {
                static_cast<void>(renderer_->drawTree(
                    tree.position, WHITE, tree.scale,
                    tree.visualVariant, tree.yawRadians));
            }
        }
    }
    if constexpr (MinimalWorldVisuals) {
        renderer_->endWorldShader();
        const auto grassStart = PerformanceClock::now();
        renderer_->drawGrassInstances(
            camera.position, static_cast<float>(snapshot.worldLimit),
            nightAmount, lighting, clearAreas);
        performanceStats_.grassRender.sample(
            performanceMilliseconds(grassStart));
        return;
    }
    // Draw the additive loot light in world space before the opaque chest.
    // It keeps terrain depth occlusion and the chest naturally covers the
    // part of the glow that is physically behind its body and lid.
    renderer_->endWorldShader();
    drawChestLootGlow(snapshot, camera);
    renderer_->beginWorldShader(lighting);
    WorldMaterialState challengeMaterial{};
    challengeMaterial.bakedAo = 0.76F;
    renderer_->setWorldMaterial(challengeMaterial);
    for (const ChallengeColumnInstance& column : snapshot.challengeColumns) {
        if (!worldObjectVisible(
                column.position,
                static_cast<float>(challenge_arena::FenceRadius + 3.0),
                worldDrawDistance)) {
            continue;
        }
        drawChallengeFence(column, true);
        const float progress = smoothstep(
            0.0F, 1.0F, static_cast<float>(column.completionProgress));
        const float bounce = std::sin(progress * PI) * (1.0F - progress) * 0.12F;
        const float scale = std::max(0.0F, 1.0F + bounce - progress);
        if (scale <= 0.001F) continue;
        const Color tint = column.state == ChallengeColumnState::Active
            ? Color{205, 235, 255, 255}
            : WHITE;
        static_cast<void>(renderer_->drawChallengeColumn(
            {static_cast<float>(column.position.x),
             static_cast<float>(column.position.y),
             static_cast<float>(column.position.z)},
            static_cast<float>(column.yaw), tint, scale));
    }
    WorldMaterialState landmarkMaterial{};
    landmarkMaterial.bakedAo = 0.82F;
    renderer_->setWorldMaterial(landmarkMaterial);
    for (const WorldLandmarkInstance& landmark : snapshot.worldLandmarks) {
        if (!worldObjectVisible(
                landmark.position,
                static_cast<float>(landmark.collisionRadius + 3.0),
                worldDrawDistance)) {
            continue;
        }
        const Color tint = landmark.activated
            ? WHITE
            : Color{218, 218, 210, 255};
        static_cast<void>(renderer_->drawWorldLandmark(
            static_cast<std::size_t>(landmark.type),
            {static_cast<float>(landmark.position.x),
             static_cast<float>(landmark.position.y),
             static_cast<float>(landmark.position.z)},
            static_cast<float>(landmark.yaw), tint));
    }
    WorldMaterialState chestMaterial{};
    chestMaterial.bakedAo = 0.78F;
    renderer_->setWorldMaterial(chestMaterial);
    for (const LootChestInstance& chest : snapshot.lootChests) {
        if (chest.looseLoot ||
            !worldObjectVisible(
                chest.position, 2.0F, worldDrawDistance)) {
            continue;
        }
        const float disappear = smoothstep(
            0.0F, 1.0F,
            static_cast<float>(
                chest.disappearanceProgress));
        const float bounce =
            std::sin(disappear*PI)*
            (1.0F - disappear)*0.16F;
        const float visualScale =
            1.0F + bounce - disappear*0.92F;
        const Vector3 position{
            static_cast<float>(
                chest.position.x +
                chest.surfaceNormal.x*disappear*0.24),
            static_cast<float>(
                chest.position.y +
                chest.surfaceNormal.y*disappear*0.24),
            static_cast<float>(
                chest.position.z +
                chest.surfaceNormal.z*disappear*0.24),
        };
        const Color tint{
            255,
            static_cast<unsigned char>(
                std::lround(255.0F - disappear*40.0F)),
            static_cast<unsigned char>(
                std::lround(255.0F - disappear*92.0F)),
            static_cast<unsigned char>(
                std::lround(255.0F*(1.0F - disappear))),
        };
        static_cast<void>(renderer_->drawLootChest(
            chest.type, position, static_cast<float>(chest.yaw),
            static_cast<float>(chest.openingProgress),
            tint, visualScale));
    }
    renderer_->beginWorldShader(lighting);
    for (const DestroyedResourceVisual& visual :
         destroyedResourceVisuals_) {
        if (!worldObjectVisible(
                visual.position, 3.5F, worldDrawDistance)) {
            continue;
        }
        const float progress = std::clamp(
            static_cast<float>(
                1.0 - visual.remaining / visual.duration),
            0.0F, 1.0F);
        const float eased =
            progress * progress * (3.0F - 2.0F * progress);
        const float scale =
            1.0F - eased * 0.82F;
        const float fade =
            1.0F - smoothstep(0.2F, 1.0F, progress);
        WorldMaterialState material{};
        material.bakedAo = 0.78F;
        material.baseColor = {1.0F, 1.0F, 1.0F, fade};
        material.windAmount =
            visual.type == ResourceType::Wood ? 1.0F : 0.0F;
        material.vegetationAmount =
            visual.type == ResourceType::Wood ? 1.0F : 0.0F;
        material.treeAmount =
            visual.type == ResourceType::Wood ? 1.0F : 0.0F;
        renderer_->setWorldMaterial(material);
        const Vector3 position{
            static_cast<float>(visual.position.x),
            static_cast<float>(
                simulation_.terrain().getHeight(
                    visual.position.x,
                    visual.position.z)),
            static_cast<float>(visual.position.z),
        };
        if (visual.type == ResourceType::Wood) {
            static_cast<void>(
                renderer_->drawTree(
                    position, WHITE,
                    scale * visual.visualScale,
                    visual.visualVariant,
                    visual.visualYaw));
        } else if (visual.type == ResourceType::Stone) {
            static_cast<void>(
                renderer_->drawRock(
                    position, WHITE, scale,
                    visual.visualVariant, visual.visualYaw));
        } else if (visual.type == ResourceType::Crystal) {
            static_cast<void>(renderer_->drawCrystalResource(
                position, WHITE,
                scale * visual.visualScale,
                visual.visualYaw,
                [&]() {
                    const Vec3 normal = simulation_.terrain().getNormal(
                        visual.position.x, visual.position.z);
                    return Vector3{
                        static_cast<float>(normal.x),
                        static_cast<float>(normal.y),
                        static_cast<float>(normal.z)};
                }()));
        } else {
            static_cast<void>(renderer_->drawDestructibleProp(
                visual.type, position, visual.visualYaw, WHITE,
                scale * visual.visualScale));
        }
    }
    WorldMaterialState platformMaterial{};
    platformMaterial.bakedAo = 0.76F;
    {
        renderer_->setWorldMaterial(platformMaterial);
        const double cellSize =
            simulation_.terrain().config().cellSize;
        const auto animationScales =
            modularAnimationScales(snapshot);
        modularBuildingRenderer_.drawWorld(
            {
                snapshot.platformFrames,
                snapshot.modularWalls,
                snapshot.ramps,
                snapshot.sharedSupports,
                cellSize,
                std::nullopt,
                animationScales,
                1.0F,
                Vec3{
                    camera.position.x,
                    camera.position.y,
                    camera.position.z},
                Vec3{
                    camera.target.x - camera.position.x,
                    camera.target.y - camera.position.y,
                    camera.target.z - camera.position.z},
                simulation_.terrain().config().terrainRenderDistance +
                    20.0,
            },
            {});
    }
    for (const auto& building : snapshot.buildings) {
        const Vec3 center = buildingWorldPosition(building);
        if (!worldObjectVisible(
                center, 4.0F, worldDrawDistance)) {
            continue;
        }
        const Vec3 impact =
            buildingImpactOffsetAt(building.id);
        float defensiveYaw = 0.0F;
        if (building.type == BuildingType::Turret ||
            building.type == BuildingType::GunTurret) {
            defensiveYaw = towerYaw(snapshot, building, interpolationAlpha);
        } else if (building.type == BuildingType::Cannon ||
                   building.type == BuildingType::Catapult) {
            defensiveYaw = cannonYaw(snapshot, building, interpolationAlpha);
        }
        const Vec3 shotRecoil =
            buildingShotRecoilOffsetAt(
                building.id, defensiveYaw);
        const bool pivotOnlyRecoil =
            building.type == BuildingType::GunTurret;
        const float x =
            static_cast<float>(
                center.x + impact.x +
                (pivotOnlyRecoil ? 0.0 : shotRecoil.x));
        const float z =
            static_cast<float>(
                center.z + impact.z +
                (pivotOnlyRecoil ? 0.0 : shotRecoil.z));
        const float groundY =
            static_cast<float>(center.y);
        const float spawnScale =
            buildingAnimationScaleAt(center, building.id);
        const auto scaledPosition =
            [x, groundY, z, spawnScale](
                float offsetX, float y, float offsetZ) {
                return Vector3{
                    x + offsetX * spawnScale,
                    groundY + y * spawnScale,
                    z + offsetZ * spawnScale,
                };
            };
        const auto drawScaledCube =
            [&scaledPosition, spawnScale](
                float offsetX, float y, float offsetZ,
                float width, float height, float depth,
                Color color) {
                DrawCube(
                    scaledPosition(offsetX, y, offsetZ),
                    width * spawnScale, height * spawnScale,
                    depth * spawnScale, color);
            };
        if (const auto foundation =
                automaticBuildingFoundation(
                    building.type,
                    building.gridPosition,
                    building.baseHeight,
                    building
                        .foundationBottomHeight,
                    simulation_.terrain()
                        .config().cellSize,
                    building.id)) {
            renderer_->setWorldMaterial(
                platformMaterial);
            const std::array<ModularAnimationScale, 1>
                foundationAnimation{{{
                    .id = building.id,
                    .scale = spawnScale,
                }}};
            modularBuildingRenderer_.drawWorld(
                {
                    std::span<
                        const PlatformFrameInstance>{
                        &*foundation, 1U},
                    {}, {}, {},
                    simulation_.terrain()
                        .config().cellSize,
                    std::nullopt,
                    foundationAnimation, 1.0F,
                },
                {});
        }
        WorldMaterialState material{};
        material.bakedAo = 0.72F;
        renderer_->setWorldMaterial(material);
        if (building.type == BuildingType::Core) {
            constexpr float QuarterTurn = PI * 0.5F;
            if (!renderer_->drawCore(
                    {x, groundY, z},
                    static_cast<float>(building.rotation) *
                        QuarterTurn,
                    WHITE, spawnScale, building.level)) {
                drawScaledCube(0.0F, 1.25F, 0.0F, 2.0F,
                               2.5F, 2.0F,
                               {219, 151, 60, 255});
            }
        } else if (building.type == BuildingType::Wall) {
            const std::uint8_t connections =
                wallConnectionMask(
                    snapshot.buildings,
                    building.gridPosition,
                    building.baseHeight);
            const float fallbackYaw = connections == 0U
                ? static_cast<float>(
                      wallFallbackRotation(
                          snapshot.buildings,
                          building)) *
                      PI * 0.5F
                : 0.0F;
            if (!renderer_->drawWall(
                    {x, groundY, z}, connections,
                    fallbackYaw,
                    WHITE, spawnScale, building.level)) {
                drawScaledCube(0.0F, 1.0F, 0.0F, 1.0F,
                               2.0F, 1.0F,
                               {126, 86, 54, 255});
            }
        } else if (building.type == BuildingType::Turret) {
            if (!renderer_->drawCrossbow(
                    {x, groundY, z},
                    defensiveYaw, WHITE,
                    spawnScale,
                    towerPitch(snapshot, building, interpolationAlpha), building.level)) {
                drawScaledCube(0.0F, 0.6F, 0.0F, 1.0F,
                               1.2F, 1.0F,
                               {68, 83, 96, 255});
                DrawCylinder(
                    scaledPosition(0.0F, 1.45F, 0.0F),
                    0.42F * spawnScale,
                    0.32F * spawnScale,
                    0.7F * spawnScale, 8,
                    {176, 128, 60, 255});
                drawScaledCube(
                    0.0F, 1.55F, -0.55F, 0.18F, 0.18F,
                    1.0F, {50, 58, 67, 255});
            }
        } else if (building.type == BuildingType::GunTurret) {
            if (!renderer_->drawGunTurret(
                    {x, groundY, z},
                    towerBaseYaw(snapshot, building),
                    defensiveYaw, WHITE, spawnScale,
                    shotRecoil, building.level)) {
                drawScaledCube(0.0F, 0.55F, 0.0F, 1.1F,
                               1.1F, 1.1F,
                               {81, 92, 101, 255});
            }
        } else if (
            building.type == BuildingType::CrystalMine ||
            building.type == BuildingType::LumberMill ||
            building.type == BuildingType::Quarry) {
            constexpr float QuarterTurn = PI * 0.5F;
            if (!renderer_->drawResourceProducer(
                    building.type, {x, groundY, z},
                    static_cast<float>(building.rotation) *
                        QuarterTurn,
                    WHITE,
                    spawnScale *
                        productionScaleAt(
                            building.id), building.level)) {
                drawScaledCube(0.0F, 0.275F, 0.0F, 1.0F,
                               0.55F, 1.0F,
                               {82, 101, 142, 255});
            }
        } else if (building.type == BuildingType::Cannon) {
            if (!renderer_->drawCannon({x, groundY, z},
                                       defensiveYaw,
                                       cannonPitch(snapshot, building, interpolationAlpha),
                                       WHITE, spawnScale, building.level)) {
                drawScaledCube(0.0F, 0.6F, 0.0F, 1.0F,
                               1.2F, 1.0F,
                               {62, 70, 78, 255});
                DrawSphere(
                    scaledPosition(0.0F, 1.35F, 0.0F),
                    0.48F * spawnScale,
                    {83, 91, 99, 255});
                drawScaledCube(
                    0.0F, 1.45F, -0.75F, 0.28F, 0.28F,
                    1.4F, {42, 48, 54, 255});
            }
        } else if (building.type == BuildingType::Catapult) {
            if (!renderer_->drawCatapult(
                    {x, groundY, z}, defensiveYaw,
                    cannonPitch(snapshot, building, interpolationAlpha),
                    false, WHITE,
                    spawnScale, building.level)) {
                drawScaledCube(0.0F, 0.45F, 0.0F, 1.2F,
                               0.9F, 1.2F, {72, 66, 58, 255});
            }
            if (cannonLoaded(snapshot, building)) {
                const Vec3 socket = catapultMuzzleWorldPosition(
                    {}, defensiveYaw, cannonPitch(snapshot, building, interpolationAlpha));
                const auto* character = playerClassDefinition(snapshot.playerClass);
                const double classDamage = character &&
                    character->startingBuilding == BuildingType::Catapult
                        ? character->startingBuildingDamageMultiplier : 1.0;
                const auto appearance = presentation::bulletAppearance(
                    CannonSystem::explosionDamage(BuildingType::Catapult, building.level) *
                    buildingAnvilMultiplier(building) *
                    std::max(0.05, snapshot.upgradeStats.defenseDamage) *
                    std::max(0.05, classDamage));
                WorldMaterialState ballMaterial{};
                ballMaterial.unlitAmount = 1.0F;
                ballMaterial.screenAoAmount = 0.0F;
                ballMaterial.inkOutlineEligible = false;
                ballMaterial.inkOccluder = true;
                renderer_->setWorldMaterial(ballMaterial);
                rlDisableBackfaceCulling();
                rlBegin(RL_TRIANGLES);
                emitCannonballGeometry(
                    scaledPosition(static_cast<float>(socket.x),
                                   static_cast<float>(socket.y), static_cast<float>(socket.z)),
                    Vector3Normalize({-std::sin(defensiveYaw), 0.5F, -std::cos(defensiveYaw)}),
                    camera.position, 0.24F * spawnScale,
                    {appearance.color.r, appearance.color.g, appearance.color.b, 255});
                rlEnd();
                rlDrawRenderBatchActive();
                rlEnableBackfaceCulling();
                renderer_->setWorldMaterial(material);
            }
        } else if (building.type == BuildingType::SlowTrap) {
            drawScaledCube(0.0F, 0.08F, 0.0F, 1.0F, 0.16F,
                           1.0F, {76, 110, 132, 255});
        } else if (building.type == BuildingType::SpikeTrap) {
            constexpr float QuarterTurn = PI * 0.5F;
            if (!renderer_->drawSpikeTrap(
                    {x, groundY, z},
                    static_cast<float>(building.rotation) *
                        QuarterTurn,
                    spikeTrapAnimationSeconds(
                        snapshot, building.id),
                    WHITE, spawnScale, building.level)) {
                drawScaledCube(
                    0.0F, 0.08F, 0.0F, 1.0F, 0.16F,
                    1.0F, {112, 96, 80, 255});
            }
        } else if (
            building.type == BuildingType::WoodStorage ||
            building.type == BuildingType::StoneStorage ||
            building.type == BuildingType::CrystalStorage) {
            const Color storageColor =
                building.type == BuildingType::WoodStorage
                    ? Color{142, 91, 48, 255}
                    : building.type == BuildingType::StoneStorage
                        ? Color{104, 112, 122, 255}
                        : Color{92, 104, 184, 255};
            drawScaledCube(0.0F, 0.75F, 0.0F, 1.8F,
                           1.5F, 1.8F, storageColor);
            drawScaledCube(0.0F, 1.62F, 0.0F, 1.25F,
                           0.24F, 1.25F,
                           {224, 195, 123, 255});
        } else if (building.type == BuildingType::Gate &&
                   !renderer_->drawGate(
                       {x, groundY, z},
                       static_cast<float>(building.rotation) *
                           PI * 0.5F,
                       gateOpenAmount(building.id, building.open),
                       WHITE, spawnScale, building.level)) {
            if ((building.rotation % 2U) == 0U) {
                drawScaledCube(-0.38F, 1.0F, 0.0F, 0.22F,
                               2.0F, 1.0F,
                               {112, 76, 48, 255});
                drawScaledCube(0.38F, 1.0F, 0.0F, 0.22F,
                               2.0F, 1.0F,
                               {112, 76, 48, 255});
                if (!building.open) {
                    drawScaledCube(
                        0.0F, 1.0F, 0.0F, 0.55F, 1.7F,
                        0.18F, {151, 105, 62, 255});
                }
            } else {
                drawScaledCube(0.0F, 1.0F, -0.38F, 1.0F,
                               2.0F, 0.22F,
                               {112, 76, 48, 255});
                drawScaledCube(0.0F, 1.0F, 0.38F, 1.0F,
                               2.0F, 0.22F,
                               {112, 76, 48, 255});
                if (!building.open) {
                    drawScaledCube(
                        0.0F, 1.0F, 0.0F, 0.18F, 1.7F,
                        0.55F, {151, 105, 62, 255});
                }
            }
        }
    }
    drawSoldBuildingVisuals();
    renderer_->setWorldMaterial({});
    if (std::ranges::any_of(
            snapshot.attackPortalPositions,
            [](const auto& position) { return position.has_value(); })) {
        Vec3 portalTarget{};
        if (const auto core = std::ranges::find_if(
                snapshot.buildings,
                [](const BuildingInstance& building) {
                    return building.type == BuildingType::Core;
                }); core != snapshot.buildings.end()) {
            portalTarget = buildingWorldPosition(*core);
        }
        renderer_->endWorldShader();
        const float portalTime = static_cast<float>(snapshot.elapsedSeconds);
        for (std::size_t index = 0;
             index < snapshot.attackPortalPositions.size(); ++index) {
            const auto& portal = snapshot.attackPortalPositions[index];
            if (!portal || !worldObjectVisible(
                    *portal, 3.2F, worldDrawDistance)) {
                continue;
            }
            const float x = static_cast<float>(portal->x);
            const float z = static_cast<float>(portal->z);
            const Vector3 position{
                x,
                static_cast<float>(
                    simulation_.terrain().getHeight(x, z)),
                z,
            };
            const float yaw = std::atan2(
                static_cast<float>(portalTarget.x - portal->x),
                static_cast<float>(portalTarget.z - portal->z)) -
                PI * 0.5F;
            constexpr float PortalScale = 0.72F;
            static_cast<void>(renderer_->drawEnemyPortal(
                position, yaw,
                portalTime * 0.32F + static_cast<float>(index) * 0.83F,
                lighting, PortalScale));
        }
        renderer_->beginWorldShader(lighting);
        renderer_->setWorldMaterial({});
    }
    renderer_->endWorldShader();
    const auto grassStart = PerformanceClock::now();
    renderer_->drawGrassInstances(
        camera.position, static_cast<float>(snapshot.worldLimit),
        nightAmount, lighting, clearAreas);
    performanceStats_.grassRender.sample(
        performanceMilliseconds(grassStart));
}

void App::drawWorldLootItems(
    const SimulationSnapshot& snapshot, const Camera3D& camera) {
    const auto horizontalView = camera_culling::horizontalView(camera);
    const float worldDrawDistance = static_cast<float>(
        simulation_.terrain().config().terrainRenderDistance + 20.0);
    // Loot is deliberately rendered without the lit world shader so its
    // rarity color, glow and permanent silhouette stay vivid at night.
    for (const LootChestInstance& chest : snapshot.lootChests) {
        if (chest.loot.revealProgress <= 0.0 ||
            chest.loot.collected ||
            !camera_culling::visibleInHorizontalRange(
                static_cast<float>(chest.position.x) - camera.position.x,
                static_cast<float>(chest.position.z) - camera.position.z,
                horizontalView, worldDrawDistance + 2.0F, 2.0F)) {
            continue;
        }
        const LootItemVisual visual = lootItemVisual(snapshot, chest);
        const Vector3 surfaceNormal{
            static_cast<float>(chest.surfaceNormal.x),
            static_cast<float>(chest.surfaceNormal.y),
            static_cast<float>(chest.surfaceNormal.z),
        };
        if (const auto target =
                rerollTargetLootItemVisual(snapshot, chest)) {
            rlDrawRenderBatchActive();
            BeginBlendMode(BLEND_ALPHA);
            rlDisableDepthMask();
            renderer_->drawLootItem(
                visual.position, chest.loot.effect, chest.loot.rarity,
                visual.rotation, visual.tint, visual.scale,
                surfaceNormal);
            renderer_->drawLootItem(
                target->position, chest.rerollTargetEffect,
                chest.rerollTargetRarity, target->rotation,
                target->tint, target->scale, surfaceNormal);
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
            EndBlendMode();
        } else {
            renderer_->drawLootItem(
                visual.position, chest.loot.effect, chest.loot.rarity,
                visual.rotation, visual.tint, visual.scale,
                surfaceNormal);
        }
    }
}

void App::drawWorldProjectiles(
    const SimulationSnapshot& snapshot, const Camera3D& camera,
    const WorldLighting& lighting, float interpolationAlpha) {
    WorldObjectProfiler objectProfiler(
        performanceStats_.worldObjectTimings,
        renderer_->settings().performanceLogging);
    objectProfiler.next(WorldObjectStage::ProjectilesPortals);
    renderer_->beginWorldShader(lighting);
    renderer_->setWorldMaterial({});
    const float projectileTime = static_cast<float>(GetTime());
    for (const auto& projectile : snapshot.iceWandProjectiles) {
        renderer_->drawIceWandProjectile(
            projectile, camera.position, projectileTime,
            interpolationAlpha);
    }
    for (const auto& projectile : snapshot.fireWandProjectiles) {
        renderer_->drawIceWandProjectile(
            projectile, camera.position, projectileTime,
            interpolationAlpha);
    }
    // A bounded ribbon follows the shell's recorded trajectory. All ballistic
    // tails share a batch, with no particles or per-frame allocations.
    WorldMaterialState cannonShotMaterial{};
    cannonShotMaterial.unlitAmount = 1.0F;
    cannonShotMaterial.screenAoAmount = 0.0F;
    cannonShotMaterial.inkOutlineEligible = false;
    renderer_->setWorldMaterial(cannonShotMaterial);
    renderer_->beginColorOnlyWorldEffects();
    beginProjectileBatch(true);
    for (const auto& projectile : snapshot.cannonProjectiles) {
        if (!projectile.active) {
            continue;
        }
        const auto appearance = presentation::bulletAppearance(
            projectile.explosionDamage);
        const Color tint{appearance.color.r, appearance.color.g,
                         appearance.color.b, 255};
        const float scale = static_cast<float>(projectile.visualScale);
        const auto trailPoint = [&projectile, interpolationAlpha](std::size_t age) {
            return ballisticTrailPoint(projectile, age, interpolationAlpha);
        };
        const auto trailSide = [&trailPoint, &projectile, &camera](std::size_t age) {
            const Vector3 point = trailPoint(age);
            const Vector3 axis = Vector3Subtract(
                trailPoint(age > 0U ? age - 1U : age),
                trailPoint(std::min(age + 1U, projectile.trailPointCount - 1U)));
            Vector3 side = Vector3CrossProduct(
                axis, Vector3Subtract(camera.position, point));
            if (Vector3LengthSqr(side) <= 0.000001F) {
                side = Vector3CrossProduct(axis, {0.0F, 1.0F, 0.0F});
            }
            return Vector3LengthSqr(side) > 0.000001F
                ? Vector3Normalize(side) : Vector3{1.0F, 0.0F, 0.0F};
        };
        for (std::size_t age = 1; age < projectile.trailPointCount; ++age) {
            const Vector3 near = trailPoint(age - 1U);
            const Vector3 far = trailPoint(age);
            if (Vector3DistanceSqr(near, far) <= 0.000001F) continue;
            const float segments = static_cast<float>(projectile.trailPointCount - 1U);
            const float nearWeight = 1.0F - static_cast<float>(age - 1U) / segments;
            const float farWeight = 1.0F - static_cast<float>(age) / segments;
            const auto trailColor = [tint](float weight) {
                Color color = ColorLerp({112, 113, 112, 255}, tint, weight * weight);
                color.a = static_cast<unsigned char>(160.0F * weight * weight);
                return color;
            };
            const Vector3 nearSide = Vector3Scale(trailSide(age - 1U),
                0.24F * scale * (0.72F + 0.28F * nearWeight));
            const Vector3 farSide = Vector3Scale(trailSide(age),
                0.24F * scale * (0.72F + 0.28F * farWeight));
            const Color nearColor = trailColor(nearWeight);
            const Color farColor = trailColor(farWeight);
            emitProjectileQuad(
                Vector3Add(far, farSide), Vector3Add(near, nearSide),
                Vector3Subtract(near, nearSide), Vector3Subtract(far, farSide),
                farColor, nearColor, nearColor, farColor);
        }
    }
    endProjectileBatch();
    renderer_->endColorOnlyWorldEffects();

    // The shaded round body masks outlines behind it, just like turret shots.
    cannonShotMaterial.inkOccluder = true;
    renderer_->setWorldMaterial(cannonShotMaterial);
    beginProjectileBatch(false);
    for (const auto& projectile : snapshot.cannonProjectiles) {
        if (!projectile.active) continue;
        const auto appearance = presentation::bulletAppearance(projectile.explosionDamage);
        const Vector3 velocity{static_cast<float>(projectile.velocity.x),
                               static_cast<float>(projectile.velocity.y),
                               static_cast<float>(projectile.velocity.z)};
        emitCannonballGeometry(
            ballisticTrailPoint(projectile, 0U, interpolationAlpha),
            Vector3LengthSqr(velocity) > 0.000001F
                ? Vector3Normalize(velocity) : Vector3{0.0F, 1.0F, 0.0F},
            camera.position, 0.24F * static_cast<float>(projectile.visualScale),
            {appearance.color.r, appearance.color.g, appearance.color.b, 255});
    }
    endProjectileBatch();
    renderer_->setWorldMaterial({});
    // Trails affect scene color; solid bullets and pointed bolts also mask
    // outlines from geometry behind them. Both use the same rlgl batches.
    WorldMaterialState turretShotMaterial{};
    turretShotMaterial.unlitAmount = 1.0F;
    turretShotMaterial.screenAoAmount = 0.0F;
    turretShotMaterial.inkOutlineEligible = false;
    renderer_->setWorldMaterial(turretShotMaterial);
    const auto drawDefenseShots = [this, &camera](bool drawTail) {
        beginProjectileBatch(drawTail);
        for (const auto& arrow : arrowVisuals_) {
            if (arrow.startDelayRemaining > 0.0) continue;
            const float progress = std::clamp(static_cast<float>(
                1.0 - arrow.remaining / arrow.duration), 0.0F, 1.0F);
            const Vector3 origin{
                static_cast<float>(arrow.origin.x),
                static_cast<float>(arrow.origin.y),
                static_cast<float>(arrow.origin.z),
            };
            const Vector3 target{
                static_cast<float>(arrow.target.x),
                static_cast<float>(arrow.target.y),
                static_cast<float>(arrow.target.z),
            };
            const Vector3 path = Vector3Subtract(target, origin);
            const float distance = Vector3Length(path);
            if (distance <= 0.0001F) continue;
            const Vector3 direction = Vector3Scale(path, 1.0F / distance);
            const float visualScale = arrow.bulletAppearance.scale *
                (arrow.shrapnel ? 0.56F : 1.0F);
            // Keep the visible front behind contact until the flight ends.
            const float launchLead = std::min(
                distance * 0.25F, 0.41F * visualScale);
            const float frontDistance = launchLead +
                (distance - launchLead) * progress;
            const Vector3 head = Vector3Add(origin,
                Vector3Scale(direction, frontDistance));
            Color shotColor{arrow.bulletAppearance.color.r,
                arrow.bulletAppearance.color.g, arrow.bulletAppearance.color.b, 255};
            if (!arrow.turretBullet) {
                shotColor = ColorLerp(shotColor, {132, 210, 255, 255}, 0.65F);
            }
            emitDefenseShotGeometry(
                head, direction, camera.position, frontDistance,
                visualScale, 1.0F, shotColor, drawTail, !arrow.turretBullet);
        }
        endProjectileBatch();
    };
    renderer_->beginColorOnlyWorldEffects();
    drawDefenseShots(true);
    renderer_->endColorOnlyWorldEffects();
    turretShotMaterial.inkOccluder = true;
    renderer_->setWorldMaterial(turretShotMaterial);
    drawDefenseShots(false);

    renderer_->beginColorOnlyWorldEffects();
    beginProjectileBatch(true, BLEND_ADDITIVE);
    for (const auto& arrow : arrowVisuals_) {
        if (!arrow.turretBullet || arrow.shrapnel ||
            arrow.startDelayRemaining > 0.0) continue;
        const float elapsed = static_cast<float>(
            arrow.duration - arrow.remaining);
        constexpr float FlashDuration = 0.10F;
        if (elapsed >= FlashDuration) continue;
        const Vector3 origin{
            static_cast<float>(arrow.origin.x),
            static_cast<float>(arrow.origin.y),
            static_cast<float>(arrow.origin.z),
        };
        Vector3 direction{
            static_cast<float>(arrow.target.x - arrow.origin.x),
            static_cast<float>(arrow.target.y - arrow.origin.y),
            static_cast<float>(arrow.target.z - arrow.origin.z),
        };
        if (Vector3LengthSqr(direction) <= 0.000001F) continue;
        direction = Vector3Normalize(direction);
        const float progress = std::clamp(
            elapsed / FlashDuration, 0.0F, 1.0F);
        const float fade = 1.0F - progress * progress;
        emitTurretMuzzleFlash(origin, direction, camera, fade);
    }
    endProjectileBatch();
    renderer_->endColorOnlyWorldEffects();
    renderer_->setWorldMaterial({});

    for (const auto& projectile : snapshot.bombProjectiles) {
        if (projectile.active) {
            const Vector3 position{
                static_cast<float>(projectile.position.x),
                static_cast<float>(projectile.position.y),
                static_cast<float>(projectile.position.z),
            };
            DrawSphereEx(position, 0.255F, 8, 8, {43, 47, 52, 255});
            const Matrix bombRotation = MatrixRotateXYZ({
                static_cast<float>(projectile.rotation.x),
                static_cast<float>(projectile.rotation.y),
                static_cast<float>(projectile.rotation.z),
            });
            const Vector3 fuseBase = Vector3Add(position, Vector3Transform(
                {0.0F, 0.21F, 0.0F}, bombRotation));
            const Vector3 fuseTip = Vector3Add(position, Vector3Transform(
                {0.0F, 0.34F, 0.0F}, bombRotation));
            DrawCylinderEx(fuseBase, fuseTip, 0.045F, 0.032F, 6,
                           {117, 91, 55, 255});
            DrawSphereEx(fuseBase, 0.048F, 5, 5, {117, 91, 55, 255});
            if (renderer_->settings().particles) {
                const float time = static_cast<float>(GetTime());
                const float urgency = 1.0F - std::clamp(
                    static_cast<float>(projectile.fuseRemaining /
                                       std::max(projectile.fuseDuration, 0.01)),
                    0.0F, 1.0F);
                const float blink = 0.55F + 0.45F * std::sin(
                    time * (10.0F + urgency * 28.0F) +
                    static_cast<float>(projectile.id.index));
                const Vector3 fuse = fuseTip;
                rlDrawRenderBatchActive();
                rlDisableDepthMask();
                BeginBlendMode(BLEND_ADDITIVE);
                DrawSphereEx(fuse, 0.045F + urgency * 0.025F, 5, 5,
                             {255, 238, 164,
                              static_cast<unsigned char>(190.0F + blink * 65.0F)});
                DrawSphereEx(fuse, 0.11F + blink * 0.04F, 5, 5,
                             {255, 72, 12,
                              static_cast<unsigned char>(45.0F + blink * 55.0F)});
                for (int spark = 0; spark < 5; ++spark) {
                    const float phase = time * (4.0F + urgency * 5.0F) +
                        static_cast<float>(spark) * 1.256637F;
                    const float travel = std::fmod(
                        time * 2.1F + static_cast<float>(spark) * 0.23F, 1.0F);
                    const Vector3 tip{
                        fuse.x + std::cos(phase) * travel * 0.18F,
                        fuse.y + travel * 0.22F - travel * travel * 0.18F,
                        fuse.z + std::sin(phase) * travel * 0.18F,
                    };
                    DrawLine3D(fuse, tip, {255, 174, 46,
                                          static_cast<unsigned char>((1.0F - travel) * 220.0F)});
                }
                EndBlendMode();

                const Vector3 velocity{
                    static_cast<float>(projectile.velocity.x),
                    static_cast<float>(projectile.velocity.y),
                    static_cast<float>(projectile.velocity.z),
                };
                const float speed = Vector3Length(velocity);
                const Vector3 backward = speed > 0.01F
                    ? Vector3Scale(velocity, -1.0F / speed)
                    : Vector3{0.0F, 0.0F, 0.0F};
                for (int mote = 0; mote < 6; ++mote) {
                    const float age = static_cast<float>(mote + 1) / 6.0F;
                    const float wobble = std::sin(
                        time * 5.0F + static_cast<float>(mote) * 2.17F) * 0.035F;
                    DrawSphereEx(
                        {position.x + backward.x * age * 0.58F + wobble,
                         position.y + backward.y * age * 0.58F + age * 0.08F,
                         position.z + backward.z * age * 0.58F - wobble},
                        0.035F + age * 0.035F, 4, 4,
                        {65, 61, 58,
                         static_cast<unsigned char>((1.0F - age) * 125.0F)});
                }
                rlDrawRenderBatchActive();
                rlEnableDepthMask();
            }
        }
    }
    renderer_->endWorldShader();
}

} // namespace ian
