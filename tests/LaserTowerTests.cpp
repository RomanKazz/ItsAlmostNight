#include "TestHarness.hpp"
#include "combat/LaserAttack.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/AttackStatCompatibility.hpp"
#include "game/Simulation.hpp"
#include <memory>
using namespace ian;
namespace {
constexpr double Step=1.0/120.0;
EnemySystem crowd() {
    auto definitions=GameBalance::defaults().enemies;
    for(auto& d:definitions)d.health=100000;
    EnemySystem enemies(definitions);
    const std::vector<EnemySpawn> spawns{{.type=EnemyType::Basic,.position={0,0,-5}},
        {.type=EnemyType::Basic,.position={.7,0,-6}}};
    enemies.spawnWave(spawns);return enemies;
}
void focus() {
    const BuildingInstance building{.id={985,1},.type=BuildingType::LaserTower,.gridPosition={0,0}};
    auto enemies=crowd();TowerSystem towers;towers.syncBuildings({building});
    double first=0,last=0;EntityId target{};int contacts=0;
    for(int frame=0;frame<600;++frame) {
        for(const auto& hit:towers.tick(Step,{building},enemies)) {
            if(first==0){first=hit.damage;target=hit.targetId;}
            require(hit.targetId==target,"focused laser holds one living target");
            last=hit.damage;++contacts;
        }
    }
    require(contacts>25 && last>first*3.5,"continuous focus substantially ramps real shared hit damage");
    requireNear(towers.towers()[0].laserFocusSeconds,LaserFocusRampSeconds,1e-8,"focus bounded at full charge");
    require(towers.projectiles().empty() && towers.launches().empty(),"beam does not spawn physical projectiles or per-pulse gunfire");
    enemies.damage(target,1000000);
    double reacquired=0;
    for(int frame=0;frame<120 && reacquired==0;++frame)
        for(const auto& hit:towers.tick(Step,{building},enemies)) reacquired=hit.damage;
    require(reacquired>0 && reacquired<last*.4,"new target resets damage ramp");
    AttackModifiers noRange;noRange[AttackStat::Range].flat=-100;
    towers.setAttackModifiers(noRange);
    for(int frame=0;frame<30;++frame) require(towers.tick(Step,{building},enemies).empty(),"no damage outside beam range");
    require(towers.towers()[0].laserFocusSeconds==0 && towers.towers()[0].laserRemaining==0,"losing range resets charge and fades beam");
    towers.clearProjectiles();
    require(towers.towers()[0].laserRemaining==0 && towers.towers()[0].laserFocusSeconds==0,"combat clear resets beam and charge");
    auto removed=std::vector<BuildingInstance>{};towers.syncBuildings(removed);
    require(towers.towers().empty(),"sold laser leaves no runtime state");
    towers.reset();require(towers.towers().empty(),"restart clears lasers");
}
void scaling() {
    const BuildingInstance building{.id={985,1},.type=BuildingType::LaserTower,.gridPosition={0,0}};
    double damage[2]{};
    for(int speed=1;speed<=2;++speed) {
        auto enemies=crowd();TowerSystem towers;AttackModifiers modifier;
        modifier[AttackStat::AttackSpeed].multiplier=speed;
        modifier[AttackStat::Quantity].flat=30;
        towers.setAttackModifiers(modifier);towers.syncBuildings({building});
        for(int frame=0;frame<600;++frame)
            for(const auto& hit:towers.tick(Step,{building},enemies))damage[speed-1]+=hit.damage;
    }
    require(damage[1]>damage[0]*1.8 && damage[1]<damage[0]*2.2,"attack speed increases throughput without changing focus speed");
    require(supportsAttackStat(BuildingType::LaserTower,AttackStat::Quantity) &&
        supportsAttackStat(BuildingType::LaserTower,AttackStat::Bounces) &&
        !supportsAttackStat(BuildingType::LaserTower,AttackStat::Pierce) &&
        supportsAttackStat(BuildingType::LaserTower,AttackStat::CritChance),"laser compatibility includes independent beam instances and chains");
    auto enemies=crowd();TowerSystem crit;AttackModifiers modifier;modifier[AttackStat::CritChance].flat=1;
    crit.setAttackModifiers(modifier);crit.syncBuildings({building});bool critical=false;
    for(int i=0;i<120;++i)for(const auto& hit:crit.tick(Step,{building},enemies))critical|=hit.critical;
    require(critical,"laser uses shared crit flow");
    for(std::uint8_t level=2;level<=8;++level) {
        const auto before=TowerSystem::baseStats(building.type,static_cast<std::uint8_t>(level-1));
        const auto after=TowerSystem::baseStats(building.type,level);
        require(after.damage>before.damage && after.range>before.range && after.attackInterval<before.attackInterval,"eight stat levels");
    }
}
void multipleRaysAndBounces() {
    const BuildingInstance building{.id={985,1},.type=BuildingType::LaserTower,.gridPosition={0,0}};
    auto enemies=crowd();TowerSystem towers;AttackModifiers upgrades;
    upgrades[AttackStat::Quantity].flat=2.8;
    upgrades[AttackStat::Bounces].flat=1.9;
    towers.setAttackModifiers(upgrades);towers.syncBuildings({building});
    bool fired=false;
    for(int frame=0;frame<120 && !fired;++frame) {
        const auto hits=towers.tick(Step,{building},enemies);
        if(hits.empty())continue;
        require(hits.size()==6,"three floored beam instances, each with first contact and one bounce");
        require(towers.laserBeams().size()==6,"real parallel and chained beam segments reach presentation");
        for(std::size_t ray=0;ray<3;++ray) {
            require(hits[ray*2].targetId!=hits[ray*2+1].targetId,"one chain cannot hit the same enemy twice");
            requireNear(hits[ray*2].damage,hits[ray*2+1].damage,1e-8,"bounce retains full focused damage");
            requireNear(hits[ray*2].damage,hits[0].damage,1e-8,"additional beams retain full damage");
        }
        fired=true;
    }
    require(fired,"upgraded laser fires");
    towers.clearProjectiles();require(towers.laserBeams().empty(),"reset clears all beam segments");
}
void denseVolley() {
    auto definitions=GameBalance::defaults().enemies;
    for(auto& definition:definitions)definition.health=1000000;
    EnemySystem enemies(definitions);
    std::vector<EnemySpawn> spawns;
    for(int i=0;i<6;++i)spawns.push_back({.type=EnemyType::Basic,.position={double(i)*.2,0,-5-double(i)*.2}});
    enemies.spawnWave(spawns);
    const std::vector<BuildingInstance> buildings{
        {.id={985,1},.type=BuildingType::LaserTower,.gridPosition={0,0}},
        {.id={986,1},.type=BuildingType::LaserTower,.gridPosition={1,0}}};
    TowerSystem towers;AttackModifiers upgrades;
    upgrades[AttackStat::Quantity].flat=29;
    upgrades[AttackStat::Bounces].flat=5;
    upgrades[AttackStat::AttackSpeed].multiplier=5;
    towers.setAttackModifiers(upgrades);towers.syncBuildings(buildings);
    std::size_t largestVolley=0;
    for(int frame=0;frame<120;++frame) {
        largestVolley=std::max(largestVolley,towers.tick(Step,buildings,enemies).size());
        require(towers.laserBeams().size()<=MaximumLaserVisualSegments,"dense volley retains bounded visual storage");
    }
    require(largestVolley==360,"visual budget does not truncate thirty beams with five bounces from two towers");
    towers.syncBuildings({});
    require(towers.laserBeams().empty(),"selling sources clears dense visual state");
}
void progression() {
    static_assert(static_cast<int>(BuildingType::LaserTower)==18 && static_cast<int>(RunUpgradeEffect::UnlockLaserTower)==63);
    auto map=MapDefinition::defaults();map.obstacles.clear();auto world=WorldConfig::defaults();world.terrainAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    auto game=std::make_unique<Simulation>(GameBalance::defaults(),map,world);game->startRunFromSeed(PlayerClass::Vanguard,702314);
    require(game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockLaserTower),"laser can be acquired through card/debug progression");
    require(!game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockLaserTower),"laser upgrade obeys core cap");
    PlayerCommand god;god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};game->tick(0,god);
    PlayerCommand core;core.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};game->tick(0,core);
    PlayerCommand up;up.upgradeBuilding=UpgradeBuildingCommand{*game->snapshot().coreId,false};game->tick(0,up);game->tick(0,up);
    PlayerCommand place;place.placeBuilding=PlaceBuildingCommand{BuildingType::LaserTower,{3,0},0};game->tick(0,place);game->tick(0,god);
    require(game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockLaserTower),"laser blueprints upgrade all copies");
    const auto slot=static_cast<std::size_t>(BuildingType::LaserTower);
    require(game->snapshot().buildingBlueprintLevels[slot]==2,"laser blueprint level");
    auto saved=std::make_unique<SuspendedRunState>(game->saveSuspendedRunState());
    auto restored=std::make_unique<Simulation>(GameBalance::defaults(),map,world);
    require(restored->loadSuspendedRunState(*saved) && restored->snapshot().buildingBlueprintLevels[slot]==2,"laser save/load");
}
}
int main(){focus();scaling();multipleRaysAndBounces();denseVolley();progression();}
