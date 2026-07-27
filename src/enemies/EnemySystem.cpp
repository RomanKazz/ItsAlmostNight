#include "enemies/EnemySystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double EnemyRadius = 0.4;
constexpr double AttackRange = 0.55;
constexpr double PlayerAttackRange = 1.0;
constexpr double AttackInterval = 1.0;
constexpr double BuildingLookAhead = 2.5;
constexpr double SeparationRadius = 1.1;
constexpr double SeparationWeight = 0.65;

double enemyRadius(EnemyType type) {
    return type == EnemyType::Boss ? 1.0 : EnemyRadius;
}

double knockbackMultiplier(EnemyType type) {
    switch (type) {
    case EnemyType::Basic:
        return 1.0;
    case EnemyType::Fast:
        return 1.15;
    case EnemyType::Heavy:
        return 0.3;
    case EnemyType::Boss:
        return 0.15;
    }
    return 1.0;
}

double buildingRadius(BuildingType type) {
    return type == BuildingType::Core ? 1.6 : 0.55;
}

double dot(Vec3 left, Vec3 right) {
    return (left.x * right.x) + (left.y * right.y) + (left.z * right.z);
}

Vec3 subtract(Vec3 left, Vec3 right) {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

std::optional<double> raySphereDistance(Vec3 origin, Vec3 direction, Vec3 center, double radius) {
    const Vec3 offset = subtract(origin, center);
    const double halfB = dot(offset, direction);
    const double c = dot(offset, offset) - (radius * radius);
    const double discriminant = (halfB * halfB) - c;
    if (discriminant < 0.0) {
        return std::nullopt;
    }

    const double root = std::sqrt(discriminant);
    const double nearDistance = -halfB - root;
    if (nearDistance >= 0.0) {
        return nearDistance;
    }
    const double farDistance = -halfB + root;
    return farDistance >= 0.0 ? std::optional<double>{farDistance} : std::nullopt;
}

} // namespace

EnemySystem::EnemySystem(
    std::array<EnemyDefinition, GameBalance::EnemyTypeCount> definitions)
    : definitions_(definitions) {
    enemies_.reserve(MaxEnemies);
    attackBuffer_.reserve(MaxEnemies);
    playerAttackBuffer_.reserve(MaxEnemies);
    areaDamageBuffer_.reserve(MaxEnemies);
    statusTargetBuffer_.reserve(MaxEnemies);
}

void EnemySystem::reset() {
    enemies_.clear();
    attackBuffer_.clear();
    playerAttackBuffer_.clear();
    areaDamageBuffer_.clear();
    statusTargetBuffer_.clear();
    nextIndex_ = 2000;
    spatialHash_.clear();
}

void EnemySystem::spawnWave(std::span<const Vec3> positions) {
    enemies_.clear();
    for (const Vec3 position : positions) {
        appendEnemy(EnemyType::Basic, position);
    }
    rebuildSpatialIndex();
}

void EnemySystem::spawnWave(std::span<const EnemySpawn> spawns) {
    enemies_.clear();
    spawnGroup(spawns);
}

void EnemySystem::spawnGroup(std::span<const EnemySpawn> spawns) {
    for (const EnemySpawn& spawn : spawns) {
        appendEnemy(spawn.type, spawn.position);
    }
    rebuildSpatialIndex();
}

