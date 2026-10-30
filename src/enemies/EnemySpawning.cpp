#include "enemies/EnemyDifficulty.hpp"
#include "enemies/EnemySystem.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace ian {
namespace {

constexpr double Pi = 3.14159265358979323846;
constexpr double SplitWindupDuration = 0.38;

[[nodiscard]] double roundedEnemyHealth(double rawHealth) {
    const double safeHealth = std::max(rawHealth, 1.0);
    const double step = safeHealth < 20.0
        ? 1.0
        : safeHealth < 100.0 ? 5.0 : 10.0;
    return std::max(step, std::round(safeHealth / step) * step);
}

double hashUnit(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return static_cast<double>(value) /
           static_cast<double>(
               std::numeric_limits<std::uint32_t>::max());
}

} // namespace

void EnemySystem::appendEnemy(
    const EnemySpawn& spawn, bool allowActiveOverflow) {
    if (!allowActiveOverflow && activeCount() >= MaxActiveEnemies) {
        return;
    }
    const EnemyType type = spawn.type;
    Vec3 position = spawn.position;
    if (type == EnemyType::Boss) position.y = AlpakingHover + AlpakingBodyCenter;
    if (spawn.summonerId) position.y = 2.22;
    const EnemyDefinition stats =
        definitions_[static_cast<std::size_t>(type)];
    const bool elite = spawn.eliteAffixes != 0U;
    const double healthMultiplier =
        std::max(0.01, spawn.healthMultiplier) *
        (elite ? EliteHealthMultiplier : 1.0);
    const double damageMultiplier =
        std::max(0.01, spawn.damageMultiplier) *
        (elite ? EliteDamageMultiplier : 1.0);
    const double maximumHealth = roundedEnemyHealth(
        stats.health * healthMultiplier) * overtime_.health;
    const auto reusable = std::find_if(
        enemies_.begin(), enemies_.end(),
        [this](const EnemyInstance& enemy) {
            return !enemy.active &&
                std::ranges::none_of(
                    splitEventBuffer_,
                    [&enemy](const EnemySplitResult& split) {
                        return split.parentId == enemy.id;
                    }) &&
                std::ranges::none_of(
                    delayedSplitBuffer_,
                    [&enemy](const PendingSplit& split) {
                        return split.id == enemy.id;
                    }) &&
                std::ranges::none_of(
                    eliteDeathEventBuffer_,
                    [&enemy](const EliteEnemyEvent& event) {
                        return event.id == enemy.id;
                    });
        });
    if (reusable == enemies_.end() && enemies_.size() >= MaxEnemies) {
        return;
    }
    const EntityId id =
        reusable == enemies_.end()
            ? EntityId{nextIndex_++, 1}
            : EntityId{reusable->id.index, reusable->id.generation + 1};
    const double firstRandom =
        hashUnit(id.index * 0x9e3779b9U + id.generation);
    const double secondRandom =
        hashUnit(id.index * 0x85ebca6bU +
                 id.generation * 17U);
    const double thirdRandom =
        hashUnit(id.index * 0xc2b2ae35U +
                 id.generation * 31U);
    EnemyApproachRole approachRole = EnemyApproachRole::Direct;
    if (type == EnemyType::Basic) {
        const std::uint32_t roleBucket = id.index % 10U;
        if (roleBucket >= 6U && roleBucket < 8U) {
            approachRole = EnemyApproachRole::FlankLeft;
        } else if (roleBucket >= 8U) {
            approachRole = EnemyApproachRole::FlankRight;
        }
    }
    const double baseTurnRate =
        type == EnemyType::Boss
            ? 1.8
            : type == EnemyType::Heavy ? 2.35
                                       : 3.2;
    const EnemyInstance instance{
        .id = id,
        .type = type,
        .position = position,
        .health = maximumHealth,
        .maxHealth = maximumHealth,
        .speed = stats.speed * (spawn.summonerId ? 1.12 : 1.0) * overtime_.speed * waveSpeedMultiplier_,
        .damage = stats.damage * damageMultiplier * overtime_.damage,
        .attackCooldownRemaining = 0.0,
        .spawnAnimationRemaining =
            type == EnemyType::Splitling ? 0.72 : 0.0,
        .splitAnimationRemaining = 0.0,
        .ramWindup = stats.ramWindup,
        .ramDamageMultiplier = stats.ramDamageMultiplier,
        .ramCooldown = stats.ramCooldown,
        .ramWindupRemaining = 0.0,
        .ramCooldownRemaining = 0.0,
        .bossPhase = 1,
        .bossAbilityWindupRemaining = 0.0,
        .bossAbilityCooldownRemaining = 0.0,
        .bossWarCryNext = false,
        .slowRemaining = 0.0,
        .movementMultiplier = 1.0,
        .knockbackVelocity = spawn.initialKnockbackVelocity,
        .yaw = 0.0,
        .steeringTime = 0.0,
        .steeringPhase = firstRandom * 2.0 * Pi,
        .steeringFrequency = 0.7 + secondRandom * 0.65,
        .turnRate = baseTurnRate *
                    (0.88 + thirdRandom * 0.24),
        .locomotionRate = 0.94 + secondRandom * 0.12,
        .approachRole = approachRole,
        .state = EnemyState::Spawn,
        .target = std::nullopt,
        .active = true,
        .eliteAffixes = spawn.eliteAffixes,
        .summonerId = spawn.summonerId,
        .arrivalPortal = spawn.arrivalPortal,
        .portalArrivalRemaining = spawn.arrivalPortal ? EnemyPortalArrivalDuration : 0,
        .portalExitYaw = spawn.portalExitYaw,
    };
    if (reusable == enemies_.end()) {
        enemies_.push_back(instance);
    } else {
        *reusable = instance;
    }
    ++activeCount_;
    if (elite) {
        eliteSpawnEventBuffer_.push_back({
            .id = id,
            .position = position,
            .affixes = spawn.eliteAffixes,
        });
    }
}

