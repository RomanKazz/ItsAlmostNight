#include "graphics/Renderer.hpp"
#include "graphics/CrystalMineModel.hpp"
#include "graphics/QuarryModel.hpp"
#include "graphics/CoreCrystalModel.hpp"
#include "graphics/GunTurretModel.hpp"
#include "graphics/CannonModel.hpp"
#include "graphics/CrossbowModel.hpp"
#include "graphics/CatapultModel.hpp"
#include "graphics/GateModel.hpp"
#include "graphics/WallUpgradeModel.hpp"
#include "graphics/RendererModelSupport.hpp"
#include "graphics/WorldTransforms.hpp"

#include "buildings/BuildingSystem.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "world/CollisionWorld.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {
using namespace renderer_model_detail;

std::optional<double> Renderer::buildingRaycastDistance(
    const BuildingInstance& building,
    std::span<const BuildingInstance> buildings, Ray ray,
    double maxDistance, float defensiveYaw,
    float cannonPitchRadians, float gateOpenAmount,
    float /*spikeAnimationSeconds*/) {
    const Vec3 worldCenter =
        buildingWorldPosition(building);
    Vector3 position{
        static_cast<float>(worldCenter.x),
        static_cast<float>(worldCenter.y),
        static_cast<float>(worldCenter.z)};
    ModelResource* resource = nullptr;
    float modelScale = 1.0F;
    float horizontalScale = 1.0F;
    float yaw = 0.0F;
    float groundOffset = 0.0F;

    constexpr float QuarterTurn = PI * 0.5F;
    switch (building.type) {
    case BuildingType::Core:
        resource = &resources_.coreModel();
        modelScale = CoreModelScale;
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        break;
    case BuildingType::Wall: {
        constexpr std::uint8_t AllConnections =
            WallConnectionNorth | WallConnectionEast |
            WallConnectionSouth | WallConnectionWest;
        const std::uint8_t connectionMask =
            wallConnectionMask(
                buildings, building.gridPosition,
                building.baseHeight) &
            AllConnections;
        const int connectionCount =
            std::popcount(connectionMask);
        if (connectionCount == 0) {
            resource = &resources_.wallIsolatedModel();
            yaw = static_cast<float>(
                      wallFallbackRotation(
                          buildings, building)) *
                  QuarterTurn;
        } else if (connectionCount == 1) {
            resource = &resources_.wallEndModel();
            if ((connectionMask &
                 WallConnectionNorth) != 0U) {
                yaw = PI * 0.5F;
            } else if (
                (connectionMask &
                 WallConnectionWest) != 0U) {
                yaw = PI;
            } else if (
                (connectionMask &
                 WallConnectionSouth) != 0U) {
                yaw = -PI * 0.5F;
            }
        } else if (connectionCount == 2) {
            const bool northSouth =
                connectionMask ==
                (WallConnectionNorth |
                 WallConnectionSouth);
            const bool eastWest =
                connectionMask ==
                (WallConnectionEast |
                 WallConnectionWest);
            if (northSouth || eastWest) {
                resource =
                    &resources_.wallIsolatedModel();
                yaw = northSouth ? PI * 0.5F : 0.0F;
            } else {
                resource = &resources_.wallCornerModel();
                if (connectionMask ==
                    (WallConnectionNorth |
                     WallConnectionEast)) {
                    yaw = -PI * 0.5F;
                } else if (
                    connectionMask ==
                    (WallConnectionEast |
                     WallConnectionSouth)) {
                    yaw = PI;
                } else if (
                    connectionMask ==
                    (WallConnectionSouth |
                     WallConnectionWest)) {
                    yaw = PI * 0.5F;
                }
            }
        } else if (connectionCount == 3) {
            resource = &resources_.wallTModel();
            const std::uint8_t missing =
                AllConnections ^ connectionMask;
            if (missing == WallConnectionEast) {
                yaw = PI * 0.5F;
            } else if (missing ==
                       WallConnectionNorth) {
                yaw = PI;
            } else if (missing ==
                       WallConnectionWest) {
                yaw = -PI * 0.5F;
            }
        } else {
            resource = &resources_.wallCrossModel();
        }
        groundOffset = 0.005F;
        break;
    }
    case BuildingType::Turret:
        resource = &resources_.crossbowModel();
        modelScale = 1.0F;
        yaw = defensiveYaw;
        groundOffset = 0.0F;
        break;
    case BuildingType::GunTurret:
        resource = &resources_.gunTurretModel();
        modelScale = 1.0F;
        yaw = static_cast<float>(buildingRotationYaw(
            building.type, building.rotation));
        break;
    case BuildingType::CrystalMine:
        resource = &resources_.mineModel();
        modelScale = 2.1F;
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        horizontalScale = 0.5F;
        break;
    case BuildingType::LumberMill:
        resource = &resources_.lumberMillModel();
        modelScale = 1.65F;
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        horizontalScale = 0.5F;
        break;
    case BuildingType::Quarry:
        resource = &resources_.quarryModel();
        modelScale = 1.3F;
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        horizontalScale = 0.5F;
        break;
    case BuildingType::SpikeTrap:
        resource = &resources_.spikeTrapModel();
        modelScale = 1.2F;
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        break;
    case BuildingType::Cannon:
        resource = &resources_.cannonModel();
        modelScale = 1.0F;
        yaw = defensiveYaw;
        break;
    case BuildingType::Flamethrower:
        resource = &resources_.flamethrowerModel();
        modelScale = 1.0F;
        yaw = defensiveYaw;
        break;
    case BuildingType::Catapult:
        resource = &resources_.catapultModel();
        modelScale = 1.0F;
        yaw = defensiveYaw;
        break;
    case BuildingType::Gate:
        resource = &resources_.gateModel();
        yaw = static_cast<float>(building.rotation) *
              QuarterTurn;
        groundOffset = 0.005F;
        break;
    case BuildingType::SlowTrap:
    case BuildingType::WoodStorage:
    case BuildingType::StoneStorage:
    case BuildingType::CrystalStorage:
        break;
    }

    const auto acceptCollision =
        [maxDistance](RayCollision collision)
            -> std::optional<double> {
            if (!collision.hit || collision.distance < 0.0F ||
                static_cast<double>(collision.distance) > maxDistance) {
                return std::nullopt;
            }
            return static_cast<double>(collision.distance);
        };
    if (building.type == BuildingType::SpikeTrap) {
        // Aim at the whole occupied cell, including gaps between retracted
        // spikes. Use the same fixed volume as the simulation collider.
        const CollisionBox box = buildingCollisionBox(
            building.type, building.gridPosition, building.baseHeight);
        return acceptCollision(GetRayCollisionBox(ray, {
            {static_cast<float>(box.minX), static_cast<float>(box.minimumBlockingEyeY),
             static_cast<float>(box.minZ)},
            {static_cast<float>(box.maxX), static_cast<float>(box.maximumBlockingEyeY),
             static_cast<float>(box.maxZ)}}));
    }
    if (building.type == BuildingType::Gate &&
        resource != nullptr && resource->valid()) {
        position.y += groundOffset;
        const Matrix baseTransform = MatrixMultiply(
            resource->get().transform,
            MatrixMultiply(
                MatrixRotateY(yaw),
                MatrixTranslate(
                    position.x, position.y, position.z)));
        const Matrix doorTransform = MatrixMultiply(
            gate_model_detail::doorHingeTransform(
                building.open ? gateOpenAmount : 0.0F),
            baseTransform);
        std::optional<double> closest;
        for (const ModelCollider& collider :
             resource->collisionAsset().colliders) {
            const BoundingBox localBounds{
                {static_cast<float>(collider.minimum.x),
                 static_cast<float>(collider.minimum.y),
                 static_cast<float>(collider.minimum.z)},
                {static_cast<float>(collider.maximum.x),
                 static_cast<float>(collider.maximum.y),
                 static_cast<float>(collider.maximum.z)}};
            const bool door = collider.name.find("DOOR") !=
                std::string::npos;
            const BoundingBox worldBounds =
                world_transforms::transformBounds(
                    localBounds,
                    door ? doorTransform : baseTransform);
            const auto distance = acceptCollision(
                GetRayCollisionBox(ray, worldBounds));
            if (distance && (!closest || *distance < *closest)) {
                closest = distance;
            }
            // When open, keep an interaction-only target in the doorway.
            // It makes closing the gate convenient without changing world
            // collision or hiding the collider on the visible door.
            if (door && std::abs(gateOpenAmount) > 0.001F) {
                const BoundingBox closedWorldBounds =
                    world_transforms::transformBounds(
                        localBounds, baseTransform);
                const auto closedDistance = acceptCollision(
                    GetRayCollisionBox(ray, closedWorldBounds));
                if (closedDistance &&
                    (!closest || *closedDistance < *closest)) {
                    closest = closedDistance;
                }
            }
        }
        Model& model = resource->get();
        if (model.meshCount >= gate_model_detail::MeshCount) {
            gate_model_detail::forEachUpgradePart(building.level,
                [&](int meshIndex, bool door) {
                    const Matrix transform = door ? doorTransform : baseTransform;
                    const auto broad = acceptCollision(GetRayCollisionBox(ray,
                        world_transforms::transformBounds(
                            gate_model_detail::partBounds(meshIndex), transform)));
                    if (!broad || (closest && *broad >= *closest)) return;
                    const auto distance = acceptCollision(GetRayCollisionMesh(
                        ray, model.meshes[meshIndex], transform));
                    if (distance && (!closest || *distance < *closest)) closest = distance;
                });
        }
        return closest;
    }
    if (resource != nullptr && resource->valid()) {
        Model& model = resource->get();
        position.y += groundOffset;
        const Matrix scale = MatrixScale(
            modelScale * horizontalScale,
            modelScale * horizontalScale,
            modelScale * horizontalScale);
        const Matrix rotation = MatrixRotateY(yaw);
        const Matrix translation = MatrixTranslate(
            position.x, position.y, position.z);
        const Matrix baseTransform = MatrixMultiply(
            model.transform,
            MatrixMultiply(
                MatrixMultiply(scale, rotation),
                translation));
        if (building.type == BuildingType::Wall &&
            model.meshCount >= wall_upgrade_model_detail::MeshCount) {
            const auto bounds = resource->meshBounds();
            auto closest = acceptCollision(GetRayCollisionBox(ray,
                world_transforms::transformBounds(bounds[0], baseTransform)));
            const int upgrade = wall_upgrade_model_detail::upgradeMeshIndex(building.level);
            if (upgrade > 0) {
                const auto broad = acceptCollision(GetRayCollisionBox(ray,
                    world_transforms::transformBounds(bounds[static_cast<std::size_t>(upgrade)], baseTransform)));
                if (broad && (!closest || *broad < *closest)) {
                    const auto distance = acceptCollision(GetRayCollisionMesh(
                        ray, model.meshes[upgrade], baseTransform));
                    if (distance && (!closest || *distance < *closest)) closest = distance;
                }
            }
            return closest;
        }
        if ((building.type == BuildingType::CrystalMine && model.meshCount >= 2) ||
            (building.type == BuildingType::Quarry && model.meshCount >= quarry_model_detail::MeshCount)) {
            std::optional<double> closest;
            const auto testPart = [&](int index, Matrix pose) {
                if (index >= model.meshCount) return;
                const Matrix transform = MatrixMultiply(
                    pose, baseTransform);
                const auto distance = acceptCollision(
                    GetRayCollisionMesh(ray, model.meshes[index], transform));
                if (distance && (!closest || *distance < *closest)) closest = distance;
            };
            if (building.type == BuildingType::Quarry) quarry_model_detail::forEachLevelPart(building.level, testPart);
            else crystal_mine_model_detail::forEachLevelPart(building.level, testPart);
            return closest;
        }
        if (building.type == BuildingType::Core) {
            const BoundingBox bounds =
                world_transforms::transformBounds(
                    core_crystal_model_detail::levelBounds(resource->visualBounds(), building.level),
                    baseTransform);
            return acceptCollision(GetRayCollisionBox(ray, bounds));
        }
        if (building.type == BuildingType::GunTurret &&
            model.meshCount >= gun_turret_model_detail::MeshCount) {
            auto closest = modelColliderRaycastDistance(*resource, baseTransform, ray, maxDistance);
            const Matrix barrelTransform = MatrixMultiply(model.transform,
                MatrixMultiply(scale, MatrixMultiply(MatrixRotateY(defensiveYaw),
                    MatrixTranslate(position.x, position.y, position.z))));
            gun_turret_model_detail::forEachUpgradePart(building.level,
                [&](int meshIndex, Matrix pose, Color, bool rotating) {
                    const BoundingBox bounds = world_transforms::transformBounds(
                        gun_turret_model_detail::partBounds(meshIndex),
                        MatrixMultiply(pose, rotating ? barrelTransform : baseTransform));
                    const auto broadDistance = acceptCollision(GetRayCollisionBox(ray, bounds));
                    if (!broadDistance || (closest && *broadDistance >= *closest)) return;
                    const auto distance = acceptCollision(GetRayCollisionMesh(ray,
                        model.meshes[meshIndex],
                        MatrixMultiply(pose, rotating ? barrelTransform : baseTransform)));
                    if (distance && (!closest || *distance < *closest)) closest = distance;
                });
            return closest;
        }
        if (building.type == BuildingType::Cannon &&
            model.meshCount >= cannon_model_detail::MeshCount) {
            auto closest = modelColliderRaycastDistance(*resource, baseTransform, ray, maxDistance);
            const Matrix barrelTransform = MatrixMultiply(
                cannon_model_detail::pitchTransform(cannonPitchRadians), baseTransform);
            cannon_model_detail::forEachUpgradePart(building.level,
                [&](int meshIndex, bool pitched) {
                    const Matrix transform = pitched ? barrelTransform : baseTransform;
                    const auto broad = acceptCollision(GetRayCollisionBox(ray,
                        world_transforms::transformBounds(
                            cannon_model_detail::partBounds(meshIndex), transform)));
                    if (!broad || (closest && *broad >= *closest)) return;
                    const auto distance = acceptCollision(GetRayCollisionMesh(
                        ray, model.meshes[meshIndex], transform));
                    if (distance && (!closest || *distance < *closest)) closest = distance;
                });
            return closest;
        }
        if (building.type == BuildingType::Turret &&
            model.meshCount >= crossbow_model_detail::MeshCount) {
            auto closest = modelColliderRaycastDistance(*resource, baseTransform, ray, maxDistance);
            const Matrix bowTransform = MatrixMultiply(
                crossbow_model_detail::pitchTransform(cannonPitchRadians), baseTransform);
            crossbow_model_detail::forEachUpgradePart(building.level,
                [&](int meshIndex, bool pitched) {
                    const Matrix transform = pitched ? bowTransform : baseTransform;
                    const auto broad = acceptCollision(GetRayCollisionBox(ray,
                        world_transforms::transformBounds(
                            crossbow_model_detail::partBounds(meshIndex), transform)));
                    if (!broad || (closest && *broad >= *closest)) return;
                    const auto distance = acceptCollision(GetRayCollisionMesh(
                        ray, model.meshes[meshIndex], transform));
                    if (distance && (!closest || *distance < *closest)) closest = distance;
                });
            return closest;
        }
        if (building.type == BuildingType::Catapult &&
            model.meshCount >= catapult_model_detail::MeshCount) {
            auto closest = modelColliderRaycastDistance(*resource, baseTransform, ray, maxDistance);
            const Matrix armTransform = MatrixMultiply(
                catapult_model_detail::pitchTransform(cannonPitchRadians), baseTransform);
            catapult_model_detail::forEachUpgradePart(building.level,
                [&](int meshIndex, bool pitched) {
                    const Matrix transform = pitched ? armTransform : baseTransform;
                    const auto broad = acceptCollision(GetRayCollisionBox(ray,
                        world_transforms::transformBounds(
                            catapult_model_detail::partBounds(meshIndex), transform)));
                    if (!broad || (closest && *broad >= *closest)) return;
                    const auto distance = acceptCollision(GetRayCollisionMesh(
                        ray, model.meshes[meshIndex], transform));
                    if (distance && (!closest || *distance < *closest)) closest = distance;
                });
            return closest;
        }
        return modelColliderRaycastDistance(
            *resource, baseTransform, ray, maxDistance);
    }

    const auto collideBox =
        [&ray, &acceptCollision](
            Vector3 center, Vector3 size) {
            const Vector3 half{
                size.x * 0.5F, size.y * 0.5F,
                size.z * 0.5F};
            return acceptCollision(GetRayCollisionBox(
                ray,
                {{center.x - half.x,
                  center.y - half.y,
                  center.z - half.z},
                 {center.x + half.x,
                  center.y + half.y,
                  center.z + half.z}}));
        };
    std::optional<double> closest;
    const auto addBox =
        [&closest, &collideBox](
            Vector3 center, Vector3 size) {
            const auto distance = collideBox(center, size);
            if (distance &&
                (!closest || *distance < *closest)) {
                closest = distance;
            }
        };
    if (building.type == BuildingType::SlowTrap) {
        addBox(
            {position.x, 0.08F, position.z},
            {1.0F, 0.16F, 1.0F});
    } else if (building.type == BuildingType::Gate) {
        if ((building.rotation % 2U) == 0U) {
            addBox(
                {position.x - 0.38F, 1.0F, position.z},
                {0.22F, 2.0F, 1.0F});
            addBox(
                {position.x + 0.38F, 1.0F, position.z},
                {0.22F, 2.0F, 1.0F});
            if (!building.open) {
                addBox(
                    {position.x, 1.0F, position.z},
                    {0.55F, 1.7F, 0.18F});
            }
        } else {
            addBox(
                {position.x, 1.0F,
                 position.z - 0.38F},
                {1.0F, 2.0F, 0.22F});
            addBox(
                {position.x, 1.0F,
                 position.z + 0.38F},
                {1.0F, 2.0F, 0.22F});
            if (!building.open) {
                addBox(
                    {position.x, 1.0F, position.z},
                    {0.18F, 1.7F, 0.55F});
            }
        }
    } else {
        Vector3 size{1.6F, 1.8F, 1.6F};
        if (building.type == BuildingType::Core) {
            size = {1.8F, 2.6F, 1.8F};
        } else if (
            building.type == BuildingType::Wall) {
            size = {0.84F, 2.05F, 0.84F};
        } else if (
            building.type == BuildingType::CrystalMine ||
            building.type == BuildingType::LumberMill ||
            building.type == BuildingType::Quarry) {
            size = {0.9F, 0.8F, 0.9F};
        }
        addBox(
            {position.x, size.y * 0.5F, position.z},
            size);
    }
    return closest;
}

