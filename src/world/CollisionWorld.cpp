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
    case BuildingType::CrystalMine:
    case BuildingType::Cannon:
    case BuildingType::SlowTrap:
    case BuildingType::SpikeTrap:
    case BuildingType::LumberMill:
    case BuildingType::Quarry:
    case BuildingType::WoodStorage:
    case BuildingType::StoneStorage:
    case BuildingType::CrystalStorage:
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

void CollisionWorld::BroadphaseGrid::reset(double worldLimit) {
    const double extent = std::max(
        worldLimit * 2.0 + CellSize * 2.0,
        CellSize);
    minimum_ = -std::max(worldLimit, CellSize) - CellSize;
    dimension_ = std::max(
        1, static_cast<int>(std::ceil(extent / CellSize)));
    buckets_.clear();
    buckets_.resize(
        static_cast<std::size_t>(dimension_) *
        static_cast<std::size_t>(dimension_));
}

int CollisionWorld::BroadphaseGrid::cellCoordinate(
    double value) const {
    if (dimension_ <= 0 || !std::isfinite(value)) {
        return value < 0.0 ? 0 : std::max(0, dimension_ - 1);
    }
    const int coordinate = static_cast<int>(std::floor(
        (value - minimum_) / CellSize));
    return std::clamp(coordinate, 0, dimension_ - 1);
}

const std::vector<std::size_t>&
CollisionWorld::BroadphaseGrid::bucket(int x, int z) const {
    static const std::vector<std::size_t> Empty;
    if (dimension_ <= 0 || x < 0 || z < 0 ||
        x >= dimension_ || z >= dimension_) {
        return Empty;
    }
    return buckets_[static_cast<std::size_t>(z) *
                        static_cast<std::size_t>(dimension_) +
                    static_cast<std::size_t>(x)];
}

bool CollisionWorld::BroadphaseGrid::empty() const {
    return dimension_ <= 0 || buckets_.empty();
}

void CollisionWorld::BroadphaseGrid::insert(
    double minX, double maxX, double minZ, double maxZ,
    std::size_t objectIndex) {
    if (empty()) {
        return;
    }
    const int minimumX = cellCoordinate(std::min(minX, maxX));
    const int maximumX = cellCoordinate(std::max(minX, maxX));
    const int minimumZ = cellCoordinate(std::min(minZ, maxZ));
    const int maximumZ = cellCoordinate(std::max(minZ, maxZ));
    for (int z = minimumZ; z <= maximumZ; ++z) {
        for (int x = minimumX; x <= maximumX; ++x) {
            buckets_[static_cast<std::size_t>(z) *
                         static_cast<std::size_t>(dimension_) +
                     static_cast<std::size_t>(x)]
                .push_back(objectIndex);
        }
    }
}

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
        type == BuildingType::SlowTrap ||
        type == BuildingType::SpikeTrap) {
        height = 0.5;
    } else if (
        type == BuildingType::CrystalMine ||
        type == BuildingType::LumberMill ||
        type == BuildingType::Quarry) {
        height = 0.85;
    } else if (
        type == BuildingType::WoodStorage ||
        type == BuildingType::StoneStorage ||
        type == BuildingType::CrystalStorage) {
        height = 1.8;
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
    double cellSize, double undersideOffset) {
    std::array<
        CollisionBox, ModularRampRunCells> boxes{};
    const double risePerCell =
        (topHeight - bottomHeight) /
        static_cast<double>(ModularRampRunCells);
    undersideOffset = std::max(undersideOffset, 0.0);
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
            segmentBottom + risePerCell,
            segmentBottom - undersideOffset,
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
    rebuildColliders();
}

void CollisionWorld::reset() {
    buildingColliders_.clear();
    modularColliders_.clear();
    rampPlacementColliders_.clear();
    resourceCylinders_.clear();
    buildingSurfaces_.clear();
    modularSurfaces_.clear();
    rebuildColliders();
}

