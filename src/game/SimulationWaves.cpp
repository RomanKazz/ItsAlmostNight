#include "game/Simulation.hpp"
#include "enemies/EnemyDifficulty.hpp"

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

std::array<std::optional<Vec3>, MaximumAttackPortals> portalPositions(
    std::span<const EnemySpawn> spawns, GridPosition corePosition,
    bool addSecondPortal, double worldLimit) {
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

    std::array<std::optional<Vec3>, MaximumAttackPortals> result{};
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

    if (addSecondPortal) {
        // Keep the original lane at each front, then place its second lane
        // sideways. Search both sides and a few outward offsets; omit a
        // candidate if the terrain is too small to fit it safely.
        constexpr std::array<double, 6> SideOffsets{
            7.0, -7.0, 10.5, -10.5, 14.0, -14.0};
        constexpr std::array<double, 3> OutwardOffsets{0.0, 3.5, 7.0};
        const double edge = worldLimit - MinimumPortalSeparation * 0.5;
        for (std::size_t index = 0; index < DirectionCount; ++index) {
            if (!result[index]) continue;
            const Vec3 primary = *result[index];
            const double outwardX =
                primary.x - static_cast<double>(corePosition.x);
            const double outwardZ =
                primary.z - static_cast<double>(corePosition.z);
            const double length =
                std::max(0.001, std::hypot(outwardX, outwardZ));
            const double normalX = outwardX / length;
            const double normalZ = outwardZ / length;
            for (const double outward : OutwardOffsets) {
                for (const double side : SideOffsets) {
                    const Vec3 candidate{
                        primary.x - normalZ * side + normalX * outward,
                        0.0,
                        primary.z + normalX * side + normalZ * outward,
                    };
                    if (std::abs(candidate.x) > edge ||
                        std::abs(candidate.z) > edge) continue;
                    const bool separated = std::ranges::all_of(
                        result, [&candidate](const auto& portal) {
                            return !portal ||
                                std::hypot(
                                    portal->x - candidate.x,
                                    portal->z - candidate.z) >=
                                    MinimumPortalSeparation;
                        });
                    if (!separated) continue;
                    result[index + DirectionCount] = candidate;
                    break;
                }
                if (result[index + DirectionCount]) break;
            }
        }
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
    const std::array<std::optional<Vec3>, MaximumAttackPortals>& portals,
    GridPosition corePosition, int groupSize) {
    constexpr std::size_t DirectionCount = 4U;
    const std::size_t normalizedGroupSize =
        static_cast<std::size_t>(std::max(1, groupSize));
    std::array<std::size_t, DirectionCount> groupsPerDirection{};
    std::size_t groupDirection = DirectionCount;
    std::size_t groupPortal = 0U;
    for (std::size_t index = 0; index < spawns.size(); ++index) {
        EnemySpawn& spawn = spawns[index];
        const std::size_t directionIndex = attackDirectionIndex(
            attackDirection(spawn.position, corePosition));
        if (index % normalizedGroupSize == 0U) {
            groupDirection = directionIndex;
            const std::size_t groupNumber =
                groupsPerDirection[directionIndex]++;
            const std::size_t secondPortal =
                directionIndex + DirectionCount;
            groupPortal = groupNumber % 2U == 1U &&
                    portals[secondPortal]
                ? secondPortal : directionIndex;
        }
        const std::size_t portalIndex =
            directionIndex == groupDirection
                ? groupPortal : directionIndex;
        if (!portals[portalIndex]) continue;

        const Vec3 portal = *portals[portalIndex];
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

template<std::size_t PortalCount>
std::size_t encounterPortalIndex(
    const std::array<std::optional<Vec3>, PortalCount>& portals,
    std::optional<AttackDirection> primary, NightPattern pattern, int act, std::uint32_t serial) {
    std::array<std::size_t, PortalCount> available{};
    std::size_t count = 0;
    const auto start = primary ? attackDirectionIndex(*primary) : 0U;
    for (std::size_t step = 0; step < 4; ++step) {
        const auto index = (start + step) % 4;
        if (index < PortalCount && portals[index]) available[count++] = index;
    }
    for (std::size_t step = 0; step < 4; ++step) {
        const auto index = (start + step) % 4 + 4;
        if (index < PortalCount && portals[index]) available[count++] = index;
    }
    if (count == 0) return start;
    if (act == 0 || (pattern == NightPattern::Siege && act == 1)) return available[0];
    if (pattern == NightPattern::Crossfire)
        return available[(serial % 2 == 0 ? 0U : std::min(std::size_t{2}, count - 1))];
    if (pattern == NightPattern::Blitz && act == 1)
        return available[(1 + serial % 2) % count];
    return available[(static_cast<std::size_t>(act) + serial) % count];
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

void spreadGroupAcrossPortals(
    std::span<EnemySpawn> spawns,
    const std::array<std::optional<Vec3>, MaximumAttackPortals>& portals,
    GridPosition corePosition, std::uint32_t serial) {
    std::array<std::size_t, MaximumAttackPortals> available{};
    std::size_t portalCount = 0;
    for (std::size_t index = 0; index < portals.size(); ++index)
        if (portals[index]) available[portalCount++] = index;
    if (portalCount == 0) return;
    for (std::size_t index = 0; index < spawns.size(); ++index) {
        const std::size_t portalIndex = available[(serial + index) % portalCount];
        const Vec3 portal = *portals[portalIndex];
        const double outwardX = portal.x - static_cast<double>(corePosition.x);
        const double outwardZ = portal.z - static_cast<double>(corePosition.z);
        const double length = std::max(0.001, std::hypot(outwardX, outwardZ));
        const double lane = (static_cast<double>(index / portalCount) -
            static_cast<double>((spawns.size() - 1) / portalCount) * .5) * 1.2;
        spawns[index].position.x = portal.x - outwardZ / length * lane - outwardX / length * .15;
        spawns[index].position.z = portal.z + outwardX / length * lane - outwardZ / length * .15;
        spawns[index].arrivalPortal = portal;
        spawns[index].portalExitYaw = std::atan2(-outwardX, -outwardZ);
    }
}

} // namespace

void Simulation::resetNightEncounter() {
    wavePattern_ = NightPattern::Horde;
    waveEncounterAct_ = 0;
    waveCombatElapsed_ = 0;
    waveReplacementTime_ = 0;
    waveObservedActive_ = 0;
    waveReplacementDemand_ = 0;
    waveGroupSerial_ = 0;
}

void Simulation::prepareWave(const WavePlan& plan, GridPosition corePosition,
                             std::size_t firstAnchorIndex,
                             double healthScale,
                             double damageScale) {
    resetNightEncounter();
    wavePattern_ = plan.pattern;
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
    wavePortalPositions_ = portalPositions(
        plan.spawns, corePosition, plan.wave >= 4,
        map_.worldLimit);
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
    waveSpawnGroupSize_ = std::clamp(plan.groupSize, 1, 10);
    waveSpawnInterval_ = plan.groupInterval;
    nextWaveSpawnIndex_ = 0;
    waveSpawnTimeRemaining_ = std::clamp(
        waveSpawnInterval_, 0.35, 2.0);
    upcomingAttackDirection_ =
        attackDirection(
            map_.enemySpawnAnchors[waveFirstAnchorIndex_], corePosition);
    upcomingAttackDirections_.fill(false);
    for (std::size_t portal = 0; portal < wavePortalPositions_.size(); ++portal)
        if (wavePortalPositions_[portal]) upcomingAttackDirections_[portal % 4] = true;
    currentWaveHasBoss_ = std::ranges::any_of(
        plan.spawns, [](const EnemySpawn& spawn) { return spawn.type == EnemyType::Boss; });
}

void Simulation::beginPreparedWave() {
    // Safety net for direct/debug wave starts and restored runs that bypass
    // the regular BuildPhase -> Sunset transition.
    rebuildFlowFieldIfDirty();
    bestWave_ = std::max(bestWave_, wave_);
    enemies_.setWaveSpeedMultiplier(enemyWaveSpeedGrowth(wave_));
    if(!sandboxMode_ && wave_>gameplay_.overtime.afterWave) {
        finalNight_=true;
        phaseTimeRemaining_=phaseDuration_=0;
    }
    const std::size_t firstGroupSize = std::min({
        static_cast<std::size_t>(waveSpawnGroupSize_),
        waveSpawnQueue_.size(), MaximumActiveEnemies});
    enemies_.setOvertimeDifficulty(overtimeDifficulty(overtimeSeconds_, gameplay_.overtime));
    if (const auto core = buildings_.core())
        spreadGroupAcrossPortals(std::span<EnemySpawn>{waveSpawnQueue_.data(), firstGroupSize},
            wavePortalPositions_, core->gridPosition, 0);
    enemies_.spawnWave(std::span<const EnemySpawn>{waveSpawnQueue_.data(), firstGroupSize});
    nextWaveSpawnIndex_ = firstGroupSize;
    waveObservedActive_ = enemies_.activeCount();
    waveGroupSerial_ = static_cast<std::uint32_t>(firstGroupSize);
    waveSpawnGroupsDue_ = 0U;
    wavePortalsActive_ = true;
}

void Simulation::refillWaveSpawnQueue() {
    const auto core = buildings_.core();
    if (!core || map_.enemySpawnAnchors.empty()) return;

    ++waveSpawnCycle_;
    const WavePlan plan = waveDirector_.buildWave(
        wave_, core->gridPosition, waveFirstAnchorIndex_,
        {.runSeed = terrain_.seed(), .act = waveEncounterAct_, .cycle = waveSpawnCycle_});
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
    nextWaveSpawnIndex_ = 0U;
}

void Simulation::tickWaveSpawning(double deltaSeconds) {
    if (forceWaveCompletion_) return;
    waveCombatElapsed_ += deltaSeconds;
    if(finalNight_ && !sandboxMode_) {
        const int tier=gameplay_.overtime.afterWave+1+static_cast<int>(std::min(1000000.0,std::floor(overtimeSeconds_/60.0)));
        if(tier>wave_) {
            events_.push_back({.type=GameEventType::WaveCompleted,.amount=tier-1});
            wave_=tier;bestWave_=std::max(bestWave_,wave_);
            enemies_.setWaveSpeedMultiplier(enemyWaveSpeedGrowth(wave_));
            waveEncounterAct_=-1;
            if(const auto core=buildings_.core()) {
                const auto plan=waveDirector_.buildWave(wave_,core->gridPosition,waveFirstAnchorIndex_,{.runSeed=terrain_.seed()});
                const auto boss=std::ranges::find_if(plan.spawns,[](const auto& spawn){return spawn.type==EnemyType::Boss;});
                if(boss!=plan.spawns.end()) waveBossSpawn_=*boss;
            }
            events_.push_back({.type=GameEventType::WaveStarted,.amount=wave_});
        }
    }
    const double progress = phaseDuration_ > 0.0
        ? std::clamp(1.0 - phaseTimeRemaining_ / phaseDuration_, 0.0, 1.0)
        : std::min(.99, waveCombatElapsed_ / 60.0);
    const int act = nightAct(progress);
    // A scheduled boss must still arrive if a long tick crossed its final
    // act, or the enemy pool was full at its normal entrance time.
    if (act == 2 && waveBossSpawn_ && enemies_.activeCount() < MaximumActiveEnemies) {
        EnemySpawn boss = *waveBossSpawn_;
        const auto portal = encounterPortalIndex(wavePortalPositions_, upcomingAttackDirection_, wavePattern_, 2, 0);
        if (const auto core = buildings_.core(); core && wavePortalPositions_[portal])
            moveGroupToPortal(std::span<EnemySpawn>{&boss, 1}, *wavePortalPositions_[portal], core->gridPosition);
        enemies_.spawnGroup(std::span<const EnemySpawn>{&boss, 1});
        waveBossSpawn_.reset();
    }
    if (!finalNight_ && phaseTimeRemaining_ <= 0.0) {
        // Close regular reinforcements without deleting live actors, their
        // attacks, delayed splitter children, or the ordinary kill rewards.
        nextWaveSpawnIndex_ = waveSpawnQueue_.size();
        waveSpawnGroupsDue_ = 0U;
        waveReplacementDemand_ = 0U;
        wavePortalsActive_ = waveBossSpawn_.has_value();
        return;
    }
    if (act != waveEncounterAct_) {
        waveEncounterAct_ = act;
        waveGroupSerial_ = 0;
        refillWaveSpawnQueue();
    }
    if (!finalNight_) {
        constexpr std::array<double, 2> WarningProgress{.36, .76};
        for (int warning = 0; warning < 2; ++warning) {
            const auto bit = static_cast<std::uint8_t>(1U << warning);
            if (progress < WarningProgress[static_cast<std::size_t>(warning)] || (waveSurgeWarningMask_ & bit) != 0U) continue;
            waveSurgeWarningMask_ |= bit;
            const auto portal = encounterPortalIndex(wavePortalPositions_, upcomingAttackDirection_, wavePattern_, warning + 1, 0);
            events_.push_back({.type = GameEventType::AttackDirectionWarned,
                .position = wavePortalPositions_[portal].value_or(Vec3{}),
                .amount = static_cast<int>(portal % 4), .critical = true});
        }
    }
    const std::size_t activeBefore = enemies_.activeCount();
    if (waveObservedActive_ > activeBefore)
        waveReplacementDemand_ = std::min(MaximumActiveEnemies, waveReplacementDemand_ + waveObservedActive_ - activeBefore);
    waveReplacementTime_ = std::max(0.0, waveReplacementTime_ - deltaSeconds);
    // Telegraphs are real brief regrouping windows, not an accrued spawn debt.
    if (!finalNight_ && nightRegrouping(progress)) {
        waveReplacementDemand_ = 0;
        waveSpawnGroupsDue_ = 0;
        waveSpawnTimeRemaining_ = .15;
        waveObservedActive_ = activeBefore;
        return;
    }
    const WaveDifficulty difficulty = waveDifficulty(wave_);
    const OvertimeDifficulty overtime = overtimeDifficulty(overtimeSeconds_, gameplay_.overtime);
    const bool surge = act == 2 || (act == 1 && progress < .55);
    const auto targetActive = std::min(MaximumActiveEnemies, static_cast<std::size_t>(std::ceil(
        waveTargetActiveEnemies(wave_, wavePattern_, progress) * overtime.spawnRate)));
    const double interval = std::clamp(waveSpawnInterval_, .35, 2.0) /
        (difficulty.spawnRateMultiplier * overtime.spawnRate) * (surge ? .6 : 1.0);
    waveSpawnTimeRemaining_ -= deltaSeconds;
    while (waveSpawnTimeRemaining_ <= 0) {
        waveSpawnGroupsDue_ = std::min(std::size_t{2}, waveSpawnGroupsDue_ + 1);
        waveSpawnTimeRemaining_ += interval;
    }
    if (waveReplacementDemand_ > 0 && waveReplacementTime_ <= 0)
        waveSpawnGroupsDue_ = std::max(std::size_t{1}, waveSpawnGroupsDue_);
    while (waveSpawnGroupsDue_ > 0 && enemies_.activeCount() < targetActive) {
        if (nextWaveSpawnIndex_ >= waveSpawnQueue_.size()) {
            refillWaveSpawnQueue();
            if (waveSpawnQueue_.empty()) break;
        }
        const auto count = std::min({static_cast<std::size_t>(waveSpawnGroupSize_),
            waveSpawnQueue_.size() - nextWaveSpawnIndex_, targetActive - enemies_.activeCount()});
        // Bounded stack formation replaces the old per-group heap allocation.
        std::array<EnemySpawn, 10> group{};
        std::copy_n(waveSpawnQueue_.begin() + static_cast<std::ptrdiff_t>(nextWaveSpawnIndex_), count, group.begin());
        if (const auto core = buildings_.core(); core)
            spreadGroupAcrossPortals(std::span<EnemySpawn>{group}.first(count),
                wavePortalPositions_, core->gridPosition, waveGroupSerial_);
        enemies_.spawnGroup(std::span<const EnemySpawn>{group}.first(count));
        waveGroupSerial_ += static_cast<std::uint32_t>(count);
        nextWaveSpawnIndex_ += count;
        waveReplacementDemand_ -= std::min(waveReplacementDemand_, count);
        waveReplacementTime_ = .22 / overtime.spawnRate;
        --waveSpawnGroupsDue_;
    }
    if (enemies_.activeCount() >= targetActive) waveReplacementDemand_ = 0;
    waveObservedActive_ = enemies_.activeCount();
    waveSpawnGroupsDue_ = 0;
}

void Simulation::completeWave() {
    relicDawn();
    const bool completedBossWave = currentWaveHasBoss_;
    resetNightEncounter();
    waveStartCheckpoint_.reset();
    enemies_.clearProjectiles();
    towers_.clearProjectiles();
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
    if (!sandboxMode_ && !stageCleared_ && wave_>=StageClearWave) {
        stageCleared_=true;
        events_.push_back({.type=GameEventType::StageCleared,.amount=wave_,.critical=true});
        finalNight_=true;
        events_.push_back({.type=GameEventType::FinalNightStarted,.amount=wave_+1,.critical=true});
        static_cast<void>(beginNextFinalNightWave());
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
        nextWave, core->gridPosition, firstAnchor, {.runSeed = terrain_.seed()});
    prepareWave(plan,core->gridPosition,firstAnchor,1.0,1.0);
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
