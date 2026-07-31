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
constexpr double Pi = 3.14159265358979323846;
constexpr int MaximumQuantityGrowthSteps = 90;

std::vector<Vec3> defaultSpawnAnchors() {
    return {
        {0.0, 0.0, -20.0},
        {20.0, 0.0, 0.0},
        {-20.0, 0.0, 0.0},
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
    const double safeDistance =
        std::max(distance, MinimumSpawnRadius);
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
    }
    return 0;
}

WaveDirector::WaveDirector(
    std::array<WaveDefinition, GameBalance::WaveCount> definitions,
    std::vector<Vec3> spawnAnchors)
    : spawnAnchors_(spawnAnchors.empty() ? defaultSpawnAnchors()
                                        : std::move(spawnAnchors)),
      definitions_(definitions) {
    spawnBuffer_.reserve(128);
}

WavePlan WaveDirector::buildWave(int wave, GridPosition corePosition,
                                 std::size_t firstAnchorIndex) {
    const int normalizedWave = std::max(1, wave);
    const WaveDefinition composition =
        this->composition(normalizedWave);
    const double waveIndex =
        static_cast<double>(normalizedWave - 1);
    const double healthMultiplier =
        1.0 + waveIndex * 0.10;
    const double damageMultiplier =
        1.0 + waveIndex * 0.06;
    firstAnchorIndex_ = firstAnchorIndex % spawnAnchors_.size();
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
        .spawns = std::span<const EnemySpawn>{spawnBuffer_},
    };
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
    result.boss = normalizedWave % ConfiguredWaveCount == 0;
    result.groupSize = std::min(24, 10 + extraWaves / 4);
    result.groupInterval = std::max(
        0.35, 1.2 - 0.02 * static_cast<double>(extraWaves));
    result.budget =
        result.basic * enemyBudgetCost(EnemyType::Basic) +
        result.fast * enemyBudgetCost(EnemyType::Fast) +
        result.heavy * enemyBudgetCost(EnemyType::Heavy) +
        result.ranged * enemyBudgetCost(EnemyType::Ranged) +
        result.sapper * enemyBudgetCost(EnemyType::Sapper) +
        result.flying * enemyBudgetCost(EnemyType::Flying);
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
            (firstAnchorIndex_ + groupIndex) % spawnAnchors_.size();
        const int laneIndex = static_cast<int>(indexInGroup) - groupSize / 2;
        const int row = static_cast<int>(groupIndex / spawnAnchors_.size());
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
        const Vec3 position{
            anchor.x + tangentX * lane + outwardX * depth,
            height,
            anchor.z + tangentZ * lane + outwardZ * depth,
        };
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
