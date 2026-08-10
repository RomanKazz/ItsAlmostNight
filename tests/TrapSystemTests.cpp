#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "navigation/FlowField.hpp"

#include <array>
#include <cmath>

void runTrapSystemTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "trap fixture creates core");
    buildings.upgrade(core->building.id, 0, 0, 50);
    const auto trap =
        buildings.place(ian::BuildingType::SlowTrap, {0, -4}, 0, 15, 20, 10);
    require(trap.has_value(), "trap fixture creates slow trap");
    require(buildings.upgrade(trap->building.id, 8, 10, 15).valid(),
            "trap fixture upgrades slow trap");

    ian::EnemySystem enemies;
    constexpr std::array<ian::Vec3, 1> Spawn{{{0.0, 0.8, -4.5}}};
    enemies.spawnWave(Spawn);

    ian::TrapSystem traps;
    traps.syncBuildings(buildings.buildings());
    const auto activation = traps.tick(1.0 / 60.0, buildings.buildings(), enemies);
    require(activation.size() == 1 && activation[0].affectedCount == 1,
            "trap activates for enemy in area");
    requireNear(activation[0].wearDamage, 10.0 / 1.12, 1e-12,
                "upgraded trap wears more slowly");
    requireNear(enemies.enemies()[0].movementMultiplier, 0.42, 1e-12,
                "upgraded trap applies stronger slow");
    ian::FlowField flow;
    flow.rebuild({0, 0}, buildings.buildings());
    const ian::Vec3 startPosition = enemies.enemies()[0].position;
    enemies.tick(1.0, buildings.buildings(), flow);
    const double deltaX = enemies.enemies()[0].position.x - startPosition.x;
    const double deltaZ = enemies.enemies()[0].position.z - startPosition.z;
    const double movement = std::sqrt((deltaX * deltaX) + (deltaZ * deltaZ));
    require(movement > 0.8 && movement < 1.0, "upgraded slow reduces enemy movement speed");
    require(traps.tick(1.0, buildings.buildings(), enemies).empty(),
            "trap cooldown prevents immediate activation");

    ian::BuildingSystem spikeBuildings;
    const auto spikeCore = spikeBuildings.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(spikeCore.has_value(),
            "spike trap fixture creates core");
    spikeBuildings.upgrade(spikeCore->building.id, 0, 0, 50);
    const auto spike = spikeBuildings.place(
        ian::BuildingType::SpikeTrap, {0, -4}, 0,
        20, 25, 15);
    require(spike.has_value(),
            "spike trap occupies one grid cell");
    ian::EnemySystem spikeEnemies;
    constexpr std::array<ian::Vec3, 1> SpikeSpawn{{
        {0.0, 0.8, -4.0},
    }};
    spikeEnemies.spawnWave(SpikeSpawn);
    ian::TrapSystem spikeTraps;
    spikeTraps.syncBuildings(spikeBuildings.buildings());
    const auto spikeActivation = spikeTraps.tick(
        1.0 / 60.0, spikeBuildings.buildings(), spikeEnemies);
    require(
        spikeActivation.size() == 1 &&
            spikeActivation.front().affectedCount == 1 &&
            spikeTraps.hits().size() == 1 &&
            spikeTraps.hits().front().result.killed,
        "spike trap damages and kills an enemy on its cell");
    requireNear(
        spikeActivation.front().wearDamage,
        ian::TrapSystem::WearDamage, 1e-12,
        "spike trap wears on activation");
    require(
        spikeTraps.traps().front().activationRemaining > 1.1,
        "spike trap exposes its trigger animation state");
    require(
        spikeTraps.tick(
            0.2, spikeBuildings.buildings(), spikeEnemies)
            .empty(),
        "spike trap cooldown prevents retriggering");
}
