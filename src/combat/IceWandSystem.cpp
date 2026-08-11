#include "combat/IceWandSystem.hpp"

#include "buildings/BuildingSystem.hpp"
#include "core/Geometry.hpp"
#include "enemies/EnemyCollision.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/MapDefinition.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double MinimumDistanceSquared = 1e-12;

using geometry::add;
using geometry::scale;
using geometry::subtract;

std::optional<double> segmentCircleTime(
    Vec3 start, Vec3 end, Vec3 center, double radius) {
    const double dx = end.x - start.x;
    const double dz = end.z - start.z;
    const double ox = start.x - center.x;
    const double oz = start.z - center.z;
    const double a = dx * dx + dz * dz;
    const double c = ox * ox + oz * oz - radius * radius;
    if (a <= MinimumDistanceSquared) {
        return c <= 0.0 ? std::optional<double>{0.0} : std::nullopt;
    }
    const double b = 2.0 * (ox * dx + oz * dz);
    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0) {
        return std::nullopt;
    }
    const double root = std::sqrt(discriminant);
    const double first = (-b - root) / (2.0 * a);
    const double second = (-b + root) / (2.0 * a);
    if (first >= 0.0 && first <= 1.0) {
        return first;
    }
    if (second >= 0.0 && second <= 1.0) {
        return second;
    }
    return std::nullopt;
}

} // namespace

IceWandSystem::IceWandSystem(IceWandBalanceDefinition definition)
    : definition_(definition) {
    reset();
}

IceWandSystem::IceWandSystem(FireWandBalanceDefinition definition)
    : definition_{
          definition.cooldown,
          definition.directDamage,
          definition.projectileSpeed,
          definition.projectileRadius,
          definition.maxLifetime,
          definition.explosionRadius,
          1.0,
          1.0,
          0.0,
          definition.chargeUpDuration,
          definition.areaDamageMultiplier,
      },
      fireDefinition_(definition), element_(WandElement::Fire),
      firstProjectileIndex_(6100) {
    reset();
}

void IceWandSystem::reset() {
    for (IceWandProjectile& projectile : projectiles_) {
        projectile = {};
    }
    for (BurningEnemy& burning : burningEnemies_) {
        burning = {};
    }
    generations_.fill(0);
    launchCount_ = 0;
    hitCount_ = 0;
    impactCount_ = 0;
    nextSlot_ = 0;
    cooldownRemaining_ = 0.0;
    chargeRemaining_ = 0.0;
    chargeOrigin_ = {};
    chargeDirection_ = {0.0, 0.0, -1.0};
    charging_ = false;
}

void IceWandSystem::setSkillModifiers(
    double damage, double radius, double statusDuration,
    double burnDamage, double thermalShockDamage) {
    damageMultiplier_ = std::max(0.05, damage);
    radiusMultiplier_ = std::max(0.05, radius);
    statusDurationMultiplier_ = std::max(0.05, statusDuration);
    burnDamageMultiplier_ = std::max(0.05, burnDamage);
    thermalShockDamage_ = std::max(0.0, thermalShockDamage);
}

bool IceWandSystem::requestFire(Vec3 origin, Vec3 direction) {
    if (charging_ || cooldownRemaining_ > 0.0) {
        return false;
    }
    chargeOrigin_ = origin;
    chargeDirection_ = geometry::normalizedOr(direction);
    chargeRemaining_ = definition_.chargeUpDuration;
    charging_ = true;
    return true;
}

