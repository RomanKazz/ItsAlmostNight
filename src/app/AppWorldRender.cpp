#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "presentation/PresentationEffectQueries.hpp"

#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace ian {

using namespace app_detail;

void App::drawWorldEntities(
    const SimulationSnapshot& snapshot, const Camera3D& camera,
    float nightAmount, const WorldLighting& lighting) {
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
                simulation_.terrain().getHeight(
                    node.position.x,
                    node.position.z)),
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
            DrawSphere({static_cast<float>(projectile.position.x),
                        static_cast<float>(projectile.position.y),
                        static_cast<float>(projectile.position.z)},
                       0.16F, {52, 57, 62, 255});
        }
    }
    enemyDrawInstances_.clear();
    enemyDrawInstances_.reserve(snapshot.enemies.size());
    const Vector3 cameraForward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    for (const auto& enemy : snapshot.enemies) {
        if (!enemy.active) {
            continue;
        }
        Vector3 enemyPosition =
            enemyRenderPosition(enemy);
        enemyPosition.y += static_cast<float>(
            simulation_.terrain().getHeight(
                enemy.position.x,
                enemy.position.z));
        const Vector3 toEnemy =
            Vector3Subtract(enemyPosition, camera.position);
        const float enemyDistanceSquared =
            Vector3LengthSqr(toEnemy);
        if (enemyDistanceSquared > 9.0F) {
            const float inverseDistance =
                1.0F / std::sqrt(enemyDistanceSquared);
            const float viewDot = Vector3DotProduct(
                cameraForward,
                Vector3Scale(toEnemy, inverseDistance));
            if (viewDot < -0.12F) {
                continue;
            }
        }
        const bool aimed =
            snapshot.aimedEnemy &&
            *snapshot.aimedEnemy == enemy.id;
        const float hitFlash =
            hitFlashAt(enemy.position, 1.6);
        Color modelTint = WHITE;
        if (enemy.slowRemaining > 0.0) {
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
            float animationTime = enemyAnimationSeconds(
                enemy, snapshot.elapsedSeconds);
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
                    enemyAnimationVisual(enemy),
                .animationSeconds = animationTime,
                .position = enemyPosition,
                .yawRadians =
                    static_cast<float>(enemy.yaw),
                .tint = modelTint,
                .scale = enemyVisualScale(enemy.type),
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
        } else if (enemy.slowRemaining > 0.0) {
            body = {70, 128, 170, 255};
        } else if (enemy.state == EnemyState::BossRamWindup) {
            body = {235, 64, 45, 255};
        }
        if (!renderer_->drawEnemy(
                enemyModelVisual(enemy.type),
                enemyAnimationVisual(enemy),
                enemyAnimationSeconds(
                    enemy, snapshot.elapsedSeconds),
                enemyPosition, static_cast<float>(enemy.yaw),
                modelTint, enemyVisualScale(enemy.type))) {
            DrawCube(enemyPosition, width, height, width,
                     body);
            DrawSphere(
                {enemyPosition.x,
                 enemyPosition.y + height * 0.62F,
                 enemyPosition.z},
                width * 0.52F,
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
