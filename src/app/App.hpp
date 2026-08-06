#pragma once

#include "app/UserSettings.hpp"
#include "audio/AudioSystem.hpp"
#include "buildings/PlacementLine.hpp"
#include "core/FixedStep.hpp"
#include "game/Simulation.hpp"
#include "graphics/EnvironmentSystem.hpp"
#include "graphics/ModularBuildingRenderer.hpp"
#include "graphics/Renderer.hpp"
#include "presentation/PresentationTypes.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudRenderer.hpp"
#include "ui/SkillTreeScreen.hpp"
#include "ui/TargetHealthBar.hpp"

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

namespace ian {

struct ResourceGainVisual {
    ResourceType type;
    Vec3 position;
    int amount;
    double remaining;
    double duration;
};

struct DestroyedResourceVisual {
    ResourceType type;
    std::size_t visualVariant{};
    float visualYaw{};
    float visualScale{1.0F};
    Vec3 position;
    double remaining;
    double duration;
};

struct ProductionVisual {
    EntityId buildingId;
    UiResourceIcon icon;
    Vec3 position;
    int amount;
    double remaining;
    double duration;
};

class App {
  public:
    App();
    int run();

  private:
    void processInput();
    void rebuildTerrainGraphics();
    void refreshDecorationExclusions(
        const SimulationSnapshot& snapshot);
    void updateModularPlacementPreview(
        const SimulationSnapshot& snapshot);
    void beginModularPlacementDrag();
    void updateModularDragAxis(Vec3 aimHit,
                               double cellSize);
    [[nodiscard]] bool finishModularPlacementDrag();
    void clearModularPlacementDrag();
    void selectModularBuildPiece(ModularBuildPiece piece);
    void setFoundationBuildMode(bool enabled);
    void rebuildModularPlacementLine();
    void update();
    void render();
    void drawShadowPass(const SimulationSnapshot& snapshot,
                        const WorldLighting& lighting);
    void drawSelectionPass(const SimulationSnapshot& snapshot,
                           const Camera3D& camera);
    void drawWorldEntities(const SimulationSnapshot& snapshot,
                           const Camera3D& camera,
                           float nightAmount,
                           const WorldLighting& lighting);
    void drawSoldBuildingVisuals();
    void drawBlobShadows(const SimulationSnapshot& snapshot,
                         const Camera3D& camera);
    void drawWorldOverlays(const SimulationSnapshot& snapshot,
                           const WorldLighting& lighting);
    void drawPresentationEffects();
    void drawAtmosphereParticles(
        const Camera3D& camera, float nightAmount);
    void drawChestLootGlow(
        const SimulationSnapshot& snapshot,
        const Camera3D& camera);
    void drawFloatingDamageNumbers(const Camera3D& camera) const;
    void drawResourceGainVisuals(const Camera3D& camera) const;
    void drawProductionVisuals(
        const Camera3D& camera) const;
    void updateHoverTarget(const SimulationSnapshot& snapshot,
                           double frameSeconds);
    [[nodiscard]] float hitFlashAt(Vec3 position,
                                   double radius) const;
    [[nodiscard]] float buildingAnimationScaleAt(
        BuildingType type, GridPosition position) const;
    [[nodiscard]] float buildingAnimationScaleAt(
        Vec3 position,
        std::optional<EntityId> entityId =
            std::nullopt) const;
    [[nodiscard]] std::vector<ModularAnimationScale>
    modularAnimationScales(
        const SimulationSnapshot& snapshot) const;
    [[nodiscard]] float productionScaleAt(
        EntityId id) const;
    [[nodiscard]] Vec3 buildingImpactOffsetAt(
        EntityId id) const;
    [[nodiscard]] Vec3 buildingShotRecoilOffsetAt(
        EntityId id, float yaw) const;
    void addBuildingShotRecoil(EntityId id, double duration,
                               float strength);
    void drawCancelledPlacementPreview(
        const WorldLighting& lighting);
    void drawGraphicsPanel();
    void persistUserSettings(bool force = false);
    void drawEnemySpawnMenu();
    void setSkillTreeVisible(bool visible);
    void drawBuildModePie() const;
    void addEffect(PresentationEffectType type, Vec3 position,
                   double duration, float scale = 1.0F,
                   std::optional<EntityId> entityId =
                       std::nullopt,
                   double startDelay = 0.0);
    void addCameraShake(double duration, double strength);
    void addCameraImpulse(Vec3 localOffset);
    void addDamageIndicator(Vec3 sourcePosition,
                            const SimulationSnapshot& snapshot, bool severe);
    void addFloatingDamageNumber(Vec3 position, double damage,
                                 bool critical);
    void addResourceGainVisual(ResourceType type, Vec3 position,
                               int amount);

