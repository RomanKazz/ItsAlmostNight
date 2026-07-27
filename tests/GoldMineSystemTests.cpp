#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "economy/GoldMineSystem.hpp"

void runGoldMineSystemTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "mine fixture creates core");
    const auto mine = buildings.place(ian::BuildingType::GoldMine, {4, 0}, 0, 20, 10);
    require(mine.has_value(), "mine fixture creates gold mine");

    ian::GoldMineSystem mines;
    mines.syncBuildings(buildings.buildings());
    require(mines.mines().size() == 1, "mine runtime follows building");
    require(mines.tick(4.9).empty(), "mine waits for production interval");
    const auto firstProduction = mines.tick(0.1);
    require(firstProduction.size() == 1 && firstProduction[0].amount == 5,
            "mine produces configured gold");
    require(buildings.upgrade(core->building.id, 0, 0, 50).valid(),
            "mine fixture upgrades core");
    const auto upgradedMine = buildings.upgrade(mine->building.id, 10, 5, 10);
    require(upgradedMine.valid(), "mine fixture upgrades mine");
    mines.syncBuildings(buildings.buildings());
    const auto upgradedProduction = mines.tick(5.0);
    require(upgradedProduction.size() == 1 && upgradedProduction[0].amount == 10,
            "level-two mine doubles gold production");

    buildings.damage(mine->building.id, upgradedMine.building->maxHealth);
    mines.syncBuildings(buildings.buildings());
    require(mines.mines().empty(), "destroyed mine removes runtime");
}
