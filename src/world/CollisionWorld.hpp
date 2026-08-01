#pragma once

#include "assets/GlbCollision.hpp"
#include "buildings/BuildingSystem.hpp"
#include "buildings/FoundationSystem.hpp"
#include "core/Types.hpp"

#include <array>
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

    [[nodiscard]] Vec3 moveCircle(
        Vec3 position, Vec3 delta, double radius,
        double maximumWalkableSurfaceHeight =
            std::numeric_limits<double>::infinity()) const;
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
    enum class SurfaceKind {
        Flat,
        Ramp,
    };

    struct WalkableSurface {
        double minX{};
        double maxX{};
        double minZ{};
        double maxZ{};
        double bottomHeight{};
        double topHeight{};
        Rotation rotation{Rotation::Deg0};
        SurfaceKind kind{SurfaceKind::Flat};
    };

    void rebuildColliders();

    double worldLimit_{48.0};
    std::vector<CollisionBox> staticColliders_;
    std::vector<CollisionBox> buildingColliders_;
    std::vector<CollisionBox> modularColliders_;
    std::vector<CollisionBox> rampPlacementColliders_;
    std::vector<CollisionBox> colliders_;
    std::vector<WalkableSurface> buildingSurfaces_;
    std::vector<WalkableSurface> modularSurfaces_;
};

[[nodiscard]] CollisionBox buildingCollisionBox(
    BuildingType type, GridPosition position,
    double baseHeight = 0.0);
[[nodiscard]] std::array<
    CollisionBox, ModularRampRunCells>
rampCollisionBoxes(
    GridCoord anchor, Rotation rotation,
    double bottomHeight, double topHeight,
    double cellSize);
[[nodiscard]] bool collisionBoxesOverlap(
    const CollisionBox& left,
    const CollisionBox& right);

} // namespace ian
