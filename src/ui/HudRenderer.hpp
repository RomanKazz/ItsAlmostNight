#pragma once

#include "enemies/EnemySystem.hpp"
#include "presentation/PresentationTypes.hpp"

#include <raylib.h>

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
    bool hideBottomHints{};
    ActionMode actionMode{ActionMode::Tools};
    bool foundationBuildMode{};
    std::size_t selectedModularBuildPiece{};
    float buildHotbarSelectionPosition{};
    float buildHotbarSelectionAlpha{};
    float foundationHotbarSelectionPosition{};
    float foundationHotbarSelectionAlpha{};
    float weaponHotbarSelectionPosition{};
    float weaponHotbarSelectionAlpha{};
    float informationExpansion{};
    bool mapOverlayOpen{};
    bool minimapHidden{};
    bool showCoreHealth{};
    bool showBuildingContextCard{};
    bool repairSweepActive{};
    float woodResourceBounce{};
    float stoneResourceBounce{};
    float crystalResourceBounce{};
    float woodResourcePulse{};
    float stoneResourcePulse{};
    float crystalResourcePulse{};
    float coinResourceBounce{};
    float coinResourcePulse{};
    double displayedInsight{};
    double insightPulse{};
    double insightGainAmount{};
    double insightGainRemaining{};
    double insightGainDuration{0.8};
    double treePointPulse{};
    std::string_view objectivePulseId;
    double objectivePulse{};
    double crosshairHitRemaining{};
    double crosshairHitDuration{};
    bool crosshairHitCritical{};
    double invalidActionRemaining{};
    float weaponRecoilAmount{};
    std::optional<EntityId> buildingStatsUpgradeEntity;
    double buildingStatsUpgradeRemaining{};
    double buildingStatsUpgradeDuration{};
};

void drawHud(GameUi& ui, const SimulationSnapshot& snapshot,
             const HudViewState& view, const Camera3D& camera,
             const ControlSettings& controls);
void drawMinimapHud(GameUi& ui, const SimulationSnapshot& snapshot,
                    float expansion);
void drawRunStateOverlay(const SimulationSnapshot& snapshot);

} // namespace ian
