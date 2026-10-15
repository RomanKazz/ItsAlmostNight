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
constexpr double MainWaveDifficultyScale = 0.85;

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

} // namespace

int enemyBudgetCost(EnemyType type) {
    switch (type) {
    case EnemyType::Basic:
        return 1;
    case EnemyType::Fast:
        return 2;
    case EnemyType::Heavy:
        return 5;
    case EnemyType::Boss:
        return 30;
    case EnemyType::Ranged:
        return 3;
    case EnemyType::Sapper:
        return 4;
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
}

WavePlan WaveDirector::buildWave(int wave, GridPosition corePosition,
                                 std::size_t firstAnchorIndex, WaveContext context) {
    const int normalizedWave = std::max(1, wave);
    const WaveDefinition base = this->composition(normalizedWave);
    const NightPattern pattern = nightPattern(base, normalizedWave, context.runSeed);
    WaveDefinition composition = nightComposition(base, pattern, context.act);
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
        composition.sapper = bounded(composition.sapper); composition.flying = bounded(composition.flying);
        composition.splitter = bounded(composition.splitter);
    }
    const double waveIndex =
        static_cast<double>(normalizedWave - 1);
    const double healthMultiplier =
        MainWaveDifficultyScale * enemyWaveHealthGrowth(normalizedWave);
    const double damageMultiplier =
        MainWaveDifficultyScale * (1.0 + waveIndex * 0.06);
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
    append(EnemyType::Sapper, composition.sapper, corePosition,
           composition.groupSize, healthMultiplier, damageMultiplier);
    append(EnemyType::Flying, composition.flying, corePosition,
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
        const int bossCycle = normalizedWave / ConfiguredWaveCount;
        const double bossHealthMultiplier =
            healthMultiplier *
            (1.0 + 0.35 * static_cast<double>(bossCycle - 1));
        const double bossDamageMultiplier =
            damageMultiplier *
            (1.0 + 0.20 * static_cast<double>(bossCycle - 1));
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
        const auto counts = waveEnemyCounts(nightComposition(base, result.pattern, act));
        for (std::size_t i = 0; i < counts.size(); ++i) result.enemies[i] = std::max(result.enemies[i], counts[i]);
    }
    return result;
}

int WaveDirector::applyEliteUpgrades(int wave) {
    if (wave < 3) {
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

    int requested = wave <= 4 ? 1 : wave <= 6 ? 2 :
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
        return definitions_[static_cast<std::size_t>(normalizedWave - 1)];
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
    result.sapper += (growth + 2) / 3;
    result.flying += growth / 3;
    result.splitter += (growth + 3) / 4;
    result.boss = normalizedWave % ConfiguredWaveCount == 0;
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
        result.sapper * enemyBudgetCost(EnemyType::Sapper) +
        result.flying * enemyBudgetCost(EnemyType::Flying) +
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
        } else if (type == EnemyType::Flying) {
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
