#include "game/Simulation.hpp"

#include "core/DeterministicRandom.hpp"
#include "core/SaturatingArithmetic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr double DebugSpawnMinimumRadius = 18.0;
constexpr double DebugSpawnMaximumRadius = 22.0;
constexpr double DebugSpawnCollisionRadius = 0.6;

double enemyHeight(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        return 0.675;
    case EnemyType::Heavy:
        return 1.0;
    case EnemyType::Boss:
        return 1.6;
    case EnemyType::Ranged:
        return 0.85;
    case EnemyType::Sapper:
        return 0.78;
    case EnemyType::Flying:
        return 2.4;
    case EnemyType::Splitter:
        return 1.05;
    case EnemyType::Splitling:
        return 0.55;
    case EnemyType::Basic:
        return 0.8;
    }
    return 0.8;
}

double horizontalDistanceSquared(Vec3 a, Vec3 b) {
    const double x = a.x - b.x;
    const double z = a.z - b.z;
    return x * x + z * z;
}

} // namespace

void Simulation::castChainLightning(
    const CastChainLightningCommand& command) {
    const int maximumTargets =
        std::clamp(command.maximumTargets, 1, 32);
    const double jumpRadius =
        std::clamp(command.jumpRadius, 0.25, 50.0);
    const double falloff =
        std::clamp(command.damageFalloff, 0.0, 1.0);
    double damage = std::max(0.0, command.damage);
    if (damage <= 0.0) {
        return;
    }

    std::optional<EntityId> current = command.firstTarget;
    if (!current || !enemies_.enemy(*current)) {
        current = aimedEnemy_;
    }
    if (!current || !enemies_.enemy(*current)) {
        current = enemies_.nearestEnemy(playerPosition_, 24.0);
    }
    if (!current) {
        return;
    }

    std::vector<EntityId> visited;
    visited.reserve(static_cast<std::size_t>(maximumTargets));
    if (command.excludedTarget) {
        visited.push_back(*command.excludedTarget);
    }
    Vec3 sourcePosition = command.sourcePosition.value_or(
        Vec3{playerPosition_.x, playerPosition_.y + 0.85,
             playerPosition_.z});
    std::optional<EntityId> sourceId = command.excludedTarget;

    for (int jump = 0; jump < maximumTargets && current;
         ++jump) {
        const auto target = enemies_.enemy(*current);
        if (!target || !target->active) {
            break;
        }
        Vec3 targetPosition = target->position;
        targetPosition.y += target->worldSurfaceHeight + 0.15;
        const auto result = enemies_.damage(*current, damage);
        if (!result) {
            break;
        }

        events_.push_back({
            .type = GameEventType::ChainLightningHit,
            .entityId = current,
            .sourceId = sourceId,
            .position = sourcePosition,
            .targetPosition = targetPosition,
            .amount = jump,
            .damage = result->damage,
            .intensity = damage,
        });
        if (result->killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = current,
                .enemyType = result->type,
                .enemyEliteAffixes = result->eliteAffixes,
                .position = result->position,
            });
            if (aimedEnemy_ == current) {
                aimedEnemy_.reset();
            }
        }

        visited.push_back(*current);
        sourceId = current;
        sourcePosition = targetPosition;
        damage *= falloff;

        std::optional<EntityId> next;
        double closestDistanceSquared = jumpRadius * jumpRadius;
        for (const EnemyInstance& candidate : enemies_.enemies()) {
            if (!candidate.active ||
                std::ranges::find(visited, candidate.id) !=
                    visited.end()) {
                continue;
            }
            const double distanceSquared =
                horizontalDistanceSquared(
                    target->position, candidate.position);
            if (distanceSquared < closestDistanceSquared ||
                (distanceSquared == closestDistanceSquared &&
                 (!next || candidate.id.index < next->index))) {
                next = candidate.id;
                closestDistanceSquared = distanceSquared;
            }
        }
        current = next;
    }
}

void Simulation::registerNailHit(
    EntityId primaryTarget, Vec3 impactPosition,
    double directHitDamage) {
    const int stacks = lootStacks_[
        lootUpgradeIndex(LootUpgradeEffect::Nail)];
    if (stacks <= 0 || directHitDamage <= 0.0) {
        return;
    }

    const int hitsPerTrigger = std::max(2, 6 - stacks);
    ++nailHitCounter_;
    if (nailHitCounter_ % static_cast<std::uint64_t>(hitsPerTrigger) != 0U) {
        return;
    }

    const double jumpRadius = std::min(
        9.0, 6.5 + static_cast<double>(stacks - 1) * 0.5);
    const double jumpRadiusSquared = jumpRadius * jumpRadius;
    std::optional<EntityId> firstTarget;
    double closestDistanceSquared = jumpRadiusSquared;
    for (const EnemyInstance& candidate : enemies_.enemies()) {
        if (!candidate.active || candidate.id == primaryTarget) {
            continue;
        }
        const double distanceSquared = horizontalDistanceSquared(
            impactPosition, candidate.position);
        if (distanceSquared < closestDistanceSquared ||
            (distanceSquared == closestDistanceSquared &&
             (!firstTarget || candidate.id.index < firstTarget->index))) {
            firstTarget = candidate.id;
            closestDistanceSquared = distanceSquared;
        }
    }
    if (!firstTarget) {
        return;
    }

    castChainLightning({
        .firstTarget = firstTarget,
        .excludedTarget = primaryTarget,
        .sourcePosition = impactPosition,
        .damage = directHitDamage * std::min(
            0.75, 0.45 + static_cast<double>(stacks - 1) * 0.08),
        .jumpRadius = jumpRadius,
        .damageFalloff = 0.78,
        .maximumTargets = std::min(6, stacks + 1),
    });
}

