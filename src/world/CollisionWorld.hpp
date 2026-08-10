#pragma once

#include "assets/GlbCollision.hpp"
#include "buildings/BuildingSystem.hpp"
#include "buildings/FoundationSystem.hpp"
#include "core/Types.hpp"
#include "resources/ResourceSystem.hpp"
#include "world/PondDecorationLayout.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace ian {

struct CollisionBox {
    double minX;
    double maxX;
    double minZ;
    double maxZ;
    double maximumBlockingEyeY{
        std::numeric_limits<double>::infinity()};
    double minimumBlockingEyeY{
        -std::numeric_limits<double>::infinity()};
};

struct ModularCollisionView {
    std::span<const PlatformFrameInstance> platformFrames;
    std::span<const WallInstance> walls;
    std::span<const RampInstance> ramps;
    double cellSize{1.0};
    std::span<const ModelCollider> platformColliders{};
    std::span<const ModelCollider> rampColliders{};
};

struct PlayerSurfaceLanding {
    Vec3 position;
    double surfaceHeight{};
};

class CollisionWorld {
  public:
    static constexpr double PlayerRadius = 0.35;

    CollisionWorld();
    CollisionWorld(double worldLimit, std::vector<CollisionBox> staticColliders);

    void reset();
    void syncBuildings(const std::vector<BuildingInstance>& buildings);
    void syncModularBuildings(
        const ModularCollisionView& buildings);
    void syncResourceCylinders(
        std::span<const ResourceNode> resources,
        std::span<const GlbCollisionAsset> treeAssets);
    void syncPondLilySurfaces(
        std::span<const PondLilyPlacement> lilies);

    [[nodiscard]] Vec3 moveCircle(
        Vec3 position, Vec3 delta, double radius,
        double maximumWalkableSurfaceHeight =
            std::numeric_limits<double>::infinity()) const;
    [[nodiscard]] Vec3 moveCircleAgainstRaisedSurfaces(
        Vec3 position, Vec3 delta, double radius,
        double maximumWalkableSurfaceHeight) const;
    [[nodiscard]] std::optional<double>
    modularSurfaceHeight(
        double worldX, double worldZ,
        double maximumSurfaceHeight) const;
    [[nodiscard]] std::optional<double>
    playerSupportHeight(
        double worldX, double worldZ, double radius,
        double maximumSurfaceHeight) const;
    [[nodiscard]] std::optional<PlayerSurfaceLanding>
    sweptPlayerLanding(
        Vec3 startPosition, Vec3 endPosition,
        double radius, double startFeetHeight,
        double endFeetHeight) const;
    [[nodiscard]] std::optional<double>
    modularCeilingHeight(
        double worldX, double worldZ,
        double minimumHeadHeight,
        double maximumHeadHeight) const;
    [[nodiscard]] bool overlapsCircle(Vec3 position, double radius,
                                      const CollisionBox& box) const;
    [[nodiscard]] bool overlapsBox(const CollisionBox& candidate) const;
    [[nodiscard]] bool overlapsRampBox(
        const CollisionBox& candidate) const;
    [[nodiscard]] const std::vector<CollisionBox>& colliders() const;

  private:
    [[nodiscard]] Vec3 moveCircleInternal(
        Vec3 position, Vec3 delta, double radius,
        double maximumWalkableSurfaceHeight,
        bool collideWithSolidGeometry) const;

    enum class SurfaceKind {
        Flat,
        Ramp,
        Disc,
    };

    struct WalkableSurface {
        double minX{};
        double maxX{};
        double minZ{};
        double maxZ{};
        double bottomHeight{};
        double topHeight{};
        double undersideOffset{};
        Rotation rotation{Rotation::Deg0};
        SurfaceKind kind{SurfaceKind::Flat};
    };

    struct PhysicalCylinder {
        double centerX{};
        double centerZ{};
        double radius{};
        double minimumBlockingEyeY{};
        double maximumBlockingEyeY{};
    };

    // Broadphase buckets keep movement, support, and placement queries
    // proportional to the nearby collider count instead of scanning the
    // complete world on every sub-step. Exact geometry tests still run after
    // the bucket lookup, so this does not change collision behaviour.
    struct BroadphaseGrid {
        static constexpr double CellSize = 4.0;

        void reset(double worldLimit);
        void insert(double minX, double maxX,
                    double minZ, double maxZ,
                    std::size_t objectIndex);
        [[nodiscard]] int cellCoordinate(double value) const;
        [[nodiscard]] const std::vector<std::size_t>&
        bucket(int x, int z) const;
        [[nodiscard]] bool empty() const;

        double minimum_{-52.0};
        int dimension_{};
        std::vector<std::vector<std::size_t>> buckets_;
    };

    void rebuildColliders();
    void rebuildResourceBroadphase();
    void rebuildSurfaceBroadphases();

    double worldLimit_{48.0};
    std::vector<CollisionBox> staticColliders_;
    std::vector<CollisionBox> buildingColliders_;
    std::vector<CollisionBox> modularColliders_;
    std::vector<CollisionBox> rampPlacementColliders_;
    std::vector<PhysicalCylinder> resourceCylinders_;
    std::vector<CollisionBox> colliders_;
    std::vector<WalkableSurface> buildingSurfaces_;
    std::vector<WalkableSurface> modularSurfaces_;
    std::vector<WalkableSurface> pondLilySurfaces_;
    BroadphaseGrid colliderBroadphase_;
    BroadphaseGrid rampPlacementBroadphase_;
    BroadphaseGrid resourceBroadphase_;
    BroadphaseGrid buildingSurfaceBroadphase_;
    BroadphaseGrid modularSurfaceBroadphase_;
    BroadphaseGrid pondLilySurfaceBroadphase_;
};

[[nodiscard]] CollisionBox buildingCollisionBox(
    BuildingType type, GridPosition position,
    double baseHeight = 0.0);
[[nodiscard]] std::array<
    CollisionBox, ModularRampRunCells>
rampCollisionBoxes(
    GridCoord anchor, Rotation rotation,
    double bottomHeight, double topHeight,
    double cellSize,
    double undersideOffset = 0.08);
[[nodiscard]] bool collisionBoxesOverlap(
    const CollisionBox& left,
    const CollisionBox& right);

} // namespace ian
