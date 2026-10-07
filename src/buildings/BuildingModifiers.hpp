#pragma once

#include "buildings/BuildingSystem.hpp"

namespace ian {

// Old saves store the first enhancement as a flag; stacks take precedence.
[[nodiscard]] inline double buildingAnvilMultiplier(
    int stacks, bool enhanced = false) {
    return stacks > 0 ? 1.0 + 0.10 * stacks : enhanced ? 1.10 : 1.0;
}

[[nodiscard]] inline double buildingAnvilMultiplier(
    const BuildingInstance& building) {
    return buildingAnvilMultiplier(building.anvilStacks, building.anvilEnhanced);
}

} // namespace ian
