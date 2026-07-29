#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "presentation/PresentationEffectQueries.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace ian {

using namespace app_detail;

void App::drawShadowPass(
    const SimulationSnapshot& snapshot,
    const WorldLighting& lighting) {
    const Vector3 shadowFocus{
        static_cast<float>(snapshot.playerPosition.x),
        0.0F,
        static_cast<float>(snapshot.playerPosition.z),
    };
    const auto withinLocalShadowDistance =
        [shadowFocus](Vector3 position, float maximumDistance) {
            const float offsetX = position.x - shadowFocus.x;
            const float offsetZ = position.z - shadowFocus.z;
            return offsetX * offsetX + offsetZ * offsetZ <=
                   maximumDistance * maximumDistance;
        };
    if (renderer_->beginShadowPass(lighting, shadowFocus)) {
        renderer_->drawTerrain(WHITE);
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
            if (renderer_->shadowCasterVisible(
                    center, std::max(width, depth) * 0.5F)) {
                DrawCube(center, width,
                         static_cast<float>(obstacle.height), depth, WHITE);
            }
        }
        const double modularCellSize =
            simulation_.terrain().config().cellSize;
        const auto animationScales =
            modularAnimationScales(snapshot);
        modularBuildingRenderer_.drawShadow({
            snapshot.platformFrames,
            snapshot.modularWalls,
            snapshot.ramps,
            snapshot.sharedSupports,
            modularCellSize,
            std::nullopt,
            animationScales,
            1.0F,
        });
        for (const auto& node : snapshot.resourceNodes) {
            if (!node.active) {
                continue;
            }
            const Vector3 nodePosition{
                static_cast<float>(node.position.x),
                static_cast<float>(
                    simulation_.terrain().getHeight(
                        node.position.x,
                        node.position.z)),
                static_cast<float>(node.position.z),
            };
            if (!renderer_->shadowCasterVisible(
                    nodePosition, static_cast<float>(node.radius)) ||
                !withinLocalShadowDistance(nodePosition, 45.0F)) {
                continue;
            }
            if (node.type == ResourceType::Wood) {
                const float hitScale =
                    presentation::resourceHitScale(
                        effects_, node.id);
                if (!renderer_->drawTree(
                        nodePosition,
                        WHITE, hitScale)) {
                    DrawCylinder(
                        {nodePosition.x,
                         nodePosition.y + 0.9F,
                         nodePosition.z},
                        0.32F, 0.42F, 1.8F, 8, WHITE);
                    DrawSphere(
                        {nodePosition.x,
                         nodePosition.y + 2.2F,
                         nodePosition.z},
                        1.15F, WHITE);
                }
            } else {
                const float hitScale =
                    presentation::resourceHitScale(
                        effects_, node.id);
                if (!renderer_->drawRock(
                        nodePosition,
                        WHITE, hitScale)) {
                    DrawSphere(nodePosition, 0.9F, WHITE);
                }
            }
        }
        for (const auto& building : snapshot.buildings) {
            const Vec3 center =
                buildingWorldPosition(building);
            const float x = static_cast<float>(center.x);
            const float z = static_cast<float>(center.z);
            const float groundY =
                static_cast<float>(center.y);
            const float spawnScale =
                buildingAnimationScaleAt(center);
            if (!renderer_->shadowCasterVisible(
                    {x, groundY + 1.0F, z}, 2.2F)) {
                continue;
            }
            if (const auto foundation =
                    automaticBuildingFoundation(
                        building.type,
                        building.gridPosition,
                        building.baseHeight,
                        building
                            .foundationBottomHeight,
                        modularCellSize,
                        building.id)) {
                modularBuildingRenderer_.drawShadow({
                    std::span<
                        const PlatformFrameInstance>{
                        &*foundation, 1U},
                    {}, {}, {},
                    modularCellSize,
                    std::nullopt, {}, 1.0F,
                });
            }
            if (building.type == BuildingType::Core) {
                constexpr float QuarterTurn = PI * 0.5F;
                if (!renderer_->drawCore(
                        {x, groundY, z},
                        static_cast<float>(building.rotation) *
                            QuarterTurn,
                        WHITE, spawnScale)) {
                    DrawCube({x, groundY + 1.25F, z},
                             2.0F, 2.5F,
                             2.0F, WHITE);
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
                    DrawCube({x, groundY + 1.0F, z},
                             1.0F, 2.0F,
                             1.0F, WHITE);
                }
            } else if (building.type == BuildingType::Turret) {
                if (!renderer_->drawCrossbow(
                        {x, groundY, z},
                        towerYaw(snapshot, building), WHITE,
                        spawnScale)) {
                    DrawCube({x, groundY + 0.6F, z},
                             1.0F, 1.2F, 1.0F,
                             WHITE);
                    DrawCylinder({x, groundY + 1.45F, z},
                                 0.42F, 0.32F,
                                 0.7F, 8, WHITE);
                    DrawCube({x, groundY + 1.55F,
                              z - 0.55F}, 0.18F,
                             0.18F, 1.0F, WHITE);
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
                    DrawCube({x, groundY + 0.55F, z},
                             2.0F, 1.1F,
                             2.0F, WHITE);
                }
            } else if (building.type == BuildingType::Cannon) {
                if (!renderer_->drawCannon({x, groundY, z},
                                           cannonYaw(snapshot, building),
                                           cannonPitch(snapshot, building),
                                           WHITE, spawnScale)) {
                    DrawCube({x, groundY + 0.6F, z},
                             1.0F, 1.2F, 1.0F,
                             WHITE);
                    DrawSphere({x, groundY + 1.35F, z},
                               0.48F, WHITE);
                    DrawCube({x, groundY + 1.45F,
                              z - 0.75F}, 0.28F,
                             0.28F, 1.4F, WHITE);
                }
            } else if (building.type == BuildingType::SlowTrap) {
                DrawCube({x, groundY + 0.08F, z},
                         1.0F, 0.16F, 1.0F,
                         WHITE);
            } else if ((building.rotation % 2U) == 0U) {
                DrawCube({x - 0.38F, groundY + 1.0F, z},
                         0.22F, 2.0F,
                         1.0F, WHITE);
                DrawCube({x + 0.38F, groundY + 1.0F, z},
                         0.22F, 2.0F,
                         1.0F, WHITE);
                if (!building.open) {
                    DrawCube({x, groundY + 1.0F, z},
                             0.55F, 1.7F, 0.18F,
                             WHITE);
                }
            } else {
                DrawCube({x, groundY + 1.0F, z - 0.38F},
                         1.0F, 2.0F,
                         0.22F, WHITE);
                DrawCube({x, groundY + 1.0F, z + 0.38F},
                         1.0F, 2.0F,
                         0.22F, WHITE);
                if (!building.open) {
                    DrawCube({x, groundY + 1.0F, z},
                             0.18F, 1.7F, 0.55F,
                             WHITE);
                }
            }
        }
        const std::size_t activeEnemyCount =
            static_cast<std::size_t>(std::count_if(
                snapshot.enemies.begin(),
                snapshot.enemies.end(),
                [](const EnemyInstance& enemy) {
                    return enemy.active;
                }));
        const std::size_t enemyShadowBudget =
            activeEnemyCount > 768U
                ? 12U
                : (activeEnemyCount > 256U
                       ? 24U
                       : std::min(
                             activeEnemyCount,
                             std::size_t{64U}));
        std::size_t renderedEnemyShadows = 0U;
        for (const auto& enemy : snapshot.enemies) {
            if (!enemy.active) {
                continue;
            }
            if (renderedEnemyShadows >= enemyShadowBudget) {
                break;
            }
            Vector3 enemyPosition =
                enemyRenderPosition(enemy);
            enemyPosition.y += static_cast<float>(
                simulation_.terrain().getHeight(
                    enemy.position.x,
                    enemy.position.z));
            float width = 0.8F;
            float height = 1.6F;
            if (enemy.type == EnemyType::Fast) {
                width = 0.65F;
                height = 1.35F;
            } else if (enemy.type == EnemyType::Heavy) {
                width = 1.15F;
                height = 2.0F;
            } else if (enemy.type == EnemyType::Boss) {
                width = 2.0F;
                height = 3.2F;
            } else if (enemy.type == EnemyType::Ranged) {
                width = 0.75F;
                height = 1.55F;
            } else if (enemy.type == EnemyType::Sapper) {
                width = 0.86F;
                height = 1.5F;
            } else if (enemy.type == EnemyType::Flying) {
                width = 0.72F;
                height = 1.0F;
            }
            float casterDistance = 36.0F;
            if (enemy.type == EnemyType::Heavy) {
                casterDistance = 45.0F;
            } else if (enemy.type == EnemyType::Boss) {
                casterDistance = 60.0F;
            }
            if (!renderer_->shadowCasterVisible(enemyPosition, width) ||
                !withinLocalShadowDistance(enemyPosition,
                                           casterDistance)) {
                continue;
            }
            if (!renderer_->drawEnemy(
                    enemyModelVisual(enemy.type),
                    enemyAnimationVisual(enemy),
                    enemyAnimationSeconds(
                        enemy, snapshot.elapsedSeconds),
                    enemyPosition,
                    static_cast<float>(enemy.yaw), WHITE,
                    enemyVisualScale(enemy.type))) {
                DrawCube(enemyPosition, width, height,
                         width, WHITE);
            }
            ++renderedEnemyShadows;
        }
        renderer_->endShadowPass();
    }
}

