#pragma once
#include "TestHarness.hpp"
#include "combat/DefenseTomeScaling.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/TeslaAttack.hpp"
#include "combat/TrapSystem.hpp"
#include "combat/AttackStatCompatibility.hpp"

namespace defense_tome_scaling_checks {
inline void run() {
    using namespace ian;
    constexpr double Step=1.0/120;
    for(const auto type:{BuildingType::GunTurret,BuildingType::Turret,BuildingType::Cannon,
        BuildingType::Catapult,BuildingType::Mortar,BuildingType::MissileLauncher,
        BuildingType::TeslaTower,BuildingType::LaserTower,BuildingType::SpikeTrap}) {
        const bool ballistic=type==BuildingType::Cannon || type==BuildingType::Catapult || type==BuildingType::Mortar;
        const bool spike=type==BuildingType::SpikeTrap;
        for(std::uint8_t level:std::array<std::uint8_t,3>{1,4,8}) {
            const auto base=ballistic?CannonSystem::baseStats(type,level):spike?
                TrapSystem::baseStats(type,level):TowerSystem::baseStats(type,level);
            AttackModifiers fraction;
            fraction[AttackStat::Quantity].flat=1.6;
            const auto partial=resolveAttackStats(base,std::array{fraction});
            fraction[AttackStat::Quantity].flat=1.4;
            AttackModifiers first;first[AttackStat::Quantity].flat=1.6;
            const auto full=resolveAttackStats(base,std::array{first,fraction});
            requireNear(partial.quantity-base.quantity,1.6,1e-12,"fractional counts are not scaled or rounded when awarded");
            requireNear(full.quantity-base.quantity,3,1e-12,"independent fractional sources retain their whole total");
            const auto response=defenseTomeScaling(type);
            double budget=0;
            for(int index=0;index<effectiveAttackCount(full.quantity,1);++index)
                budget+=defenseInstanceStats(full,type,index).damage;
            requireNear(budget,base.damage*(base.quantity+3*response.extraInstanceDamage),1e-10,
                "three extra instances add their weighted damage without weakening authored instances");
            const auto native=resolveAttackStats(base);
            requireNear(defenseContactStats(native,type,effectiveAttackCount(base.bounces),
                effectiveAttackCount(base.pierce)).damage,base.damage,1e-12,
                "all authored penetration and chain jumps keep their original damage");
            requireNear(defenseContactStats(native,type,effectiveAttackCount(base.bounces)+2,
                effectiveAttackCount(base.pierce)+1).damage,
                base.damage*response.bounceRetention*response.bounceRetention*response.pierceRetention,1e-12,
                "bonus chain and penetration losses compose independently");
        }
        // Exercise actual consumers, not just the coefficient helper. Keep
        // physical shots stationary so their configuration can be inspected.
        const std::vector<BuildingInstance> buildings{{.id={9730,1},.type=type,.level=4}};
        auto definitions=GameBalance::defaults().enemies;
        for(auto& d:definitions)d.health=1000000;
        EnemySystem enemies(definitions);
        enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Heavy,.position={0,0,-6}}});
        if(spike) {
            enemies.reset();
            enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Heavy,.position={0,0,0}}});
        }
        AttackModifiers modifiers;modifiers[AttackStat::Quantity].flat=2.8;
        if(type!=BuildingType::Mortar)modifiers[AttackStat::ProjectileSpeed].multiplier=0;
        TowerSystem tower;CannonSystem cannon;TrapSystem trap;
        tower.setAttackModifiers(modifiers);cannon.setAttackModifiers(modifiers);trap.setAttackModifiers(modifiers);
        tower.syncBuildings(buildings);cannon.syncBuildings(buildings);trap.syncBuildings(buildings);
        const auto stats=ballistic?cannon.finalStats(buildings[0]):spike?trap.finalStats(buildings[0]):tower.finalStats(buildings[0]);
        const int count=effectiveAttackCount(stats.quantity,1),native=effectiveAttackCount(stats.nativeQuantity,1);
        double damage=0,firstDamage=0;int contacts=0;
        for(int frame=0;frame<480;++frame) {
            if(ballistic) {
                cannon.tick(Step,buildings,enemies);
                if(cannon.shots().empty())continue;
                for(const auto& shot:cannon.projectiles())if(shot.active){damage+=shot.configuration.stats.damage;++contacts;}
                firstDamage=stats.damage;break;
            }
            if(spike) {
                trap.tick(Step,buildings,enemies);
                for(const auto& hit:trap.hits()){if(!contacts)firstDamage=hit.result.damage;damage+=hit.result.damage;++contacts;}
            } else {
                const auto hits=tower.tick(Step,buildings,enemies);
                if(type==BuildingType::LaserTower || type==BuildingType::TeslaTower) {
                    for(const auto& hit:hits){if(!contacts)firstDamage=hit.payloadDamage;damage+=hit.payloadDamage;++contacts;}
                } else if(tower.projectiles().size()==static_cast<std::size_t>(count)) {
                    for(const auto& shot:tower.projectiles())if(shot.active){damage+=shot.configuration.stats.damage;++contacts;}
                    firstDamage=stats.damage;break;
                }
            }
            if(contacts>=count)break;
        }
        require(contacts==count,"every whole bonus instance reaches its real consumer");
        requireNear(damage,firstDamage*(native+(count-native)*defenseTomeScaling(type).extraInstanceDamage),1e-7,
            "real projectiles, rays, discharges and spike pulses obey the same damage budget");
    }
    {
        const BuildingInstance building{.type=BuildingType::Cannon};
        CannonSystem cannon;cannon.setAttackStyle(CannonAttackStyle::Scatter);
        AttackModifiers extra;extra[AttackStat::Quantity].flat=2;
        cannon.setAttackModifiers(extra);
        const auto stats=cannon.finalStats(building);
        require(stats.nativeQuantity==5 && stats.quantity==7,
            "class Scatter pellets remain native while tome pellets are counted separately");
        for(int i=0;i<7;++i)requireNear(defenseInstanceStats(stats,building.type,i).damage,
            stats.damage*(i<5?1.0:.25),1e-12,"Scatter keeps all five authored pellets at full damage");
        TrapSystem spikes;AttackModifiers duration;duration[AttackStat::Duration].multiplier=100;
        const BuildingInstance trap{.type=BuildingType::SpikeTrap};
        const auto before=spikes.finalStats(trap);spikes.setAttackModifiers(duration);
        const auto after=spikes.finalStats(trap);
        require(before.duration==after.duration && before.attackInterval==after.attackInterval,
            "taking duration cannot stretch spike animation or impose a slower attack cycle");
    }
    require(!supportsAttackStat(BuildingType::SpikeTrap,AttackStat::Duration),
        "duration cannot lengthen spike cooldown under the guise of an upgrade");
    require(!supportsAttackStat(BuildingType::LaserTower,AttackStat::Knockback),
        "a percentage knockback upgrade is not advertised on a zero-knockback laser");
    {
        auto definitions=GameBalance::defaults().enemies;
        for(auto& d:definitions)d.health=1000000;
        EnemySystem enemies(definitions);
        enemies.spawnGroup(std::array{
            EnemySpawn{.position={0,0,-2}},EnemySpawn{.position={0,0,-4}},
            EnemySpawn{.position={0,0,-6}},EnemySpawn{.position={2,0,-6}},EnemySpawn{.position={4,0,-6}}});
        AttackStats base;base.damage=10;base.pierce=1;base.bounces=1;
        base.projectileSpeed=20;base.duration=4;
        AttackModifiers bonus;bonus[AttackStat::Pierce].flat=1;bonus[AttackStat::Bounces].flat=1;
        const auto stats=resolveAttackStats(base,std::array{bonus});
        AttackProjectileSystem shots;shots.spawn({9750,1},BuildingType::GunTurret,{0,.56,0},{0,0,-1},stats,0);
        std::vector<ProjectileImpact> hits;
        for(int frame=0;frame<240;++frame) {
            const auto step=shots.tick(Step,enemies);hits.insert(hits.end(),step.begin(),step.end());
        }
        require(hits.size()==5,"a real bullet spends native and bonus pierce before native and bonus bounces");
        constexpr std::array expected{10.0,10.0,8.5,8.5,6.375};
        for(std::size_t i=0;i<hits.size();++i) {
            requireNear(hits[i].hit.result.damage,expected[i],1e-10,"physical contacts apply attenuation only after native interactions");
            requireNear(hits[i].payloadDamage,expected[i],1e-10,"item effects receive the actual attenuated payload");
        }
    }
    // Natural Tesla jumps stay full strength; the two added jumps attenuate.
    auto definitions=GameBalance::defaults().enemies;
    for(auto& d:definitions)d.health=1000000;
    EnemySystem enemies(definitions);
    enemies.spawnGroup(std::array{
        EnemySpawn{.position={0,0,-3}},EnemySpawn{.position={1,0,-3}},
        EnemySpawn{.position={2,0,-3}},EnemySpawn{.position={3,0,-3}},EnemySpawn{.position={4,0,-3}}});
    AttackCollisionQuery query;query.rebuild(enemies);
    auto base=TowerSystem::baseStats(BuildingType::TeslaTower,1);
    AttackModifiers extra;extra[AttackStat::Bounces].flat=2;
    const auto stats=resolveAttackStats(base,std::array{extra});
    std::vector<EntityId> visited;std::vector<double> damage;
    applyTeslaChain(enemies,query,enemies.enemies()[0].id,{},stats,77,visited,
        [&](const AttackHit& hit,Vec3,Vec3,int){damage.push_back(hit.payloadDamage);});
    require(damage.size()==5,"two bonus chain targets remain real hits");
    for(int i=0;i<5;++i)requireNear(damage[static_cast<std::size_t>(i)],base.damage*std::pow(.75,std::max(0,i-2)),1e-12,
        "only jumps beyond Tesla's native chain lose energy");
}
}
