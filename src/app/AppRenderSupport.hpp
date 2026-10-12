#pragma once

#include "app/App.hpp"
#include "graphics/SplitterPresentation.hpp"
#include "buildings/PlacementLine.hpp"
#include "world/WorldConfig.hpp"

#include <array>
#include <functional>
#include <optional>
#include <vector>

namespace ian::app_detail {

struct LootItemVisual {
    Vector3 position{};
    float rotation{};
    float scale{};
    Color tint{WHITE};
};

struct ResourceAimHit {
    EntityId id;
    Vec3 position;
};

[[nodiscard]] LootItemVisual lootItemVisual(
    const SimulationSnapshot& snapshot,
    const LootChestInstance& chest);
[[nodiscard]] std::optional<LootItemVisual>
rerollTargetLootItemVisual(
    const SimulationSnapshot& snapshot,
    const LootChestInstance& chest);

[[nodiscard]] EnemyModelVisual enemyModelVisual(
    EnemyType type);
[[nodiscard]] EnemyModelVisual enemyModelVisual(const EnemyInstance& enemy);
[[nodiscard]] EnemyAnimationVisual enemyAnimationVisual(
    const EnemyInstance& enemy);
[[nodiscard]] float enemyVisualScale(EnemyType type);
[[nodiscard]] float enemyHitScale(
    const EnemyInstance& enemy);
[[nodiscard]] SplitterPose enemySplitPose(
    const EnemyInstance& enemy, double subTickSeconds = 0.0);
[[nodiscard]] double enemySplitSubTickSeconds(
    const SimulationSnapshot& snapshot, double interpolationAlpha);
[[nodiscard]] Vector3 enemyRenderPosition(
    const EnemyInstance& enemy, double subTickSeconds = 0.0);
[[nodiscard]] float enemyAnimationSeconds(
    const EnemyInstance& enemy, double elapsedSeconds);
[[nodiscard]] float spikeTrapAnimationSeconds(
    const SimulationSnapshot& snapshot, EntityId id);
void drawCentered(
    const char* text, int y, int fontSize, Color color);
[[nodiscard]] const char* upgradeErrorMessage(
    UpgradeError error);
[[nodiscard]] const char* buildingActionErrorMessage(
    BuildingActionError error);
[[nodiscard]] const char* weaponUpgradeErrorMessage(
    WeaponUpgradeError error);
[[nodiscard]] bool acceptsGameplayInput(RunState state);

[[nodiscard]] std::vector<GridPosition> placementLine(
    BuildingType type, GridPosition start,
    GridPosition end,
    std::optional<PlacementLineAxis> axis =
        std::nullopt);
[[nodiscard]] std::vector<GridPosition> placementGestureLine(
    BuildingType type, GridPosition start,
    GridPosition aimedEnd, bool dragExtended,
    std::optional<PlacementLineAxis> axis =
        std::nullopt);
[[nodiscard]] std::uint8_t wallConnectionToward(
    GridPosition from, GridPosition to);
[[nodiscard]] Vector2 repelInvalidPreview(
    Vector2 center, const BuildingPreview& preview,
    Vec3 playerPosition);

[[nodiscard]] float cannonYaw(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building, float interpolationAlpha = 1.0F);
[[nodiscard]] float cannonBaseYaw(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building);
[[nodiscard]] float towerYaw(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building, float interpolationAlpha = 1.0F);
[[nodiscard]] float towerBaseYaw(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building);
[[nodiscard]] float towerPitch(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building, float interpolationAlpha = 1.0F);
[[nodiscard]] float cannonPitch(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building, float interpolationAlpha = 1.0F);
[[nodiscard]] bool cannonLoaded(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building);
[[nodiscard]] std::optional<EntityId> preciseBuildingAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot,
    const std::function<float(EntityId, bool)>&
        gateOpenAmount = {});
[[nodiscard]] std::optional<ResourceAimHit> preciseResourceAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot,
    PlayerWeapon weapon);
[[nodiscard]] std::optional<EntityId>
preciseModularBuildingAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot);
[[nodiscard]] std::optional<EntityId>
preciseWorldLandmarkAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot);
[[nodiscard]] Vector3 colorToVector(Color color);

[[nodiscard]] float smoothstep(
    float edge0, float edge1, float value);
void drawBuildGrid(
    Vector3 playerPosition, double worldLimit,
    const TerrainHeightfield& terrain);
[[nodiscard]] Color placementColor(
    PlacementError error, bool fill);
[[nodiscard]] bool placementPreviewObstructed(
    PlacementError error);
void drawPlacementFootprint(
    const BuildingPreview& preview, Vector2 visualCenter,
    float visualYaw);
void drawPlacementPreviewGeometry(
    Renderer& renderer, BuildingType type, Vector3 position,
    float yawRadians, std::uint8_t rotation,
    std::uint8_t wallConnections = 0U);
[[nodiscard]] std::optional<PlatformFrameInstance>
automaticBuildingFoundation(
    BuildingType type, GridPosition position,
    double topHeight, double bottomHeight,
    double cellSize, EntityId id = {});
void drawBuildingTacticalOverlay(
    Renderer& renderer, const SimulationSnapshot& snapshot);
[[nodiscard]] GameBalance loadAppBalance();
[[nodiscard]] MapDefinition loadAppMap();
[[nodiscard]] WorldConfig loadAppWorldConfig();
[[nodiscard]] InsightConfig loadAppInsightConfig();
[[nodiscard]] std::vector<ObjectiveDefinition> loadAppObjectives();
[[nodiscard]] std::array<EnvironmentProfile, 4>
loadAppEnvironment();

} // namespace ian::app_detail
