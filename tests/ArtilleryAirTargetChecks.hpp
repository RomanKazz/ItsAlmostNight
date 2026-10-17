#pragma once
#include "TestHarness.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/AttackCollision.hpp"

namespace artillery_air_target_checks {
inline void run() {
    using namespace ian;
    constexpr double Step = 1.0 / 60.0;
    for (const auto type : {BuildingType::Catapult, BuildingType::Mortar, BuildingType::Cannon}) {
        const bool groundOnly = type != BuildingType::Cannon;
        const std::vector<BuildingInstance> buildings{{.id={9100,1},.type=type,.gridPosition={0,0}}};
        for (bool marked : {false, true}) {
            EnemySystem air;
            air.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Flying,.position={0,0,-10}}});
            if (marked) air.markTarget(air.enemies()[0].id,30,1.2);
            CannonSystem defense;defense.syncBuildings(buildings);defense.setPreferMarkedTargets(marked);
            bool fired=false;
            for (int frame=0;frame<240;++frame) {
                defense.tick(Step,buildings,air);fired|=!defense.shots().empty();
            }
            require(fired!=groundOnly,"only ground artillery ignores lone airborne targets, including marked targets");
        }
        auto definitions=GameBalance::defaults().enemies;
        for(auto& definition:definitions)definition.health=100000;
        EnemySystem mixed(definitions);
        mixed.spawnGroup(std::array{
            EnemySpawn{.type=EnemyType::Basic,.position={0,0,-10}},
            EnemySpawn{.type=EnemyType::Basic,.position={0.8,0,-10}},
            EnemySpawn{.type=EnemyType::Flying,.position={0.1,0,-10}},
            EnemySpawn{.type=EnemyType::Flying,.position={0.2,0,-10}}});
        mixed.markTarget(mixed.enemies()[2].id,30,1.2);
        CannonSystem defense;defense.syncBuildings(buildings);defense.setPreferMarkedTargets(true);
        defense.setModifiers(1,1,1,1,3);
        AttackModifiers modifiers;modifiers[AttackStat::Pierce].flat=2;
        modifiers[AttackStat::Bounces].flat=2;defense.setAttackModifiers(modifiers);
        bool groundHit=false,airHit=false,cluster=false;
        for (int frame=0;frame<720;++frame) {
            defense.tick(Step,buildings,mixed);
            if(groundOnly && defense.cannons()[0].targetId) {
                require(mixed.enemy(*defense.cannons()[0].targetId)->type!=EnemyType::Flying,
                    "mixed groups never redirect ground artillery toward marked flying targets");
            }
            for(const auto& hit:defense.hits()) {
                groundHit|=hit.result.type==EnemyType::Basic;
                airHit|=hit.result.type==EnemyType::Flying;
            }
            for(const auto& shell:defense.projectiles())cluster|=shell.clusterExplosionCount==3;
        }
        require(groundHit && cluster,"ground artillery still fires physical shells and cluster blasts at ground groups");
        require(airHit!=groundOnly,"ground-only filtering also covers pierce, splash and cluster blasts without changing cannon splash");
        AttackCollisionQuery query;query.rebuild(mixed);
        const std::array groundHistory{mixed.enemies()[0].id,mixed.enemies()[1].id};
        const auto bounce=query.bounceTarget(mixed,{0,0,-10},10,type,0,groundHistory);
        require(bounce.has_value()!=groundOnly,"ricochets cannot retarget airborne enemies from a ground-only weapon");
    }
}
}
