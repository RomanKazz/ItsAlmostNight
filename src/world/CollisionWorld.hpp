#pragma once

#include "buildings/BuildingSystem.hpp"
#include "core/Types.hpp"

#include <vector>

namespace ian {

struct CollisionBox {
    double minX;
    double maxX;
    double minZ;
    double maxZ;
};

class CollisionWorld {
  public:
    static constexpr double PlayerRadius = 0.35;

    CollisionWorld();
    CollisionWorld(double worldLimit, std::vector<CollisionBox> staticColliders);

    void reset();
    void syncBuildings(const std::vector<BuildingInstance>& buildings);

    [[nodiscard]] Vec3 moveCircle(Vec3 position, Vec3 delta, double radius) const;
    [[nodiscard]] bool overlapsCircle(Vec3 position, double radius,
                                      const CollisionBox& box) const;
    [[nodiscard]] bool overlapsBox(const CollisionBox& candidate) const;
    [[nodiscard]] const std::vector<CollisionBox>& colliders() const;

  private:
    double worldLimit_{48.0};
    std::vector<CollisionBox> staticColliders_;
    std::vector<CollisionBox> colliders_;
};

[[nodiscard]] CollisionBox buildingCollisionBox(BuildingType type, GridPosition position);

} // namespace ian
