#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "combat/TowerSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "navigation/FlowField.hpp"

#include <array>

void runTowerSystemTests() {
    ian::BuildingSystem buildings;
    require(buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0).has_value(),
            "tower fixture creates core");
    const auto turret = buildings.place(ian::BuildingType::Turret, {0, -4}, 0, 25, 15);
    require(turret.has_value(), "tower fixture creates turret");
    const auto core = buildings.core();
    require(core.has_value(), "tower fixture finds core");
    require(buildings.upgrade(core->id, 0, 0, 50).valid(),
            "tower fixture upgrades core");
    require(buildings.upgrade(turret->building.id, 13, 8, 10).valid(),
            "tower fixture upgrades turret");

    ian::FlowField flow;
    flow.rebuild({0, 0}, buildings.buildings());
    ian::EnemySystem enemies;
    constexpr std::array<ian::Vec3, 1> Spawn{{{0.0, 0.8, -8.0}}};
    enemies.spawnWave(Spawn);
    enemies.tick(1.0 / 60.0, buildings.buildings(), flow);

    ian::TowerSystem towers;
    towers.syncBuildings(buildings.buildings());
    require(towers.towers().size() == 1, "tower runtime follows turret building");

    int shotCount = 0;
    bool killed = false;
    for (int tick = 0; tick < 120 && !killed; ++tick) {
        const auto shots = towers.tick(1.0 / 60.0, buildings.buildings(), enemies);
        shotCount += static_cast<int>(shots.size());
        for (const auto& shot : shots) {
            killed = killed || shot.killed;
        }
    }
    require(shotCount == 2, "level-two turret deals upgraded damage");
    require(killed, "turret can kill target");
    require(enemies.activeCount() == 0, "tower damage updates enemy system");
}
