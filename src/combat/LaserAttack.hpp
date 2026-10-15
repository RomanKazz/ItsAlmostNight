#pragma once
#include <algorithm>
namespace ian {
inline constexpr double LaserFocusRampSeconds = 3.0;
inline constexpr double LaserMaximumDamageMultiplier = 4.0;
[[nodiscard]] inline double laserFocusMultiplier(double seconds) {
    return 1.0 + (LaserMaximumDamageMultiplier - 1.0) *
        std::clamp(seconds / LaserFocusRampSeconds, 0.0, 1.0);
}
}
