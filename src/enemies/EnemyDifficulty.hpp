#pragma once

#include <algorithm>

namespace ian {

inline constexpr double EliteHealthMultiplier = 4.0;
inline constexpr double EliteDamageMultiplier = 1.5;

[[nodiscard]] constexpr double enemyWaveSpeedGrowth(int wave) {
    return 1.0 + .02 * static_cast<double>(std::clamp(wave, 1, 16) - 1);
}

// Polynomial growth keeps strong builds under pressure without exponential
// overflow in endless runs. Wave one retains its existing health.
[[nodiscard]] constexpr double enemyWaveHealthGrowth(int wave) {
    const double steps = static_cast<double>(std::max(1, wave)) - 1.0;
    return 1.0 + 0.24 * steps + 0.035 * steps * steps;
}

} // namespace ian
