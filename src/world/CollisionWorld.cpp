#include "world/CollisionWorld.hpp"

#include <algorithm>
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

CollisionWorld::CollisionWorld() : CollisionWorld(48.0, defaultStaticColliders()) {}

CollisionWorld::CollisionWorld(double worldLimit, std::vector<CollisionBox> staticColliders)
    : worldLimit_(worldLimit), staticColliders_(std::move(staticColliders)),
      colliders_(staticColliders_) {
    colliders_.reserve(128);
}

void CollisionWorld::reset() {
    buildingColliders_.clear();
    modularColliders_.clear();
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
    modularSurfaces_.clear();
    modularColliders_.reserve(buildings.walls.size());
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

Vec3 CollisionWorld::moveCircle(Vec3 position, Vec3 delta, double radius) const {
    const double distance = std::sqrt((delta.x * delta.x) + (delta.z * delta.z));
    const int stepCount =
        std::max(1, static_cast<int>(std::ceil(distance / MaxMovementStep)));
    const double stepX = delta.x / static_cast<double>(stepCount);
    const double stepZ = delta.z / static_cast<double>(stepCount);

    for (int step = 0; step < stepCount; ++step) {
        const bool escapingCollider =
            std::any_of(
                colliders_.begin(), colliders_.end(),
                [position, radius](const auto& box) {
                    return pointInsideExpandedBox(
                        position, radius, box);
                });
        Vec3 candidate = position;
        candidate.x =
            std::clamp(candidate.x + stepX, -worldLimit_ + radius, worldLimit_ - radius);
        const bool blockedX = !escapingCollider &&
            std::any_of(colliders_.begin(), colliders_.end(),
                        [candidate, radius](const auto& box) {
                return pointInsideExpandedBox(
                    candidate, radius, box);
            });
        if (!blockedX) {
            position.x = candidate.x;
        }

        candidate = position;
        candidate.z =
            std::clamp(candidate.z + stepZ, -worldLimit_ + radius, worldLimit_ - radius);
        const bool blockedZ = !escapingCollider &&
            std::any_of(colliders_.begin(), colliders_.end(),
                        [candidate, radius](const auto& box) {
                return pointInsideExpandedBox(
                    candidate, radius, box);
            });
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
    return std::any_of(colliders_.begin(), colliders_.end(),
                       [candidate](const CollisionBox& collider) {
                           return candidate.minX < collider.maxX &&
                                  candidate.maxX > collider.minX &&
                                  candidate.minZ < collider.maxZ &&
                                  candidate.maxZ > collider.minZ &&
                                  candidate.minimumBlockingEyeY <
                                      collider.maximumBlockingEyeY &&
                                  candidate.maximumBlockingEyeY >
                                      collider.minimumBlockingEyeY;
                       });
}

const std::vector<CollisionBox>& CollisionWorld::colliders() const {
    return colliders_;
}

} // namespace ian
