#include "buildings/FoundationSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

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

GridCoord rampAnchorFromSupport(
    GridCoord support, Rotation rotation) {
    // The supporting PlatformFrame sits directly below the
    // ramp's low half. The ramp then rises out past its edge.
    if (rotation == Rotation::Deg180) {
        support.z -=
            ModularRampRunCells -
            PlatformFrameWidthCells;
    } else if (rotation == Rotation::Deg90) {
        support.x -=
            ModularRampRunCells -
            PlatformFrameWidthCells;
    }
    return support;
}

GridCoord rampSupportCell(
    GridCoord anchor, Rotation rotation) {
    if (rotation == Rotation::Deg180) {
        anchor.z += ModularRampRunCells - 1;
    } else if (rotation == Rotation::Deg90) {
        anchor.x += ModularRampRunCells - 1;
    }
    return anchor;
}

std::array<GridCoord, ModularRampWidthCells>
rampSupportCells(
    GridCoord anchor, Rotation rotation) {
    const GridCoord first =
        rampSupportCell(anchor, rotation);
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

std::uint64_t entityKey(EntityId id) {
    return
        (static_cast<std::uint64_t>(id.generation) << 32U) |
        static_cast<std::uint64_t>(id.index);
}

bool validSupportState(StructuralSupportState state) {
    return state == StructuralSupportState::Supported ||
           state == StructuralSupportState::Unsupported;
}

bool validStructureHealth(double health, double maxHealth) {
    return std::isfinite(health) && std::isfinite(maxHealth) &&
           health > 0.0 && maxHealth > 0.0 &&
           health <= maxHealth;
}

} // namespace

