#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "buildings/BuildingOrientation.hpp"
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
    std::vector<GrassClearArea>& result =
        activeDecorationClearAreaBuffer_;
    result.clear();
    result.reserve(
        grassClearAreas_.size() +
        modularPlatformDragPreviews_.size() +
        modularWallDragPreviews_.size() +
        modularRampDragPreviews_.size() + 16U);
    result.insert(
        result.end(), grassClearAreas_.begin(), grassClearAreas_.end());
    const double cellSize =
        simulation_.terrain().config().cellSize;
    const auto add = [&result](
                         double x, double z,
                         double radius) {
        result.push_back({
            .center = {
                static_cast<float>(x),
                static_cast<float>(z),
            },
            .innerRadius =
                static_cast<float>(radius),
            .amount = 1.0F,
        });
    };
    const auto addPlatform = [&](const auto& placement) {
        add(
            (placement.anchor.x + 1.0) * cellSize,
            (placement.anchor.z + 1.0) * cellSize,
            cellSize * 1.35);
    };
    const auto addWall = [&](const auto& placement) {
        add(
            (placement.anchor.x + 0.5) * cellSize,
            (placement.anchor.z + 0.5) * cellSize,
            cellSize * 0.72);
    };
    const auto addRamp = [&](const auto& placement) {
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
            (placement.anchor.x + widthCells * 0.5) *
                cellSize,
            (placement.anchor.z + depthCells * 0.5) *
                cellSize,
            cellSize * 0.5 *
                std::hypot(widthCells, depthCells));
    };

    if (platformFramePreview_ &&
        !modularDragPiece_) {
        addPlatform(*platformFramePreview_);
    }
    for (const auto& placement :
         modularPlatformDragPreviews_) {
        addPlatform(placement);
    }
    if (wallPreview_ && !modularDragPiece_) {
        addWall(*wallPreview_);
    }
    for (const auto& placement :
         modularWallDragPreviews_) {
        addWall(placement);
    }
    if (rampPreview_ && !modularDragPiece_) {
        addRamp(*rampPreview_);
    }
    for (const auto& placement :
         modularRampDragPreviews_) {
        addRamp(placement);
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
            add(center.x, center.z, radius);
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
    WorldMaterialState obstacleMaterial{};
    obstacleMaterial.bakedAo = 0.74F;
    renderer_->setWorldMaterial(obstacleMaterial);
    for (const auto& obstacle : snapshot.mapObstacles) {
        const float width =
            static_cast<float>(obstacle.collision.maxX - obstacle.collision.minX);
        const float depth =
            static_cast<float>(obstacle.collision.maxZ - obstacle.collision.minZ);
        const Vector3 center{
            static_cast<float>((obstacle.collision.minX + obstacle.collision.maxX) * 0.5),
            static_cast<float>(obstacle.height * 0.5),
            static_cast<float>((obstacle.collision.minZ + obstacle.collision.maxZ) * 0.5),
        };
        if (!worldObjectVisible(
                {center.x, center.y, center.z},
                0.5F * std::hypot(width, depth),
                worldDrawDistance)) {
            continue;
        }
        DrawCube(center, width, static_cast<float>(obstacle.height), depth,
                 {99, 111, 122, 255});
    }
    WorldMaterialState boundaryForestMaterial{};
    boundaryForestMaterial.baseColor = {
        0.45F, 0.53F, 0.43F, 1.0F};
    boundaryForestMaterial.bakedAo = 0.72F;
    boundaryForestMaterial.screenAoAmount = 0.0F;
    boundaryForestMaterial.windAmount = 0.18F;
    boundaryForestMaterial.distantFadeAmount = 1.0F;
    boundaryForestMaterial.vegetationAmount = 1.0F;
    renderer_->setWorldMaterial(boundaryForestMaterial);
    renderer_->drawBoundaryForest(camera);
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
        if (!node.active) {
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
            material.distantFadeAmount = 1.0F;
            material.vegetationAmount = 1.0F;
        }
        material.hitFlashAmount =
            presentation::resourceHitFlash(
                effects_, node.id);
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
        treeMaterial.distantFadeAmount = 1.0F;
        treeMaterial.vegetationAmount = 1.0F;
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
    // Loot is deliberately rendered without the lit world shader so its
    // rarity color, glow and permanent silhouette stay vivid at night.
    renderer_->endWorldShader();
    for (const LootChestInstance& chest : snapshot.lootChests) {
        if (chest.loot.revealProgress <= 0.0 ||
            chest.loot.collected ||
            !worldObjectVisible(
                chest.position, 2.0F, worldDrawDistance)) {
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
    for (const CoinPickup& coin : snapshot.coinPickups) {
        if (!worldObjectVisible(coin.position, 1.0F, 60.0F)) {
            continue;
        }
        const float spawn = std::clamp(
            static_cast<float>(coin.age / 0.14), 0.0F, 1.0F);
        const float pop =
            spawn < 1.0F
                ? spawn + std::sin(spawn * PI) * 0.22F
                : 1.0F;
        const float magnetStretch = coin.magnetized
            ? 1.0F + std::min(
                  0.18F,
                  static_cast<float>(coin.magnetTime) * 0.55F)
            : 1.0F;
        const Vector3 pickupPosition{
            static_cast<float>(coin.position.x),
            static_cast<float>(coin.position.y),
            static_cast<float>(coin.position.z)};
        const float rotation = static_cast<float>(
            coin.spinPhase + coin.age * 8.5);
        if (coin.kind == PickupKind::Heart) {
            renderer_->drawHeart(
                pickupPosition, rotation, pop * magnetStretch);
        } else {
            renderer_->drawCoin(
                coin.type, pickupPosition, rotation,
                pop * magnetStretch);
        }
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
            defensiveYaw = towerYaw(snapshot, building);
        } else if (building.type == BuildingType::Cannon ||
                   building.type == BuildingType::Catapult) {
            defensiveYaw = cannonYaw(snapshot, building);
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
                    WHITE, spawnScale)) {
                drawScaledCube(0.0F, 1.25F, 0.0F, 2.0F,
                               2.5F, 2.0F,
                               {219, 151, 60, 255});
            }
            if (nightAmount > 0.0F) {
                const unsigned char alpha =
                    static_cast<unsigned char>(80.0F + 120.0F * nightAmount);
                DrawSphere(
                    scaledPosition(0.0F, 2.35F, 0.0F),
                    0.22F * spawnScale,
                    {255, 204, 91, alpha});
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
                    WHITE, spawnScale)) {
                drawScaledCube(0.0F, 1.0F, 0.0F, 1.0F,
                               2.0F, 1.0F,
                               {126, 86, 54, 255});
            }
        } else if (building.type == BuildingType::Turret) {
            if (!renderer_->drawCrossbow(
                    {x, groundY, z},
                    defensiveYaw, WHITE,
                    spawnScale,
                    towerPitch(snapshot, building))) {
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
                    shotRecoil)) {
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
                            building.id))) {
                drawScaledCube(0.0F, 0.275F, 0.0F, 1.0F,
                               0.55F, 1.0F,
                               {82, 101, 142, 255});
            }
        } else if (building.type == BuildingType::Cannon) {
            if (!renderer_->drawCannon({x, groundY, z},
                                       defensiveYaw,
                                       cannonPitch(snapshot, building),
                                       WHITE, spawnScale)) {
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
                    cannonPitch(snapshot, building),
                    cannonLoaded(snapshot, building), WHITE,
                    spawnScale)) {
                drawScaledCube(0.0F, 0.45F, 0.0F, 1.2F,
                               0.9F, 1.2F, {72, 66, 58, 255});
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
                    WHITE, spawnScale)) {
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
        } else {
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
    for (const auto& projectile : snapshot.cannonProjectiles) {
        if (projectile.active) {
            const Vector3 projectilePosition{
                static_cast<float>(projectile.position.x),
                static_cast<float>(projectile.position.y),
                static_cast<float>(projectile.position.z),
            };
            const bool drawn = projectile.type == BuildingType::Catapult
                ? renderer_->drawCatapultBall(projectilePosition)
                : renderer_->drawCannonball(projectilePosition);
            if (!drawn) {
                DrawSphere(projectilePosition, 0.2F,
                           {36, 39, 43, 255});
            }
        }
    }
    for (const auto& arrow : arrowVisuals_) {
        const double progress =
            1.0 - arrow.remaining / arrow.duration;
        const Vec3 arrowPosition{
            arrow.origin.x +
                (arrow.target.x - arrow.origin.x) * progress,
            arrow.origin.y +
                (arrow.target.y - arrow.origin.y) * progress,
            arrow.origin.z +
                (arrow.target.z - arrow.origin.z) * progress,
        };
        const Vector3 projectilePosition{
            static_cast<float>(arrowPosition.x),
            static_cast<float>(arrowPosition.y),
            static_cast<float>(arrowPosition.z)};
        const Vector3 projectileDirection{
            static_cast<float>(arrow.direction.x),
            static_cast<float>(arrow.direction.y),
            static_cast<float>(arrow.direction.z)};
        if (arrow.turretBullet) {
            (void)renderer_->drawTurretBullet(
                projectilePosition, projectileDirection);
        } else {
            (void)renderer_->drawArrow(
                projectilePosition, projectileDirection);
        }
    }
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
            }
        }
    }
    drawWorldEnemies(snapshot, camera);
    renderer_->endWorldShader();
    const auto grassStart = PerformanceClock::now();
    renderer_->drawGrassInstances(
        camera.position, static_cast<float>(snapshot.worldLimit),
        nightAmount, lighting, clearAreas);
    performanceStats_.grassRender.sample(
        performanceMilliseconds(grassStart));
}

} // namespace ian
