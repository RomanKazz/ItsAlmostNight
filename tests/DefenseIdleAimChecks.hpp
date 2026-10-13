#pragma once

#include "TestHarness.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/TowerSystem.hpp"

#include <array>
#include <cmath>

namespace defense_idle_aim_checks {

template <typename System, typename Runtime>
void check(ian::BuildingType type, System& system, Runtime runtime) {
    using namespace ian;
    std::vector<BuildingInstance> registry{{.id={995,1}, .type=type}};
    auto definitions=GameBalance::defaults().enemies;
    for (auto& definition:definitions) definition.health=100000;
    const bool ballistic=type==BuildingType::Cannon || type==BuildingType::Catapult;
    const Vec3 target=ballistic ? Vec3{4,1,-7} : Vec3{1,.8,-2};
    const auto enemiesAt=[&](Vec3 position) {
        EnemySystem enemies(definitions);
        const std::array<EnemySpawn,1> spawns{{{.type=EnemyType::Basic,.position=position}}};
        enemies.spawnWave(spawns);
        return enemies;
    };
    auto targets=enemiesAt(target);
    system.syncBuildings(registry);
    for(int tick=0;tick<180;++tick) system.tick(1.0/60,registry,targets);
    require(std::abs(runtime().yaw)>.1,"all five directional defenses acquire an off-axis enemy");

    // A living target leaving coverage must preserve the whole aiming pose,
    // including zeroing its old steering velocity. Catapult arm animation
    // finishes its throw separately from horizontal aiming.
    auto distant=enemiesAt({80,0,80});
    const double yaw=runtime().yaw;
    const double pitch=runtime().pitch;
    for(int tick=0;tick<240;++tick) {
        system.tick(1.0/60,registry,distant);
        if(tick%15==0) system.syncBuildings(registry);
    }
    require(!runtime().targetId,"defense discards enemies outside its sector/range");
    requireNear(runtime().yaw,yaw,1e-12,"idle defense preserves its last yaw through periodic synchronization");
    if(type!=BuildingType::Catapult)
        requireNear(runtime().pitch,pitch,1e-12,"idle defense preserves its last pitch");
    requireNear(runtime().yawVelocity,0,1e-12,"idle defense clears old yaw inertia");

    // Reacquire from the held pose rather than snapping to the new enemy.
    targets=enemiesAt(ballistic ? Vec3{-2,1,-7} : Vec3{-1,.8,-2});
    system.tick(1.0/60,registry,targets);
    require(std::abs(wrapBuildingAngle(runtime().yaw-yaw))<.08,"reacquisition turns smoothly from the held yaw");
    for(int tick=0;tick<180;++tick) system.tick(1.0/60,registry,targets);
    require(runtime().yaw>.1,"idle defense can aim in the other direction after reacquiring");
    targets.reset();
    system.tick(1.0/60,registry,targets);
    const double afterDeath=runtime().yaw;
    for(int tick=0;tick<120;++tick) system.tick(1.0/60,registry,targets);
    requireNear(runtime().yaw,afterDeath,1e-12,"target death does not send defense back to placement yaw");

    registry.front().rotation=2;
    system.syncBuildings(registry);
    require(runtime().manualTurnPending,"explicit player rotation requests an idle turn");
    system.tick(1.0/60,registry,targets);
    require(std::abs(wrapBuildingAngle(runtime().yaw-afterDeath))<.5,"manual rotation remains eased");
    for(int tick=0;tick<180;++tick) system.tick(1.0/60,registry,targets);
    requireNear(runtime().yaw,PiRadians*.5,1e-9,"explicit manual rotation still turns idle defenses");
}

inline void towers() {
    for(const auto type:{ian::BuildingType::Turret,ian::BuildingType::GunTurret,ian::BuildingType::Flamethrower}) {
        ian::TowerSystem system;
        check(type,system,[&]() -> const ian::TowerRuntime& {return system.towers().front();});
    }
}

inline void ballistics() {
    for(const auto type:{ian::BuildingType::Cannon,ian::BuildingType::Catapult}) {
        ian::CannonSystem system;
        check(type,system,[&]() -> const ian::CannonRuntime& {return system.cannons().front();});
    }
}

} // namespace defense_idle_aim_checks
