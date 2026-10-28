#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "economy/CrystalMineSystem.hpp"

#include <algorithm>
#include <limits>

void runCrystalMineSystemTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "mine fixture creates core");
    require(buildings.upgrade(core->building.id, 0, 0, 50).valid(),
            "mine fixture reaches required core level");
    const auto mine = buildings.place(ian::BuildingType::CrystalMine, {4, 0}, 0, 20, 20, 5);
    require(mine.has_value(), "mine fixture creates crystals mine");

    ian::CrystalMineSystem mines;
    mines.syncBuildings(buildings.buildings());
    require(mines.mines().size() == 1, "mine runtime follows building");
    requireNear(60.0 * mines.productionAmount(1, ian::BuildingType::LumberMill) /
                    mines.productionInterval(ian::BuildingType::LumberMill),
                20.0, 1e-9, "level-one lumber mill produces twenty wood per minute");
    requireNear(60.0 * mines.productionAmount(1, ian::BuildingType::Quarry) /
                    mines.productionInterval(ian::BuildingType::Quarry),
                10.0, 1e-9, "level-one quarry produces ten stone per minute");
    requireNear(60.0 * mines.productionAmount(1, ian::BuildingType::CrystalMine) /
                    mines.productionInterval(ian::BuildingType::CrystalMine),
                4.0, 1e-9, "level-one crystal mine produces four crystals per minute");
    for (const auto type : {ian::BuildingType::LumberMill, ian::BuildingType::Quarry, ian::BuildingType::CrystalMine}) {
        const double base = mines.productionAmount(1, type) / mines.productionInterval(type, 1);
        for (int level = 2; level <= ian::MaxStructureLevel; ++level) {
            const auto tier = static_cast<std::uint8_t>(level);
            const double rate = mines.productionAmount(tier, type) / mines.productionInterval(type, tier);
            requireNear(rate, base * (1.0 + .20 * (level - 1)), 1e-9,
                "every producer level has the same absolute income increase, including batch changes");
        }
    }
    require(mines.tick(29.9).empty(), "mine waits for production interval");
    const auto firstProduction = mines.tick(0.1);
    require(firstProduction.size() == 1 && firstProduction[0].amount == 2,
            "mine produces configured crystals");
    require(mines.mines().front().stored == 0,
            "new production does not enter a local storage buffer");
    ian::CrystalMineSystem steadyProduction;
    steadyProduction.syncBuildings(buildings.buildings());
    int firstMinute = 0;
    for (int second = 0; second < 60; ++second) {
        for (const auto& produced : steadyProduction.tick(1.0))
            firstMinute += produced.amount;
    }
    require(firstMinute == 4 && steadyProduction.mines().front().stored == 0,
            "base crystal mine produces four crystals in its first minute");
    const auto continued = steadyProduction.tick(300.0);
    const int continuedAmount = continued.size() == 1 ? continued.front().amount : 0;
    require(continuedAmount == 20 &&
                steadyProduction.tick(30.0).front().amount == 2 &&
                steadyProduction.collect(mine->building.id, 2) == 0,
            "mine keeps producing without local capacity or collection");
    const auto upgradedMine = buildings.upgrade(mine->building.id, 100, 100, 100);
    require(upgradedMine.valid(), "mine fixture upgrades mine");
    mines.syncBuildings(buildings.buildings());
    const auto upgradedProduction = mines.tick(mines.productionInterval(ian::BuildingType::CrystalMine, 2));
    require(upgradedProduction.size() == 1 && upgradedProduction[0].amount == 2,
            "level-two mine gains a predictable cycle-speed bonus");
    requireNear(
        mines.productionInterval(ian::BuildingType::CrystalMine, 2),
        25.0, 1e-9,
        "producer level two earns twenty percent more");
    requireNear(
        mines.productionInterval(ian::BuildingType::CrystalMine, 3),
        225.0 / 7.0, 1e-9,
        "larger batches keep the same income increment");

    require(buildings.upgrade(core->building.id, 0, 0, 100).valid(),
            "producer fixture reaches core level three");

    const auto lumberMill = buildings.place(
        ian::BuildingType::LumberMill, {7, 0}, 0,
        40, 15, 10);
    const auto quarry = buildings.place(
        ian::BuildingType::Quarry, {10, 0}, 0,
        30, 40, 15);
    require(lumberMill.has_value() && quarry.has_value(),
            "quarry and lumber mill unlock at core level three");
    mines.syncBuildings(buildings.buildings());
    const auto resourceProduction = mines.tick(15.0);
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
                    return produced.amount > 0;
                }),
        "huge time jumps produce a bounded integer amount without local storage");
    require(!largeDeltaMines.tick(1000.0).empty(),
            "producers continue after a large time jump");
    require(largeDeltaMines.collect(mine->building.id, 4) == 0 &&
                largeDeltaMines.collect(mine->building.id, 0) == 0,
            "new production has no manually collectible stock");
    const auto savedRuntimes = mines.mines();
    ian::CrystalMineSystem restoredMines;
    restoredMines.syncBuildings(buildings.buildings());
    require(restoredMines.restoreRuntimes(savedRuntimes) &&
                restoredMines.mines().size() == savedRuntimes.size() &&
                restoredMines.mines().front().productionProgress ==
                    savedRuntimes.front().productionProgress,
            "producer progress survives a runtime restore");
    require(
        largeDeltaMines.tick(
            std::numeric_limits<double>::quiet_NaN()).empty() &&
            largeDeltaMines.tick(-1.0).empty(),
        "production rejects invalid delta times without poisoning timers");

    ian::CrystalMineSystem specialized;
    specialized.setProductionSpeedMultiplier(1.5);
    const double unchangedQuarry = specialized.productionInterval(ian::BuildingType::Quarry);
    specialized.setTypeProductionSpeedMultipliers(1.4, 1.0, 1.0);
    requireNear(specialized.productionInterval(ian::BuildingType::LumberMill), 30.0 / 7.0, 1e-9,
        "type production bonuses combine with global speed");
    requireNear(specialized.productionInterval(ian::BuildingType::Quarry), unchangedQuarry, 1e-9,
        "lumber mill card does not change quarry production");
    specialized.syncBuildings(buildings.buildings());
    const auto specializedOutput = specialized.tick(specialized.productionInterval(ian::BuildingType::LumberMill));
    require(specializedOutput.size() == 1 && specializedOutput.front().buildingType == ian::BuildingType::LumberMill,
        "specialized production changes actual cycles, not only UI");

    buildings.damage(lumberMill->building.id, 1.0);
    mines.syncBuildings(buildings.buildings());
    const auto damagedProduction = mines.tick(12.0);
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
