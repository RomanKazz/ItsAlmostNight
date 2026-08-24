#include "enemies/EnemySystem.hpp"

#include "core/Geometry.hpp"
#include "enemies/EnemyCollision.hpp"
#include "world/TerrainHeightfield.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ian {
namespace {

std::size_t statusEffectIndex(StatusEffectType type) {
    return type == StatusEffectType::Freeze ? 0U : 1U;
}

double knockbackMultiplier(EnemyType type) {
    switch (type) {
    case EnemyType::Basic: return 1.0;
    case EnemyType::Fast: return 1.15;
    case EnemyType::Heavy: return 0.3;
    case EnemyType::Boss: return 0.15;
    case EnemyType::Ranged: return 0.85;
    case EnemyType::Sapper: return 0.55;
    case EnemyType::Flying: return 0.7;
    case EnemyType::Splitter: return 0.45;
    case EnemyType::Splitling: return 1.2;
    }
    return 1.0;
}

} // namespace

std::optional<EntityId> EnemySystem::raycast(
    Vec3 origin, Vec3 direction, double maxDistance,
    const TerrainHeightfield* terrain) const {
    std::optional<EntityId> result;
    double closestDistance = std::numeric_limits<double>::max();
    for (const auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }

        const EnemyCapsule capsule = enemyCapsule(enemy.type);
        // Aim follows the animated silhouette with a small tolerance. This
        // does not affect physical crowd collision or melee reach.
        constexpr double AimRadiusPadding = 0.14;
        constexpr double AimHeightPadding = 0.12;
        Vec3 center = enemy.position;
        if (terrain != nullptr) {
            center.y += terrain->getHeight(center.x, center.z);
        }
        center.y += enemy.surfaceHeightOffset;
        const auto distance = geometry::rayVerticalCapsuleDistance(
            origin, direction, center,
            capsule.radius + AimRadiusPadding,
            capsule.segmentHalfHeight + AimHeightPadding);
        if (distance && *distance <= maxDistance && *distance < closestDistance) {
            result = enemy.id;
            closestDistance = *distance;
        }
    }
    return result;
}

std::optional<EnemyDamageResult> EnemySystem::damage(EntityId id, double amount) {
    EnemyInstance* enemy = findEnemy(id);
    if (enemy == nullptr || !enemy->active || amount <= 0.0) {
        return std::nullopt;
    }

    amount *= incomingDamageMultiplier(*enemy);
    const double previousHealth = enemy->health;
    enemy->health = std::max(0.0, enemy->health - amount);
    enemy->hitAnimationRemaining = 0.22;
    const bool killed = enemy->health <= 0.0;
    const EnemyType killedType = enemy->type;
    const std::uint8_t killedAffixes = enemy->eliteAffixes;
    const Vec3 killedPosition = enemy->position;
    const double eliteHealthMultiplier =
        enemy->eliteAffixes != 0U ? 1.6 : 1.0;
    const double eliteDamageMultiplier =
        enemy->eliteAffixes != 0U ? 1.1 : 1.0;
    const double healthMultiplier = enemy->maxHealth /
        definitions_[static_cast<std::size_t>(enemy->type)].health /
        eliteHealthMultiplier;
    const double damageMultiplier = enemy->damage /
        definitions_[static_cast<std::size_t>(enemy->type)].damage /
        eliteDamageMultiplier;
    if (killed) {
        markEnemyDead(*enemy);
    }
    const EnemyDamageResult result{
        .id = id,
        .type = killedType,
        .eliteAffixes = killedAffixes,
        .position = killedPosition,
        .damage = previousHealth - enemy->health,
        .remainingHealth = enemy->health,
        .killed = killed,
    };
    if (killed && killedType == EnemyType::Splitter) {
        scheduleSplit(
            id, killedPosition,
            healthMultiplier, damageMultiplier);
    }
    return result;
}

