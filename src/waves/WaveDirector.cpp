#include "waves/WaveDirector.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <utility>

namespace ian {
namespace {

std::vector<Vec3> defaultSpawnAnchors() {
    return {
        {0.0, 0.0, -20.0},
        {20.0, 0.0, 0.0},
        {-20.0, 0.0, 0.0},
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
    const int clampedWave = std::clamp(wave, 1, WaveCount);
    const WaveDefinition& composition =
        definitions_[static_cast<std::size_t>(clampedWave - 1)];
    firstAnchorIndex_ = firstAnchorIndex % spawnAnchors_.size();
    spawnBuffer_.clear();
    append(EnemyType::Basic, composition.basic, corePosition, composition.groupSize);
    append(EnemyType::Fast, composition.fast, corePosition, composition.groupSize);
    append(EnemyType::Heavy, composition.heavy, corePosition, composition.groupSize);
    if (composition.boss) {
        const Vec3 anchor = spawnAnchors_[firstAnchorIndex_];
        spawnBuffer_.push_back({
            .type = EnemyType::Boss,
            .position = {anchor.x, 1.6, anchor.z},
        });
    }
    return {
        .wave = clampedWave,
        .regularBudget = composition.budget,
        .hasBoss = composition.boss,
        .groupSize = composition.groupSize,
        .groupInterval = composition.groupInterval,
        .spawns = std::span<const EnemySpawn>{spawnBuffer_},
    };
}

void WaveDirector::append(EnemyType type, int count, GridPosition corePosition,
                          int groupSize) {
    for (int index = 0; index < count; ++index) {
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
        const double height =
            type == EnemyType::Fast ? 0.675 : (type == EnemyType::Heavy ? 1.0 : 0.8);

        const Vec3 anchor = spawnAnchors_[anchorIndex];
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
        spawnBuffer_.push_back({type, position});
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
