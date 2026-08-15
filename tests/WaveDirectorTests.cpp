#include "TestHarness.hpp"
#include "waves/WaveDirector.hpp"

#include <array>
#include <algorithm>
#include <cstddef>

void runWaveDirectorTests() {
    ian::WaveDirector director;
    constexpr std::array<int, 6> ExpectedBudgets{15, 25, 40, 55, 75, 100};

    for (int wave = 1;
         wave <= ian::WaveDirector::ConfiguredWaveCount;
         ++wave) {
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
        const int eliteCount = static_cast<int>(std::count_if(
            plan.spawns.begin(), plan.spawns.end(),
            [](const ian::EnemySpawn& spawn) {
                return spawn.eliteAffixes != 0U;
            }));
        require(
            eliteCount == plan.eliteCount &&
                (wave < 3 ? eliteCount == 0
                          : eliteCount > 0),
            "elite enemies begin on wave three and match plan metadata");
        require(
            std::none_of(
                plan.spawns.begin(), plan.spawns.end(),
                [](const ian::EnemySpawn& spawn) {
                    return spawn.type == ian::EnemyType::Boss &&
                        spawn.eliteAffixes != 0U;
                }),
            "bosses never receive elite affixes");
    }
    const auto firstComposition = director.composition(1);
    const auto secondComposition = director.composition(2);
    const auto thirdComposition = director.composition(3);
    const auto fourthComposition = director.composition(4);
    const auto fifthComposition = director.composition(5);
    require(
        firstComposition.fast == 0 &&
            secondComposition.fast > 0 &&
            secondComposition.ranged > 0 &&
            thirdComposition.heavy > 0 &&
            fourthComposition.sapper > 0 &&
            fifthComposition.flying > 0,
        "enemy roles unlock progressively across early waves");

    const auto firstBuild = director.buildWave(4, {3, -2});
    const ian::Vec3 firstPosition = firstBuild.spawns.front().position;
    const auto repeatedBuild = director.buildWave(4, {3, -2});
    requireNear(repeatedBuild.spawns.front().position.x, firstPosition.x, 1e-12,
                "wave spawn positions are deterministic");
    requireNear(repeatedBuild.spawns.front().position.z, firstPosition.z, 1e-12,
                "wave spawn depth is deterministic");
    for (const auto& spawn : repeatedBuild.spawns) {
        const double distanceFromMovedCore = std::hypot(
            spawn.position.x - 3.0,
            spawn.position.z + 2.0);
        require(
            distanceFromMovedCore >= 20.0 &&
                distanceFromMovedCore <= 30.0 + 1e-9,
            "wave spawn stays inside the safe ring around a moved core");
    }

    const auto edgeBaseWave = director.buildWave(6, {150, -140});
    for (const auto& spawn : edgeBaseWave.spawns) {
        const double distanceFromEdgeBase = std::hypot(
            spawn.position.x - 150.0,
            spawn.position.z + 140.0);
        require(
            distanceFromEdgeBase >= 20.0 &&
                distanceFromEdgeBase <= 30.0 + 1e-9,
            "map anchors cannot place enemies too far from an edge base");
    }

    const auto finalWave = director.buildWave(6, {0, 0});
    require(finalWave.spawns.size() <= 200,
            "sixth configured wave stays inside enemy pool budget");
    require(
        std::any_of(
            finalWave.spawns.begin(), finalWave.spawns.end(),
            [](const ian::EnemySpawn& spawn) {
                return spawn.type == ian::EnemyType::Ranged;
            }) &&
            std::any_of(
                finalWave.spawns.begin(),
                finalWave.spawns.end(),
                [](const ian::EnemySpawn& spawn) {
                    return spawn.type ==
                           ian::EnemyType::Sapper;
                }) &&
            std::any_of(
                finalWave.spawns.begin(),
                finalWave.spawns.end(),
                [](const ian::EnemySpawn& spawn) {
                    return spawn.type ==
                           ian::EnemyType::Flying;
                }),
        "late waves contain ranged, sapper and flying roles");
    const std::size_t sixthWaveSize =
        finalWave.spawns.size();
    const double sixthWaveHealthMultiplier =
        finalWave.spawns.front().healthMultiplier;
    const double sixthWaveDamageMultiplier =
        finalWave.spawns.front().damageMultiplier;
    const auto firstBoss = std::find_if(
        finalWave.spawns.begin(), finalWave.spawns.end(),
        [](const ian::EnemySpawn& spawn) {
            return spawn.type == ian::EnemyType::Boss;
        });
    require(firstBoss != finalWave.spawns.end(),
            "sixth wave contains first boss");
    const ian::EnemySpawn firstBossSpawn = *firstBoss;

    const auto seventhWave = director.buildWave(7, {0, 0});
    require(
        seventhWave.wave == 7 && !seventhWave.hasBoss &&
            seventhWave.spawns.size() > sixthWaveSize,
        "wave generation continues and grows after configured waves");
    require(
        seventhWave.spawns.front().healthMultiplier >
                sixthWaveHealthMultiplier &&
            seventhWave.spawns.front().damageMultiplier >
                sixthWaveDamageMultiplier,
        "enemy health and damage scale with wave number");

    const auto twelfthWave = director.buildWave(12, {0, 0});
    const auto secondBoss = std::find_if(
        twelfthWave.spawns.begin(), twelfthWave.spawns.end(),
        [](const ian::EnemySpawn& spawn) {
            return spawn.type == ian::EnemyType::Boss;
        });
    require(
        twelfthWave.hasBoss &&
            secondBoss != twelfthWave.spawns.end() &&
            secondBoss->healthMultiplier >
                firstBossSpawn.healthMultiplier &&
            secondBoss->damageMultiplier >
                firstBossSpawn.damageMultiplier,
        "an increasingly strong boss returns every six waves");

    const auto distantWave = director.buildWave(1000000, {0, 0});
    require(
        distantWave.wave == 1000000 &&
            distantWave.spawns.size() <=
                ian::WaveDirector::MaximumWaveEnemies,
        "unbounded wave numbers keep a bounded spawn queue");

    const auto groupedWave = director.buildWave(1, {0, 0});
    requireNear(groupedWave.spawns[0].position.z, -24.0, 1e-12,
                "first group uses first attack direction");
    requireNear(groupedWave.spawns[5].position.x, 24.0, 1e-12,
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
    requireNear(hiddenBuild.spawns.front().position.x, 24.0, 1e-12,
                "configured first attack direction rotates spawn groups");
}