double EnemySystem::incomingDamageMultiplier(
    const EnemyInstance& target) const {
    constexpr double WardenRadiusSquared = 5.5 * 5.5;
    for (const EnemyInstance& candidate : enemies_) {
        if (!candidate.active || candidate.id == target.id ||
            !hasEliteAffix(
                candidate.eliteAffixes, EliteAffix::Warden)) {
            continue;
        }
        const double x = candidate.position.x - target.position.x;
        const double z = candidate.position.z - target.position.z;
        if (x * x + z * z <= WardenRadiusSquared) {
            return 0.75;
        }
    }
    return 1.0;
}

std::optional<EntityId> EnemySystem::nearestEnemy(Vec3 position, double radius) const {
    std::optional<EntityId> nearest;
    double nearestDistanceSquared = radius * radius;
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& entry) {
        const EnemyInstance* enemy = findEnemy(entry.id);
        if (enemy == nullptr || !enemy->active) {
            return;
        }
        const double deltaX = entry.position.x - position.x;
        const double deltaZ = entry.position.z - position.z;
        const double distanceSquared = (deltaX * deltaX) + (deltaZ * deltaZ);
        if (distanceSquared < nearestDistanceSquared ||
            (distanceSquared == nearestDistanceSquared &&
             (!nearest || entry.id.index < nearest->index))) {
            nearest = entry.id;
            nearestDistanceSquared = distanceSquared;
        }
    });
    return nearest;
}

std::optional<EntityId> EnemySystem::nearestEnemyInArc(
    Vec3 position, double radius, double yaw,
    double halfAngle, bool includeFlying,
    double maximumSurfaceHeightDifference) const {
    const double forwardX = -std::sin(yaw);
    const double forwardZ = -std::cos(yaw);
    const double minimumDot = std::cos(halfAngle);
    std::optional<EntityId> nearest;
    double nearestDistanceSquared = radius * radius;
    spatialHash_.forEachNearby(
        position, radius, [&](const SpatialEntry& entry) {
            const EnemyInstance* enemy = findEnemy(entry.id);
            if (enemy == nullptr || !enemy->active ||
                (!includeFlying && enemy->type == EnemyType::Flying) ||
                std::abs(enemy->worldSurfaceHeight - position.y) >
                    maximumSurfaceHeightDifference) {
                return;
            }
            const double deltaX = entry.position.x - position.x;
            const double deltaZ = entry.position.z - position.z;
            const double distanceSquared =
                deltaX * deltaX + deltaZ * deltaZ;
            if (distanceSquared <= 1e-10) return;
            const double inverseDistance =
                1.0 / std::sqrt(distanceSquared);
            const double directionDot =
                (deltaX * forwardX + deltaZ * forwardZ) *
                inverseDistance;
            if (directionDot + 1e-9 < minimumDot) return;
            if (distanceSquared < nearestDistanceSquared ||
                (distanceSquared == nearestDistanceSquared &&
                 (!nearest || entry.id.index < nearest->index))) {
                nearest = entry.id;
                nearestDistanceSquared = distanceSquared;
            }
        });
    return nearest;
}

std::optional<EntityId> EnemySystem::densestEnemy(Vec3 position, double radius,
                                                  double clusterRadius) const {
    std::optional<EntityId> best;
    std::size_t bestCount = 0;
    double bestDistanceSquared = radius * radius;
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& candidate) {
        const EnemyInstance* candidateEnemy = findEnemy(candidate.id);
        if (candidateEnemy == nullptr || !candidateEnemy->active) {
            return;
        }
        std::size_t count = 0;
        spatialHash_.forEachNearby(candidate.position, clusterRadius,
                                   [this, &count](const SpatialEntry& entry) {
            const EnemyInstance* enemy = findEnemy(entry.id);
            if (enemy != nullptr && enemy->active) {
                ++count;
            }
        });
        const double deltaX = candidate.position.x - position.x;
        const double deltaZ = candidate.position.z - position.z;
        const double distanceSquared = (deltaX * deltaX) + (deltaZ * deltaZ);
        if (count > bestCount ||
            (count == bestCount &&
             (distanceSquared < bestDistanceSquared ||
              (distanceSquared == bestDistanceSquared &&
               (!best || candidate.id.index < best->index))))) {
            best = candidate.id;
            bestCount = count;
            bestDistanceSquared = distanceSquared;
        }
    });
    return best;
}

