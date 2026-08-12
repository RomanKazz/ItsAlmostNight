#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "economy/CrystalMineSystem.hpp"

#include <algorithm>
#include <limits>

void runCrystalMineSystemTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "mine fixture creates core");
    const auto mine = buildings.place(ian::BuildingType::CrystalMine, {4, 0}, 0, 20, 10);
    require(mine.has_value(), "mine fixture creates crystals mine");

    ian::CrystalMineSystem mines;
    mines.syncBuildings(buildings.buildings());
    require(mines.mines().size() == 1, "mine runtime follows building");
    require(mines.tick(7.9).empty(), "mine waits for production interval");
    const auto firstProduction = mines.tick(0.1);
    require(firstProduction.size() == 1 && firstProduction[0].amount == 4,
            "mine produces configured crystals");
    require(buildings.upgrade(core->building.id, 0, 0, 50).valid(),
            "mine fixture upgrades core");
    const auto upgradedMine = buildings.upgrade(mine->building.id, 10, 5, 10);
    require(upgradedMine.valid(), "mine fixture upgrades mine");
    mines.syncBuildings(buildings.buildings());
    const auto upgradedProduction = mines.tick(7.3);
    require(upgradedProduction.size() == 1 && upgradedProduction[0].amount == 5,
            "level-two mine gains output and cycle speed");

    const auto lumberMill = buildings.place(
        ian::BuildingType::LumberMill, {7, 0}, 0,
        40, 15, 10);
    const auto quarry = buildings.place(
        ian::BuildingType::Quarry, {10, 0}, 0,
        30, 40, 15);
    require(lumberMill.has_value() && quarry.has_value(),
            "autonomous resource producers unlock at core level two");
    mines.syncBuildings(buildings.buildings());
    const auto resourceProduction = mines.tick(10.0);
    const auto wood = std::find_if(
        resourceProduction.begin(), resourceProduction.end(),
        [](const ian::CrystalProduced& produced) {
            return produced.buildingType ==
                       ian::BuildingType::LumberMill &&
                   produced.amount == 3;
        });
    const auto stone = std::find_if(
        resourceProduction.begin(), resourceProduction.end(),
        [](const ian::CrystalProduced& produced) {
            return produced.buildingType ==
                       ian::BuildingType::Quarry &&
                   produced.amount == 2;
        });
    require(
        wood != resourceProduction.end() &&
            stone != resourceProduction.end(),
        "lumber mill and quarry produce configured resources");

    ian::CrystalMineSystem largeDeltaMines;
    largeDeltaMines.syncBuildings(buildings.buildings());
    const auto largeDeltaProduction = largeDeltaMines.tick(
        std::numeric_limits<double>::max());
    require(
        !largeDeltaProduction.empty() &&
            std::all_of(
                largeDeltaProduction.begin(),
                largeDeltaProduction.end(),
                [](const ian::CrystalProduced& produced) {
                    return produced.amount ==
                           std::numeric_limits<int>::max();
                }),
        "production handles huge time jumps in constant time with saturation");
    require(
        largeDeltaMines.tick(
            std::numeric_limits<double>::quiet_NaN()).empty() &&
            largeDeltaMines.tick(-1.0).empty(),
        "production rejects invalid delta times without poisoning timers");

    buildings.damage(lumberMill->building.id, 1.0);
    mines.syncBuildings(buildings.buildings());
    const auto damagedProduction = mines.tick(8.0);
    require(
        std::any_of(
            damagedProduction.begin(), damagedProduction.end(),
            [](const ian::CrystalProduced& produced) {
                return produced.buildingType ==
                       ian::BuildingType::LumberMill;
            }),
        "lightly damaged lumber mill keeps producing at reduced speed");

    buildings.damage(mine->building.id, upgradedMine.building->maxHealth);
    mines.syncBuildings(buildings.buildings());
    require(
        std::none_of(
            mines.mines().begin(), mines.mines().end(),
            [](const ian::CrystalMineRuntime& runtime) {
                return runtime.buildingType ==
                       ian::BuildingType::CrystalMine;
            }),
        "destroyed mine removes its runtime");
}
