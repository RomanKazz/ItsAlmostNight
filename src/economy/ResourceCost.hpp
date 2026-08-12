#pragma once

#include "core/SaturatingArithmetic.hpp"

namespace ian {

struct ResourceCost {
    int wood{};
    int stone{};
    int crystals{};

    friend bool operator==(
        const ResourceCost&, const ResourceCost&) = default;
};

[[nodiscard]] constexpr ResourceCost addResourceCosts(
    ResourceCost left, ResourceCost right) {
    return {
        saturatingAdd(left.wood, right.wood),
        saturatingAdd(left.stone, right.stone),
        saturatingAdd(left.crystals, right.crystals),
    };
}

[[nodiscard]] constexpr bool canAfford(
    ResourceCost cost, int wood, int stone, int crystals) {
    return wood >= cost.wood &&
           stone >= cost.stone &&
           crystals >= cost.crystals;
}

} // namespace ian