void CollisionWorld::syncResourceCylinders(
    std::span<const ResourceNode> resources,
    std::span<const GlbCollisionAsset> treeAssets,
    std::span<const GlbCollisionAsset> stoneAssets) {
    resourceCylinders_.clear();
    resourceCylinders_.reserve(resources.size());
    for (const ResourceNode& resource : resources) {
        if (!resource.active) {
            continue;
        }
        if (isDestructibleProp(resource.type)) {
            const double terrainHeight =
                resource.position.y - resource.groundOffset;
            resourceCylinders_.push_back({
                .centerX = resource.position.x,
                .centerZ = resource.position.z,
                .radius = resource.radius * 0.78 * resource.visualScale,
                .minimumBlockingEyeY = terrainHeight,
                .maximumBlockingEyeY = terrainHeight +
                    1.35 * resource.visualScale,
            });
            continue;
        }
        const bool tree = resource.type == ResourceType::Wood;
        const bool stone = resource.type == ResourceType::Stone;
        const std::span<const GlbCollisionAsset> assets =
            tree ? treeAssets : stone ? stoneAssets
                                      : std::span<const GlbCollisionAsset>{};
        if (assets.empty()) {
            continue;
        }
        const std::size_t variant =
            resource.visualVariant % assets.size();
        const double scale = tree
            ? resource.visualScale
            : StoneVisualModelScale;
        const double cosine = std::cos(resource.visualYaw);
        const double sine = std::sin(resource.visualYaw);
        const double terrainHeight =
            resource.position.y - resource.groundOffset;
        const double modelOriginY = terrainHeight +
            (tree
                 ? TreeVisualGroundOffsets[
                       variant % TreeVisualVariantCount] * scale
                 : StoneVisualGroundOffsets[
                       variant % StoneVisualVariantCount]);
        for (const ModelCollider& collider :
             assets[variant].colliders) {
            if (collider.type != ModelColliderType::Cylinder &&
                collider.type != ModelColliderType::Sphere) {
                continue;
            }
            const double localX =
                (collider.minimum.x + collider.maximum.x) * 0.5;
            const double localZ =
                (collider.minimum.z + collider.maximum.z) * 0.5;
            resourceCylinders_.push_back({
                .centerX = resource.position.x +
                    (localX * cosine + localZ * sine) * scale,
                .centerZ = resource.position.z +
                    (-localX * sine + localZ * cosine) * scale,
                .radius = std::max(
                    collider.maximum.x - collider.minimum.x,
                    collider.maximum.z - collider.minimum.z) *
                    0.5 * scale,
                .minimumBlockingEyeY = modelOriginY +
                    collider.minimum.y * scale,
                .maximumBlockingEyeY = stone
                    ? std::max(
                          modelOriginY +
                              collider.maximum.y * scale,
                          terrainHeight +
                              WallJumpClearanceEyeHeight)
                    : modelOriginY +
                          collider.maximum.y * scale,
            });
        }
    }
    rebuildResourceBroadphase();
}

