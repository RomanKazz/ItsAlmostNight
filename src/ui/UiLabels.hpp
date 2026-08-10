#pragma once

#include "buildings/BuildingSystem.hpp"

#include <string>
#include <string_view>

namespace ian {

[[nodiscard]] std::string_view buildingDisplayName(
    BuildingType type);

[[nodiscard]] std::string introGatherRewardMessage(
    double insightReward);

} // namespace ian
