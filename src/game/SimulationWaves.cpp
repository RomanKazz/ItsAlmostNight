#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

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

std::size_t attackDirectionIndex(AttackDirection direction) {
    return static_cast<std::size_t>(direction);
}

} // namespace

void Simulation::prepareWave(const WavePlan& plan, GridPosition corePosition,
                             std::size_t firstAnchorIndex,
                             double healthScale,
                             double damageScale) {
    waveSpawnQueue_.assign(plan.spawns.begin(), plan.spawns.end());
    healthScale = std::max(0.05, healthScale);
    damageScale = std::max(0.05, damageScale);
    for (EnemySpawn& spawn : waveSpawnQueue_) {
        spawn.healthMultiplier *= healthScale;
        spawn.damageMultiplier *= damageScale;
    }
    riskyInvestmentActive_ = riskyInvestmentPending_;
    riskyInvestmentPending_ = 0;
    if (riskyInvestmentActive_ > 0) {
        const double healthMultiplier =
            1.0 + 0.25 * static_cast<double>(riskyInvestmentActive_);
        const double damageMultiplier =
            1.0 + 0.15 * static_cast<double>(riskyInvestmentActive_);
        for (EnemySpawn& spawn : waveSpawnQueue_) {
            spawn.healthMultiplier *= healthMultiplier;
            spawn.damageMultiplier *= damageMultiplier;
        }
    }
    waveSpawnGroupSize_ = plan.groupSize;
    waveSpawnInterval_ = plan.groupInterval;
    nextWaveSpawnIndex_ = 0;
    waveSpawnTimeRemaining_ = waveSpawnInterval_;
    upcomingAttackDirection_ =
        attackDirection(map_.enemySpawnAnchors[firstAnchorIndex], corePosition);
    upcomingAttackDirections_.fill(false);
    for (const EnemySpawn& spawn : plan.spawns) {
        const AttackDirection direction =
            attackDirection(spawn.position, corePosition);
        upcomingAttackDirections_[attackDirectionIndex(direction)] = true;
    }
    currentWaveHasBoss_ = std::ranges::any_of(
        plan.spawns, [](const EnemySpawn& spawn) { return spawn.type == EnemyType::Boss; });
}

