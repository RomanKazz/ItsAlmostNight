#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "game/StumpRoots.hpp"
#include "progression/MetaProgression.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <set>

namespace ian {
// Reuse the simulation's existing test seam; no runtime debug API is needed.
struct RelicTestAccess {
    static EnemySystem& enemies(Simulation& s) { return s.enemies_; }
    static void position(Simulation& s) { s.playerPosition_={0,1.7,0}; s.playerYaw_=3.141592653589793; s.playerPitch_=0; s.snapshotCache_.reset(); }
    static void strike(Simulation& s) {
        s.pickaxeCooldownRemaining_=0;
        s.updatePlayerActions(0,{.usePickaxe=true});
        s.snapshotCache_.reset();
    }
    static void roots(Simulation& s,double dt) { s.updateStumpRootWaves(dt); s.snapshotCache_.reset(); }
};
}

int main() {
    using namespace ian;
    using A=RelicTestAccess;
    auto balance=GameBalance::defaults();
    balance.gameplay.pickaxeDamageVariation=0;
    balance.gameplay.pickaxeCriticalChance=0;
    auto map=MapDefinition::defaults(); map.resources.clear(); map.obstacles.clear();
    auto world=WorldConfig::defaults(); world.terrainAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    const auto create=[&](PlayerClass type=PlayerClass::Stump) {
        auto s=std::make_unique<Simulation>(balance,map,world);
        s->startRunFromSeed(type,0x5354554dU);
        A::position(*s);
        return s;
    };
    const auto spawn=[](Simulation& s,std::initializer_list<EnemySpawn> values) {
        A::enemies(s).spawnGroup(std::span(values.begin(),values.size()));
    };
    const auto count=[](const auto& events,GameEventType type) {
        return std::ranges::count_if(events,[&](const auto& e){return e.type==type;});
    };
    const auto stock=[&](Simulation& s) {
        spawn(s,{{.type=EnemyType::Basic,.position={0,0,2},.healthMultiplier=1000},
                 {.type=EnemyType::Basic,.position={.3,0,2.2},.healthMultiplier=1000},
                 {.type=EnemyType::Basic,.position={-.3,0,2.2},.healthMultiplier=1000}});
    };
    const auto charge=[&](Simulation& s) {
        stock(s);
        for(int hit=0;hit<4;++hit) A::strike(s);
    };
    {
        auto s=create(),baseline=create(PlayerClass::Prospector);
        require(s->snapshot().selectedWeapon==PlayerWeapon::Club,"Stump starts with the club selected");
        requireNear(s->snapshot().playerMaxHealth,baseline->snapshot().playerMaxHealth*1.25,1e-9,"Stump has 25% extra HP");
        require(playerClassDefinition(PlayerClass::Stump)->startingBuilding==BuildingType::SpikeTrap,"Stump starts with the spikes blueprint");
        requireNear(playerClassDefinition(PlayerClass::Stump)->moveSpeedMultiplier,.9,1e-12,"Stump is 10% slower");
        const auto one=playerClassLevelBonuses(PlayerClass::Stump,1),twentyOne=playerClassLevelBonuses(PlayerClass::Stump,21);
        requireNear(one.meleeDamage,1,1e-12,"level one has no earned growth");
        requireNear(twentyOne.meleeDamage,1.4,1e-12,"melee growth is additive per earned level");
        requireNear(twentyOne.rootWaveScale,1.1,1e-12,"roots grow by half a percent per level");
        stock(*s);
        for(int hit=1;hit<=4;++hit) {
            static_cast<void>(s->takeEvents());
            A::strike(*s);
            const auto events=s->takeEvents();
            require(count(events,GameEventType::PickaxeHit)==3,"club sweep damages three enemies");
            require(s->snapshot().classAbilityState.stumpCombo==hit%4,"multi-target sweep charges exactly once");
            require(count(events,GameEventType::StumpRootWave)==(hit==4?1:0),"only fourth successful swing releases roots");
            if(hit==4) {
                const auto wave=std::ranges::find(events,GameEventType::StumpRootWave,&GameEvent::type);
                const auto strike=std::ranges::find(events,GameEventType::PickaxeHit,&GameEvent::type);
                requireNear(wave->damage,strike->damage*.7,1e-9,"roots inherit 70% of triggering melee damage");
            }
        }
    }
    {
        auto s=create(); A::strike(*s);
        require(s->snapshot().classAbilityState.stumpCombo==0,"misses do not charge roots");
        charge(*s); static_cast<void>(s->takeEvents());
        A::enemies(*s).reset();
        spawn(*s,{{.type=EnemyType::Basic,.position={0,0,1}},
                  {.type=EnemyType::Basic,.position={1,0,4}},
                  {.type=EnemyType::Basic,.position={0,0,6.4}},
                  {.type=EnemyType::Flying,.position={0,0,2}},
                  {.type=EnemyType::Basic,.position={0,0,-2}},
                  {.type=EnemyType::Basic,.position={5,0,3}},
                  {.type=EnemyType::Basic,.position={0,3,3}}});
        std::set<unsigned> hitIds;
        for(int step=0;step<21;++step) {
            A::roots(*s,.02);
            const auto events=s->takeEvents();
            for(const auto& e:events) if(e.type==GameEventType::StumpRootHit) {
                require(e.entityId && hitIds.insert(e.entityId->index).second,"one root wave never hits the same enemy twice");
            }
        }
        require(hitIds.size()==3,"roots reach the swept cone but exclude air, platforms, behind and outside");
        for(std::size_t i=0;i<3;++i) {
            const auto& slow=A::enemies(*s).enemies()[i].statusEffects[1];
            requireNear(slow.remaining,1.5,1e-12,"root slow lasts 1.5 seconds");
            requireNear(slow.intensity,.4,1e-12,"root slow is 40 percent");
        }
        A::roots(*s,5);
        require(count(s->takeEvents(),GameEventType::StumpRootHit)==0,"finished wave cannot deal more damage");
        require(s->snapshot().classAbilityState.stumpCombo==0,"secondary root hits do not recharge the passive");
    }
    {
        auto s=create(); charge(*s); static_cast<void>(s->takeEvents()); A::enemies(*s).reset();
        spawn(*s,{{.type=EnemyType::Basic,.position={0,0,6},.healthMultiplier=.001}});
        const auto target=A::enemies(*s).enemies().front();
        static_cast<void>(A::enemies(*s).damage(target.id,target.health-.1));
        A::roots(*s,1);
        const auto events=s->takeEvents();
        require(count(events,GameEventType::StumpRootHit)==1 && count(events,GameEventType::EnemyKilled)==1,
            "long frame sweeps full range and root kills produce normal reward events");
    }
    {
        auto s=create(); stock(*s);
        for(int i=0;i<3;++i) A::strike(*s);
        const auto state=s->saveSuspendedRunState();
        auto loaded=create(); loaded->returnToMainMenu();
        require(loaded->loadSuspendedRunState(state),"Stump is accepted by suspended-run validation");
        require(loaded->snapshot().playerClass==PlayerClass::Stump && loaded->snapshot().classAbilityState.stumpCombo==3,
            "class ID and three-hit charge survive loading");
        A::position(*loaded); stock(*loaded); static_cast<void>(loaded->takeEvents()); A::strike(*loaded);
        require(count(loaded->takeEvents(),GameEventType::StumpRootWave)==1,"first hit after resume releases charged roots");
        auto invalid=state; invalid.classAbilityState.stumpCombo=4;
        require(!loaded->loadSuspendedRunState(invalid),"invalid root charge is rejected");
        auto legacy=create(PlayerClass::Vanguard);
        const auto old=legacy->saveSuspendedRunState(); loaded->returnToMainMenu();
        require(loaded->loadSuspendedRunState(old) && loaded->snapshot().playerClass==PlayerClass::Vanguard,
            "old Vanguard checkpoints retain their class rather than becoming Stump");
        requireNear(playerClassDefinition(PlayerClass::Vanguard)->maxHealthMultiplier,1.35,1e-12,"legacy Vanguard balance is unchanged");
    }
    std::cout<<"Stump combat, progression and checkpoint tests passed\n";
}
