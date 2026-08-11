#pragma once

#include "economy/ResourceCost.hpp"

#include <string>

namespace ian::hud_detail {

[[nodiscard]] inline std::string costText(const ResourceCost& cost) {
    std::string result;
    const auto append = [&result](const char* prefix, int amount) {
        if (amount <= 0) {
            return;
        }
        if (!result.empty()) {
            result += "  ";
        }
        result += prefix;
        result += std::to_string(amount);
    };
    append("W:", cost.wood);
    append("S:", cost.stone);
    append("C:", cost.gold);
    return result.empty() ? "FREE" : result;
}

} // namespace ian::hud_detail
