#pragma once

#include "game/Simulation.hpp"

#include <optional>

#include <raylib.h>

namespace ian::app_detail {

[[nodiscard]] Rectangle runUpgradeCardBounds(
    std::size_t index, std::size_t count);
[[nodiscard]] std::optional<std::size_t> hoveredRunUpgradeChoice(
    const SimulationSnapshot& snapshot, Vector2 mousePosition);
void drawRunUpgradeOverlay(const SimulationSnapshot& snapshot);

} // namespace ian::app_detail