void CollisionWorld::syncPondLilySurfaces(
    std::span<const PondLilyPlacement> lilies) {
    pondLilySurfaces_.clear();
    pondLilySurfaces_.reserve(lilies.size());
    for (const PondLilyPlacement& lily : lilies) {
        if (lily.collisionRadius <= 0.0 ||
            lily.surfaceHeight <= lily.position.y) {
            continue;
        }
        pondLilySurfaces_.push_back({
            .minX = lily.position.x - lily.collisionRadius,
            .maxX = lily.position.x + lily.collisionRadius,
            .minZ = lily.position.z - lily.collisionRadius,
            .maxZ = lily.position.z + lily.collisionRadius,
            .bottomHeight = lily.position.y,
            .topHeight = lily.surfaceHeight,
            .kind = SurfaceKind::Disc,
        });
    }
    rebuildSurfaceBroadphases();
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
                    building.baseHeight - 0.50,
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
        [this, cellSize = buildings.cellSize,
         colliders = buildings.platformColliders](
            GridCoord anchor,
            double height) {
            const double centerX =
                (anchor.x +
                 PlatformFrameWidthCells * 0.5) *
                cellSize;
            const double centerZ =
                (anchor.z +
                 PlatformFrameWidthCells * 0.5) *
                cellSize;
            bool importedWalkable = false;
            for (const ModelCollider& collider : colliders) {
                if (!collider.walkable ||
                    collider.type !=
                        ModelColliderType::Box) {
                    continue;
                }
                // Platform visuals are rotated 180 degrees by
                // RendererModels before being placed in world.
                modularSurfaces_.push_back({
                    .minX = centerX -
                        collider.maximum.x * cellSize,
                    .maxX = centerX -
                        collider.minimum.x * cellSize,
                    .minZ = centerZ -
                        collider.maximum.z * cellSize,
                    .maxZ = centerZ -
                        collider.minimum.z * cellSize,
                    .bottomHeight = height +
                        collider.minimum.y * cellSize,
                    .topHeight = height +
                        collider.maximum.y * cellSize,
                    .kind = SurfaceKind::Flat,
                });
                importedWalkable = true;
            }
            if (!importedWalkable) {
                constexpr double FallbackThickness = 0.50;
                modularSurfaces_.push_back({
                    .minX = anchor.x * cellSize,
                    .maxX =
                        (anchor.x +
                         PlatformFrameWidthCells) *
                        cellSize,
                    .minZ = anchor.z * cellSize,
                    .maxZ =
                        (anchor.z +
                         PlatformFrameWidthCells) *
                        cellSize,
                    .bottomHeight =
                        height - FallbackThickness,
                    .topHeight = height,
                    .kind = SurfaceKind::Flat,
                });
            }
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
        double undersideOffset = 0.18;
        for (const ModelCollider& collider :
             buildings.rampColliders) {
            if (collider.walkable &&
                collider.type == ModelColliderType::Slope &&
                collider.minimum.y < 0.0) {
                undersideOffset = std::max(
                    -collider.minimum.y * buildings.cellSize,
                    0.01);
                break;
            }
        }
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
            .undersideOffset = undersideOffset,
            .rotation = ramp.rotation,
            .kind = SurfaceKind::Ramp,
        });
        const auto rampBoxes =
            rampCollisionBoxes(
                ramp.anchor, ramp.rotation,
                ramp.bottomHeight, ramp.topHeight,
                buildings.cellSize,
                undersideOffset);
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
    colliderBroadphase_.reset(worldLimit_);
    for (std::size_t index = 0; index < colliders_.size(); ++index) {
        const CollisionBox& box = colliders_[index];
        colliderBroadphase_.insert(
            box.minX, box.maxX, box.minZ, box.maxZ, index);
    }
    rampPlacementBroadphase_.reset(worldLimit_);
    for (std::size_t index = 0;
         index < rampPlacementColliders_.size(); ++index) {
        const CollisionBox& box = rampPlacementColliders_[index];
        rampPlacementBroadphase_.insert(
            box.minX, box.maxX, box.minZ, box.maxZ, index);
    }
    rebuildSurfaceBroadphases();
}

void CollisionWorld::rebuildResourceBroadphase() {
    resourceBroadphase_.reset(worldLimit_);
    for (std::size_t index = 0;
         index < resourceCylinders_.size(); ++index) {
        const PhysicalCylinder& cylinder =
            resourceCylinders_[index];
        resourceBroadphase_.insert(
            cylinder.centerX - cylinder.radius,
            cylinder.centerX + cylinder.radius,
            cylinder.centerZ - cylinder.radius,
            cylinder.centerZ + cylinder.radius,
            index);
    }
}

void CollisionWorld::rebuildSurfaceBroadphases() {
    const auto rebuild = [this](
                             BroadphaseGrid& grid,
                             const std::vector<WalkableSurface>& surfaces) {
        grid.reset(worldLimit_);
        for (std::size_t index = 0; index < surfaces.size(); ++index) {
            const WalkableSurface& surface = surfaces[index];
            grid.insert(
                surface.minX, surface.maxX,
                surface.minZ, surface.maxZ, index);
        }
    };
    rebuild(buildingSurfaceBroadphase_, buildingSurfaces_);
    rebuild(modularSurfaceBroadphase_, modularSurfaces_);
    rebuild(pondLilySurfaceBroadphase_, pondLilySurfaces_);
}

Vec3 CollisionWorld::moveCircle(
    Vec3 position, Vec3 delta, double radius,
    double maximumWalkableSurfaceHeight) const {
    return moveCircleInternal(
        position, delta, radius,
        maximumWalkableSurfaceHeight, true);
}

