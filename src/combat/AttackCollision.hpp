#pragma once
#include "combat/DefenseTargeting.hpp"
#include "core/Geometry.hpp"
#include "enemies/EnemyCollision.hpp"
#include "world/SpatialHash.hpp"
#include <span>

namespace ian {
[[nodiscard]] inline Vec3 attackTargetPosition(const EnemyInstance& enemy) {
    // Models use a terrain-relative visual root, not navigation position.y.
    // These centers preserve the authored targeting heights of each silhouette.
    const double height = [&] {
        switch (enemy.type) {
        case EnemyType::AirTank: return 2.35;
        case EnemyType::Bee: return 2.10;
        case EnemyType::Flying: return 1.72;
        case EnemyType::Boss: return 1.25;
        case EnemyType::Heavy: return 0.86;
        case EnemyType::Splitter: return 0.90;
        case EnemyType::Ranged: return 0.70;
        case EnemyType::Sapper: return 0.62;
        case EnemyType::Basic: return 0.56;
        case EnemyType::Fast: return 0.48;
        case EnemyType::Splitling: return 0.44;
        }
        return 0.56;
    }();
    return {enemy.position.x, enemy.worldSurfaceHeight + height, enemy.position.z};
}

struct AttackContact { EntityId id; Vec3 position; double distance; };

// Rebuilt once per weapon tick. Narrow phase uses the existing gameplay
// capsules expanded by projectile radius, never the selected target's ID.
class AttackCollisionQuery {
  public:
    void rebuild(const EnemySystem& enemies) {
        hash_.clear();
        for (const auto& enemy : enemies.enemies()) if (enemy.active) hash_.insert(enemy.id, enemy.position);
    }
    template <typename Visitor>
    void nearby(const EnemySystem& enemies, Vec3 origin, double radius, Visitor&& visitor) const {
        hash_.forEachNearby(origin, radius, [&](const SpatialEntry& entry) {
            const auto enemy = enemies.enemy(entry.id);
            if (enemy && enemy->active) visitor(*enemy);
        });
    }
    [[nodiscard]] std::optional<AttackContact> sweep(
        const EnemySystem& enemies, Vec3 start, Vec3 end, double radius,
        BuildingType type, double surfaceHeight, std::span<const EntityId> history) const {
        const Vec3 path = geometry::subtract(end, start);
        const double length = geometry::length(path);
        if (length < 1e-9) return std::nullopt;
        const Vec3 direction = geometry::scale(path, 1.0 / length);
        std::optional<AttackContact> closest;
        nearby(enemies, geometry::scale(geometry::add(start, end), 0.5),
            length * 0.5 + radius + 1.0, [&](const EnemyInstance& enemy) {
            if (!projectileDefenseCanTarget(type, enemy, surfaceHeight) ||
                std::ranges::find(history, enemy.id) != history.end()) return;
            const auto capsule = enemyCapsule(enemy.type);
            const auto distance = geometry::rayVerticalCapsuleDistance(start, direction,
                attackTargetPosition(enemy), capsule.radius + radius, capsule.segmentHalfHeight);
            if (distance && *distance <= length && (!closest || *distance < closest->distance)) {
                closest = AttackContact{enemy.id,
                    geometry::add(start, geometry::scale(direction, *distance)), *distance};
            }
        });
        return closest;
    }
    [[nodiscard]] std::optional<EnemyInstance> bounceTarget(
        const EnemySystem& enemies, Vec3 origin, double range, BuildingType type,
        double surfaceHeight, std::span<const EntityId> history) const {
        std::optional<EnemyInstance> nearest;
        double distance = range * range;
        nearby(enemies, origin, range, [&](const EnemyInstance& enemy) {
            if (!projectileDefenseCanTarget(type, enemy, surfaceHeight) ||
                std::ranges::find(history, enemy.id) != history.end()) return;
            const double candidate = geometry::distanceSquared(origin, attackTargetPosition(enemy));
            if (candidate <= distance) { distance = candidate; nearest = enemy; }
        });
        return nearest;
    }
  private:
    SpatialHash hash_;
};
} // namespace ian
