#include "TestHarness.hpp"
#include "waves/WaveDirector.hpp"

#include <array>
#include <cstddef>

void runWaveDirectorTests() {
    ian::WaveDirector director;
    constexpr std::array<int, 6> ExpectedBudgets{15, 25, 40, 55, 75, 100};

    for (int wave = 1; wave <= ian::WaveDirector::WaveCount; ++wave) {
        const auto plan = director.buildWave(wave, {0, 0});
        int regularBudget = 0;
        int bossCount = 0;
        for (const auto& spawn : plan.spawns) {
            if (spawn.type == ian::EnemyType::Boss) {
                ++bossCount;
            } else {
                regularBudget += ian::enemyBudgetCost(spawn.type);
            }
        }
        require(regularBudget == ExpectedBudgets[static_cast<std::size_t>(wave - 1)],
                "wave composition matches configured budget");
        require(bossCount == (wave == 6 ? 1 : 0), "boss appears only in sixth wave");
        require(plan.groupSize > 0 && plan.groupInterval > 0.0,
                "wave exposes valid group schedule");
    }

    const auto firstBuild = director.buildWave(4, {3, -2});
    const ian::Vec3 firstPosition = firstBuild.spawns.front().position;
    const auto repeatedBuild = director.buildWave(4, {3, -2});
    requireNear(repeatedBuild.spawns.front().position.x, firstPosition.x, 1e-12,
                "wave spawn positions are deterministic");
    requireNear(repeatedBuild.spawns.front().position.z, firstPosition.z, 1e-12,
                "wave spawn depth is deterministic");

    const auto finalWave = director.buildWave(6, {0, 0});
    require(finalWave.spawns.size() <= 200, "final wave stays inside enemy pool budget");

    const auto groupedWave = director.buildWave(1, {0, 0});
    requireNear(groupedWave.spawns[0].position.z, -20.0, 1e-12,
                "first group uses first attack direction");
    requireNear(groupedWave.spawns[5].position.x, 20.0, 1e-12,
                "second group rotates attack direction");

    constexpr std::array<ian::Vec3, 3> Anchors{{
        {0.0, 0.0, -20.0},
        {20.0, 0.0, 0.0},
        {-20.0, 0.0, 0.0},
    }};
    const std::size_t hiddenAnchor = ian::leastVisibleSpawnAnchor(
        Anchors, {0.0, 1.7, 6.0}, {0.0, 0.0, -1.0});
    require(hiddenAnchor == 1, "director avoids attack zone directly in view");
    const auto hiddenBuild = director.buildWave(1, {0, 0}, hiddenAnchor);
    requireNear(hiddenBuild.spawns.front().position.x, 20.0, 1e-12,
                "configured first attack direction rotates spawn groups");
}
