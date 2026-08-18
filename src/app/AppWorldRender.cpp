#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "game/ChallengeArena.hpp"
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

void emitFlameVertex(Vector3 position, Color color) {
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlVertex3f(position.x, position.y, position.z);
}

void emitFlameTriangle(
    Vector3 first, Color firstColor,
    Vector3 second, Color secondColor,
    Vector3 third, Color thirdColor) {
    emitFlameVertex(first, firstColor);
    emitFlameVertex(second, secondColor);
    emitFlameVertex(third, thirdColor);
}

void emitFlameLobe(
    Vector3 base, Vector3 cameraRight, float width,
    float height, float sway, float amount) {
    const Vector3 left = Vector3Add(
        base, Vector3Scale(cameraRight, -width));
    const Vector3 right = Vector3Add(
        base, Vector3Scale(cameraRight, width));
    Vector3 middle = base;
    middle.y += height * 0.52F;
    middle = Vector3Add(
        middle, Vector3Scale(cameraRight, sway * 0.34F));
    const Vector3 middleLeft = Vector3Add(
        middle, Vector3Scale(cameraRight, -width * 0.58F));
    const Vector3 middleRight = Vector3Add(
        middle, Vector3Scale(cameraRight, width * 0.58F));
    Vector3 tip = base;
    tip.y += height;
    tip = Vector3Add(tip, Vector3Scale(cameraRight, sway));
    const auto alpha = [amount](float multiplier) {
        return static_cast<unsigned char>(std::lround(
            255.0F * std::clamp(amount * multiplier, 0.0F, 1.0F)));
    };
    const Color hot{255, 250, 194, alpha(1.0F)};
    const Color orange{255, 116, 12, alpha(1.0F)};
    const Color ember{255, 42, 3, alpha(0.72F)};
    const Color clear{170, 20, 4, 0};
    emitFlameTriangle(left, hot, right, hot, middleRight, orange);
    emitFlameTriangle(left, hot, middleRight, orange, middleLeft, orange);
    emitFlameTriangle(
        middleLeft, orange, middleRight, ember, tip, clear);
    const Vector3 innerLeft = Vector3Add(
        base, Vector3Scale(cameraRight, -width * 0.43F));
    const Vector3 innerRight = Vector3Add(
        base, Vector3Scale(cameraRight, width * 0.43F));
    Vector3 innerTip = base;
    innerTip.y += height * 0.72F;
    innerTip = Vector3Add(
        innerTip, Vector3Scale(cameraRight, sway * 0.48F));
    emitFlameTriangle(
        innerLeft, {255, 255, 226, alpha(1.0F)},
        innerRight, {255, 242, 128, alpha(1.0F)},
        innerTip, {255, 142, 18, alpha(0.18F)});
}

void emitEmber(
    Vector3 center, Vector3 cameraRight,
    float size, float amount) {
    const Vector3 left = Vector3Add(
        center, Vector3Scale(cameraRight, -size));
    const Vector3 right = Vector3Add(
        center, Vector3Scale(cameraRight, size));
    Vector3 bottom = center;
    bottom.y -= size * 1.55F;
    Vector3 top = center;
    top.y += size * 1.55F;
    const auto alpha = static_cast<unsigned char>(std::lround(
        255.0F * std::clamp(amount, 0.0F, 1.0F)));
    const Color core{255, 246, 183, alpha};
    const Color edge{255, 94, 12,
                     static_cast<unsigned char>(alpha * 0.72F)};
    const Color clear{190, 24, 2, 0};
    emitFlameTriangle(bottom, clear, right, edge, top, core);
    emitFlameTriangle(bottom, clear, top, core, left, edge);
}

float enemyFlameScale(EnemyType type) {
    switch (type) {
    case EnemyType::Boss: return 1.65F;
    case EnemyType::Heavy: return 1.25F;
    case EnemyType::Splitter: return 1.35F;
    case EnemyType::Splitling: return 0.62F;
    case EnemyType::Fast: return 0.82F;
    case EnemyType::Flying: return 0.78F;
    case EnemyType::Basic:
    case EnemyType::Ranged:
    case EnemyType::Sapper:
        return 1.0F;
    }
    return 1.0F;
}

} // namespace

