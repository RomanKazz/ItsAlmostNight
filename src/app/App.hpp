#pragma once

#include "audio/AudioSystem.hpp"
#include "core/FixedStep.hpp"
#include "game/Simulation.hpp"
#include "graphics/EnvironmentSystem.hpp"
#include "graphics/ModularBuildingRenderer.hpp"
#include "graphics/Renderer.hpp"
#include "presentation/PresentationTypes.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudRenderer.hpp"
#include "ui/TargetHealthBar.hpp"

#include <optional>
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
    void updateModularPlacementPreview(
        const SimulationSnapshot& snapshot);
    void beginModularPlacementDrag();
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
    void drawEnemySpawnMenu();
    void drawBuildModePie() const;
    void addEffect(PresentationEffectType type, Vec3 position,
                   double duration, float scale = 1.0F,
                   std::optional<EntityId> entityId =
                       std::nullopt);
    void addCameraShake(double duration, double strength);
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
    ModularBuildingRenderer modularBuildingRenderer_;
    std::optional<Renderer> renderer_;
    GameUi ui_;
    bool pendingStartFromUi_{};
    bool graphicsPanelWasVisible_{};
    int graphicsPanelTab_{};
    TargetHealthBar targetHealthBar_;
    PlayerCommand input_;
    double pendingYaw_{};
    double pendingPitch_{};
    bool pendingJump_{};
    bool pendingPickaxe_{};
    bool pendingRifleShot_{};
    std::optional<BuildingType> pendingBuildingSelection_;
    bool pendingBuildingCancel_{};
    std::optional<PlaceBuildingCommand> pendingBuildingPlacement_;
    std::optional<GridPosition> wallDragStart_;
    std::optional<GridPosition> wallDragEnd_;
    std::optional<BuildingType> placementDragType_;
    std::optional<BuildingPlatformSurface>
        placementDragSurface_;
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
    std::optional<BuildingInstance> pendingSoldBuildingVisual_;
    std::uint8_t pendingSoldWallConnections_{};
    bool pendingWeaponToggle_{};
    bool pendingWeaponUpgrade_{};
    bool pendingBombThrow_{};
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
    bool foundationBuildMode_{};
    ModularBuildPiece modularBuildPiece_{
        ModularBuildPiece::PlatformFrame};
    std::optional<PlatformFramePlacement>
        platformFramePreview_;
    std::optional<PlatformFrameColumnPlacement>
        platformFrameColumnPreview_;
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
    std::optional<GridCoord> modularRearmAnchor_;
    bool modularVerticalRearmBlocked_{};
    std::optional<GridCoord> modularDragStart_;
    std::optional<GridCoord> modularDragEnd_;
    std::optional<int> modularDragStorey_;
    std::optional<double> modularDragFloorHeight_;
    std::optional<double> modularDragPlaneHeight_;
    std::optional<Rotation> modularDragRotation_;
    std::optional<ModularBuildPiece> modularDragPiece_;
    std::vector<Vec3> modularDragHits_;
    std::vector<PlatformFramePlacement>
        modularPlatformDragPreviews_;
    std::vector<PlatformFrameColumnPlacement>
        modularPlatformColumnDragPreviews_;
    std::vector<WallPlacement>
        modularWallDragPreviews_;
    std::vector<RampPlacement>
        modularRampDragPreviews_;
    bool hideBottomHud_{};
    double simulationTickMilliseconds_{};
    double peakSimulationTickMilliseconds_{};
};

} // namespace ian
