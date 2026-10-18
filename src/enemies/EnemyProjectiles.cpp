#include "enemies/EnemySystem.hpp"

#include "core/Geometry.hpp"
#include "world/CollisionWorld.hpp"
#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

void EnemySystem::tickBee(EnemyInstance& enemy, double dt, std::optional<Vec3> playerPosition) {
    constexpr double Range=6.5, Interval=2.8, Speed=7.0, Windup=.75;
    const bool elite=enemy.eliteAffixes!=0;
    if(enemy.spawnAnimationRemaining>0) return;
    std::optional<EntityId> targetId;
    Vec3 aim{};
    bool targetsPlayer=false;
    double nearest=std::numeric_limits<double>::infinity();
    for(const auto& structure:structureBuffer_) {
        if(!structure.attackable || structure.traversable) continue;
        // Decoys keep their normal priority; otherwise favor weapons and the
        // Core over the outer wall. Stingers can still be intercepted by walls.
        if(enemy.decoyTarget && enemy.decoyRemaining>0 && structure.id!=*enemy.decoyTarget) continue;
        if(!enemy.decoyTarget && structure.buildingType &&
            (*structure.buildingType==BuildingType::Wall || *structure.buildingType==BuildingType::Gate)) continue;
        const double distance=std::hypot(structure.position.x-enemy.position.x,structure.position.z-enemy.position.z);
        if(distance>=nearest) continue;
        nearest=distance;targetId=structure.id;aim=structure.position;
        aim.y=structure.attackSurfaceHeight.value_or(structure.position.y)+.9;
    }
    if(playerPosition && !enemy.decoyTarget) {
        const double distance=std::hypot(playerPosition->x-enemy.position.x,playerPosition->z-enemy.position.z);
        if(distance<5 && distance<nearest) {
            nearest=distance;targetId.reset();targetsPlayer=true;
            aim={playerPosition->x,playerPosition->y-.62,playerPosition->z};
        }
    }
    if(!std::isfinite(nearest)) {enemy.beeWindupRemaining=0;enemy.target.reset();return;}
    const Vec3 origin{enemy.position.x,enemy.worldSurfaceHeight+2.10,enemy.position.z};
    const Vec3 facing=enemy.beeWindupRemaining>0?enemy.beeAim:aim;
    const double yaw=std::atan2(facing.x-origin.x,facing.z-origin.z);
    const double angle=std::atan2(std::sin(yaw-enemy.yaw),std::cos(yaw-enemy.yaw));
    enemy.yaw+=std::clamp(angle,-enemy.turnRate*dt,enemy.turnRate*dt);
    if(enemy.beeWindupRemaining>0) {
        // Lock the aim when charging starts. No homing or instant hits: a
        // strafing player can dodge and walls can catch the physical stingers.
        if(targetId!=enemy.target || targetsPlayer!=enemy.beeTargetsPlayer || nearest>Range+1.5) {
            enemy.beeWindupRemaining=0;enemy.attackCooldownRemaining=.4;return;
        }
        enemy.beeWindupRemaining=std::max(0.0,enemy.beeWindupRemaining-dt);
        enemy.state=targetsPlayer?EnemyState::AttackPlayer:EnemyState::AttackBuilding;
        if(enemy.beeWindupRemaining>0) return;
        const Vec3 forward=geometry::normalizedOr(geometry::subtract(enemy.beeAim,origin));
        const Vec3 muzzle=geometry::add(origin,geometry::scale(forward,.4));
        const int shots=elite?3:1;
        for(int index=0;index<shots;++index) {
            const double spread=elite?(index-1)*.18:0;
            const Vec3 direction{forward.x*std::cos(spread)-forward.z*std::sin(spread),
                forward.y,forward.x*std::sin(spread)+forward.z*std::cos(spread)};
            projectiles_.push_back({.id={nextProjectileIndex_++,1},.ownerId=enemy.id,
                .targetId=enemy.target,.position=muzzle,.targetPosition=enemy.beeAim,.previousPosition=muzzle,
                .velocity=geometry::scale(direction,Speed),.damage=enemy.damage*(elite?.50:1.0),
                .radius=.13,.targetRadius=.4,.lifetimeRemaining=2.2,
                .targetsPlayer=targetsPlayer,.stinger=true,.duration=2.2});
        }
        magicEventBuffer_.push_back({enemy.id,muzzle,false,false,false,true});
        enemy.attackCooldownRemaining=Interval;enemy.beeRepositionRemaining=.7;
        return;
    }
    enemy.target=targetId;enemy.beeTargetsPlayer=targetsPlayer;
    if(enemy.beeRepositionRemaining>0 || nearest>Range || nearest<3.2) {
        enemy.beeRepositionRemaining=std::max(0.0,enemy.beeRepositionRemaining-dt);
        const double x=aim.x-enemy.position.x,z=aim.z-enemy.position.z;
        const double length=std::max(.001,std::hypot(x,z));
        const double side=enemy.beeRepositionRemaining>0?(enemy.id.index%2?1.0:-1.0):0;
        const double approach=nearest>Range?1.0:nearest<3.2?-1.0:0;
        const auto direction=geometry::normalizedOr(Vec3{x/length*approach-z/length*side,0,z/length*approach+x/length*side});
        enemy.position.x+=direction.x*enemy.speed*enemy.movementMultiplier*dt;
        enemy.position.z+=direction.z*enemy.speed*enemy.movementMultiplier*dt;
        enemy.state=EnemyState::MoveToCore;
        return;
    }
    enemy.state=EnemyState::AttackBuilding;
    if(enemy.attackCooldownRemaining<=0 && std::abs(angle)<.18) {
        enemy.beeAim=aim;enemy.beeWindupRemaining=elite?1.0:Windup;
        enemy.state=targetsPlayer?EnemyState::AttackPlayer:EnemyState::AttackBuilding;
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
                {projectile.ownerId, projectile.position, true, true, projectile.royal,projectile.stinger});
            continue;
        }
        if(projectile.stinger && projectile.targetsPlayer) {
            double first=2;std::optional<EntityId> intercepted;
            const auto consider=[&](EntityId id,const EnemyProjectileBlocker& box) {
                const auto fraction=segmentBoxFraction(previousPosition,projectile.position,box,projectile.radius);
                if(fraction && *fraction<first){first=*fraction;intercepted=id;}
            };
            for(const auto& building:buildings) {
                if((building.type!=BuildingType::Wall && building.type!=BuildingType::Gate) || !buildingBlocksMovement(building)) continue;
                const auto box=buildingCollisionBox(building.type,building.gridPosition,building.baseHeight);
                consider(building.id,{{box.minX,box.minimumBlockingEyeY,box.minZ},{box.maxX,box.maximumBlockingEyeY,box.maxZ}});
            }
            for(const auto& structure:additionalStructures)if(structure.projectileBlocker)consider(structure.id,*structure.projectileBlocker);
            if(intercepted) {
                projectile.position=geometry::lerpClamped(previousPosition,projectile.position,first);
                attackBuffer_.push_back({projectile.ownerId,*intercepted,projectile.damage,false});
                magicEventBuffer_.push_back({projectile.ownerId,projectile.position,true,false,false,true});
                projectile.active=false;continue;
            }
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
                        {projectile.ownerId, projectile.position, true, false, projectile.royal,projectile.stinger});
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
                if (projectile.royal || projectile.stinger) {
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
                        magicEventBuffer_.push_back({projectile.ownerId,projectile.position,true,false,projectile.royal,projectile.stinger});
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
                            {projectile.ownerId, projectile.position, true, false, projectile.royal,projectile.stinger});
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
                {projectile.ownerId, projectile.position, true, false, projectile.royal,projectile.stinger});
            projectile.active = false;
        }
    }
    std::erase_if(projectiles_, [](const EnemyProjectile& projectile) {
        return !projectile.active;
    });
}

} // namespace ian
