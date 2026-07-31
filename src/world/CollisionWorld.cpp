#include "world/CollisionWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace ian {
namespace {

constexpr double MaxMovementStep = 0.15;
constexpr double WallColliderWidth = 0.20;
constexpr double WallJumpClearanceEyeHeight = 2.15;

std::vector<CollisionBox> defaultStaticColliders() {
    return {
        {-9.0, -7.0, -8.0, -6.0},
        {7.5, 10.5, -13.5, -10.5},
    };
}

bool pointInsideExpandedBox(Vec3 point, double radius, const CollisionBox& box) {
    return point.y > box.minimumBlockingEyeY &&
           point.y < box.maximumBlockingEyeY &&
           point.x > box.minX - radius && point.x < box.maxX + radius &&
           point.z > box.minZ - radius && point.z < box.maxZ + radius;
}

bool buildingBlocksPlayer(
    const BuildingInstance& building) {
    switch (building.type) {
    case BuildingType::Wall:
        return true;
    case BuildingType::Core:
        return false;
    case BuildingType::Gate:
        return !building.open;
    case BuildingType::Turret:
    case BuildingType::GoldMine:
    case BuildingType::Cannon:
    case BuildingType::SlowTrap:
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
        return false;
    }
    return false;
}

void appendWallColliders(
    std::vector<CollisionBox>& colliders,
    const BuildingInstance& wall,
    const std::vector<BuildingInstance>& buildings) {
    const Vec3 center = buildingWorldPosition(wall);
    constexpr double HalfLength = 0.5;
    constexpr double HalfWidth = WallColliderWidth * 0.5;
    const auto add = [&colliders, &wall](
                         double minimumX, double maximumX,
                         double minimumZ, double maximumZ) {
        colliders.push_back({
            minimumX, maximumX, minimumZ, maximumZ,
            wall.baseHeight +
                WallJumpClearanceEyeHeight,
            wall.baseHeight,
        });
    };

    const std::uint8_t connections =
        wallConnectionMask(
            buildings, wall.gridPosition,
            wall.baseHeight);
    if (connections == 0U) {
        if ((wall.rotation % 2U) == 0U) {
            add(center.x - HalfLength,
                center.x + HalfLength,
                center.z - HalfWidth,
                center.z + HalfWidth);
        } else {
            add(center.x - HalfWidth,
                center.x + HalfWidth,
                center.z - HalfLength,
                center.z + HalfLength);
        }
        return;
    }

    add(center.x - HalfWidth, center.x + HalfWidth,
        center.z - HalfWidth, center.z + HalfWidth);
    if ((connections & WallConnectionNorth) != 0U) {
        add(center.x - HalfWidth, center.x + HalfWidth,
            center.z - HalfLength, center.z);
    }
    if ((connections & WallConnectionEast) != 0U) {
        add(center.x, center.x + HalfLength,
            center.z - HalfWidth, center.z + HalfWidth);
    }
    if ((connections & WallConnectionSouth) != 0U) {
        add(center.x - HalfWidth, center.x + HalfWidth,
            center.z, center.z + HalfLength);
    }
    if ((connections & WallConnectionWest) != 0U) {
        add(center.x - HalfLength, center.x,
            center.z - HalfWidth, center.z + HalfWidth);
    }
}

} // namespace

CollisionBox buildingCollisionBox(
    BuildingType type, GridPosition position,
    double baseHeight) {
    const double halfExtent =
        buildingFootprintHalfExtent(type);
    const Vec3 center = buildingWorldPosition(type, position);
    double height = 2.2;
    if (type == BuildingType::Core) {
        height = 2.7;
    } else if (
        type == BuildingType::SlowTrap) {
        height = 0.5;
    } else if (
        type == BuildingType::GoldMine ||
        type == BuildingType::LumberMill ||
        type == BuildingType::Quarry) {
        height = 1.7;
    }
    return {
        center.x - halfExtent,
        center.x + halfExtent,
        center.z - halfExtent,
        center.z + halfExtent,
        baseHeight +
            ((type == BuildingType::Wall ||
              type == BuildingType::Gate)
                 ? WallJumpClearanceEyeHeight
                 : height),
        baseHeight,
    };
}

