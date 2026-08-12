#pragma once

#include "buildings/BuildingSystem.hpp"

namespace ian {

[[nodiscard]] inline double buildingHealthBarHeightOffset(
    BuildingType type) {
    switch (type) {
    case BuildingType::Core:
        return 2.08;
    case BuildingType::Turret:
        return 1.52;
    case BuildingType::CrystalMine:
        return 0.72;
    case BuildingType::LumberMill:
        return 0.85;
    case BuildingType::Quarry:
        return 0.56;
    case BuildingType::Cannon:
        return 1.76;
    case BuildingType::SlowTrap:
    case BuildingType::SpikeTrap:
        return 0.224;
    default:
        return 1.68;
    }
}

[[nodiscard]] inline Vec3 buildingHealthBarWorldAnchor(
    const BuildingInstance& building) {
    Vec3 anchor = buildingWorldPosition(building);
    anchor.y += buildingHealthBarHeightOffset(
        building.type);
    return anchor;
}

[[nodiscard]] inline Vec3 buildingProductionVisualWorldAnchor(
    const BuildingInstance& building) {
    Vec3 anchor = buildingWorldPosition(building);
    const bool compactProducer =
        building.type == BuildingType::CrystalMine ||
        building.type == BuildingType::LumberMill ||
        building.type == BuildingType::Quarry;
    anchor.y += compactProducer ? 0.675 : 1.35;
    return anchor;
}

} // namespace ian
