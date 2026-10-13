#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"
#include "enemies/EnemyCollision.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
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

std::array<std::optional<Vec3>, 4> portalPositions(
    std::span<const EnemySpawn> spawns, GridPosition corePosition) {
    constexpr std::size_t DirectionCount = 4U;
    std::array<double, DirectionCount> nearestRadius{};
    nearestRadius.fill(std::numeric_limits<double>::infinity());
    for (const EnemySpawn& spawn : spawns) {
        const std::size_t index = attackDirectionIndex(
            attackDirection(spawn.position, corePosition));
        nearestRadius[index] = std::min(
            nearestRadius[index],
            std::hypot(
                spawn.position.x - static_cast<double>(corePosition.x),
                spawn.position.z - static_cast<double>(corePosition.z)));
    }

    std::array<Vec3, DirectionCount> sums{};
    std::array<std::size_t, DirectionCount> counts{};
    for (const EnemySpawn& spawn : spawns) {
        const std::size_t index = attackDirectionIndex(
            attackDirection(spawn.position, corePosition));
        const double radius = std::hypot(
            spawn.position.x - static_cast<double>(corePosition.x),
            spawn.position.z - static_cast<double>(corePosition.z));
        if (radius > nearestRadius[index] + 0.22) continue;
        sums[index].x += spawn.position.x;
        sums[index].z += spawn.position.z;
        ++counts[index];
    }

    std::array<std::optional<Vec3>, DirectionCount> result{};
    for (std::size_t index = 0; index < DirectionCount; ++index) {
        if (counts[index] == 0U) continue;
        const double inverseCount =
            1.0 / static_cast<double>(counts[index]);
        result[index] = Vec3{
            sums[index].x * inverseCount,
            0.0,
            sums[index].z * inverseCount,
        };
    }

    constexpr double MinimumPortalSeparation = 5.2;
    constexpr std::array<Vec3, DirectionCount> DirectionVectors{{
        {0.0, 0.0, -1.0},
        {1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0},
        {-1.0, 0.0, 0.0},
    }};
    // Adjacent attack fronts can select spawn lanes near the same map
    // corner. Relax the four portal centers before routing enemies so their
    // meshes and spawn apertures never overlap.
    for (int iteration = 0; iteration < 8; ++iteration) {
        bool moved = false;
        for (std::size_t left = 0; left < DirectionCount; ++left) {
            if (!result[left]) continue;
            for (std::size_t right = left + 1U;
                 right < DirectionCount; ++right) {
                if (!result[right]) continue;
                double deltaX =
                    result[right]->x - result[left]->x;
                double deltaZ =
                    result[right]->z - result[left]->z;
                double distance = std::hypot(deltaX, deltaZ);
                if (distance >= MinimumPortalSeparation) continue;
                if (distance < 1e-6) {
                    deltaX = DirectionVectors[right].x -
                        DirectionVectors[left].x;
                    deltaZ = DirectionVectors[right].z -
                        DirectionVectors[left].z;
                    distance = std::max(
                        1e-6, std::hypot(deltaX, deltaZ));
                }
                const double correction =
                    (MinimumPortalSeparation - distance) * 0.5 +
                    0.001;
                const double normalX = deltaX / distance;
                const double normalZ = deltaZ / distance;
                result[left]->x -= normalX * correction;
                result[left]->z -= normalZ * correction;
                result[right]->x += normalX * correction;
                result[right]->z += normalZ * correction;
                moved = true;
            }
        }
        if (!moved) break;
    }
    return result;
}

