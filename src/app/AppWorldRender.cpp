#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "presentation/PresentationEffectQueries.hpp"

#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace ian {

using namespace app_detail;

namespace {

struct LootItemVisual {
    Vector3 position{};
    float rotation{};
    float scale{};
};

LootItemVisual lootItemVisual(
    const SimulationSnapshot& snapshot,
    const LootChestInstance& chest) {
    const float reveal = static_cast<float>(chest.loot.revealProgress);
    const float eased = 1.0F - std::pow(1.0F - reveal, 3.0F);
    const float riseSpin = reveal * reveal * (3.0F - 2.0F * reveal);
    return {
        .position = {
            static_cast<float>(chest.position.x),
            static_cast<float>(chest.position.y) + 0.48F +
                eased * 1.18F +
                std::sin(static_cast<float>(chest.loot.hoverTime) * 2.4F) *
                    0.08F,
            static_cast<float>(chest.position.z),
        },
        .rotation = static_cast<float>(snapshot.elapsedSeconds) * 1.65F +
            static_cast<float>(chest.id.index) * 0.73F + riseSpin * PI,
        .scale = 1.50F * (0.35F + eased * 0.65F) +
            std::sin(reveal * PI) * 0.24F,
    };
}

} // namespace

void App::drawLootItemOutlines(
    const SimulationSnapshot& snapshot) {
    for (const LootChestInstance& chest : snapshot.lootChests) {
        if (chest.loot.revealProgress <= 0.0 || chest.loot.collected) {
            continue;
        }
        const LootItemVisual visual = lootItemVisual(snapshot, chest);
        renderer_->drawLootItemOutline(
            visual.position, chest.loot.effect, chest.loot.rarity,
            visual.rotation, visual.scale);
    }
}

