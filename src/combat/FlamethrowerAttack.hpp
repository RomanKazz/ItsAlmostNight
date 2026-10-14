#pragma once

#include "buildings/TowerRig.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "combat/AttackCollision.hpp"
#include "combat/AttackStats.hpp"

namespace ian {

inline constexpr double FlamethrowerHalfAngle = 16.0 * PiRadians / 180.0;
inline constexpr double FlamethrowerMinimumPitch = -10.0 * PiRadians / 180.0;
// The hand-edited tank intersects its mount above 21 degrees.
inline constexpr double FlamethrowerMaximumPitch = 21.0 * PiRadians / 180.0;
inline constexpr double FlamethrowerBurnInterval = 0.5;
inline constexpr double FlamethrowerJetSpeed = 7.6;
inline constexpr double FlamethrowerFadeSeconds = 0.16;

[[nodiscard]] inline double flamethrowerRange(const AttackStats& stats) {
    return stats.range * stats.size;
}

[[nodiscard]] inline double flamethrowerStreamLength(const AttackStats& stats) {
    return std::max(0.05, flamethrowerRange(stats) - 0.59);
}

[[nodiscard]] inline double flamethrowerTailSeconds(const AttackStats& stats) {
    return flamethrowerStreamLength(stats) / FlamethrowerJetSpeed + FlamethrowerFadeSeconds;
}

// A tapered jet touches the existing finite enemy capsule. Pick the point
// on its vertical axis closest to the stream, then include its radius.
// The far cap also follows the actual jet length, never an infinite cone.
[[nodiscard]] inline bool flamethrowerTouchesEnemy(
    const EnemyInstance& enemy, Vec3 muzzle, Vec3 direction, double length) {
    const auto capsule = enemyCapsule(enemy.type);
    const auto center = attackTargetPosition(enemy);
    const Vec3 delta = geometry::subtract(center, muzzle);
    const double horizontal = direction.x * direction.x + direction.z * direction.z;
    double along = horizontal > 1e-8
        ? (delta.x * direction.x + delta.z * direction.z) / horizontal
        : geometry::dot(delta, direction);
    along = std::clamp(along, 0.0, length);
    Vec3 nearest = center;
    nearest.y = std::clamp(muzzle.y + direction.y * along,
        center.y - capsule.segmentHalfHeight, center.y + capsule.segmentHalfHeight);
    const auto offset = geometry::subtract(nearest, muzzle);
    along = geometry::dot(offset, direction);
    if (along < -capsule.radius || along > length + capsule.radius) return false;
    const double radial = std::sqrt(std::max(0.0,
        geometry::dot(offset, offset) - along * along));
    if (along > length) {
        const double rim = 0.065 + length * std::tan(FlamethrowerHalfAngle);
        return std::hypot(along - length, std::max(0.0, radial - rim)) <= capsule.radius;
    }
    if (along < 0) return std::hypot(-along, std::max(0.0, radial - 0.065)) <= capsule.radius;
    const double distanceToCone = radial * std::cos(FlamethrowerHalfAngle) -
        along * std::sin(FlamethrowerHalfAngle);
    return distanceToCone <= capsule.radius + 0.065;
}

} // namespace ian
