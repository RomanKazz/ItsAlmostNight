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