void IceWandSystem::tick(
    double deltaSeconds, EnemySystem& enemies,
    const TerrainHeightfield* terrain,
    std::span<const BuildingInstance> buildings,
    std::span<const MapObstacle> obstacles) {
    launchCount_ = 0;
    hitCount_ = 0;
    impactCount_ = 0;
    updateBurning(deltaSeconds, enemies);
    cooldownRemaining_ = std::max(
        0.0, cooldownRemaining_ - deltaSeconds);

    if (charging_) {
        chargeRemaining_ = std::max(
            0.0, chargeRemaining_ - deltaSeconds);
        if (chargeRemaining_ <= 0.0) {
            spawnProjectile();
            charging_ = false;
            cooldownRemaining_ = definition_.cooldown;
        }
    }

    for (IceWandProjectile& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }
        projectile.previousPosition = projectile.position;
        projectile.position = add(
            projectile.position,
            scale(projectile.velocity, deltaSeconds));
        projectile.age += deltaSeconds;
        for (std::size_t index = IceWandTrailPointCount - 1;
             index > 0; --index) {
            projectile.trail[index] = projectile.trail[index - 1];
        }
        projectile.trail[0] = projectile.position;
        projectile.trailCount = std::min(
            IceWandTrailPointCount, projectile.trailCount + 1);

        const auto enemyHit = sweepEnemy(
            projectile.previousPosition, projectile.position,
            projectile.radius, enemies);
        const auto buildingHit = sweepBuilding(
            projectile.previousPosition, projectile.position,
            projectile.radius, buildings);
        const auto terrainHit = sweepTerrain(
            projectile.previousPosition, projectile.position,
            projectile.radius, terrain);
        const auto obstacleHit = sweepObstacles(
            projectile.previousPosition, projectile.position,
            projectile.radius, obstacles);

        double bestSurfaceTime = std::numeric_limits<double>::infinity();
        if (buildingHit) {
            bestSurfaceTime = std::min(bestSurfaceTime, *buildingHit);
        }
        if (terrainHit) {
            bestSurfaceTime = std::min(bestSurfaceTime, *terrainHit);
        }
        if (obstacleHit) {
            bestSurfaceTime = std::min(bestSurfaceTime, *obstacleHit);
        }
        if (enemyHit && enemyHit->time <= bestSurfaceTime) {
            impactProjectile(projectile, enemyHit->position,
                              enemyHit->id, enemies);
            continue;
        }
        if (std::isfinite(bestSurfaceTime)) {
            const Vec3 movement = subtract(
                projectile.position, projectile.previousPosition);
            const Vec3 impactPosition = add(
                projectile.previousPosition,
                scale(movement, bestSurfaceTime));
            impactProjectile(projectile, impactPosition, std::nullopt, enemies);
            continue;
        }
        if (projectile.age >= projectile.lifetime) {
            impactProjectile(projectile, projectile.position,
                             std::nullopt, enemies);
        }
    }
}

void IceWandSystem::clearProjectiles() {
    for (IceWandProjectile& projectile : projectiles_) {
        projectile.active = false;
        projectile.trailCount = 0;
    }
    charging_ = false;
    chargeRemaining_ = 0.0;
}

std::span<const IceWandProjectile> IceWandSystem::projectiles() const {
    return projectiles_;
}

std::span<const IceWandLaunch> IceWandSystem::launches() const {
    return {launchBuffer_.data(), launchCount_};
}

std::span<const IceWandHit> IceWandSystem::hits() const {
    return {hitBuffer_.data(), hitCount_};
}

std::span<const IceWandImpact> IceWandSystem::impacts() const {
    return {impactBuffer_.data(), impactCount_};
}

double IceWandSystem::chargeRemaining() const { return chargeRemaining_; }

double IceWandSystem::chargeDuration() const {
    return definition_.chargeUpDuration;
}

double IceWandSystem::cooldownRemaining() const {
    return cooldownRemaining_;
}

double IceWandSystem::directDamage() const {
    return definition_.directDamage * damageMultiplier_;
}

double IceWandSystem::maximumRange() const {
    return definition_.projectileSpeed * definition_.maxLifetime;
}

double IceWandSystem::burnDuration() const {
    return element_ == WandElement::Fire
        ? fireDefinition_.burnDuration * statusDurationMultiplier_
        : 0.0;
}

