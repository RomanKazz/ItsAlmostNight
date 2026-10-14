#pragma once
#include "combat/AttackCollision.hpp"
#include "combat/AttackHit.hpp"
#include "combat/LightningTargetCache.hpp"
#include "buildings/BuildingSystem.hpp"
namespace ian {
inline constexpr double TeslaEnergyHeight = 1.260;
// Reusable history belongs to TowerSystem: no vector allocation per discharge.
template<typename Visitor>
void applyTeslaChain(EnemySystem& enemies, const AttackCollisionQuery& query,
    EntityId first, Vec3 energy, const FinalAttackStats& stats, std::uint64_t seed,
    std::vector<EntityId>& visited, Visitor&& visitor, LightningTargetCache* cache=nullptr) {
    visited.clear();
    std::optional<EnemyInstance> target = enemies.enemy(first);
    Vec3 origin=energy;
    const int jumps=effectiveAttackCount(stats.bounces);
    for (int jump=0;target && jump<=jumps;++jump) {
        const Vec3 point=attackTargetPosition(*target);
        visited.push_back(target->id);
        if(const auto hit=applyAttackHit(enemies,target->id,stats,geometry::subtract(point,origin),
                seed ^ (static_cast<std::uint64_t>(target->id.index)*0x9e3779b97f4a7c15ULL)))
            visitor(*hit,origin,point,jump);
        origin=point;
        target=cache ? cache->next(enemies,query,visited.back(),origin,stats.bounceRange,visited)
            : query.bounceTarget(enemies,origin,stats.bounceRange,BuildingType::TeslaTower,0,visited);
    }
}
}
