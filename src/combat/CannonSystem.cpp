#include "combat/CannonSystem.hpp"
#include "combat/MortarAttack.hpp"
#include "combat/DefenseRuntimeSupport.hpp"
#include "combat/DefenseAiming.hpp"
#include "buildings/BuildingModifiers.hpp"
#include "buildings/CannonRig.hpp"
#include "buildings/CatapultRig.hpp"
#include "buildings/BuildingOrientation.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

constexpr double BaseRange = 12.0;
constexpr double ClusterRadius = 3.0;
constexpr double BaseFireInterval = 3.2;
constexpr double SearchInterval = 0.5;
constexpr double TurnSpeed = 12.0;
constexpr double PitchSpeed = 8.0;
constexpr double AimTolerance = 0.08726646259971647;
constexpr double MinimumPitch = -0.2617993877991494;
constexpr double MaximumPitch = 0.9599310885968813;
constexpr double ProjectileSpeed = 12.0;
constexpr double Gravity = 9.8;
constexpr double BaseExplosionRadius = 2.5;
constexpr double BaseExplosionDamage = 2.5;
constexpr double BaseExplosionImpulse = 3.5;
constexpr double CatapultRange = 15.0;
constexpr double CatapultMinimumRange = 4.25;
constexpr double CatapultFireInterval = 4.2;
constexpr double CatapultProjectileSpeed = 9.0;
constexpr double CatapultExplosionRadius = 3.1;
constexpr double CatapultExplosionDamage = 4.0;
constexpr double CatapultAnimationDuration = 0.95;
constexpr double CatapultReleaseTime = 0.34;
constexpr double CatapultWindupTime = 0.12;
constexpr double CatapultFollowThroughTime = 0.43;
constexpr double CatapultWindupPitch = 0.18;
constexpr double CatapultFollowThroughPitch = -1.20;
constexpr std::size_t MaxProjectiles = 300;

[[nodiscard]] double catapultArmPitch(double elapsed) {
    // Each phase joins at rest with zero acceleration. Release happens during
    // the fast throw, before follow-through, rather than at a stopped arm.
    const auto ease = [](double value) {
        value = std::clamp(value, 0.0, 1.0);
        return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
    };
    if (elapsed <= CatapultWindupTime) {
        return CatapultWindupPitch * ease(elapsed / CatapultWindupTime);
    }
    if (elapsed <= CatapultFollowThroughTime) {
        return CatapultWindupPitch +
            (CatapultFollowThroughPitch - CatapultWindupPitch) * ease(
                (elapsed - CatapultWindupTime) /
                (CatapultFollowThroughTime - CatapultWindupTime));
    }
    return CatapultFollowThroughPitch * (1.0 - ease(
        (elapsed - CatapultFollowThroughTime) /
        (CatapultAnimationDuration - CatapultFollowThroughTime)));
}

} // namespace

CannonSystem::CannonSystem() {
    cannons_.reserve(64);
    projectiles_.reserve(MaxProjectiles);
    explosionBuffer_.reserve(32);
    shotBuffer_.reserve(32);
}

double CannonSystem::attackRange(std::uint8_t level) {
    return BaseRange +
           0.35 * static_cast<double>(level - 1);
}

double CannonSystem::attackRange(
    BuildingType type, std::uint8_t level) {
    if (type == BuildingType::Mortar) return 18.0 + 0.5 * (std::clamp<int>(level,1,8)-1);
    if (type == BuildingType::Catapult) {
        return CatapultRange + 0.45 * static_cast<double>(level - 1);
    }
    return attackRange(level);
}

double CannonSystem::minimumRange(
    BuildingType type, std::uint8_t level) {
    if (type == BuildingType::Mortar) return 4.0;
    if (type != BuildingType::Catapult) return 0.0;
    return std::max(
        3.25, CatapultMinimumRange -
            0.08 * static_cast<double>(level - 1));
}