std::optional<EntityId> EnemySystem::densestEnemyInArc(
    Vec3 position, double radius, double clusterRadius,
    double yaw, double halfAngle, double minimumRadius) const {
    const double forwardX = -std::sin(yaw);
    const double forwardZ = -std::cos(yaw);
    const double minimumDot = std::cos(halfAngle);
    std::optional<EntityId> best;
    std::size_t bestCount = 0;
    double bestDistanceSquared = radius * radius;
    const double minimumDistanceSquared =
        minimumRadius * minimumRadius;
    spatialHash_.forEachNearby(
        position, radius, [&](const SpatialEntry& candidate) {
            const EnemyInstance* enemy = findEnemy(candidate.id);
            if (enemy == nullptr || !enemy->active) return;
            const double deltaX = candidate.position.x - position.x;
            const double deltaZ = candidate.position.z - position.z;
            const double distanceSquared =
                deltaX * deltaX + deltaZ * deltaZ;
            if (distanceSquared <= std::max(1e-10, minimumDistanceSquared)) {
                return;
            }
            const double inverseDistance =
                1.0 / std::sqrt(distanceSquared);
            const double directionDot =
                (deltaX * forwardX + deltaZ * forwardZ) *
                inverseDistance;
            if (directionDot + 1e-9 < minimumDot) return;
            std::size_t count = 0;
            spatialHash_.forEachNearby(
                candidate.position, clusterRadius,
                [this, &count](const SpatialEntry& entry) {
                    const EnemyInstance* nearby = findEnemy(entry.id);
                    if (nearby != nullptr && nearby->active) ++count;
                });
            if (count > bestCount ||
                (count == bestCount &&
                 (distanceSquared < bestDistanceSquared ||
                  (distanceSquared == bestDistanceSquared &&
                   (!best || candidate.id.index < best->index))))) {
                best = candidate.id;
                bestCount = count;
                bestDistanceSquared = distanceSquared;
            }
        });
    return best;
}

std::optional<EnemyInstance> EnemySystem::enemy(EntityId id) const {
    const EnemyInstance* instance = findEnemy(id);
    if (instance == nullptr || !instance->active) {
        return std::nullopt;
    }
    return *instance;
}