std::pair<double, double> portalSpawnOffset(std::span<const EnemySpawn> group, std::size_t index) {
    if (group.size() < 2U) return {0, -.15};
    double maximumRadius = 0;
    for (const auto& spawn : group) {
        EnemyInstance body{};
        body.type = spawn.type;
        body.eliteAffixes = spawn.eliteAffixes;
        maximumRadius = std::max(maximumRadius, enemyCollisionCylinder(body).radius);
    }
    const double spacing = maximumRadius * 2 + 0.06;
    if (group.size() == 2U) return {(static_cast<double>(index) - 0.5) * spacing, -.15};
    // Three ordinary bodies fit a triangle inside the aperture; a straight
    // line cannot fit without overlap. Larger groups get a wider exit ring.
    constexpr double Pi = 3.14159265358979323846;
    const double radius = spacing / (2 * std::sin(Pi / static_cast<double>(group.size())));
    const double angle = static_cast<double>(index) * 2 * Pi / static_cast<double>(group.size()) - Pi * 0.5;
    double frontShift=0;
    for(std::size_t i=0;i<group.size();++i) frontShift=std::max(frontShift,std::sin(
        static_cast<double>(i)*2*Pi/static_cast<double>(group.size())-Pi*.5)*radius);
    // Translate the intact formation onto the forward side. Do not mirror
    // individual offsets: that collapses pair spacing and overlaps bodies.
    return {std::cos(angle) * radius, std::sin(angle) * radius-frontShift-.15};
}

void routeSpawnsThroughPortals(
    std::vector<EnemySpawn>& spawns,
    const std::array<std::optional<Vec3>, 4>& portals,
    GridPosition corePosition, int groupSize) {
    const std::size_t normalizedGroupSize =
        static_cast<std::size_t>(std::max(1, groupSize));
    for (std::size_t index = 0; index < spawns.size(); ++index) {
        EnemySpawn& spawn = spawns[index];
        const std::size_t directionIndex = attackDirectionIndex(
            attackDirection(spawn.position, corePosition));
        if (!portals[directionIndex]) continue;

        const Vec3 portal = *portals[directionIndex];
        const double outwardX =
            portal.x - static_cast<double>(corePosition.x);
        const double outwardZ =
            portal.z - static_cast<double>(corePosition.z);
        const double length = std::max(
            0.001, std::hypot(outwardX, outwardZ));
        const double tangentX = -outwardZ / length;
        const double tangentZ = outwardX / length;
        const std::size_t indexInGroup = index % normalizedGroupSize;
        const std::size_t first = index - indexInGroup;
        const auto [lane, depth] = portalSpawnOffset(
            std::span<const EnemySpawn>{spawns}.subspan(first,
                std::min(normalizedGroupSize, spawns.size() - first)), indexInGroup);
        spawn.position.x = portal.x + tangentX * lane + outwardX / length * depth;
        spawn.position.z = portal.z + tangentZ * lane + outwardZ / length * depth;
        spawn.arrivalPortal=portal;
        spawn.portalExitYaw=std::atan2(-outwardX,-outwardZ);
    }
}

std::size_t surgePortalIndex(
    const std::array<std::optional<Vec3>, 4>& portals,
    std::optional<AttackDirection> primary, int surge) {
    const std::size_t start = primary
        ? attackDirectionIndex(*primary) : 0U;
    for (std::size_t step = 0; step < portals.size(); ++step) {
        const std::size_t index =
            (start + step + (surge == 1 ? 1U : 0U)) % portals.size();
        if (portals[index]) return index;
    }
    return start;
}