double CannonSystem::fireInterval(std::uint8_t level) {
    const double levelBonus = static_cast<double>(level - 1);
    return BaseFireInterval / (1.0 + 0.05 * levelBonus);
}

double CannonSystem::fireInterval(
    BuildingType type, std::uint8_t level) {
    if (type == BuildingType::Mortar) return 4.0 / (1.0 + 0.06 * (std::clamp<int>(level,1,8)-1));
    if (type == BuildingType::Catapult) {
        return CatapultFireInterval /
            (1.0 + 0.045 * static_cast<double>(level - 1));
    }
    return fireInterval(level);
}

double CannonSystem::explosionRadius(std::uint8_t level) {
    return BaseExplosionRadius +
           0.12 * static_cast<double>(level - 1);
}

double CannonSystem::explosionRadius(
    BuildingType type, std::uint8_t level) {
    if (type == BuildingType::Mortar) return 3.25 + 0.12 * (std::clamp<int>(level,1,8)-1);
    if (type == BuildingType::Catapult) {
        return CatapultExplosionRadius +
            0.15 * static_cast<double>(level - 1);
    }
    return explosionRadius(level);
}

double CannonSystem::explosionDamage(std::uint8_t level) {
    return BaseExplosionDamage +
           0.35 * static_cast<double>(level - 1);
}

double CannonSystem::explosionDamage(
    BuildingType type, std::uint8_t level) {
    if (type == BuildingType::Mortar) return 6.0 + 1.0 * (std::clamp<int>(level,1,8)-1);
    if (type == BuildingType::Catapult) {
        return CatapultExplosionDamage +
            0.45 * static_cast<double>(level - 1);
    }
    return explosionDamage(level);
}

void CannonSystem::reset() {
    setAttackStyle(CannonAttackStyle::Standard);
    setTypeBonuses(BuildingType::Core, 1.0, 1.0);
    cannons_.clear();
    projectiles_.clear();
    explosionBuffer_.clear();
    shotBuffer_.clear();
    hitBuffer_.clear();
}

void CannonSystem::setAttackStyle(CannonAttackStyle style) {
    attackStyle_ = style;
}

void CannonSystem::setTypeBonuses(BuildingType type, double damage, double fireRate) {
    bonusType_ = type;
    typeDamageMultiplier_ = std::max(0.05, damage);
    typeFireRateMultiplier_ = std::max(0.05, fireRate);
}

void CannonSystem::setModifiers(
    double damage, double radius, double fireRate,
    double highGroundDamage, int clusterExplosionCount) {
    damageMultiplier_ = std::max(0.05, damage);
    radiusMultiplier_ = std::max(0.05, radius);
    fireRateMultiplier_ = std::max(0.05, fireRate);
    highGroundDamageMultiplier_ = std::max(1.0, highGroundDamage);
    clusterExplosionCount_ = std::clamp(
        clusterExplosionCount, 0, 4);
}

void CannonSystem::clearProjectiles() {
    for (auto& projectile : projectiles_) {
        projectile.active = false;
    }
}

void CannonSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    defense_runtime::syncBuildings(cannons_, buildings, SearchInterval,
        [](BuildingType type) {
            return type == BuildingType::Cannon || type == BuildingType::Catapult || type == BuildingType::Mortar;
        },
        [](const BuildingInstance& building, const CannonRuntime& runtime) {
            return building.type == runtime.type;
        });
    for (auto& cannon : cannons_) {
        // Newly registered rotated defenses must not interpolate from zero.
        cannon.previousYaw = cannon.yaw;
        cannon.previousPitch = cannon.pitch;
    }
}