std::optional<double> Renderer::resourceRaycastDistance(
    ResourceType type, Vector3 position,
    Ray ray, double maxDistance,
    std::size_t visualVariant,
    float visualScale, float yawRadians) {
    ModelResource& resource = type == ResourceType::Wood
        ? resources_.treeModel(visualVariant)
        : type == ResourceType::Stone
            ? resources_.rockModel(visualVariant)
            : type == ResourceType::Crystal
                ? resources_.crystalResourceModel()
            : resources_.destructiblePropModel(propModelIndex(type));
    if (!resource.valid() || maxDistance <= 0.0) {
        return std::nullopt;
    }
    const float modelScale = type == ResourceType::Wood
        ? TreeModelScale * visualScale
        : type == ResourceType::Stone
            ? RockModelScale
            : type == ResourceType::Crystal
                ? static_cast<float>(CrystalVisualModelScale) * visualScale
                : visualScale;
    position.y += type == ResourceType::Wood
        ? static_cast<float>(TreeVisualGroundOffsets[
              visualVariant % TreeVisualVariantCount] * visualScale *
              TreeVisualHeightScale)
        : type == ResourceType::Stone
            ? rockGroundOffset(visualVariant)
            : type == ResourceType::Crystal
                ? static_cast<float>(CrystalVisualGroundOffset) *
                      modelScale
            : 0.0F;
    const Matrix rotation = isDestructibleProp(type)
        ? terrainAlignedRotation(position.x, position.z, yawRadians)
        : MatrixRotateY(yawRadians);
    const float modelScaleY = type == ResourceType::Wood
        ? modelScale * TreeVisualHeightScale
        : modelScale;
    const Matrix transform = MatrixMultiply(
        resource.get().transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(modelScale, modelScaleY, modelScale),
                rotation),
            MatrixTranslate(position.x, position.y, position.z)));
    return modelColliderRaycastDistance(
        resource, transform, ray, maxDistance);
}

std::optional<double> Renderer::worldLandmarkRaycastDistance(
    std::size_t variant, Vector3 position,
    float yawRadians, Ray ray, double maxDistance) {
    ModelResource& resource = resources_.worldLandmarkModel(variant);
    if (!resource.valid() || maxDistance <= 0.0) return std::nullopt;
    const BoundingBox bounds = resource.visualBounds();
    const float authoredHeight = std::max(
        0.001F, bounds.max.y - bounds.min.y);
    constexpr float TargetHeight = 6.76F;
    const float modelScale = TargetHeight / authoredHeight;
    position.y -= bounds.min.y * modelScale;
    const Matrix transform = MatrixMultiply(
        resource.get().transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(modelScale, modelScale, modelScale),
                MatrixRotateY(yawRadians)),
            MatrixTranslate(position.x, position.y, position.z)));
    return modelColliderRaycastDistance(
        resource, transform, ray, maxDistance);
}


} // namespace ian
