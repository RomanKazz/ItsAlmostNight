#include "buildings/FoundationSystem.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
FoundationSystem::FoundationSystem(
    const TerrainHeightfield& terrain,
    WorldConfig config)
    : terrain_(terrain), grid_(config) {
    platformFrames_.reserve(256);
    walls_.reserve(256);
    ramps_.reserve(64);
}

void FoundationSystem::reset() {
    platformFrames_.clear();
    walls_.clear();
    ramps_.clear();
    collapsedBuildings_.clear();
    grid_ = BuildGrid{grid_.config()};
    supports_.reset();
    structuralGraph_.reset();
    nextIndex_ = 12000U;
}

const PlatformFrameInstance*
FoundationSystem::frameAt(
    GridCoord anchor, int storey) const {
    const auto frame = std::find_if(
        platformFrames_.begin(), platformFrames_.end(),
        [anchor, storey](
            const PlatformFrameInstance& candidate) {
            return candidate.anchor.x == anchor.x &&
                   candidate.anchor.z == anchor.z &&
                   candidate.storey == storey;
        });
    return frame == platformFrames_.end()
               ? nullptr
               : &*frame;
}

PlatformFramePlacement
FoundationSystem::previewPlatformFrame(
    Vec3 terrainHit, Vec3 playerPosition) const {
    GridCoord anchor = grid_.worldToGrid(terrainHit);
    anchor.x = snapPlatformFrameAxis(anchor.x);
    anchor.z = snapPlatformFrameAxis(anchor.z);

    const PlatformFrameInstance* previous =
        frameAt(anchor, 0);
    if (previous) {
        for (int storey = 1;; ++storey) {
            const PlatformFrameInstance* next =
                frameAt(anchor, storey);
            if (!next) {
                break;
            }
            previous = next;
        }
    }
    if (!previous) {
        return PlacementValidator{
            terrain_, grid_}
            .validateGroundPlatformFrame(
                terrainHit, playerPosition);
    }

    return previewPlatformFrameAt(
        anchor, previous->storey + 1,
        previous->floorHeight +
            modularStoreyHeight(grid_.config()),
        playerPosition);
}

PlatformFramePlacement
FoundationSystem::previewPlatformFrameAt(
    GridCoord anchor, int storey,
    double floorHeight,
    Vec3 playerPosition) const {
    anchor.x = snapPlatformFrameAxis(anchor.x);
    anchor.z = snapPlatformFrameAxis(anchor.z);
    if (storey == 0) {
        return PlacementValidator{
            terrain_, grid_}
            .validateGroundPlatformFrameAt(
                anchor, floorHeight,
                playerPosition);
    }

    PlatformFramePlacement placement{
        .anchor = anchor,
        .floorHeight = floorHeight,
        .storey = storey,
    };
    const WorldConfig& config = grid_.config();
    placement.anchor.yLevel =
        static_cast<int>(std::lround(
            placement.floorHeight /
            config.verticalGridStep));
    const double minimumX =
        placement.anchor.x * config.cellSize;
    const double minimumZ =
        placement.anchor.z * config.cellSize;
    const double width =
        PlatformFrameWidthCells * config.cellSize;
    const double maximumX = minimumX + width;
    const double maximumZ = minimumZ + width;
    const std::array<Vec3, 4> corners{{
        {minimumX, placement.floorHeight, minimumZ},
        {maximumX, placement.floorHeight, minimumZ},
        {minimumX, placement.floorHeight, maximumZ},
        {maximumX, placement.floorHeight, maximumZ},
    }};
    for (std::size_t index = 0;
         index < corners.size(); ++index) {
        placement.supports[index] = {
            .top = corners[index],
            .bottom = {
                corners[index].x,
                floorHeight -
                    modularStoreyHeight(config),
                corners[index].z,
            },
            .length = modularStoreyHeight(config),
        };
    }
    const PlatformFrameInstance* previous =
        frameAt(anchor, storey - 1);
    if (!previous ||
        previous->supportState !=
        StructuralSupportState::Supported) {
        placement.error =
            ModularPlacementError::NoSupport;
        return placement;
    }
    if (placement.storey >= config.maxStoreys) {
        placement.error =
            ModularPlacementError::MaximumStorey;
        return placement;
    }
    const Vec3 center = grid_.worldCenter(
        placement.anchor, Footprint::TwoByTwo);
    if (std::hypot(
            center.x - playerPosition.x,
            center.z - playerPosition.z) >
        config.buildPreviewDistance) {
        placement.error =
            ModularPlacementError::TooFar;
        return placement;
    }
    if (!grid_.canOccupy(
            placement.anchor, Footprint::TwoByTwo, 1,
            OccupancyLayer::Floor)) {
        placement.error =
            ModularPlacementError::Occupied;
        return placement;
    }

    return placement;
}

