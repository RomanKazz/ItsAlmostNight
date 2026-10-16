#pragma once
#include "buildings/BuildingOrientation.hpp"
#include "combat/AttackStats.hpp"
#include "enemies/EnemyCollision.hpp"
#include <cmath>

namespace ian {
inline constexpr double WindBlowerPulseSeconds = .7;
inline constexpr double WindBlowerMuzzleY = .960;
inline constexpr double WindBlowerMuzzleZ = -.479;
[[nodiscard]] inline Vec3 windBlowerMuzzle(Vec3 base, double yaw) {
    return {base.x + std::sin(yaw) * WindBlowerMuzzleZ,
        base.y + WindBlowerMuzzleY, base.z + std::cos(yaw) * WindBlowerMuzzleZ};
}
[[nodiscard]] inline bool windBlowerCanTarget(const EnemyInstance& enemy, double surface) {
    return enemy.active && enemy.type != EnemyType::Boss && enemy.type != EnemyType::Flying &&
        std::abs(enemy.worldSurfaceHeight - surface) <= 1.25;
}
[[nodiscard]] inline double windBlowerRange(const FinalAttackStats& stats) {
    return stats.range * stats.size;
}
// A broad horizontal gust, tested against real collision cylinders. The
// visual stream and gameplay use the same 60-degree cone and end distance.
[[nodiscard]] inline bool windBlowerTouches(const EnemyInstance& enemy, Vec3 base,
    double yaw, double range) {
    if (!windBlowerCanTarget(enemy, base.y)) return false;
    const auto cylinder = enemyCollisionCylinder(enemy);
    const double dx = cylinder.center.x - base.x, dz = cylinder.center.z - base.z;
    const double forward = -dx * std::sin(yaw) - dz * std::cos(yaw);
    const double side = std::abs(dx * std::cos(yaw) - dz * std::sin(yaw));
    return forward > .15 && dx*dx + dz*dz <= (range+cylinder.radius)*(range+cylinder.radius) &&
        side <= .25 + forward * .57735026919 + cylinder.radius;
}
}
