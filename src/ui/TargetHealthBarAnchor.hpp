#pragma once

#include "buildings/BuildingSystem.hpp"

#include <raylib.h>

#include <cmath>
#include <optional>

namespace ian {

// Provider bounds include an elite's crown. Reserve a small proportional
// margin for head movement/squash without moving the bar with every pose.
[[nodiscard]] inline Vector3 enemyHealthBarWorldAnchor(BoundingBox bounds,bool elite) {
    const float motionClearance=elite ? .08F+.06F*(bounds.max.y-bounds.min.y) : 0;
    return {(bounds.min.x+bounds.max.x)*.5F,bounds.max.y+.12F+motionClearance,
        (bounds.min.z+bounds.max.z)*.5F};
}

inline constexpr float BuildingUpgradePromptWorldHeight = 0.20F;
inline constexpr float BuildingHealthBarUpgradeLift = 0.28F;

[[nodiscard]] inline double buildingHealthBarHeightOffset(
    BuildingType type) {
    switch (type) {
    case BuildingType::Core:
        // The core's low pedestal needs the same compact HUD anchor as its bar.
        return 1.45;
    case BuildingType::Turret:
        return 1.52;
    case BuildingType::TeslaTower: return 1.54;
    case BuildingType::MissileLauncher: return 1.65;
    case BuildingType::Flamethrower: return 1.14;
    case BuildingType::GunTurret:
        return 1.16;
    case BuildingType::CrystalMine:
        return 0.72;
    case BuildingType::LumberMill:
        return 0.85;
    case BuildingType::Quarry:
        return 0.56;
    case BuildingType::Cannon:
        return 1.76;
    case BuildingType::Catapult:
        return 1.82;
    case BuildingType::SlowTrap:
    case BuildingType::SpikeTrap:
        return 0.224;
    default:
        return 1.68;
    }
}

[[nodiscard]] inline Vec3 buildingHealthBarWorldAnchor(
    const BuildingInstance& building,
    std::optional<double> visualHeight = std::nullopt) {
    Vec3 anchor = buildingWorldPosition(building);
    anchor.y += (visualHeight && std::isfinite(*visualHeight) && *visualHeight > 0.0
        ? *visualHeight : buildingHealthBarHeightOffset(building.type)) + 0.18;
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
