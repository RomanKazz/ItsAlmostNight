#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "combat/CannonSystem.hpp"
#include "enemies/EnemySystem.hpp"

#include <array>

void runCannonSystemTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "cannon fixture creates core");
    buildings.upgrade(core->building.id, 0, 0, 50);
    const auto cannon = buildings.place(ian::BuildingType::Cannon, {0, -4}, 0, 40, 30, 25);
    require(cannon.has_value(), "cannon fixture creates cannon");
    require(buildings.upgrade(cannon->building.id, 20, 15, 23).valid(),
            "cannon fixture upgrades cannon");

    ian::EnemySystem enemies;
    constexpr std::array<ian::Vec3, 4> Cluster{{
        {0.0, 0.8, -6.0},
        {-0.6, 0.8, -10.0},
        {0.0, 0.8, -10.0},
        {0.6, 0.8, -10.0},
    }};
    enemies.spawnWave(Cluster);

    ian::CannonSystem cannons;
    cannons.syncBuildings(buildings.buildings());
    cannons.tick(1.0 / 60.0, buildings.buildings(), enemies);
    require(!cannons.projectiles().empty() &&
                cannons.projectiles().front().explosionDamage == 6.0 &&
                cannons.projectiles().front().explosionRadius == 3.5,
            "level-two cannon launches upgraded shell");
    bool exploded = false;
    for (int tick = 0; tick < 179 && !exploded; ++tick) {
        const auto explosions = cannons.tick(1.0 / 60.0, buildings.buildings(), enemies);
        if (!explosions.empty()) {
            exploded = true;
            require(explosions.front().hitCount == 3, "explosion damages clustered enemies");
            require(explosions.front().killedCount == 3, "explosion kills basic enemy cluster");
        }
    }
    require(exploded, "physical cannon projectile reaches target");
    require(enemies.activeCount() == 1,
            "cannon targets dense group instead of closer isolated enemy");

    ian::EnemySystem angledEnemies;
    constexpr std::array<ian::Vec3, 3> AngledCluster{{
        {5.0, 0.8, -8.0},
        {5.4, 0.8, -8.0},
        {5.0, 0.8, -8.4},
    }};
    angledEnemies.spawnWave(AngledCluster);
    ian::CannonSystem aimingCannons;
    aimingCannons.syncBuildings(buildings.buildings());
    aimingCannons.tick(1.0 / 60.0, buildings.buildings(), angledEnemies);
    aimingCannons.tick(1.0 / 60.0, buildings.buildings(), angledEnemies);
    require(aimingCannons.cannons().front().yaw < 0.0,
            "cannon turns toward positive X using raylib Y-axis convention");
    require(aimingCannons.cannons().front().pitch > 0.0,
            "cannon barrel raises for ballistic trajectory");
}