std::span<const EnemyDamageResult> EnemySystem::damageInRadius(Vec3 position, double radius,
                                                               double amount,
                                                               double knockbackStrength,
                                                               std::optional<Vec3> knockbackOrigin,
                                                               double maxTotalDamage,
                                                               std::optional<EntityId> excludedId) {
    areaDamageBuffer_.clear();
    if (radius <= 0.0 || amount <= 0.0) {
        return areaDamageBuffer_;
    }

    // Player attacks can land between enemy ticks. Refresh positions before
    // querying, then include capsule radius so enemies touching the impact
    // area are not missed just because their centers are outside it.
    rebuildSpatialIndex();
    const double queryRadius =
        radius + enemyCapsule(EnemyType::Boss).radius;
    std::size_t targetCount = 0;
    spatialHash_.forEachNearby(position, queryRadius,
                               [&](const SpatialEntry& entry) {
        const EnemyInstance* candidate = findEnemy(entry.id);
        if (candidate == nullptr || !candidate->active ||
            (excludedId && candidate->id == *excludedId)) {
            return;
        }
        const double hitRadius =
            radius + enemyCapsule(candidate->type).radius;
        const double deltaX = entry.position.x - position.x;
        const double deltaZ = entry.position.z - position.z;
        if (deltaX * deltaX + deltaZ * deltaZ <=
                hitRadius * hitRadius &&
            targetCount < areaTargetBuffer_.size()) {
            areaTargetBuffer_[targetCount++] = entry.id;
        }
    });
    const double damagePerTarget =
        maxTotalDamage > 0.0 && targetCount > 0U
            ? std::min(
                  amount,
                  maxTotalDamage /
                      static_cast<double>(targetCount))
            : amount;
    const Vec3 impulseOrigin =
        knockbackOrigin.value_or(position);
    pendingSplitBuffer_.clear();
    for (std::size_t index = 0; index < targetCount; ++index) {
        EnemyInstance* enemy = findEnemy(areaTargetBuffer_[index]);
        if (enemy == nullptr || !enemy->active || amount <= 0.0) {
            continue;
        }
        const double effectiveDamage = damagePerTarget *
            incomingDamageMultiplier(*enemy);
        const double previousHealth = enemy->health;
        enemy->health = std::max(
            0.0, enemy->health - effectiveDamage);
        enemy->hitAnimationRemaining = 0.22;
        const bool killed = enemy->health <= 0.0;
        const EnemyType killedType = enemy->type;
        const std::uint8_t killedAffixes = enemy->eliteAffixes;
        const double childHealthMultiplier = enemy->maxHealth /
            definitions_[static_cast<std::size_t>(enemy->type)].health /
            (enemy->eliteAffixes != 0U ? 1.6 : 1.0);
        const double childDamageMultiplier = enemy->damage /
            definitions_[static_cast<std::size_t>(enemy->type)].damage /
            (enemy->eliteAffixes != 0U ? 1.1 : 1.0);
        if (killed) {
            markEnemyDead(*enemy);
        }
        areaDamageBuffer_.push_back({
            .id = enemy->id,
            .type = killedType,
            .eliteAffixes = killedAffixes,
            .position = enemy->position,
            .damage = previousHealth - enemy->health,
            .remainingHealth = enemy->health,
            .killed = killed,
        });
        if (killed && killedType == EnemyType::Splitter) {
            pendingSplitBuffer_.push_back({
                .id = enemy->id,
                .position = enemy->position,
                .healthMultiplier = childHealthMultiplier,
                .damageMultiplier = childDamageMultiplier,
            });
        }
        if (killed || knockbackStrength <= 0.0) {
            continue;
        }
        const double areaOffsetX =
            enemy->position.x - position.x;
        const double areaOffsetZ =
            enemy->position.z - position.z;
        const double radialDistance =
            std::sqrt(
                (areaOffsetX * areaOffsetX) +
                (areaOffsetZ * areaOffsetZ));
        double offsetX =
            enemy->position.x - impulseOrigin.x;
        double offsetZ =
            enemy->position.z - impulseOrigin.z;
        double directionDistance =
            std::sqrt((offsetX * offsetX) + (offsetZ * offsetZ));
        if (directionDistance <= 1e-9) {
            offsetX = (enemy->id.index % 2U) == 0U ? -1.0 : 1.0;
            offsetZ = 0.0;
            directionDistance = 1.0;
        }
        const double surfaceDistance = std::max(
            0.0,
            radialDistance - enemyCapsule(enemy->type).radius);
        const double falloff =
            std::max(0.0, 1.0 - surfaceDistance / radius);
        const double impulse =
            knockbackStrength * falloff * knockbackMultiplier(enemy->type);
        enemy->knockbackVelocity.x +=
            (offsetX / directionDistance) * impulse;
        enemy->knockbackVelocity.z +=
            (offsetZ / directionDistance) * impulse;
    }
    // Schedule only after resolving the original area target set. Children
    // are therefore never damaged by the explosion that created them.
    for (const PendingSplit& split : pendingSplitBuffer_) {
        scheduleSplit(
            split.id, split.position,
            split.healthMultiplier,
            split.damageMultiplier);
    }
    return areaDamageBuffer_;
}

