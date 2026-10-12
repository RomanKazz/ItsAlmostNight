#pragma once

#include "TestHarness.hpp"
#include "core/Geometry.hpp"
#include "enemies/EnemySystem.hpp"

inline void checkBossCounterplay() {
    using namespace ian;
    BuildingSystem base;
    base.setSandboxMode(true);
    require(base.place(BuildingType::Core,{0,0},0,30,0).has_value(),"counterplay fixture has a Core");
    FlowField flow;
    flow.rebuild({0,0},base.buildings());
    const Vec3 player{16,1.7,16};
    const std::array<EnemySpawn,1> spawn{{{.type=EnemyType::Boss,.position={0,3,-9}}}};
    const auto awaitStorm=[&](EnemySystem& enemies) {
        for(int step=0;step<2400;++step) {
            enemies.tick(1.0/60,base.buildings(),flow,player);
            static_cast<void>(enemies.takeBossActionEvents());
            if(enemies.enemies().front().state==EnemyState::BossStormWindup) return;
        }
        require(false,"boss starts an interruptible storm");
    };
    EnemySystem storm;
    storm.spawnWave(spawn);
    const auto id=storm.enemies().front().id;
    awaitStorm(storm);
    const double threshold=storm.enemies().front().bossStormInterruptDamage;
    requireNear(threshold,72,1e-9,"phase one interrupt requires three percent of scaled max HP");
    storm.damage(id,threshold*.4);
    require(storm.enemies().front().state==EnemyState::BossStormWindup,
        "partial damage does not interrupt the spell");
    static_cast<void>(storm.damageInRadius(storm.enemies().front().position,1,threshold*.6));
    require(storm.enemies().front().state==EnemyState::MoveToCore &&
        storm.enemies().front().bossStrikesRemaining==0 && storm.enemies().front().bossAbilityWindupRemaining==0,
        "direct and area damage combine to cancel all three storm strikes");
    const auto interrupted=storm.takeBossActionEvents();
    require(std::count_if(interrupted.begin(),interrupted.end(),[](const auto& action) {
        return action.type==BossActionType::Interrupted;
    })==1,"interruption emits one feedback event");
    storm.damage(id,1);
    require(storm.takeBossActionEvents().empty(),"extra hits cannot repeat the cancellation event");
    for(int step=0;step<60;++step) {
        storm.tick(1.0/60,base.buildings(),flow,player);
        require(storm.takeBossActionEvents().empty(),"interrupted storm cannot release stale impacts");
    }
    require(storm.enemies().front().bossInterruptRemaining>0,"interrupt gives a recovery window");
    awaitStorm(storm);
    require(storm.enemies().front().bossStormDamageTaken==0,"new storm resets accumulated interrupt damage");
    const double nextThreshold=storm.enemies().front().bossStormInterruptDamage;
    storm.damage(id,nextThreshold*.99);
    require(storm.enemies().front().state==EnemyState::BossStormWindup,"below-threshold damage allows the attack");
    // Once the preparation ends, damage cannot retroactively cancel queued strikes.
    storm.tick(storm.enemies().front().bossAbilityWindupRemaining+.01,base.buildings(),flow,player);
    static_cast<void>(storm.takeBossActionEvents());
    storm.damage(id,nextThreshold);
    require(storm.takeBossActionEvents().empty(),"damage after release is too late to interrupt");
    storm.tick(.7,base.buildings(),flow,player);
    const auto impacts=storm.takeBossActionEvents();
    require(std::count_if(impacts.begin(),impacts.end(),[](const auto& action) {
        return action.type==BossActionType::RoyalImpact && action.attack==RoyalAttack::Storm;
    })==3,"uninterrupted storm retains three strikes");
    storm.clearProjectiles();
    require(storm.enemies().front().bossStormDamageTaken==0 &&
        storm.enemies().front().bossInterruptRemaining==0,"combat cleanup clears counterplay state");
    storm.reset();storm.spawnWave(spawn);
    require(storm.enemies().front().bossStormInterruptDamage==0,"respawn cannot retain previous charge data");

    EnemySystem scaled;
    auto scaledSpawn=spawn;scaledSpawn[0].healthMultiplier=3.5;
    scaled.spawnWave(scaledSpawn);awaitStorm(scaled);
    requireNear(scaled.enemies().front().bossStormInterruptDamage,threshold*3.5,1e-8,
        "interrupt threshold follows actual wave-scaled boss health");

    const auto awaitSiegeVolley=[&](EnemySystem& enemies) {
        enemies.spawnWave(spawn);
        for(int step=0;step<3000;++step) {
            enemies.tick(1.0/60,base.buildings(),flow,player);
            static_cast<void>(enemies.takeBossActionEvents());
            const EnemyProjectile* center=nullptr;double alignment=-2;
            for(const auto& shot:enemies.projectiles()) if(shot.royal && !shot.targetsPlayer) {
                const double value=geometry::dot(geometry::normalizedOr(shot.velocity),
                    geometry::normalizedOr(geometry::subtract(shot.targetPosition,shot.position)));
                if(value>alignment) {alignment=value;center=&shot;}
            }
            if(center) return *center;
        }
        require(false,"boss fires a physical siege volley");
        return EnemyProjectile{};
    };
    // Use the first royal projectile's locked target to place a real conventional
    // wall in its path. All fixtures reach the same deterministic firing position.
    for(const int mode : {0,1,2,3,4,5,6}) {
        EnemySystem volley;
        const auto shot=awaitSiegeVolley(volley);
        auto structures=base.buildings();
        const Vec3 point=geometry::lerpClamped(shot.position,shot.targetPosition,.65);
        if(mode<=3) {
            if(mode!=0) for(int offset=-1;offset<=1;++offset) {
            const bool alongZ=std::abs(shot.targetPosition.z-shot.position.z)>
                std::abs(shot.targetPosition.x-shot.position.x);
            structures.push_back({.id={static_cast<std::uint32_t>(5501+offset),1},.type=mode==2?BuildingType::Gate:BuildingType::Wall,
                .gridPosition={static_cast<int>(std::floor(point.x))+(alongZ?offset:0),
                    static_cast<int>(std::floor(point.z))+(alongZ?0:offset)},
                .health=220,.maxHealth=220,.open=mode==2,.baseHeight=mode==3?10.0:0.0});
            }
        }
        const auto blocker=[&](double fraction,EntityId wallId) {
            const Vec3 p=geometry::lerpClamped(shot.position,shot.targetPosition,fraction);
            const bool alongZ=std::abs(shot.targetPosition.z-shot.position.z)>
                std::abs(shot.targetPosition.x-shot.position.x);
            const double hx=alongZ?5:.09,hz=alongZ?.09:5;
            return EnemyStructureTarget{.id=wallId,.position={p.x,0,p.z},.radius=.1,.modular=true,
                .projectileBlocker=EnemyProjectileBlocker{{p.x-hx,0,p.z-hz},{p.x+hx,5,p.z+hz}}};
        };
        std::array walls{blocker(.8,{5600,1}),blocker(.4,{5601,1})};
        if(mode==5) {
            walls[0].projectileBlocker->minimum.y=10;
            walls[0].projectileBlocker->maximum.y=11;
            walls[1].projectileBlocker.reset();
        }
        if(mode==6) {
            const auto d=geometry::subtract(shot.targetPosition,shot.position);
            for(auto& wall:walls) {
                wall.projectileBlocker->minimum=geometry::add(wall.projectileBlocker->minimum,d);
                wall.projectileBlocker->maximum=geometry::add(wall.projectileBlocker->maximum,d);
            }
        }
        // A large step crosses both walls and the Core in one update.
        const auto hits=volley.tick(1.2,structures,flow,player,
            mode>=4 ? std::span<const EnemyStructureTarget>{walls} : std::span<const EnemyStructureTarget>{});
        const auto count=[&](EntityId target) {return std::count_if(hits.begin(),hits.end(),[&](const auto& hit) {
            return hit.targetId==target;
        });};
        if(mode==1) require(count({5500,1})+count({5501,1})+count({5502,1})>0 && count(base.core()->id)==0,
            "closed conventional wall absorbs siege damage and shields the Core");
        else if(mode==4) require(count({5601,1})>0 && count({5600,1})==0 && count(base.core()->id)==0,
            "nearest modular wall wins regardless of input order, without tunneling or duplicate damage");
        else require(count(base.core()->id)>0,
            "unprotected Core still gets hit: open gates, wrong heights and walls behind it do not block");
    }
    BossBattleConfig invalid;
    invalid.stormInterruptHealthFraction=0;
    require(!invalid.valid(),"zero interrupt threshold is rejected");
}
