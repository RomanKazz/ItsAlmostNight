#pragma once

#include "buildings/BuildingHotbarLayout.hpp"
#include "enemies/EnemySystem.hpp"
#include "presentation/PresentationTypes.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace ian {

class GameUi;
struct ControlSettings;
struct SimulationSnapshot;

struct HudViewState {
    std::span<const DamageIndicator> damageIndicators;
    std::string_view statusMessage;
    double statusMessageRemaining{};
    double statusMessageShakeRemaining{};
    ActionMode actionMode{ActionMode::Equipment};
    float hotbarModeBlend{};
    bool foundationBuildMode{};
    std::size_t selectedModularBuildPiece{};
    BuildingHotbarCategory buildingHotbarCategory{
        BuildingHotbarCategory::Base};
    float buildHotbarSelectionPosition{};
    float buildHotbarSelectionAlpha{};
    float foundationHotbarSelectionPosition{};
    float foundationHotbarSelectionAlpha{};
    float weaponHotbarSelectionPosition{};
    float weaponHotbarSelectionAlpha{};
    float informationExpansion{};
    bool mapOverlayOpen{};
    bool minimapHidden{};
    bool showFps{};
    bool showCoreHealth{};
    double playerHealthTrail{};
    float playerDamagePulse{};
    bool showBuildingContextCard{};
    float buildingContextCardEntranceSeconds{};
    bool repairSweepActive{};
    float woodResourcePop{};
    float stoneResourcePop{};
    float crystalResourcePop{};
    float coinResourcePop{};
    double displayedWood{};
    double displayedStone{};
    double displayedCrystals{};
    double displayedCoins{};
    double displayedInsight{};
    double insightPulse{};
    double treePointPulse{};
    std::string_view objectivePulseId;
    double objectivePulse{};
    float objectiveTrackerExpansion{};
    std::string_view objectiveCompletionTitle;
    int objectiveCompletionReward{};
    double objectiveCompletionProgress{};
    double insightGainAmount{};
    double insightGainProgress{};
    double crosshairHitRemaining{};
    double crosshairHitDuration{};
    bool crosshairHitCritical{};
    double invalidActionRemaining{};
    float weaponRecoilAmount{};
    std::optional<EntityId> buildingStatsUpgradeEntity;
    double buildingStatsUpgradeRemaining{};
    double buildingStatsUpgradeDuration{};
};

struct HudRenderPerformanceStats {
    double resourcesMs{};
    double vitalityMs{};
    double informationMs{};
    double objectivesMs{};
    double lootMs{};
    double phaseMs{};
    double hotbarMs{};
    double feedbackMs{};
};

struct MinimapRenderPerformanceStats {
    double setupMs{};
    double targetRefreshMs{};
    double frameMs{};
    double labelsMs{};
};

[[nodiscard]] const HudRenderPerformanceStats&
hudRenderPerformanceStats() noexcept;
[[nodiscard]] std::array<Vector2, 4>
hudResourceIconCenters() noexcept;
[[nodiscard]] const MinimapRenderPerformanceStats&
minimapRenderPerformanceStats() noexcept;

void drawHud(GameUi& ui, const SimulationSnapshot& snapshot,
             const HudViewState& view, const Camera3D& camera,
             const ControlSettings& controls);
enum class MinimapHudPass { Complete, Prepare, Composite };
void drawMinimapHud(GameUi& ui, const SimulationSnapshot& snapshot,
                    float expansion, MinimapHudPass pass = MinimapHudPass::Complete);
void drawRunStateOverlay(const SimulationSnapshot& snapshot);

} // namespace ian