std::array<CollisionBox, ModularRampRunCells>
rampCollisionBoxes(
    GridCoord anchor, Rotation rotation,
    double bottomHeight, double topHeight,
    double cellSize) {
    std::array<
        CollisionBox, ModularRampRunCells> boxes{};
    const double risePerCell =
        (topHeight - bottomHeight) /
        static_cast<double>(ModularRampRunCells);
    constexpr double HalfBoardThickness = 0.08;
    for (int runCell = 0;
         runCell < ModularRampRunCells; ++runCell) {
        int minimumX = anchor.x;
        int maximumX =
            anchor.x + ModularRampWidthCells;
        int minimumZ = anchor.z;
        int maximumZ =
            anchor.z + ModularRampWidthCells;
        switch (rotation) {
        case Rotation::Deg0:
            minimumZ += runCell;
            maximumZ = minimumZ + 1;
            break;
        case Rotation::Deg90:
            minimumX +=
                ModularRampRunCells - runCell - 1;
            maximumX = minimumX + 1;
            break;
        case Rotation::Deg180:
            minimumZ +=
                ModularRampRunCells - runCell - 1;
            maximumZ = minimumZ + 1;
            break;
        case Rotation::Deg270:
            minimumX += runCell;
            maximumX = minimumX + 1;
            break;
        }
        const double segmentBottom =
            bottomHeight +
            static_cast<double>(runCell) *
                risePerCell;
        boxes[static_cast<std::size_t>(runCell)] = {
            minimumX * cellSize,
            maximumX * cellSize,
            minimumZ * cellSize,
            maximumZ * cellSize,
            segmentBottom + risePerCell +
                HalfBoardThickness,
            segmentBottom - HalfBoardThickness,
        };
    }
    return boxes;
}

bool collisionBoxesOverlap(
    const CollisionBox& left,
    const CollisionBox& right) {
    return left.minX < right.maxX &&
           left.maxX > right.minX &&
           left.minZ < right.maxZ &&
           left.maxZ > right.minZ &&
           left.minimumBlockingEyeY <
               right.maximumBlockingEyeY &&
           left.maximumBlockingEyeY >
               right.minimumBlockingEyeY;
}

CollisionWorld::CollisionWorld() : CollisionWorld(48.0, defaultStaticColliders()) {}

CollisionWorld::CollisionWorld(double worldLimit, std::vector<CollisionBox> staticColliders)
    : worldLimit_(worldLimit), staticColliders_(std::move(staticColliders)),
      colliders_(staticColliders_) {
    colliders_.reserve(128);
}

void CollisionWorld::reset() {
    buildingColliders_.clear();
    modularColliders_.clear();
    rampPlacementColliders_.clear();
    buildingSurfaces_.clear();
    modularSurfaces_.clear();
    rebuildColliders();
}

void CollisionWorld::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    buildingColliders_.clear();
    buildingSurfaces_.clear();
    buildingColliders_.reserve(buildings.size() * 3U);
    buildingSurfaces_.reserve(buildings.size());
    for (const auto& building : buildings) {
        if (building.baseHeight -
                building.foundationBottomHeight >
            0.025) {
            const Vec3 center =
                buildingWorldPosition(building);
            const double halfExtent =
                buildingFootprintHalfExtent(
                    building.type);
            buildingSurfaces_.push_back({
                .minX = center.x - halfExtent,
                .maxX = center.x + halfExtent,
                .minZ = center.z - halfExtent,
                .maxZ = center.z + halfExtent,
                .bottomHeight =
                    building.baseHeight,
                .topHeight = building.baseHeight,
                .kind = SurfaceKind::Flat,
            });
        }
        if (!buildingBlocksPlayer(building)) {
            continue;
        }
        if (building.type == BuildingType::Wall) {
            appendWallColliders(
                buildingColliders_, building, buildings);
        } else {
            buildingColliders_.push_back(
                buildingCollisionBox(
                    building.type,
                    building.gridPosition,
                    building.baseHeight));
        }
    }
    rebuildColliders();
}

