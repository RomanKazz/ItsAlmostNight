#pragma once

#include "buildings/BuildingSystem.hpp"

#include <string_view>

namespace ian {

[[nodiscard]] std::string_view buildingDisplayName(
    BuildingType type);

} // namespace ian
