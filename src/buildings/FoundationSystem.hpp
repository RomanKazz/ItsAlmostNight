#pragma once

#include "buildings/BuildingSystem.hpp"
#include "buildings/ModularBuildingConstants.hpp"
#include "buildings/PlacementValidator.hpp"
#include "buildings/StructuralSupportGraph.hpp"
#include "buildings/SupportSystem.hpp"

#include <optional>
#include <span>
#include <vector>

namespace ian {

struct PlatformFrameInstance {
    EntityId id;
    GridCoord anchor;
    double floorHeight{};
    int storey{};
    double health{PlatformFrameMaxHealth};
    double maxHealth{PlatformFrameMaxHealth};
    std::array<FoundationSupport, 4> supports{};
    std::array<std::uint32_t, 4> supportIds{};
    StructuralSupportState supportState{
        StructuralSupportState::Supported};
};

struct WallPlacement {
    ModularPlacementError error{
        ModularPlacementError::None};
    GridCoord anchor;
    Rotation rotation{Rotation::Deg0};
    double bottomHeight{};
    double topHeight{};
    int storey{};

    [[nodiscard]] bool valid() const {
        return error == ModularPlacementError::None;
    }
};

struct WallInstance {
    EntityId id;
    GridCoord anchor;
    Rotation rotation{Rotation::Deg0};
    double bottomHeight{};
    double topHeight{};
    int storey{};
    double health{ModularWallMaxHealth};
    double maxHealth{ModularWallMaxHealth};
    StructuralSupportState supportState{
        StructuralSupportState::Supported};
};

struct RampPlacement {
    ModularPlacementError error{
        ModularPlacementError::None};
    GridCoord anchor;
    Rotation rotation{Rotation::Deg0};
    double bottomHeight{};
    double topHeight{};
    int targetStorey{};

    [[nodiscard]] bool valid() const {
        return error == ModularPlacementError::None;
    }
};

struct RampInstance {
    EntityId id;
    GridCoord anchor;
    Rotation rotation{Rotation::Deg0};
    double bottomHeight{};
    double topHeight{};
    int targetStorey{};
    double health{ModularRampMaxHealth};
    double maxHealth{ModularRampMaxHealth};
    StructuralSupportState supportState{
        StructuralSupportState::Supported};
};

struct ModularBuildingDamageResult {
    EntityId id;
    std::optional<PlatformFrameInstance> platformFrame;
    std::optional<WallInstance> wall;
    std::optional<RampInstance> ramp;
    bool destroyed{};
};

struct ModularBuildingRepairResult {
    BuildingActionError error{
        BuildingActionError::None};
    EntityId id;
    std::optional<PlatformFrameInstance> platformFrame;
    std::optional<WallInstance> wall;
    std::optional<RampInstance> ramp;
    ResourceCost cost;
    double repairedHealth{};

    [[nodiscard]] bool valid() const {
        return error == BuildingActionError::None;
    }
};

struct BuildingPlatformSurface {
    double height{};
    double foundationBottomHeight{};
    int storey{};
};

class FoundationSystem {
  public:
    FoundationSystem(
        const TerrainHeightfield& terrain,
        WorldConfig config = WorldConfig::defaults());

    void reset();
    void setMaxHealthMultiplier(double multiplier);
    double restoreHealthFraction(double fraction);
    [[nodiscard]] PlatformFramePlacement
    previewFoundation(
        Vec3 terrainHit, Vec3 playerPosition) const;
    [[nodiscard]] PlatformFramePlacement
    previewFoundationAtHeight(
        Vec3 terrainHit, double floorHeight,
        Vec3 playerPosition) const;
    [[nodiscard]] PlatformFramePlacement
    previewAutomaticBuildingFoundationAtHeight(
        GridCoord anchor, double floorHeight,
        Vec3 playerPosition) const;
    [[nodiscard]] PlatformFramePlacement
    previewFloorPlatform(
        GridCoord anchor, int storey,
        double floorHeight,
        Vec3 playerPosition) const;
    [[nodiscard]] std::optional<PlatformFrameInstance>
    placePlatformFrame(
        const PlatformFramePlacement& placement);
    [[nodiscard]] WallPlacement previewWall(
        Vec3 terrainHit, Vec3 playerPosition,
        Rotation rotation) const;
    [[nodiscard]] std::optional<WallInstance>
    placeWall(const WallPlacement& placement);
    [[nodiscard]] RampPlacement previewRamp(
        Vec3 terrainHit, Vec3 playerPosition,
        Rotation rotation) const;
    [[nodiscard]] std::optional<RampInstance>
    placeRamp(const RampPlacement& placement);
    [[nodiscard]] bool remove(EntityId id);
    [[nodiscard]] std::optional<ModularBuildingDamageResult>
    damage(EntityId id, double amount);
    [[nodiscard]] ModularBuildingRepairResult repair(
        EntityId id, int wood, int stone);
    [[nodiscard]] std::optional<EntityId> raycast(
        Vec3 origin, Vec3 direction,
        double maximumDistance) const;
    [[nodiscard]] std::optional<Vec3>
    raycastPlatformSurface(
        Vec3 origin, Vec3 direction,
        double maximumDistance) const;
    [[nodiscard]] std::optional<BuildingPlatformSurface>
    buildingSurface(
        int minimumCellX, int minimumCellZ,
        int widthCells) const;
    [[nodiscard]] bool buildingFootprintIntersectsPlatform(
        int minimumCellX, int minimumCellZ,
        int widthCells) const;
    [[nodiscard]] std::size_t clear();
    [[nodiscard]] bool updateStructuralSupport(
        double deltaSeconds);
    [[nodiscard]] std::vector<ModularBuildingDamageResult>
    takeCollapsedBuildings();
    void setStructuralCollapseEnabled(bool enabled);
    [[nodiscard]] bool structuralCollapseEnabled() const;
    void setStructuralCollapseDelay(double seconds);
    [[nodiscard]] const StructuralSupportGraph&
    structuralGraph() const;

    [[nodiscard]] std::span<const PlatformFrameInstance>
    platformFrames() const;
    [[nodiscard]] std::span<const WallInstance>
    walls() const;
    [[nodiscard]] std::span<const RampInstance>
    ramps() const;
    [[nodiscard]] const BuildGrid& grid() const;
    [[nodiscard]] const SupportSystem& supportSystem() const;

  private:
    struct FloorSurface {
        EntityId id;
        double height{};
        int storey{};
    };

    [[nodiscard]] std::optional<FloorSurface>
    topFloorAtCell(int x, int z) const;
    [[nodiscard]] const PlatformFrameInstance*
    frameAt(GridCoord anchor, int storey) const;
    void syncStructuralStates();
    [[nodiscard]] bool eraseInstance(
        EntityId id, bool releaseFoundationSupports);

    const TerrainHeightfield& terrain_;
    BuildGrid grid_;
    SupportSystem supports_;
    StructuralSupportGraph structuralGraph_;
    std::vector<PlatformFrameInstance> platformFrames_;
    std::vector<WallInstance> walls_;
    std::vector<RampInstance> ramps_;
    std::vector<ModularBuildingDamageResult>
        collapsedBuildings_;
    std::uint32_t nextIndex_{12000U};
    bool structuralCollapseEnabled_{true};
    double structuralCollapseDelay_{1.5};
    double maxHealthMultiplier_{1.0};
};

} // namespace ian
