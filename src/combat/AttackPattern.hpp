#pragma once
#include "combat/AttackStats.hpp"

namespace ian {
inline constexpr double GunBurstDurationFraction = 0.6;
struct BurstTiming {
    double interval;
    double duration;
};
// Event frequency, instance count and intra-event spacing remain independent.
// Density changes timing only: every instance retains its full snapshot damage.
[[nodiscard]] inline BurstTiming gunBurstTiming(const FinalAttackStats& stats) {
    const int gaps = std::max(0, effectiveAttackCount(stats.quantity, 1) - 1);
    const double interval = std::min(
        stats.burstInterval / std::sqrt(std::max(1.0, stats.attackSpeed)),
        stats.attackInterval * GunBurstDurationFraction / std::max(1, gaps));
    return {interval, interval * gaps};
}
} // namespace ian
