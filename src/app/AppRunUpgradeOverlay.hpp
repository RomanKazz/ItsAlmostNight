#pragma once

#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"

#include <algorithm>
#include <optional>
#include <span>

#include <raylib.h>

namespace ian::app_detail {

inline constexpr double RunUpgradeChoiceInputQuietDuration = 0.35;

// Attack clicks must stop before a fresh press can select an upgrade. A long
// opening frame must not consume the entire protection window at once.
[[nodiscard]] inline double advanceRunUpgradeInputDelay(
    double remaining, bool inputsReleased, double frameSeconds) {
    if (!inputsReleased) return RunUpgradeChoiceInputQuietDuration;
    return std::max(0.0, remaining - std::clamp(frameSeconds, 0.0, 0.05));
}

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
struct RunShopLayout {
    std::array<Rectangle, RunShopSlotCount> cards{}, buy{}, locks{};
    Rectangle inventory{}, stats{}, header{}, reroll{}, proceed{};
    float scale{1};
};
[[nodiscard]] RunShopLayout runShopLayout();
void drawRunShopOverlay(GameUi& ui, const SimulationSnapshot& snapshot,
    std::optional<int> keyboardFocus = std::nullopt,
    std::optional<Vector2> pointerPosition = std::nullopt);
} // namespace ian::app_detail
