#include "TestHarness.hpp"
#include "combat/BombSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "world/TerrainHeightfield.hpp"

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
    const int stockBeforeFreeThrow = bombs.remainingBombs();
    require(bombs.throwBomb(
                {0.0, 1.7, 0.0}, {1.0, 0.0, 0.0}, false) &&
                bombs.remainingBombs() == stockBeforeFreeThrow,
            "god-mode bomb throw never consumes stock");
    const ian::EntityId firstRunProjectile =
        bombs.projectiles().front().id;
    bombs.reset();
    require(
        bombs.throwBomb(
            {0.0, 1.7, 0.0}, {0.0, 0.0, -1.0}) &&
            bombs.projectiles().front().id != firstRunProjectile,
        "bomb reset never aliases a previous run projectile ID");
    const ian::Vec3 initialRotation = bombs.projectiles().front().rotation;
    static_cast<void>(bombs.tick(1.0 / 60.0, enemies));
    const ian::Vec3 airborneRotation = bombs.projectiles().front().rotation;
    require(std::abs(airborneRotation.x - initialRotation.x) +
                std::abs(airborneRotation.y - initialRotation.y) +
                std::abs(airborneRotation.z - initialRotation.z) > 0.01,
            "bomb spin visibly starts during first airborne frame");

    bool exploded = false;
    for (int tick = 0; tick < 180 && !exploded; ++tick) {
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

    ian::TerrainHeightfield terrain;
    ian::Vec3 steepPoint{};
    double steepestSlope = 0.0;
    for (double z = -120.0; z <= 120.0; z += 6.0) {
        for (double x = -120.0; x <= 120.0; x += 6.0) {
            const ian::Vec3 normal = terrain.getNormal(x, z);
            const double slope = std::hypot(normal.x, normal.z);
            if (slope > steepestSlope && !terrain.isDeepWater(x, z)) {
                steepestSlope = slope;
                steepPoint = {x, terrain.getHeight(x, z) + 0.28, z};
            }
        }
    }
    ian::EnemySystem emptyEnemies;
    ian::BombSystem slopeBomb;
    require(slopeBomb.throwBomb(
                {steepPoint.x, steepPoint.y + 0.05, steepPoint.z}, {}),
            "slope physics fixture throws bomb");
    for (int tick = 0; tick < 120; ++tick) {
        static_cast<void>(slopeBomb.tick(1.0 / 60.0, emptyEnemies, &terrain));
        const auto& projectile = slopeBomb.projectiles().front();
        if (!projectile.active) break;
        require(projectile.position.y + 1e-8 >=
                    terrain.getHeight(projectile.position.x, projectile.position.z) + 0.28,
                "bomb remains above procedural terrain surface");
    }
    const auto& rolled = slopeBomb.projectiles().front();
    require(std::hypot(rolled.position.x - steepPoint.x,
                       rolled.position.z - steepPoint.z) > 0.08,
            "resting bomb rolls down terrain slope");
    require(std::abs(rolled.rotation.x) + std::abs(rolled.rotation.y) +
                std::abs(rolled.rotation.z) > 0.1,
            "rolling bomb updates visible angular orientation");
}
