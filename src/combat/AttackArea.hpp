#pragma once
#include "combat/AttackCollision.hpp"
#include "combat/AttackHit.hpp"

namespace ian {
// An area is an attack instance too. Its per-hit crit/knockback use the same
// flow as a bolt. Caller owns event presentation and any repeat-hit history.
template <typename Visitor>
void applyAttackArea(EnemySystem& enemies, const AttackCollisionQuery& query,
    Vec3 origin, double radius, const AttackStats& stats, std::uint64_t seed,
    Visitor&& visitor, std::vector<EntityId>* history = nullptr, bool includeFlying = true) {
    query.nearby(enemies, origin, radius + 1.0, [&](const EnemyInstance& enemy) {
        if (!includeFlying && enemyIsFlying(enemy.type)) return;
        if (history && std::ranges::find(*history, enemy.id) != history->end()) return;
        const double reach = radius + enemyCapsule(enemy.type).radius;
        const double dx = enemy.position.x - origin.x, dz = enemy.position.z - origin.z;
        if (dx * dx + dz * dz > reach * reach) return;
        if (history) history->push_back(enemy.id);
        if (const auto hit = applyAttackHit(enemies, enemy.id, stats,
            geometry::subtract(attackTargetPosition(enemy), origin),
            seed ^ (static_cast<std::uint64_t>(enemy.id.index) * 0x9e3779b97f4a7c15ULL))) visitor(*hit);
    });
}
} // namespace ian
