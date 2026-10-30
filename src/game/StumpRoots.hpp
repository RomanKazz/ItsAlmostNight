#pragma once

#include "core/Types.hpp"
#include <algorithm>
#include <cmath>

namespace ian::stump_roots {
inline constexpr int HitsToCharge = 4;
inline constexpr double TravelSeconds = 0.42;
inline constexpr double VisualSeconds = 1.25;
inline constexpr double Range = 6.5;
inline constexpr double DamageFraction = 0.70;
inline constexpr double SlowFraction = 0.40;
inline constexpr double SlowSeconds = 1.5;

[[nodiscard]] inline double front(double seconds, double scale) {
    return Range * scale * std::clamp(seconds / TravelSeconds, 0.0, 1.0);
}

[[nodiscard]] inline bool reaches(Vec3 origin, Vec3 direction, Vec3 target,
    double previousFront, double currentFront, double scale) {
    const double x = target.x-origin.x, z = target.z-origin.z;
    const double along = x*direction.x+z*direction.z;
    const double side = std::abs(x*direction.z-z*direction.x);
    return along >= std::max(-0.25, previousFront-0.5*scale) &&
        along <= currentFront+0.35*scale && side <= 0.45*scale+0.30*std::max(0.0,along);
}
} // namespace ian::stump_roots
