#include "buildings/FoundationSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {

namespace {

int rampWidthCells(Rotation rotation) {
    return rotation == Rotation::Deg0 ||
                   rotation == Rotation::Deg180
               ? ModularRampWidthCells
               : ModularRampRunCells;
}

int rampDepthCells(Rotation rotation) {
    return rotation == Rotation::Deg0 ||
                   rotation == Rotation::Deg180
               ? ModularRampRunCells
               : ModularRampWidthCells;
}

GridCoord rampAnchorFromBottom(
    GridCoord bottom, Rotation rotation) {
    if (rotation == Rotation::Deg180) {
        bottom.z -= ModularRampRunCells - 1;
    } else if (rotation == Rotation::Deg90) {
        bottom.x -= ModularRampRunCells - 1;
    }
    return bottom;
}

GridCoord rampBottomCell(
    GridCoord anchor, Rotation rotation) {
    if (rotation == Rotation::Deg180) {
        anchor.z += ModularRampRunCells - 1;
    } else if (rotation == Rotation::Deg90) {
        anchor.x += ModularRampRunCells - 1;
    }
    return anchor;
}

std::array<GridCoord, ModularRampWidthCells>
rampBottomCells(
    GridCoord anchor, Rotation rotation) {
    const GridCoord first =
        rampBottomCell(anchor, rotation);
    std::array<GridCoord, ModularRampWidthCells>
        cells{};
    for (int index = 0;
         index < ModularRampWidthCells; ++index) {
        cells[static_cast<std::size_t>(index)] =
            first;
        if (rotation == Rotation::Deg0 ||
            rotation == Rotation::Deg180) {
            cells[static_cast<std::size_t>(index)].x +=
                index;
        } else {
            cells[static_cast<std::size_t>(index)].z +=
                index;
        }
    }
    return cells;
}

} // namespace

WallPlacement FoundationSystem::previewWall(
    Vec3 terrainHit, Vec3 playerPosition,
    Rotation rotation) const {
    WallPlacement placement{
        .anchor = grid_.worldToGrid(terrainHit),
        .rotation = rotation,
    };
    const WorldConfig& config = grid_.config();
    if (!terrain_.isInside(
            terrainHit.x, terrainHit.z)) {
        placement.error =
            ModularPlacementError::OutOfBounds;
        return placement;
    }
    const Vec3 center = grid_.worldCenter(
        placement.anchor, Footprint::OneByOne);
    if (std::hypot(
            center.x - playerPosition.x,
            center.z - playerPosition.z) >
        config.buildPreviewDistance) {
        placement.error =
            ModularPlacementError::TooFar;
        return placement;
    }
    const auto floor = topFloorAtCell(
        placement.anchor.x,
        placement.anchor.z);
    if (!floor) {
        placement.error =
            ModularPlacementError::NoSupport;
        return placement;
    }
    placement.storey = floor->storey;
    placement.bottomHeight = floor->height;
    placement.topHeight =
        floor->height + modularStoreyHeight(config);
    placement.anchor.yLevel =
        static_cast<int>(std::lround(
            placement.bottomHeight /
            config.verticalGridStep));
    if (placement.storey >= config.maxStoreys) {
        placement.error =
            ModularPlacementError::MaximumStorey;
        return placement;
    }
    const int heightLevels =
        modularStoreyHeightLevels(config);
    if (!grid_.canOccupy(
            placement.anchor,
            Footprint::OneByOne,
            heightLevels,
            OccupancyLayer::Wall)) {
        placement.error =
            ModularPlacementError::Occupied;
        return placement;
    }
    return placement;
}

std::optional<WallInstance>
FoundationSystem::placeWall(
    const WallPlacement& placement) {
    if (!placement.valid()) {
        return std::nullopt;
    }
    WallInstance instance{
        .id = {nextIndex_++, 1U},
        .anchor = placement.anchor,
        .rotation = placement.rotation,
        .bottomHeight = placement.bottomHeight,
        .topHeight = placement.topHeight,
        .storey = placement.storey,
    };
    const int heightLevels =
        modularStoreyHeightLevels(grid_.config());
    if (!grid_.occupy(
            instance.id, instance.anchor,
            Footprint::OneByOne,
            heightLevels,
            OccupancyLayer::Wall)) {
        return std::nullopt;
    }
    const auto source = topFloorAtCell(
        instance.anchor.x, instance.anchor.z);
    if (!source ||
        !structuralGraph_.add(
            instance.id, false,
            std::span<const EntityId>{
                &source->id, 1U})) {
        grid_.release(instance.id);
        return std::nullopt;
    }
    walls_.push_back(instance);
    return instance;
}

