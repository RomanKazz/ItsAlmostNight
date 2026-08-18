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
        ian::BuildingType::CrystalMine,
        ian::BuildingType::Cannon,
        ian::BuildingType::SlowTrap,
        ian::BuildingType::Gate,
        ian::BuildingType::LumberMill,
        ian::BuildingType::Quarry,
        ian::BuildingType::SpikeTrap,
        ian::BuildingType::WoodStorage,
        ian::BuildingType::StoneStorage,
        ian::BuildingType::CrystalStorage,
        ian::BuildingType::GunTurret,
    };
    for (const ian::BuildingType type : BuildingTypes) {
        require(
            !ian::buildingDisplayName(type).empty(),
            "every building type has a shared UI label");
    }
    require(
        ian::buildingDisplayName(
            ian::BuildingType::CrystalMine) ==
            "Crystal Mine",
        "crystals mine label follows crystal currency");
    const std::string introReward =
        ian::introGatherRewardMessage(60.0);
    require(
        introReward ==
            "Gathering objective complete: +60 Insight" &&
            introReward.find("Skill point") == std::string::npos,
        "intro gathering completion reports Insight, not a skill point");

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

    ian::BuildingInstance compactQuarry{
        .id = {43U, 1U},
        .type = ian::BuildingType::Quarry,
        .gridPosition = {2, 3},
        .baseHeight = 4.0,
    };
    requireNear(
        ian::buildingHealthBarWorldAnchor(compactQuarry).y,
        4.56, 1e-9,
        "one-cell quarry health bar follows uniform half scale");
    requireNear(
        ian::buildingProductionVisualWorldAnchor(compactQuarry).y,
        4.675, 1e-9,
        "one-cell producer output follows uniform half scale");
}