void CollisionWorld::syncModularBuildings(
    const ModularCollisionView& buildings) {
    modularColliders_.clear();
    rampPlacementColliders_.clear();
    modularSurfaces_.clear();
    modularColliders_.reserve(
        buildings.walls.size());
    rampPlacementColliders_.reserve(
        buildings.ramps.size() *
            ModularRampRunCells);
    modularSurfaces_.reserve(
        buildings.platformFrames.size() +
        buildings.ramps.size());

    const auto addFloor =
        [this, cellSize = buildings.cellSize](
            GridCoord anchor,
            double height) {
            modularSurfaces_.push_back({
                .minX = anchor.x * cellSize,
                .maxX =
                    (anchor.x + PlatformFrameWidthCells) *
                    cellSize,
                .minZ = anchor.z * cellSize,
                .maxZ =
                    (anchor.z + PlatformFrameWidthCells) *
                    cellSize,
                .bottomHeight = height,
                .topHeight = height,
                .kind = SurfaceKind::Flat,
            });
        };
    for (const PlatformFrameInstance& frame :
         buildings.platformFrames) {
        addFloor(
            frame.anchor, frame.floorHeight);
    }
    constexpr double WallThickness = 0.14;
    constexpr double PlayerEyeOffset = 1.7;
    for (const WallInstance& wall : buildings.walls) {
        const bool alongX =
            wall.rotation == Rotation::Deg0 ||
            wall.rotation == Rotation::Deg180;
        const double centerX =
            (wall.anchor.x + 0.5) * buildings.cellSize;
        const double centerZ =
            (wall.anchor.z + 0.5) * buildings.cellSize;
        const double halfLength = buildings.cellSize * 0.5;
        const double halfThickness = WallThickness * 0.5;
        modularColliders_.push_back({
            .minX = centerX -
                (alongX ? halfLength : halfThickness),
            .maxX = centerX +
                (alongX ? halfLength : halfThickness),
            .minZ = centerZ -
                (alongX ? halfThickness : halfLength),
            .maxZ = centerZ +
                (alongX ? halfThickness : halfLength),
            .maximumBlockingEyeY =
                wall.topHeight + PlayerEyeOffset,
            .minimumBlockingEyeY = wall.bottomHeight,
        });
    }
    for (const RampInstance& ramp : buildings.ramps) {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const int widthCells =
            alongZ ? ModularRampWidthCells
                   : ModularRampRunCells;
        const int depthCells =
            alongZ ? ModularRampRunCells
                   : ModularRampWidthCells;
        modularSurfaces_.push_back({
            .minX =
                ramp.anchor.x * buildings.cellSize,
            .maxX =
                (ramp.anchor.x + widthCells) *
                buildings.cellSize,
            .minZ =
                ramp.anchor.z * buildings.cellSize,
            .maxZ =
                (ramp.anchor.z + depthCells) *
                buildings.cellSize,
            .bottomHeight = ramp.bottomHeight,
            .topHeight = ramp.topHeight,
            .rotation = ramp.rotation,
            .kind = SurfaceKind::Ramp,
        });
        const auto rampBoxes =
            rampCollisionBoxes(
                ramp.anchor, ramp.rotation,
                ramp.bottomHeight, ramp.topHeight,
                buildings.cellSize);
        // Ramp surface handles player movement. These
        // boxes only reject intersecting placement.
        rampPlacementColliders_.insert(
            rampPlacementColliders_.end(),
            rampBoxes.begin(), rampBoxes.end());
    }
    rebuildColliders();
}

void CollisionWorld::rebuildColliders() {
    colliders_.clear();
    colliders_.reserve(
        staticColliders_.size() +
        buildingColliders_.size() +
        modularColliders_.size());
    colliders_.insert(
        colliders_.end(),
        staticColliders_.begin(), staticColliders_.end());
    colliders_.insert(
        colliders_.end(),
        buildingColliders_.begin(),
        buildingColliders_.end());
    colliders_.insert(
        colliders_.end(),
        modularColliders_.begin(),
        modularColliders_.end());
}

