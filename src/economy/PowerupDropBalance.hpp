#pragma once

namespace ian {

// Boss classification takes precedence; these are total chances of any
// powerup. Type selection remains a separate equal-weight deterministic roll.
[[nodiscard]] constexpr double powerupDropChance(bool elite, bool boss) {
    return boss ? 0.10 : elite ? 0.025 : 0.005;
}

} // namespace ian
