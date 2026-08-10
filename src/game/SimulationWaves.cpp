#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

AttackDirection attackDirection(Vec3 anchor, GridPosition corePosition) {
    const double deltaX = anchor.x - static_cast<double>(corePosition.x);
    const double deltaZ = anchor.z - static_cast<double>(corePosition.z);
    if (std::abs(deltaX) > std::abs(deltaZ)) {
        return deltaX >= 0.0 ? AttackDirection::East
                             : AttackDirection::West;
    }
    return deltaZ >= 0.0 ? AttackDirection::South
                         : AttackDirection::North;
}

} // namespace

void Simulation::prepareWave(const WavePlan& plan, GridPosition corePosition,
                             std::size_t firstAnchorIndex) {
    waveSpawnQueue_.assign(plan.spawns.begin(), plan.spawns.end());
    waveSpawnGroupSize_ = plan.groupSize;
    waveSpawnInterval_ = plan.groupInterval;
    nextWaveSpawnIndex_ = 0;
    waveSpawnTimeRemaining_ = waveSpawnInterval_;
    upcomingAttackDirection_ =
        attackDirection(map_.enemySpawnAnchors[firstAnchorIndex], corePosition);
    currentWaveHasBoss_ = std::ranges::any_of(
        plan.spawns, [](const EnemySpawn& spawn) { return spawn.type == EnemyType::Boss; });
}

void Simulation::beginPreparedWave() {
    bestWave_ = std::max(bestWave_, wave_);
    const std::size_t firstGroupSize = std::min({
        static_cast<std::size_t>(waveSpawnGroupSize_),
        waveSpawnQueue_.size(), MaximumActiveEnemies});
    enemies_.spawnWave(
        std::span<const EnemySpawn>{waveSpawnQueue_.data(), firstGroupSize});
    nextWaveSpawnIndex_ = firstGroupSize;
}

void Simulation::tickWaveSpawning(double deltaSeconds) {
    if (nextWaveSpawnIndex_ >= waveSpawnQueue_.size()) {
        return;
    }
    waveSpawnTimeRemaining_ -= deltaSeconds;
    while (waveSpawnTimeRemaining_ <= 0.0 &&
           nextWaveSpawnIndex_ < waveSpawnQueue_.size()) {
        const std::size_t active = enemies_.activeCount();
        if (active >= MaximumActiveEnemies) {
            return;
        }
        const std::size_t remaining =
            waveSpawnQueue_.size() - nextWaveSpawnIndex_;
        const std::size_t groupSize = std::min({
            static_cast<std::size_t>(waveSpawnGroupSize_),
            remaining, MaximumActiveEnemies - active});
        enemies_.spawnGroup(std::span<const EnemySpawn>{
            waveSpawnQueue_.data() + nextWaveSpawnIndex_, groupSize});
        nextWaveSpawnIndex_ += groupSize;
        if (groupSize <
            std::min(static_cast<std::size_t>(waveSpawnGroupSize_),
                     remaining)) {
            waveSpawnTimeRemaining_ = 0.0;
            return;
        }
        waveSpawnTimeRemaining_ += waveSpawnInterval_;
    }
}

void Simulation::completeWave() {
    cannons_.clearProjectiles();
    bombs_.clearProjectiles();
    iceWand_.clearProjectiles();
    fireWand_.clearProjectiles();
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0;
    upcomingAttackDirection_.reset();
    const int nightlyChests = static_cast<int>(std::lround(
        skillTree_.effectValue("loot.nightly_chests")));
    if (nightlyChests > 0) {
        std::optional<Vec3> preferredCenter;
        double preferredRadius = 0.0;
        if (skillTree_.hasEffect("loot.safe_delivery")) {
            if (const auto core = buildings_.core()) {
                preferredCenter = buildingWorldPosition(*core);
                preferredRadius = 28.0;
            }
        }
        lootChests_.spawnAdditionalChests(
            nightlyChests, terrain_.seed(), map_.worldLimit,
            terrain_, resources_.nodes(), playerPosition_,
            preferredCenter, preferredRadius);
    }
    int additionalChests = 0;
    if (currentWaveHasBoss_) {
        const int mapStacks = lootStacks_[
            lootUpgradeIndex(LootUpgradeEffect::Map)];
        additionalChests = saturatingAdd(
            additionalChests, mapStacks);
    }
    if (additionalChests > 0) {
        lootChests_.spawnAdditionalChests(
            additionalChests, terrain_.seed(), map_.worldLimit,
            terrain_, resources_.nodes(), playerPosition_);
    }
    const double restoredHealthFraction = std::clamp(
        skillTree_.effectValue("wave.repair_fraction"), 0.0, 1.0);
    if (restoredHealthFraction > 0.0) {
        static_cast<void>(buildings_.restoreHealthFraction(
            restoredHealthFraction));
        static_cast<void>(foundations_.restoreHealthFraction(
            restoredHealthFraction));
    }
    events_.push_back({
        .type = GameEventType::WaveCompleted,
        .amount = wave_,
        .critical = currentWaveHasBoss_,
    });
    currentWaveHasBoss_ = false;
    const int reward = saturatingMultiplyNonNegative(
        economy_.waveRewardPerWave, wave_);
    addGold(reward);
    events_.push_back({
        .type = GameEventType::WaveRewardGranted,
        .amount = reward,
    });
    state_ = RunState::WaveComplete;
    phaseTimeRemaining_ = gameplay_.dawnSeconds;
    phaseDuration_ = phaseTimeRemaining_;
}

} // namespace ian