std::span<const EntityId> EnemySystem::applySlowInRadius(Vec3 position, double radius,
                                                        double multiplier, double duration) {
    statusTargetBuffer_.clear();
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& entry) {
        EnemyInstance* enemy = findEnemy(entry.id);
        if (enemy == nullptr || !enemy->active) {
            return;
        }
        enemy->movementMultiplier =
            std::min(enemy->movementMultiplier, std::clamp(multiplier, 0.1, 1.0));
        enemy->slowRemaining = std::max(enemy->slowRemaining, duration);
        statusTargetBuffer_.push_back(enemy->id);
    });
    return statusTargetBuffer_;
}

std::span<const EntityId> EnemySystem::knockbackInRadius(
    Vec3 position, double radius, double strength) {
    statusTargetBuffer_.clear();
    if (radius <= 0.0 || strength <= 0.0) {
        return statusTargetBuffer_;
    }
    rebuildSpatialIndex();
    const double queryRadius =
        radius + enemyCapsule(EnemyType::Boss).radius;
    spatialHash_.forEachNearby(
        position, queryRadius, [&](const SpatialEntry& entry) {
            EnemyInstance* enemy = findEnemy(entry.id);
            if (enemy == nullptr || !enemy->active) return;
            double offsetX = enemy->position.x - position.x;
            double offsetZ = enemy->position.z - position.z;
            double distance = std::hypot(offsetX, offsetZ);
            const double surfaceDistance = std::max(
                0.0, distance - enemyCapsule(enemy->type).radius);
            if (surfaceDistance > radius) return;
            if (distance <= 1e-9) {
                offsetX = (enemy->id.index % 2U) == 0U ? -1.0 : 1.0;
                offsetZ = 0.0;
                distance = 1.0;
            }
            const double falloff = std::max(
                0.18, 1.0 - surfaceDistance / radius);
            const double impulse = strength * falloff *
                knockbackMultiplier(enemy->type);
            enemy->knockbackVelocity.x += offsetX / distance * impulse;
            enemy->knockbackVelocity.z += offsetZ / distance * impulse;
            statusTargetBuffer_.push_back(enemy->id);
        });
    return statusTargetBuffer_;
}

bool EnemySystem::applyStatus(
    EntityId id, StatusEffectType requestedType,
    std::optional<EntityId> source, double duration,
    double intensity, StatusEffectRules rules) {
    EnemyInstance* enemy = findEnemy(id);
    if (enemy == nullptr || !enemy->active || duration <= 0.0) {
        return false;
    }

    const bool elite = enemy->eliteAffixes != 0U ||
        enemy->type == EnemyType::Heavy;
    const bool boss = enemy->type == EnemyType::Boss;
    const StatusEffectType appliedType =
        requestedType == StatusEffectType::Freeze && boss
            ? StatusEffectType::Slow
            : requestedType;
    EnemyStatusEffect& status =
        enemy->statusEffects[statusEffectIndex(appliedType)];
    if (status.remaining > 0.0 || status.immunityRemaining > 0.0) {
        // A hit still refreshes the crack cue, but cannot extend an active
        // control effect. This is the immunity/diminishing-return window.
        status.visualParameter = std::max(status.visualParameter, 0.7);
        return false;
    }

    double effectiveDuration = duration;
    if (elite && appliedType == StatusEffectType::Freeze) {
        effectiveDuration *= std::clamp(
            rules.eliteDurationMultiplier, 0.0, 1.0);
    }
    double effectiveIntensity = std::clamp(intensity, 0.0, 1.0);
    if (boss && requestedType == StatusEffectType::Freeze) {
        effectiveIntensity = std::clamp(rules.bossSlowAmount, 0.0, 1.0);
    }
    status.type = appliedType;
    status.source = source;
    status.remaining = effectiveDuration;
    status.intensity = effectiveIntensity;
    status.immunityRemaining = effectiveDuration * std::clamp(
        rules.immunityWindowFraction, 0.0, 1.0);
    status.visualParameter = 1.0;

    if (appliedType == StatusEffectType::Freeze) {
        enemy->knockbackVelocity = {};
        enemy->state = EnemyState::ChasePlayer;
        enemy->target.reset();
    } else {
        const double movementMultiplier = std::clamp(
            1.0 - effectiveIntensity, 0.1, 1.0);
        enemy->movementMultiplier = std::min(
            enemy->movementMultiplier, movementMultiplier);
        enemy->slowRemaining = std::max(
            enemy->slowRemaining, effectiveDuration);
    }
    return true;
}