void App::drawWorldEntities(
    const SimulationSnapshot& snapshot, const Camera3D& camera,
    float nightAmount, const WorldLighting& lighting,
    float interpolationAlpha) {
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
        0.56F, 0.62F, 0.48F, 1.0F};
    boundaryForestMaterial.bakedAo = 0.66F;
    boundaryForestMaterial.windAmount = 0.28F;
    renderer_->setWorldMaterial(boundaryForestMaterial);
    renderer_->drawBoundaryForest();
    WorldMaterialState decorativeRockMaterial{};
    decorativeRockMaterial.bakedAo = 0.82F;
    renderer_->setWorldMaterial(decorativeRockMaterial);
    renderer_->drawDecorativeRocks(
        camera.position,
        static_cast<float>(snapshot.worldLimit),
        grassClearAreas_);
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
                    .visualVariant = static_cast<std::size_t>(
                        node.id.index % TreeVisualVariantCount),
                });
                continue;
            }
            renderer_->setWorldMaterial(material);
            if (!renderer_->drawTree(
                    nodePosition,
                    WHITE,
                    hitScale * static_cast<float>(node.visualScale),
                    static_cast<std::size_t>(
                        node.id.index % TreeVisualVariantCount),
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
        } else {
            if (material.hitFlashAmount <= 0.001F) {
                resourceRockDrawInstances_.push_back({
                    .position = nodePosition,
                    .scale = hitScale,
                });
                continue;
            }
            renderer_->setWorldMaterial(material);
            if (!renderer_->drawRock(
                    nodePosition,
                    WHITE, hitScale)) {
                DrawSphere(
                    nodePosition, 0.9F, {104, 116, 128, 255});
            }
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
                    rock.position, WHITE, rock.scale));
            }
        }
    }
    if (!resourceTreeDrawInstances_.empty()) {
        WorldMaterialState treeMaterial{};
        treeMaterial.bakedAo = 0.78F;
        treeMaterial.windAmount = 1.0F;
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
    WorldMaterialState chestMaterial{};
    chestMaterial.bakedAo = 0.78F;
    renderer_->setWorldMaterial(chestMaterial);
    for (const LootChestInstance& chest : snapshot.lootChests) {
        const Vector3 position{
            static_cast<float>(chest.position.x),
            static_cast<float>(chest.position.y),
            static_cast<float>(chest.position.z),
        };
        static_cast<void>(renderer_->drawLootChest(
            chest.type, position, static_cast<float>(chest.yaw),
            static_cast<float>(chest.openingProgress)));
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
        renderer_->drawLootItem(
            visual.position, chest.loot.effect, chest.loot.rarity,
            visual.rotation, WHITE, visual.scale);
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
        } else {
            static_cast<void>(
                renderer_->drawRock(position, WHITE, scale));
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
            {
                platformFramePreview_
                            && !modularDragPiece_
                    ? &*platformFramePreview_
                    : nullptr,
                wallPreview_ && !modularDragPiece_
                    ? &*wallPreview_
                    : nullptr,
                rampPreview_ && !modularDragPiece_
                    ? &*rampPreview_
                    : nullptr,
                modularPlatformDragPreviews_,
                modularWallDragPreviews_,
                modularRampDragPreviews_,
                foundationTerrainHit_
                    ? &*foundationTerrainHit_
                    : nullptr,
                simulation_.terrain().config()
                    .maxWoodSupportLength,
                (wallPreview_ || rampPreview_)
                    ? std::optional<float>{
                          static_cast<float>(
                              placementRotationYaw_)}
                    : std::nullopt,
                !modularDragPiece_ &&
                        modularPreviewVisualOrigin_
                    ? &*modularPreviewVisualOrigin_
                    : nullptr,
            });
    }
    for (const auto& building : snapshot.buildings) {
        const Vec3 center = buildingWorldPosition(building);
        const Vec3 impact =
            buildingImpactOffsetAt(building.id);
        float defensiveYaw = 0.0F;
        if (building.type == BuildingType::Turret) {
            defensiveYaw = towerYaw(snapshot, building);
        } else if (building.type == BuildingType::Cannon) {
            defensiveYaw = cannonYaw(snapshot, building);
        }
        const Vec3 shotRecoil =
            buildingShotRecoilOffsetAt(
                building.id, defensiveYaw);
        const float x =
            static_cast<float>(
                center.x + impact.x + shotRecoil.x);
        const float z =
            static_cast<float>(
                center.z + impact.z + shotRecoil.z);
        const float groundY =
            static_cast<float>(center.y);
        const float spawnScale =
            buildingAnimationScaleAt(center);
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
            modularBuildingRenderer_.drawWorld(
                {
                    std::span<
                        const PlatformFrameInstance>{
                        &*foundation, 1U},
                    {}, {}, {},
                    simulation_.terrain()
                        .config().cellSize,
                    std::nullopt, {}, 1.0F,
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
                    spawnScale)) {
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
        } else if (
            building.type == BuildingType::GoldMine ||
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
                drawScaledCube(0.0F, 0.55F, 0.0F, 2.0F,
                               1.1F, 2.0F,
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
        } else if (building.type == BuildingType::SlowTrap) {
            drawScaledCube(0.0F, 0.08F, 0.0F, 1.0F, 0.16F,
                           1.0F, {76, 110, 132, 255});
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
    for (const auto& projectile : snapshot.cannonProjectiles) {
        if (projectile.active) {
            const Vector3 projectilePosition{
                static_cast<float>(projectile.position.x),
                static_cast<float>(projectile.position.y),
                static_cast<float>(projectile.position.z),
            };
            if (!renderer_->drawCannonball(projectilePosition)) {
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
        (void)renderer_->drawArrow(
            {static_cast<float>(arrowPosition.x),
             static_cast<float>(arrowPosition.y),
             static_cast<float>(arrowPosition.z)},
            {static_cast<float>(arrow.target.x - arrow.origin.x),
             static_cast<float>(arrow.target.y - arrow.origin.y),
             static_cast<float>(arrow.target.z - arrow.origin.z)});
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
    constexpr float EnemyFullDetailDistance = 30.0F;
    constexpr float EnemyFullDetailDistanceSquared =
        EnemyFullDetailDistance * EnemyFullDetailDistance;
    for (const auto& enemy : snapshot.enemies) {
        if (!enemy.active) {
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
            enemyDistanceSquared > EnemyFullDetailDistanceSquared;
        const float hitFlash =
            hitFlashAt(enemy.position, 1.6);
        const float enemyScale =
            enemyVisualScale(enemy.type) * enemyHitScale(enemy);
        const EnemyStatusEffect& freezeStatus =
            enemyStatusEffect(enemy, StatusEffectType::Freeze);
        const bool frozen = freezeStatus.remaining > 0.0;
        Color modelTint = WHITE;
        if (frozen) {
            modelTint = {151, 224, 255, 255};
        } else if (enemy.slowRemaining > 0.0) {
            modelTint = {184, 222, 255, 255};
        } else if (
            enemy.state == EnemyState::BossRamWindup) {
            modelTint = {255, 178, 150, 255};
        }
        if (!aimed) {
            if (hitFlash > 0.001F) {
                const float quantizedFlash =
                    std::ceil(
                        std::clamp(hitFlash, 0.0F, 1.0F) *
                        3.0F) /
                    3.0F;
                modelTint = {
                    static_cast<unsigned char>(
                        std::lround(
                            static_cast<float>(modelTint.r) +
                            (255.0F -
                             static_cast<float>(
                                 modelTint.r)) *
                                quantizedFlash)),
                    static_cast<unsigned char>(
                        std::lround(
                            static_cast<float>(modelTint.g) +
                            (176.0F -
                             static_cast<float>(
                                 modelTint.g)) *
                                quantizedFlash)),
                    static_cast<unsigned char>(
                        std::lround(
                            static_cast<float>(modelTint.b) +
                            (145.0F -
                             static_cast<float>(
                                 modelTint.b)) *
                                quantizedFlash)),
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
                    frozen ? EnemyAnimationVisual::Idle
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
        }
        if (aimed) {
            body = {242, 118, 76, 255};
        } else if (frozen) {
            body = {91, 183, 225, 255};
        } else if (enemy.slowRemaining > 0.0) {
            body = {70, 128, 170, 255};
        } else if (enemy.state == EnemyState::BossRamWindup) {
            body = {235, 64, 45, 255};
        }
        if (!renderer_->drawEnemy(
                enemyModelVisual(enemy.type),
                frozen ? EnemyAnimationVisual::Idle
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
                    instance.loop));
            }
        }
    }
    BeginBlendMode(BLEND_ADDITIVE);
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
            .tint = {255, 255, 255, alpha},
            .scale = enemyVisualScale(visual.type),
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
                    instance.loop));
            }
        }
    }
    renderer_->endWorldShader();
    renderer_->drawGrassInstances(
        camera.position, static_cast<float>(snapshot.worldLimit),
        nightAmount, lighting, grassClearAreas_);
}

} // namespace ian
