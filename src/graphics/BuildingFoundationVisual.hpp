#pragma once

#include "buildings/FoundationSystem.hpp"

namespace ian {

// Legacy buildings without a real modular foundation still draw a 2x2 pallet.
[[nodiscard]] inline std::optional<PlatformFrameInstance> makeBuildingFoundationVisual(
    BuildingType type, GridPosition position, double topHeight, double bottomHeight,
    double cellSize, EntityId id = {}) {
    if (topHeight - bottomHeight <= 0.025) return std::nullopt;
    const Vec3 center = buildingWorldPosition(type, position);
    const double halfExtent = buildingFootprintHalfExtent(type);
    const double minimumX = std::floor((center.x - halfExtent) / cellSize) * cellSize;
    const double maximumX = minimumX + PlatformFrameWidthCells * cellSize;
    const double minimumZ = std::floor((center.z - halfExtent) / cellSize) * cellSize;
    const double maximumZ = minimumZ + PlatformFrameWidthCells * cellSize;
    const auto support = [topHeight, bottomHeight](double x, double z) {
        return FoundationSupport{
            .top = {x, topHeight, z}, .bottom = {x, bottomHeight, z},
            .length = topHeight - bottomHeight,
        };
    };
    return PlatformFrameInstance{
        .id = id,
        .anchor = {
            static_cast<int>(std::floor(minimumX / cellSize)),
            static_cast<int>(std::lround(topHeight / cellSize)),
            static_cast<int>(std::floor(minimumZ / cellSize)),
        },
        .floorHeight = topHeight,
        .storey = -1,
        .supports = {
            support(minimumX, minimumZ), support(maximumX, minimumZ),
            support(minimumX, maximumZ), support(maximumX, maximumZ),
        },
    };
}

// Use the exact support chosen by placement, including alternative anchors at
// the edge of an existing platform. The trap's own occupied cell remains 1x1.
[[nodiscard]] inline std::optional<PlatformFrameInstance> makeBuildingFoundationPreviewVisual(
    const BuildingPreview& preview, double cellSize) {
    if (preview.foundationPreview) {
        const auto& placement = *preview.foundationPreview;
        if (!placement.valid()) return std::nullopt;
        return PlatformFrameInstance{
            .id = {}, .anchor = placement.anchor, .floorHeight = placement.floorHeight,
            .storey = placement.storey, .supports = placement.supports,
        };
    }
    if (preview.platformStorey >= 0) return std::nullopt;
    return makeBuildingFoundationVisual(preview.type, preview.gridPosition,
        preview.baseHeight, preview.foundationBottomHeight, cellSize);
}

[[nodiscard]] inline Vec3 platformFrameVisualCenter(const PlatformFrameInstance& frame) {
    return {(frame.supports[0].top.x + frame.supports[3].top.x) * .5,
        frame.floorHeight, (frame.supports[0].top.z + frame.supports[3].top.z) * .5};
}

} // namespace ian
