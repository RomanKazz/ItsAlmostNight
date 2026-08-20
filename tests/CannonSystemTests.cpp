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
    const ian::ResourceCost cannonBlueprintCost =
        buildings.blueprintUpgradeCost(ian::BuildingType::Cannon);
    require(buildings.upgradeBlueprint(
                ian::BuildingType::Cannon,
                cannonBlueprintCost.wood,
                cannonBlueprintCost.stone,
                cannonBlueprintCost.crystals).valid(),
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
    const ian::EntityId firstRunProjectile =
        cannons.projectiles().front().id;
    cannons.reset();
    cannons.syncBuildings(buildings.buildings());
    cannons.tick(1.0 / 60.0, buildings.buildings(), enemies);
    require(!cannons.projectiles().empty() &&
                cannons.projectiles().front().id !=
                    firstRunProjectile &&
                cannons.projectiles().front().explosionDamage == 2.85 &&
                cannons.projectiles().front().explosionRadius == 2.62,
            "cannon reset preserves ID safety and upgraded shell stats");
    bool exploded = false;
    for (int tick = 0; tick < 179 && !exploded; ++tick) {
        const auto explosions = cannons.tick(1.0 / 60.0, buildings.buildings(), enemies);
        if (!explosions.empty()) {
            exploded = true;
            require(explosions.front().hitCount == 3, "explosion damages clustered enemies");
            require(explosions.front().killedCount == 0,
                    "weaker cannon does not erase healthy enemy cluster");
        }
    }
    require(exploded, "physical cannon projectile reaches target");
    require(enemies.activeCount() == 4,
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

    const std::vector<ian::BuildingInstance> catapultBuildings{{
        .id = {900, 1},
        .type = ian::BuildingType::Catapult,
        .gridPosition = {0, 0},
        .health = 210.0,
        .maxHealth = 210.0,
    }};
    ian::EnemySystem catapultEnemies;
    constexpr std::array<ian::Vec3, 4> CatapultTargets{{
        {0.0, 0.8, -2.5},
        {-0.5, 0.8, -8.0},
        {0.0, 0.8, -8.0},
        {0.5, 0.8, -8.0},
    }};
    catapultEnemies.spawnWave(CatapultTargets);
    ian::CannonSystem catapults;
    catapults.syncBuildings(catapultBuildings);
    catapults.tick(1.0 / 60.0, catapultBuildings, catapultEnemies);
    require(catapults.projectiles().empty() &&
                catapults.cannons().front().loaded,
            "catapult keeps its ball in the cup during windup");
    for (int tick = 0; tick < 12; ++tick) {
        catapults.tick(1.0 / 60.0, catapultBuildings,
                       catapultEnemies);
    }
    require(catapults.cannons().front().pitch < -0.1 &&
                catapults.projectiles().empty(),
            "catapult arm pitches before releasing its ball");
    for (int tick = 0; tick < 20 &&
         catapults.projectiles().empty(); ++tick) {
        catapults.tick(1.0 / 60.0, catapultBuildings,
                       catapultEnemies);
    }
    require(!catapults.projectiles().empty() &&
                catapults.projectiles().front().type ==
                    ian::BuildingType::Catapult &&
                !catapults.cannons().front().loaded &&
                catapults.projectiles().front().targetPosition.z < -4.0,
            "catapult releases a ballistic ball beyond its dead zone");
}