std::optional<IceWandSystem::EnemySweepHit> IceWandSystem::sweepEnemy(
    Vec3 start, Vec3 end, double radius,
    const EnemySystem& enemies) const {
    std::optional<EnemySweepHit> best;
    for (const EnemyInstance& enemy : enemies.enemies()) {
        if (!enemy.active) {
            continue;
        }
        const EnemyCapsule capsule = enemyCapsule(enemy.type);
        const auto time = segmentCircleTime(
            start, end, enemy.position,
            capsule.radius + radius);
        if (!time) {
            continue;
        }
        const double y = start.y + (end.y - start.y) * *time;
        if (std::abs(y - enemy.position.y) >
            capsule.segmentHalfHeight + radius) {
            continue;
        }
        if (!best || *time < best->time) {
            const Vec3 movement = subtract(end, start);
            best = EnemySweepHit{
                .id = enemy.id,
                .time = *time,
                .position = add(start, scale(movement, *time)),
            };
        }
    }
    return best;
}

std::optional<double> IceWandSystem::sweepBuilding(
    Vec3 start, Vec3 end, double radius,
    std::span<const BuildingInstance> buildings) const {
    std::optional<double> best;
    for (const BuildingInstance& building : buildings) {
        if (!buildingBlocksMovement(building)) {
            continue;
        }
        const Vec3 center = buildingWorldPosition(building);
        const double buildingRadius =
            building.type == BuildingType::Core ? 1.6 :
            buildingFootprintHalfExtent(building.type) == 1.0 ? 1.1 : 0.55;
        const auto time = segmentCircleTime(
            start, end, center, buildingRadius + radius);
        if (!time) {
            continue;
        }
        const double y = start.y + (end.y - start.y) * *time;
        if (y < building.baseHeight - radius ||
            y > building.baseHeight + 2.5 + radius) {
            continue;
        }
        if (!best || *time < *best) {
            best = *time;
        }
    }
    return best;
}

std::optional<double> IceWandSystem::sweepTerrain(
    Vec3 start, Vec3 end, double radius,
    const TerrainHeightfield* terrain) const {
    if (terrain == nullptr || !terrain->isInside(end.x, end.z)) {
        return std::nullopt;
    }
    const double endSurface =
        terrain->getHeight(end.x, end.z) + radius;
    const double startSurface = terrain->isInside(start.x, start.z)
        ? terrain->getHeight(start.x, start.z) + radius
        : endSurface;
    if (start.y <= startSurface) {
        return 0.0;
    }
    if (end.y > endSurface) {
        return std::nullopt;
    }
    const double denominator = (start.y - end.y) -
        (startSurface - endSurface);
    if (std::abs(denominator) <= 1e-9) {
        return 1.0;
    }
    return std::clamp(
        (start.y - startSurface) / denominator, 0.0, 1.0);
}

std::optional<double> IceWandSystem::sweepObstacles(
    Vec3 start, Vec3 end, double radius,
    std::span<const MapObstacle> obstacles) const {
    std::optional<double> best;
    const Vec3 movement = subtract(end, start);
    for (const MapObstacle& obstacle : obstacles) {
        const double minimumX = obstacle.collision.minX - radius;
        const double maximumX = obstacle.collision.maxX + radius;
        const double minimumZ = obstacle.collision.minZ - radius;
        const double maximumZ = obstacle.collision.maxZ + radius;
        const double minimumY = -radius;
        const double maximumY = obstacle.height + radius;
        double enter = 0.0;
        double exit = 1.0;
        const auto clip = [&](double startValue, double direction,
                              double minimum, double maximum) {
            if (std::abs(direction) <= 1e-9) {
                return startValue >= minimum && startValue <= maximum;
            }
            const double inverse = 1.0 / direction;
            double first = (minimum - startValue) * inverse;
            double second = (maximum - startValue) * inverse;
            if (first > second) {
                std::swap(first, second);
            }
            enter = std::max(enter, first);
            exit = std::min(exit, second);
            return enter <= exit;
        };
        if (!clip(start.x, movement.x, minimumX, maximumX) ||
            !clip(start.y, movement.y, minimumY, maximumY) ||
            !clip(start.z, movement.z, minimumZ, maximumZ)) {
            continue;
        }
        if (!best || enter < *best) {
            best = std::clamp(enter, 0.0, 1.0);
        }
    }
    return best;
}

