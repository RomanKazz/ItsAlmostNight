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
    case BuildingType::GoldMine:
        return 1.44;
    case BuildingType::LumberMill:
        return 1.70;
    case BuildingType::Quarry:
        return 1.12;
    case BuildingType::Cannon:
        return 1.76;
    case BuildingType::SlowTrap:
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

} // namespace ian
