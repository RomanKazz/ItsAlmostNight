#include "TestHarness.hpp"
#include "combat/BombSystem.hpp"
#include "enemies/EnemySystem.hpp"

#include <array>
#include <cmath>

void runBombSystemTests() {
    ian::EnemySystem enemies;
    constexpr std::array<ian::EnemySpawn, 2> Spawn{{
        {ian::EnemyType::Basic, {0.0, 0.8, -7.0}},
        {ian::EnemyType::Heavy, {0.8, 1.0, -7.0}},
    }};
    enemies.spawnWave(Spawn);

    ian::BombSystem bombs;
    require(bombs.throwBomb({0.0, 1.7, 0.0}, {0.0, 0.0, -1.0}),
            "bomb throw consumes available bomb");
    require(bombs.remainingBombs() == 2, "bomb stock decreases after throw");

    bool exploded = false;
    for (int tick = 0; tick < 120 && !exploded; ++tick) {
        const auto explosions = bombs.tick(1.0 / 60.0, enemies);
        if (!explosions.empty()) {
            exploded = true;
            require(explosions.front().hitCount == 2, "bomb damages enemies in radius");
            require(explosions.front().killedCount == 1, "bomb kills basic but not heavy enemy");
        }
    }
    require(exploded, "bomb fuse triggers explosion");
    require(enemies.activeCount() == 1, "heavy enemy survives bomb");
    const auto knockback = enemies.enemies()[1].knockbackVelocity;
    require(std::abs(knockback.x) + std::abs(knockback.z) > 0.0,
            "surviving enemy receives outward knockback");
}