    FixedStep fixedStep_;
    Simulation simulation_;
    EnvironmentSystem environment_;
    AudioSystem audio_;
    UserSettings userSettings_;
    ModularBuildingRenderer modularBuildingRenderer_;
    std::optional<Renderer> renderer_;
    GameUi ui_;
    SkillTreeScreen skillTree_;
    bool pendingStartFromUi_{};
    bool pendingOpenSkillTreeFromUi_{};
    bool skillTreePausedSimulation_{};
    bool graphicsPanelWasVisible_{};
    int graphicsPanelTab_{};
    FirstPersonToolTuning toolTuning_{};
    bool toolPanelPreviewUsesAxe_{};
    int toolPanelPage_{};
    TargetHealthBar targetHealthBar_;
    PlayerCommand input_;
    double pendingYaw_{};
    double pendingPitch_{};
    bool pendingJump_{};
    bool pendingPickaxe_{};
    bool toolSwingUsesAxe_{};
    bool toolSwingQueued_{};
    bool toolQueuedSwingHasAttack_{};
    std::optional<EntityId> toolQueuedResourceTarget_;
    bool toolSwingAttackPending_{};
    double toolSwingQueueRemaining_{};
    double toolSwingRemaining_{};
    double toolSwingDuration_{0.48};
    double toolContactHoldRemaining_{};
    bool displayedToolUsesAxe_{};
    bool toolSwapCandidateUsesAxe_{};
    bool toolSwapDestinationUsesAxe_{};
    double toolSwapCandidateSeconds_{};
    double toolSwapRemaining_{};
    double toolSwapDuration_{0.32};
    bool pendingRifleShot_{};
    std::optional<BuildingType> pendingBuildingSelection_;
    bool pendingBuildingCancel_{};
    std::optional<PlaceBuildingCommand> pendingBuildingPlacement_;
    std::optional<GridPosition> wallDragStart_;
    std::optional<GridPosition> wallDragEnd_;
    std::optional<BuildingType> placementDragType_;
    std::optional<BuildingPlatformSurface>
        placementDragSurface_;
    std::optional<PlacementLineAxis> placementDragAxis_;
    std::vector<PlaceBuildingCommand> pendingWallPlacements_;
    int pendingBuildingRotation_{};
    double buildingRotationWheelAccumulator_{};
    double buildingRotationCooldownRemaining_{};
    bool pendingStartWave_{};
    bool pendingUnlimitedResources_{};
    std::optional<UpgradeBuildingCommand> pendingBuildingUpgrade_;
    std::optional<RepairBuildingCommand> pendingBuildingRepair_;
    std::optional<EntityId> repairSweepTarget_;
    bool repairSweepActive_{};
    std::optional<SellBuildingCommand> pendingBuildingSale_;
    std::vector<SellBuildingCommand>
        queuedBuildingSales_;
    std::optional<RemoveModularBuildingCommand>
        pendingModularBuildingRemoval_;
    std::vector<RemoveModularBuildingCommand>
        queuedModularBuildingRemovals_;
    bool removalDragActive_{};
    std::vector<EntityId> removalDragTargets_;
    std::vector<EntityId> structuralRiskIds_;
    std::optional<BuildingInstance> pendingSoldBuildingVisual_;
    std::uint8_t pendingSoldWallConnections_{};
    bool pendingWeaponToggle_{};
    bool pendingWeaponUpgrade_{};
    bool pendingBombThrow_{};
    bool pendingInteract_{};
    bool pendingDefeatAllEnemies_{};
    bool pendingToggleInvulnerability_{};
    bool pendingDamageCore_{};
    bool pendingSpawnEnemy_{};
    bool enemySpawnMenuVisible_{};
    int debugSpawnCount_{50};
    std::optional<ToggleGateCommand> pendingGateToggle_;
    std::string statusMessage_;
    double statusMessageRemaining_{};
    std::vector<PresentationEffect> effects_;
    std::vector<ArrowVisual> arrowVisuals_;
    double cameraShakeRemaining_{};
    double cameraShakeStrength_{};
    double cameraBobPhase_{};
    double cameraBobAmount_{};
    double cameraBobSpeed_{};
    Vec3 cameraBobPreviousPosition_{};
    bool cameraBobPositionInitialized_{};
    double cameraLookYawLag_{};
    double cameraLookPitchLag_{};
    double cameraStrafeLean_{};
    double previousVisualYaw_{};
    double previousVisualPitch_{};
    bool cameraInertiaInitialized_{};
    double smoothedGroundCameraY_{};
    bool groundCameraSmoothingInitialized_{};
    bool groundCameraWasGrounded_{};
    Vec3 cameraImpulseOffset_{};
    double landingResponseRemaining_{};
    double landingResponseDuration_{0.24};
    double landingResponseStrength_{};
    bool playerSpawnDropActive_{};
    double playerSpawnDropHeight_{};
    double playerSpawnDropVelocity_{};
    float motionBobIntensity_{1.0F};
    float motionShakeIntensity_{1.0F};
    float motionLandingIntensity_{1.0F};
    float motionSwayIntensity_{1.0F};
    std::vector<DamageIndicator> damageIndicators_;
    std::vector<FloatingDamageNumber> floatingDamageNumbers_;
    std::vector<ResourceGainVisual> resourceGainVisuals_;
    std::vector<ProductionVisual> productionVisuals_;
    std::vector<DestroyedResourceVisual>
        destroyedResourceVisuals_;
    struct DestroyedEnemyVisual {
        EnemyType type;
        Vec3 position;
        double yaw;
        double remaining;
        double duration;
    };
    std::vector<DestroyedEnemyVisual>
        destroyedEnemyVisuals_;
    std::vector<EnemyDrawInstance>
        enemyDrawInstances_;
    std::vector<EnemyDrawInstance>
        destroyedEnemyDrawInstances_;
    std::vector<TreeDrawInstance>
        resourceTreeDrawInstances_;
    std::vector<RockDrawInstance>
        resourceRockDrawInstances_;
    struct SoldBuildingVisual {
        std::optional<BuildingInstance> building;
        std::optional<PlatformFrameInstance>
            platformFrame;
        std::optional<WallInstance> modularWall;
        std::optional<RampInstance> ramp;
        std::uint8_t wallConnections{};
        double remaining;
        double duration;
    };
    std::vector<SoldBuildingVisual> soldBuildingVisuals_;
    std::optional<SoldBuildingVisual>
        pendingSoldModularVisual_;
    std::vector<GrassClearArea> grassClearAreas_;
    std::uint64_t decorationExclusionFingerprint_{};
    Vector2 worldRevealOrigin_{};
    double worldRevealElapsed_{1000.0};
    struct BuildingImpactVisual {
        EntityId id;
        Vec3 direction;
        double remaining;
        double duration;
    };
    std::vector<BuildingImpactVisual> buildingImpactVisuals_;
    struct BuildingShotRecoilVisual {
        EntityId id;
        double remaining;
        double duration;
        float strength;
    };
    std::vector<BuildingShotRecoilVisual>
        buildingShotRecoilVisuals_;
    double woodHudBounceRemaining_{};
    double stoneHudBounceRemaining_{};
    std::optional<EntityId> hoveredResource_;
    std::optional<EntityId> hoveredBuilding_;
    std::optional<EntityId> hoveredEnemy_;
    std::optional<ResourceCost> hoveredBuildingUpgradeCost_;
    std::optional<BuildingStatComparison> hoveredBuildingStats_;
    double hoverGraceRemaining_{};
    double buildingHoverSeconds_{};
    std::optional<EntityId> buildingContextCardTarget_;
    std::optional<ResourceCost>
        buildingContextCardUpgradeCost_;
    std::optional<BuildingStatComparison>
        buildingContextCardStats_;
    std::optional<EntityId> recentlyDamagedBuilding_;
    double damagedBuildingHealthBarRemaining_{};
    double playerDamageFlashRemaining_{};
    double hitStopRemaining_{};
    double crosshairHitRemaining_{};
    double crosshairHitDuration_{0.18};
    bool crosshairHitCritical_{};
    double invalidActionRemaining_{};
    std::optional<Vector2> placementPreviewCenter_;
    std::optional<GridPosition> placementPreviewGrid_;
    std::optional<BuildingType> placementPreviewType_;
    double placementSnapPulseRemaining_{};
    double placementRotationYaw_{};
    struct CancelledPlacementPreview {
        BuildingType type;
        float yaw;
        Vector2 center;
        double remaining;
        double duration;
    };
    std::optional<CancelledPlacementPreview>
        cancelledPlacementPreview_;
    double weaponRecoilRemaining_{};
    double weaponRecoilDuration_{0.16};
    float weaponRecoilStrength_{};
    float cameraFov_{75.0F};
    std::optional<EntityId> buildingStatsUpgradeEntity_;
    double buildingStatsUpgradeRemaining_{};
    double buildingStatsUpgradeDuration_{0.9};
    EnemyType debugSpawnType_{EnemyType::Basic};
    bool slowMotion_{};
    bool showColliders_{};
    bool showFlowField_{};
    bool showSpatialHash_{};
    bool showTerrainWireframe_{};
    enum class BuildModePieChoice {
        Buildings,
        Foundations,
    };
    bool buildModePieVisible_{};
    Vector2 buildModePieDirection_{};
    std::optional<BuildModePieChoice>
        buildModePieChoice_;
    BuildingType lastBuildingSelection_{
        BuildingType::Wall};
    float buildHotbarSelectionPosition_{1.0F};
    float buildHotbarSelectionAlpha_{};
    float foundationHotbarSelectionPosition_{};
    float foundationHotbarSelectionAlpha_{};
    float minimapExpansion_{};
    bool foundationBuildMode_{};
    ModularBuildPiece modularBuildPiece_{
        ModularBuildPiece::Foundation};
    std::optional<PlatformFramePlacement>
        platformFramePreview_;
    std::optional<WallPlacement> wallPreview_;
    std::optional<RampPlacement> rampPreview_;
    Rotation modularRotation_{Rotation::Deg0};
    std::optional<Vec3> foundationTerrainHit_;
    std::optional<Vec3> modularSnapHit_;
    std::optional<Vec3> modularSnapMarker_;
    std::optional<EntityId> modularEdgeHoverFrame_;
    std::optional<GridCoord>
        modularEdgeExtensionAnchor_;
    std::optional<EntityId> rampSocketFrame_;
    std::optional<Rotation> rampSocketRotation_;
    double rampSocketLostGraceRemaining_{};
    double rampSocketManualOverrideRemaining_{};
    std::optional<GridCoord> modularPreviewAnchor_;
    std::optional<Vec3> modularPreviewVisualOrigin_;
    std::optional<GridCoord> modularDragStart_;
    std::optional<GridCoord> modularDragEnd_;
    std::optional<Vec3> modularDragOrigin_;
    std::optional<int> modularDragStorey_;
    std::optional<double>
        modularDragTargetFloorHeight_;
    std::optional<double> modularDragPlaneHeight_;
    std::optional<Rotation> modularDragRotation_;
    std::optional<ModularBuildPiece> modularDragPiece_;
    std::optional<PlacementLineAxis> modularDragAxis_;
    std::optional<GridCoord>
        modularDragCandidateEnd_;
    int modularDragCandidateFrames_{};
    double modularDragLookMovement_{};
    bool modularDragExtended_{};
    std::vector<Vec3> modularDragHits_;
    std::vector<PlatformFramePlacement>
        modularPlatformDragPreviews_;
    std::vector<WallPlacement>
        modularWallDragPreviews_;
    std::vector<RampPlacement>
        modularRampDragPreviews_;
    bool hideBottomHud_{};
};

} // namespace ian