void App::drawSelectionPass(
    const SimulationSnapshot& snapshot, const Camera3D& camera) {
    renderer_->clearSelectionOutline();
    if (!removalDragActive_ &&
        (snapshot.aimedResource || snapshot.aimedBuilding ||
         snapshot.aimedEnemy ||
         (!foundationBuildMode_ &&
          snapshot.aimedModularBuilding)) &&
        renderer_->beginSelectionMaskPass(camera)) {
        if (snapshot.aimedResource) {
            const auto resource = std::find_if(
                snapshot.resourceNodes.begin(),
                snapshot.resourceNodes.end(),
                [&snapshot](const ResourceNode& node) {
                    return node.active &&
                           node.id == *snapshot.aimedResource;
                });
            if (resource != snapshot.resourceNodes.end()) {
                const Vec3 hitOffset =
                    presentation::resourceHitOffset(
                        effects_,
                        resource->id,
                        resource->position);
                const Vector3 resourcePosition{
                    static_cast<float>(
                        resource->position.x +
                        hitOffset.x),
                    static_cast<float>(
                        simulation_.terrain().getHeight(
                            resource->position.x,
                            resource->position.z)),
                    static_cast<float>(
                        resource->position.z +
                        hitOffset.z),
                };
                if (resource->type == ResourceType::Wood) {
                    renderer_->setSelectionMaskWind(1.0F);
                    const float hitScale =
                        presentation::resourceHitScale(
                            effects_,
                            resource->id);
                    if (!renderer_->drawTree(
                            resourcePosition,
                            WHITE, hitScale)) {
                        DrawCylinder(
                            {resourcePosition.x,
                             resourcePosition.y + 0.9F,
                             resourcePosition.z},
                            0.32F, 0.42F, 1.8F, 8, WHITE);
                        DrawSphere(
                            {resourcePosition.x,
                             resourcePosition.y + 2.2F,
                             resourcePosition.z},
                            1.15F, WHITE);
                    }
                } else if (!renderer_->drawRock(
                               resourcePosition,
                               WHITE,
                               presentation::resourceHitScale(
                                   effects_,
                                   resource->id))) {
                    DrawSphere(resourcePosition, 0.9F, WHITE);
                }
            }
        } else if (snapshot.aimedBuilding) {
            const auto building = std::find_if(
                snapshot.buildings.begin(),
                snapshot.buildings.end(),
                [&snapshot](const BuildingInstance& candidate) {
                    return candidate.id == *snapshot.aimedBuilding;
                });
            if (building != snapshot.buildings.end()) {
                const Vec3 center =
                    buildingWorldPosition(*building);
                const Vec3 impact =
                    buildingImpactOffsetAt(building->id);
                float defensiveYaw = 0.0F;
                if (building->type == BuildingType::Turret) {
                    defensiveYaw =
                        towerYaw(snapshot, *building);
                } else if (
                    building->type == BuildingType::Cannon) {
                    defensiveYaw =
                        cannonYaw(snapshot, *building);
                }
                const Vec3 shotRecoil =
                    buildingShotRecoilOffsetAt(
                        building->id, defensiveYaw);
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
                if (building->type == BuildingType::Core) {
                    constexpr float QuarterTurn = PI * 0.5F;
                    if (!renderer_->drawCore(
                            {x, groundY, z},
                            static_cast<float>(building->rotation) *
                                QuarterTurn,
                            WHITE, spawnScale)) {
                        DrawCube({x, groundY + 1.25F, z},
                                 2.0F, 2.5F,
                                 2.0F, WHITE);
                    }
                } else if (building->type == BuildingType::Wall) {
                    const std::uint8_t connections =
                        wallConnectionMask(
                            snapshot.buildings,
                            building->gridPosition,
                            building->baseHeight);
                    if (!renderer_->drawWall(
                            {x, groundY, z}, connections,
                            static_cast<float>(
                                wallFallbackRotation(
                                    snapshot.buildings,
                                    *building)) *
                                PI * 0.5F,
                            WHITE, spawnScale)) {
                        DrawCube({x, groundY + 1.0F, z},
                                 1.0F, 2.0F,
                                 1.0F, WHITE);
                    }
                } else if (building->type ==
                           BuildingType::Turret) {
                    if (!renderer_->drawCrossbow(
                            {x, groundY, z},
                            defensiveYaw,
                            WHITE, spawnScale)) {
                        DrawCube({x, groundY + 0.6F, z},
                                 1.0F, 1.2F,
                                 1.0F, WHITE);
                        DrawCylinder(
                            {x, groundY + 1.45F, z},
                            0.42F, 0.32F,
                            0.7F, 8, WHITE);
                        DrawCube(
                            {x, groundY + 1.55F,
                             z - 0.55F}, 0.18F,
                            0.18F, 1.0F, WHITE);
                    }
                } else if (
                    building->type ==
                        BuildingType::GoldMine ||
                    building->type ==
                        BuildingType::LumberMill ||
                    building->type ==
                        BuildingType::Quarry) {
                    constexpr float QuarterTurn =
                        PI * 0.5F;
                    if (!renderer_->drawResourceProducer(
                            building->type, {x, groundY, z},
                            static_cast<float>(
                                building->rotation) *
                                QuarterTurn,
                            WHITE,
                            spawnScale *
                                productionScaleAt(
                                    building->id))) {
                        DrawCube({x, groundY + 0.55F, z},
                                 2.0F,
                                 1.1F, 2.0F, WHITE);
                    }
                } else if (building->type ==
                           BuildingType::Cannon) {
                    if (!renderer_->drawCannon(
                            {x, groundY, z},
                            defensiveYaw,
                            cannonPitch(snapshot, *building),
                            WHITE, spawnScale)) {
                        DrawCube({x, groundY + 0.6F, z},
                                 1.0F, 1.2F,
                                 1.0F, WHITE);
                        DrawSphere({x, groundY + 1.35F, z},
                                   0.48F,
                                   WHITE);
                        DrawCube(
                            {x, groundY + 1.45F,
                             z - 0.75F}, 0.28F,
                            0.28F, 1.4F, WHITE);
                    }
                } else if (building->type ==
                           BuildingType::SlowTrap) {
                    DrawCube({x, groundY + 0.08F, z},
                             1.0F, 0.16F,
                             1.0F, WHITE);
                } else if ((building->rotation % 2U) == 0U) {
                    DrawCube({x - 0.38F, groundY + 1.0F, z},
                             0.22F,
                             2.0F, 1.0F, WHITE);
                    DrawCube({x + 0.38F, groundY + 1.0F, z},
                             0.22F,
                             2.0F, 1.0F, WHITE);
                    if (!building->open) {
                        DrawCube({x, groundY + 1.0F, z},
                                 0.55F, 1.7F,
                                 0.18F, WHITE);
                    }
                } else {
                    DrawCube({x, groundY + 1.0F, z - 0.38F},
                             1.0F,
                             2.0F, 0.22F, WHITE);
                    DrawCube({x, groundY + 1.0F, z + 0.38F},
                             1.0F,
                             2.0F, 0.22F, WHITE);
                    if (!building->open) {
                        DrawCube({x, groundY + 1.0F, z},
                                 0.18F, 1.7F,
                                 0.55F, WHITE);
                    }
                }
            }
        } else if (snapshot.aimedEnemy) {
            const auto enemy = std::find_if(
                snapshot.enemies.begin(),
                snapshot.enemies.end(),
                [&snapshot](const EnemyInstance& candidate) {
                    return candidate.active &&
                           candidate.id ==
                               *snapshot.aimedEnemy;
                });
            if (enemy != snapshot.enemies.end()) {
                Vector3 enemyPosition =
                    enemyRenderPosition(*enemy);
                enemyPosition.y += static_cast<float>(
                    simulation_.terrain().getHeight(
                        enemy->position.x,
                        enemy->position.z));
                static_cast<void>(renderer_->drawEnemy(
                    enemyModelVisual(enemy->type),
                    enemyAnimationVisual(*enemy),
                    enemyAnimationSeconds(
                        *enemy, snapshot.elapsedSeconds),
                    enemyPosition,
                    static_cast<float>(enemy->yaw), WHITE,
                    enemyVisualScale(enemy->type)));
            }
        } else if (
            !foundationBuildMode_ &&
            snapshot.aimedModularBuilding) {
            const EntityId target =
                *snapshot.aimedModularBuilding;
            const double cellSize =
                simulation_.terrain()
                    .config().cellSize;
            const auto animationScales =
                modularAnimationScales(snapshot);
            const auto frame = std::find_if(
                snapshot.platformFrames.begin(),
                snapshot.platformFrames.end(),
                [target](
                    const PlatformFrameInstance& candidate) {
                    return candidate.id == target;
                });
            const auto wall = std::find_if(
                snapshot.modularWalls.begin(),
                snapshot.modularWalls.end(),
                [target](const WallInstance& candidate) {
                    return candidate.id == target;
                });
            const auto ramp = std::find_if(
                snapshot.ramps.begin(),
                snapshot.ramps.end(),
                [target](const RampInstance& candidate) {
                    return candidate.id == target;
                });
            modularBuildingRenderer_.drawWorld(
                {
                    !snapshot.selectedBuilding &&
                            frame !=
                                snapshot.platformFrames.end()
                        ? std::span<
                              const PlatformFrameInstance>{
                              &*frame, 1U}
                        : std::span<
                              const PlatformFrameInstance>{},
                    wall != snapshot.modularWalls.end()
                        ? std::span<const WallInstance>{
                              &*wall, 1U}
                        : std::span<const WallInstance>{},
                    ramp != snapshot.ramps.end()
                        ? std::span<const RampInstance>{
                              &*ramp, 1U}
                        : std::span<const RampInstance>{},
                    {},
                    cellSize,
                    std::nullopt,
                    animationScales,
                    1.0F,
                },
                {});
        }
        renderer_->endSelectionMaskPass();
    }
}

} // namespace ian