std::vector<GrassClearArea>
App::activeDecorationClearAreas(
    const SimulationSnapshot& snapshot) const {
    std::vector<GrassClearArea> result =
        grassClearAreas_;
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
    return result;
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
    renderer_->drawBoundaryForest();
    WorldMaterialState decorativeRockMaterial{};
    decorativeRockMaterial.bakedAo = 0.82F;
    decorativeRockMaterial.screenAoAmount = 0.0F;
    renderer_->setWorldMaterial(decorativeRockMaterial);
    renderer_->drawDecorativeRocks(
        camera.position,
        static_cast<float>(snapshot.worldLimit),
        clearAreas);
    performanceStats_.decorationsRender.sample(
        performanceMilliseconds(decorationsStart));
    const float resourceDrawDistance = static_cast<float>(
        simulation_.terrain().config().terrainRenderDistance +
        12.0);
    const float resourceDrawDistanceSquared =
        resourceDrawDistance * resourceDrawDistance;
    resourceTreeDrawInstances_.clear();
    resourceTreeDrawInstances_.reserve(snapshot.resourceNodes.size());
    resourceRockDrawInstances_.clear();
    resourceRockDrawInstances_.reserve(snapshot.resourceNodes.size());
    for (const auto& node : snapshot.resourceNodes) {
        if (!node.active) {
            continue;
        }
        const float cameraOffsetX =
            static_cast<float>(node.position.x) - camera.position.x;
        const float cameraOffsetZ =
            static_cast<float>(node.position.z) - camera.position.z;
        if (cameraOffsetX * cameraOffsetX +
                cameraOffsetZ * cameraOffsetZ >
            resourceDrawDistanceSquared) {
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
        if (chest.looseLoot) continue;
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
            chest.loot.collected) {
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
        const float deltaX =
            static_cast<float>(coin.position.x) - camera.position.x;
        const float deltaZ =
            static_cast<float>(coin.position.z) - camera.position.z;
        if (deltaX * deltaX + deltaZ * deltaZ > 3600.0F) {
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
            },
            {});
    }
    for (const auto& building : snapshot.buildings) {
        const Vec3 center = buildingWorldPosition(building);
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
            if (!renderer_->drawWall(
                    {x, groundY, z}, connections,
                    static_cast<float>(
                        wallFallbackRotation(
                            snapshot.buildings,
                            building)) *
                        PI * 0.5F,
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
    const auto enemyRenderStart = PerformanceClock::now();
    performanceStats_.visibleEnemies = 0U;
    enemyDrawInstances_.clear();
    enemyDrawInstances_.reserve(snapshot.enemies.size());
    const Vector3 cameraForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    constexpr float EnemyFullDetailDistance = 20.0F;
    constexpr float EnemyFullDetailDistanceSquared =
        EnemyFullDetailDistance * EnemyFullDetailDistance;
    enemyHitFlashById_.clear();
    enemyHitFlashById_.reserve(effects_.size());
    enemyBurnAmountById_.clear();
    enemyBurnAmountById_.reserve(effects_.size());
    const auto effectKey = [](EntityId id) {
        return
            (static_cast<std::uint64_t>(id.generation) << 32U) |
            static_cast<std::uint64_t>(id.index);
    };
    for (const PresentationEffect& effect : effects_) {
        if (!effect.entityId || effect.duration <= 0.0 ||
            effect.startDelayRemaining > 0.0) {
            continue;
        }
        const std::uint64_t key = effectKey(*effect.entityId);
        const float remainingFraction = std::clamp(
            static_cast<float>(effect.remaining / effect.duration),
            0.0F, 1.0F);
        if (effect.type == PresentationEffectType::EnemyHitImpact) {
            const float flashStrength = std::clamp(
                std::pow(remainingFraction, 0.42F) * effect.scale * 0.48F,
                0.0F, 1.0F);
            auto [entry, inserted] =
                enemyHitFlashById_.try_emplace(
                    key, flashStrength);
            if (!inserted) {
                entry->second = std::max(
                    entry->second, flashStrength);
            }
        } else if (
            effect.type == PresentationEffectType::EnemyBurn) {
            const float fade = std::clamp(
                remainingFraction / 0.28F, 0.0F, 1.0F);
            auto [entry, inserted] =
                enemyBurnAmountById_.try_emplace(key, fade);
            if (!inserted) {
                entry->second = std::max(entry->second, fade);
            }
        }
    }
    for (const auto& enemy : snapshot.enemies) {
        const bool splitting =
            enemy.type == EnemyType::Splitter &&
            enemy.splitAnimationRemaining > 0.0;
        if (!enemy.active && !splitting) {
            continue;
        }
        const Vector3 cullPosition = enemyRenderPosition(enemy);
        const Vector3 cullToEnemy =
            Vector3Subtract(cullPosition, camera.position);
        const float cullDistanceSquared =
            Vector3LengthSqr(cullToEnemy);
        if (cullDistanceSquared > 9.0F) {
            const float inverseDistance =
                1.0F / std::sqrt(cullDistanceSquared);
            const float viewDot = Vector3DotProduct(
                cameraForward,
                Vector3Scale(cullToEnemy, inverseDistance));
            if (viewDot < -0.12F) {
                continue;
            }
        }
        Vector3 enemyPosition = cullPosition;
        enemyPosition.y += static_cast<float>(
            simulation_.terrain().getHeight(
                enemy.position.x,
                enemy.position.z));
        const Vector3 toEnemy =
            Vector3Subtract(enemyPosition, camera.position);
        const float enemyDistanceSquared =
            Vector3LengthSqr(toEnemy);
        ++performanceStats_.visibleEnemies;
        const bool aimed =
            snapshot.aimedEnemy &&
            *snapshot.aimedEnemy == enemy.id;
        const bool lowDetail =
            !aimed && enemy.type != EnemyType::Boss &&
            enemy.eliteAffixes == 0U &&
            enemyDistanceSquared > EnemyFullDetailDistanceSquared;
        const auto flash = enemyHitFlashById_.find(
            effectKey(enemy.id));
        const float hitFlash =
            flash != enemyHitFlashById_.end()
                ? flash->second
                : 0.0F;
        const float enemyScale =
            enemyVisualScale(enemy.type) * enemyHitScale(enemy) *
            (enemy.eliteAffixes != 0U ? 1.08F : 1.0F);
        const EnemyStatusEffect& freezeStatus =
            enemyStatusEffect(enemy, StatusEffectType::Freeze);
        const bool frozen = freezeStatus.remaining > 0.0;
        const bool burning = enemyBurnAmountById_.contains(
            effectKey(enemy.id));
        Color modelTint = WHITE;
        if (splitting) {
            const float progress = std::clamp(
                static_cast<float>(
                    1.0 - enemy.splitAnimationRemaining / 0.38),
                0.0F, 1.0F);
            modelTint = {
                255,
                static_cast<unsigned char>(
                    std::lround(238.0F - progress * 72.0F)),
                static_cast<unsigned char>(
                    std::lround(184.0F - progress * 86.0F)),
                255,
            };
        } else if (frozen) {
            modelTint = {151, 224, 255, 255};
        } else if (burning) {
            modelTint = {255, 118, 42, 255};
        } else if (enemy.slowRemaining > 0.0) {
            modelTint = {184, 222, 255, 255};
        } else if (
            enemy.state == EnemyState::BossRamWindup) {
            modelTint = {255, 178, 150, 255};
        } else if (hasEliteAffix(
                       enemy.eliteAffixes,
                       EliteAffix::Berserker)) {
            const bool enraged = enemy.maxHealth > 0.0 &&
                enemy.health / enemy.maxHealth <= 0.5;
            modelTint = enraged
                ? Color{255, 82, 70, 255}
                : Color{238, 150, 140, 255};
        } else if (hasEliteAffix(
                       enemy.eliteAffixes,
                       EliteAffix::Warden)) {
            modelTint = {124, 195, 255, 255};
        } else if (hasEliteAffix(
                       enemy.eliteAffixes,
                       EliteAffix::Volatile)) {
            modelTint = {255, 181, 78, 255};
        }
        if (!aimed) {
            if (hitFlash > 0.001F) {
                const float smoothFlash =
                    std::clamp(hitFlash, 0.0F, 1.0F);
                const float batchedFlash =
                    std::ceil(smoothFlash * 4.0F) / 4.0F;
                modelTint = {
                    static_cast<unsigned char>(
                        std::lround(
                            static_cast<float>(modelTint.r) +
                            (255.0F -
                             static_cast<float>(
                                 modelTint.r)) *
                                batchedFlash)),
                    static_cast<unsigned char>(
                        std::lround(
                            static_cast<float>(modelTint.g) +
                            (244.0F -
                             static_cast<float>(
                                 modelTint.g)) *
                                batchedFlash)),
                    static_cast<unsigned char>(
                        std::lround(
                            static_cast<float>(modelTint.b) +
                            (205.0F -
                             static_cast<float>(
                                 modelTint.b)) *
                                batchedFlash)),
                    255,
                };
            }
            float animationTime = frozen
                ? 0.0F
                : enemyAnimationSeconds(enemy, snapshot.elapsedSeconds);
            if (enemyDistanceSquared > 625.0F &&
                enemy.hitAnimationRemaining <= 0.0 &&
                enemy.state !=
                    EnemyState::BossRamWindup) {
                animationTime = static_cast<float>(
                    snapshot.elapsedSeconds);
            }
            enemyDrawInstances_.push_back({
                .modelVisual = enemyModelVisual(enemy.type),
                .animationVisual =
                    frozen || splitting
                        ? EnemyAnimationVisual::Idle
                           : enemyAnimationVisual(enemy),
                .animationSeconds = animationTime,
                .position = enemyPosition,
                .yawRadians =
                    static_cast<float>(enemy.yaw),
                .tint = modelTint,
                .scale = enemyScale,
                .lowDetail = lowDetail,
            });
            continue;
        }
        WorldMaterialState material{};
        material.bakedAo = 0.82F;
        material.hitFlashAmount = hitFlash;
        material.selectionAmount = aimed ? 0.32F : 0.0F;
        material.selectionTint = {1.0F, 0.38F, 0.12F};
        renderer_->setWorldMaterial(material);
        float width = 0.8F;
        float height = 1.6F;
        Color body = {150, 55, 52, 255};
        if (enemy.type == EnemyType::Fast) {
            width = 0.65F;
            height = 1.35F;
            body = {191, 104, 52, 255};
        } else if (enemy.type == EnemyType::Heavy) {
            width = 1.15F;
            height = 2.0F;
            body = {93, 60, 105, 255};
            } else if (enemy.type == EnemyType::Boss) {
                width = 2.0F;
                height = 3.2F;
                body = {74, 35, 45, 255};
            } else if (enemy.type == EnemyType::Ranged) {
                width = 0.75F;
                height = 1.55F;
                body = {55, 118, 154, 255};
            } else if (enemy.type == EnemyType::Sapper) {
                width = 0.86F;
                height = 1.5F;
                body = {170, 118, 43, 255};
            } else if (enemy.type == EnemyType::Flying) {
                width = 0.72F;
                height = 1.0F;
                body = {102, 71, 167, 255};
            } else if (enemy.type == EnemyType::Splitter) {
                width = 1.45F;
                height = 2.35F;
                body = {67, 154, 73, 255};
            } else if (enemy.type == EnemyType::Splitling) {
                width = 0.65F;
                height = 1.05F;
                body = {82, 190, 91, 255};
        }
        if (aimed) {
            body = {242, 118, 76, 255};
        } else if (frozen) {
            body = {91, 183, 225, 255};
        } else if (burning) {
            body = {246, 76, 16, 255};
        } else if (enemy.slowRemaining > 0.0) {
            body = {70, 128, 170, 255};
        } else if (enemy.state == EnemyState::BossRamWindup) {
            body = {235, 64, 45, 255};
        }
        if (!renderer_->drawEnemy(
                enemyModelVisual(enemy.type),
                frozen || splitting
                    ? EnemyAnimationVisual::Idle
                       : enemyAnimationVisual(enemy),
                frozen ? 0.0F : enemyAnimationSeconds(
                    enemy, snapshot.elapsedSeconds),
                enemyPosition, static_cast<float>(enemy.yaw),
                modelTint, enemyScale)) {
            const float hitScale = enemyHitScale(enemy);
            DrawCube(enemyPosition, width * hitScale,
                     height * hitScale, width * hitScale,
                     body);
            DrawSphere(
                {enemyPosition.x,
                 enemyPosition.y + height * hitScale * 0.62F,
                 enemyPosition.z},
                width * hitScale * 0.52F,
                aimed ? ORANGE : MAROON);
        }
    }
    if (!enemyDrawInstances_.empty()) {
        WorldMaterialState material{};
        material.bakedAo = 0.82F;
        renderer_->setWorldMaterial(material);
        if (!renderer_->drawEnemiesInstanced(
                enemyDrawInstances_)) {
            for (const EnemyDrawInstance& instance :
                 enemyDrawInstances_) {
                static_cast<void>(renderer_->drawEnemy(
                    instance.modelVisual,
                    instance.animationVisual,
                    instance.animationSeconds,
                    instance.position,
                    instance.yawRadians,
                    instance.tint, instance.scale,
                    instance.loop,
                    instance.inkOutlineEligible));
            }
        }
    }
    BeginBlendMode(BLEND_ADDITIVE);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    for (const EnemyInstance& elite : snapshot.enemies) {
        if (!elite.active || elite.eliteAffixes == 0U) {
            continue;
        }
        Vector3 center = enemyRenderPosition(elite);
        center.y += static_cast<float>(
            simulation_.terrain().getHeight(
                elite.position.x, elite.position.z));
        const float pulse = 0.5F + 0.5F * std::sin(
            static_cast<float>(snapshot.elapsedSeconds) * 5.5F +
            static_cast<float>(elite.id.index % 31U));
        Color aura{255, 128, 68, 125};
        if (hasEliteAffix(
                elite.eliteAffixes, EliteAffix::Warden)) {
            aura = {74, 174, 255, 130};
        } else if (hasEliteAffix(
                       elite.eliteAffixes,
                       EliteAffix::Berserker)) {
            aura = elite.maxHealth > 0.0 &&
                    elite.health / elite.maxHealth <= 0.5
                ? Color{255, 54, 42, 170}
                : Color{255, 112, 82, 110};
        }
        DrawCircle3D(
            {center.x, center.y + 0.025F, center.z},
            0.68F + pulse * 0.11F,
            {1.0F, 0.0F, 0.0F}, 90.0F, aura);
        DrawCircle3D(
            {center.x, center.y + 0.035F, center.z},
            0.42F + pulse * 0.07F,
            {1.0F, 0.0F, 0.0F}, 90.0F,
            {aura.r, aura.g, aura.b,
             static_cast<unsigned char>(aura.a / 2U)});

        if (!hasEliteAffix(
                elite.eliteAffixes, EliteAffix::Warden)) {
            continue;
        }
        int linked = 0;
        for (const EnemyInstance& protectedEnemy :
             snapshot.enemies) {
            if (!protectedEnemy.active ||
                protectedEnemy.id == elite.id || linked >= 8) {
                continue;
            }
            const double x =
                protectedEnemy.position.x - elite.position.x;
            const double z =
                protectedEnemy.position.z - elite.position.z;
            if (x * x + z * z > 5.5 * 5.5) {
                continue;
            }
            Vector3 target = enemyRenderPosition(protectedEnemy);
            target.y += static_cast<float>(
                simulation_.terrain().getHeight(
                    protectedEnemy.position.x,
                    protectedEnemy.position.z)) + 0.55F;
            const Vector3 source{
                center.x, center.y + 0.72F, center.z};
            DrawCylinderEx(
                source, target, 0.025F, 0.012F, 5,
                {63, 162, 255, 48});
            DrawLine3D(
                source, target, {176, 226, 255, 155});
            ++linked;
        }
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    for (const EnemyProjectile& projectile : snapshot.enemyProjectiles) {
        if (!projectile.active) continue;
        const Vector3 head{
            static_cast<float>(projectile.position.x),
            static_cast<float>(projectile.position.y),
            static_cast<float>(projectile.position.z)};
        Vector3 velocity{
            static_cast<float>(projectile.velocity.x),
            static_cast<float>(projectile.velocity.y),
            static_cast<float>(projectile.velocity.z)};
        if (Vector3LengthSqr(velocity) > 0.001F) {
            velocity = Vector3Normalize(velocity);
        }
        const Vector3 tail = Vector3Subtract(
            head, Vector3Scale(velocity, 0.72F));
        const float pulse = 0.5F + 0.5F * std::sin(
            static_cast<float>(snapshot.elapsedSeconds) * 13.0F +
            static_cast<float>(projectile.id.index) * 0.73F);
        DrawCylinderEx(tail, head, 0.035F, 0.12F, 8,
                       {84, 58, 220, 150});
        DrawSphere(head, 0.20F + pulse * 0.035F,
                   {166, 105, 255, 235});
        DrawSphere(head, 0.09F, {238, 221, 255, 255});
    }
    EndBlendMode();
    BeginBlendMode(BLEND_ADDITIVE);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    rlBegin(RL_TRIANGLES);
    Vector3 flameRight = Vector3CrossProduct(
        cameraForward, {0.0F, 1.0F, 0.0F});
    if (Vector3LengthSqr(flameRight) < 0.001F) {
        flameRight = {1.0F, 0.0F, 0.0F};
    } else {
        flameRight = Vector3Normalize(flameRight);
    }
    const bool detailedFlames = renderer_->settings().particles;
    for (const EnemyInstance& enemy : snapshot.enemies) {
        if (!enemy.active) {
            continue;
        }
        const auto burn = enemyBurnAmountById_.find(
            effectKey(enemy.id));
        if (burn == enemyBurnAmountById_.end() ||
            burn->second <= 0.01F) {
            continue;
        }
        Vector3 position = enemyRenderPosition(enemy);
        position.y += static_cast<float>(
            simulation_.terrain().getHeight(
                enemy.position.x, enemy.position.z));
        const float distanceSquared = Vector3DistanceSqr(
            position, camera.position);
        if (distanceSquared > 1600.0F) {
            continue;
        }
        const float scale = enemyFlameScale(enemy.type);
        Vector3 toCamera = Vector3Subtract(camera.position, position);
        toCamera.y = 0.0F;
        if (Vector3LengthSqr(toCamera) < 0.001F) {
            toCamera = Vector3Negate(cameraForward);
            toCamera.y = 0.0F;
        }
        toCamera = Vector3Normalize(toCamera);
        const Vector3 visibleFront = Vector3Add(
            position, Vector3Scale(toCamera, scale * 0.42F));
        const int lobeCount =
            detailedFlames && distanceSquared < 324.0F ? 3 : 2;
        for (int lobe = 0; lobe < lobeCount; ++lobe) {
            const float lobeIndex = static_cast<float>(lobe);
            const float phase = static_cast<float>(
                snapshot.elapsedSeconds * (8.2 + lobe * 1.35)) +
                static_cast<float>(enemy.id.index % 97U) * 0.37F +
                lobeIndex * 2.1F;
            Vector3 base = visibleFront;
            base.y += scale * (0.12F + lobeIndex * 0.16F);
            base = Vector3Add(
                base,
                Vector3Scale(
                    flameRight,
                    scale * (lobeIndex -
                        static_cast<float>(lobeCount - 1) * 0.5F) *
                        0.22F));
            const float height = scale *
                (1.02F + lobeIndex * 0.18F +
                 (0.5F + 0.5F * std::sin(phase * 1.31F)) * 0.24F);
            const float width = scale *
                (0.21F + 0.03F * lobeIndex);
            const float sway = scale * 0.20F * std::sin(phase);
            emitFlameLobe(
                base, flameRight, width, height, sway,
                burn->second * (1.0F - lobeIndex * 0.08F));
        }
        const int emberCount = detailedFlames &&
                distanceSquared < 324.0F
            ? 6
            : 2;
        for (int ember = 0; ember < emberCount; ++ember) {
            const float emberIndex = static_cast<float>(ember);
            const float seed = static_cast<float>(
                (enemy.id.index * 17U +
                 static_cast<std::uint32_t>(ember) * 29U) % 101U) /
                101.0F;
            const float rise = std::fmod(
                static_cast<float>(snapshot.elapsedSeconds) *
                    (0.62F + emberIndex * 0.035F) + seed,
                1.0F);
            const float orbit =
                static_cast<float>(snapshot.elapsedSeconds) *
                    (2.2F + emberIndex * 0.18F) +
                seed * 2.0F * PI;
            Vector3 emberPosition = visibleFront;
            emberPosition = Vector3Add(
                emberPosition,
                Vector3Scale(
                    flameRight,
                    std::sin(orbit) * scale *
                        (0.24F + emberIndex * 0.035F)));
            emberPosition = Vector3Add(
                emberPosition,
                Vector3Scale(toCamera, std::cos(orbit) * scale * 0.08F));
            emberPosition.y += scale * (0.48F + rise * 1.42F);
            const float emberFade =
                (1.0F - rise) * std::min(1.0F, rise * 7.0F);
            emitEmber(
                emberPosition, flameRight,
                scale * (0.035F + 0.008F *
                    (0.5F + 0.5F * std::sin(orbit * 1.7F))),
                burn->second * emberFade * 0.95F);
        }
    }
    rlEnd();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    for (const auto& enemy : snapshot.enemies) {
        if (!enemy.active) {
            continue;
        }
        const EnemyStatusEffect& freezeStatus =
            enemyStatusEffect(enemy, StatusEffectType::Freeze);
        if (freezeStatus.visualParameter <= 0.01) {
            continue;
        }
        Vector3 position = enemyRenderPosition(enemy);
        position.y += static_cast<float>(simulation_.terrain().getHeight(
            enemy.position.x, enemy.position.z));
        const float pulse = 0.5F + 0.5F * std::sin(
            static_cast<float>(snapshot.elapsedSeconds) * 6.0F +
            static_cast<float>(enemy.id.index) * 0.07F);
        const bool thawing = freezeStatus.remaining > 0.0 &&
            freezeStatus.remaining < 0.28;
        const float thawPulse = thawing
            ? 0.58F + 0.42F * (0.5F + 0.5F * std::sin(
                static_cast<float>(snapshot.elapsedSeconds) * 24.0F +
                static_cast<float>(enemy.id.index) * 0.31F))
            : 1.0F;
        const float effectAmount = static_cast<float>(
            std::clamp(freezeStatus.visualParameter, 0.0, 1.0)) *
            thawPulse;
        DrawCircle3D(
            {position.x, position.y + 0.035F, position.z},
            0.48F + pulse * 0.09F,
            {1.0F, 0.0F, 0.0F}, 90.0F,
            {142, 229, 255, static_cast<unsigned char>(
                150.0F * effectAmount)});
        if (freezeStatus.remaining > 0.0 && renderer_->settings().particles) {
            for (int mistIndex = 0; mistIndex < 1; ++mistIndex) {
                const float mistPhase =
                    static_cast<float>(snapshot.elapsedSeconds) *
                        (1.4F + static_cast<float>(mistIndex) * 0.35F) +
                    static_cast<float>(enemy.id.index) * 0.23F +
                    static_cast<float>(mistIndex) * 3.1F;
                DrawSphereEx(
                    {position.x + std::cos(mistPhase) * 0.24F,
                     position.y + 0.28F +
                         std::sin(mistPhase * 1.17F) * 0.11F,
                     position.z + std::sin(mistPhase) * 0.24F},
                    0.075F + 0.018F *
                        (0.5F + 0.5F * std::sin(mistPhase * 1.6F)),
                    5, 5,
                    {142, 229, 255,
                     static_cast<unsigned char>(42.0F * effectAmount)});
            }
            for (int crystal = 0; crystal < 4; ++crystal) {
                const float crystalIndex = static_cast<float>(crystal);
                const float angle = crystalIndex * 1.5708F +
                    static_cast<float>(enemy.id.index % 11U) * 0.19F;
                const float height = 0.22F +
                    (0.13F + crystalIndex * 0.025F) * effectAmount;
                const Vector3 base{
                    position.x + std::cos(angle) * 0.34F,
                    position.y + 0.04F,
                    position.z + std::sin(angle) * 0.34F};
                const Vector3 tip{
                    base.x + std::cos(angle) * 0.06F,
                    base.y + height,
                    base.z + std::sin(angle) * 0.06F};
                DrawLine3D(base, tip, {191, 246, 255,
                                       static_cast<unsigned char>(
                                           185.0F * effectAmount)});
                if (crystal % 2 == 0) {
                    DrawSphereEx(tip, 0.045F, 5, 5,
                                 {191, 246, 255,
                                  static_cast<unsigned char>(
                                      150.0F * effectAmount)});
                }
            }
        }
        if ((freezeStatus.remaining <= 0.0 || thawing) &&
            effectAmount > 0.05F) {
            const float crack = effectAmount * (0.5F + 0.5F * pulse);
            for (int branch = 0; branch < 3; ++branch) {
                const float branchIndex = static_cast<float>(branch);
                const float angle = branchIndex * 2.0944F +
                    static_cast<float>(enemy.id.index % 5U) * 0.21F;
                DrawLine3D(
                    {position.x, position.y + 0.55F, position.z},
                    {position.x + std::cos(angle) * 0.42F * crack,
                     position.y + 0.72F + branchIndex * 0.09F * crack,
                     position.z + std::sin(angle) * 0.42F * crack},
                    {223, 248, 255,
                     static_cast<unsigned char>(190.0F * crack)});
            }
        }
    }
    EndBlendMode();
    performanceStats_.enemyRender.sample(
        performanceMilliseconds(enemyRenderStart));
    destroyedEnemyDrawInstances_.clear();
    destroyedEnemyDrawInstances_.reserve(
        destroyedEnemyVisuals_.size());
    for (const DestroyedEnemyVisual& visual :
         destroyedEnemyVisuals_) {
        const float progress = static_cast<float>(
            1.0 - visual.remaining / visual.duration);
        const float fade = 1.0F - smoothstep(
            0.68F, 1.0F, progress);
        const float quantizedFade =
            std::ceil(
                std::clamp(fade, 0.0F, 1.0F) * 4.0F) /
            4.0F;
        const auto alpha = static_cast<unsigned char>(
            std::lround(quantizedFade * 255.0F));
        const EnemyInstance visualEnemy{
            .type = visual.type,
            .position = visual.position,
            .eliteAffixes = visual.eliteAffixes,
            .surfaceHeightOffset =
                visual.surfaceHeightOffset,
        };
        Vector3 position =
            enemyRenderPosition(visualEnemy);
        position.y += static_cast<float>(
            simulation_.terrain().getHeight(
                visual.position.x,
                visual.position.z));
        const Vector3 toEnemy =
            Vector3Subtract(position, camera.position);
        const float distanceSquared =
            Vector3LengthSqr(toEnemy);
        if (distanceSquared > 9.0F) {
            const float inverseDistance =
                1.0F / std::sqrt(distanceSquared);
            if (Vector3DotProduct(
                    cameraForward,
                    Vector3Scale(
                        toEnemy, inverseDistance)) <
                -0.12F) {
                continue;
            }
        }
        destroyedEnemyDrawInstances_.push_back({
            .modelVisual =
                enemyModelVisual(visual.type),
            .animationVisual =
                EnemyAnimationVisual::Death,
            .animationSeconds =
                progress *
                static_cast<float>(visual.duration),
            .position = position,
            .yawRadians =
                static_cast<float>(visual.yaw),
            .tint = visual.eliteAffixes != 0U
                ? Color{255, 176, 122, alpha}
                : Color{255, 255, 255, alpha},
            .scale = enemyVisualScale(visual.type) *
                (visual.eliteAffixes != 0U ? 1.08F : 1.0F),
            .loop = false,
            .lowDetail = distanceSquared >
                EnemyFullDetailDistanceSquared &&
                visual.type != EnemyType::Boss,
        });
    }
    if (!destroyedEnemyDrawInstances_.empty()) {
        WorldMaterialState material{};
        material.bakedAo = 0.82F;
        renderer_->setWorldMaterial(material);
        if (!renderer_->drawEnemiesInstanced(
                destroyedEnemyDrawInstances_)) {
            for (const EnemyDrawInstance& instance :
                 destroyedEnemyDrawInstances_) {
                static_cast<void>(renderer_->drawEnemy(
                    instance.modelVisual,
                    instance.animationVisual,
                    instance.animationSeconds,
                    instance.position,
                    instance.yawRadians,
                    instance.tint, instance.scale,
                    instance.loop,
                    instance.inkOutlineEligible));
            }
        }
    }
    renderer_->endWorldShader();
    const auto grassStart = PerformanceClock::now();
    renderer_->drawGrassInstances(
        camera.position, static_cast<float>(snapshot.worldLimit),
        nightAmount, lighting, clearAreas);
    performanceStats_.grassRender.sample(
        performanceMilliseconds(grassStart));
}

} // namespace ian