Vec3 CollisionWorld::moveCircle(
    Vec3 position, Vec3 delta, double radius,
    double maximumWalkableSurfaceHeight) const {
    const double distance = std::sqrt((delta.x * delta.x) + (delta.z * delta.z));
    const int stepCount =
        std::max(1, static_cast<int>(std::ceil(distance / MaxMovementStep)));
    const double stepX = delta.x / static_cast<double>(stepCount);
    const double stepZ = delta.z / static_cast<double>(stepCount);
    const auto blockedByRaisedSurface =
        [this, maximumWalkableSurfaceHeight,
         radius](Vec3 candidate) {
            if (!std::isfinite(
                    maximumWalkableSurfaceHeight)) {
                return false;
            }
            const std::array<Vec3, 5> samples{{
                candidate,
                {candidate.x + radius,
                 candidate.y, candidate.z},
                {candidate.x - radius,
                 candidate.y, candidate.z},
                {candidate.x,
                 candidate.y, candidate.z + radius},
                {candidate.x,
                 candidate.y, candidate.z - radius},
            }};
            return std::any_of(
                samples.begin(), samples.end(),
                [this,
                 maximumWalkableSurfaceHeight](
                    Vec3 sample) {
                    constexpr double SurfaceEpsilon =
                        1e-6;
                    constexpr double HeadAboveEye =
                        0.15;
                    const auto surface =
                        modularSurfaceHeight(
                            sample.x, sample.z,
                            sample.y + HeadAboveEye);
                    return surface &&
                        *surface >
                            maximumWalkableSurfaceHeight +
                                SurfaceEpsilon;
                });
        };

    for (int step = 0; step < stepCount; ++step) {
        const bool escapingCollider =
            std::any_of(
                colliders_.begin(), colliders_.end(),
                [position, radius](const auto& box) {
                    return pointInsideExpandedBox(
                        position, radius, box);
                });
        // A jump can move the player's vertical range into a
        // platform or ramp while their horizontal center is
        // already inside its footprint. In that state the normal
        // raised-surface sweep must permit movement until the
        // expanded footprint has been exited, or every direction
        // remains blocked forever.
        const bool escapingRaisedSurface =
            blockedByRaisedSurface(position);
        Vec3 candidate = position;
        candidate.x =
            std::clamp(candidate.x + stepX, -worldLimit_ + radius, worldLimit_ - radius);
        const bool blockedX =
            (!escapingRaisedSurface &&
             blockedByRaisedSurface(candidate)) ||
            (!escapingCollider &&
             std::any_of(
                 colliders_.begin(), colliders_.end(),
                 [candidate, radius](const auto& box) {
                     return pointInsideExpandedBox(
                         candidate, radius, box);
                 }));
        if (!blockedX) {
            position.x = candidate.x;
        }

        candidate = position;
        candidate.z =
            std::clamp(candidate.z + stepZ, -worldLimit_ + radius, worldLimit_ - radius);
        const bool blockedZ =
            (!escapingRaisedSurface &&
             blockedByRaisedSurface(candidate)) ||
            (!escapingCollider &&
             std::any_of(
                 colliders_.begin(), colliders_.end(),
                 [candidate, radius](const auto& box) {
                     return pointInsideExpandedBox(
                         candidate, radius, box);
                 }));
        if (!blockedZ) {
            position.z = candidate.z;
        }
    }

    return position;
}

std::optional<double>
CollisionWorld::modularSurfaceHeight(
    double worldX, double worldZ,
    double maximumSurfaceHeight) const {
    std::optional<double> result;
    constexpr double EdgeEpsilon = 1e-6;
    const auto sampleSurface =
        [worldX, worldZ, maximumSurfaceHeight,
         &result](const WalkableSurface& surface) {
        if (worldX < surface.minX - EdgeEpsilon ||
            worldX > surface.maxX + EdgeEpsilon ||
            worldZ < surface.minZ - EdgeEpsilon ||
            worldZ > surface.maxZ + EdgeEpsilon) {
            return;
        }
        double height = surface.topHeight;
        if (surface.kind == SurfaceKind::Ramp) {
            const double widthX =
                surface.maxX - surface.minX;
            const double widthZ =
                surface.maxZ - surface.minZ;
            double progress = 0.0;
            switch (surface.rotation) {
            case Rotation::Deg0:
                progress =
                    (worldZ - surface.minZ) / widthZ;
                break;
            case Rotation::Deg90:
                progress =
                    (surface.maxX - worldX) / widthX;
                break;
            case Rotation::Deg180:
                progress =
                    (surface.maxZ - worldZ) / widthZ;
                break;
            case Rotation::Deg270:
                progress =
                    (worldX - surface.minX) / widthX;
                break;
            }
            height = surface.bottomHeight +
                std::clamp(progress, 0.0, 1.0) *
                    (surface.topHeight -
                     surface.bottomHeight);
        }
        if (height > maximumSurfaceHeight +
                EdgeEpsilon ||
            (result && height <= *result)) {
            return;
        }
        result = height;
    };
    for (const WalkableSurface& surface :
         buildingSurfaces_) {
        sampleSurface(surface);
    }
    for (const WalkableSurface& surface :
         modularSurfaces_) {
        sampleSurface(surface);
    }
    return result;
}

