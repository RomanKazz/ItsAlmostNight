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
[[nodiscard]] std::optional<std::size_t> hoveredRunUpgradeChoice(
    const SimulationSnapshot& snapshot, Vector2 mousePosition);
void drawRunUpgradeOverlay(
    GameUi& ui,
    const SimulationSnapshot& snapshot,
    const SkillTree& skillTree,
    double entranceSeconds,
    double exitSeconds,
    std::optional<std::size_t> selectedChoice,
    std::span<const float> hoverAmounts);
[[nodiscard]] std::size_t cardCollectionPageCount(
    const SkillTree& skillTree);
void drawCardCollectionOverlay(
    GameUi& ui,
    const SimulationSnapshot& snapshot,
    const SkillTree& skillTree,
    std::size_t page);

} // namespace ian::app_detail
