#pragma once

#include "app/UserSettings.hpp"
#include "audio/AudioSystem.hpp"
#include "buildings/PlacementLine.hpp"
#include "core/FixedStep.hpp"
#include "core/PerformanceRecorder.hpp"
#include "game/Simulation.hpp"
#include "graphics/EnvironmentSystem.hpp"
#include "graphics/ModularBuildingRenderer.hpp"
#include "graphics/Renderer.hpp"
#include "presentation/PresentationTypes.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudRenderer.hpp"
#include "ui/InteractionPrompt.hpp"
#include "ui/SkillTreeScreen.hpp"
#include "ui/TargetHealthBar.hpp"

#include <optional>
#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
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

struct AppPerformanceStats {
    PerformanceMetric frame;
    PerformanceMetric input;
    PerformanceMetric simulation;
    PerformanceMetric simulationTick;
    PerformanceMetric render;
    PerformanceMetric renderPreparation;
    PerformanceMetric shadowPass;
    PerformanceMetric selectionPass;
    PerformanceMetric terrainRender;
    PerformanceMetric worldEntitiesRender;
    PerformanceMetric decorationsRender;
    PerformanceMetric grassRender;
    PerformanceMetric environmentRender;
    PerformanceMetric overlayRender;
    PerformanceMetric postProcess;
    PerformanceMetric uiRender;
    PerformanceMetric present;
    PerformanceMetric enemyRender;
    PerformanceMetric blobShadows;
    std::size_t fixedTicks{};
    std::size_t visibleEnemies{};
    std::size_t enemyShadowDraws{};
};

class App {
  public:
    App();
    int run();