std::optional<double>
CollisionWorld::playerSupportHeight(
    double worldX, double worldZ, double radius,
    double maximumSurfaceHeight) const {
    std::optional<double> result =
        modularSurfaceHeight(
            worldX, worldZ, maximumSurfaceHeight);
    constexpr double EdgeEpsilon = 1e-6;
    const auto sampleSupport =
        [worldX, worldZ, radius,
         maximumSurfaceHeight, &result](
            const WalkableSurface& surface) {
            const double closestX = std::clamp(
                worldX, surface.minX, surface.maxX);
            const double closestZ = std::clamp(
                worldZ, surface.minZ, surface.maxZ);
            const double deltaX = worldX - closestX;
            const double deltaZ = worldZ - closestZ;
            if (deltaX * deltaX + deltaZ * deltaZ >
                radius * radius + EdgeEpsilon) {
                return;
            }
            double height = surface.topHeight;
            if (surface.kind == SurfaceKind::Ramp) {
                const double widthX =
                    surface.maxX - surface.minX;
                const double widthZ =
                    surface.maxZ - surface.minZ;
                double progress = 0.0;
                switch (surface.rotation) {
                case Rotation::Deg0:
                    progress =
                        (closestZ - surface.minZ) / widthZ;
                    break;
                case Rotation::Deg90:
                    progress =
                        (surface.maxX - closestX) / widthX;
                    break;
                case Rotation::Deg180:
                    progress =
                        (surface.maxZ - closestZ) / widthZ;
                    break;
                case Rotation::Deg270:
                    progress =
                        (closestX - surface.minX) / widthX;
                    break;
                }
                height = surface.bottomHeight +
                    std::clamp(progress, 0.0, 1.0) *
                        (surface.topHeight -
                         surface.bottomHeight);
            }
            if (height > maximumSurfaceHeight +
                    EdgeEpsilon ||
                (result && height <= *result)) {
                return;
            }
            result = height;
        };
    for (const WalkableSurface& surface :
         buildingSurfaces_) {
        sampleSupport(surface);
    }
    for (const WalkableSurface& surface :
         modularSurfaces_) {
        sampleSupport(surface);
    }
    return result;
}

