#pragma once

#include "core/Types.hpp"
#include <algorithm>
#include <cmath>

namespace ian {
inline constexpr double DecoyBeaconInterval = 8.0;
inline constexpr double DecoyBeaconDuration = 5.0;
inline constexpr double DecoyBeaconPulseDuration = 1.4;
[[nodiscard]] inline double decoyBeaconRadius(int level) {
    return 6.0 + .25 * (std::clamp(level, 1, 8) - 1);
}
[[nodiscard]] inline int decoyBeaconTargets(int level) {
    return 8 + (std::clamp(level, 1, 8) - 1);
}
struct DecoyBeaconRuntime {
    EntityId buildingId;
    Vec3 position;
    int level{1};
    double pulseRemaining{DecoyBeaconInterval};
    double pulseAge{DecoyBeaconInterval};
};
[[nodiscard]] inline double decoyBeaconLightStrength(const DecoyBeaconRuntime& beacon) {
    const double charge = std::clamp(1.0 - beacon.pulseRemaining / 1.4, 0.0, 1.0);
    return std::clamp(.35 + .5 * charge * charge + .65 * std::exp(-beacon.pulseAge * 3.5), 0.0, 1.0);
}
} // namespace ian
