#include "combat/CannonSystem.hpp"
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
constexpr double CatapultReleasePitch = -1.05;
constexpr std::size_t MaxProjectiles = 300;

[[nodiscard]] double easeInOut(double value) {
    value = std::clamp(value, 0.0, 1.0);
    return value * value * (3.0 - 2.0 * value);
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
    if (type == BuildingType::Catapult) {
        return CatapultRange + 0.45 * static_cast<double>(level - 1);
    }
    return attackRange(level);
}

double CannonSystem::minimumRange(
    BuildingType type, std::uint8_t level) {
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
    if (type == BuildingType::Catapult) {
        return CatapultExplosionDamage +
            0.45 * static_cast<double>(level - 1);
    }
    return explosionDamage(level);
}

void CannonSystem::reset() {
    cannons_.clear();
    projectiles_.clear();
    explosionBuffer_.clear();
    shotBuffer_.clear();
    hitBuffer_.clear();
}

void CannonSystem::setSkillModifiers(
    double damage, double radius, double fireRate,
    double highGroundDamage) {
    damageMultiplier_ = std::max(0.05, damage);
    radiusMultiplier_ = std::max(0.05, radius);
    fireRateMultiplier_ = std::max(0.05, fireRate);
    highGroundDamageMultiplier_ = std::max(1.0, highGroundDamage);
}

void CannonSystem::clearProjectiles() {
    for (auto& projectile : projectiles_) {
        projectile.active = false;
    }
}

