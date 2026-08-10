#include "TestHarness.hpp"
#include "ui/TargetHealthBarAnchor.hpp"
#include "ui/UiLabels.hpp"

#include <array>
#include <cmath>

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
        ian::BuildingType::SpikeTrap,
        ian::BuildingType::WoodStorage,
        ian::BuildingType::StoneStorage,
        ian::BuildingType::CrystalStorage,
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

    ian::BuildingInstance elevatedTurret{
        .id = {42U, 1U},
        .type = ian::BuildingType::Turret,
        .gridPosition = {4, 6},
        .baseHeight = 8.0,
    };
    const ian::Vec3 healthBarAnchor =
        ian::buildingHealthBarWorldAnchor(
            elevatedTurret);
    require(
        std::abs(
            healthBarAnchor.y -
            (elevatedTurret.baseHeight + 1.52)) <
            1e-9,
        "building health bar follows elevated base height");
    const ian::Vec3 productionAnchor =
        ian::buildingProductionVisualWorldAnchor(
            elevatedTurret);
    requireNear(
        productionAnchor.y,
        elevatedTurret.baseHeight + 1.35, 1e-9,
        "production visual follows elevated building base height");
}