std::span<const CannonExplosion> CannonSystem::tick(
    double deltaSeconds, const std::vector<BuildingInstance>& buildings, EnemySystem& enemies) {
    explosionBuffer_.clear();
    shotBuffer_.clear();
    hitBuffer_.clear();
    for (auto& cannon : cannons_) {
        const auto building =
            std::find_if(buildings.begin(), buildings.end(), [&cannon](const BuildingInstance& item) {
                return item.id == cannon.buildingId;
            });
        if (building == buildings.end()) {
            continue;
        }

        cannon.previousYaw = cannon.yaw;
        cannon.previousPitch = cannon.pitch;
        cannon.fireCooldownRemaining = std::max(
            0.0, cannon.fireCooldownRemaining - deltaSeconds);
        cannon.targetSearchCooldownRemaining =
            std::max(0.0, cannon.targetSearchCooldownRemaining - deltaSeconds);
        const Vec3 targetingOrigin = buildingWorldPosition(*building);
        const double range = attackRange(building->type, building->level);
        const double deadZone = minimumRange(
            building->type, building->level);
        const auto profile = cannonAttackProfile(building->type == BuildingType::Cannon
            ? attackStyle_ : CannonAttackStyle::Standard);
        const double shotInterval = profile.interval * fireInterval(
            building->type, building->level) /
            (fireRateMultiplier_ * (building->type == bonusType_ ? typeFireRateMultiplier_ : 1.0));
        cannon.baseYaw = smoothBuildingAngle(
            cannon.baseYaw, cannon.restYaw, deltaSeconds);

        if (building->type == BuildingType::Catapult &&
            cannon.firingAnimationRemaining > 0.0) {
            const double previousElapsed = CatapultAnimationDuration -
                cannon.firingAnimationRemaining;
            cannon.firingAnimationRemaining = std::max(
                0.0, cannon.firingAnimationRemaining - deltaSeconds);
            const double elapsed = CatapultAnimationDuration -
                cannon.firingAnimationRemaining;
            cannon.pitch = catapultArmPitch(elapsed);
            if (cannon.pendingTargetPosition && cannon.targetId) {
                cannon.manualTurnPending = false;
                const Vec3& target = *cannon.pendingTargetPosition;
                const double desiredYaw = std::atan2(
                    targetingOrigin.x - target.x, targetingOrigin.z - target.z);
                cannon.yaw = advanceDefenseAim(
                    cannon.yaw, desiredYaw, cannon.yawVelocity,
                    deltaSeconds, 12.0, TurnSpeed, true);
            } else {
                defense_runtime::holdAimWithoutTarget(cannon, deltaSeconds);
            }
            if (previousElapsed < CatapultReleaseTime &&
                elapsed >= CatapultReleaseTime &&
                cannon.pendingTargetPosition) {
                launch(*building, *cannon.pendingTargetPosition,
                       cannon.yaw, catapultArmPitch(CatapultReleaseTime));
                cannon.loaded = false;
                cannon.fireCooldownRemaining = shotInterval;
            }
            if (cannon.firingAnimationRemaining <= 0.0) {
                cannon.pitch = 0.0;
                cannon.loaded = true;
                cannon.pendingTargetPosition.reset();
            }
            continue;
        }

        if (cannon.targetId) {
            const auto target = enemies.enemy(*cannon.targetId);
            if (!target) {
                cannon.targetId.reset();
            } else {
                const double deltaX =
                    target->position.x - targetingOrigin.x;
                const double deltaZ =
                    target->position.z - targetingOrigin.z;
                const double distanceSquared =
                    deltaX * deltaX + deltaZ * deltaZ;
                if (distanceSquared > range * range ||
                    distanceSquared < deadZone * deadZone ||
                    !directionInsideDefenseArc(
                        building->type, targetingOrigin, target->position, cannon.restYaw,
                        building->level)) {
                    cannon.targetId.reset();
                }
            }
        }
        if (!cannon.targetId && cannon.targetSearchCooldownRemaining <= 0.0) {
            cannon.targetId = enemies.densestEnemyInArc(
                targetingOrigin, range, ClusterRadius, cannon.restYaw,
                defenseAttackHalfAngleRadians(building->type, building->level), deadZone);
            cannon.targetSearchCooldownRemaining = SearchInterval;
        }

        double desiredYaw = cannon.restYaw;
        double desiredPitch = 0.0;
        if (cannon.targetId) {
            const auto target = enemies.enemy(*cannon.targetId);
            if (target) {
                const Vec3 origin = building->type == BuildingType::Catapult
                    ? targetingOrigin
                    : building->type == BuildingType::Mortar
                    ? mortarMuzzleWorldPosition(targetingOrigin,cannon.yaw,cannon.pitch)
                    : cannonMuzzleWorldPosition(
                        targetingOrigin, cannon.yaw, cannon.pitch);
                const double deltaX = target->position.x - origin.x;
                const double deltaZ = target->position.z - origin.z;
                desiredYaw = std::atan2(-deltaX, -deltaZ);
                if (building->type == BuildingType::Mortar) {
                    Vec3 impact=target->position;impact.y+=target->worldSurfaceHeight;
                    const auto trajectory=mortarTrajectory(origin,impact,MortarHorizontalSpeed);
                    if (!trajectory) {cannon.targetId.reset();continue;}
                    desiredPitch=trajectory->pitch;
                } else if (building->type == BuildingType::Cannon) {
                    const double horizontalDistance = std::hypot(deltaX, deltaZ);
                    const double flightTime = std::max(
                        0.2, horizontalDistance / ProjectileSpeed);
                    const double verticalSpeed =
                        (target->position.y - origin.y +
                         0.5 * Gravity * flightTime * flightTime) / flightTime;
                    desiredPitch = std::clamp(
                        std::atan2(verticalSpeed, ProjectileSpeed),
                        MinimumPitch, MaximumPitch);
                }
            } else {
                cannon.targetId.reset();
            }
        }
        if (cannon.targetId) {
            cannon.manualTurnPending = false;
            cannon.yaw = advanceDefenseAim(
                cannon.yaw, desiredYaw, cannon.yawVelocity,
                deltaSeconds, 12.0, TurnSpeed, true);
        } else {
            defense_runtime::holdAimWithoutTarget(cannon, deltaSeconds);
        }
        if ((building->type == BuildingType::Cannon || building->type == BuildingType::Mortar) && cannon.targetId) {
            cannon.pitch = advanceDefenseAim(
                cannon.pitch, desiredPitch, cannon.pitchVelocity,
                deltaSeconds, 10.0, PitchSpeed, false);
            const double limitedPitch = std::clamp(cannon.pitch, building->type == BuildingType::Mortar ? MortarMinimumPitch : MinimumPitch,
                building->type == BuildingType::Mortar ? MortarMaximumPitch : MaximumPitch);
            if (limitedPitch != cannon.pitch) cannon.pitchVelocity = 0.0;
            cannon.pitch = limitedPitch;
        }
        const bool aimedAtTarget = cannon.targetId &&
            std::abs(wrapBuildingAngle(desiredYaw - cannon.yaw)) <= AimTolerance &&
            std::abs(desiredPitch - cannon.pitch) <= AimTolerance;
        if (aimedAtTarget && cannon.fireCooldownRemaining <= 0.0) {
            const auto target = enemies.enemy(*cannon.targetId);
            if (target) {
                if (building->type == BuildingType::Catapult) {
                    cannon.pendingTargetPosition = target->position;
                    cannon.firingAnimationRemaining =
                        CatapultAnimationDuration;
                    cannon.loaded = true;
                } else {
                    launch(*building, target->position,
                           cannon.yaw, cannon.pitch);
                    cannon.fireCooldownRemaining = shotInterval;
                }
            }
        }
    }

    for (auto& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }
        projectile.fuseRemaining -= deltaSeconds;
        projectile.velocity.y -= Gravity * deltaSeconds;
        projectile.position.x += projectile.velocity.x * deltaSeconds;
        projectile.position.y += projectile.velocity.y * deltaSeconds;
        projectile.position.z += projectile.velocity.z * deltaSeconds;
        projectile.trailHead = (projectile.trailHead + 1U) %
            CannonProjectile::TrailCapacity;
        projectile.trailPositions[projectile.trailHead] = projectile.position;
        projectile.trailPointCount = std::min(
            projectile.trailPointCount + 1U, CannonProjectile::TrailCapacity);
        if (projectile.fuseRemaining <= 0.0) {
            projectile.position = projectile.targetPosition;
            explode(projectile, enemies);
        }
    }
    return explosionBuffer_;
}

