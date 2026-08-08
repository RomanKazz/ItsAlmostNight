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
}

void FoundationSystem::setMaxHealthMultiplier(double multiplier) {
    const double next = std::max(1.0, multiplier);
    if (std::abs(next - maxHealthMultiplier_) <= 1e-9) {
        return;
    }
    const auto update = [next](double baseMaxHealth,
                                     double& health,
                                     double& maxHealth) {
        const double previous = maxHealth;
        maxHealth = baseMaxHealth * next;
        health += maxHealth - previous;
    };
    for (auto& frame : platformFrames_) {
        update(PlatformFrameMaxHealth, frame.health,
               frame.maxHealth);
    }
    for (auto& wall : walls_) {
        update(ModularWallMaxHealth, wall.health,
               wall.maxHealth);
    }
    for (auto& ramp : ramps_) {
        update(ModularRampMaxHealth, ramp.health,
               ramp.maxHealth);
    }
    maxHealthMultiplier_ = next;
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
FoundationSystem::previewFoundation(
    Vec3 terrainHit, Vec3 playerPosition) const {
    GridCoord anchor = grid_.worldToGrid(terrainHit);
    anchor.x = snapPlatformFrameAxis(anchor.x);
    anchor.z = snapPlatformFrameAxis(anchor.z);

    constexpr std::array<GridCoord, 4> NeighbourOffsets{{
        {-PlatformFrameWidthCells, 0, 0},
        {PlatformFrameWidthCells, 0, 0},
        {0, 0, -PlatformFrameWidthCells},
        {0, 0, PlatformFrameWidthCells},
    }};
    std::vector<double> neighbourLevels;
    for (const GridCoord offset : NeighbourOffsets) {
        const PlatformFrameInstance* neighbour = frameAt(
            {anchor.x + offset.x, 0,
             anchor.z + offset.z},
            0);
        if (!neighbour ||
            neighbour->supportState !=
                StructuralSupportState::Supported) {
            continue;
        }
        if (std::none_of(
                neighbourLevels.begin(),
                neighbourLevels.end(),
                [height = neighbour->floorHeight](
                    double candidate) {
                    return std::abs(candidate - height) <=
                           1e-6;
                })) {
            neighbourLevels.push_back(
                neighbour->floorHeight);
        }
    }

    const PlacementValidator validator{terrain_, grid_};
    if (neighbourLevels.empty()) {
        return validator.validateGroundPlatformFrame(
            terrainHit, playerPosition);
    }

    std::sort(neighbourLevels.begin(),
              neighbourLevels.end());
    std::optional<PlatformFramePlacement> bestValid;
    std::optional<PlatformFramePlacement> bestInvalid;
    const double desiredHeight =
        terrainHit.y +
        grid_.config().minimumGroundClearance;
    const auto distanceToTerrain =
        [desiredHeight](
            const PlatformFramePlacement& placement) {
            return std::abs(
                placement.floorHeight - desiredHeight);
        };
    for (const double neighbourLevel : neighbourLevels) {
        PlatformFramePlacement candidate =
            validator.validateGroundPlatformFrameAt(
                anchor, neighbourLevel,
                playerPosition);
        if (candidate.valid()) {
            if (!bestValid ||
                distanceToTerrain(candidate) <
                    distanceToTerrain(*bestValid)) {
                bestValid = candidate;
            }
        } else if (!bestInvalid ||
                   distanceToTerrain(candidate) <
                       distanceToTerrain(*bestInvalid)) {
            bestInvalid = candidate;
        }
    }
    if (bestValid) {
        return *bestValid;
    }
    return *bestInvalid;
}

PlatformFramePlacement
FoundationSystem::previewFoundationAtHeight(
    Vec3 terrainHit, double floorHeight,
    Vec3 playerPosition) const {
    GridCoord anchor = grid_.worldToGrid(terrainHit);
    anchor.x = snapPlatformFrameAxis(anchor.x);
    anchor.z = snapPlatformFrameAxis(anchor.z);
    return PlacementValidator{terrain_, grid_}
        .validateGroundPlatformFrameAt(
            anchor, floorHeight, playerPosition);
}

PlatformFramePlacement
FoundationSystem::previewFloorPlatform(
    GridCoord anchor, int storey,
    double floorHeight,
    Vec3 playerPosition) const {
    anchor.x = snapPlatformFrameAxis(anchor.x);
    anchor.z = snapPlatformFrameAxis(anchor.z);
    PlatformFramePlacement placement{
        .anchor = anchor,
        .floorHeight = floorHeight,
        .storey = storey,
    };
    if (storey <= 0) {
        placement.error =
            ModularPlacementError::NoSupport;
        return placement;
    }
    const WorldConfig& config = grid_.config();
    if (storey >= config.maxStoreys) {
        placement.error =
            ModularPlacementError::MaximumStorey;
        return placement;
    }
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
    const double baseElevation =
        previous->floorHeight -
        static_cast<double>(storey - 1) *
            modularStoreyHeight(config);
    const double expectedFloorHeight =
        baseElevation +
        static_cast<double>(storey) *
            modularStoreyHeight(config);
    if (std::abs(expectedFloorHeight - floorHeight) >
        1e-6) {
        placement.error =
            ModularPlacementError::NoSupport;
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
        .health = PlatformFrameMaxHealth * maxHealthMultiplier_,
        .maxHealth = PlatformFrameMaxHealth * maxHealthMultiplier_,
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