void moveGroupToPortal(
    std::span<EnemySpawn> spawns, Vec3 portal,
    GridPosition corePosition) {
    const double outwardX =
        portal.x - static_cast<double>(corePosition.x);
    const double outwardZ =
        portal.z - static_cast<double>(corePosition.z);
    const double length = std::max(
        0.001, std::hypot(outwardX, outwardZ));
    const double tangentX = -outwardZ / length;
    const double tangentZ = outwardX / length;
    for (std::size_t index = 0; index < spawns.size(); ++index) {
        const auto [lane, depth] = portalSpawnOffset(spawns, index);
        spawns[index].position.x = portal.x + tangentX * lane + outwardX / length * depth;
        spawns[index].position.z = portal.z + tangentZ * lane + outwardZ / length * depth;
        spawns[index].arrivalPortal=portal;
        spawns[index].portalExitYaw=std::atan2(-outwardX,-outwardZ);
    }
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
    waveBossSpawn_.reset();
    waveSurgeWarningMask_ = 0U;
    forceWaveCompletion_ = false;
    wavePortalsActive_ = false;
    waveFirstAnchorIndex_ = firstAnchorIndex %
        map_.enemySpawnAnchors.size();
    wavePortalPositions_ = portalPositions(plan.spawns, corePosition);
    routeSpawnsThroughPortals(
        waveSpawnQueue_, wavePortalPositions_, corePosition,
        plan.groupSize);
    for (EnemySpawn& spawn : waveSpawnQueue_) {
        spawn.healthMultiplier *= healthScale;
        spawn.damageMultiplier *= damageScale;
    }
    if (!finalNight_) {
        const auto boss = std::ranges::find_if(
            waveSpawnQueue_, [](const EnemySpawn& spawn) {
                return spawn.type == EnemyType::Boss;
            });
        if (boss != waveSpawnQueue_.end()) {
            waveBossSpawn_ = *boss;
            waveSpawnQueue_.erase(boss);
        }
    }
    waveSpawnGroupSize_ = plan.groupSize;
    waveSpawnInterval_ = plan.groupInterval;
    nextWaveSpawnIndex_ = 0;
    waveSpawnTimeRemaining_ = std::clamp(
        waveSpawnInterval_, 0.35, 2.0);
    upcomingAttackDirection_ =
        attackDirection(
            map_.enemySpawnAnchors[waveFirstAnchorIndex_], corePosition);
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
    // Safety net for direct/debug wave starts and restored runs that bypass
    // the regular BuildPhase -> Sunset transition.
    rebuildFlowFieldIfDirty();
    bestWave_ = std::max(bestWave_, wave_);
    if (playerWeaponUnlocked(PlayerWeapon::Bomb)) {
        bombs_.addBombs(2 + runNightlyBombBonus_);
    }
    const std::size_t firstGroupSize = std::min({
        static_cast<std::size_t>(waveSpawnGroupSize_),
        waveSpawnQueue_.size(), MaximumActiveEnemies});
    enemies_.setOvertimeDifficulty(overtimeDifficulty(overtimeSeconds_, gameplay_.overtime));
    enemies_.spawnWave(
        std::span<const EnemySpawn>{waveSpawnQueue_.data(), firstGroupSize});
    nextWaveSpawnIndex_ = firstGroupSize;
    waveSpawnGroupsDue_ = 0U;
    wavePortalsActive_ = true;
}

