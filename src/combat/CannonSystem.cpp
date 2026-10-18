#include "combat/CannonSystem.hpp"
#include "combat/MortarAttack.hpp"
#include "combat/DefenseTargeting.hpp"
#include "combat/DefenseRuntimeSupport.hpp"
#include "combat/DefenseAiming.hpp"
#include "combat/AttackArea.hpp"
#include "combat/DefenseTomeScaling.hpp"
#include "buildings/BuildingModifiers.hpp"
#include "buildings/CannonRig.hpp"
#include "buildings/CatapultRig.hpp"
#include "buildings/BuildingOrientation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace ian {
namespace {

constexpr double BaseRange = 12.0;
constexpr double ClusterRadius = 3.0;
constexpr double SearchInterval = 0.5;
constexpr double TurnSpeed = 12.0;
constexpr double PitchSpeed = 8.0;
constexpr double AimTolerance = 0.08726646259971647;
constexpr double MinimumPitch = -0.2617993877991494;
constexpr double MaximumPitch = 0.9599310885968813;
constexpr double ProjectileSpeed = 12.0;
constexpr double Gravity = 9.8;
constexpr double BaseExplosionRadius = 2.5;
constexpr double BaseExplosionImpulse = 3.5;
constexpr double CatapultRange = 15.0;
constexpr double CatapultMinimumRange = 4.25;
constexpr double CatapultProjectileSpeed = 9.0;
constexpr double CatapultExplosionRadius = 3.0;
constexpr double CatapultAnimationDuration = 0.95;
constexpr double CatapultReleaseTime = 0.34;
constexpr double CatapultWindupTime = 0.12;
constexpr double CatapultFollowThroughTime = 0.43;
constexpr double CatapultWindupPitch = 0.18;
constexpr double CatapultFollowThroughPitch = -1.20;


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

    explosionBuffer_.reserve(32);
    shotBuffer_.reserve(32);
}

double CannonSystem::attackRange(std::uint8_t level) {
    return BaseRange +
           0.5 * static_cast<double>(level - 1);
}

