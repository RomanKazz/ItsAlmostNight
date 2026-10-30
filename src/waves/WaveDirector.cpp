#include "enemies/EnemyDifficulty.hpp"
#include "waves/WaveDirector.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <numeric>
#include <utility>

namespace ian {
namespace {

constexpr double MinimumSpawnRadius = 20.0;
constexpr double MaximumSpawnRadius = 30.0;
constexpr double Pi = 3.14159265358979323846;
constexpr int MaximumQuantityGrowthSteps = 90;
// 8% less health and 8% less damage gives roughly 15% less sustained
// pressure (0.92 * 0.92 = 0.8464) without changing wave composition.
constexpr double MainWaveDifficultyScale = 0.85 * 0.92;

std::vector<Vec3> defaultSpawnAnchors() {
    return {
        {0.0, 0.0, -24.0},
        {24.0, 0.0, 0.0},
        {0.0, 0.0, 24.0},
        {-24.0, 0.0, 0.0},
    };
}

Vec3 safeSpawnAnchor(
    Vec3 anchor, GridPosition corePosition,
    std::size_t anchorIndex, std::size_t anchorCount) {
    double outwardX =
        anchor.x - static_cast<double>(corePosition.x);
    double outwardZ =
        anchor.z - static_cast<double>(corePosition.z);
    double distance = std::hypot(outwardX, outwardZ);
    if (distance <= 1e-9) {
        const double angle =
            (2.0 * Pi * static_cast<double>(anchorIndex)) /
            static_cast<double>(std::max<std::size_t>(1, anchorCount));
        outwardX = std::cos(angle);
        outwardZ = std::sin(angle);
        distance = 1.0;
    }
    outwardX /= distance;
    outwardZ /= distance;
    const double safeDistance = std::clamp(
        distance, MinimumSpawnRadius,
        MaximumSpawnRadius);
    return {
        static_cast<double>(corePosition.x) +
            outwardX * safeDistance,
        anchor.y,
        static_cast<double>(corePosition.z) +
            outwardZ * safeDistance,
    };
}

WaveDefinition encounterComposition(
    const WaveDefinition& base, NightPattern pattern, int act, int night) {
    if (night <= 5) return base;
    WaveDefinition result = nightComposition(base, pattern, act);
    const auto capRole = [&](int& count, int maximum, int cost) {
        if (count > maximum) {
            result.basic += (count - maximum) * cost;
            count = maximum;
        }
    };
    if (night == 6) {
        // The first procedural themes should change the mix, not turn the
        // first air or siege night into a wall of newly introduced enemies.
        capRole(result.fast, 10, 2);
        capRole(result.heavy, 5, 5);
        capRole(result.ranged, 4, 3);
        capRole(result.splitter, 2, 4);
        capRole(result.flying, 3, 3);
    } else if (night == 7) {
        capRole(result.bee, 3, 3);
    } else if (night == 8) {
        // The first boss remains the focal point of its encounter.
        capRole(result.heavy, 4, 5);
        capRole(result.ranged, 5, 3);
        capRole(result.flying, 5, 3);
        capRole(result.bee, 2, 3);
        capRole(result.splitter, 2, 4);
    } else if (night == 9) {
        capRole(result.airTank, 2, 8);
    }
    // The first encounter after a role unlock must actually teach that role,
    // even if its chosen theme would otherwise round the count down to zero.
    const auto introduce = [&](int& count, int cost) {
        if (count == 0 && result.basic >= cost) {
            result.basic -= cost;
            ++count;
        }
    };
    if (night == 6 && base.flying > 0) introduce(result.flying, 3);
    if (night == 7 && base.bee > 0) introduce(result.bee, 3);
    if (night == 9 && base.airTank > 0) introduce(result.airTank, 8);
    return result;
}

} // namespace

WaveDifficulty waveDifficulty(int wave) {
    const int night = std::max(1, wave);
    if (night <= 6) {
        // Gentler first-night pressure; later onboarding keeps its existing curve.
        constexpr std::array<double, 6> Health{.65, .9975, 1.3375, 1.737, 2.1875, 2.6975};
        constexpr std::array<double, 6> Damage{.60, .77, .86, .96, 1.07, 1.17};
        constexpr std::array<double, 6> SpawnRate{.95, 1.0, 1.06, 1.12, 1.18, 1.24};
        const std::size_t index = static_cast<std::size_t>(night - 1);
        const double baseDamage = MainWaveDifficultyScale * (1.0 + .06 * (night - 1));
        return {
            .regularHealthScale = Health[index] /
                (MainWaveDifficultyScale * enemyWaveHealthGrowth(night)),
            .regularDamageScale = Damage[index] / baseDamage,
            .extraActiveEnemies = 0,
            .spawnRateMultiplier = SpawnRate[index],
        };
    }
    // Blend the gentler onboarding curve into the reduced polynomial by
    // night twelve. Damage, population and overtime are independent.
    constexpr double SixthNightScale = 2.6975 /
        (MainWaveDifficultyScale * enemyWaveHealthGrowth(6));
    const double bridgeProgress =
        static_cast<double>(std::clamp(night, 6, 12) - 6) / 6.0;
    const double bridge = bridgeProgress * bridgeProgress;
    const int lateNights = std::clamp(wave, 3, 12) - 3;
    const double statSteps = static_cast<double>(
        std::min(lateNights, 5));
    return {
        .regularHealthScale = SixthNightScale + (1.0 - SixthNightScale) * bridge,
        .regularDamageScale = night < 12
            ? (1.17 + (MainWaveDifficultyScale * (1.0 + .06 * 11.0) * .95 - 1.17) *
                bridgeProgress) /
                (MainWaveDifficultyScale * (1.0 + .06 * (night - 1)))
            : 1.0 - 0.01 * statSteps,
        .extraActiveEnemies = static_cast<std::size_t>(lateNights * 2),
        .spawnRateMultiplier = 1.0 + 0.08 *
            static_cast<double>(std::min(lateNights, 8)),
    };
}

double waveTargetActiveEnemies(int wave, NightPattern pattern, double progress) {
    const int night=std::max(1,wave);
    const int act=nightAct(std::clamp(progress,0.0,1.0));
    constexpr std::array<double,10> Population{6,14,22,32,45,60,78,85,110,135};
    const double base=night<=10 ? Population[static_cast<std::size_t>(night-1)]
        : std::min(512.0,160.0+18.0*(night-11));
    const double patternScale=night<=5 ? 1.0 : std::max(.85,nightPopulationScale(pattern,night));
    return std::min(512.0,base*(1+.10*act)*patternScale);
}

int enemyBudgetCost(EnemyType type) {
    switch (type) {
    case EnemyType::Basic:
        return 1;
    case EnemyType::Fast:
        return 2;
    case EnemyType::AirTank:
        return 8;
    case EnemyType::Heavy:
        return 5;
    case EnemyType::Boss:
        return 30;
    case EnemyType::Ranged:
        return 3;
    case EnemyType::Sapper:
        return 4;
    case EnemyType::Bee:
    case EnemyType::Flying:
        return 3;
    case EnemyType::Splitter:
        return 4;
    case EnemyType::Splitling:
        return 0;
    }
    return 0;
}

WaveDirector::WaveDirector(
    std::array<WaveDefinition, GameBalance::WaveCount> definitions,
    std::vector<Vec3> spawnAnchors)
    : spawnAnchors_(spawnAnchors.empty() ? defaultSpawnAnchors()
                                        : std::move(spawnAnchors)),
      definitions_(definitions) {
    spawnBuffer_.reserve(MaximumWaveEnemies);
    scratchBuffer_.reserve(MaximumWaveEnemies);
    // Keep the legacy enemy ID/config field, but retire it from every wave.
    for (auto& definition : definitions_) definition.sapper = 0;
}

WavePlan WaveDirector::buildWave(int wave, GridPosition corePosition,
                                 std::size_t firstAnchorIndex, WaveContext context) {
    const int normalizedWave = std::max(1, wave);
    const WaveDefinition base = this->composition(normalizedWave);
    const NightPattern pattern = nightPattern(base, normalizedWave, context.runSeed);
    WaveDefinition composition = encounterComposition(
        base, pattern, context.act, normalizedWave);
    // Proportionally bound very late rosters before append, so a large basic
    // contingent cannot consume the pool and erase all specialized roles.
    const auto requested = waveEnemyCounts(composition);
    const std::size_t populationLimit = MaximumWaveEnemies - static_cast<std::size_t>(composition.boss);
    std::int64_t population = 0;
    for (std::size_t role = 0; role < requested.size(); ++role)
        if (role != static_cast<std::size_t>(EnemyType::Boss)) population += std::max(0, requested[role]);
    if (population > static_cast<std::int64_t>(populationLimit)) {
        const auto bounded = [&](int count) {
            return count <= 0 ? 0 : std::max(1, static_cast<int>(
                static_cast<std::int64_t>(count) * static_cast<std::int64_t>(populationLimit - 8) / population));
        };
        composition.basic = bounded(composition.basic); composition.fast = bounded(composition.fast);
        composition.heavy = bounded(composition.heavy); composition.ranged = bounded(composition.ranged);
        composition.flying = bounded(composition.flying); composition.bee=bounded(composition.bee);
        composition.airTank = bounded(composition.airTank);
        composition.splitter = bounded(composition.splitter);
    }
    const double waveIndex =
        static_cast<double>(normalizedWave - 1);
    const WaveDifficulty difficulty = waveDifficulty(normalizedWave);
    const double baseHealthMultiplier =
        MainWaveDifficultyScale * enemyWaveHealthGrowth(normalizedWave);
    const double baseDamageMultiplier =
        MainWaveDifficultyScale * (1.0 + std::min(10.0,waveIndex) * 0.06);
    const double healthMultiplier =
        baseHealthMultiplier * difficulty.regularHealthScale;
    const double damageMultiplier =
        baseDamageMultiplier * waveDifficulty(std::min(11,normalizedWave)).regularDamageScale;
    firstAnchorIndex_ = firstAnchorIndex % spawnAnchors_.size();
    attackFrontCount_ = std::min(
        spawnAnchors_.size(),
        static_cast<std::size_t>(normalizedWave == 1 ? 1 :
                                 normalizedWave == 2 ? 2 :
                                                       spawnAnchors_.size()));
    spawnBuffer_.clear();
    spawnLimit_ = MaximumWaveEnemies -
        static_cast<std::size_t>(composition.boss);
    append(EnemyType::Basic, composition.basic, corePosition,
           composition.groupSize, healthMultiplier, damageMultiplier);
    append(EnemyType::Fast, composition.fast, corePosition,
           composition.groupSize, healthMultiplier, damageMultiplier);
    append(EnemyType::Heavy, composition.heavy, corePosition,
           composition.groupSize, healthMultiplier, damageMultiplier);
    append(EnemyType::Ranged, composition.ranged, corePosition,
           composition.groupSize, healthMultiplier, damageMultiplier);
    append(EnemyType::Flying, composition.flying, corePosition,
           composition.groupSize, healthMultiplier, damageMultiplier);
    append(EnemyType::AirTank, composition.airTank, corePosition,
           composition.groupSize, healthMultiplier, damageMultiplier);
    append(EnemyType::Bee, composition.bee, corePosition,
           composition.groupSize, healthMultiplier, damageMultiplier);
    append(EnemyType::Splitter, composition.splitter, corePosition,
           composition.groupSize, healthMultiplier, damageMultiplier);
    // Coherent role groups replace the old per-enemy uniform mixture. Choose
    // batches by remaining threat budget; reuse storage across every refill.
    scratchBuffer_.assign(spawnBuffer_.begin(), spawnBuffer_.end());
    constexpr std::size_t RoleCount = GameBalance::EnemyTypeCount;
    std::array<std::size_t, RoleCount> begins{}, counts{}, used{};
    for (std::size_t i = 0; i < scratchBuffer_.size(); ++i) {
        const auto role = static_cast<std::size_t>(scratchBuffer_[i].type);
        if (counts[role]++ == 0) begins[role] = i;
    }
    std::uint64_t random = mixBits64(static_cast<std::uint64_t>(context.runSeed) ^
        static_cast<std::uint64_t>(normalizedWave) * 0x9e3779b97f4a7c15ULL ^
        static_cast<std::uint64_t>(context.cycle) * 0xd1b54a32d192ed03ULL ^
        static_cast<std::uint64_t>(std::clamp(context.act, 0, 2)) * 0x94d049bb133111ebULL);
    std::size_t output = 0;
    while (output < spawnBuffer_.size()) {
        const auto end = std::min(spawnBuffer_.size(), output + static_cast<std::size_t>(std::max(1, composition.groupSize)));
        while (output < end) {
            std::size_t remainingBudget = 0;
            for (std::size_t role = 0; role < RoleCount; ++role)
                remainingBudget += (counts[role] - used[role]) * static_cast<std::size_t>(enemyBudgetCost(static_cast<EnemyType>(role)));
            if (remainingBudget == 0) break;
            random = mixBits64(random + 0x9e3779b97f4a7c15ULL);
            std::size_t pick = random % remainingBudget, chosen = 0;
            for (; chosen < RoleCount; ++chosen) {
                const auto weight = (counts[chosen] - used[chosen]) * static_cast<std::size_t>(enemyBudgetCost(static_cast<EnemyType>(chosen)));
                if (pick < weight) break;
                pick -= weight;
            }
            const auto batch = std::min(end - output, counts[chosen] - used[chosen]);
            for (std::size_t i = 0; i < batch; ++i, ++output) {
                EnemySpawn spawn = scratchBuffer_[begins[chosen] + used[chosen]++];
                spawn.position.x = scratchBuffer_[output].position.x;
                spawn.position.z = scratchBuffer_[output].position.z;
                spawnBuffer_[output] = spawn;
            }
        }
    }
    if (composition.boss) {
        const int bossCycle = normalizedWave / BossWaveInterval;
        const double bossHealthMultiplier =
            baseHealthMultiplier *
            (1.0 + 0.35 * static_cast<double>(bossCycle - 1));
        const double bossDamageMultiplier =
            baseDamageMultiplier *
            (1.0 + 0.02 * static_cast<double>(bossCycle - 1));
        const Vec3 anchor = safeSpawnAnchor(
            spawnAnchors_[firstAnchorIndex_], corePosition,
            firstAnchorIndex_, spawnAnchors_.size());
        if (spawnBuffer_.size() < MaximumWaveEnemies) {
            spawnBuffer_.push_back({
                .type = EnemyType::Boss,
                .position = {anchor.x, 1.6, anchor.z},
                .healthMultiplier = bossHealthMultiplier,
                .damageMultiplier = bossDamageMultiplier,
            });
        }
    }
    const int eliteCount = applyEliteUpgrades(normalizedWave);
    const int actualRegularBudget =
        std::accumulate(
            spawnBuffer_.begin(), spawnBuffer_.end(), 0,
            [](int budget, const EnemySpawn& spawn) {
                return spawn.type == EnemyType::Boss
                           ? budget
                           : budget + enemyBudgetCost(spawn.type);
            });
    return {
        .wave = normalizedWave,
        .regularBudget = actualRegularBudget,
        .hasBoss = composition.boss,
        .groupSize = composition.groupSize,
        .groupInterval = composition.groupInterval,
        .eliteCount = eliteCount,
        .spawns = std::span<const EnemySpawn>{spawnBuffer_},
        .pattern = pattern,
    };
}

NightForecast WaveDirector::forecast(int wave, std::uint32_t runSeed) const {
    const auto base = composition(wave);
    NightForecast result{.pattern = nightPattern(base, wave, runSeed)};
    for (int act = 0; act < 3; ++act) {
        const auto counts = waveEnemyCounts(encounterComposition(
            base, result.pattern, act, wave));
        for (std::size_t i = 0; i < counts.size(); ++i) result.enemies[i] = std::max(result.enemies[i], counts[i]);
    }
    return result;
}

int WaveDirector::applyEliteUpgrades(int wave) {
    if (wave < 5) {
        return 0;
    }
    std::vector<std::size_t> eligible;
    eligible.reserve(spawnBuffer_.size());
    for (std::size_t index = 0; index < spawnBuffer_.size(); ++index) {
        const EnemyType type = spawnBuffer_[index].type;
        if (type != EnemyType::Boss && type != EnemyType::Splitling) {
            eligible.push_back(index);
        }
    }
    if (eligible.empty()) {
        return 0;
    }

    int requested = wave == 5 ? 1 : wave == 6 ? 2 :
        2 + (wave - 6) / 2;
    const int populationLimit = std::max(
        1, static_cast<int>(std::ceil(
               static_cast<double>(eligible.size()) * 0.12)));
    requested = std::clamp(
        requested, 1,
        std::min(populationLimit,
                 static_cast<int>(eligible.size())));

    for (int elite = 0; elite < requested; ++elite) {
        const std::size_t pick =
            (static_cast<std::size_t>(wave) * 7U +
             static_cast<std::size_t>(elite) * 11U) % eligible.size();
        EnemySpawn& spawn = spawnBuffer_[eligible[pick]];
        const int affixIndex = (wave + elite * 2) % 3;
        const EliteAffix affix = affixIndex == 0
            ? EliteAffix::Berserker
            : affixIndex == 1 ? EliteAffix::Warden
                              : EliteAffix::Volatile;
        spawn.eliteAffixes = eliteAffixMask(affix);
        eligible.erase(eligible.begin() +
                       static_cast<std::ptrdiff_t>(pick));
    }
    return requested;
}

WaveDefinition WaveDirector::composition(int wave) const {
    const int normalizedWave = std::max(1, wave);
    if (normalizedWave <= ConfiguredWaveCount) {
        WaveDefinition result = definitions_[static_cast<std::size_t>(normalizedWave - 1)];
        // Override legacy sixth-night boss flags from older wave configs.
        result.boss = normalizedWave % BossWaveInterval == 0;
        // Old/custom wave files cannot bypass the onboarding role gates.
        const auto defer = [&](int& count, int cost) {
            result.basic += count * cost;
            count = 0;
        };
        if (normalizedWave < 2) defer(result.fast, 2);
        if (normalizedWave < 3) defer(result.heavy, 5);
        if (normalizedWave < 4) defer(result.ranged, 3);
        if (normalizedWave < 5) defer(result.splitter, 4);
        if (normalizedWave < 6) defer(result.flying, 3);
        if (normalizedWave < 7) defer(result.bee, 3);
        if (normalizedWave < 9) defer(result.airTank, 8);
        result.sapper = 0;
        return result;
    }

    WaveDefinition result = definitions_.back();
    const int extraWaves =
        normalizedWave - ConfiguredWaveCount;
    const int growth = std::min(
        extraWaves, MaximumQuantityGrowthSteps);
    result.basic += growth * 2;
    result.fast += growth;
    result.heavy += (growth + 1) / 2;
    result.ranged += growth / 2;
    result.flying += growth / 3;
    result.bee += growth / 4;
    result.airTank += growth / 6;
    result.splitter += (growth + 3) / 4;
    if (normalizedWave < 9) {
        result.basic += result.airTank * 8;
        result.airTank = 0;
    }
    if (normalizedWave >= 7 && result.basic >= 3) {
        ++result.bee;
        result.basic -= 3;
    }
    if (normalizedWave >= 9 && result.basic >= 8) {
        ++result.airTank;
        result.basic -= 8;
    }
    result.boss = normalizedWave % BossWaveInterval == 0;
    result.groupSize = std::min(
        10, definitions_.back().groupSize + extraWaves / 4);
    result.groupInterval = std::max(
        0.8, definitions_.back().groupInterval -
                 0.05 * static_cast<double>(extraWaves));
    result.budget =
        result.basic * enemyBudgetCost(EnemyType::Basic) +
        result.fast * enemyBudgetCost(EnemyType::Fast) +
        result.heavy * enemyBudgetCost(EnemyType::Heavy) +
        result.ranged * enemyBudgetCost(EnemyType::Ranged) +
        result.flying * enemyBudgetCost(EnemyType::Flying) +
        result.bee * enemyBudgetCost(EnemyType::Bee) +
        result.airTank * enemyBudgetCost(EnemyType::AirTank) +
        result.splitter * enemyBudgetCost(EnemyType::Splitter);
    return result;
}

void WaveDirector::append(EnemyType type, int count, GridPosition corePosition,
                          int groupSize, double healthMultiplier,
                          double damageMultiplier) {
    for (int index = 0;
         index < count &&
         spawnBuffer_.size() < spawnLimit_;
         ++index) {
        const std::size_t globalIndex = spawnBuffer_.size();
        const std::size_t configuredGroupSize = static_cast<std::size_t>(groupSize);
        const std::size_t groupIndex = globalIndex / configuredGroupSize;
        const std::size_t indexInGroup = globalIndex % configuredGroupSize;
        const std::size_t anchorIndex =
            (firstAnchorIndex_ + groupIndex % attackFrontCount_) %
            spawnAnchors_.size();
        const int laneIndex = static_cast<int>(indexInGroup) - groupSize / 2;
        const int row = static_cast<int>(groupIndex / attackFrontCount_);
        const double lane = static_cast<double>(laneIndex) * 1.1;
        const double depth = static_cast<double>(row) * 1.2;
        double height = 0.8;
        if (type == EnemyType::Fast) {
            height = 0.675;
        } else if (type == EnemyType::Heavy) {
            height = 1.0;
        } else if (type == EnemyType::Ranged) {
            height = 0.85;
        } else if (type == EnemyType::Sapper) {
            height = 0.78;
        } else if (enemyIsFlying(type)) {
            height = 2.4;
        } else if (type == EnemyType::Splitter) {
            height = 1.05;
        } else if (type == EnemyType::Splitling) {
            height = 0.55;
        }

        const Vec3 anchor = safeSpawnAnchor(
            spawnAnchors_[anchorIndex], corePosition,
            anchorIndex, spawnAnchors_.size());
        const double fromCoreX = anchor.x - static_cast<double>(corePosition.x);
        const double fromCoreZ = anchor.z - static_cast<double>(corePosition.z);
        const double length = std::max(0.001, std::hypot(fromCoreX, fromCoreZ));
        const double outwardX = fromCoreX / length;
        const double outwardZ = fromCoreZ / length;
        const double tangentX = -outwardZ;
        const double tangentZ = outwardX;
        Vec3 position{
            anchor.x + tangentX * lane + outwardX * depth,
            height,
            anchor.z + tangentZ * lane + outwardZ * depth,
        };
        const double positionOffsetX =
            position.x - static_cast<double>(corePosition.x);
        const double positionOffsetZ =
            position.z - static_cast<double>(corePosition.z);
        const double positionDistance = std::hypot(
            positionOffsetX, positionOffsetZ);
        if (positionDistance > MaximumSpawnRadius) {
            const double scale =
                MaximumSpawnRadius / positionDistance;
            position.x = static_cast<double>(corePosition.x) +
                positionOffsetX * scale;
            position.z = static_cast<double>(corePosition.z) +
                positionOffsetZ * scale;
        }
        spawnBuffer_.push_back({
            .type = type,
            .position = position,
            .healthMultiplier = healthMultiplier,
            .damageMultiplier = damageMultiplier,
        });
    }
}

std::size_t leastVisibleSpawnAnchor(std::span<const Vec3> anchors,
                                    Vec3 viewerPosition, Vec3 viewDirection) {
    if (anchors.empty()) {
        return 0;
    }
    std::size_t leastVisible = 0;
    double lowestAlignment = 2.0;
    for (std::size_t index = 0; index < anchors.size(); ++index) {
        const double offsetX = anchors[index].x - viewerPosition.x;
        const double offsetZ = anchors[index].z - viewerPosition.z;
        const double length = std::hypot(offsetX, offsetZ);
        if (length <= 1e-9) {
            continue;
        }
        const double alignment =
            (offsetX / length) * viewDirection.x + (offsetZ / length) * viewDirection.z;
        if (alignment < lowestAlignment) {
            lowestAlignment = alignment;
            leastVisible = index;
        }
    }
    return leastVisible;
}

} // namespace ian