const std::vector<CannonProjectile>& CannonSystem::projectiles() const {
    return projectiles_;
}

const std::vector<CannonRuntime>& CannonSystem::cannons() const {
    return cannons_;
}

std::span<const CannonShot> CannonSystem::shots() const {
    return shotBuffer_;
}

std::span<const CannonHit> CannonSystem::hits() const {
    return hitBuffer_;
}

void CannonSystem::launch(const BuildingInstance& cannon,
                          Vec3 targetPosition, double yawRadians,
                          double pitchRadians) {
    const Vec3 origin = cannon.type == BuildingType::Catapult
        ? catapultMuzzleWorldPosition(
            buildingWorldPosition(cannon), yawRadians, pitchRadians)
        : cannon.type == BuildingType::Mortar
        ? mortarMuzzleWorldPosition(buildingWorldPosition(cannon),yawRadians,pitchRadians)
        : cannonMuzzleWorldPosition(
            buildingWorldPosition(cannon), yawRadians, pitchRadians);
    const auto profile = cannonAttackProfile(cannon.type == BuildingType::Cannon
        ? attackStyle_ : CannonAttackStyle::Standard);
    for (int index = 0; index < profile.projectileCount; ++index) {
        Vec3 impact = targetPosition;
        if (index > 0) {
            // Four pellets fan out around the aimed shell. Separate impact
            // points and trajectories keep the volley legible in the world.
            constexpr double HalfPi = 1.5707963267948966;
            const double angle = yawRadians + HalfPi * static_cast<double>(index - 1);
            const double spread = 1.4;
            impact.x += std::cos(angle) * spread;
            impact.z += std::sin(angle) * spread;
        }
        launchProjectile(cannon, origin, impact, profile, index == 0);
    }
}