std::span<const EnemyAttack> EnemySystem::tick(
    double deltaSeconds, const std::vector<BuildingInstance>& buildings,
    const FlowField& flowField, std::optional<Vec3> playerPosition) {
    attackBuffer_.clear();
    playerAttackBuffer_.clear();
    const auto core =
        std::find_if(buildings.begin(), buildings.end(), [](const BuildingInstance& building) {
            return building.type == BuildingType::Core;
        });
    if (core == buildings.end()) {
        return attackBuffer_;
    }

    rebuildSpatialIndex();

    for (auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }

        enemy.attackCooldownRemaining =
            std::max(0.0, enemy.attackCooldownRemaining - deltaSeconds);
        enemy.ramCooldownRemaining =
            std::max(0.0, enemy.ramCooldownRemaining - deltaSeconds);
        enemy.slowRemaining = std::max(0.0, enemy.slowRemaining - deltaSeconds);
        if (enemy.slowRemaining <= 0.0) {
            enemy.movementMultiplier = 1.0;
        }
        const double movementSpeed = enemy.speed * enemy.movementMultiplier;
        enemy.position.x += enemy.knockbackVelocity.x * deltaSeconds;
        enemy.position.z += enemy.knockbackVelocity.z * deltaSeconds;
        const double knockbackDecay = std::max(0.0, 1.0 - 5.0 * deltaSeconds);
        enemy.knockbackVelocity.x *= knockbackDecay;
        enemy.knockbackVelocity.z *= knockbackDecay;

        if (enemy.state == EnemyState::BossRamWindup) {
            const auto target =
                std::find_if(buildings.begin(), buildings.end(),
                             [&enemy](const BuildingInstance& building) {
                                 return enemy.target && building.id == *enemy.target;
                             });
            if (target == buildings.end()) {
                enemy.state = EnemyState::MoveToCore;
                enemy.target.reset();
                enemy.ramWindupRemaining = 0.0;
                continue;
            }
            enemy.ramWindupRemaining =
                std::max(0.0, enemy.ramWindupRemaining - deltaSeconds);
            if (enemy.ramWindupRemaining <= 0.0) {
                attackBuffer_.push_back({
                    enemy.id,
                    target->id,
                    enemy.damage * enemy.ramDamageMultiplier,
                    true,
                });
                enemy.state = target->type == BuildingType::Core
                                  ? EnemyState::AttackCore
                                  : EnemyState::AttackBuilding;
                enemy.attackCooldownRemaining = AttackInterval;
                enemy.ramCooldownRemaining = enemy.ramCooldown;
            }
            continue;
        }

        if (playerPosition) {
            const double playerOffsetX = playerPosition->x - enemy.position.x;
            const double playerOffsetZ = playerPosition->z - enemy.position.z;
            const double playerDistanceSquared =
                (playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ);
            if (playerDistanceSquared <= PlayerAttackRange * PlayerAttackRange) {
                enemy.state = EnemyState::AttackPlayer;
                enemy.target.reset();
                if (enemy.attackCooldownRemaining <= 0.0) {
                    playerAttackBuffer_.push_back({enemy.id, enemy.damage});
                    enemy.attackCooldownRemaining = AttackInterval;
                }
                continue;
            }
        }

        const double coreX = static_cast<double>(core->gridPosition.x);
        const double coreZ = static_cast<double>(core->gridPosition.z);
        const double toCoreX = coreX - enemy.position.x;
        const double toCoreZ = coreZ - enemy.position.z;
        const double coreDistance = std::sqrt((toCoreX * toCoreX) + (toCoreZ * toCoreZ));
        if (coreDistance <= 1e-9) {
            continue;
        }
        double directionX = toCoreX / coreDistance;
        double directionZ = toCoreZ / coreDistance;
        const auto flowDirection = flowField.directionAt(enemy.position);
        if (flowDirection) {
            const double flowLength = std::sqrt((flowDirection->x * flowDirection->x) +
                                                (flowDirection->z * flowDirection->z));
            if (flowLength > 1e-9) {
                directionX = flowDirection->x / flowLength;
                directionZ = flowDirection->z / flowLength;
            }
        }

        double separationX = 0.0;
        double separationZ = 0.0;
        spatialHash_.forEachNearby(
            enemy.position, SeparationRadius, [&enemy, &separationX, &separationZ](
                                                  const SpatialEntry& neighbor) {
                if (neighbor.id == enemy.id) {
                    return;
                }
                double offsetX = enemy.position.x - neighbor.position.x;
                double offsetZ = enemy.position.z - neighbor.position.z;
                double distance =
                    std::sqrt((offsetX * offsetX) + (offsetZ * offsetZ));
                if (distance <= 1e-9) {
                    offsetX = enemy.id.index < neighbor.id.index ? -1.0 : 1.0;
                    offsetZ = 0.0;
                    distance = 1.0;
                }
                const double strength =
                    std::max(0.0, (SeparationRadius - distance) / SeparationRadius);
                separationX += (offsetX / distance) * strength;
                separationZ += (offsetZ / distance) * strength;
            });

        const double separationLength =
            std::sqrt((separationX * separationX) + (separationZ * separationZ));
        if (separationLength > 1e-9) {
            directionX += (separationX / separationLength) * SeparationWeight;
            directionZ += (separationZ / separationLength) * SeparationWeight;
            const double combinedLength =
                std::sqrt((directionX * directionX) + (directionZ * directionZ));
            directionX /= combinedLength;
            directionZ /= combinedLength;
        }

        const BuildingInstance* blocker = nullptr;
        double closestContactDistance = std::numeric_limits<double>::max();
        for (const auto& building : buildings) {
            if (!buildingBlocksMovement(building)) {
                continue;
            }
            const double offsetX =
                static_cast<double>(building.gridPosition.x) - enemy.position.x;
            const double offsetZ =
                static_cast<double>(building.gridPosition.z) - enemy.position.z;
            const double projection = (offsetX * directionX) + (offsetZ * directionZ);
            if (projection < 0.0 ||
                projection > BuildingLookAhead + buildingRadius(building.type)) {
                continue;
            }

            const double perpendicularX = offsetX - directionX * projection;
            const double perpendicularZ = offsetZ - directionZ * projection;
            const double perpendicularDistance =
                std::sqrt((perpendicularX * perpendicularX) + (perpendicularZ * perpendicularZ));
            const double combinedRadius = buildingRadius(building.type) + enemyRadius(enemy.type);
            if (perpendicularDistance > combinedRadius) {
                continue;
            }

            const double contactDistance = std::max(0.0, projection - combinedRadius);
            if (contactDistance < closestContactDistance) {
                blocker = &building;
                closestContactDistance = contactDistance;
            }
        }

        if (blocker == nullptr) {
            enemy.state = EnemyState::MoveToCore;
            enemy.target.reset();
            enemy.position.x += directionX * movementSpeed * deltaSeconds;
            enemy.position.z += directionZ * movementSpeed * deltaSeconds;
            continue;
        }

        if (closestContactDistance > AttackRange) {
            const double movement =
                std::min(movementSpeed * deltaSeconds, closestContactDistance - AttackRange);
            enemy.position.x += directionX * movement;
            enemy.position.z += directionZ * movement;
            enemy.state = EnemyState::MoveToCore;
            enemy.target.reset();
            continue;
        }

        enemy.target = blocker->id;
        if (enemy.type == EnemyType::Boss && enemy.ramCooldownRemaining <= 0.0) {
            enemy.state = EnemyState::BossRamWindup;
            enemy.ramWindupRemaining = enemy.ramWindup;
            continue;
        }
        enemy.state = blocker->type == BuildingType::Core ? EnemyState::AttackCore
                                                          : EnemyState::AttackBuilding;
        if (enemy.attackCooldownRemaining <= 0.0) {
            attackBuffer_.push_back({enemy.id, blocker->id, enemy.damage, false});
            enemy.attackCooldownRemaining = AttackInterval;
        }
    }

    rebuildSpatialIndex();
    return attackBuffer_;
}