void CannonSystem::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    std::erase_if(cannons_, [&buildings](const CannonRuntime& cannon) {
        return std::none_of(buildings.begin(), buildings.end(),
                            [&cannon](const BuildingInstance& building) {
                                return building.id == cannon.buildingId &&
                                       building.type == cannon.type &&
                                       (building.type == BuildingType::Cannon ||
                                        building.type == BuildingType::Catapult);
                            });
    });
    for (const auto& building : buildings) {
        if (building.type != BuildingType::Cannon &&
            building.type != BuildingType::Catapult) {
            continue;
        }
        const double restYaw = buildingRotationYaw(
            building.type, building.rotation);
        const auto runtime = std::find_if(
            cannons_.begin(), cannons_.end(),
            [&building](const CannonRuntime& cannon) {
                return cannon.buildingId == building.id;
            });
        if (runtime == cannons_.end()) {
            cannons_.push_back({
                .buildingId = building.id,
                .type = building.type,
                .restYaw = restYaw,
                .baseYaw = restYaw,
                .yaw = restYaw,
            });
        } else if (
            std::abs(wrapBuildingAngle(
                runtime->restYaw - restYaw)) >
            0.0001) {
            runtime->restYaw = restYaw;
            runtime->targetId.reset();
            runtime->targetSearchCooldownRemaining = SearchInterval;
        }
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

        cannon.fireCooldownRemaining = std::max(
            0.0, cannon.fireCooldownRemaining - deltaSeconds);
        cannon.targetSearchCooldownRemaining =
            std::max(0.0, cannon.targetSearchCooldownRemaining - deltaSeconds);
        const Vec3 targetingOrigin = buildingWorldPosition(*building);
        const double range = attackRange(building->type, building->level);
        const double deadZone = minimumRange(
            building->type, building->level);
        const double shotInterval = fireInterval(
            building->type, building->level) /
            fireRateMultiplier_;
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
            if (elapsed <= CatapultReleaseTime) {
                cannon.pitch = CatapultReleasePitch * easeInOut(
                    elapsed / CatapultReleaseTime);
            } else {
                const double recovery = easeInOut(
                    (elapsed - CatapultReleaseTime) /
                    (CatapultAnimationDuration - CatapultReleaseTime));
                cannon.pitch = CatapultReleasePitch * (1.0 - recovery);
            }
            if (previousElapsed < CatapultReleaseTime &&
                elapsed >= CatapultReleaseTime &&
                cannon.pendingTargetPosition) {
                launch(*building, *cannon.pendingTargetPosition,
                       cannon.yaw, CatapultReleasePitch);
                cannon.pendingTargetPosition.reset();
                cannon.loaded = false;
                cannon.fireCooldownRemaining = shotInterval;
            }
            if (cannon.firingAnimationRemaining <= 0.0) {
                cannon.pitch = 0.0;
                cannon.loaded = true;
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
                        targetingOrigin, target->position, cannon.restYaw,
                        building->level)) {
                    cannon.targetId.reset();
                }
            }
        }
        if (!cannon.targetId && cannon.targetSearchCooldownRemaining <= 0.0) {
            cannon.targetId = enemies.densestEnemyInArc(
                targetingOrigin, range, ClusterRadius, cannon.restYaw,
                defenseAttackHalfAngleRadians(building->level), deadZone);
            cannon.targetSearchCooldownRemaining = SearchInterval;
        }

        if (!cannon.targetId) {
            cannon.yaw = smoothBuildingAngle(
                cannon.yaw, cannon.restYaw, deltaSeconds);
        }

        bool aimedAtTarget = false;
        if (cannon.targetId) {
            const auto target = enemies.enemy(*cannon.targetId);
            if (target) {
                const Vec3 origin = building->type == BuildingType::Catapult
                    ? targetingOrigin
                    : cannonMuzzleWorldPosition(
                        targetingOrigin, cannon.yaw, cannon.pitch);
                const double deltaX = target->position.x - origin.x;
                const double deltaZ = target->position.z - origin.z;
                const double desiredYaw = std::atan2(-deltaX, -deltaZ);
                const double yawDelta = wrapBuildingAngle(
                    desiredYaw - cannon.yaw);
                cannon.yaw +=
                    std::clamp(yawDelta, -TurnSpeed * deltaSeconds,
                               TurnSpeed * deltaSeconds);
                cannon.yaw = wrapBuildingAngle(cannon.yaw);

                if (building->type == BuildingType::Catapult) {
                    cannon.pitch = 0.0;
                    aimedAtTarget = std::abs(wrapBuildingAngle(
                        desiredYaw - cannon.yaw)) <= AimTolerance;
                } else {
                    const double horizontalDistance =
                        std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
                    const double flightTime = std::max(
                        0.2, horizontalDistance / ProjectileSpeed);
                    const double verticalSpeed =
                        (target->position.y - origin.y +
                         0.5 * Gravity * flightTime * flightTime) /
                        flightTime;
                    const double desiredPitch = std::clamp(
                        std::atan2(verticalSpeed, ProjectileSpeed),
                        MinimumPitch, MaximumPitch);
                    const double pitchDelta = desiredPitch - cannon.pitch;
                    cannon.pitch += std::clamp(
                        pitchDelta, -PitchSpeed * deltaSeconds,
                        PitchSpeed * deltaSeconds);
                    aimedAtTarget =
                        std::abs(wrapBuildingAngle(
                            desiredYaw - cannon.yaw)) <= AimTolerance &&
                        std::abs(desiredPitch - cannon.pitch) <= AimTolerance;
                }
            } else {
                cannon.targetId.reset();
            }
        }
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

    const Vec3 origin = cannon.type == BuildingType::Catapult
        ? catapultMuzzleWorldPosition(
            buildingWorldPosition(cannon), yawRadians, pitchRadians)
        : cannonMuzzleWorldPosition(
            buildingWorldPosition(cannon), yawRadians, pitchRadians);
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
    projectile->targetPosition = targetPosition;
    projectile->velocity = {
        deltaX / flightTime,
        (targetPosition.y - origin.y + 0.5 * Gravity * flightTime * flightTime) / flightTime,
        deltaZ / flightTime,
    };
    projectile->fuseRemaining = flightTime;
    const double levelBonus = static_cast<double>(cannon.level - 1);
    projectile->explosionRadius = explosionRadius(
        cannon.type, cannon.level) *
        radiusMultiplier_;
    const double towerBonus = cannon.anvilStacks > 0
        ? 1.0 + 0.10 * cannon.anvilStacks
        : cannon.anvilEnhanced ? 1.10 : 1.0;
    const double heightBonus = cannon.platformStorey > 0
        ? highGroundDamageMultiplier_ : 1.0;
    projectile->explosionDamage = explosionDamage(
        cannon.type, cannon.level) *
        towerBonus * damageMultiplier_ * heightBonus;
    projectile->explosionImpulse =
        BaseExplosionImpulse + 0.35 * levelBonus;
    projectile->active = true;
    shotBuffer_.push_back({
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
    projectile.active = false;
}

} // namespace ian
