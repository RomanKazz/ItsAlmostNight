#pragma once

#include "app/App.hpp"
#include "buildings/PlacementLine.hpp"
#include "world/WorldConfig.hpp"

#include <array>
#include <optional>
#include <vector>

namespace ian::app_detail {

[[nodiscard]] EnemyModelVisual enemyModelVisual(
    EnemyType type);
[[nodiscard]] EnemyAnimationVisual enemyAnimationVisual(
    const EnemyInstance& enemy);
[[nodiscard]] float enemyVisualScale(EnemyType type);
[[nodiscard]] Vector3 enemyRenderPosition(
    const EnemyInstance& enemy);
[[nodiscard]] float enemyAnimationSeconds(
    const EnemyInstance& enemy, double elapsedSeconds);
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
[[nodiscard]] std::uint8_t wallConnectionToward(
    GridPosition from, GridPosition to);
[[nodiscard]] Vector2 repelInvalidPreview(
    Vector2 center, const BuildingPreview& preview,
    Vec3 playerPosition);

[[nodiscard]] float cannonYaw(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building);
[[nodiscard]] float towerYaw(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building);
[[nodiscard]] float cannonPitch(
    const SimulationSnapshot& snapshot,
    const BuildingInstance& building);
[[nodiscard]] std::optional<EntityId> preciseBuildingAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot);
[[nodiscard]] std::optional<EntityId> preciseResourceAim(
    Renderer& renderer,
    const SimulationSnapshot& snapshot);
[[nodiscard]] std::optional<EntityId>
preciseModularBuildingAim(
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
[[nodiscard]] std::optional<PlatformFrameInstance>
automaticBuildingFoundation(
    BuildingType type, GridPosition position,
    double topHeight, double bottomHeight,
    double cellSize, EntityId id = {});
void drawBuildingTacticalOverlay(
    const SimulationSnapshot& snapshot);
[[nodiscard]] GameBalance loadAppBalance();
[[nodiscard]] MapDefinition loadAppMap();
[[nodiscard]] WorldConfig loadAppWorldConfig();
[[nodiscard]] std::array<EnvironmentProfile, 4>
loadAppEnvironment();

} // namespace ian::app_detail
