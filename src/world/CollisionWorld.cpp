#include "world/CollisionWorld.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ian {
namespace {

constexpr double MaxMovementStep = 0.15;

std::vector<CollisionBox> defaultStaticColliders() {
    return {
        {-9.0, -7.0, -8.0, -6.0},
        {7.5, 10.5, -13.5, -10.5},
    };
}

bool pointInsideExpandedBox(Vec3 point, double radius, const CollisionBox& box) {
    return point.x > box.minX - radius && point.x < box.maxX + radius &&
           point.z > box.minZ - radius && point.z < box.maxZ + radius;
}

} // namespace

CollisionBox buildingCollisionBox(BuildingType type, GridPosition position) {
    const double halfExtent = type == BuildingType::Core ? 1.0 : 0.5;
    return {
        static_cast<double>(position.x) - halfExtent,
        static_cast<double>(position.x) + halfExtent,
        static_cast<double>(position.z) - halfExtent,
        static_cast<double>(position.z) + halfExtent,
    };
}

CollisionWorld::CollisionWorld() : CollisionWorld(48.0, defaultStaticColliders()) {}

CollisionWorld::CollisionWorld(double worldLimit, std::vector<CollisionBox> staticColliders)
    : worldLimit_(worldLimit), staticColliders_(std::move(staticColliders)),
      colliders_(staticColliders_) {
    colliders_.reserve(128);
}

void CollisionWorld::reset() {
    colliders_ = staticColliders_;
    colliders_.reserve(128);
}

void CollisionWorld::syncBuildings(const std::vector<BuildingInstance>& buildings) {
    colliders_ = staticColliders_;
    for (const auto& building : buildings) {
        if (buildingBlocksMovement(building)) {
            colliders_.push_back(buildingCollisionBox(building.type, building.gridPosition));
        }
    }
}

Vec3 CollisionWorld::moveCircle(Vec3 position, Vec3 delta, double radius) const {
    const double distance = std::sqrt((delta.x * delta.x) + (delta.z * delta.z));
    const int stepCount =
        std::max(1, static_cast<int>(std::ceil(distance / MaxMovementStep)));
    const double stepX = delta.x / static_cast<double>(stepCount);
    const double stepZ = delta.z / static_cast<double>(stepCount);

    for (int step = 0; step < stepCount; ++step) {
        Vec3 candidate = position;
        candidate.x =
            std::clamp(candidate.x + stepX, -worldLimit_ + radius, worldLimit_ - radius);
        const bool blockedX =
            std::any_of(colliders_.begin(), colliders_.end(), [candidate, radius](const auto& box) {
                return pointInsideExpandedBox(candidate, radius, box);
            });
        if (!blockedX) {
            position.x = candidate.x;
        }

        candidate = position;
        candidate.z =
            std::clamp(candidate.z + stepZ, -worldLimit_ + radius, worldLimit_ - radius);
        const bool blockedZ =
            std::any_of(colliders_.begin(), colliders_.end(), [candidate, radius](const auto& box) {
                return pointInsideExpandedBox(candidate, radius, box);
            });
        if (!blockedZ) {
            position.z = candidate.z;
        }
    }

    return position;
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
                                  candidate.maxZ > collider.minZ;
                       });
}

const std::vector<CollisionBox>& CollisionWorld::colliders() const {
    return colliders_;
}

} // namespace ian