void Simulation::refillWaveSpawnQueue() {
    const auto core = buildings_.core();
    if (!core || map_.enemySpawnAnchors.empty()) return;

    ++waveSpawnCycle_;
    const WavePlan plan = waveDirector_.buildWave(
        wave_, core->gridPosition, waveFirstAnchorIndex_);
    waveSpawnQueue_.clear();
    waveSpawnQueue_.reserve(plan.spawns.size());
    for (EnemySpawn spawn : plan.spawns) {
        // Bosses appear once per night. Later batches remain endless but use
        // regular and elite enemies only.
        if (spawn.type == EnemyType::Boss) continue;
        spawn.healthMultiplier *= waveHealthScale_;
        spawn.damageMultiplier *= waveDamageScale_;
        waveSpawnQueue_.push_back(spawn);
    }
    routeSpawnsThroughPortals(
        waveSpawnQueue_, wavePortalPositions_, core->gridPosition,
        plan.groupSize);
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
    constexpr std::size_t TechnicalActiveEnemyLimit = 96U;
    const double progress = phaseDuration_ > 0.0
        ? std::clamp(
              1.0 - phaseTimeRemaining_ / phaseDuration_, 0.0, 1.0)
        : 0.0;
    if (!finalNight_) {
        constexpr std::array<double, 2> WarningProgress{0.36, 0.76};
        for (int surge = 0; surge < 2; ++surge) {
            const std::uint8_t bit =
                static_cast<std::uint8_t>(1U << surge);
            if (progress < WarningProgress[
                    static_cast<std::size_t>(surge)] ||
                (waveSurgeWarningMask_ & bit) != 0U) continue;
            waveSurgeWarningMask_ |= bit;
            const std::size_t portalIndex = surgePortalIndex(
                wavePortalPositions_, upcomingAttackDirection_, surge);
            events_.push_back({
                .type = GameEventType::AttackDirectionWarned,
                .position = wavePortalPositions_[portalIndex].value_or(Vec3{}),
                .amount = static_cast<int>(portalIndex),
                .critical = true,
            });
        }
    }
    const bool firstSurge = !finalNight_ &&
        progress >= 0.40 && progress < 0.55;
    const bool finalSurge = !finalNight_ && progress >= 0.80;
    const int surge = firstSurge ? 0 : finalSurge ? 1 : -1;
    const OvertimeDifficulty overtime = overtimeDifficulty(overtimeSeconds_, gameplay_.overtime);
    const std::size_t baseTargetActive = static_cast<std::size_t>(
        std::min(34, 7 + std::min(wave_, 9) * 3 +
                 static_cast<int>(progress * 5.0)) +
        (firstSurge ? 5 : finalSurge ? 9 : 0));
    const std::size_t targetActive = std::min(MaximumActiveEnemies,
        static_cast<std::size_t>(std::ceil(static_cast<double>(baseTargetActive) * overtime.spawnRate)));
    const double baseInterval = std::clamp(
        waveSpawnInterval_, 0.35, 2.0) / overtime.spawnRate;
    const double interval = surge >= 0
        ? baseInterval * 0.6 : baseInterval;

    if (finalSurge && waveBossSpawn_ &&
        enemies_.activeCount() < TechnicalActiveEnemyLimit) {
        EnemySpawn boss = *waveBossSpawn_;
        if (const auto core = buildings_.core()) {
            const std::size_t portalIndex = surgePortalIndex(
                wavePortalPositions_, upcomingAttackDirection_, 1);
            if (wavePortalPositions_[portalIndex]) {
                moveGroupToPortal(
                    std::span<EnemySpawn>{&boss, 1U},
                    *wavePortalPositions_[portalIndex],
                    core->gridPosition);
            }
        }
        enemies_.spawnGroup(std::span<const EnemySpawn>{&boss, 1U});
        waveBossSpawn_.reset();
    }

    waveSpawnTimeRemaining_ -= deltaSeconds;
    while (waveSpawnTimeRemaining_ <= 0.0) {
        waveSpawnGroupsDue_ = std::min(
            std::size_t{32}, waveSpawnGroupsDue_ + 1U);
        waveSpawnTimeRemaining_ += interval;
    }
    while (waveSpawnGroupsDue_ > 0U &&
           enemies_.activeCount() < targetActive) {
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
        if (groupSize == 0U || active + groupSize > technicalLimit) break;
        const std::span<const EnemySpawn> queuedGroup{
            waveSpawnQueue_.data() + nextWaveSpawnIndex_, groupSize};
        if (surge >= 0) {
            const std::size_t portalIndex = surgePortalIndex(
                wavePortalPositions_, upcomingAttackDirection_, surge);
            if (wavePortalPositions_[portalIndex]) {
                if (const auto core = buildings_.core()) {
                    std::vector<EnemySpawn> group(
                        queuedGroup.begin(), queuedGroup.end());
                    moveGroupToPortal(
                        group, *wavePortalPositions_[portalIndex],
                        core->gridPosition);
                    enemies_.spawnGroup(group);
                } else {
                    enemies_.spawnGroup(queuedGroup);
                }
            } else {
                enemies_.spawnGroup(queuedGroup);
            }
        } else {
            enemies_.spawnGroup(queuedGroup);
        }
        nextWaveSpawnIndex_ += groupSize;
        --waveSpawnGroupsDue_;
    }
    // Do not bank old groups while the arena is full: they would all arrive
    // at once after a strong attack clears it.
    waveSpawnGroupsDue_ = 0U;
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
    waveBossSpawn_.reset();
    waveSurgeWarningMask_ = 0U;
    upcomingAttackDirection_.reset();
    upcomingAttackDirections_.fill(false);
    wavePortalPositions_.fill(std::nullopt);
    wavePortalsActive_ = false;
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
        lootChests_.spawnAdditionalChests(
            additionalChests, terrain_.seed(), map_.worldLimit,
            terrain_, resources_.nodes(), playerPosition_,
            preferredCenter, 28.0, 8.0,
            LootChestPurpose::Reward);
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
        prepareRunShop();
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
        waveStartCheckpoint_ =
            std::make_unique<SuspendedRunState>(
                std::move(checkpoint));
        static_cast<void>(beginNextFinalNightWave());
        return;
    }
    state_ = RunState::WaveComplete;
    if (!sandboxMode_) prepareRunShop();
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
