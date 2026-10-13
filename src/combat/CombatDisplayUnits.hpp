#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {

// Combat uses compact internal values for balancing. Present them on a
// ten-times-larger integer scale so percentage modifiers never fill the
// battlefield with decimal damage numbers.
inline constexpr double CombatDisplayScale = 10.0;

[[nodiscard]] inline int combatDisplayValue(double value) {
    if (std::isnan(value) || value <= 0.0) {
        return 0;
    }
    const double scaled = std::min(value * CombatDisplayScale,
        static_cast<double>(std::numeric_limits<int>::max()));
    return std::max(1, static_cast<int>(std::lround(scaled)));
}

[[nodiscard]] inline double combatDisplayScalar(double value) {
    return value * CombatDisplayScale;
}

} // namespace ian
