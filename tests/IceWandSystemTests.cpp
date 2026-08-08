#include "TestHarness.hpp"
#include "combat/IceWandSystem.hpp"

#include <array>

void runIceWandSystemTests() {
    ian::IceWandBalanceDefinition balance{
        .cooldown = 0.1,
        .directDamage = 1.0,
        .projectileSpeed = 18.0,
        .projectileRadius = 0.22,
        .maxLifetime = 2.5,
        .explosionRadius = 3.5,
        .freezeDuration = 1.4,
        .eliteFreezeMultiplier = 0.65,
        .bossSlowAmount = 0.35,
        .chargeUpDuration = 0.12,
        .areaDamageMultiplier = 0.5,
    };
    ian::IceWandSystem wand{balance};
    ian::EnemySystem enemies;
    enemies.spawnGroup(std::array<ian::EnemySpawn, 2>{{
        {ian::EnemyType::Basic, {0.0, 0.8, -2.0}},
        {ian::EnemyType::Basic, {1.0, 0.8, -2.0}},
    }});

    require(wand.requestFire({0.0, 0.8, 0.0}, {0.0, 0.0, -1.0}),
            "ice wand starts charge when cooldown is ready");
    wand.tick(0.12, enemies, nullptr, {});
    require(wand.launches().size() == 1 &&
                wand.impacts().size() == 1,
            "ice wand charge releases a visible projectile and swept impact");
    require(wand.hits().size() == 2,
            "ice wand splash reaches enemies standing near the direct target");
    require(enemies.enemies()[0].health < enemies.enemies()[0].maxHealth &&
                enemies.enemies()[1].health < enemies.enemies()[1].maxHealth,
            "ice wand applies direct and area damage");
    require(ian::enemyHasStatus(
                enemies.enemies()[0], ian::StatusEffectType::Freeze),
            "ice wand applies universal freeze status");

    ian::EnemySystem bossEnemies;
    bossEnemies.spawnGroup(std::array<ian::EnemySpawn, 1>{{
        {ian::EnemyType::Boss, {0.0, 1.2, -2.0}},
    }});
    ian::IceWandSystem bossWand{balance};
    require(bossWand.requestFire({0.0, 1.2, 0.0}, {0.0, 0.0, -1.0}),
            "boss fixture accepts a second wand charge");
    bossWand.tick(0.12, bossEnemies, nullptr, {});
    const auto& boss = bossEnemies.enemies().front();
    require(!ian::enemyHasStatus(boss, ian::StatusEffectType::Freeze) &&
                ian::enemyHasStatus(boss, ian::StatusEffectType::Slow) &&
                ian::enemyStatusEffect(
                    boss, ian::StatusEffectType::Slow).intensity == 0.35,
            "boss converts freeze into the configured slow status");
}
