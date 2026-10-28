#pragma once

#include "buildings/BuildGrid.hpp"
#include "buildings/ModularBuildingConstants.hpp"
#include "world/TerrainHeightfield.hpp"

#include <array>
#include <cmath>
#include <optional>

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
    InsufficientResources,
    CoreLevelRequired,
    OutsideCoreArea,
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
    std::optional<EntityId> existingFrame;

    // Geometry remains useful for an invalid (red) preview. Validation may
    // reject range/cost/overlap after all four supports have been computed.
    [[nodiscard]] bool hasGeometry() const {
        if (!std::isfinite(floorHeight) ||
            supports[3].top.x <= supports[0].top.x ||
            supports[3].top.z <= supports[0].top.z) return false;
        for (const auto& support : supports) {
            if (!std::isfinite(support.top.x) || !std::isfinite(support.top.y) ||
                !std::isfinite(support.top.z) || !std::isfinite(support.bottom.y) ||
                !std::isfinite(support.length) || std::abs(support.top.y - floorHeight) > 1e-5)
                return false;
        }
        return true;
    }

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
    [[nodiscard]] PlatformFramePlacement
    validateGroundPlatformFrameAt(
        GridCoord anchor, double floorHeight,
        Vec3 playerPosition) const;
    [[nodiscard]] PlatformFramePlacement
    validateAutomaticBuildingFoundationAt(
        GridCoord anchor, double floorHeight,
        Vec3 playerPosition) const;
    [[nodiscard]] double stableGroundPlatformHeight(
        GridCoord anchor, double heightStep) const;

  private:
    [[nodiscard]] PlatformFramePlacement
    validateGroundPlatformFrameAtImpl(
        GridCoord anchor, double floorHeight,
        Vec3 playerPosition, bool snapAnchor) const;

    const TerrainHeightfield& terrain_;
    const BuildGrid& grid_;
};

} // namespace ian