void IceWandSystem::spawnProjectile() {
    IceWandProjectile* projectile = nullptr;
    for (std::size_t offset = 0; offset < projectiles_.size(); ++offset) {
        const std::size_t slot =
            (nextSlot_ + offset) % projectiles_.size();
        if (!projectiles_[slot].active) {
            nextSlot_ = (slot + 1) % projectiles_.size();
            projectile = &projectiles_[slot];
            const std::uint32_t generation = ++generations_[slot];
            projectile->id = {
                firstProjectileIndex_ + static_cast<std::uint32_t>(slot),
                generation,
            };
            break;
        }
    }
    if (projectile == nullptr) {
        return;
    }
    projectile->previousPosition = chargeOrigin_;
    projectile->position = add(
        chargeOrigin_, scale(chargeDirection_, 0.55));
    projectile->previousPosition = projectile->position;
    projectile->velocity = scale(
        chargeDirection_, definition_.projectileSpeed);
    projectile->trail.fill(projectile->position);
    projectile->trailCount = 1;
    projectile->age = 0.0;
    projectile->lifetime = definition_.maxLifetime;
    projectile->radius = definition_.projectileRadius;
    projectile->element = element_;
    projectile->active = true;
    if (launchCount_ < launchBuffer_.size()) {
        launchBuffer_[launchCount_++] = {
            .projectileId = projectile->id,
            .position = projectile->position,
        };
    }
}

void IceWandSystem::impactProjectile(
    IceWandProjectile& projectile, Vec3 impactPosition,
    std::optional<EntityId> directTarget, EnemySystem& enemies) {
    int hitCount = 0;
    int killedCount = 0;
    const auto recordDamage = [&](const EnemyDamageResult& result) {
        const auto currentEnemy = enemies.enemy(result.id);
        const bool frozenBeforeHit = currentEnemy &&
            enemyHasStatus(*currentEnemy, StatusEffectType::Freeze);
        const bool alreadyAffected = element_ == WandElement::Ice
            ? currentEnemy && enemyHasStatus(
                  *currentEnemy, StatusEffectType::Freeze)
            : isBurning(result.id);
        recordHit(projectile, result, alreadyAffected);
        ++hitCount;
        if (result.killed) {
            ++killedCount;
        }
        if (!result.killed && element_ == WandElement::Ice) {
            (void)enemies.applyStatus(
                result.id, StatusEffectType::Freeze, projectile.id,
                definition_.freezeDuration *
                    statusDurationMultiplier_, 1.0,
                StatusEffectRules{
                    .eliteDurationMultiplier = definition_.eliteFreezeMultiplier,
                    .bossSlowAmount = definition_.bossSlowAmount,
                });
        } else if (!result.killed) {
            if (frozenBeforeHit && thermalShockDamage_ > 0.0 &&
                enemies.clearStatus(
                    result.id, StatusEffectType::Freeze)) {
                if (const auto thermal = enemies.damage(
                        result.id, thermalShockDamage_)) {
                    recordHit(projectile, *thermal, true);
                    if (thermal->killed) ++killedCount;
                }
            }
            applyBurn(result.id, projectile.id, enemies);
        }
    };

    // Resolve the splash against the pre-impact population first. A lethal
    // direct hit may spawn Splitlings; those children belong to the result of
    // this impact and must not be picked up by its own subsequent AoE query.
    const auto splash = enemies.damageInRadius(
        impactPosition,
        definition_.explosionRadius * radiusMultiplier_,
        definition_.directDamage * damageMultiplier_ *
            definition_.areaDamageMultiplier,
        0.0, std::nullopt, 0.0, directTarget);
    for (const auto& result : splash) {
        recordDamage(result);
    }
    if (directTarget) {
        if (const auto direct = enemies.damage(
                *directTarget,
                definition_.directDamage * damageMultiplier_)) {
            recordDamage(*direct);
        }
    }
    if (impactCount_ < impactBuffer_.size()) {
        impactBuffer_[impactCount_++] = {
            .projectileId = projectile.id,
            .position = impactPosition,
            .hitCount = hitCount,
            .killedCount = killedCount,
        };
    }
    projectile.active = false;
    projectile.position = impactPosition;
}