std::optional<PlayerSurfaceLanding>
CollisionWorld::sweptPlayerLanding(
    Vec3 startPosition, Vec3 endPosition,
    double radius, double startFeetHeight,
    double endFeetHeight) const {
    const double deltaX =
        endPosition.x - startPosition.x;
    const double deltaZ =
        endPosition.z - startPosition.z;
    const double deltaFeet =
        endFeetHeight - startFeetHeight;
    const double sweepDistance = std::max(
        std::hypot(deltaX, deltaZ),
        std::abs(deltaFeet));
    const double maximumStep =
        std::max(0.025, radius * 0.2);
    const int stepCount = std::clamp(
        static_cast<int>(std::ceil(
            sweepDistance / maximumStep)),
        1, 256);
    constexpr double SurfaceEpsilon = 1e-6;
    std::optional<PlayerSurfaceLanding> result;
    double earliestTime = 2.0;

    const auto supportHeight =
        [radius](const WalkableSurface& surface,
                 double worldX, double worldZ)
            -> std::optional<double> {
            const double closestX = std::clamp(
                worldX, surface.minX, surface.maxX);
            const double closestZ = std::clamp(
                worldZ, surface.minZ, surface.maxZ);
            const double offsetX = worldX - closestX;
            const double offsetZ = worldZ - closestZ;
            if (offsetX * offsetX + offsetZ * offsetZ >
                radius * radius + SurfaceEpsilon) {
                return std::nullopt;
            }
            if (surface.kind != SurfaceKind::Ramp) {
                return surface.topHeight;
            }
            double progress = 0.0;
            switch (surface.rotation) {
            case Rotation::Deg0:
                progress =
                    (closestZ - surface.minZ) /
                    (surface.maxZ - surface.minZ);
                break;
            case Rotation::Deg90:
                progress =
                    (surface.maxX - closestX) /
                    (surface.maxX - surface.minX);
                break;
            case Rotation::Deg180:
                progress =
                    (surface.maxZ - closestZ) /
                    (surface.maxZ - surface.minZ);
                break;
            case Rotation::Deg270:
                progress =
                    (closestX - surface.minX) /
                    (surface.maxX - surface.minX);
                break;
            }
            return surface.bottomHeight +
                std::clamp(progress, 0.0, 1.0) *
                    (surface.topHeight -
                     surface.bottomHeight);
        };

    const auto sampleSurface =
        [&](const WalkableSurface& surface) {
            auto previousSurface = supportHeight(
                surface, startPosition.x,
                startPosition.z);
            double previousGap = previousSurface
                ? startFeetHeight - *previousSurface
                : 0.0;
            for (int step = 1; step <= stepCount;
                 ++step) {
                const double time =
                    static_cast<double>(step) /
                    static_cast<double>(stepCount);
                if (time > earliestTime) {
                    break;
                }
                const double worldX =
                    startPosition.x + deltaX * time;
                const double worldZ =
                    startPosition.z + deltaZ * time;
                const double feetHeight =
                    startFeetHeight + deltaFeet * time;
                const auto currentSurface =
                    supportHeight(
                        surface, worldX, worldZ);
                if (currentSurface) {
                    const double currentGap =
                        feetHeight - *currentSurface;
                    const double entryTolerance =
                        sweepDistance /
                            static_cast<double>(stepCount) +
                        SurfaceEpsilon;
                    const bool crossedFromAbove =
                        previousSurface &&
                        previousGap >= -SurfaceEpsilon &&
                        currentGap <= SurfaceEpsilon;
                    const bool enteredAtSurface =
                        !previousSurface &&
                        currentGap <= SurfaceEpsilon &&
                        currentGap >= -entryTolerance;
                    if (crossedFromAbove ||
                        enteredAtSurface) {
                        if (time < earliestTime -
                                       SurfaceEpsilon ||
                            (!result ||
                             (std::abs(time - earliestTime) <=
                                  SurfaceEpsilon &&
                              *currentSurface >
                                  result->surfaceHeight))) {
                            earliestTime = time;
                            result = PlayerSurfaceLanding{
                                .position = {
                                    worldX,
                                    endPosition.y,
                                    worldZ,
                                },
                                .surfaceHeight =
                                    *currentSurface,
                            };
                        }
                        break;
                    }
                    previousGap = currentGap;
                }
                previousSurface = currentSurface;
            }
        };
    for (const WalkableSurface& surface :
         buildingSurfaces_) {
        sampleSurface(surface);
    }
    for (const WalkableSurface& surface :
         modularSurfaces_) {
        sampleSurface(surface);
    }
    return result;
}

