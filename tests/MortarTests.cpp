#include "TestHarness.hpp"
#include "combat/MortarAttack.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/AttackStatCompatibility.hpp"
#include "game/Simulation.hpp"
#include <memory>

using namespace ian;
namespace {
constexpr double Step = 1.0 / 120.0;
const BuildingInstance Mortar{.id={978,1},.type=BuildingType::Mortar,.gridPosition={0,0}};
EnemySystem crowd(double distance=10) {
    auto definitions=GameBalance::defaults().enemies;
    for(auto& definition:definitions)definition.health=100000;
    EnemySystem enemies(definitions);
    enemies.spawnWave(std::vector<EnemySpawn>{{.type=EnemyType::Basic,.position={0,0,-distance}},
        {.type=EnemyType::Basic,.position={.8,0,-distance-.5}}});
    return enemies;
}
void trajectory() {
    for(double distance:{4.0,10.0,18.0})for(double height:{-1.0,0.0,2.0}) {
        const Vec3 origin{0,1.2,0},target{0,height,-distance};
        const auto arc=mortarTrajectory(origin,target,MortarHorizontalSpeed);
        require(arc.has_value(),"mortar solves reachable targets on uneven terrain");
        const auto& a=*arc;
        require(a.pitch>=MortarMinimumPitch-1e-8 && a.pitch<=MortarMaximumPitch+1e-8,"arc obeys authored elevation stops");
        requireNear(origin.y+a.velocity.y*a.flightTime-.5*MortarGravity*a.flightTime*a.flightTime,target.y,1e-8,"ballistic arc reaches actual target height");
        requireNear(origin.z+a.velocity.z*a.flightTime,target.z,1e-8,"ballistic arc reaches target distance");
        const auto fast=mortarTrajectory(origin,target,MortarHorizontalSpeed*2);
        require(fast && fast->flightTime<a.flightTime,"projectile speed shortens travel time");
    }
    require(!mortarTrajectory({}, {0,100,-4},7),"unreachable elevation does not create invalid projectiles");
    require(!mortarTrajectory({}, {0,0,-10},0),"zero speed cannot create an invalid arc");
}
void combat() {
    auto enemies=crowd();CannonSystem cannons;cannons.syncBuildings({Mortar});
    bool launched=false,hitGroup=false;
    for(int frame=0;frame<480;++frame) {
        cannons.tick(Step,{Mortar},enemies);
        if(!cannons.shots().empty()) {
            const auto& runtime=cannons.cannons()[0];
            const auto expected=mortarMuzzleWorldPosition(buildingWorldPosition(Mortar),runtime.yaw,runtime.pitch);
            const auto position=cannons.shots()[0].position;
            requireNear(position.y,expected.y,1e-8,"shell launches from authored raised muzzle");
            requireNear(position.z,expected.z,1e-8,"shell origin follows pitch");
            const auto& shell=cannons.projectiles()[0];
            require(shell.velocity.y>std::hypot(shell.velocity.x,shell.velocity.z),"mortar launches in a visible high arc");
            launched=true;
        }
        hitGroup|=cannons.hits().size()>=2;
    }
    require(launched && hitGroup,"real pooled shell explodes and damages a group");
    for(double distance:{2.5,19.5}) {
        auto outside=crowd(distance);CannonSystem idle;idle.syncBuildings({Mortar});
        for(int frame=0;frame<480;++frame) {
            idle.tick(Step,{Mortar},outside);
            require(idle.shots().empty(),"mortar respects near dead zone and outer range");
        }
    }
    auto behind=crowd(-10);CannonSystem idle;idle.syncBuildings({Mortar});
    for(int frame=0;frame<240;++frame) {idle.tick(Step,{Mortar},behind);require(idle.shots().empty(),"placement firing sector matters");}
}
void statsAndPooling() {
    auto enemies=crowd();CannonSystem cannons;AttackModifiers modifiers;
    modifiers[AttackStat::Quantity].flat=2.8;
    modifiers[AttackStat::Bounces].flat=1.9;
    modifiers[AttackStat::CritChance].flat=1;
    cannons.setAttackModifiers(modifiers);cannons.syncBuildings({Mortar});bool fired=false;
    for(int frame=0;frame<120 && !fired;++frame) {
        cannons.tick(Step,{Mortar},enemies);
        if(cannons.shots().empty())continue;
        require(cannons.projectiles().size()==3,"fractional quantity creates three distinct pooled shells");
        for(const auto& shell:cannons.projectiles())require(shell.active && shell.runtime.remainingBounces==1,"each shell gets floored independent bounces");
        fired=true;
    }
    require(fired,"upgraded mortar fires");
    bool criticalHit=false;
    for(int frame=0;frame<240 && !criticalHit;++frame) {
        cannons.tick(Step,{Mortar},enemies);
        for(const auto& hit:cannons.hits()) {
            require(hit.critical,"mortar explosions use shared guaranteed crit");
            criticalHit=true;
        }
    }
    require(criticalHit,"upgraded mortar shell impacts a real enemy");
    cannons.clearProjectiles();cannons.setAttackModifiers({});
    bool criticalLeak=false;
    for(int frame=0;frame<1000;++frame) {
        cannons.tick(Step,{Mortar},enemies);
        for(const auto& shell:cannons.projectiles())if(shell.active)
            require(shell.runtime.remainingBounces==0 && shell.configuration.stats.critChance==0,"pool reuse clears previous modifiers and bounce state");
        for(const auto& hit:cannons.hits())criticalLeak|=hit.critical;
    }
    require(!criticalLeak && cannons.projectiles().size()<=3,"pool remains reusable and bounded by peak demand");
    for(std::uint8_t level=2;level<=8;++level) {
        auto before=CannonSystem::baseStats(BuildingType::Mortar,static_cast<std::uint8_t>(level-1));
        auto after=CannonSystem::baseStats(BuildingType::Mortar,level);
        require(after.damage>before.damage && after.range>before.range && after.attackInterval<before.attackInterval,"eight progression levels");
    }
    require(supportsAttackStat(BuildingType::Mortar,AttackStat::Quantity) &&
        supportsAttackStat(BuildingType::Mortar,AttackStat::Bounces) &&
        supportsAttackStat(BuildingType::Mortar,AttackStat::Pierce),"mortar reuses physical projectile compatibility");
    cannons.reset();require(cannons.cannons().empty() && std::ranges::none_of(cannons.projectiles(),[](const auto& p){return p.active;}),"run reset clears mortar combat");
}
void bounce() {
    auto definitions=GameBalance::defaults().enemies;
    for(auto& definition:definitions)definition.health=100000;
    EnemySystem enemies(definitions);
    enemies.spawnWave(std::vector<EnemySpawn>{{.type=EnemyType::Basic,.position={0,0,-8}},
        {.type=EnemyType::Basic,.position={0,0,-12.5}}});
    CannonSystem cannons;AttackModifiers modifiers;modifiers[AttackStat::Bounces].flat=1;
    modifiers[AttackStat::AttackSpeed].multiplier=.1;
    cannons.setAttackModifiers(modifiers);cannons.syncBuildings({Mortar});
    int explosions=0;std::vector<EntityId> targets;
    for(int frame=0;frame<900;++frame) {
        explosions+=static_cast<int>(cannons.tick(Step,{Mortar},enemies).size());
        for(const auto& hit:cannons.hits())targets.push_back(hit.result.id);
    }
    require(explosions==2 && targets.size()==2 && targets[0]!=targets[1],"one pooled mortar shell bounces in a new arc and hits each group once: explosions="+std::to_string(explosions)+", hits="+std::to_string(targets.size()));
}
void progression() {
    static_assert(static_cast<int>(BuildingType::Mortar)==19 && static_cast<int>(RunUpgradeEffect::UnlockMortar)==64);
    auto map=MapDefinition::defaults();map.obstacles.clear();auto world=WorldConfig::defaults();world.terrainAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    auto game=std::make_unique<Simulation>(GameBalance::defaults(),map,world);game->startRunFromSeed(PlayerClass::Vanguard,702314);
    require(game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMortar),"mortar unlocks through shared card/debug system");
    require(!game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMortar),"mortar obeys core upgrade cap");
    PlayerCommand god;god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};game->tick(0,god);
    PlayerCommand core;core.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};game->tick(0,core);
    PlayerCommand up;up.upgradeBuilding=UpgradeBuildingCommand{*game->snapshot().coreId,false};game->tick(0,up);game->tick(0,up);
    PlayerCommand place;place.placeBuilding=PlaceBuildingCommand{BuildingType::Mortar,{3,0},0};game->tick(0,place);game->tick(0,god);
    require(std::ranges::any_of(game->snapshot().buildings,[](const auto& b){return b.type==BuildingType::Mortar;}),"mortar places on a foundation");
    require(game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMortar),"blueprint card upgrades existing mortars");
    auto saved=std::make_unique<SuspendedRunState>(game->saveSuspendedRunState());
    auto restored=std::make_unique<Simulation>(GameBalance::defaults(),map,world);
    require(restored->loadSuspendedRunState(*saved) && restored->snapshot().buildingBlueprintLevels[19]==2,"mortar save/load retains blueprint level");
}
}
int main(){trajectory();combat();statsAndPooling();bounce();progression();}