Vec3 CollisionWorld::moveCircleAgainstRaisedSurfaces(
    Vec3 position, Vec3 delta, double radius,
    double maximumWalkableSurfaceHeight) const {
    return moveCircleInternal(
        position, delta, radius,
        maximumWalkableSurfaceHeight, false);
}

Vec3 CollisionWorld::moveCircleInternal(
    Vec3 position, Vec3 delta, double radius,
    double maximumWalkableSurfaceHeight,
    bool collideWithSolidGeometry) const {
    const double distance = std::sqrt((delta.x * delta.x) + (delta.z * delta.z));
    const int stepCount =
        std::max(1, static_cast<int>(std::ceil(distance / MaxMovementStep)));
    const double stepX = delta.x / static_cast<double>(stepCount);
    const double stepZ = delta.z / static_cast<double>(stepCount);
    constexpr double CollisionEpsilon = 1e-6;
    const auto forEachNearby = [](
                                const BroadphaseGrid& grid,
                                double x, double z, double queryRadius,
                                auto&& visitor) {
        if (grid.empty()) {
            return;
        }
        const int minimumX = grid.cellCoordinate(x - queryRadius);
        const int maximumX = grid.cellCoordinate(x + queryRadius);
        const int minimumZ = grid.cellCoordinate(z - queryRadius);
        const int maximumZ = grid.cellCoordinate(z + queryRadius);
        for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
            for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
                for (const std::size_t index :
                     grid.bucket(cellX, cellZ)) {
                    visitor(index);
                }
            }
        }
    };
    const auto circleRectanglePenetration =
        [radius](Vec3 point, double minimumX,
                 double maximumX, double minimumZ,
                 double maximumZ) {
            const double closestX = std::clamp(
                point.x, minimumX, maximumX);
            const double closestZ = std::clamp(
                point.z, minimumZ, maximumZ);
            const double offsetX = point.x - closestX;
            const double offsetZ = point.z - closestZ;
            const double distance =
                std::hypot(offsetX, offsetZ);
            if (distance >= radius) {
                return 0.0;
            }
            const bool centerInside =
                point.x >= minimumX &&
                point.x <= maximumX &&
                point.z >= minimumZ &&
                point.z <= maximumZ;
            if (!centerInside) {
                return radius - distance;
            }
            return std::min({
                point.x - (minimumX - radius),
                maximumX + radius - point.x,
                point.z - (minimumZ - radius),
                maximumZ + radius - point.z,
            });
        };
    const auto raisedSurfacePenetration =
        [maximumWalkableSurfaceHeight, radius,
         &circleRectanglePenetration](
            const WalkableSurface& surface,
            Vec3 point) {
            if (!std::isfinite(
                    maximumWalkableSurfaceHeight)) {
                return 0.0;
            }
            const double closestX = std::clamp(
                point.x, surface.minX, surface.maxX);
            const double closestZ = std::clamp(
                point.z, surface.minZ, surface.maxZ);
            double surfaceHeight = surface.topHeight;
            if (surface.kind == SurfaceKind::Ramp) {
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
                surfaceHeight = surface.bottomHeight +
                    std::clamp(progress, 0.0, 1.0) *
                        (surface.topHeight -
                         surface.bottomHeight);
            }
            constexpr double HeadAboveEye = 0.15;
            if (surfaceHeight <=
                    maximumWalkableSurfaceHeight +
                        CollisionEpsilon ||
                surfaceHeight >
                    point.y + HeadAboveEye +
                        CollisionEpsilon) {
                return 0.0;
            }
            if (surface.kind == SurfaceKind::Disc) {
                const double centerX =
                    (surface.minX + surface.maxX) * 0.5;
                const double centerZ =
                    (surface.minZ + surface.maxZ) * 0.5;
                const double surfaceRadius =
                    (surface.maxX - surface.minX) * 0.5;
                return std::max(
                    0.0, radius + surfaceRadius -
                        std::hypot(
                            point.x - centerX,
                            point.z - centerZ));
            }
            return circleRectanglePenetration(
                point, surface.minX, surface.maxX,
                surface.minZ, surface.maxZ);
        };
    const auto raisedSurfaceBlocksMovement =
        [this, &raisedSurfacePenetration,
         &forEachNearby, radius](Vec3 from, Vec3 candidate) {
            const auto blocks =
                [&raisedSurfacePenetration,
                 from, candidate](
                    const WalkableSurface& surface) {
                    const double candidateDepth =
                        raisedSurfacePenetration(
                            surface, candidate);
                    if (candidateDepth <=
                        CollisionEpsilon) {
                        return false;
                    }
                    const double currentDepth =
                        raisedSurfacePenetration(
                            surface, from);
                    return currentDepth <=
                               CollisionEpsilon ||
                           candidateDepth >
                               currentDepth +
                                   CollisionEpsilon;
                };
            const auto anyNearby =
                [&forEachNearby, &blocks, candidate,
                 radius](const BroadphaseGrid& grid,
                       const std::vector<WalkableSurface>& surfaces) {
                    bool found = false;
                    forEachNearby(
                        grid, candidate.x, candidate.z, radius,
                        [&found, &blocks, &surfaces](std::size_t index) {
                            if (!found) {
                                found = blocks(surfaces[index]);
                            }
                        });
                    return found;
                };
            return anyNearby(
                       buildingSurfaceBroadphase_,
                       buildingSurfaces_) ||
                   anyNearby(
                       modularSurfaceBroadphase_,
                       modularSurfaces_) ||
                   anyNearby(
                       pondLilySurfaceBroadphase_,
                       pondLilySurfaces_);
        };
    const auto colliderPenetration =
        [radius](Vec3 point, const CollisionBox& box) {
            if (point.y <= box.minimumBlockingEyeY ||
                point.y >= box.maximumBlockingEyeY) {
                return 0.0;
            }
            const double minimumX = box.minX - radius;
            const double maximumX = box.maxX + radius;
            const double minimumZ = box.minZ - radius;
            const double maximumZ = box.maxZ + radius;
            if (point.x <= minimumX ||
                point.x >= maximumX ||
                point.z <= minimumZ ||
                point.z >= maximumZ) {
                return 0.0;
            }
            return std::min({
                point.x - minimumX,
                maximumX - point.x,
                point.z - minimumZ,
                maximumZ - point.z,
            });
        };
    const auto colliderBlocksMovement =
        [this, &colliderPenetration,
         &forEachNearby, radius](
            Vec3 from, Vec3 candidate) {
            bool boxBlocks = false;
            forEachNearby(
                colliderBroadphase_, candidate.x, candidate.z, radius,
                [&boxBlocks, &colliderPenetration,
                 from, candidate, this](std::size_t index) {
                    if (boxBlocks) {
                        return;
                    }
                    const CollisionBox& box = colliders_[index];
                    const double candidateDepth =
                        colliderPenetration(candidate, box);
                    if (candidateDepth <= CollisionEpsilon) {
                        return;
                    }
                    const double currentDepth =
                        colliderPenetration(from, box);
                    boxBlocks = currentDepth <= CollisionEpsilon ||
                        candidateDepth >
                            currentDepth + CollisionEpsilon;
                });
            if (boxBlocks) {
                return true;
            }
            const auto cylinderPenetration =
                [radius](Vec3 point,
                         const PhysicalCylinder& cylinder) {
                    if (point.y <= cylinder.minimumBlockingEyeY ||
                        point.y >= cylinder.maximumBlockingEyeY) {
                        return 0.0;
                    }
                    const double deltaX =
                        point.x - cylinder.centerX;
                    const double deltaZ =
                        point.z - cylinder.centerZ;
                    const double combinedRadius =
                        radius + cylinder.radius;
                    return std::max(
                        0.0,
                        combinedRadius -
                            std::hypot(deltaX, deltaZ));
                };
            bool cylinderBlocks = false;
            forEachNearby(
                resourceBroadphase_, candidate.x, candidate.z, radius,
                [&cylinderBlocks, &cylinderPenetration,
                 from, candidate, this](std::size_t index) {
                    if (cylinderBlocks) {
                        return;
                    }
                    const PhysicalCylinder& cylinder =
                        resourceCylinders_[index];
                    const double candidateDepth =
                        cylinderPenetration(candidate, cylinder);
                    if (candidateDepth <= CollisionEpsilon) {
                        return;
                    }
                    const double currentDepth =
                        cylinderPenetration(from, cylinder);
                    cylinderBlocks =
                        currentDepth <= CollisionEpsilon ||
                        candidateDepth >
                            currentDepth + CollisionEpsilon;
                });
            return cylinderBlocks;
        };

    for (int step = 0; step < stepCount; ++step) {
        Vec3 candidate = position;
        candidate.x =
            std::clamp(candidate.x + stepX, -worldLimit_ + radius, worldLimit_ - radius);
        const bool blockedX =
            raisedSurfaceBlocksMovement(
                position, candidate) ||
            (collideWithSolidGeometry &&
             colliderBlocksMovement(position, candidate));
        if (!blockedX) {
            position.x = candidate.x;
        }

        candidate = position;
        candidate.z =
            std::clamp(candidate.z + stepZ, -worldLimit_ + radius, worldLimit_ - radius);
        const bool blockedZ =
            raisedSurfaceBlocksMovement(
                position, candidate) ||
            (collideWithSolidGeometry &&
             colliderBlocksMovement(position, candidate));
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
    const int cellX = buildingSurfaceBroadphase_.cellCoordinate(worldX);
    const int cellZ = buildingSurfaceBroadphase_.cellCoordinate(worldZ);
    for (const std::size_t index :
         buildingSurfaceBroadphase_.bucket(cellX, cellZ)) {
        sampleSurface(buildingSurfaces_[index]);
    }
    const int modularCellX = modularSurfaceBroadphase_.cellCoordinate(worldX);
    const int modularCellZ = modularSurfaceBroadphase_.cellCoordinate(worldZ);
    for (const std::size_t index :
         modularSurfaceBroadphase_.bucket(modularCellX, modularCellZ)) {
        sampleSurface(modularSurfaces_[index]);
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
            double closestX = worldX;
            double closestZ = worldZ;
            if (surface.kind == SurfaceKind::Disc) {
                const double centerX =
                    (surface.minX + surface.maxX) * 0.5;
                const double centerZ =
                    (surface.minZ + surface.maxZ) * 0.5;
                const double surfaceRadius =
                    (surface.maxX - surface.minX) * 0.5;
                const double combinedRadius =
                    surfaceRadius + radius;
                const double deltaX = worldX - centerX;
                const double deltaZ = worldZ - centerZ;
                if (deltaX * deltaX + deltaZ * deltaZ >
                    combinedRadius * combinedRadius + EdgeEpsilon) {
                    return;
                }
            } else {
                closestX = std::clamp(
                    worldX, surface.minX, surface.maxX);
                closestZ = std::clamp(
                    worldZ, surface.minZ, surface.maxZ);
                const double deltaX = worldX - closestX;
                const double deltaZ = worldZ - closestZ;
                if (deltaX * deltaX + deltaZ * deltaZ >
                    radius * radius + EdgeEpsilon) {
                    return;
                }
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
    const auto sampleNearby =
        [worldX, worldZ, radius, &sampleSupport](
            const BroadphaseGrid& grid,
            const std::vector<WalkableSurface>& surfaces) {
            const double queryRadius = std::max(radius, 0.0);
            const int minimumX = grid.cellCoordinate(
                worldX - queryRadius);
            const int maximumX = grid.cellCoordinate(
                worldX + queryRadius);
            const int minimumZ = grid.cellCoordinate(
                worldZ - queryRadius);
            const int maximumZ = grid.cellCoordinate(
                worldZ + queryRadius);
            for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
                for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
                    for (const std::size_t index :
                         grid.bucket(cellX, cellZ)) {
                        sampleSupport(surfaces[index]);
                    }
                }
            }
        };
    sampleNearby(buildingSurfaceBroadphase_, buildingSurfaces_);
    sampleNearby(modularSurfaceBroadphase_, modularSurfaces_);
    sampleNearby(pondLilySurfaceBroadphase_, pondLilySurfaces_);
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
    const double timeStep =
        1.0 / static_cast<double>(stepCount);
    const double verticalTravelPerStep =
        std::abs(deltaFeet) * timeStep;
    const double horizontalTravelPerStep =
        std::hypot(deltaX, deltaZ) * timeStep;

    const auto supportHeight =
        [radius](const WalkableSurface& surface,
                 double worldX, double worldZ)
            -> std::optional<double> {
            double closestX = worldX;
            double closestZ = worldZ;
            if (surface.kind == SurfaceKind::Disc) {
                const double centerX =
                    (surface.minX + surface.maxX) * 0.5;
                const double centerZ =
                    (surface.minZ + surface.maxZ) * 0.5;
                const double surfaceRadius =
                    (surface.maxX - surface.minX) * 0.5;
                const double combinedRadius =
                    surfaceRadius + radius;
                const double offsetX = worldX - centerX;
                const double offsetZ = worldZ - centerZ;
                if (offsetX * offsetX + offsetZ * offsetZ >
                    combinedRadius * combinedRadius + SurfaceEpsilon) {
                    return std::nullopt;
                }
            } else {
                closestX = std::clamp(
                    worldX, surface.minX, surface.maxX);
                closestZ = std::clamp(
                    worldZ, surface.minZ, surface.maxZ);
                const double offsetX = worldX - closestX;
                const double offsetZ = worldZ - closestZ;
                if (offsetX * offsetX + offsetZ * offsetZ >
                    radius * radius + SurfaceEpsilon) {
                    return std::nullopt;
                }
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
            double surfaceSlope = 0.0;
            if (surface.kind == SurfaceKind::Ramp) {
                const double run =
                    surface.rotation == Rotation::Deg0 ||
                            surface.rotation == Rotation::Deg180
                        ? surface.maxZ - surface.minZ
                        : surface.maxX - surface.minX;
                if (run > SurfaceEpsilon) {
                    surfaceSlope = std::abs(
                        surface.topHeight -
                        surface.bottomHeight) / run;
                }
            }
            // Gap can close from both falling feet and a rising
            // slope. Ignoring the second term loses the first
            // contact when entering a ramp diagonally.
            const double entryTolerance =
                verticalTravelPerStep +
                surfaceSlope * horizontalTravelPerStep +
                SurfaceEpsilon;
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
                        double collisionTime = time;
                        double collisionX = worldX;
                        double collisionZ = worldZ;
                        double collisionSurface =
                            *currentSurface;
                        if (crossedFromAbove) {
                            const double gapDelta =
                                previousGap - currentGap;
                            const double fraction =
                                gapDelta > SurfaceEpsilon
                                    ? std::clamp(
                                          previousGap /
                                              gapDelta,
                                          0.0, 1.0)
                                    : 1.0;
                            collisionTime =
                                (static_cast<double>(step - 1) +
                                 fraction) *
                                timeStep;
                            collisionX = startPosition.x +
                                deltaX * collisionTime;
                            collisionZ = startPosition.z +
                                deltaZ * collisionTime;
                            if (const auto exactSurface =
                                    supportHeight(
                                        surface, collisionX,
                                        collisionZ)) {
                                collisionSurface =
                                    *exactSurface;
                            }
                        }
                        if (collisionTime < earliestTime -
                                       SurfaceEpsilon ||
                            (!result ||
                             (std::abs(
                                  collisionTime -
                                  earliestTime) <=
                                  SurfaceEpsilon &&
                              collisionSurface >
                                  result->surfaceHeight))) {
                            earliestTime = collisionTime;
                            result = PlayerSurfaceLanding{
                                .position = {
                                    collisionX,
                                    endPosition.y,
                                    collisionZ,
                                },
                                .surfaceHeight =
                                    collisionSurface,
                            };
                        }
                        break;
                    }
                    previousGap = currentGap;
                }
                previousSurface = currentSurface;
            }
        };
    const double queryCenterX =
        (startPosition.x + endPosition.x) * 0.5;
    const double queryCenterZ =
        (startPosition.z + endPosition.z) * 0.5;
    const double queryRadius =
        std::hypot(deltaX, deltaZ) * 0.5 +
        std::max(radius, 0.0);
    const auto sampleNearby =
        [queryCenterX, queryCenterZ, queryRadius, &sampleSurface](
            const BroadphaseGrid& grid,
            const std::vector<WalkableSurface>& surfaces) {
            const int minimumX = grid.cellCoordinate(
                queryCenterX - queryRadius);
            const int maximumX = grid.cellCoordinate(
                queryCenterX + queryRadius);
            const int minimumZ = grid.cellCoordinate(
                queryCenterZ - queryRadius);
            const int maximumZ = grid.cellCoordinate(
                queryCenterZ + queryRadius);
            for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
                for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
                    for (const std::size_t index :
                         grid.bucket(cellX, cellZ)) {
                        sampleSurface(surfaces[index]);
                    }
                }
            }
        };
    sampleNearby(buildingSurfaceBroadphase_, buildingSurfaces_);
    sampleNearby(modularSurfaceBroadphase_, modularSurfaces_);
    sampleNearby(pondLilySurfaceBroadphase_, pondLilySurfaces_);
    return result;
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
            const double underside =
                surface.kind == SurfaceKind::Ramp
                    ? surfaceHeight -
                          surface.undersideOffset
                    : surface.bottomHeight;
            if (underside < minimumHeadHeight -
                    EdgeEpsilon ||
                underside > maximumHeadHeight +
                    EdgeEpsilon ||
                (result && underside >= *result)) {
                return;
            }
            result = underside;
        };
    const int buildingCellX =
        buildingSurfaceBroadphase_.cellCoordinate(worldX);
    const int buildingCellZ =
        buildingSurfaceBroadphase_.cellCoordinate(worldZ);
    for (const std::size_t index :
         buildingSurfaceBroadphase_.bucket(
             buildingCellX, buildingCellZ)) {
        sampleCeiling(buildingSurfaces_[index]);
    }
    const int modularCellX =
        modularSurfaceBroadphase_.cellCoordinate(worldX);
    const int modularCellZ =
        modularSurfaceBroadphase_.cellCoordinate(worldZ);
    for (const std::size_t index :
         modularSurfaceBroadphase_.bucket(
             modularCellX, modularCellZ)) {
        sampleCeiling(modularSurfaces_[index]);
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
    const auto anyNearby =
        [&overlapsCandidate, candidate](
            const BroadphaseGrid& grid,
            const std::vector<CollisionBox>& colliders) {
            if (grid.empty()) {
                return false;
            }
            const int minimumX = grid.cellCoordinate(candidate.minX);
            const int maximumX = grid.cellCoordinate(candidate.maxX);
            const int minimumZ = grid.cellCoordinate(candidate.minZ);
            const int maximumZ = grid.cellCoordinate(candidate.maxZ);
            for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
                for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
                    for (const std::size_t index :
                         grid.bucket(cellX, cellZ)) {
                        if (overlapsCandidate(colliders[index])) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
    return anyNearby(colliderBroadphase_, colliders_) ||
           anyNearby(
               rampPlacementBroadphase_,
               rampPlacementColliders_);
}

bool CollisionWorld::overlapsRampBox(
    const CollisionBox& candidate) const {
    if (rampPlacementBroadphase_.empty()) {
        return false;
    }
    const int minimumX =
        rampPlacementBroadphase_.cellCoordinate(candidate.minX);
    const int maximumX =
        rampPlacementBroadphase_.cellCoordinate(candidate.maxX);
    const int minimumZ =
        rampPlacementBroadphase_.cellCoordinate(candidate.minZ);
    const int maximumZ =
        rampPlacementBroadphase_.cellCoordinate(candidate.maxZ);
    for (int cellZ = minimumZ; cellZ <= maximumZ; ++cellZ) {
        for (int cellX = minimumX; cellX <= maximumX; ++cellX) {
            for (const std::size_t index :
                 rampPlacementBroadphase_.bucket(cellX, cellZ)) {
                if (collisionBoxesOverlap(
                        candidate,
                        rampPlacementColliders_[index])) {
                    return true;
                }
            }
        }
    }
    return false;
}

const std::vector<CollisionBox>& CollisionWorld::colliders() const {
    return colliders_;
}

} // namespace ian