std::optional<EntityId> EnemySystem::raycast(Vec3 origin, Vec3 direction,
                                             double maxDistance) const {
    std::optional<EntityId> result;
    double closestDistance = std::numeric_limits<double>::max();
    for (const auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }

        const double radius = enemy.type == EnemyType::Boss ? 1.25 : 0.65;
        const auto distance = raySphereDistance(origin, direction, enemy.position, radius);
        if (distance && *distance <= maxDistance && *distance < closestDistance) {
            result = enemy.id;
            closestDistance = *distance;
        }
    }
    return result;
}

std::optional<EnemyDamageResult> EnemySystem::damage(EntityId id, double amount) {
    const auto iterator = std::find_if(enemies_.begin(), enemies_.end(),
                                       [id](const EnemyInstance& enemy) {
                                           return enemy.id == id;
                                       });
    if (iterator == enemies_.end() || !iterator->active || amount <= 0.0) {
        return std::nullopt;
    }

    iterator->health = std::max(0.0, iterator->health - amount);
    const bool killed = iterator->health <= 0.0;
    if (killed) {
        iterator->active = false;
        iterator->state = EnemyState::Dead;
        iterator->target.reset();
        rebuildSpatialIndex();
    }
    return EnemyDamageResult{
        .id = iterator->id,
        .position = iterator->position,
        .remainingHealth = iterator->health,
        .killed = killed,
    };
}

std::optional<EntityId> EnemySystem::nearestEnemy(Vec3 position, double radius) const {
    std::optional<EntityId> nearest;
    double nearestDistanceSquared = radius * radius;
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& entry) {
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

std::optional<EntityId> EnemySystem::densestEnemy(Vec3 position, double radius,
                                                  double clusterRadius) const {
    std::optional<EntityId> best;
    std::size_t bestCount = 0;
    double bestDistanceSquared = radius * radius;
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& candidate) {
        std::size_t count = 0;
        spatialHash_.forEachNearby(candidate.position, clusterRadius,
                                   [&count](const SpatialEntry&) { ++count; });
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

std::optional<EnemyInstance> EnemySystem::enemy(EntityId id) const {
    const auto iterator =
        std::find_if(enemies_.begin(), enemies_.end(),
                     [id](const EnemyInstance& instance) { return instance.id == id; });
    if (iterator == enemies_.end() || !iterator->active) {
        return std::nullopt;
    }
    return *iterator;
}

std::span<const EnemyDamageResult> EnemySystem::damageInRadius(Vec3 position, double radius,
                                                               double amount,
                                                               double knockbackStrength) {
    areaDamageBuffer_.clear();
    std::size_t targetCount = 0;
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& entry) {
        if (targetCount < areaTargetBuffer_.size()) {
            areaTargetBuffer_[targetCount++] = entry.id;
        }
    });
    for (std::size_t index = 0; index < targetCount; ++index) {
        const auto target = enemy(areaTargetBuffer_[index]);
        const auto result = damage(areaTargetBuffer_[index], amount);
        if (result) {
            areaDamageBuffer_.push_back(*result);
        }
        if (!target || !result || result->killed || knockbackStrength <= 0.0) {
            continue;
        }
        const auto iterator =
            std::find_if(enemies_.begin(), enemies_.end(), [&result](const EnemyInstance& enemy) {
                return enemy.id == result->id;
            });
        if (iterator == enemies_.end()) {
            continue;
        }
        double offsetX = target->position.x - position.x;
        double offsetZ = target->position.z - position.z;
        double distance = std::sqrt((offsetX * offsetX) + (offsetZ * offsetZ));
        if (distance <= 1e-9) {
            offsetX = (target->id.index % 2U) == 0U ? -1.0 : 1.0;
            offsetZ = 0.0;
            distance = 1.0;
        }
        const double falloff = std::max(0.0, 1.0 - distance / radius);
        const double impulse =
            knockbackStrength * falloff * knockbackMultiplier(iterator->type);
        iterator->knockbackVelocity.x += (offsetX / distance) * impulse;
        iterator->knockbackVelocity.z += (offsetZ / distance) * impulse;
    }
    return areaDamageBuffer_;
}

