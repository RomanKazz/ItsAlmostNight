#include "enemies/EnemySystem.hpp"

#include "core/Geometry.hpp"
#include "world/CollisionWorld.hpp"
#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

std::optional<double> segmentBoxFraction(Vec3 start, Vec3 end,
    const EnemyProjectileBlocker& box, double radius) {
    double near=0, far=1;
    const auto slab=[&](double origin,double delta,double minimum,double maximum) {
        minimum-=radius; maximum+=radius;
        if (std::abs(delta)<1e-12) return origin>=minimum && origin<=maximum;
        double a=(minimum-origin)/delta,b=(maximum-origin)/delta;
        if(a>b) std::swap(a,b);
        near=std::max(near,a); far=std::min(far,b);
        return near<=far;
    };
    if(!slab(start.x,end.x-start.x,box.minimum.x,box.maximum.x) ||
       !slab(start.y,end.y-start.y,box.minimum.y,box.maximum.y) ||
       !slab(start.z,end.z-start.z,box.minimum.z,box.maximum.z)) return std::nullopt;
    return near;
}

double pointSegmentDistanceSquared(
    Vec3 point, Vec3 start, Vec3 end) {
    const Vec3 segment{
        end.x - start.x,
        end.y - start.y,
        end.z - start.z};
    const Vec3 offset{
        point.x - start.x,
        point.y - start.y,
        point.z - start.z};
    const double lengthSquared =
        segment.x * segment.x + segment.y * segment.y +
        segment.z * segment.z;
    const double projection = lengthSquared > 1e-12
        ? std::clamp(
              (offset.x * segment.x + offset.y * segment.y +
               offset.z * segment.z) / lengthSquared,
              0.0, 1.0)
        : 0.0;
    const double dx = offset.x - segment.x * projection;
    const double dy = offset.y - segment.y * projection;
    const double dz = offset.z - segment.z * projection;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace

void EnemySystem::trackPlayerVelocity(
    double deltaSeconds, std::optional<Vec3> playerPosition) {
    if (playerPosition) {
        if (previousPlayerPosition_ && deltaSeconds > 1e-6) {
            const double rawX =
                (playerPosition->x - previousPlayerPosition_->x) /
                deltaSeconds;
            const double rawZ =
                (playerPosition->z - previousPlayerPosition_->z) /
                deltaSeconds;
            constexpr double MaximumTrackedPlayerSpeed = 12.0;
            const double rawSpeed = std::hypot(rawX, rawZ);
            const double clampScale = rawSpeed > MaximumTrackedPlayerSpeed
                ? MaximumTrackedPlayerSpeed / rawSpeed
                : 1.0;
            const double response = std::clamp(
                deltaSeconds * 10.0, 0.0, 1.0);
            estimatedPlayerVelocity_.x +=
                (rawX * clampScale - estimatedPlayerVelocity_.x) *
                response;
            estimatedPlayerVelocity_.z +=
                (rawZ * clampScale - estimatedPlayerVelocity_.z) *
                response;
        } else {
            estimatedPlayerVelocity_ = {};
        }
        previousPlayerPosition_ = *playerPosition;
    } else {
        previousPlayerPosition_.reset();
        estimatedPlayerVelocity_ = {};
    }
}

void EnemySystem::updateProjectiles(
    double deltaSeconds,
    const std::vector<BuildingInstance>& buildings,
    std::optional<Vec3> playerPosition,
    std::span<const EnemyStructureTarget> additionalStructures,
    const TerrainHeightfield* terrain) {
    for (EnemyProjectile& projectile : projectiles_) {
        if (!projectile.active) continue;
        const Vec3 previousPosition = projectile.position;
        projectile.previousPosition = previousPosition;
        projectile.position.x += projectile.velocity.x * deltaSeconds;
        projectile.position.y += projectile.velocity.y * deltaSeconds;
        projectile.position.z += projectile.velocity.z * deltaSeconds;
        projectile.lifetimeRemaining -= deltaSeconds;
        if (projectile.lifetimeRemaining <= 0.0) {
            projectile.active = false;
            magicEventBuffer_.push_back(
                {projectile.ownerId, projectile.position, true, true, projectile.royal});
            continue;
        }
        if (projectile.targetsPlayer) {
            if (playerPosition) {
                const Vec3 playerCenter{
                    playerPosition->x, playerPosition->y - 0.62,
                    playerPosition->z};
                constexpr double PlayerHitRadius = 0.42;
                const double hitRadius =
                    projectile.radius + PlayerHitRadius;
                if (pointSegmentDistanceSquared(
                        playerCenter, previousPosition,
                        projectile.position) <=
                    hitRadius * hitRadius) {
                    playerAttackBuffer_.push_back(
                        {projectile.ownerId, projectile.damage});
                    magicEventBuffer_.push_back(
                        {projectile.ownerId, projectile.position, true, false, projectile.royal});
                    projectile.active = false;
                }
            }
        } else if (projectile.targetId) {
            const bool targetExists =
                std::ranges::any_of(
                    buildings, [&projectile](const BuildingInstance& building) {
                        return building.id == *projectile.targetId;
                    }) ||
                std::ranges::any_of(
                    additionalStructures,
                    [&projectile](const EnemyStructureTarget& structure) {
                        return structure.id == *projectile.targetId;
                    });
            if (!targetExists) {
                projectile.active = false;
            } else {
                if (projectile.royal) {
                    // Swept collision chooses the first wall or target, including
                    // on coarse ticks. Walls behind the Core cannot intercept it.
                    double first=2;
                    std::optional<EntityId> hit;
                    const auto segment=geometry::subtract(projectile.position,previousPosition);
                    const double length=geometry::length(segment);
                    const double radius=projectile.radius+projectile.targetRadius;
                    if(geometry::distanceSquared(previousPosition,projectile.targetPosition)<=radius*radius) {
                        first=0;hit=projectile.targetId;
                    } else if(length>1e-12) {
                        const auto distance=geometry::raySphereDistance(previousPosition,
                            geometry::scale(segment,1/length),projectile.targetPosition,radius);
                        if(distance && *distance<=length) {first=*distance/length;hit=projectile.targetId;}
                    }
                    const auto consider=[&](EntityId id,const EnemyProjectileBlocker& box) {
                        const auto fraction=segmentBoxFraction(previousPosition,projectile.position,box,projectile.radius);
                        if(fraction && *fraction<first) {first=*fraction;hit=id;}
                    };
                    for(const auto& building:buildings) {
                        if(building.id==*projectile.targetId ||
                           (building.type!=BuildingType::Wall && building.type!=BuildingType::Gate) ||
                           !buildingBlocksMovement(building)) continue;
                        const auto box=buildingCollisionBox(building.type,building.gridPosition,building.baseHeight);
                        consider(building.id,{{box.minX,box.minimumBlockingEyeY,box.minZ},
                            {box.maxX,box.maximumBlockingEyeY,box.maxZ}});
                    }
                    for(const auto& structure:additionalStructures)
                        if(structure.attackable && structure.projectileBlocker)
                            consider(structure.id,*structure.projectileBlocker);
                    if(hit) {
                        projectile.position=geometry::lerpClamped(previousPosition,projectile.position,first);
                        attackBuffer_.push_back({projectile.ownerId,*hit,projectile.damage,false});
                        magicEventBuffer_.push_back({projectile.ownerId,projectile.position,true,false,true});
                        projectile.active=false;
                    }
                    // Royal shots never use the old target-only collision below.
                    if (!projectile.active) continue;
                } else {
                    const double hitRadius =
                        projectile.radius + projectile.targetRadius;
                    if (pointSegmentDistanceSquared(
                            projectile.targetPosition, previousPosition,
                            projectile.position) <=
                        hitRadius * hitRadius) {
                        attackBuffer_.push_back({
                            projectile.ownerId, *projectile.targetId,
                            projectile.damage, false});
                        magicEventBuffer_.push_back(
                            {projectile.ownerId, projectile.position, true, false, projectile.royal});
                        projectile.active = false;
                    }
                }
            }
        }
        if (projectile.active && terrain &&
            projectile.position.y <=
                terrain->getHeight(
                    projectile.position.x,
                    projectile.position.z) + 0.12) {
            projectile.position.y = terrain->getHeight(
                projectile.position.x, projectile.position.z) + 0.12;
            magicEventBuffer_.push_back(
                {projectile.ownerId, projectile.position, true, false, projectile.royal});
            projectile.active = false;
        }
    }
    std::erase_if(projectiles_, [](const EnemyProjectile& projectile) {
        return !projectile.active;
    });
}

} // namespace ian
