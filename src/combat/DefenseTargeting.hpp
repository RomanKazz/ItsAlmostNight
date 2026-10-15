#pragma once

#include "buildings/BuildingSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "combat/DefenseAiming.hpp"

#include <cmath>

namespace ian {

// The gun has no pitch pivot. Keep its direct, piercing and secondary shots
// on the same surface, with a small tolerance for rolling terrain.
inline constexpr double GunTurretMaximumSurfaceHeightDifference = 1.25;

[[nodiscard]] inline bool projectileDefenseCanTarget(
    BuildingType type, const EnemyInstance& enemy, double surfaceHeight) {
    return type != BuildingType::GunTurret ||
        (enemy.type != EnemyType::Flying &&
         std::abs(enemy.worldSurfaceHeight - surfaceHeight) <=
             GunTurretMaximumSurfaceHeightDifference);
}

[[nodiscard]] inline std::optional<EntityId> markedDefenseTarget(
    const EnemySystem& enemies, BuildingType type, Vec3 origin, double range,
    double restYaw, std::uint8_t level, double minimumRange = 0.0) {
    std::optional<EntityId> target;
    double nearest = range * range;
    for (const auto& enemy : enemies.enemies()) {
        if (!enemy.active || enemy.markRemaining <= 0 ||
            !projectileDefenseCanTarget(type, enemy, origin.y) ||
            !directionInsideDefenseArc(type, origin, enemy.position, restYaw, level)) continue;
        const double x = enemy.position.x - origin.x, z = enemy.position.z - origin.z;
        const double distance = x*x + z*z;
        if (distance < nearest && distance >= minimumRange * minimumRange) {
            nearest = distance; target = enemy.id;
        }
    }
    return target;
}

} // namespace ian
