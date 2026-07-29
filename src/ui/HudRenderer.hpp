#pragma once

#include "enemies/EnemySystem.hpp"
#include "presentation/PresentationTypes.hpp"

#include <raylib.h>

#include <optional>
#include <span>
#include <string_view>

namespace ian {

class GameUi;
struct SimulationSnapshot;

struct HudViewState {
    std::span<const DamageIndicator> damageIndicators;
    std::string_view statusMessage;
    double statusMessageRemaining{};
    EnemyType debugSpawnType{EnemyType::Basic};
    bool slowMotion{};
    bool showColliders{};
    bool showFlowField{};
    bool showSpatialHash{};
    bool hideBottomHints{};
    bool showBuildingContextCard{};
    bool repairSweepActive{};
    double simulationTickMilliseconds{};
    double peakSimulationTickMilliseconds{};
    float woodResourceBounce{};
    float stoneResourceBounce{};
    float goldResourceBounce{};
    float woodResourcePulse{};
    float stoneResourcePulse{};
    float goldResourcePulse{};
    double crosshairHitRemaining{};
    double crosshairHitDuration{};
    bool crosshairHitCritical{};
    double invalidActionRemaining{};
    float weaponRecoilAmount{};
    std::optional<EntityId> buildingStatsUpgradeEntity;
    double buildingStatsUpgradeRemaining{};
    double buildingStatsUpgradeDuration{};
    std::string_view environmentProfile;
    float environmentTime{};
    bool environmentFrozen{};
    bool environmentManualOverride{};
};

void drawHud(GameUi& ui, const SimulationSnapshot& snapshot,
             const HudViewState& view, const Camera3D& camera);
void drawRunStateOverlay(const SimulationSnapshot& snapshot);

} // namespace ian
