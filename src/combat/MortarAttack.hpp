#pragma once
#include "buildings/MortarRig.hpp"
#include <algorithm>
#include <optional>

namespace ian {
inline constexpr double MortarHorizontalSpeed = 6.0;
inline constexpr double MortarGravity = 9.8;
struct MortarTrajectory {
    Vec3 velocity;
    double flightTime;
    double pitch;
};
// Solve an actual gravity arc. Projectile Speed shortens flight time until the
// model's 35-degree mechanical stop; the 45-degree neutral is already authored.
[[nodiscard]] inline std::optional<MortarTrajectory> mortarTrajectory(
    Vec3 origin, Vec3 target, double speed) {
    if (speed <= 0) return std::nullopt;
    const double x = target.x - origin.x, z = target.z - origin.z;
    const double distance = std::hypot(x, z), height = target.y - origin.y;
    if (distance < 0.01) return std::nullopt;
    const double minimumRise = distance * std::tan(MortarNeutralElevation + MortarMinimumPitch) - height;
    const double maximumRise = distance * std::tan(MortarNeutralElevation + MortarMaximumPitch) - height;
    if (maximumRise <= 0) return std::nullopt;
    const double minimumTime = std::sqrt(std::max(0.0001, 2 * minimumRise / MortarGravity));
    const double maximumTime = std::sqrt(2 * maximumRise / MortarGravity);
    if (maximumTime < minimumTime) return std::nullopt;
    const double time = std::clamp(std::max(1.7, distance / MortarHorizontalSpeed) *
        MortarHorizontalSpeed / speed, minimumTime, maximumTime);
    const double verticalSpeed = height / time + 0.5 * MortarGravity * time;
    return MortarTrajectory{{x / time, verticalSpeed, z / time}, time,
        std::atan2(verticalSpeed, distance / time) - MortarNeutralElevation};
}
}
