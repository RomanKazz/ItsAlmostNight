#pragma once

#include "buildings/BuildingSystem.hpp"
#include "enemies/EnemySystem.hpp"

#include <cmath>

namespace ian {

// The gun has no pitch pivot. Keep its direct, piercing and secondary shots
// on the same surface, with a small tolerance for rolling terrain.
inline constexpr double GunTurretMaximumSurfaceHeightDifference = 1.25;

[[nodiscard]] inline bool hitscanDefenseCanTarget(
    BuildingType type, const EnemyInstance& enemy, double surfaceHeight) {
    return type != BuildingType::GunTurret ||
        (enemy.type != EnemyType::Flying &&
         std::abs(enemy.worldSurfaceHeight - surfaceHeight) <=
             GunTurretMaximumSurfaceHeightDifference);
}

} // namespace ian
