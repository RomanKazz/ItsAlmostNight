#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"
#include "core/DeterministicRandom.hpp"

#include <algorithm>

namespace ian {
namespace {

constexpr double EnemyMedkitDropChance = 0.06;

int coinDropAmount(EnemyType type) {
    switch (type) {
    case EnemyType::Basic:
    case EnemyType::Fast:
        return 1;
    case EnemyType::Heavy:
        return 3;
    case EnemyType::Ranged:
    case EnemyType::Sapper:
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
        const int baseCoins = coinDropAmount(type);
        coinPickups_.spawnValue(
            event.position,
            baseCoins + (elite ? std::max(2, baseCoins / 2) : 0),
            coinSeed(*event.entityId, tick_), terrain_);
        maybeDropMedkit(
            *event.entityId, event.position, type, elite);
    }

    // Area systems such as cannons only report an aggregate kill count.
    // Dead-state scanning covers those deaths without relying on event shape.
    for (const EnemyInstance& enemy : enemies_.enemies()) {
        if (enemy.active || enemy.state != EnemyState::Dead) {
            continue;
        }
        if (!markEnemyRewarded(
                rewardedEnemyCoins_, enemy.id)) {
            continue;
        }
        const int baseCoins = coinDropAmount(enemy.type);
        coinPickups_.spawnValue(
            enemy.position,
            baseCoins + (enemy.eliteAffixes != 0U
                ? std::max(2, baseCoins / 2) : 0),
            coinSeed(enemy.id, tick_), terrain_);
        maybeDropMedkit(
            enemy.id, enemy.position, enemy.type,
            enemy.eliteAffixes != 0U);
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
