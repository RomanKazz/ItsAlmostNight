#include "game/Simulation.hpp"

#include "core/DeterministicRandom.hpp"

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
    case EnemyType::Basic:
        return 0.8;
    }
    return 0.8;
}

} // namespace

void Simulation::processDebugCommands(
    const PlayerCommand& command) {
    if (command.enableUnlimitedResources) {
        unlimitedResources_ = !unlimitedResources_;
        playerInvulnerable_ = unlimitedResources_;
        if (unlimitedResources_) {
            playerHealth_ =
                gameplay_.playerMaxHealth *
                    playerMaxHealthMultiplier_ +
                playerBonusMaxHealth_;
        } else {
            const PlayerWeapon selected =
                playerWeapons_.selectedWeapon();
            const bool stillUnlocked =
                selected == PlayerWeapon::BareHands ||
                (selected == PlayerWeapon::Axe &&
                 skillTree_.hasEffect(SkillEffect::UnlockAxe)) ||
                (selected == PlayerWeapon::Pickaxe &&
                 skillTree_.hasEffect(SkillEffect::UnlockPickaxe)) ||
                (selected == PlayerWeapon::Club &&
                 skillTree_.hasEffect(SkillEffect::UnlockClub)) ||
                (selected == PlayerWeapon::Hammer &&
                 skillTree_.hasEffect(SkillEffect::UnlockHammer)) ||
                (selected == PlayerWeapon::Rifle &&
                 skillTree_.hasEffect(SkillEffect::UnlockRifle));
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
                    });
                    break;
                }
            }
            if (!spawns.empty()) {
                enemies_.spawnGroup(spawns);
            }
        }
    }
    if (command.toggleWeapon) {
        cycleUnlockedTool();
        selectedBuilding_.reset();
        buildingPreview_.reset();
    }
    if (command.upgradeWeapon) {
        const auto core = buildings_.core();
        const int coreLevel =
            core ? static_cast<int>(core->level) : 0;
        const int availableGold = unlimitedResources_
            ? std::numeric_limits<int>::max()
            : gold_;
        const WeaponUpgradeResult result =
            playerWeapons_.upgrade(coreLevel, availableGold);
        if (result.valid()) {
            if (!unlimitedResources_) {
                gold_ -= result.goldCost;
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
    }
}

} // namespace ian
