#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"
#include "core/DeterministicRandom.hpp"

#include <algorithm>
#include <cmath>

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
    case EnemyType::AirTank:
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
    case EnemyType::AirTank:
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

// Rewards arrive as visible orbs; their total value is independent of count.
int enemyExperience(
    const InsightConfig& config, EnemyType type, bool elite) {
    const int amount = static_cast<int>(std::lround(
        config.enemy[static_cast<std::size_t>(type)]));
    return elite ? amount * 2 : amount;
}

int resourceExperience(const InsightConfig& config, ResourceType type) {
    const std::size_t index = static_cast<std::size_t>(type);
    return static_cast<int>(std::lround(
        isHarvestableResource(type)
            ? config.resourceDepleted[index]
            : config.prop[index - 3U]));
}

int experienceOrbCount(int amount) {
    // Merge pairs from the previous 2–8 orb burst. Integer value distribution
    // in spawnExperience preserves the entire reward, including odd remainders.
    const int previousCount = std::clamp(amount / 3 + (amount % 3 != 0), 2, 8);
    return (previousCount + 1) / 2;
}

std::uint64_t coinSeed(EntityId id, std::uint64_t tick) {
    return (static_cast<std::uint64_t>(id.generation) << 32U) ^
           static_cast<std::uint64_t>(id.index) ^
           (tick * 0x9e3779b97f4a7c15ULL);
}

} // namespace

void Simulation::updateCoinPickups(
    double deltaSeconds, bool suppressEnemyRewards) {
    int overflowExperience = 0;
    const InsightConfig& experienceConfig = insight_.config();
    const auto dropExperience = [this, &overflowExperience](
        Vec3 position, int amount, std::uint64_t seed,
        double spread = 1.0, bool valuableProp = false) {
        if (coinPickups_.spawnExperience(
                position, amount, valuableProp ? std::max(1,amount/8) : experienceOrbCount(amount),
                seed ^ 0x8ebc6af09c88c6e3ULL, terrain_, spread) == 0) {
            overflowExperience = saturatingAdd(overflowExperience, amount);
        }
    };
    for (const GameEvent& event : events_) {
        if (unlimitedResources_ || !event.entityId) continue;
        if (event.type == GameEventType::ResourceCollected &&
            event.resourceType) {
            const int amount = resourceExperience(
                experienceConfig, *event.resourceType);
            dropExperience(event.position, amount,
                           coinSeed(*event.entityId, tick_),
                           isDestructibleProp(*event.resourceType) ? 0.5 : 1.0,
                           isDestructibleProp(*event.resourceType));
        } else if (event.type == GameEventType::ChestOpened) {
            const auto chest = std::find_if(
                lootChests_.chests().begin(), lootChests_.chests().end(),
                [&event](const LootChestInstance& value) {
                    return value.id == *event.entityId;
                });
            if (chest != lootChests_.chests().end()) {
                dropExperience(chest->position,
                    static_cast<int>(std::lround(experienceConfig.chest[
                        static_cast<std::size_t>(chest->type)])),
                    coinSeed(*event.entityId, tick_));
            }
        }
    }
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
        return (baseCoins +
            (elite ? std::max(2, baseCoins / 2) : 0)) * powerups_.enemyCoinMultiplier();
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
    const auto maybeDropPowerup = [this, suppressEnemyRewards](EntityId id, Vec3 position, EnemyType type, bool elite) {
        if (suppressEnemyRewards) return;
        const auto drop = PowerupSystem::rollDrop(coinSeed(id, tick_), elite, type == EnemyType::Boss);
        if (!drop) return;
        const double ground = terrain_.getHeight(position.x, position.z);
        const auto support = collisionWorld_.playerSupportHeight(
            position.x, position.z, 0.2, std::max(position.y, ground) + 0.2);
        position.y = std::max(ground, support.value_or(ground));
        static_cast<void>(powerups_.spawn(*drop, position));
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
        if (!suppressEnemyRewards) {
            dropExperience(event.position,
                           enemyExperience(experienceConfig, type, elite),
                           coinSeed(*event.entityId, tick_));
        }
        const int coinValue = rolledCoinValue(
            *event.entityId, type, elite);
        if (coinValue > 0) {
            coinPickups_.spawnValue(
                event.position, coinValue,
                coinSeed(*event.entityId, tick_), terrain_);
        }
        maybeDropMedkit(
            *event.entityId, event.position, type, elite);
        maybeDropPowerup(*event.entityId, event.position, type, elite);
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
        if (!suppressEnemyRewards) {
            dropExperience(enemy.position,
                enemyExperience(
                    experienceConfig, enemy.type,
                    enemy.eliteAffixes != 0U),
                coinSeed(enemy.id, tick_));
        }
        if (coinValue > 0) {
            coinPickups_.spawnValue(
                enemy.position, coinValue,
                coinSeed(enemy.id, tick_), terrain_);
        }
        maybeDropMedkit(
            enemy.id, enemy.position, enemy.type,
            enemy.eliteAffixes != 0U);
        maybeDropPowerup(enemy.id, enemy.position, enemy.type, enemy.eliteAffixes != 0U);
    }

    if (powerups_.active(PowerupType::Magnet)) coinPickups_.magnetizeExperienceAndCoins();
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
    if (collected.silver > 0) {
        events_.push_back({.type=GameEventType::MetaSilverCollected,
            .position=collected.position,.amount=collected.silver,
            .intensity=static_cast<double>(collected.silverCount)});
    }
    if (collected.experience > 0) {
        events_.push_back({
            .type = GameEventType::ExperienceCollected,
            .position = collected.position,
            .amount = collected.experience,
            .intensity = static_cast<double>(collected.experienceCount),
        });
    }
    const int earnedExperience = saturatingAdd(
        collected.experience, overflowExperience);
    if (earnedExperience > 0) {
        grantConfiguredInsight(
            earnedExperience, InsightSource::Other,
            InsightCategory::Exploration, {.bypassDiminishing = true});
    }
}

} // namespace ian