void CannonSystem::launchProjectile(const BuildingInstance& cannon, Vec3 origin,
                                    Vec3 targetPosition, const CannonAttackProfile& profile, bool recordShot) {
    const auto mortar = cannon.type == BuildingType::Mortar ? mortarTrajectory(origin,targetPosition,MortarHorizontalSpeed)
        : std::optional<MortarTrajectory>{};
    if (cannon.type == BuildingType::Mortar && !mortar) return;
    CannonProjectile* projectile = nullptr;
    for (auto& candidate : projectiles_) {
        if (!candidate.active) {
            projectile = &candidate;
            ++projectile->id.generation;
            break;
        }
    }
    if (projectile == nullptr) {
        if (projectiles_.size() >= MaxProjectiles) {
            return;
        }
        projectiles_.push_back({});
        projectile = &projectiles_.back();
        projectile->id = {nextProjectileIndex_++, 1};
    }

    const double deltaX = targetPosition.x - origin.x;
    const double deltaZ = targetPosition.z - origin.z;
    const double horizontalDistance = std::sqrt((deltaX * deltaX) + (deltaZ * deltaZ));
    const double projectileSpeed = cannon.type == BuildingType::Catapult
        ? CatapultProjectileSpeed : ProjectileSpeed;
    const double flightTime = std::max(
        0.2, horizontalDistance / projectileSpeed);
    projectile->cannonId = cannon.id;
    projectile->type = cannon.type;
    projectile->position = origin;
    projectile->trailHead = 0;
    projectile->trailPointCount = 1U;
    projectile->trailPositions[0] = origin;
    projectile->targetPosition = targetPosition;
    projectile->velocity = {
        deltaX / flightTime,
        (targetPosition.y - origin.y + 0.5 * Gravity * flightTime * flightTime) / flightTime,
        deltaZ / flightTime,
    };
    if(mortar) projectile->velocity=mortar->velocity;
    projectile->fuseRemaining = mortar ? mortar->flightTime : flightTime;
    const double levelBonus = static_cast<double>(cannon.level - 1);
    projectile->explosionRadius = explosionRadius(
        cannon.type, cannon.level) *
        radiusMultiplier_ * profile.radius;
    const double towerBonus = buildingAnvilMultiplier(cannon);
    const double heightBonus = cannon.platformStorey > 0
        ? highGroundDamageMultiplier_ : 1.0;
    projectile->explosionDamage = explosionDamage(
        cannon.type, cannon.level) *
        towerBonus * damageMultiplier_ * heightBonus * profile.damage *
        (cannon.type == bonusType_ ? typeDamageMultiplier_ : 1.0);
    projectile->explosionImpulse =
        BaseExplosionImpulse + 0.35 * levelBonus;
    projectile->visualScale = profile.visualScale;
    projectile->clusterExplosionCount = clusterExplosionCount_ + profile.clusterCount;
    projectile->active = true;
    if (recordShot) shotBuffer_.push_back({
        .cannonId = cannon.id,
        .projectileId = projectile->id,
        .position = origin,
        .type = cannon.type,
    });
}