bool EnemySystem::clearStatus(
    EntityId id, StatusEffectType type) {
    EnemyInstance* enemy = findEnemy(id);
    if (enemy == nullptr || !enemy->active) return false;
    EnemyStatusEffect& status =
        enemy->statusEffects[statusEffectIndex(type)];
    const bool active = status.remaining > 0.0;
    status.remaining = 0.0;
    status.immunityRemaining = 0.0;
    status.visualParameter = 0.0;
    return active;
}

std::span<const EntityId> EnemySystem::applyStatusInRadius(
    Vec3 position, double radius, StatusEffectType type,
    std::optional<EntityId> source, double duration,
    double intensity, StatusEffectRules rules) {
    statusTargetBuffer_.clear();
    rebuildSpatialIndex();
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& entry) {
        EnemyInstance* enemy = findEnemy(entry.id);
        if (enemy == nullptr || !enemy->active) {
            return;
        }
        const double hitRadius = radius + enemyCapsule(enemy->type).radius;
        const double offsetX = enemy->position.x - position.x;
        const double offsetZ = enemy->position.z - position.z;
        if (offsetX * offsetX + offsetZ * offsetZ > hitRadius * hitRadius) {
            return;
        }
        if (applyStatus(enemy->id, type, source, duration, intensity, rules)) {
            statusTargetBuffer_.push_back(enemy->id);
        }
    });
    return statusTargetBuffer_;
}

std::size_t EnemySystem::defeatAll() {
    std::size_t defeated = 0;
    for (auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }
        enemy.health = 0.0;
        markEnemyDead(enemy);
        ++defeated;
    }
    delayedSplitBuffer_.clear();
    for (EnemyInstance& enemy : enemies_) {
        enemy.splitAnimationRemaining = 0.0;
    }
    rebuildSpatialIndex();
    return defeated;
}

std::size_t EnemySystem::activeCount() const {
    return activeCount_;
}

const std::vector<EnemyInstance>& EnemySystem::enemies() const {
    return enemies_;
}

std::span<const EnemyProjectile> EnemySystem::projectiles() const {
    return projectiles_;
}

void EnemySystem::clearProjectiles() {
    projectiles_.clear();
}

std::span<const EnemyPlayerAttack> EnemySystem::playerAttacks() const {
    return playerAttackBuffer_;
}

const EnemyPerformanceStats& EnemySystem::performanceStats() const {
    return performanceStats_;
}

std::vector<EnemySplitResult> EnemySystem::takeSplitEvents() {
    return std::exchange(splitEventBuffer_, {});
}

std::vector<EliteEnemyEvent> EnemySystem::takeEliteSpawnEvents() {
    return std::exchange(eliteSpawnEventBuffer_, {});
}

std::vector<EliteEnemyEvent> EnemySystem::takeEliteDeathEvents() {
    return std::exchange(eliteDeathEventBuffer_, {});
}

std::vector<BossActionEvent> EnemySystem::takeBossActionEvents() {
    return std::exchange(bossActionEventBuffer_, {});
}

const EnemyStatusEffect& enemyStatusEffect(
    const EnemyInstance& enemy, StatusEffectType type) {
    return enemy.statusEffects[statusEffectIndex(type)];
}

bool enemyHasStatus(const EnemyInstance& enemy, StatusEffectType type) {
    return enemyStatusEffect(enemy, type).remaining > 0.0;
}


} // namespace ian