bool FoundationSystem::restoreStructures(
    std::span<const PlatformFrameInstance> platformFrames,
    std::span<const WallInstance> walls,
    std::span<const RampInstance> ramps) {
    constexpr std::size_t MaximumRestoredStructures = 4096U;
    if (platformFrames.size() + walls.size() + ramps.size() >
        MaximumRestoredStructures) {
        return false;
    }

    FoundationSystem restored{terrain_, grid_.config()};
    restored.maxHealthMultiplier_ = maxHealthMultiplier_;
    restored.structuralCollapseEnabled_ = structuralCollapseEnabled_;
    restored.structuralCollapseDelay_ = structuralCollapseDelay_;

    const WorldConfig& config = grid_.config();
    const double maximumCoordinate =
        config.terrainWorldSize * 0.5 +
        static_cast<double>(ModularRampRunCells) * config.cellSize;
    const double maximumHeight =
        std::max(100.0,
                 config.terrainBoundaryRiseHeight +
                     config.terrainAmplitude +
                     static_cast<double>(config.maxStoreys + 1) *
                         modularStoreyHeight(config));
    const auto validAnchor = [&](GridCoord anchor) {
        const double x = static_cast<double>(anchor.x) * config.cellSize;
        const double z = static_cast<double>(anchor.z) * config.cellSize;
        return std::abs(x) <= maximumCoordinate &&
               std::abs(z) <= maximumCoordinate &&
               std::abs(static_cast<double>(anchor.yLevel) *
                        config.verticalGridStep) <= maximumHeight;
    };
    const auto validId = [](EntityId id) {
        return id.index >= 12000U &&
               id.index <
                   std::numeric_limits<std::uint32_t>::max() - 4096U &&
               id.generation != 0U;
    };
    const auto validRotation = [](Rotation rotation) {
        return static_cast<unsigned int>(rotation) <=
            static_cast<unsigned int>(Rotation::Deg270);
    };

    std::unordered_set<std::uint64_t> ids;
    ids.reserve(platformFrames.size() + walls.size() + ramps.size());
    std::uint32_t maximumIndex = 11999U;
    const auto recordId = [&](EntityId id) {
        if (!validId(id) || !ids.insert(entityKey(id)).second) {
            return false;
        }
        maximumIndex = std::max(maximumIndex, id.index);
        return true;
    };

    for (const PlatformFrameInstance& frame : platformFrames) {
        if (!recordId(frame.id) || !validAnchor(frame.anchor) ||
            frame.storey < 0 || frame.storey >= config.maxStoreys ||
            !std::isfinite(frame.floorHeight) ||
            std::abs(frame.floorHeight) > maximumHeight ||
            frame.anchor.yLevel != static_cast<int>(std::lround(
                frame.floorHeight / config.verticalGridStep)) ||
            !validStructureHealth(frame.health, frame.maxHealth) ||
            !validSupportState(frame.supportState)) {
            return false;
        }
    }
    for (const WallInstance& wall : walls) {
        if (!recordId(wall.id) || !validAnchor(wall.anchor) ||
            !validRotation(wall.rotation) ||
            wall.storey < 0 || wall.storey >= config.maxStoreys ||
            !std::isfinite(wall.bottomHeight) ||
            !std::isfinite(wall.topHeight) ||
            std::abs(wall.bottomHeight) > maximumHeight ||
            std::abs(wall.topHeight - wall.bottomHeight -
                     modularStoreyHeight(config)) > 1e-6 ||
            wall.anchor.yLevel != static_cast<int>(std::lround(
                wall.bottomHeight / config.verticalGridStep)) ||
            !validStructureHealth(wall.health, wall.maxHealth) ||
            !validSupportState(wall.supportState)) {
            return false;
        }
    }
    for (const RampInstance& ramp : ramps) {
        if (!recordId(ramp.id) || !validAnchor(ramp.anchor) ||
            !validRotation(ramp.rotation) ||
            ramp.targetStorey <= 0 ||
            ramp.targetStorey >= config.maxStoreys ||
            !std::isfinite(ramp.bottomHeight) ||
            !std::isfinite(ramp.topHeight) ||
            std::abs(ramp.bottomHeight) > maximumHeight ||
            std::abs(ramp.topHeight - ramp.bottomHeight -
                     modularStoreyHeight(config)) > 1e-6 ||
            ramp.anchor.yLevel != static_cast<int>(std::lround(
                ramp.bottomHeight / config.verticalGridStep)) + 1 ||
            !validStructureHealth(ramp.health, ramp.maxHealth) ||
            !validSupportState(ramp.supportState)) {
            return false;
        }
    }

    const auto floorSource = [&restored](
        int x, int z, int storey, double height)
        -> const PlatformFrameInstance* {
        const auto source = std::find_if(
            restored.platformFrames_.begin(),
            restored.platformFrames_.end(),
            [=](const PlatformFrameInstance& frame) {
                return frame.storey == storey &&
                    std::abs(frame.floorHeight - height) <= 1e-6 &&
                    x >= frame.anchor.x &&
                    x < frame.anchor.x + PlatformFrameWidthCells &&
                    z >= frame.anchor.z &&
                    z < frame.anchor.z + PlatformFrameWidthCells;
            });
        return source == restored.platformFrames_.end()
            ? nullptr : &*source;
    };

    for (int storey = 0; storey < config.maxStoreys; ++storey) {
        for (const PlatformFrameInstance& saved : platformFrames) {
            if (saved.storey != storey) {
                continue;
            }
            PlatformFrameInstance frame = saved;
            PlatformFramePlacement placement{
                .anchor = frame.anchor,
                .floorHeight = frame.floorHeight,
                .storey = frame.storey,
            };
            const double minimumX =
                static_cast<double>(frame.anchor.x) * config.cellSize;
            const double minimumZ =
                static_cast<double>(frame.anchor.z) * config.cellSize;
            const double maximumX = minimumX +
                PlatformFrameWidthCells * config.cellSize;
            const double maximumZ = minimumZ +
                PlatformFrameWidthCells * config.cellSize;
            const std::array<Vec3, 4> corners{{
                {minimumX, frame.floorHeight, minimumZ},
                {maximumX, frame.floorHeight, minimumZ},
                {minimumX, frame.floorHeight, maximumZ},
                {maximumX, frame.floorHeight, maximumZ},
            }};
            for (std::size_t index = 0; index < corners.size(); ++index) {
                const double bottomHeight = storey == 0
                    ? terrain_.getHeight(corners[index].x, corners[index].z)
                    : frame.floorHeight - modularStoreyHeight(config);
                placement.supports[index] = {
                    .top = corners[index],
                    .bottom = {corners[index].x, bottomHeight,
                               corners[index].z},
                    .length = frame.floorHeight - bottomHeight,
                };
            }
            if (!restored.grid_.occupy(
                    frame.id, frame.anchor, Footprint::TwoByTwo, 1,
                    OccupancyLayer::Floor)) {
                return false;
            }
            frame.supports = placement.supports;
            frame.supportIds = restored.supports_.acquire(placement);
            const PlatformFrameInstance* previous = storey > 0
                ? restored.frameAt(frame.anchor, storey - 1)
                : nullptr;
            const bool hasExpectedSupport = storey == 0 || previous != nullptr;
            if (!hasExpectedSupport &&
                saved.supportState == StructuralSupportState::Supported) {
                return false;
            }
            const std::span<const EntityId> sources = previous
                ? std::span<const EntityId>{&previous->id, 1U}
                : std::span<const EntityId>{};
            if (!restored.structuralGraph_.add(
                    frame.id, storey == 0, sources)) {
                return false;
            }
            restored.platformFrames_.push_back(frame);
        }

        for (const WallInstance& saved : walls) {
            if (saved.storey != storey) {
                continue;
            }
            if (!restored.grid_.occupy(
                    saved.id, saved.anchor, Footprint::OneByOne,
                    modularStoreyHeightLevels(config),
                    OccupancyLayer::Wall)) {
                return false;
            }
            const PlatformFrameInstance* source = floorSource(
                saved.anchor.x, saved.anchor.z,
                saved.storey, saved.bottomHeight);
            if (source == nullptr &&
                saved.supportState == StructuralSupportState::Supported) {
                return false;
            }
            const std::span<const EntityId> sources = source
                ? std::span<const EntityId>{&source->id, 1U}
                : std::span<const EntityId>{};
            if (!restored.structuralGraph_.add(
                    saved.id, false, sources)) {
                return false;
            }
            restored.walls_.push_back(saved);
        }

        for (const RampInstance& saved : ramps) {
            if (saved.targetStorey != storey + 1) {
                continue;
            }
            const int widthCells = rampWidthCells(saved.rotation);
            const int depthCells = rampDepthCells(saved.rotation);
            if (!restored.grid_.occupyRectangle(
                    saved.id, saved.anchor, widthCells, depthCells,
                    std::max(1, modularStoreyHeightLevels(config) - 1),
                    OccupancyLayer::Volume)) {
                return false;
            }
            std::vector<EntityId> sources;
            for (const GridCoord cell :
                 rampSupportCells(saved.anchor, saved.rotation)) {
                const PlatformFrameInstance* source = floorSource(
                    cell.x, cell.z, storey, saved.bottomHeight);
                if (source != nullptr &&
                    std::ranges::find(sources, source->id) ==
                        sources.end()) {
                    sources.push_back(source->id);
                }
            }
            if (sources.empty() &&
                saved.supportState == StructuralSupportState::Supported) {
                return false;
            }
            if (!restored.structuralGraph_.add(
                    saved.id, false, sources)) {
                return false;
            }
            restored.ramps_.push_back(saved);
        }
    }

    restored.nextIndex_ = maximumIndex + 1U;
    restored.syncStructuralStates();
    grid_ = std::move(restored.grid_);
    supports_ = std::move(restored.supports_);
    structuralGraph_ = std::move(restored.structuralGraph_);
    platformFrames_ = std::move(restored.platformFrames_);
    walls_ = std::move(restored.walls_);
    ramps_ = std::move(restored.ramps_);
    collapsedBuildings_.clear();
    nextIndex_ = restored.nextIndex_;
    return true;
}

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
        .health = ModularWallMaxHealth * maxHealthMultiplier_,
        .maxHealth = ModularWallMaxHealth * maxHealthMultiplier_,
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
        .anchor = rampAnchorFromSupport(
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
    const auto supportCells =
        rampSupportCells(
            placement.anchor, rotation);
    auto floor = topFloorAtCell(
        supportCells[0].x, supportCells[0].z);
    if (!floor) {
        placement.error =
            ModularPlacementError::NoSupport;
        return placement;
    }
    for (std::size_t index = 1;
         index < supportCells.size(); ++index) {
        const auto supportedFloor = topFloorAtCell(
            supportCells[index].x,
            supportCells[index].z);
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
        .health = ModularRampMaxHealth * maxHealthMultiplier_,
        .maxHealth = ModularRampMaxHealth * maxHealthMultiplier_,
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
    std::vector<EntityId> sources;
    const auto supportCells =
        rampSupportCells(
            instance.anchor, instance.rotation);
    for (const GridCoord cell : supportCells) {
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
