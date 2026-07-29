#include "TestHarness.hpp"
#include "ui/UiLabels.hpp"

#include <array>

void runUiLabelsTests() {
    constexpr std::array BuildingTypes{
        ian::BuildingType::Core,
        ian::BuildingType::Wall,
        ian::BuildingType::Turret,
        ian::BuildingType::GoldMine,
        ian::BuildingType::Cannon,
        ian::BuildingType::SlowTrap,
        ian::BuildingType::Gate,
        ian::BuildingType::LumberMill,
        ian::BuildingType::Quarry,
    };
    for (const ian::BuildingType type : BuildingTypes) {
        require(
            !ian::buildingDisplayName(type).empty(),
            "every building type has a shared UI label");
    }
    require(
        ian::buildingDisplayName(
            ian::BuildingType::GoldMine) ==
            "Crystal Mine",
        "gold mine label follows crystal currency");
}
