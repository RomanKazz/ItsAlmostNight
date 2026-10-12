#pragma once

#include "TestHarness.hpp"
#include "enemies/EnemySystem.hpp"

inline void checkAlpakingBattle() {
    using namespace ian;
    BuildingSystem buildings;
    buildings.setSandboxMode(true);
    require(buildings.place(BuildingType::Core,{0,0},0,30,0).has_value(),"royal battle fixture has core");
    FlowField flow;flow.rebuild({0,0},buildings.buildings());
    EnemySystem king;
    const std::array<EnemySpawn,1> spawn{{{.type=EnemyType::Boss,.position={0,3,-9}}}};
    king.spawnWave(spawn);
    const EntityId id=king.enemies().front().id;
    require(king.enemies().front().maxHealth==2400,"Alpaking base health replaces the fragile old boss");
    king.tick(1.0/60.0,buildings.buildings(),flow,Vec3{0,1.7,0});
    require(king.enemies().front().state==EnemyState::BossPhaseTransition,"king telegraphs arrival");
    const auto shielded=king.damage(id,100);
    require(shielded && std::abs(shielded->damage-35)<1e-8,"visible crown shield reduces intro damage");
    static_cast<void>(king.takeBossActionEvents());
    bool summoned=false,volley=false,impact=false,travelHit=false,locked=false;
    Vec3 lockedTarget{};
    for(int tick=0;tick<1800 && !(summoned && volley && impact && travelHit);++tick) {
        const Vec3 player=locked ? Vec3{7,1.7,0} : Vec3{0,1.7,0};
        king.tick(1.0/60.0,buildings.buildings(),flow,player);
        for(const auto& event:king.takeBossActionEvents()) {
            if(event.type==BossActionType::WarCry) {summoned=true;require(event.count==3,"phase one summons three familiars");}
            if(event.type==BossActionType::Volley) {volley=true;require(event.count==5,"phase one volley emits five physical shots");}
            if(event.type==BossActionType::Telegraph && event.attack==RoyalAttack::Dive) {
                locked=true;lockedTarget=event.position;
                require(event.warningSeconds>=1.2 && event.radius==2.7,"dive has a readable warning and exact radius");
            }
            if(event.type==BossActionType::RoyalImpact && event.attack==RoyalAttack::Dive) {
                impact=true;requireNear(event.position.x,lockedTarget.x,1e-8,"dive cannot home after warning");
                requireNear(event.position.z,lockedTarget.z,1e-8,"dive retains locked Z");
            }
        }
        if(!king.playerAttacks().empty()) travelHit=true;
        require(king.enemies().front().state!=EnemyState::BossRamWindup,"new king never uses instant legacy ram");
        require(king.projectiles().size()<40,"royal shot storage stays bounded in a single boss fight");
    }
    require(summoned && volley && impact && travelHit,"phase one performs summons, volley, physical hit and locked dive");
    // Damage thresholds and storms replace the old conditional melee slam.
    king.damage(id,king.enemies().front().health-king.enemies().front().maxHealth*.5);
    king.tick(1.0/60.0,buildings.buildings(),flow,Vec3{0,1.7,0});
    auto actions=king.takeBossActionEvents();
    require(king.enemies().front().bossPhase==2,"phase two begins below sixty percent HP");
    king.tick(1.1,buildings.buildings(),flow,Vec3{0,1.7,0});static_cast<void>(king.takeBossActionEvents());
    bool storm=false;int warnings=0,strikes=0;
    for(int tick=0;tick<1800 && strikes<3;++tick) {
        king.tick(1.0/60.0,buildings.buildings(),flow,Vec3{0,1.7,0});
        for(const auto& event:king.takeBossActionEvents()) {
            if(event.type==BossActionType::Telegraph && event.attack==RoyalAttack::Storm) {storm=true;++warnings;}
            if(event.type==BossActionType::RoyalImpact && event.attack==RoyalAttack::Storm) ++strikes;
        }
    }
    require(storm && warnings==3 && strikes==3,"phase two storm warns three distinct blasts before releasing each once");
    king.damage(id,king.enemies().front().health-king.enemies().front().maxHealth*.25);
    king.tick(1.0/60.0,buildings.buildings(),flow,Vec3{0,1.7,0});static_cast<void>(king.takeBossActionEvents());
    require(king.enemies().front().bossPhase==3,"royal fury begins below thirty percent HP");
    king.tick(1.1,buildings.buildings(),flow,Vec3{0,1.7,0});static_cast<void>(king.takeBossActionEvents());
    bool furyVolley=false;
    for(int tick=0;tick<1000 && !furyVolley;++tick) {
        king.tick(1.0/60.0,buildings.buildings(),flow,Vec3{0,1.7,0});
        for(const auto& event:king.takeBossActionEvents()) if(event.type==BossActionType::Volley) {
            require(event.count==9,"royal fury has nine independently physical projectiles");furyVolley=true;
        }
    }
    require(furyVolley,"phase three continues attack sequences");
    // Fill the court to its configured cap, then force the next summon turn.
    std::array<EnemySpawn,12> court{};
    for(std::size_t i=0;i<court.size();++i) court[i]={.type=EnemyType::Flying,
        .position={30+double(i)*3,2.22,20},.summonerId=id};
    king.spawnGroup(court);
    for(int tick=0;tick<2000;++tick) {
        king.tick(1.0/60.0,buildings.buildings(),flow,Vec3{0,1.7,0});
        for(const auto& event:king.takeBossActionEvents()) if(event.type==BossActionType::WarCry)
            require(event.count==0,"a full court cannot create more summons");
    }
    king.damage(id,100000);
    require(std::none_of(king.enemies().begin(),king.enemies().end(),[&](const EnemyInstance& e){
        return e.active && (e.id==id || e.summonerId==id);}),"king death dismisses his court");
    require(std::none_of(king.projectiles().begin(),king.projectiles().end(),[&](const EnemyProjectile& p){return p.ownerId==id;}),
            "king death removes remaining royal shots");
    king.reset();require(king.takeBossActionEvents().empty() && king.projectiles().empty(),"reset clears royal sequences");
    BossBattleConfig invalid;invalid.phaseThree=.8;require(!invalid.valid(),"invalid phase order is rejected");
}