RampPlacement FoundationSystem::previewRamp(
    Vec3 terrainHit, Vec3 playerPosition,
    Rotation rotation) const {
    GridCoord bottomCell =
        grid_.worldToGrid(terrainHit);
    bottomCell.x =
        snapPlatformFrameAxis(bottomCell.x);
    bottomCell.z =
        snapPlatformFrameAxis(bottomCell.z);
    RampPlacement placement{
        .anchor = rampAnchorFromBottom(
            bottomCell, rotation),
        .rotation = rotation,
    };
    const WorldConfig& config = grid_.config();
    const int widthCells =
        rampWidthCells(rotation);
    const int depthCells =
        rampDepthCells(rotation);
    const double minimumX =
        placement.anchor.x * config.cellSize;
    const double minimumZ =
        placement.anchor.z * config.cellSize;
    const double maximumX =
        (placement.anchor.x + widthCells) *
        config.cellSize;
    const double maximumZ =
        (placement.anchor.z + depthCells) *
        config.cellSize;
    if (!terrain_.isInside(minimumX, minimumZ) ||
        !terrain_.isInside(maximumX, maximumZ)) {
        placement.error =
            ModularPlacementError::OutOfBounds;
        return placement;
    }
    const Vec3 center{
        (minimumX + maximumX) * 0.5,
        0.0,
        (minimumZ + maximumZ) * 0.5,
    };
    if (std::hypot(
            center.x - playerPosition.x,
            center.z - playerPosition.z) >
        config.buildPreviewDistance) {
        placement.error =
            ModularPlacementError::TooFar;
        return placement;
    }
    const auto bottomCells =
        rampBottomCells(
            placement.anchor, rotation);
    auto floor = topFloorAtCell(
        bottomCells[0].x, bottomCells[0].z);
    if (!floor) {
        placement.error =
            ModularPlacementError::NoSupport;
        return placement;
    }
    for (std::size_t index = 1;
         index < bottomCells.size(); ++index) {
        const auto supportedFloor = topFloorAtCell(
            bottomCells[index].x,
            bottomCells[index].z);
        if (!supportedFloor ||
            std::abs(
                floor->height -
                supportedFloor->height) > 1e-6 ||
            floor->storey != supportedFloor->storey) {
            placement.error =
                ModularPlacementError::NoSupport;
            return placement;
        }
    }
    placement.targetStorey = floor->storey + 1;
    placement.bottomHeight = floor->height;
    placement.topHeight =
        floor->height + modularStoreyHeight(config);
    if (placement.targetStorey >=
        config.maxStoreys) {
        placement.error =
            ModularPlacementError::MaximumStorey;
        return placement;
    }
    placement.anchor.yLevel =
        static_cast<int>(std::lround(
            placement.bottomHeight /
            config.verticalGridStep)) +
        1;
    const int heightLevels = std::max(
        1,
        modularStoreyHeightLevels(config) - 1);
    if (!grid_.canOccupyRectangle(
            placement.anchor, widthCells,
            depthCells, heightLevels,
            OccupancyLayer::Volume)) {
        placement.error =
            ModularPlacementError::Occupied;
        return placement;
    }
    return placement;
}

std::optional<RampInstance>
FoundationSystem::placeRamp(
    const RampPlacement& placement) {
    if (!placement.valid()) {
        return std::nullopt;
    }
    RampInstance instance{
        .id = {nextIndex_++, 1U},
        .anchor = placement.anchor,
        .rotation = placement.rotation,
        .bottomHeight = placement.bottomHeight,
        .topHeight = placement.topHeight,
        .targetStorey =
            placement.targetStorey,
    };
    const int heightLevels = std::max(
        1,
        modularStoreyHeightLevels(
            grid_.config()) -
            1);
    const int widthCells =
        rampWidthCells(instance.rotation);
    const int depthCells =
        rampDepthCells(instance.rotation);
    if (!grid_.occupyRectangle(
            instance.id, instance.anchor,
            widthCells, depthCells,
            heightLevels,
            OccupancyLayer::Volume)) {
        return std::nullopt;
    }
    const auto bottomCells =
        rampBottomCells(
            instance.anchor, instance.rotation);
    std::vector<EntityId> sources;
    for (const GridCoord cell : bottomCells) {
        const auto source =
            topFloorAtCell(cell.x, cell.z);
        if (!source) {
            grid_.release(instance.id);
            return std::nullopt;
        }
        if (std::find(
                sources.begin(), sources.end(),
                source->id) == sources.end()) {
            sources.push_back(source->id);
        }
    }
    if (sources.empty() ||
        !structuralGraph_.add(
            instance.id, false,
            sources)) {
        grid_.release(instance.id);
        return std::nullopt;
    }
    ramps_.push_back(instance);
    return instance;
}

} // namespace ian
