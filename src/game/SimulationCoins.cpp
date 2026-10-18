#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"
#include "core/DeterministicRandom.hpp"

#include <algorithm>

namespace ian {
namespace {

constexpr double EnemyMedkitDropChance = 0.06;

double coinDropChance(EnemyType type, bool elite) {
    if (elite || type == EnemyType::Boss) {
        return 1.0;
    }
    switch (type) {
    case EnemyType::Basic:
    case EnemyType::Fast:
        return 0.30;
    case EnemyType::Heavy:
        return 0.70;
    case EnemyType::Ranged:
    case EnemyType::Bee:
    case EnemyType::Flying:
        return 0.45;
    case EnemyType::Sapper:
    case EnemyType::Splitter:
        return 0.55;
    case EnemyType::Splitling:
        return 0.15;
    case EnemyType::Boss:
        return 1.0;
    }
    return 0.30;
}

int coinDropAmount(EnemyType type) {
    switch (type) {
    case EnemyType::Basic:
    case EnemyType::Fast:
        return 1;
    case EnemyType::Heavy:
        return 3;
    case EnemyType::Ranged:
    case EnemyType::Sapper:
    case EnemyType::Bee:
    case EnemyType::Flying:
        return 2;
    case EnemyType::Splitter:
        return 2;
    case EnemyType::Splitling:
        return 1;
    case EnemyType::Boss:
        return 12;
    }
    return 1;
}

std::uint64_t coinSeed(EntityId id, std::uint64_t tick) {
    return (static_cast<std::uint64_t>(id.generation) << 32U) ^
           static_cast<std::uint64_t>(id.index) ^
           (tick * 0x9e3779b97f4a7c15ULL);
}

} // namespace

void Simulation::updateCoinPickups(double deltaSeconds) {
    const auto rolledCoinValue = [this](
        EntityId id, EnemyType type,
        bool elite) -> int {
        const std::uint64_t seed = coinSeed(id, tick_);
        constexpr std::uint64_t DropRollSalt =
            0xd1b54a32d192ed03ULL;
        if (unitRandom(seed ^ DropRollSalt) >=
            coinDropChance(type, elite)) {
            return 0;
        }
        const int baseCoins = coinDropAmount(type);
        return baseCoins +
            (elite ? std::max(2, baseCoins / 2) : 0);
    };
    const auto maybeDropMedkit = [this](
        EntityId id, Vec3 position, EnemyType type,
        bool elite) {
        const double chance = type == EnemyType::Boss
            ? 0.25
            : elite ? 0.14 : EnemyMedkitDropChance;
        const std::uint64_t seed = coinSeed(id, tick_) ^
            0xa0761d6478bd642fULL;
        if (unitRandom(seed) < chance) {
            coinPickups_.spawnHeart(position, seed, terrain_);
        }
    };
    int automaticallyCollectedCoins = 0;
    int automaticCollectionCount = 0;
    Vec3 automaticCollectionPosition = playerPosition_;
    // Explicit kill events preserve the death position even when the enemy
    // slot is reused by wave spawning later in the same simulation tick.
    for (const GameEvent& event : events_) {
        if (event.type != GameEventType::EnemyKilled ||
            !event.entityId) {
            continue;
        }
        if (!markEnemyRewarded(
                rewardedEnemyCoins_, *event.entityId)) {
            continue;
        }
        auto enemy = enemies_.enemies().end();
        if (!event.enemyType) {
            enemy = std::find_if(
                enemies_.enemies().begin(), enemies_.enemies().end(),
                [&event](const EnemyInstance& candidate) {
                    return candidate.id == *event.entityId;
                });
        }
        const EnemyType type = event.enemyType.value_or(
            enemy != enemies_.enemies().end()
                ? enemy->type
                : EnemyType::Basic);
        const bool elite = event.enemyType
            ? event.enemyEliteAffixes != 0U
            : enemy != enemies_.enemies().end() &&
                enemy->eliteAffixes != 0U;
        const int coinValue = rolledCoinValue(
            *event.entityId, type, elite);
        const bool killedByTurret =
            event.buildingType == BuildingType::Turret ||
            event.buildingType == BuildingType::GunTurret ||
            event.buildingType == BuildingType::Cannon ||
            event.buildingType == BuildingType::Catapult;
        if (coinValue > 0 && killedByTurret) {
            automaticallyCollectedCoins = saturatingAdd(
                automaticallyCollectedCoins, coinValue);
            ++automaticCollectionCount;
            automaticCollectionPosition = event.position;
        } else if (coinValue > 0) {
            coinPickups_.spawnValue(
                event.position, coinValue,
                coinSeed(*event.entityId, tick_), terrain_);
        }
        maybeDropMedkit(
            *event.entityId, event.position, type, elite);
    }

    // Fallback for deaths that do not emit a source-aware kill event.
    for (const EnemyInstance& enemy : enemies_.enemies()) {
        if (enemy.active || enemy.state != EnemyState::Dead) {
            continue;
        }
        if (!markEnemyRewarded(
                rewardedEnemyCoins_, enemy.id)) {
            continue;
        }
        const int coinValue = rolledCoinValue(
            enemy.id, enemy.type,
            enemy.eliteAffixes != 0U);
        if (coinValue > 0) {
            coinPickups_.spawnValue(
                enemy.position, coinValue,
                coinSeed(enemy.id, tick_), terrain_);
        }
        maybeDropMedkit(
            enemy.id, enemy.position, enemy.type,
            enemy.eliteAffixes != 0U);
    }

    if (automaticallyCollectedCoins > 0) {
        coins_ = saturatingAdd(coins_, automaticallyCollectedCoins);
        events_.push_back({
            .type = GameEventType::CoinCollected,
            .position = automaticCollectionPosition,
            .amount = automaticallyCollectedCoins,
            .intensity = static_cast<double>(automaticCollectionCount),
        });
    }

    const CoinCollection collected = coinPickups_.tick(
        deltaSeconds, playerPosition_, terrain_, collisionWorld_,
        playerPermanentMaxHealth() + playerTemporaryHealth_ -
            playerHealth_,
        std::min(
            20.0,
            CoinPickupSystem::AttractionRadius +
                6.0 * static_cast<double>(lootStacks_[
                    lootUpgradeIndex(LootUpgradeEffect::Magnet)])));
    if (collected.healing > 0.0) {
        playerHealth_ = std::min(
            playerPermanentMaxHealth() + playerTemporaryHealth_,
            playerHealth_ + collected.healing);
    }
    if (collected.value > 0) {
        coins_ = saturatingAdd(coins_, collected.value);
        events_.push_back({
            .type = GameEventType::CoinCollected,
            .position = collected.position,
            .amount = collected.value,
            .intensity = static_cast<double>(collected.count),
        });
    }
}

} // namespace ian