PlatformFrameColumnPlacement
FoundationSystem::previewPlatformFrameColumn(
    GridCoord anchor, int targetStorey,
    double targetFloorHeight,
    Vec3 playerPosition) const {
    anchor.x = snapPlatformFrameAxis(anchor.x);
    anchor.z = snapPlatformFrameAxis(anchor.z);
    PlatformFrameColumnPlacement result{
        .anchor = anchor,
        .targetFloorHeight = targetFloorHeight,
        .targetStorey = targetStorey,
    };
    if (targetStorey < 0 ||
        targetStorey >= grid_.config().maxStoreys) {
        result.error =
            ModularPlacementError::MaximumStorey;
        return result;
    }

    FoundationSystem preview = *this;
    const double storeyHeight =
        modularStoreyHeight(grid_.config());
    for (int storey = 0;
         storey <= targetStorey; ++storey) {
        const double floorHeight =
            targetFloorHeight -
            (targetStorey - storey) *
                storeyHeight;
        if (const PlatformFrameInstance* existing =
                preview.frameAt(anchor, storey)) {
            if (std::abs(
                    existing->floorHeight -
                    floorHeight) > 1e-6 ||
                existing->supportState !=
                    StructuralSupportState::Supported) {
                result.error =
                    ModularPlacementError::NoSupport;
                return result;
            }
            if (storey == targetStorey) {
                result.error =
                    ModularPlacementError::Occupied;
                return result;
            }
            continue;
        }
        PlatformFramePlacement frame =
            preview.previewPlatformFrameAt(
                anchor, storey, floorHeight,
                playerPosition);
        result.frames.push_back(frame);
        if (!frame.valid()) {
            result.error = frame.error;
            return result;
        }
        if (!preview.placePlatformFrame(frame)) {
            result.error =
                ModularPlacementError::Occupied;
            return result;
        }
    }
    return result;
}

std::optional<PlatformFrameInstance>
FoundationSystem::placePlatformFrame(
    const PlatformFramePlacement& placement) {
    if (!placement.valid()) {
        return std::nullopt;
    }
    PlatformFrameInstance instance{
        .id = {nextIndex_++, 1U},
        .anchor = placement.anchor,
        .floorHeight = placement.floorHeight,
        .storey = placement.storey,
        .health = PlatformFrameMaxHealth,
        .maxHealth = PlatformFrameMaxHealth,
        .supports = placement.supports,
    };
    if (!grid_.occupy(
            instance.id, instance.anchor,
            Footprint::TwoByTwo, 1,
            OccupancyLayer::Floor)) {
        return std::nullopt;
    }

    instance.supportIds = supports_.acquire(placement);
    bool graphAdded = false;
    if (instance.storey == 0) {
        graphAdded =
            structuralGraph_.add(instance.id, true);
    } else {
        const PlatformFrameInstance* previous =
            frameAt(instance.anchor, instance.storey - 1);
        graphAdded =
            previous &&
            previous->supportState ==
                StructuralSupportState::Supported &&
            structuralGraph_.add(
                instance.id, false,
                std::span<const EntityId>{
                    &previous->id, 1U});
    }
    if (!graphAdded) {
        supports_.release(instance.supportIds);
        grid_.release(instance.id);
        return std::nullopt;
    }
    platformFrames_.push_back(instance);
    return instance;
}

