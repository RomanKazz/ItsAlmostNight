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
    waveHealthScale_ = healthScale;
    waveDamageScale_ = damageScale;
    waveSpawnCycle_ = 0U;
    waveSpawnGroupsDue_ = 0U;
    forceWaveCompletion_ = false;
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
    waveSpawnGroupsDue_ = 0U;
}

void Simulation::refillWaveSpawnQueue() {
    const auto core = buildings_.core();
    if (!core || map_.enemySpawnAnchors.empty()) return;

    ++waveSpawnCycle_;
    const std::size_t anchor =
        (static_cast<std::size_t>(std::max(1, wave_)) +
         static_cast<std::size_t>(waveSpawnCycle_) * 3U) %
        map_.enemySpawnAnchors.size();
    const WavePlan plan = waveDirector_.buildWave(
        wave_, core->gridPosition, anchor);
    waveSpawnQueue_.clear();
    waveSpawnQueue_.reserve(plan.spawns.size());
    const double riskyHealth = 1.0 +
        0.25 * static_cast<double>(riskyInvestmentActive_);
    const double riskyDamage = 1.0 +
        0.15 * static_cast<double>(riskyInvestmentActive_);
    for (EnemySpawn spawn : plan.spawns) {
        // Bosses appear once per night. Later batches remain endless but use
        // regular and elite enemies only.
        if (spawn.type == EnemyType::Boss) continue;
        spawn.healthMultiplier *= waveHealthScale_ * riskyHealth;
        spawn.damageMultiplier *= waveDamageScale_ * riskyDamage;
        waveSpawnQueue_.push_back(spawn);
    }
    if (waveSpawnQueue_.size() > 1U) {
        const std::size_t offset =
            (static_cast<std::size_t>(waveSpawnCycle_) * 7U +
             static_cast<std::size_t>(std::max(1, wave_)) * 5U) %
            waveSpawnQueue_.size();
        std::rotate(
            waveSpawnQueue_.begin(),
            waveSpawnQueue_.begin() +
                static_cast<std::ptrdiff_t>(offset),
            waveSpawnQueue_.end());
    }
    nextWaveSpawnIndex_ = 0U;
}

void Simulation::tickWaveSpawning(double deltaSeconds) {
    constexpr std::size_t TechnicalActiveEnemyLimit = 240U;
    constexpr std::size_t MaximumSpawnBacklog = 4096U;
    waveSpawnTimeRemaining_ -= deltaSeconds;
    // The timetable advances regardless of how quickly the base kills. A
    // strong defense therefore cannot trigger extra groups, and a weak one
    // cannot slow the director down by leaving enemies alive.
    while (waveSpawnTimeRemaining_ <= 0.0) {
        waveSpawnGroupsDue_ = std::min(
            MaximumSpawnBacklog, waveSpawnGroupsDue_ + 1U);
        waveSpawnTimeRemaining_ += waveSpawnInterval_;
    }

    while (waveSpawnGroupsDue_ > 0U) {
        if (nextWaveSpawnIndex_ >= waveSpawnQueue_.size()) {
            refillWaveSpawnQueue();
            if (waveSpawnQueue_.empty()) return;
        }
        const std::size_t active = enemies_.activeCount();
        const std::size_t remaining =
            waveSpawnQueue_.size() - nextWaveSpawnIndex_;
        const std::size_t groupSize = std::min(
            static_cast<std::size_t>(waveSpawnGroupSize_),
            remaining);
        const std::size_t technicalLimit = std::min(
            MaximumActiveEnemies, TechnicalActiveEnemyLimit);
        if (groupSize == 0U || active + groupSize > technicalLimit) {
            // Keep the scheduled group in the backlog. This guard exists only
            // to protect frame time during pathological accumulation.
            return;
        }
        enemies_.spawnGroup(std::span<const EnemySpawn>{
            waveSpawnQueue_.data() + nextWaveSpawnIndex_, groupSize});
        nextWaveSpawnIndex_ += groupSize;
        --waveSpawnGroupsDue_;
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
    waveSpawnGroupsDue_ = 0U;
    waveSpawnCycle_ = 0U;
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
    if (!sandboxMode_ && !stageCleared_ && completedBossWave &&
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