void IceWandSystem::recordHit(
    const IceWandProjectile& projectile,
    const EnemyDamageResult& result,
    bool alreadyFrozen) {
    if (hitCount_ >= hitBuffer_.size()) {
        return;
    }
    hitBuffer_[hitCount_] = {
        .projectileId = projectile.id,
        .enemyId = result.id,
        .position = result.position,
        .damage = result.damage,
        .killed = result.killed,
        .alreadyFrozen = alreadyFrozen,
        .periodicBurn = false,
    };
    ++hitCount_;
}

void IceWandSystem::updateBurning(
    double deltaSeconds, EnemySystem& enemies) {
    if (element_ != WandElement::Fire) {
        return;
    }
    for (BurningEnemy& burning : burningEnemies_) {
        if (!burning.active) {
            continue;
        }
        const double activeDelta = std::min(
            deltaSeconds, burning.remaining);
        burning.remaining = std::max(
            0.0, burning.remaining - deltaSeconds);
        burning.tickRemaining -= activeDelta;
        const auto enemy = enemies.enemy(burning.enemyId);
        if (!enemy || !enemy->active) {
            burning.active = false;
            continue;
        }
        while (burning.active && burning.tickRemaining <= 1e-9 &&
               activeDelta > 0.0) {
            const double damage =
                fireDefinition_.burnDamagePerSecond *
                burnDamageMultiplier_ *
                fireDefinition_.burnTickInterval;
            const auto result = enemies.damage(burning.enemyId, damage);
            if (!result) {
                burning.active = false;
                break;
            }
            if (hitCount_ < hitBuffer_.size()) {
                hitBuffer_[hitCount_++] = {
                    .projectileId = burning.sourceProjectileId,
                    .enemyId = result->id,
                    .position = result->position,
                    .damage = result->damage,
                    .killed = result->killed,
                    .alreadyFrozen = false,
                    .periodicBurn = true,
                };
            }
            if (result->killed) {
                burning.active = false;
                break;
            }
            burning.tickRemaining +=
                fireDefinition_.burnTickInterval;
        }
        if (burning.remaining <= 0.0) {
            burning.active = false;
        }
    }
}

void IceWandSystem::applyBurn(
    EntityId enemyId, EntityId sourceProjectileId,
    const EnemySystem& enemies) {
    const auto enemy = enemies.enemy(enemyId);
    if (!enemy || !enemy->active) {
        return;
    }
    BurningEnemy* available = nullptr;
    for (BurningEnemy& burning : burningEnemies_) {
        if (burning.active && burning.enemyId == enemyId) {
            available = &burning;
            break;
        }
        if (!burning.active && available == nullptr) {
            available = &burning;
        }
    }
    if (available == nullptr) {
        return;
    }
    const bool newlyBurning = !available->active;
    available->enemyId = enemyId;
    available->sourceProjectileId = sourceProjectileId;
    available->remaining = fireDefinition_.burnDuration *
        statusDurationMultiplier_;
    available->tickRemaining = newlyBurning
        ? fireDefinition_.burnTickInterval
        : std::min(
              available->tickRemaining,
              fireDefinition_.burnTickInterval);
    available->active = true;
}

bool IceWandSystem::isBurning(EntityId enemyId) const {
    return std::ranges::any_of(
        burningEnemies_, [enemyId](const BurningEnemy& burning) {
            return burning.active && burning.enemyId == enemyId &&
                burning.remaining > 0.0;
        });
}

} // namespace ian
