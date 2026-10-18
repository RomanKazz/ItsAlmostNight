#pragma once

#include "buildings/BuildingSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"
#include "waves/NightEncounter.hpp"

#include <span>
#include <vector>

namespace ian {

struct WavePlan {
    int wave;
    int regularBudget;
    bool hasBoss;
    int groupSize;
    double groupInterval;
    int eliteCount{};
    std::span<const EnemySpawn> spawns;
    NightPattern pattern{NightPattern::Horde};
};

class WaveDirector {
  public:
    static constexpr int ConfiguredWaveCount = 6;
    // Boss cadence is independent of the six configured roster templates.
    static constexpr int BossWaveInterval = 8;
    static constexpr std::size_t MaximumWaveEnemies = 512;

    explicit WaveDirector(
        std::array<WaveDefinition, GameBalance::WaveCount> definitions =
            GameBalance::defaults().waves,
        std::vector<Vec3> spawnAnchors = {});

    WavePlan buildWave(int wave, GridPosition corePosition,
                       std::size_t firstAnchorIndex = 0, WaveContext context = {});
    [[nodiscard]] WaveDefinition composition(int wave) const;
    [[nodiscard]] NightForecast forecast(int wave, std::uint32_t runSeed) const;

  private:
    void append(
        EnemyType type, int count,
        GridPosition corePosition, int groupSize,
        double healthMultiplier,
        double damageMultiplier);
    int applyEliteUpgrades(int wave);

    std::vector<EnemySpawn> spawnBuffer_;
    std::vector<EnemySpawn> scratchBuffer_;
    std::vector<Vec3> spawnAnchors_;
    std::size_t firstAnchorIndex_{};
    std::size_t attackFrontCount_{1};
    std::size_t spawnLimit_{MaximumWaveEnemies};
    std::array<WaveDefinition, GameBalance::WaveCount> definitions_;
};

[[nodiscard]] int enemyBudgetCost(EnemyType type);
[[nodiscard]] std::size_t leastVisibleSpawnAnchor(std::span<const Vec3> anchors,
                                                  Vec3 viewerPosition,
                                                  Vec3 viewDirection);

} // namespace ian
