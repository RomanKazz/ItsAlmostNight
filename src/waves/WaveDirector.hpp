#pragma once

#include "buildings/BuildingSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"

#include <span>
#include <vector>

namespace ian {

struct WavePlan {
    int wave;
    int regularBudget;
    bool hasBoss;
    int groupSize;
    double groupInterval;
    std::span<const EnemySpawn> spawns;
};

class WaveDirector {
  public:
    static constexpr int WaveCount = 6;

    explicit WaveDirector(
        std::array<WaveDefinition, GameBalance::WaveCount> definitions =
            GameBalance::defaults().waves,
        std::vector<Vec3> spawnAnchors = {});

    WavePlan buildWave(int wave, GridPosition corePosition,
                       std::size_t firstAnchorIndex = 0);

  private:
    void append(EnemyType type, int count, GridPosition corePosition, int groupSize);

    std::vector<EnemySpawn> spawnBuffer_;
    std::vector<Vec3> spawnAnchors_;
    std::size_t firstAnchorIndex_{};
    std::array<WaveDefinition, GameBalance::WaveCount> definitions_;
};

[[nodiscard]] int enemyBudgetCost(EnemyType type);
[[nodiscard]] std::size_t leastVisibleSpawnAnchor(std::span<const Vec3> anchors,
                                                  Vec3 viewerPosition,
                                                  Vec3 viewDirection);

} // namespace ian
