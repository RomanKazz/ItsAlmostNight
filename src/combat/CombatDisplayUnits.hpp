#pragma once

#include <algorithm>
#include <cmath>

namespace ian {

// Combat uses compact internal values for balancing. Present them on a
// ten-times-larger integer scale so percentage modifiers never fill the
// battlefield with decimal damage numbers.
inline constexpr double CombatDisplayScale = 10.0;

[[nodiscard]] inline int combatDisplayValue(double value) {
    if (value <= 0.0) {
        return 0;
    }
    return std::max(
        1, static_cast<int>(std::lround(value * CombatDisplayScale)));
}

[[nodiscard]] inline double combatDisplayScalar(double value) {
    return value * CombatDisplayScale;
}

} // namespace ian