std::vector<PlatformFrameInstance>
FoundationSystem::placePlatformFrameColumn(
    const PlatformFrameColumnPlacement& placement) {
    std::vector<PlatformFrameInstance> placed;
    if (!placement.valid()) {
        return placed;
    }
    placed.reserve(placement.frames.size());
    for (const PlatformFramePlacement& frame :
         placement.frames) {
        const auto instance =
            placePlatformFrame(frame);
        if (!instance) {
            for (auto placedFrame = placed.rbegin();
                 placedFrame != placed.rend();
                 ++placedFrame) {
                static_cast<void>(
                    remove(placedFrame->id));
            }
            placed.clear();
            return placed;
        }
        placed.push_back(*instance);
    }
    return placed;
}

std::optional<FoundationSystem::FloorSurface>
FoundationSystem::topFloorAtCell(
    int x, int z) const {
    std::optional<FloorSurface> result;
    for (const PlatformFrameInstance& frame :
         platformFrames_) {
        if (x < frame.anchor.x ||
            x >=
                frame.anchor.x +
                    PlatformFrameWidthCells ||
            z < frame.anchor.z ||
            z >=
                frame.anchor.z +
                    PlatformFrameWidthCells) {
            continue;
        }
        if (!result ||
            frame.floorHeight > result->height) {
            result = FloorSurface{
                .id = frame.id,
                .height = frame.floorHeight,
                .storey = frame.storey,
            };
        }
    }
    return result;
}

std::optional<BuildingPlatformSurface>
FoundationSystem::buildingSurface(
    int minimumCellX, int minimumCellZ,
    int widthCells) const {
    if (widthCells <= 0) {
        return std::nullopt;
    }

    std::optional<BuildingPlatformSurface> result;
    for (const PlatformFrameInstance& candidate :
         platformFrames_) {
        if (candidate.supportState !=
            StructuralSupportState::Supported) {
            continue;
        }
        bool fullySupported = true;
        for (int z = minimumCellZ;
             z < minimumCellZ + widthCells &&
             fullySupported;
             ++z) {
            for (int x = minimumCellX;
                 x < minimumCellX + widthCells;
                 ++x) {
                const bool cellSupported = std::any_of(
                    platformFrames_.begin(),
                    platformFrames_.end(),
                    [x, z, &candidate](
                        const PlatformFrameInstance& frame) {
                        return frame.supportState ==
                                   StructuralSupportState::Supported &&
                               frame.storey ==
                                   candidate.storey &&
                               std::abs(
                                   frame.floorHeight -
                                   candidate.floorHeight) <
                                   1e-4 &&
                               x >= frame.anchor.x &&
                               x < frame.anchor.x +
                                       PlatformFrameWidthCells &&
                               z >= frame.anchor.z &&
                               z < frame.anchor.z +
                                       PlatformFrameWidthCells;
                    });
                if (!cellSupported) {
                    fullySupported = false;
                    break;
                }
            }
        }
        if (fullySupported &&
            (!result ||
             candidate.floorHeight > result->height)) {
            result = BuildingPlatformSurface{
                .height = candidate.floorHeight,
                .foundationBottomHeight =
                    candidate.floorHeight,
                .storey = candidate.storey,
            };
        }
    }
    return result;
}

bool FoundationSystem::buildingFootprintIntersectsPlatform(
    int minimumCellX, int minimumCellZ,
    int widthCells) const {
    if (widthCells <= 0) {
        return false;
    }
    const int maximumCellX =
        minimumCellX + widthCells;
    const int maximumCellZ =
        minimumCellZ + widthCells;
    return std::any_of(
        platformFrames_.begin(),
        platformFrames_.end(),
        [=](const PlatformFrameInstance& frame) {
            return frame.supportState ==
                       StructuralSupportState::Supported &&
                   minimumCellX <
                       frame.anchor.x +
                           PlatformFrameWidthCells &&
                   maximumCellX > frame.anchor.x &&
                   minimumCellZ <
                       frame.anchor.z +
                           PlatformFrameWidthCells &&
                   maximumCellZ > frame.anchor.z;
        });
}

std::span<const PlatformFrameInstance>
FoundationSystem::platformFrames() const {
    return platformFrames_;
}

std::span<const WallInstance>
FoundationSystem::walls() const {
    return walls_;
}

std::span<const RampInstance>
FoundationSystem::ramps() const {
    return ramps_;
}

const BuildGrid& FoundationSystem::grid() const {
    return grid_;
}

const SupportSystem&
FoundationSystem::supportSystem() const {
    return supports_;
}

} // namespace ian