double CannonSystem::attackRange(
    BuildingType type, std::uint8_t level) {
    if (type == BuildingType::Mortar) return 18.0 + 0.5 * (std::clamp<int>(level,1,8)-1);
    if (type == BuildingType::Catapult) {
        return CatapultRange + 0.5 * static_cast<double>(level - 1);
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
    return 1.0 / (0.30 +
        0.02 * static_cast<double>(level - 1));
}

double CannonSystem::fireInterval(
    BuildingType type, std::uint8_t level) {
    if (type == BuildingType::Mortar) return 4.0 / (1.0 + 0.06 * (std::clamp<int>(level,1,8)-1));
    if (type == BuildingType::Catapult) {
        return 1.0 / (0.25 +
            0.02 * static_cast<double>(level - 1));
    }
    return fireInterval(level);
}

double CannonSystem::explosionRadius(std::uint8_t level) {
    return BaseExplosionRadius +
           0.1 * static_cast<double>(level - 1);
}

double CannonSystem::explosionRadius(
    BuildingType type, std::uint8_t level) {
    if (type == BuildingType::Mortar) return 3.25 + 0.12 * (std::clamp<int>(level,1,8)-1);
    if (type == BuildingType::Catapult) {
        return CatapultExplosionRadius +
            0.1 * static_cast<double>(level - 1);
    }
    return explosionRadius(level);
}

double CannonSystem::explosionDamage(std::uint8_t level) {
    constexpr std::array<double, MaxBuildingLevel> Damage{
        2.5, 3.0, 4.0, 4.5, 5.5, 6.5, 7.0, 7.5};
    return Damage[static_cast<std::size_t>(
        std::clamp<int>(level, 1, MaxBuildingLevel) - 1)];
}

double CannonSystem::explosionDamage(
    BuildingType type, std::uint8_t level) {
    if (type == BuildingType::Mortar) return 6.0 + 1.0 * (std::clamp<int>(level,1,8)-1);
    if (type == BuildingType::Catapult) {
        constexpr std::array<double, MaxBuildingLevel> Damage{
            4.0, 5.0, 6.0, 7.5, 8.5, 10.0, 11.0, 12.0};
        return Damage[static_cast<std::size_t>(
            std::clamp<int>(level, 1, MaxBuildingLevel) - 1)];
    }
    return explosionDamage(level);
}

void CannonSystem::reset() {
    setAttackStyle(CannonAttackStyle::Standard);
    setTypeBonuses(BuildingType::Core, 1.0, 1.0);
    cannons_.clear();
    pool_.clear();
    modifiers_ = {}; typeModifiers_ = {}; highGroundDamageMultiplier_ = 1.0; clusterExplosionCount_ = 0;
    explosionBuffer_.clear();
    shotBuffer_.clear();
    hitBuffer_.clear();
}

void CannonSystem::setAttackStyle(CannonAttackStyle style) {
    attackStyle_ = style;
}

AttackStats CannonSystem::baseStats(BuildingType type, std::uint8_t level) {
    level = std::clamp<std::uint8_t>(level, 1, MaxBuildingLevel);
    AttackStats stats;
    stats.damage = explosionDamage(type, level);
    stats.attackInterval = fireInterval(type, level);
    stats.range = attackRange(type, level);
    stats.projectileSpeed = type == BuildingType::Catapult ? CatapultProjectileSpeed : ProjectileSpeed;
    if (type == BuildingType::Mortar) stats.projectileSpeed = MortarHorizontalSpeed;
    stats.duration = type == BuildingType::Mortar ? 8.0 : 4.0;
    stats.knockback = defenseBaseKnockback(type, BaseExplosionImpulse + 0.35 * (level - 1));
    stats.spreadDegrees = 0.0;
    return stats;
}

FinalAttackStats CannonSystem::finalStats(const BuildingInstance& building) const {
    auto base = baseStats(building.type, building.level);
    const auto profile = cannonAttackProfile(building.type == BuildingType::Cannon
        ? attackStyle_ : CannonAttackStyle::Standard);
    base.quantity = profile.projectileCount;
    base.attackInterval *= profile.interval;
    base.damage *= profile.damage;
    return resolveBuildingAttackStats(base, building, modifiers_, bonusType_, typeModifiers_,
        highGroundDamageMultiplier_);
}

void CannonSystem::setTypeBonuses(BuildingType type, double damage, double fireRate) {
    bonusType_ = type; typeModifiers_ = {};
    typeModifiers_[AttackStat::Damage].multiplier = std::max(0.05, damage);
    typeModifiers_[AttackStat::AttackSpeed].multiplier = std::max(0.05, fireRate);
}

void CannonSystem::setModifiers(double damage, double radius, double fireRate,
    double highGroundDamage, int clusterExplosionCount) {
    modifiers_ = {};
    modifiers_[AttackStat::Damage].multiplier = std::max(0.05, damage);
    modifiers_[AttackStat::Size].multiplier = std::max(0.05, radius);
    modifiers_[AttackStat::AttackSpeed].multiplier = std::max(0.05, fireRate);
    highGroundDamageMultiplier_ = std::max(1.0, highGroundDamage);
    clusterExplosionCount_ = std::max(0, clusterExplosionCount);
}
void CannonSystem::clearProjectiles() {
    pool_.clear();
    for (auto& cannon : cannons_) {
        cannon.pendingTargetPosition.reset(); cannon.firingAnimationRemaining = 0.0; cannon.loaded = true;
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
    query_.rebuild(enemies);
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
        const auto stats = finalStats(*building);
        const double range = stats.range;
        const double deadZone = minimumRange(
            building->type, building->level);
        const double shotInterval = stats.attackInterval;
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
                       cannon.yaw, catapultArmPitch(CatapultReleaseTime), cannon.attack.stats);
                cannon.loaded = false;
                cannon.fireCooldownRemaining = cannon.attack.stats.attackInterval;
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
            if (!target || !projectileDefenseCanTarget(building->type, *target, building->baseHeight)) {
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
        if (preferMarkedTargets_ && cannon.targetSearchCooldownRemaining <= 0.0) {
            if (auto marked = markedDefenseTarget(enemies, building->type, targetingOrigin, range,
                    cannon.restYaw, building->level, deadZone)) {
                cannon.targetId = marked;
                cannon.targetSearchCooldownRemaining = SearchInterval;
            } else if (cannon.targetId) {
                cannon.targetSearchCooldownRemaining = SearchInterval;
            }
        }
        if (!cannon.targetId && cannon.targetSearchCooldownRemaining <= 0.0) {
            cannon.targetId = enemies.densestEnemyInArc(
                targetingOrigin, range, ClusterRadius, cannon.restYaw,
                defenseAttackHalfAngleRadians(building->type, building->level), deadZone,
                defenseCanTargetFlying(building->type));
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
                    const auto trajectory=mortarTrajectory(origin,impact,stats.projectileSpeed);
                    if (!trajectory) {cannon.targetId.reset();continue;}
                    desiredPitch=trajectory->pitch;
                } else if (building->type == BuildingType::Cannon) {
                    const double horizontalDistance = std::hypot(deltaX, deltaZ);
                    const double flightTime = std::max(
                        0.2, horizontalDistance / std::max(0.001, stats.projectileSpeed));
                    const double verticalSpeed =
                        (target->position.y + target->worldSurfaceHeight - origin.y +
                         0.5 * Gravity * flightTime * flightTime) / flightTime;
                    desiredPitch = std::clamp(
                        std::atan2(verticalSpeed, std::max(0.001, stats.projectileSpeed)),
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
                cannon.attack = {cannon.buildingId, stats};
                if (building->type == BuildingType::Catapult) {
                    cannon.pendingTargetPosition = target->position;
                    cannon.pendingTargetPosition->y += target->worldSurfaceHeight;
                    cannon.firingAnimationRemaining =
                        CatapultAnimationDuration;
                    cannon.loaded = true;
                } else {
                    auto impactPosition = target->position;
                    impactPosition.y += target->worldSurfaceHeight;
                    launch(*building, impactPosition, cannon.yaw, cannon.pitch, stats);
                    cannon.fireCooldownRemaining = shotInterval;
                }
            }
        }
    }

    auto& items = pool_.items();
    for (std::size_t slot = 0; slot < items.size(); ++slot) {
        auto& projectile = items[slot];
        if (!projectile.active) continue;
        const auto& stats = projectile.configuration.stats;
        const double dt = std::min(deltaSeconds, std::max(0.0, stats.duration - projectile.runtime.elapsedLifetime));
        projectile.runtime.elapsedLifetime += dt;
        const Vec3 start = projectile.position;
        // Exact constant-gravity integration avoids arrival teleports.
        Vec3 end = geometry::add(start, geometry::scale(projectile.velocity, dt));
        end.y -= 0.5 * Gravity * dt * dt;
        projectile.velocity.y -= Gravity * dt;
        projectile.fuseRemaining -= dt;
        bool impact = false;
        Vec3 sweepStart = start;
        while (const auto contact = query_.sweep(enemies, sweepStart, end,
                projectile.configuration.hitRadius, projectile.type,
                projectile.configuration.sourceSurfaceHeight, projectile.runtime.alreadyHitTargets)) {
            if (projectile.runtime.remainingPierce <= 0) {
                end = contact->position; impact = true; break;
            }
            const auto contactStats = defenseContactStats(stats, projectile.type,
                effectiveAttackCount(stats.bounces)-projectile.runtime.remainingBounces,
                effectiveAttackCount(stats.pierce)-projectile.runtime.remainingPierce);
            --projectile.runtime.remainingPierce;
            projectile.runtime.alreadyHitTargets.push_back(contact->id);
            if (const auto hit = applyAttackHit(enemies, contact->id, contactStats, projectile.velocity,
                    (static_cast<std::uint64_t>(projectile.id.index) << 32U) ^ projectile.id.generation ^ contact->id.index)) {
                hitBuffer_.push_back({projectile.cannonId, hit->result, projectile.type, hit->critical});
            }
            sweepStart = contact->position;
        }
        projectile.position = end;
        projectile.trailHead = (projectile.trailHead + 1U) % CannonProjectile::TrailCapacity;
        projectile.trailPositions[projectile.trailHead] = end;
        projectile.trailPointCount = std::min(projectile.trailPointCount + 1U, CannonProjectile::TrailCapacity);
        if (impact || projectile.fuseRemaining <= 0.0) {
            explode(projectile, enemies);
            if (!bounce(projectile, enemies)) pool_.release(slot);
        } else if (projectile.runtime.elapsedLifetime >= stats.duration) {
            pool_.release(slot);
        }
    }
    return explosionBuffer_;
}

const std::vector<CannonProjectile>& CannonSystem::projectiles() const {
    return pool_.items();
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
                          double pitchRadians, const FinalAttackStats& stats) {
    const Vec3 origin = cannon.type == BuildingType::Catapult
        ? catapultMuzzleWorldPosition(
            buildingWorldPosition(cannon), yawRadians, pitchRadians)
        : cannon.type == BuildingType::Mortar
        ? mortarMuzzleWorldPosition(buildingWorldPosition(cannon),yawRadians,pitchRadians)
        : cannonMuzzleWorldPosition(
            buildingWorldPosition(cannon), yawRadians, pitchRadians);
    const auto profile = cannonAttackProfile(cannon.type == BuildingType::Cannon
        ? attackStyle_ : CannonAttackStyle::Standard);
    for (int index = 0; index < effectiveAttackCount(stats.quantity, 1); ++index) {
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
        if (!launchProjectile(cannon, origin, impact, profile, defenseInstanceStats(stats,cannon.type,index), index == 0)) break;
    }
}

bool CannonSystem::launchProjectile(const BuildingInstance& cannon, Vec3 origin,
                                    Vec3 targetPosition, const CannonAttackProfile& profile, const FinalAttackStats& stats, bool recordShot) {
    const auto mortar = cannon.type == BuildingType::Mortar ? mortarTrajectory(origin,targetPosition,stats.projectileSpeed)
        : std::optional<MortarTrajectory>{};
    if (cannon.type == BuildingType::Mortar && !mortar) return false;
    CannonProjectile* projectile = pool_.acquire();
    if (!projectile) return false;

    const double deltaX = targetPosition.x - origin.x;
    const double deltaZ = targetPosition.z - origin.z;
    const double horizontalDistance = std::sqrt((deltaX * deltaX) + (deltaZ * deltaZ));
    const double flightTime = std::max(0.2, horizontalDistance / std::max(0.001, stats.projectileSpeed));
    projectile->configuration = {stats, 0.24 * stats.size * profile.visualScale, cannon.baseHeight};
    projectile->runtime.remainingBounces = effectiveAttackCount(stats.bounces);
    projectile->runtime.remainingPierce = effectiveAttackCount(stats.pierce);
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
    if (stats.projectileSpeed <= 0.0) {
        projectile->velocity = {};
        projectile->fuseRemaining = std::numeric_limits<double>::infinity();
    }
    projectile->explosionRadius = explosionRadius(cannon.type, cannon.level) * stats.size * profile.radius;
    projectile->explosionDamage = stats.damage;
    projectile->explosionImpulse = stats.knockback;
    projectile->visualScale = profile.visualScale * stats.size;
    projectile->clusterExplosionCount = clusterExplosionCount_ + profile.clusterCount;
    projectile->active = true;
    if (recordShot) shotBuffer_.push_back({
        .cannonId = cannon.id,
        .projectileId = projectile->id,
        .position = origin,
        .type = cannon.type,
    });
    return true;
}

bool CannonSystem::bounce(CannonProjectile& projectile, EnemySystem& enemies) {
    auto& state = projectile.runtime;
    const auto& stats = projectile.configuration.stats;
    if (state.remainingBounces <= 0 || state.elapsedLifetime >= stats.duration || stats.projectileSpeed <= 0.0) return false;
    const auto target = query_.bounceTarget(enemies, projectile.position, stats.bounceRange,
        projectile.type, projectile.configuration.sourceSurfaceHeight, state.alreadyHitTargets);
    if (!target) return false;
    --state.remainingBounces; state.currentTarget = target->id;
    projectile.targetPosition = attackTargetPosition(*target);
    const Vec3 offset = geometry::subtract(projectile.targetPosition, projectile.position);
    if(projectile.type == BuildingType::Mortar) {
        const auto arc=mortarTrajectory(projectile.position,projectile.targetPosition,stats.projectileSpeed);
        if(!arc) return false;
        projectile.velocity=arc->velocity;projectile.fuseRemaining=arc->flightTime;return true;
    }
    const double time = std::max(0.2, std::hypot(offset.x, offset.z) / std::max(0.001, stats.projectileSpeed));
    projectile.velocity = {offset.x / time, offset.y / time + 0.5 * Gravity * time, offset.z / time};
    projectile.fuseRemaining = time;
    return true;
}

void CannonSystem::explode(CannonProjectile& projectile, EnemySystem& enemies) {
    const auto pulse = [&](Vec3 origin, double radius, double fraction, std::uint64_t salt,
                           std::vector<EntityId>* history) {
        auto stats = defenseContactStats(projectile.configuration.stats, projectile.type,
            effectiveAttackCount(projectile.configuration.stats.bounces)-projectile.runtime.remainingBounces,
            effectiveAttackCount(projectile.configuration.stats.pierce)-projectile.runtime.remainingPierce);
        stats.damage *= fraction; stats.knockback *= fraction;
        int hits = 0, kills = 0;
        applyAttackArea(enemies, query_, origin, radius, stats,
            (static_cast<std::uint64_t>(projectile.id.index) << 32U) ^ projectile.id.generation ^ salt,
            [&](const AttackHit& hit) {
                ++hits; kills += hit.result.killed ? 1 : 0;
                hitBuffer_.push_back({projectile.cannonId, hit.result, projectile.type, hit.critical});
            }, history, defenseCanTargetFlying(projectile.type));
        explosionBuffer_.push_back({projectile.id, origin, radius, hits, kills});
    };
    pulse(projectile.position, projectile.explosionRadius, 1.0, 0, &projectile.runtime.alreadyHitTargets);
    constexpr double FullCircle = 6.283185307179586;
    // Cluster payload remains a weapon-specific area pattern. Each mini blast
    // is its own attack instance, sharing the central hit calculation.
    for (int index = 0; index < projectile.clusterExplosionCount; ++index) {
        const double angle = FullCircle * (static_cast<double>(index) /
            std::max(1, projectile.clusterExplosionCount) + static_cast<double>(projectile.id.index % 17U) / 17.0);
        const double offset = projectile.explosionRadius * 0.72;
        pulse({projectile.position.x + std::cos(angle) * offset, projectile.position.y,
            projectile.position.z + std::sin(angle) * offset}, projectile.explosionRadius * 0.52,
            0.45, static_cast<std::uint64_t>(index + 1), nullptr);
    }
}

} // namespace ian