  private:
    static constexpr int ToolSettingsTab = 8;
    static constexpr double ToolSwapHideFraction = 0.42;
    static constexpr double ToolSwapDurationScale = 1.35;

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
                           const WorldLighting& lighting,
                           float interpolationAlpha);
    void drawChallengeFence(
        const ChallengeColumnInstance& column,
        bool drawRopes);
    [[nodiscard]] std::vector<GrassClearArea>
        activeDecorationClearAreas(
            const SimulationSnapshot& snapshot) const;
    void drawSoldBuildingVisuals();
    void drawBlobShadows(const SimulationSnapshot& snapshot,
                         const Camera3D& camera);
    void drawWorldOverlays(const SimulationSnapshot& snapshot,
                           const WorldLighting& lighting);
    void drawPerformanceOverlay(
        const SimulationSnapshot& snapshot) const;
    void drawPresentationEffects(const Camera3D& camera);
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
    [[nodiscard]] std::optional<InteractionPrompt>
    buildInteractionPrompt(const SimulationSnapshot& snapshot,
                           const Camera3D& camera) const;
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
    void drawMainMenu(const SimulationSnapshot& snapshot);
    void drawMainMenuWorld(const SimulationSnapshot& snapshot);
    void persistUserSettings(bool force = false);
    void applyFullscreenSetting(bool fullscreen);
    void drawEnemySpawnMenu();
    void drawItemGrantMenu();
    void drawObjectiveDebugMenu(const SimulationSnapshot& snapshot);
    [[nodiscard]] FirstPersonToolTuning& activeToolTuning();
    [[nodiscard]] const FirstPersonToolTuning& activeToolTuning() const;
    [[nodiscard]] static const char* toolTuningPath(
        FirstPersonToolVisual visual);
    void processPresentationEvents(
        std::span<const GameEvent> events,
        const SimulationSnapshot& snapshot);
    void setSkillTreeVisible(bool visible);
    void drawBuildModePie() const;
    void selectActionMode(
        ActionMode mode,
        const SimulationSnapshot& snapshot);
    void selectNextActionModeItem(
        const SimulationSnapshot& snapshot,
        int direction = 1);
    void resetRunInputState();
    void addEffect(PresentationEffectType type, Vec3 position,
                   double duration, float scale = 1.0F,
                   std::optional<EntityId> entityId =
                       std::nullopt,
                   double startDelay = 0.0);
    void addLootPickupEffect(Vec3 position,
                             LootRarity rarity,
                             LootUpgradeEffect effect,
                             std::optional<EntityId> lootId =
                                 std::nullopt);
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
    UserSettings persistedUserSettings_;
    ModularBuildingRenderer modularBuildingRenderer_;
    std::optional<Renderer> renderer_;
    GameUi ui_;
    SkillTreeScreen skillTree_;
    bool pendingStartFromUi_{};
    bool pendingOpenSkillTreeFromUi_{};
    bool pendingResumeFromUi_{};
    bool pendingRestartFromUi_{};
    bool pendingReturnToMenuFromUi_{};
    bool automaticRunRestartPending_{};
    bool exitRequested_{};
    bool skillTreePausedSimulation_{};
    bool graphicsPanelWasVisible_{};
    bool runUpgradeChoiceWasVisible_{};
    bool fullscreenApplied_{};
    int graphicsPanelTab_{};
    std::optional<ControlAction> pendingControlRebind_;
    std::array<FirstPersonToolTuning, 8> toolTunings_{};
    FirstPersonToolVisual toolPanelPreviewVisual_{
        FirstPersonToolVisual::Axe};
    int toolPanelPage_{};
    TargetHealthBar targetHealthBar_;
    InteractionPromptRenderer interactionPromptRenderer_;
    PlayerCommand input_;
    double pendingYaw_{};
    double pendingPitch_{};
    bool pendingJump_{};
    bool pendingDash_{};
    bool pendingPickaxe_{};
    bool toolSwingUsesAxe_{};
    bool toolSwingQueued_{};
    bool toolQueuedSwingHasAttack_{};
    std::optional<EntityId> toolQueuedResourceTarget_;
    bool toolSwingAttackPending_{};
    double primaryAttackHoldSeconds_{};
    double toolSwingQueueRemaining_{};
    double toolSwingRemaining_{};
    double toolSwingDuration_{0.48};
    double toolContactHoldRemaining_{};
    FirstPersonToolVisual displayedToolVisual_{
        FirstPersonToolVisual::None};
    FirstPersonToolVisual toolSwapCandidateVisual_{
        FirstPersonToolVisual::None};
    FirstPersonToolVisual toolSwapDestinationVisual_{
        FirstPersonToolVisual::None};
    bool toolViewModelInitialized_{};
    double toolSwapCandidateSeconds_{};
    double toolSwapRemaining_{};
    double toolSwapDuration_{0.32};
    bool pendingRifleShot_{};
    bool pendingIceWandShot_{};
    bool pendingFireWandShot_{};
    std::optional<BuildingType> pendingBuildingSelection_;
    bool pendingBuildingCancel_{};
    std::optional<PlaceBuildingCommand> pendingBuildingPlacement_;
    std::optional<GridPosition> wallDragStart_;
    std::optional<GridPosition> wallDragEnd_;
    std::optional<BuildingType> placementDragType_;
    std::optional<BuildingPlatformSurface>
        placementDragSurface_;
    std::optional<PlacementLineAxis> placementDragAxis_;
    std::optional<GridPosition> placementDragCandidateEnd_;
    int placementDragCandidateFrames_{};
    double placementDragLookMovement_{};
    bool placementDragExtended_{};
    std::deque<PlaceBuildingCommand> pendingWallPlacements_;
    int pendingBuildingRotation_{};
    double buildingRotationWheelAccumulator_{};
    double buildingRotationCooldownRemaining_{};
    bool pendingStartWave_{};
    bool pendingUnlimitedResources_{};
    std::optional<UpgradeBuildingCommand> pendingBuildingUpgrade_;
    std::optional<RepairBuildingCommand> pendingBuildingRepair_;
    bool pendingRepairAllBuildings_{};
    bool pendingPurchaseBombBundle_{};
    std::optional<EntityId> repairSweepTarget_;
    bool repairSweepActive_{};
    std::optional<SellBuildingCommand> pendingBuildingSale_;
    std::deque<SellBuildingCommand>
        queuedBuildingSales_;
    std::optional<RemoveModularBuildingCommand>
        pendingModularBuildingRemoval_;
    std::deque<RemoveModularBuildingCommand>
        queuedModularBuildingRemovals_;
    bool removalDragActive_{};
    std::vector<EntityId> removalDragTargets_;
    std::vector<EntityId> structuralRiskIds_;
    std::vector<EntityId> structuralRiskCacheRoots_;
    std::uint64_t structuralRiskCacheRevision_{
        std::numeric_limits<std::uint64_t>::max()};
    bool structuralRiskCacheValid_{};
    std::optional<BuildingInstance> pendingSoldBuildingVisual_;
    std::uint8_t pendingSoldWallConnections_{};
    std::optional<PlayerWeapon> pendingWeaponSelection_;
    bool pendingWeaponUpgrade_{};
    bool pendingBombThrow_{};
    bool pendingInteract_{};
    std::optional<RerollChestCommand> pendingChestReroll_;
    bool pendingRevealChest_{};
    bool pendingDefeatAllEnemies_{};
    bool pendingToggleInvulnerability_{};
    bool pendingDamageCore_{};
    bool pendingSpawnEnemy_{};
    bool pendingChainLightning_{};
    bool enemySpawnMenuVisible_{};
    bool itemGrantMenuVisible_{};
    int debugSpawnCount_{50};
    LootUpgradeEffect debugGrantLootEffect_{
        LootUpgradeEffect::Apple};
    LootRarity debugGrantLootRarity_{LootRarity::Common};
    int debugGrantLootCount_{1};
    struct PendingLootGrant {
        LootUpgradeEffect effect;
        LootRarity rarity;
        int count;
    };
    std::optional<PendingLootGrant> pendingLootGrant_;
    std::optional<ToggleGateCommand> pendingGateToggle_;
    std::optional<RotatePlacedBuildingCommand>
        pendingPlacedBuildingRotation_;
    std::string statusMessage_;
    double statusMessageRemaining_{};
    double lootDescriptionRemaining_{};
    std::vector<PresentationEffect> effects_;
    std::unordered_map<std::uint64_t, float>
        enemyHitFlashById_;
    std::unordered_map<std::uint64_t, float>
        enemyBurnAmountById_;
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
        EliteAffixMask eliteAffixes{};
        Vec3 position;
        double surfaceHeightOffset;
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
    std::vector<std::pair<float, std::size_t>>
        shadowCandidateBuffer_;
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
    double crystalHudBounceRemaining_{};
    double coinHudBounceRemaining_{};
    double displayedInsight_{-1.0};
    double insightPulseRemaining_{};
    double insightPulseDuration_{0.55};
    double insightGainAmount_{};
    double insightGainRemaining_{};
    double insightGainDuration_{0.8};
    double insightPointSequenceRemaining_{};
    double insightPointSequenceDuration_{0.48};
    double insightAnimationBefore_{};
    double insightAnimationAfter_{};
    double insightAnimationRequirement_{100.0};
    int insightAnimationPoints_{};
    int pendingInsightPointNotification_{};
    std::unordered_map<std::string, double>
        objectiveProgressCache_;
    std::string objectivePulseId_;
    double objectivePulseRemaining_{};
    double objectivePulseDuration_{0.55};
    std::optional<EntityId> hoveredResource_;
    std::optional<EntityId> interactionResourceAim_;
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
    float lowHealthEffect_{};
    double iceImpactFlashRemaining_{};
    double iceWandRecoilRemaining_{};
    double iceWandRecoilDuration_{0.20};
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
    EliteAffix debugSpawnEliteAffix_{EliteAffix::None};
    bool slowMotion_{};
    bool showColliders_{};
    bool showFlowField_{};
    bool showSpatialHash_{};
    bool showTerrainWireframe_{};
    bool performanceOverlayVisible_{};
    bool objectiveDebugMenuVisible_{};
    AppPerformanceStats performanceStats_{};
    PerformanceRecorder performanceRecorder_{};
    bool performanceLoggingApplied_{};
    std::uint64_t performanceFrameIndex_{};
    bool buildModePieVisible_{};
    Vector2 buildModePieDirection_{};
    std::optional<ActionMode>
        buildModePieChoice_;
    ActionMode actionMode_{ActionMode::Tools};
    ActionMode previousActionMode_{ActionMode::Weapons};
    PlayerWeapon lastToolSelection_{PlayerWeapon::BareHands};
    PlayerWeapon lastWeaponSelection_{PlayerWeapon::Club};
    BuildingType lastBuildingSelection_{
        BuildingType::Wall};
    float buildHotbarSelectionPosition_{1.0F};
    float buildHotbarSelectionAlpha_{};
    float foundationHotbarSelectionPosition_{};
    float foundationHotbarSelectionAlpha_{};
    float weaponHotbarSelectionPosition_{};
    float weaponHotbarSelectionAlpha_{1.0F};
    float minimapExpansion_{};
    bool minimapHidden_{};
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
    std::optional<std::uint64_t> modularDragPreviewKey_;
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
