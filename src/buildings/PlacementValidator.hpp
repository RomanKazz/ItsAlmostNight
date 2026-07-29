#pragma once

#include "buildings/BuildGrid.hpp"
#include "buildings/ModularBuildingConstants.hpp"
#include "world/TerrainHeightfield.hpp"

#include <array>

namespace ian {

enum class ModularPlacementError {
    None,
    Occupied,
    OutOfBounds,
    TooFar,
    SupportTooLong,
    PlayerOverlap,
    TerrainIntersection,
    MaximumStorey,
    NoSupport,
    ResourceBlocked,
};

struct FoundationSupport {
    Vec3 top;
    Vec3 bottom;
    double length{};
};

struct PlatformFramePlacement {
    ModularPlacementError error{
        ModularPlacementError::None};
    GridCoord anchor;
    double floorHeight{};
    int storey{};
    std::array<FoundationSupport, 4> supports{};

    [[nodiscard]] bool valid() const {
        return error == ModularPlacementError::None;
    }
};

class PlacementValidator {
  public:
    PlacementValidator(
        const TerrainHeightfield& terrain,
        const BuildGrid& grid);

    [[nodiscard]] PlatformFramePlacement
    validateGroundPlatformFrame(
        Vec3 terrainHit, Vec3 playerPosition) const;

  private:
    const TerrainHeightfield& terrain_;
    const BuildGrid& grid_;
};

} // namespace ian
