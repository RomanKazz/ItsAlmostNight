#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"

#include <algorithm>

namespace ian {
namespace {

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
    // Explicit kill events preserve the death position even when the enemy
    // slot is reused by wave spawning later in the same simulation tick.
    for (const GameEvent& event : events_) {
        if (event.type != GameEventType::EnemyKilled ||
            !event.entityId) {
            continue;
        }
        const std::uint64_t key = coinSeed(*event.entityId, 0U);
        if (!rewardedEnemyCoins_.insert(key).second) {
            continue;
        }
        const auto enemy = std::find_if(
            enemies_.enemies().begin(), enemies_.enemies().end(),
            [&event](const EnemyInstance& candidate) {
                return candidate.id == *event.entityId;
            });
        const EnemyType type = enemy != enemies_.enemies().end()
            ? enemy->type
            : EnemyType::Basic;
        coinPickups_.spawn(
            event.position, coinDropAmount(type),
            coinSeed(*event.entityId, tick_), terrain_);
    }

    // Area systems such as cannons only report an aggregate kill count.
    // Dead-state scanning covers those deaths without relying on event shape.
    for (const EnemyInstance& enemy : enemies_.enemies()) {
        if (enemy.active || enemy.state != EnemyState::Dead) {
            continue;
        }
        const std::uint64_t key = coinSeed(enemy.id, 0U);
        if (!rewardedEnemyCoins_.insert(key).second) {
            continue;
        }
        coinPickups_.spawn(
            enemy.position, coinDropAmount(enemy.type),
            coinSeed(enemy.id, tick_), terrain_);
    }

    const CoinCollection collected = coinPickups_.tick(
        deltaSeconds, playerPosition_, terrain_);
    if (collected.value <= 0) {
        return;
    }
    coins_ = saturatingAdd(coins_, collected.value);
    events_.push_back({
        .type = GameEventType::CoinCollected,
        .position = collected.position,
        .amount = collected.value,
        .intensity = static_cast<double>(collected.count),
    });
}

} // namespace ian