bool CollisionWorld::playerVolumeIntersectsSolid(
    Vec3 eyePosition, double radius,
    double feetHeight, double headHeight) const {
    if (!std::isfinite(eyePosition.x) ||
        !std::isfinite(eyePosition.y) ||
        !std::isfinite(eyePosition.z) ||
        !std::isfinite(feetHeight) ||
        !std::isfinite(headHeight)) {
        return true;
    }
    if (std::any_of(
            colliders_.begin(), colliders_.end(),
            [eyePosition, radius](const CollisionBox& box) {
                return pointInsideExpandedBox(
                    eyePosition, radius, box);
            })) {
        return true;
    }

    constexpr double ContactTolerance = 0.025;
    constexpr double PlatformThickness = 0.50;
    constexpr double RampThickness = 0.18;
    const auto intersectsSurface =
        [eyePosition, feetHeight, headHeight](
            const WalkableSurface& surface) {
            if (eyePosition.x <
                    surface.minX - ContactTolerance ||
                eyePosition.x >
                    surface.maxX + ContactTolerance ||
                eyePosition.z <
                    surface.minZ - ContactTolerance ||
                eyePosition.z >
                    surface.maxZ + ContactTolerance) {
                return false;
            }
            double surfaceHeight = surface.topHeight;
            if (surface.kind == SurfaceKind::Ramp) {
                double progress = 0.0;
                switch (surface.rotation) {
                case Rotation::Deg0:
                    progress =
                        (eyePosition.z - surface.minZ) /
                        (surface.maxZ - surface.minZ);
                    break;
                case Rotation::Deg90:
                    progress =
                        (surface.maxX - eyePosition.x) /
                        (surface.maxX - surface.minX);
                    break;
                case Rotation::Deg180:
                    progress =
                        (surface.maxZ - eyePosition.z) /
                        (surface.maxZ - surface.minZ);
                    break;
                case Rotation::Deg270:
                    progress =
                        (eyePosition.x - surface.minX) /
                        (surface.maxX - surface.minX);
                    break;
                }
                surfaceHeight = surface.bottomHeight +
                    std::clamp(progress, 0.0, 1.0) *
                        (surface.topHeight -
                         surface.bottomHeight);
            }
            const double thickness =
                surface.kind == SurfaceKind::Ramp
                    ? RampThickness
                    : PlatformThickness;
            const double underside =
                surfaceHeight - thickness;
            return feetHeight <
                       surfaceHeight - ContactTolerance &&
                   headHeight >
                       underside + ContactTolerance;
        };
    return std::any_of(
               buildingSurfaces_.begin(),
               buildingSurfaces_.end(),
               intersectsSurface) ||
           std::any_of(
               modularSurfaces_.begin(),
               modularSurfaces_.end(),
               intersectsSurface);
}

Vec3 CollisionWorld::resolvePlayerPenetration(
    Vec3 eyePosition, double radius) const {
    constexpr double Separation = 1e-4;
    constexpr int MaximumIterations = 8;
    eyePosition.x = std::clamp(
        eyePosition.x,
        -worldLimit_ + radius,
        worldLimit_ - radius);
    eyePosition.z = std::clamp(
        eyePosition.z,
        -worldLimit_ + radius,
        worldLimit_ - radius);
    for (int iteration = 0;
         iteration < MaximumIterations; ++iteration) {
        bool foundOverlap = false;
        double bestDistance =
            std::numeric_limits<double>::infinity();
        Vec3 bestPosition = eyePosition;
        for (const CollisionBox& box : colliders_) {
            if (!pointInsideExpandedBox(
                    eyePosition, radius, box)) {
                continue;
            }
            foundOverlap = true;
            const double minimumX = box.minX - radius;
            const double maximumX = box.maxX + radius;
            const double minimumZ = box.minZ - radius;
            const double maximumZ = box.maxZ + radius;
            const std::array<Vec3, 4> candidates{{
                {minimumX - Separation,
                 eyePosition.y, eyePosition.z},
                {maximumX + Separation,
                 eyePosition.y, eyePosition.z},
                {eyePosition.x,
                 eyePosition.y, minimumZ - Separation},
                {eyePosition.x,
                 eyePosition.y, maximumZ + Separation},
            }};
            for (const Vec3 candidate : candidates) {
                const double distance = std::hypot(
                    candidate.x - eyePosition.x,
                    candidate.z - eyePosition.z);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestPosition = candidate;
                }
            }
        }
        if (!foundOverlap) {
            break;
        }
        eyePosition = bestPosition;
        eyePosition.x = std::clamp(
            eyePosition.x,
            -worldLimit_ + radius,
            worldLimit_ - radius);
        eyePosition.z = std::clamp(
            eyePosition.z,
            -worldLimit_ + radius,
            worldLimit_ - radius);
    }
    return eyePosition;
}

