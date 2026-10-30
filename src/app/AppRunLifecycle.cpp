#include "app/App.hpp"

#include <raylib.h>

namespace ian {

void App::resetRunClientState(
    const SimulationSnapshot& snapshot) {
    // Build the static landscape before the HUD arrival fade. The first
    // visible minimap refresh must not generate/upload a million-pixel atlas.
    const auto minimapStart = PerformanceClock::now();
    ui_.prepareMinimapLandscape(snapshot);
    performanceStats_.minimapPreparation.sample(performanceMilliseconds(minimapStart));
    restartHold_.resetProgress();
    runShopWasVisible_=false;
    runShopOverlayEntranceSeconds_=0;
    fixedStep_.reset();
    statusMessage_.clear();
    statusMessageRemaining_ = 0.0;
    statusMessageShakeRemaining_ = 0.0;
    effects_.clear();
    floatingDamageNumbers_.clear();
    resourceGainVisuals_.clear();
    productionVisuals_.clear();
    destroyedResourceVisuals_.clear();
    nightForecast_ = {};
    tutorialHint_ = {};
    soldBuildingVisuals_.clear();
    destroyedEnemyVisuals_.clear();
    enemyDrawInstances_.clear();
    destroyedEnemyDrawInstances_.clear();
    pendingSoldBuildingVisual_.reset();
    pendingSoldModularVisual_.reset();
    pendingSoldWallConnections_ = 0U;
    queuedBuildingSales_.clear();
    queuedModularBuildingRemovals_.clear();
    removalDragActive_ = false;
    removalLookAwaySeconds_ = 0.0;
    removalDragTargets_.clear();
    persistentDecorationClearAreaBuffer_.clear();
    activeDecorationClearAreaBuffer_.clear();
    grassClearAreaFingerprint_ = 0U;
    buildingRotationWheelAccumulator_ = 0.0;
    buildingRotationCooldownRemaining_ = 0.0;
    cameraShakeRemaining_ = 0.0;
    cameraShakeStrength_ = 0.0;
    explosionShakeCooldownRemaining_ = 0.0;
    weaponLookYawOffset_ = 0.0;
    weaponLookPitchOffset_ = 0.0;
    weaponLookYawVelocity_ = 0.0;
    weaponLookPitchVelocity_ = 0.0;
    cameraStrafeLean_ = 0.0;
    cameraImpulseOffset_ = {};
    landingResponseRemaining_ = 0.0;
    playerArrival_.reset();
    cameraInertiaInitialized_ = false;
    groundCameraSmoothingInitialized_ = false;
    groundCameraWasGrounded_ = false;
    cameraBobPositionInitialized_ = false;
    damageIndicators_.clear();
    woodHudBounceRemaining_ = 0.0;
    stoneHudBounceRemaining_ = 0.0;
    crystalHudBounceRemaining_ = 0.0;
    coinHudBounceRemaining_ = 0.0;
    playerDamageFlashRemaining_ = 0.0;
    playerRespawnRevealRemaining_ = 0.0;
    recentlyDamagedBuilding_.reset();
    damagedBuildingHealthBarRemaining_ = 0.0;
    toolContactHoldRemaining_ = 0.0;
    crosshairHitRemaining_ = 0.0;
    invalidActionRemaining_ = 0.0;
    modularPlacementFailureRepeatWindow_ = 0.0;
    buildingHoverSeconds_ = 0.0;
    buildingContextCardTarget_.reset();
    buildingContextCardUpgradeCost_.reset();
    buildingContextCardStats_.reset();
    buildingImpactVisuals_.clear();
    placementPreviewCenter_.reset();
    placementPreviewGrid_.reset();
    placementPreviewType_.reset();
    placementSnapPulseRemaining_ = 0.0;
    weaponRecoilRemaining_ = 0.0;
    weaponRecoilStrength_ = 0.0F;
    cameraFov_ = 75.0F;
    cancelledPlacementPreview_.reset();
    buildingShotRecoilVisuals_.clear();
    buildingStatsUpgradeEntity_.reset();
    buildingStatsUpgradeRemaining_ = 0.0;
    wallDragStart_.reset();
    wallDragEnd_.reset();
    placementDragType_.reset();
    placementDragSurface_.reset();
    placementDragAxis_.reset();
    placementDragCandidateEnd_.reset();
    placementDragCandidateFrames_ = 0;
    placementDragLookMovement_ = 0.0;
    placementDragExtended_ = false;
    pendingWallPlacements_.clear();
    clearModularPlacementDrag();
    modularSnapHit_.reset();
    modularSnapMarker_.reset();
    modularEdgeHoverFrame_.reset();
    modularEdgeExtensionAnchor_.reset();
    rampSocketFrame_.reset();
    rampSocketRotation_.reset();
    rampSocketLostGraceRemaining_ = 0.0;
    rampSocketManualOverrideRemaining_ = 0.0;
    modularPreviewAnchor_.reset();
    modularFloorPreviewBlockedAim_.reset();
    resetRunInputState();
    resetEquipmentActionMode(snapshot);
    DisableCursor();
}

} // namespace ian