void Simulation::processDebugCommands(
    const PlayerCommand& command) {
    if (command.enableUnlimitedResources) {
        unlimitedResources_ = !unlimitedResources_;
        playerInvulnerable_ = unlimitedResources_;
        if (unlimitedResources_) {
            playerHealth_ = playerPermanentMaxHealth() +
                playerTemporaryHealth_;
        } else {
            clampResourcesToCapacity();
            const PlayerWeapon selected =
                playerWeapons_.selectedWeapon();
            const bool stillUnlocked =
                selected == PlayerWeapon::BareHands ||
                (selected == PlayerWeapon::Axe &&
                 skillTree_.hasEffect("unlock.axe")) ||
                (selected == PlayerWeapon::Pickaxe &&
                 skillTree_.hasEffect("unlock.pickaxe")) ||
                (selected == PlayerWeapon::Club &&
                 skillTree_.hasEffect("unlock.club")) ||
                (selected == PlayerWeapon::IceWand &&
                 skillTree_.hasEffect("unlock.ice_wand")) ||
                (selected == PlayerWeapon::FireWand &&
                 skillTree_.hasEffect("unlock.fire_wand")) ||
                (selected == PlayerWeapon::Hammer &&
                 skillTree_.hasEffect("unlock.hammer")) ||
                (selected == PlayerWeapon::Rifle &&
                 skillTree_.hasEffect("unlock.rifle"));
            if (!stillUnlocked) {
                playerWeapons_.selectWeapon(PlayerWeapon::BareHands);
            }
        }
    }
    if (command.toggleInvulnerability &&
        !unlimitedResources_) {
        playerInvulnerable_ = !playerInvulnerable_;
    }
    if (command.damageCore && !unlimitedResources_) {
        const auto core = buildings_.core();
        if (core) {
            const auto damage = buildings_.damage(
                core->id, command.damageCore->amount);
            if (damage) {
                events_.push_back({
                    .type = GameEventType::CoreDamaged,
                    .entityId = damage->id,
                    .buildingType = damage->type,
                    .position = buildingWorldPosition(*damage),
                    .amount = static_cast<int>(
                        command.damageCore->amount),
                });
                if (damage->destroyed) {
                    syncWorldStructures();
                    enemies_.clearProjectiles();
                    state_ = RunState::Defeat;
                    events_.push_back({
                        .type = GameEventType::BuildingDestroyed,
                        .entityId = damage->id,
                        .buildingType = damage->type,
                        .position = buildingWorldPosition(*damage),
                    });
                    events_.push_back({
                        .type = GameEventType::RunEnded,
                    });
                }
            }
        }
    }
    if (command.damagePlayer) {
        damagePlayer(
            command.damagePlayer->amount, std::nullopt,
            playerPosition_);
    }
    if (command.spawnEnemy) {
        const auto core = buildings_.core();
        if (core) {
            constexpr double TwoPi =
                6.28318530717958647692;
            const int requestedCount = std::clamp(
                command.spawnEnemy->count, 1, 1000);
            std::vector<EnemySpawn> spawns;
            spawns.reserve(
                static_cast<std::size_t>(requestedCount));
            for (int index = 0; index < requestedCount;
                 ++index) {
                const std::uint64_t sequence =
                    debugSpawnSequence_++;
                for (std::uint64_t attempt = 0;
                     attempt < 12; ++attempt) {
                    const std::uint64_t seed = mixBits64(
                        tick_ ^
                        (sequence *
                         0x9e3779b97f4a7c15ULL) ^
                        attempt);
                    const double angle =
                        unitRandom(seed) * TwoPi;
                    const double radius =
                        DebugSpawnMinimumRadius +
                        unitRandom(
                            seed ^
                            0xd1b54a32d192ed03ULL) *
                            (DebugSpawnMaximumRadius -
                             DebugSpawnMinimumRadius);
                    const Vec3 position{
                        static_cast<double>(
                            core->gridPosition.x) +
                            std::cos(angle) * radius,
                        enemyHeight(
                            command.spawnEnemy->type),
                        static_cast<double>(
                            core->gridPosition.z) +
                            std::sin(angle) * radius,
                    };
                    if (std::abs(position.x) >
                            map_.worldLimit -
                                DebugSpawnCollisionRadius ||
                        std::abs(position.z) >
                            map_.worldLimit -
                                DebugSpawnCollisionRadius) {
                        continue;
                    }
                    const bool blocked = std::any_of(
                        collisionWorld_.colliders().begin(),
                        collisionWorld_.colliders().end(),
                        [this, position](
                            const CollisionBox& collider) {
                            return collisionWorld_.overlapsCircle(
                                position,
                                DebugSpawnCollisionRadius,
                                collider);
                        });
                    if (blocked) {
                        continue;
                    }
                    spawns.push_back({
                        .type = command.spawnEnemy->type,
                        .position = position,
                        .eliteAffixes =
                            command.spawnEnemy->eliteAffixes,
                    });
                    break;
                }
            }
            if (!spawns.empty()) {
                enemies_.spawnGroup(spawns);
            }
        }
    }
    if (command.castChainLightning) {
        castChainLightning(*command.castChainLightning);
    }
    if (command.toggleWeapon) {
        cycleUnlockedTool();
        selectedBuilding_.reset();
        buildingPreview_.reset();
    }
    if (command.selectWeapon) {
        const PlayerWeapon weapon = command.selectWeapon->weapon;
        bool unlocked = weapon == PlayerWeapon::BareHands;
        switch (weapon) {
        case PlayerWeapon::BareHands: break;
        case PlayerWeapon::Axe:
            unlocked = unlimitedResources_ ||
                skillTree_.hasEffect("unlock.axe");
            break;
        case PlayerWeapon::Pickaxe:
            unlocked = unlimitedResources_ ||
                skillTree_.hasEffect("unlock.pickaxe");
            break;
        case PlayerWeapon::Club:
            unlocked = unlimitedResources_ ||
                skillTree_.hasEffect("unlock.club");
            break;
        case PlayerWeapon::IceWand:
            unlocked = unlimitedResources_ ||
                skillTree_.hasEffect("unlock.ice_wand");
            break;
        case PlayerWeapon::FireWand:
            unlocked = unlimitedResources_ ||
                skillTree_.hasEffect("unlock.fire_wand");
            break;
        case PlayerWeapon::Hammer:
            unlocked = unlimitedResources_ ||
                skillTree_.hasEffect("unlock.hammer");
            break;
        case PlayerWeapon::Rifle:
            unlocked = unlimitedResources_ ||
                skillTree_.hasEffect("unlock.rifle");
            break;
        case PlayerWeapon::Bomb:
            unlocked = unlimitedResources_ ||
                skillTree_.hasEffect("unlock.bombs");
            break;
        }
        if (unlocked) {
            playerWeapons_.selectWeapon(weapon);
            selectedBuilding_.reset();
            buildingPreview_.reset();
        }
    }
    if (command.upgradeWeapon) {
        if (playerWeapons_.selectedWeapon() == PlayerWeapon::Bomb) {
            const int bombCost = saturatingAdd(
                economy_.bombPurchaseCoinCost,
                saturatingMultiplyNonNegative(
                    economy_.bombPurchaseCoinCostPerWave, wave_));
            if (unlimitedResources_ ||
                coins_ >= bombCost) {
                if (!unlimitedResources_)
                    coins_ -= bombCost;
                bombs_.addBombs(economy_.bombPurchaseAmount);
                events_.push_back({
                    .type = GameEventType::BombPurchased,
                    .amount = bombCost,
                    .coinAmount = economy_.bombPurchaseAmount,
                });
            } else {
                events_.push_back({
                    .type = GameEventType::EconomyPurchaseRejected,
                    .amount = bombCost,
                });
            }
            return;
        }
        const auto core = buildings_.core();
        const int coreLevel =
            core ? static_cast<int>(core->level) : 0;
        const int availableCurrency = unlimitedResources_
            ? std::numeric_limits<int>::max()
            : crystals_;
        const WeaponUpgradeResult result =
            playerWeapons_.upgrade(coreLevel, availableCurrency);
        if (result.valid()) {
            if (!unlimitedResources_) {
                crystals_ -= result.crystalCost;
            }
            events_.push_back({
                .type = GameEventType::WeaponUpgraded,
                .amount = result.level,
            });
        } else {
            events_.push_back({
                .type = GameEventType::WeaponUpgradeRejected,
                .weaponUpgradeError = result.error,
            });
        }
    }
    if (command.defeatAllEnemies &&
        enemies_.activeCount() > 0) {
        if (state_ == RunState::Wave) {
            nextWaveSpawnIndex_ = waveSpawnQueue_.size();
        }
        enemies_.defeatAll();
        // The debug wave-clear command must not turn every volatile elite
        // into a delayed base-wiping explosion.
        static_cast<void>(enemies_.takeEliteDeathEvents());
        enemies_.clearProjectiles();
    }
}

} // namespace ian