std::optional<double>
CollisionWorld::modularCeilingHeight(
    double worldX, double worldZ,
    double minimumHeadHeight,
    double maximumHeadHeight) const {
    if (maximumHeadHeight < minimumHeadHeight) {
        std::swap(
            minimumHeadHeight, maximumHeadHeight);
    }
    std::optional<double> result;
    constexpr double EdgeEpsilon = 1e-6;
    constexpr double PlatformThickness = 0.50;
    constexpr double RampThickness = 0.18;
    const auto sampleCeiling =
        [worldX, worldZ, minimumHeadHeight,
         maximumHeadHeight, &result](
            const WalkableSurface& surface) {
            if (worldX < surface.minX - EdgeEpsilon ||
                worldX > surface.maxX + EdgeEpsilon ||
                worldZ < surface.minZ - EdgeEpsilon ||
                worldZ > surface.maxZ + EdgeEpsilon) {
                return;
            }
            double surfaceHeight = surface.topHeight;
            if (surface.kind == SurfaceKind::Ramp) {
                const double widthX =
                    surface.maxX - surface.minX;
                const double widthZ =
                    surface.maxZ - surface.minZ;
                double progress = 0.0;
                switch (surface.rotation) {
                case Rotation::Deg0:
                    progress =
                        (worldZ - surface.minZ) / widthZ;
                    break;
                case Rotation::Deg90:
                    progress =
                        (surface.maxX - worldX) / widthX;
                    break;
                case Rotation::Deg180:
                    progress =
                        (surface.maxZ - worldZ) / widthZ;
                    break;
                case Rotation::Deg270:
                    progress =
                        (worldX - surface.minX) / widthX;
                    break;
                }
                surfaceHeight = surface.bottomHeight +
                    std::clamp(progress, 0.0, 1.0) *
                        (surface.topHeight -
                         surface.bottomHeight);
            }
            const double underside = surfaceHeight -
                (surface.kind == SurfaceKind::Ramp
                     ? RampThickness
                     : PlatformThickness);
            if (underside < minimumHeadHeight -
                    EdgeEpsilon ||
                underside > maximumHeadHeight +
                    EdgeEpsilon ||
                (result && underside >= *result)) {
                return;
            }
            result = underside;
        };
    for (const WalkableSurface& surface :
         buildingSurfaces_) {
        sampleCeiling(surface);
    }
    for (const WalkableSurface& surface :
         modularSurfaces_) {
        sampleCeiling(surface);
    }
    return result;
}

bool CollisionWorld::overlapsCircle(Vec3 position, double radius, const CollisionBox& box) const {
    const double closestX = std::clamp(position.x, box.minX, box.maxX);
    const double closestZ = std::clamp(position.z, box.minZ, box.maxZ);
    const double deltaX = position.x - closestX;
    const double deltaZ = position.z - closestZ;
    return (deltaX * deltaX) + (deltaZ * deltaZ) < radius * radius;
}

bool CollisionWorld::overlapsBox(const CollisionBox& candidate) const {
    if (candidate.minX < -worldLimit_ || candidate.maxX > worldLimit_ ||
        candidate.minZ < -worldLimit_ || candidate.maxZ > worldLimit_) {
        return true;
    }
    const auto overlapsCandidate =
        [candidate](const CollisionBox& collider) {
            return collisionBoxesOverlap(
                candidate, collider);
        };
    return std::any_of(
               colliders_.begin(), colliders_.end(),
               overlapsCandidate) ||
           std::any_of(
               rampPlacementColliders_.begin(),
               rampPlacementColliders_.end(),
               overlapsCandidate);
}

bool CollisionWorld::overlapsRampBox(
    const CollisionBox& candidate) const {
    return std::any_of(
        rampPlacementColliders_.begin(),
        rampPlacementColliders_.end(),
        [candidate](const CollisionBox& collider) {
            return collisionBoxesOverlap(
                candidate, collider);
        });
}

const std::vector<CollisionBox>& CollisionWorld::colliders() const {
    return colliders_;
}

} // namespace ian