void EnemySystem::scheduleSplit(
    EntityId parentId, Vec3 position,
    double healthMultiplier, double damageMultiplier) {
    if (std::ranges::find(
            delayedSplitBuffer_, parentId,
            &PendingSplit::id) != delayedSplitBuffer_.end()) {
        return;
    }
    delayedSplitBuffer_.push_back({
        .id = parentId,
        .position = position,
        .healthMultiplier = healthMultiplier,
        .damageMultiplier = damageMultiplier,
        .remaining = SplitWindupDuration,
    });
    if (EnemyInstance* parent = findEnemy(parentId)) {
        parent->splitAnimationRemaining = SplitWindupDuration;
    }
}

void EnemySystem::updatePendingSplits(double deltaSeconds) {
    for (PendingSplit& split : delayedSplitBuffer_) {
        split.remaining = std::max(
            0.0, split.remaining - deltaSeconds);
        if (EnemyInstance* parent = findEnemy(split.id)) {
            parent->splitAnimationRemaining = split.remaining;
        }
        if (split.remaining <= 0.0) {
            spawnSplitlings(
                split.id, split.position,
                split.healthMultiplier,
                split.damageMultiplier);
        }
    }
    std::erase_if(
        delayedSplitBuffer_,
        [](const PendingSplit& split) {
            return split.remaining <= 0.0;
        });
}

void EnemySystem::spawnSplitlings(
    EntityId parentId, Vec3 position,
    double healthMultiplier, double damageMultiplier) {
    constexpr int ChildCount = 3;
    constexpr double SpawnRadius = 0.42;
    constexpr double LaunchSpeed = 2.8;
    constexpr double TwoPi = 6.28318530717958647692;
    auto split = std::ranges::find(
        splitEventBuffer_, parentId,
        &EnemySplitResult::parentId);
    if (split == splitEventBuffer_.end()) {
        splitEventBuffer_.push_back({
            .parentId = parentId,
            .position = position,
            .childCount = 0,
        });
        split = std::prev(splitEventBuffer_.end());
    }
    const double phase = hashUnit(
        parentId.index * 0x9e3779b9U + parentId.generation) * TwoPi;
    for (int child = 0; child < ChildCount; ++child) {
        const double angle = phase + TwoPi *
            static_cast<double>(child) /
            static_cast<double>(ChildCount);
        const Vec3 direction{std::cos(angle), 0.0, std::sin(angle)};
        const std::size_t before = activeCount_;
        appendEnemy({
            .type = EnemyType::Splitling,
            .position = {
                position.x + direction.x * SpawnRadius,
                0.55,
                position.z + direction.z * SpawnRadius,
            },
            .healthMultiplier = healthMultiplier,
            .damageMultiplier = damageMultiplier,
            .initialKnockbackVelocity = {
                direction.x * LaunchSpeed,
                0.0,
                direction.z * LaunchSpeed,
            },
        }, true);
        if (activeCount_ > before) {
            ++split->childCount;
        }
    }
    spatialHashDirty_ = true;
}

} // namespace ian
