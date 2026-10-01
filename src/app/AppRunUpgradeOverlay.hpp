#pragma once

#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"

#include <optional>
#include <span>

#include <raylib.h>

namespace ian::app_detail {

inline constexpr double RunUpgradeOverlayExitDuration = 0.34;

[[nodiscard]] Rectangle runUpgradeCardBounds(
    std::size_t index, std::size_t count);
[[nodiscard]] Rectangle runUpgradeStatsPanelBounds(std::size_t count);
[[nodiscard]] std::optional<std::size_t> hoveredRunUpgradeChoice(
    const SimulationSnapshot& snapshot, Vector2 mousePosition);
// Projects only HUD values; the simulation and its world views are unchanged.
[[nodiscard]] SimulationSnapshot previewRunUpgrade(
    const SimulationSnapshot& snapshot, std::size_t choiceIndex);
void drawRunUpgradeOverlay(
    GameUi& ui,
    const SimulationSnapshot& snapshot,
    double entranceSeconds,
    double exitSeconds,
    std::optional<std::size_t> selectedChoice,
    std::span<const float> hoverAmounts,
    std::optional<Vector2> pointerPosition = std::nullopt);
} // namespace ian::app_detail