std::span<const EntityId> EnemySystem::applySlowInRadius(Vec3 position, double radius,
                                                        double multiplier, double duration) {
    statusTargetBuffer_.clear();
    spatialHash_.forEachNearby(position, radius, [&](const SpatialEntry& entry) {
        const auto iterator =
            std::find_if(enemies_.begin(), enemies_.end(),
                         [&entry](const EnemyInstance& enemy) { return enemy.id == entry.id; });
        if (iterator == enemies_.end() || !iterator->active) {
            return;
        }
        iterator->movementMultiplier =
            std::min(iterator->movementMultiplier, std::clamp(multiplier, 0.1, 1.0));
        iterator->slowRemaining = std::max(iterator->slowRemaining, duration);
        statusTargetBuffer_.push_back(iterator->id);
    });
    return statusTargetBuffer_;
}

std::size_t EnemySystem::defeatAll() {
    std::size_t defeated = 0;
    for (auto& enemy : enemies_) {
        if (!enemy.active) {
            continue;
        }
        enemy.active = false;
        enemy.health = 0.0;
        enemy.state = EnemyState::Dead;
        enemy.target.reset();
        ++defeated;
    }
    rebuildSpatialIndex();
    return defeated;
}

std::size_t EnemySystem::activeCount() const {
    return static_cast<std::size_t>(
        std::count_if(enemies_.begin(), enemies_.end(),
                      [](const EnemyInstance& enemy) { return enemy.active; }));
}

const std::vector<EnemyInstance>& EnemySystem::enemies() const {
    return enemies_;
}

std::span<const EnemyPlayerAttack> EnemySystem::playerAttacks() const {
    return playerAttackBuffer_;
}

void EnemySystem::appendEnemy(EnemyType type, Vec3 position) {
    const EnemyDefinition stats = definitions_[static_cast<std::size_t>(type)];
    const auto reusable =
        std::find_if(enemies_.begin(), enemies_.end(),
                     [](const EnemyInstance& enemy) { return !enemy.active; });
    if (reusable == enemies_.end() && enemies_.size() >= MaxEnemies) {
        return;
    }
    const EntityId id =
        reusable == enemies_.end()
            ? EntityId{nextIndex_++, 1}
            : EntityId{reusable->id.index, reusable->id.generation + 1};
    const EnemyInstance instance{
        .id = id,
        .type = type,
        .position = position,
        .health = stats.health,
        .maxHealth = stats.health,
        .speed = stats.speed,
        .damage = stats.damage,
        .attackCooldownRemaining = 0.0,
        .ramWindup = stats.ramWindup,
        .ramDamageMultiplier = stats.ramDamageMultiplier,
        .ramCooldown = stats.ramCooldown,
        .ramWindupRemaining = 0.0,
        .ramCooldownRemaining = 0.0,
        .slowRemaining = 0.0,
        .movementMultiplier = 1.0,
        .knockbackVelocity = {},
        .state = EnemyState::Spawn,
        .target = std::nullopt,
        .active = true,
    };
    if (reusable == enemies_.end()) {
        enemies_.push_back(instance);
    } else {
        *reusable = instance;
    }
}

void EnemySystem::rebuildSpatialIndex() {
    spatialHash_.clear();
    for (const auto& enemy : enemies_) {
        if (enemy.active) {
            spatialHash_.insert(enemy.id, enemy.position);
        }
    }
}

} // namespace ian
