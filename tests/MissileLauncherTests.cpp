#include "TestHarness.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/AttackStatCompatibility.hpp"
#include "buildings/BuildingHotbarLayout.hpp"
#include "buildings/TowerRig.hpp"
#include "progression/RunUpgrade.hpp"
#include "game/Simulation.hpp"
#include <memory>
#include <set>
using namespace ian;
namespace {
constexpr double Step = 1.0 / 120.0;
EnemySystem crowd(std::initializer_list<Vec3> positions) {
    auto defs = GameBalance::defaults().enemies;
    for (auto& d : defs) d.health = 100000;
    EnemySystem result(defs);
    std::vector<EnemySpawn> spawns;
    for (auto pos : positions) spawns.push_back({.type=EnemyType::Basic, .position=pos});
    result.spawnWave(spawns); return result;
}
void projectileChecks() {
    const auto stats = resolveAttackStats(TowerSystem::baseStats(BuildingType::MissileLauncher,1));
    auto enemies = crowd({{2,0,-6},{2.5,0,-6},{6,0,-6}});
    const auto target = enemies.enemies()[0].id;
    AttackProjectileSystem pool;
    require(pool.spawn({999,1},BuildingType::MissileLauncher,{0,.56,0},{0,0,-1},stats,0,0,false,target),"rocket spawn");
    std::vector<ProjectileImpact> hits; int blasts=0; bool turned=false;
    for (int frame=0;frame<240;++frame) {
        const auto batch=pool.tick(Step,enemies); hits.insert(hits.end(),batch.begin(),batch.end());
        blasts+=static_cast<int>(pool.blasts().size());
        if (pool.projectiles()[0].active) turned |= pool.projectiles()[0].runtime.velocity.x > 1;
    }
    require(turned,"rocket steers toward off-axis target");
    require(blasts==1,"one blast per missile");
    require(hits.size()==2,"impact includes direct and nearby enemy, excludes distant enemy");
    require(hits[0].hit.result.id==target && !hits[0].secondary,"direct target hit once");
    require(hits[1].secondary,"splash retains shared hit attribution");
    requireNear(hits[0].payloadDamage,stats.damage,1e-12,"full direct damage");
    requireNear(hits[1].payloadDamage,stats.damage,1e-12,"full splash damage");
    pool.reset();
    EnemySystem airborne;
    const std::vector<EnemySpawn> flyers{{.type=EnemyType::Flying,.position={1,0,-8}}};airborne.spawnWave(flyers);
    const auto flyer=airborne.enemies()[0].id;
    pool.spawn({999,1},BuildingType::MissileLauncher,{1,1.2,0},{0,.5,-.866},stats,0,0,false,flyer);
    bool hitFlyer=false;
    for(int frame=0;frame<240;++frame) for(const auto& hit:pool.tick(Step,airborne)) hitFlyer |= hit.hit.result.id==flyer;
    require(hitFlyer,"guided rockets hit airborne targets without a height restriction");
    pool.reset();
    auto incidental=crowd({{0,0,-1.6},{0,0,-6}});
    pool.spawn({999,1},BuildingType::MissileLauncher,{0,.56,0},{0,0,-1},stats,0,0,false,incidental.enemies()[1].id);
    bool closeHit=false;
    for(int frame=0;frame<60;++frame) for(const auto& hit:pool.tick(Step,incidental)) closeHit |= hit.hit.result.id==incidental.enemies()[0].id;
    require(closeHit,"fired rockets can collide with incidental enemies inside the blind zone");
    pool.reset();
    auto targetDies = crowd({{0,0,-3},{2,0,-5}});
    const auto first=targetDies.enemies()[0].id, next=targetDies.enemies()[1].id;
    require(pool.spawn({999,1},BuildingType::MissileLauncher,{0,.56,0},{0,0,-1},stats,0,0,false,first),"reuse spawn");
    require(pool.projectiles()[0].runtime.alreadyHitTargets.empty(),"reused projectile clears hit history");
    targetDies.damage(first,1000000);
    pool.tick(Step,targetDies);
    require(pool.projectiles()[0].runtime.currentTarget==next,"lost target reacquired");
    pool.reset(); require(std::ranges::none_of(pool.projectiles(), [](const auto& p){ return p.active; }) && pool.blasts().empty(),"reset clears rockets and blasts");
    auto empty=crowd({});
    pool.spawn({999,1},BuildingType::MissileLauncher,{0,.56,0},{0,0,-1},stats,0);
    for(int frame=0;frame<600;++frame) pool.tick(Step,empty);
    require(!pool.projectiles()[0].active,"unopposed rocket expires");
    pool.spawn({999,1},BuildingType::GunTurret,{0,.56,0},{0,0,-1},stats,0);
    require(!pool.projectiles()[0].runtime.currentTarget,"reuse clears homing target on ordinary bullet");
}
void curvedFlightChecks() {
    const auto stats = resolveAttackStats(TowerSystem::baseStats(BuildingType::MissileLauncher,1));
    auto enemies = crowd({{0,0,-14}});
    const auto target = enemies.enemies()[0].id;
    std::array<Vec3,3> sampledPositions{};
    for (int sample=0; sample<3; ++sample) {
        const double step = 1.0 / (60 << sample);
        AttackProjectileSystem pool;
        pool.spawn({999,1},BuildingType::MissileLauncher,{0,1.2,0},{0,.5,-.8660254},stats,0,0,false,target);
        Vec3 previous = geometry::normalizedOr(pool.projectiles()[0].runtime.velocity);
        int curvedSteps=0;
        double earlyTurn=0, laterTurn=0;
        for(int frame=0;frame<(60 << sample)*3/5;++frame) {
            pool.tick(step,enemies);
            const auto& rocket = pool.projectiles()[0];
            require(rocket.active,"opening arc remains in flight");
            const Vec3 heading = geometry::normalizedOr(rocket.runtime.velocity);
            const double turn = std::acos(std::clamp(geometry::dot(previous,heading),-1.0,1.0));
            require(turn < 3.2*step,"opening turn stays broad without a sharp corner");
            if(rocket.runtime.elapsedLifetime<=.12) earlyTurn+=turn;
            else laterTurn+=turn;
            if(turn>.05*step) ++curvedSteps;
            requireNear(geometry::length(rocket.runtime.velocity),stats.projectileSpeed,1e-9,"arc preserves flight speed");
            previous=heading;
        }
        require(earlyTurn < laterTurn*.1,"guidance turns on gradually after leaving the tube");
        require(curvedSteps > (60 << sample)/3,"turn is distributed along the arc");
        sampledPositions[static_cast<std::size_t>(sample)]=pool.projectiles()[0].position;
    }
    require(geometry::length(geometry::subtract(sampledPositions[0],sampledPositions[2]))<.09 &&
        geometry::length(geometry::subtract(sampledPositions[1],sampledPositions[2]))<.04,
        "arc remains consistent at 60, 120 and 240 Hz");
    // Broad launch arcs must still intercept close targets and boosted projectiles.
    for(double speed : {6.0,12.0,30.0}) for(double distance : {2.6,8.0,18.0}) {
        auto nearby=crowd({{0,0,-distance}});
        auto tuned=stats;tuned.projectileSpeed=speed;
        for(std::uint8_t muzzle=0;muzzle<4;++muzzle) {
            AttackProjectileSystem pool;
            pool.spawn({999,1},BuildingType::MissileLauncher,{0,1.2,0},{0,.5,-.8660254},tuned,0,muzzle,false,nearby.enemies()[0].id);
            bool hit=false;
            for(int frame=0;frame<540;++frame) hit |= !pool.tick(Step,nearby).empty();
            require(hit,"all four arcs intercept close/far targets with speed modifiers");
        }
    }
    auto changed=crowd({{0,0,-14},{5,0,-5}});
    AttackProjectileSystem retarget;
    retarget.spawn({999,1},BuildingType::MissileLauncher,{0,1.2,0},{0,.5,-.8660254},stats,0,0,false,changed.enemies()[0].id);
    for(int frame=0;frame<60;++frame) retarget.tick(Step,changed);
    const auto before=geometry::normalizedOr(retarget.projectiles()[0].runtime.velocity);
    changed.damage(changed.enemies()[0].id,1000000);
    retarget.tick(Step,changed);
    const auto after=geometry::normalizedOr(retarget.projectiles()[0].runtime.velocity);
    require(std::acos(std::clamp(geometry::dot(before,after),-1.0,1.0))<.04,"retargeting keeps a continuous heading");
    bool hit=false;
    for(int frame=0;frame<480;++frame) hit |= !retarget.tick(Step,changed).empty();
    require(hit,"curved retargeting reaches replacement enemy");
}
void towerChecks() {
    const BuildingInstance building{.id={870,1},.type=BuildingType::MissileLauncher,.gridPosition={0,0},.level=1};
    for (int quantity : {2,3,10,30}) {
        auto enemies=crowd({{1,0,-8}}); TowerSystem towers;
        AttackModifiers mods; mods[AttackStat::Quantity].flat=quantity-2;
        mods[AttackStat::AttackSpeed].multiplier=5;
        mods[AttackStat::Bounces].flat=5; mods[AttackStat::Pierce].flat=5;
        towers.setAttackModifiers(mods); towers.syncBuildings({building});
        int launches=0,events=0; std::vector<int> sockets;
        for(int frame=0;frame<160;++frame) {
            towers.tick(Step,{building},enemies);
            events+=static_cast<int>(towers.attacks().size());
            for(const auto& launch:towers.launches()) { ++launches;sockets.push_back(launch.muzzleIndex); }
            for(const auto& rocket:towers.projectiles()) if(rocket.active) {
                require(rocket.runtime.remainingBounces==0 && rocket.runtime.remainingPierce==0,"unsupported stats filtered");
                require(rocket.runtime.currentTarget.has_value(),"initial target wired to rocket");
            }
        }
        require(events>1 && launches>=quantity*(events-1) && launches<=quantity*events,"quantity and speed produce exact non-overlapping bursts");
        for(std::size_t i=0;i<sockets.size();++i) require(sockets[i]==static_cast<int>(i%4),"four authored tubes alternate");
        require(towers.droppedProjectiles()==0,"stress burst stays within pool");
    }
    {
        auto close=crowd({{1,0,-.5}}); TowerSystem blind; blind.syncBuildings({building});
        for(int frame=0;frame<240;++frame) {
            blind.tick(Step,{building},close);
            require(blind.launches().empty() && !blind.towers()[0].targetId,"blind zone forbids acquiring close targets");
        }
        auto front=crowd({{1,0,-8}});
        bool climbed=false, arrived=false;
        for(int frame=0;frame<240;++frame) {
            const auto impacts=blind.tick(Step,{building},front); arrived |= !impacts.empty();
            for(const auto& rocket:blind.projectiles()) if(rocket.active && rocket.runtime.elapsedLifetime>.1 && rocket.runtime.elapsedLifetime<.2)
                climbed |= rocket.position.y > rocket.launchOrigin.y + .4;
        }
        require(climbed,"raised tubes launch upward before guided dive");
        require(arrived,"upward salvo returns to hit its target");
        requireNear(blind.towers()[0].pitch,MissileLauncherMaximumPitch,.001,"launcher holds upward firing pitch");
        auto rear=crowd({{1,0,8}});
        for(int frame=0;frame<240;++frame) {blind.tick(Step,{building},rear);require(blind.launches().empty(),"rear arc remains blind");}
    }
    auto previous=TowerSystem::baseStats(building.type,1);
    for(std::uint8_t level=2;level<=8;++level) {
        auto current=TowerSystem::baseStats(building.type,level);
        require(current.damage>previous.damage && current.attackInterval<previous.attackInterval && current.range>previous.range,"eight stat levels improve launcher");previous=current;
    }
    require(supportsAttackStat(building.type,AttackStat::Homing) && supportsAttackStat(building.type,AttackStat::Quantity) &&
        !supportsAttackStat(building.type,AttackStat::Bounces) && !supportsAttackStat(building.type,AttackStat::Pierce),"launcher compatibility");
    require(runCombatBuilding(building.type) && runUpgradeBuilding(RunUpgradeEffect::UnlockMissileLauncher)==building.type,"card registration");
    require(static_cast<int>(building.type)==16 && static_cast<int>(RunUpgradeEffect::UnlockMissileLauncher)==61,"append-only save IDs");
    std::array<bool,GameBalance::BuildingTypeCount> unlocked;unlocked.fill(true);
    const auto hotbar=makeBuildingHotbarLayout(unlocked,BuildingHotbarCategory::Defense,true,8,true);
    require(hotbar.count==15 && hotbar.types[5]==building.type && hotbar.types[7]==BuildingType::LaserTower && hotbar.types[14]==BuildingType::SlowTrap,"new slot before walls and traps");
    const Vec3 socket=missileLauncherMuzzleWorldPosition({},0,0,0);
    requireNear(socket.x,-.113,1e-10,"socket x");requireNear(socket.y,1.203,1e-10,"socket y");requireNear(socket.z,-.4388,1e-10,"socket z");
    const Vec3 raised=missileLauncherMuzzleWorldPosition({},0,MissileLauncherMaximumPitch,0);
    require(raised.y>socket.y,"positive pitch raises sockets");
}
}
void progressionChecks() {
    auto map=MapDefinition::defaults();map.obstacles.clear();
    auto world=WorldConfig::defaults();world.terrainAmplitude=0;world.pondMinimumCount=world.pondMaximumCount=0;
    auto game = std::make_unique<Simulation>(GameBalance::defaults(),map,world);
    game->startRunFromSeed(PlayerClass::Vanguard,702314);
    require(game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMissileLauncher), "acquire launcher card before core");
    const auto slot=static_cast<std::size_t>(BuildingType::MissileLauncher);
    require(game->snapshot().buildingBlueprintLevels[slot]==1 && game->snapshot().buildingArsenalCount==2,"launcher blueprint unlocks");
    require(!game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMissileLauncher),"core-less launcher level capped at one");
    PlayerCommand god;god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};game->tick(0,god);
    PlayerCommand core;core.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};game->tick(0,core);
    require(game->snapshot().coreId.has_value(),"core fixture placement");
    PlayerCommand up;up.upgradeBuilding=UpgradeBuildingCommand{*game->snapshot().coreId,false};game->tick(0,up);game->tick(0,up);
    PlayerCommand place;place.placeBuilding=PlaceBuildingCommand{BuildingType::MissileLauncher,{3,0},0};game->tick(0,place);
    game->tick(0,god); // Leave creative mode to exercise ordinary core-limited cards.
    require(game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMissileLauncher),"core level permits launcher upgrade");
    while(game->snapshot().runUpgradeChoicePending) require(game->selectRunUpgrade(0),"resolve fixture level rewards");
    require(std::ranges::any_of(game->snapshot().buildings,[](const auto& b){return b.type==BuildingType::MissileLauncher && b.level==2;}),"card upgrades the constructed launcher");
    BuildingSystem constructed;
    require(constructed.place(BuildingType::Core,{0,0},0,9999,9999,9999).has_value(),"blueprint construction core");
    constructed.setCombatCardProgressionEnabled(true);
    constructed.setCombatBlueprintLevel(BuildingType::MissileLauncher,2);
    const auto copy=constructed.place(BuildingType::MissileLauncher,{3,0},0,9999,9999,9999);
    require(copy && copy->building.level==2,"new normal construction inherits shared level");
    auto saved=std::make_unique<SuspendedRunState>(game->saveSuspendedRunState());
    auto restored=std::make_unique<Simulation>(GameBalance::defaults(),map,world);require(restored->loadSuspendedRunState(*saved),"launcher run saves and restores");
    require(restored->snapshot().buildingBlueprintLevels[slot]==2 && restored->snapshot().unlockedBuildings[slot],"restored level and unlock stable");
    require(restored->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMissileLauncher),"restored launcher can reach level three core");
    require(!restored->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMissileLauncher),"restored launcher respects core cap");
}
int main(){projectileChecks();curvedFlightChecks();towerChecks();progressionChecks();}