void Simulation::beginPreparedWave() {
    bestWave_ = std::max(bestWave_, wave_);
    if (unlimitedResources_ || skillTree_.hasEffect("unlock.bombs")) {
        bombs_.addBombs(2 + runNightlyBombBonus_ + std::max(
            0, static_cast<int>(std::lround(
                skillTree_.effectValue("bomb.nightly_bonus")))));
    }
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
    const bool completedBossWave = currentWaveHasBoss_;
    waveStartCheckpoint_.reset();
    enemies_.clearProjectiles();
    cannons_.clearProjectiles();
    bombs_.clearProjectiles();
    iceWand_.clearProjectiles();
    fireWand_.clearProjectiles();
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0;
    upcomingAttackDirection_.reset();
    upcomingAttackDirections_.fill(false);
    int nightlyChests = static_cast<int>(std::lround(
        skillTree_.effectValue("loot.nightly_chests")));
    if (nightlyChests <= 0 &&
        skillTree_.hasEffect("loot.chest_every_two_nights") &&
        wave_ % 2 == 0) {
        nightlyChests = 1;
    }
    if (nightlyChests > 0) {
        std::optional<Vec3> preferredCenter;
        if (const auto core = buildings_.core()) {
            preferredCenter = buildingWorldPosition(*core);
        }
        const double preferredRadius =
            skillTree_.hasEffect("loot.safe_delivery") ? 18.0 : 28.0;
        lootChests_.spawnAdditionalChests(
            nightlyChests, terrain_.seed(), map_.worldLimit,
            terrain_, resources_.nodes(), playerPosition_,
            preferredCenter, preferredRadius, 8.0,
            LootChestPurpose::Reward);
    }
    int additionalChests = 0;
    if (currentWaveHasBoss_) {
        const int mapStacks = lootStacks_[
            lootUpgradeIndex(LootUpgradeEffect::Map)];
        additionalChests = saturatingAdd(
            additionalChests, mapStacks);
    }
    if (additionalChests > 0) {
        std::optional<Vec3> preferredCenter;
        if (const auto core = buildings_.core()) {
            preferredCenter = buildingWorldPosition(*core);
        }
        const double preferredRadius =
            skillTree_.hasEffect("loot.safe_delivery") ? 18.0 : 28.0;
        lootChests_.spawnAdditionalChests(
            additionalChests, terrain_.seed(), map_.worldLimit,
            terrain_, resources_.nodes(), playerPosition_,
            preferredCenter, preferredRadius, 8.0,
            LootChestPurpose::Reward);
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
        .critical = completedBossWave,
    });
    currentWaveHasBoss_ = false;
    const int reward = saturatingAdd(
        economy_.waveRewardBase,
        saturatingMultiplyNonNegative(
            economy_.waveRewardPerWave, wave_));
    addCrystals(reward);
    events_.push_back({
        .type = GameEventType::WaveRewardGranted,
        .amount = reward,
    });
    if (!stageCleared_ && completedBossWave &&
        wave_ == StageClearWave) {
        stageCleared_ = true;
        state_ = RunState::StageClear;
        phaseTimeRemaining_ = 0.0;
        phaseDuration_ = 0.0;
        events_.push_back({
            .type = GameEventType::StageCleared,
            .amount = wave_,
            .critical = true,
        });
        return;
    }
    if (finalNight_) {
        // Endless-night waves have no dawn pause. Keep a fresh checkpoint
        // after the completed wave, before the next wave grants consumables
        // or spawns actors.
        SuspendedRunState checkpoint = saveSuspendedRunState();
        checkpoint.resumeState = RunState::StageClear;
        checkpoint.wave = wave_;
        checkpoint.bestWave = std::max(bestWave_, wave_);
        checkpoint.runStatistics.wavesSurvived = std::max(
            checkpoint.runStatistics.wavesSurvived, wave_);
        checkpoint.stageCleared = true;
        checkpoint.finalNight = false;
        checkpoint.phaseTimeRemaining = 0.0;
        checkpoint.phaseDuration = 0.0;
        checkpoint.riskyInvestmentPending = riskyInvestmentPending_;
        waveStartCheckpoint_ =
            std::make_unique<SuspendedRunState>(
                std::move(checkpoint));
        static_cast<void>(beginNextFinalNightWave());
        return;
    }
    state_ = RunState::WaveComplete;
    phaseTimeRemaining_ = gameplay_.dawnSeconds;
    phaseDuration_ = phaseTimeRemaining_;
    prepareRunUpgradeChoices();
}

bool Simulation::beginNextFinalNightWave() {
    const auto core = buildings_.core();
    if (!core) return false;
    const int nextWave = saturatingAdd(wave_, 1);
    const Vec3 horizontalView = lookDirection(playerYaw_, 0.0);
    const std::size_t firstAnchor = leastVisibleSpawnAnchor(
        map_.enemySpawnAnchors, playerPosition_, horizontalView);
    const WavePlan plan = waveDirector_.buildWave(
        nextWave, core->gridPosition, firstAnchor);
    const int finalNightDepth = std::max(
        1, nextWave - StageClearWave);
    const double healthScale =
        1.0 + 0.20 * static_cast<double>(finalNightDepth);
    const double damageScale =
        1.0 + 0.13 * static_cast<double>(finalNightDepth);
    prepareWave(
        plan, core->gridPosition, firstAnchor,
        healthScale, damageScale);
    wave_ = nextWave;
    applyPotionWaveStart();
    beginPreparedWave();
    state_ = RunState::Wave;
    phaseTimeRemaining_ = 0.0;
    phaseDuration_ = 0.0;
    events_.push_back({
        .type = GameEventType::WaveStarted,
        .amount = wave_,
        .critical = true,
    });
    return true;
}

bool Simulation::enterFinalNight() {
    if (state_ != RunState::StageClear || !stageCleared_) {
        return false;
    }
    invalidateSnapshotCache();
    waveStartCheckpoint_ = std::make_unique<SuspendedRunState>(
        saveSuspendedRunState());
    finalNight_ = true;
    events_.push_back({
        .type = GameEventType::FinalNightStarted,
        .amount = wave_,
        .critical = true,
    });
    if (beginNextFinalNightWave()) {
        return true;
    }
    finalNight_ = false;
    waveStartCheckpoint_.reset();
    return false;
}

bool Simulation::bankStageClear() {
    if (state_ != RunState::StageClear || !stageCleared_) {
        return false;
    }
    invalidateSnapshotCache();
    state_ = RunState::Victory;
    events_.push_back({
        .type = GameEventType::RunEnded,
        .amount = wave_,
        .critical = true,
    });
    return true;
}

} // namespace ian
