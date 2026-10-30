#include "TestHarness.hpp"
#include "waves/WaveDirector.hpp"
#include "enemies/EnemyDifficulty.hpp"
#include "waves/NightEncounter.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>

void runWaveDirectorTests() {
    ian::WaveDirector director;
    requireNear(ian::enemyWaveHealthGrowth(1), 1.0, 1e-12,
                "opening wave health remains unchanged");
    requireNear(ian::enemyWaveHealthGrowth(8), 3.88575, 1e-12,
                "first boss wave has substantially stronger health growth");
    requireNear(ian::enemyWaveHealthGrowth(12), 6.84375, 1e-12,
                "strong builds face accelerating late-wave health");
    require(std::isfinite(ian::enemyWaveHealthGrowth(std::numeric_limits<int>::max())) &&
                ian::enemyWaveHealthGrowth(-10) == 1.0,
            "endless health growth stays finite and invalid waves normalize");
    constexpr std::array<int, 6> ExpectedBudgets{8, 17, 23, 30, 44, 100};
    constexpr std::array<double, 6> ExpectedHealth{.65, .9975, 1.3375, 1.737, 2.1875, 2.6975};
    constexpr std::array<double, 6> ExpectedDamage{.60, .77, .86, .96, 1.07, 1.17};
    constexpr std::array<int, 6> EarlyActiveStart{6, 14, 22, 32, 45, 60};
    for (int night = 1; night <= 6; ++night) {
        const auto plan = director.buildWave(night, {});
        requireNear(plan.spawns.front().healthMultiplier,
            ExpectedHealth[static_cast<std::size_t>(night - 1)], 1e-12,
            "early health follows the hand-tuned curve");
        requireNear(plan.spawns.front().damageMultiplier,
            ExpectedDamage[static_cast<std::size_t>(night - 1)], 1e-12,
            "early damage grows more gently than population");
        requireNear(ian::waveTargetActiveEnemies(night, plan.pattern, 0),
            EarlyActiveStart[static_cast<std::size_t>(night - 1)] *
                (night<=5 ? 1 : std::max(.85,ian::nightPopulationScale(plan.pattern,night))), 1e-12,
            "early active population starts at the configured target");
        requireNear(ian::waveTargetActiveEnemies(night, plan.pattern, .9),
            EarlyActiveStart[static_cast<std::size_t>(night - 1)] * 1.2 *
                (night<=5 ? 1 : std::max(.85,ian::nightPopulationScale(plan.pattern,night))), 1e-12,
            "early active population rises modestly through the night");
    }
    require(ExpectedHealth[5] / ExpectedHealth[4] < 1.26 &&
            EarlyActiveStart[5] > EarlyActiveStart[4] &&
            director.buildWave(6, {}).spawns.front().damageMultiplier /
                ExpectedDamage[4] < 1.10,
        "night six continues the fifth-night HP, damage and population curves");
    require(director.buildWave(7, {}).spawns.front().healthMultiplier / ExpectedHealth[5] < 1.27,
        "the first procedural night continues the health curve without a second jump");
    for(int wave=2;wave<=16;++wave) {
        require(ian::waveTargetActiveEnemies(wave,ian::NightPattern::Horde,0) >
                ian::waveTargetActiveEnemies(wave-1,ian::NightPattern::Horde,0),
            "each successive night targets a larger crowd");
        require(ian::enemyWaveSpeedGrowth(wave)>ian::enemyWaveSpeedGrowth(wave-1),
            "night progression gradually increases movement speed");
    }
    requireNear(ian::enemyWaveSpeedGrowth(16),1.3,1e-12,"night movement bonus is capped at thirty percent");

    double previousHealth = 0.0;
    double previousDamage = 0.0;
    for (int wave = 1; wave <= 24; ++wave) {
        const auto difficulty = ian::waveDifficulty(wave);
        if (wave <= 6) {
            require(difficulty.extraActiveEnemies == 0U &&
                    difficulty.spawnRateMultiplier >= .95,
                "onboarding nights use controlled pressure");
        } else if (wave >= 12) {
            require(
                difficulty.regularHealthScale == 1.0 &&
                    difficulty.regularDamageScale >= 0.95 &&
                    difficulty.regularDamageScale < 1.0 &&
                    difficulty.extraActiveEnemies > 0U &&
                    difficulty.spawnRateMultiplier > 1.0,
                "mature nights retain their original formula");
        }
        const auto plan = director.buildWave(wave, {0, 0});
        const auto& regular = plan.spawns.front();
        require(
            regular.healthMultiplier > previousHealth &&
                regular.damageMultiplier >= previousDamage,
            "individual enemy stats still increase smoothly each night");
        previousHealth = regular.healthMultiplier;
        previousDamage = regular.damageMultiplier;
        for (const auto& spawn : plan.spawns) {
            if (spawn.type != ian::EnemyType::Boss) continue;
            const double cycle = static_cast<double>(wave / ian::WaveDirector::BossWaveInterval - 1);
            requireNear(
                spawn.healthMultiplier,
                0.85 * 0.92 * ian::enemyWaveHealthGrowth(wave) *
                    (1.0 + 0.35 * cycle),
                1e-12, "boss health uses accelerating growth and boss-cycle bonus");
            requireNear(
                spawn.damageMultiplier,
                0.85 * 0.92 * (1.0 + std::min(10, wave - 1) * 0.06) *
                    (1.0 + 0.02 * cycle),
                1e-12, "later bosses add only a small damage bonus");
        }
    }
    auto legacyWaves = ian::GameBalance::defaults().waves;
    for (auto& wave : legacyWaves) wave.sapper = 12;
    legacyWaves.back().boss = true;
    ian::WaveDirector legacyDirector(legacyWaves);
    require(!legacyDirector.composition(6).boss && legacyDirector.composition(8).boss,
        "legacy sixth-night boss flags obey the new eighth-night schedule");
    for (const int wave : {1, 4, 6, 12, 1000000}) {
        require(legacyDirector.composition(wave).sapper == 0,
                "legacy wave data cannot restore the retired sapper");
        require(legacyDirector.forecast(wave, 42).enemies[
                    static_cast<std::size_t>(ian::EnemyType::Sapper)] == 0,
                "retired enemy is absent from the night forecast");
        for (int act = 0; act < 3; ++act) {
            const auto plan = legacyDirector.buildWave(wave, {0, 0}, 0,
                {.runSeed = 42, .act = act});
            require(std::none_of(plan.spawns.begin(), plan.spawns.end(),
                        [](const auto& spawn) { return spawn.type == ian::EnemyType::Sapper; }),
                    "all night acts and endless waves exclude the retired sapper");
        }
    }
    auto ungatedWaves = ian::GameBalance::defaults().waves;
    for (auto& wave : ungatedWaves) {
        wave.fast = wave.heavy = wave.ranged = wave.flying = 1;
        wave.splitter = wave.bee = wave.airTank = 1;
    }
    ian::WaveDirector gatedDirector(ungatedWaves);
    for (int night = 1; night <= 8; ++night) {
        const auto roster = gatedDirector.composition(night);
        require((night >= 2 || roster.fast == 0) &&
                (night >= 3 || roster.heavy == 0) &&
                (night >= 4 || roster.ranged == 0) &&
                (night >= 5 || roster.splitter == 0) &&
                (night >= 6 || roster.flying == 0) &&
                (night >= 7 || roster.bee == 0) && roster.airTank == 0,
            "older wave data cannot unlock enemy roles before their first night");
    }

    const auto deepDifficulty = ian::waveDifficulty(1000000);
    require(
        deepDifficulty.extraActiveEnemies <= 18U &&
            deepDifficulty.spawnRateMultiplier <= 1.64 + 1e-12,
        "late horde pressure remains bounded for endless runs");

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
        require(bossCount == 0 && !plan.hasBoss, "six configured opening rosters contain no boss");
        require(plan.groupSize > 0 && plan.groupInterval > 0.0,
                "wave exposes valid group schedule");
        const int eliteCount = static_cast<int>(std::count_if(
            plan.spawns.begin(), plan.spawns.end(),
            [](const ian::EnemySpawn& spawn) {
                return spawn.eliteAffixes != 0U;
            }));
        require(
            eliteCount == plan.eliteCount &&
                (wave < 5 ? eliteCount == 0
                          : eliteCount > 0),
            "elite enemies begin on night five and match plan metadata");
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
    require(firstComposition.basic > 0 && firstComposition.fast == 0 &&
                firstComposition.heavy == 0 && firstComposition.ranged == 0 &&
                firstComposition.splitter == 0 && firstComposition.flying == 0 &&
                secondComposition.fast > 0 && secondComposition.heavy == 0 &&
                thirdComposition.heavy > 0 && thirdComposition.ranged == 0 &&
                fourthComposition.ranged > 0 && fourthComposition.splitter == 0 &&
                fifthComposition.splitter > 0 && fifthComposition.flying == 0 &&
                director.composition(6).flying > 0 && director.composition(6).bee == 0 &&
                director.composition(7).bee > 0 && director.composition(7).airTank == 0 &&
                director.composition(8).boss && director.composition(8).airTank == 0 &&
                director.composition(9).airTank > 0,
        "enemy roles unlock in the specified order through night nine");
    require(
        firstComposition.groupSize == 2 &&
            firstComposition.groupInterval == 2.0,
        "first timed night uses a manageable spawn cadence");

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
    const auto lateForecast = director.forecast(8, 0);
    require(lateForecast.enemies[static_cast<std::size_t>(ian::EnemyType::Ranged)] > 0 &&
                lateForecast.enemies[static_cast<std::size_t>(ian::EnemyType::Sapper)] == 0 &&
                lateForecast.enemies[static_cast<std::size_t>(ian::EnemyType::Flying)] > 0,
            "boss-night acts retain ranged and flying roles without the retired sapper");
    const int sixthWaveBudget = finalWave.regularBudget;
    const double sixthWaveHealthMultiplier =
        finalWave.spawns.front().healthMultiplier;
    const double sixthWaveDamageMultiplier =
        finalWave.spawns.front().damageMultiplier;
    const auto firstBossWave = director.buildWave(8, {0, 0});
    const auto firstBoss = std::find_if(
        firstBossWave.spawns.begin(), firstBossWave.spawns.end(),
        [](const ian::EnemySpawn& spawn) {
            return spawn.type == ian::EnemyType::Boss;
        });
    require(firstBoss != firstBossWave.spawns.end(),
            "eighth wave contains first boss");
    const ian::EnemySpawn firstBossSpawn = *firstBoss;

    const auto seventhWave = director.buildWave(7, {0, 0});
    require(
        seventhWave.wave == 7 && !seventhWave.hasBoss &&
            seventhWave.regularBudget > sixthWaveBudget,
        "wave generation continues and grows after configured waves");
    require(
        seventhWave.spawns.front().healthMultiplier >
                sixthWaveHealthMultiplier &&
            seventhWave.spawns.front().damageMultiplier >
                sixthWaveDamageMultiplier,
        "enemy health and damage scale with wave number");

    const auto secondBossWave = director.buildWave(16, {0, 0});
    const auto secondBoss = std::find_if(
        secondBossWave.spawns.begin(), secondBossWave.spawns.end(),
        [](const ian::EnemySpawn& spawn) {
            return spawn.type == ian::EnemyType::Boss;
        });
    require(
        secondBossWave.hasBoss &&
            secondBoss != secondBossWave.spawns.end() &&
            secondBoss->healthMultiplier >
                firstBossSpawn.healthMultiplier &&
            secondBoss->damageMultiplier >
                firstBossSpawn.damageMultiplier,
        "an increasingly strong boss returns every eight waves");

    for (int wave = 1; wave <= 25; ++wave) {
        const auto plan = director.buildWave(wave, {});
        const auto count = std::count_if(plan.spawns.begin(), plan.spawns.end(),
            [](const auto& spawn) { return spawn.type == ian::EnemyType::Boss; });
        const bool expected = wave == 8 || wave == 16 || wave == 24;
        const auto forecast = director.forecast(wave, 123);
        require(plan.hasBoss == expected && count == (expected ? 1 : 0) &&
            forecast.enemies[static_cast<std::size_t>(ian::EnemyType::Boss)] == count &&
            (forecast.pattern == ian::NightPattern::RoyalCourt) == expected,
            "actual boss spawns and forecasts agree on nights 8, 16 and 24 only");
    }

    const auto distantWave = director.buildWave(1000000, {0, 0});
    require(
        distantWave.wave == 1000000 &&
            distantWave.spawns.size() <=
                ian::WaveDirector::MaximumWaveEnemies &&
            distantWave.groupSize <= 10 &&
            distantWave.groupInterval >= 0.8,
        "unbounded wave numbers keep a bounded spawn queue");

    const auto groupedWave = director.buildWave(1, {0, 0});
    requireNear(groupedWave.spawns.front().healthMultiplier, .65, 1e-12,
                "first night has gentle enemy health");
    requireNear(groupedWave.spawns.front().damageMultiplier, .60, 1e-12,
                "first night forgives a few leaks");
    requireNear(groupedWave.spawns[0].position.z, -24.0, 1e-12,
                "first group uses first attack direction");
    require(std::ranges::all_of(groupedWave.spawns,
                [](const ian::EnemySpawn& spawn) { return spawn.position.z < -20.0; }),
            "first wave keeps its original single attack front");

    const auto twoFrontWave = director.buildWave(2, {0, 0});
    require(twoFrontWave.spawns[0].position.z < -20.0 &&
                twoFrontWave.spawns[3].position.x > 20.0 &&
                twoFrontWave.spawns[6].position.z < -20.0,
            "second wave keeps its original two attack fronts");
    require(std::count_if(twoFrontWave.spawns.begin(), twoFrontWave.spawns.end(),
                [](const auto& spawn) { return spawn.type == ian::EnemyType::Fast; }) > 0,
            "second night retains unlocked fast attackers in coherent groups");

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
