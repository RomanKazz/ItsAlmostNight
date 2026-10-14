#pragma once
#include "combat/AttackCollision.hpp"
#include <array>
#include <limits>
namespace ian {
// Nearby chains often visit the same crowd. Cache only the nearest candidates
// for this tick, then fall back to the spatial query for unusually long chains.
// Storage is bounded by enemy slots and reused without per-discharge allocation.
class LightningTargetCache {
public:
    LightningTargetCache():entries_(EnemySystem::MaxEnemies) {}
    void beginTick() {
        if(++epoch_==0) {for(auto& entry:entries_) entry.epoch=0;epoch_=1;}
    }
    std::optional<EnemyInstance> next(const EnemySystem& enemies,const AttackCollisionQuery& query,
        EntityId source,Vec3 origin,double radius,std::span<const EntityId> excluded) {
        const auto slot=enemies.slotIndex(source);
        if(!slot || *slot>=entries_.size()) return query.bounceTarget(enemies,origin,radius,BuildingType::TeslaTower,0,excluded);
        auto& entry=entries_[*slot];
        if(entry.epoch!=epoch_ || entry.id!=source || entry.radius!=radius || geometry::distanceSquared(entry.origin,origin)>1e-12) {
            entry.epoch=epoch_;entry.id=source;entry.radius=radius;entry.origin=origin;entry.count=0;
            query.nearby(enemies,origin,radius,[&](const EnemyInstance& candidate) {
                if(candidate.id==source) return;
                const double distance=geometry::distanceSquared(attackTargetPosition(candidate),origin);
                if(distance>radius*radius) return;
                std::size_t index=0;
                while(index<entry.count && entry.candidates[index].distance<=distance) ++index;
                if(index>=entry.candidates.size()) return;
                const std::size_t end=std::min(entry.count,entry.candidates.size()-1);
                for(std::size_t i=end;i>index;--i) entry.candidates[i]=entry.candidates[i-1];
                entry.candidates[index]={candidate.id,distance};entry.count=std::min(entry.count+1,entry.candidates.size());
            });
        }
        for(std::size_t i=0;i<entry.count;++i) {
            const auto id=entry.candidates[i].id;
            if(std::ranges::find(excluded,id)!=excluded.end()) continue;
            if(auto enemy=enemies.enemy(id)) return enemy;
        }
        if(entry.count==entry.candidates.size()) return query.bounceTarget(enemies,origin,radius,BuildingType::TeslaTower,0,excluded);
        return std::nullopt;
    }
private:
    struct Candidate {EntityId id;double distance;};
    struct Entry {
        EntityId id;Vec3 origin;double radius{};std::uint64_t epoch{};
        std::array<Candidate,16> candidates{};std::size_t count{};
    };
    std::vector<Entry> entries_;
    std::uint64_t epoch_{1};
};
}