void CannonSystem::explode(CannonProjectile& projectile, EnemySystem& enemies) {
    const auto damage = enemies.damageInRadius(projectile.position, projectile.explosionRadius,
                                               projectile.explosionDamage,
                                               projectile.explosionImpulse);
    int killedCount = 0;
    for (const auto& result : damage) {
        hitBuffer_.push_back({
            .cannonId = projectile.cannonId,
            .result = result,
            .type = projectile.type,
        });
        if (result.killed) {
            ++killedCount;
        }
    }
    explosionBuffer_.push_back({
        .projectileId = projectile.id,
        .position = projectile.position,
        .radius = projectile.explosionRadius,
        .hitCount = static_cast<int>(damage.size()),
        .killedCount = killedCount,
    });
    constexpr double FullCircle = 6.283185307179586;
    for (int clusterIndex = 0;
         clusterIndex < projectile.clusterExplosionCount; ++clusterIndex) {
        const double angle = FullCircle *
            (static_cast<double>(clusterIndex) /
                 static_cast<double>(
                     std::max(1, projectile.clusterExplosionCount)) +
             static_cast<double>(projectile.id.index % 17U) /
                 17.0);
        const double offset = projectile.explosionRadius * 0.72;
        const Vec3 clusterPosition{
            projectile.position.x + std::cos(angle) * offset,
            projectile.position.y,
            projectile.position.z + std::sin(angle) * offset,
        };
        const double clusterRadius =
            projectile.explosionRadius * 0.52;
        const auto clusterDamage = enemies.damageInRadius(
            clusterPosition, clusterRadius,
            projectile.explosionDamage * 0.45,
            projectile.explosionImpulse * 0.45);
        int clusterKills = 0;
        for (const EnemyDamageResult& result : clusterDamage) {
            hitBuffer_.push_back({
                .cannonId = projectile.cannonId,
                .result = result,
                .type = projectile.type,
            });
            if (result.killed) ++clusterKills;
        }
        explosionBuffer_.push_back({
            .projectileId = projectile.id,
            .position = clusterPosition,
            .radius = clusterRadius,
            .hitCount = static_cast<int>(clusterDamage.size()),
            .killedCount = clusterKills,
        });
    }
    projectile.active = false;
}

} // namespace ian
