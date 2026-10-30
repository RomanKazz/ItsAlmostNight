#pragma once

#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"

#include <algorithm>
#include <optional>
#include <span>

#include <raylib.h>

namespace ian::app_detail {

inline constexpr double RunUpgradeOverlayEntranceDuration = 0.18;
inline constexpr double RunUpgradeOverlayExitDuration = 0.10;
inline constexpr double RunUpgradeChoiceFeedbackDuration = 0.24;
inline constexpr double RunUpgradeChoiceTotalDuration =
    RunUpgradeChoiceFeedbackDuration + RunUpgradeOverlayExitDuration;
inline constexpr double RunUpgradeChoiceInputQuietDuration = 0.35;

// Attack clicks must stop before a fresh press can select an upgrade. A long
// opening frame must not consume the entire protection window at once.
[[nodiscard]] inline double advanceRunUpgradeInputDelay(
    double remaining, bool inputsReleased, double frameSeconds) {
    if (!inputsReleased) return RunUpgradeChoiceInputQuietDuration;
    return std::max(0.0, remaining - std::clamp(frameSeconds, 0.0, 0.05));
}

struct RunUpgradeOverlayTransition { float opacity; float scale; };
[[nodiscard]] RunUpgradeOverlayTransition runUpgradeOverlayTransition(
    double entranceSeconds, double exitSeconds, bool closing);

// Bounds use logical coordinates inside a UI canvas, physical screen pixels
// for gameplay input/update callers outside it.
[[nodiscard]] Rectangle runUpgradeCardBounds(
    std::size_t index, std::size_t count);
[[nodiscard]] Rectangle runUpgradeRerollBounds(std::size_t count);
[[nodiscard]] Rectangle runUpgradeUtilityBounds(std::size_t count,std::size_t action);
[[nodiscard]] std::optional<std::size_t> hoveredRunUpgradeChoice(
    const SimulationSnapshot& snapshot, Vector2 mousePosition);
[[nodiscard]] inline bool canSelectRunUpgradeChoice(
    const SimulationSnapshot& snapshot, std::size_t index) {
    return index < snapshot.runUpgradeChoiceCount &&
        (snapshot.unlimitedResources || snapshot.coins >=
            runUpgradeChoicePrice(index, snapshot.runUpgradePaidChoicePrice));
}
// Effective attack/trigger distance, including base levels and the offered roll.
[[nodiscard]] std::pair<double,double> buildingRangeUpgradePreview(
    const SimulationSnapshot& snapshot, BuildingType type, RunUpgradeRarity rarity,
    std::uint64_t rollSeed);
[[nodiscard]] std::pair<double,double> buildingAttackCountUpgradePreview(
    const SimulationSnapshot& snapshot, BuildingType type, RunUpgradeRarity rarity,
    std::uint64_t rollSeed);

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
    std::optional<Vector2> pointerPosition = std::nullopt,
    std::optional<std::size_t> keyboardChoice = std::nullopt, bool reduceFlashes = false, bool banishArmed = false);
struct RunShopLayout {
    std::array<Rectangle, RunShopSlotCount> cards{}, buy{}, locks{};
    Rectangle inventory{}, stats{}, header{}, reroll{}, proceed{};
    float scale{1};
};
inline constexpr double RunShopOverlayEntranceDuration=.18;
[[nodiscard]] RunShopLayout runShopLayout();
void drawRunShopOverlay(GameUi& ui, const SimulationSnapshot& snapshot,
    std::optional<int> keyboardFocus = std::nullopt,
    std::optional<Vector2> pointerPosition = std::nullopt,
    double entranceSeconds = RunShopOverlayEntranceDuration, bool reduceFlashes = false);
} // namespace ian::app_detail
