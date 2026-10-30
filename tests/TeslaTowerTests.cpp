#include "TestHarness.hpp"
#include "combat/TeslaAttack.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/AttackStatCompatibility.hpp"
#include "game/Simulation.hpp"
#include <chrono>
#include <memory>
#include <set>
using namespace ian;
namespace {
constexpr double Step=1.0/120.0;
EnemySystem crowd(std::initializer_list<Vec3> positions) {
    auto definitions=GameBalance::defaults().enemies;
    for(auto& d:definitions) d.health=100000;
    EnemySystem enemies(definitions);std::vector<EnemySpawn> spawns;
    for(auto position:positions) spawns.push_back({.type=EnemyType::Basic,.position=position});
    enemies.spawnWave(spawns);return enemies;
}
void chainChecks() {
    const auto base=TowerSystem::baseStats(BuildingType::TeslaTower,1);
    requireNear(base.range,8,1e-12,"tesla base range");
    requireNear(base.bounces,2,1e-12,"two additional chain jumps in base");
    auto enemies=crowd({{0,0,-3},{1,0,-4},{2,0,-5},{3,0,-6},{20,0,-6}});
    AttackCollisionQuery query;query.rebuild(enemies);
    std::vector<EntityId> visited;visited.reserve(100);
    std::vector<AttackHit> hits;
    auto stats=resolveAttackStats(base);stats.critChance=1;stats.critDamage=2;stats.bounces=2.9;
    applyTeslaChain(enemies,query,enemies.enemies()[0].id,{0,TeslaEnergyHeight,0},stats,917,visited,
        [&](const AttackHit& hit,Vec3,Vec3,int jump){require(jump==static_cast<int>(hits.size()),"chain order");hits.push_back(hit);});
    require(hits.size()==3 && visited.size()==3,"bounces floored only at usage, first contact plus two jumps");
    require(hits[0].result.id!=hits[1].result.id && hits[1].result.id!=hits[2].result.id,"one contact per target in a chain");
    for(const auto& hit:hits) require(hit.critical && hit.result.damage>base.damage,"shared critical hit flow");
    hits.clear();stats.bounces=1.0;stats.bounceRange=.2;
    applyTeslaChain(enemies,query,enemies.enemies()[0].id,{},stats,927,visited,
        [&](const AttackHit& hit,Vec3,Vec3,int){hits.push_back(hit);});
    require(hits.size()==1,"chain reach is physical neighbor range");
    LightningTargetCache cache;cache.beginTick();
    const auto source=enemies.enemies()[0].id;
    std::vector<EntityId> exclusions{source};
    const auto ordinary=query.bounceTarget(enemies,attackTargetPosition(enemies.enemies()[0]),4.5,BuildingType::TeslaTower,0,exclusions);
    const auto cached=cache.next(enemies,query,source,attackTargetPosition(enemies.enemies()[0]),4.5,exclusions);
    require(ordinary && cached && ordinary->id==cached->id,"cached neighbor agrees with spatial lookup");
    exclusions.push_back(cached->id);
    require(cache.next(enemies,query,source,attackTargetPosition(enemies.enemies()[0]),4.5,exclusions)->id!=cached->id,"cache respects each bolt history");
    enemies.damage(cached->id,1000000);
    require(cache.next(enemies,query,source,attackTargetPosition(enemies.enemies()[0]),4.5,{&source,1})->id!=cached->id,"cache cannot resurrect dead targets");
    require(!cache.next(enemies,query,source,attackTargetPosition(enemies.enemies()[0]),.1,{&source,1}),"changing chain range invalidates cache");
    cache.beginTick();
    require(supportsAttackStat(BuildingType::TeslaTower,AttackStat::Bounces) &&
        supportsAttackStat(BuildingType::TeslaTower,AttackStat::Quantity) &&
        !supportsAttackStat(BuildingType::TeslaTower,AttackStat::ProjectileSpeed) &&
        !supportsAttackStat(BuildingType::TeslaTower,AttackStat::Pierce) &&
        !supportsAttackStat(BuildingType::TeslaTower,AttackStat::Duration),"explicit lightning compatibility");
}
void towerChecks() {
    const BuildingInstance building{.id={970,1},.type=BuildingType::TeslaTower,.gridPosition={0,0},.level=1};
    auto enemies=crowd({{0,0,3},{1,0,4},{2,0,5},{10,0,0}});
    TowerSystem towers;towers.syncBuildings({building});
    int hits=0;for(int frame=0;frame<300;++frame) hits+=static_cast<int>(towers.tick(Step,{building},enemies).size());
    require(hits>=6 && towers.towers()[0].targetId==enemies.enemies()[0].id,"static tesla fires into rear hemisphere");
    require(towers.projectiles().empty() && towers.launches().empty(),"instant chains create no fake projectiles");
    for(int quantity:{1,3,10,30}) {
        auto lone=crowd({{0,0,3}});TowerSystem scaled;
        AttackModifiers modifiers;modifiers[AttackStat::Quantity].flat=quantity-1;
        modifiers[AttackStat::AttackSpeed].multiplier=5;
        modifiers[AttackStat::Pierce].flat=5;modifiers[AttackStat::ProjectileSpeed].multiplier=10;
        scaled.setAttackModifiers(modifiers);scaled.syncBuildings({building});
        int events=0,contacts=0;
        for(int frame=0;frame<150;++frame) {
            contacts+=static_cast<int>(scaled.tick(Step,{building},lone).size());events+=static_cast<int>(scaled.attacks().size());
            require(scaled.towers()[0].sequenceRemaining>=0 && scaled.projectiles().empty(),"one non-overlapping tesla sequence");
        }
        require(contacts>=quantity*(events-1) && contacts<=quantity*events && events>=5,"count and attack speed preserve actual throughput");
        scaled.clearProjectiles();require(scaled.towers()[0].sequenceRemaining==0,"clearing combat cancels discharge sequence");
    }
    auto previous=TowerSystem::baseStats(building.type,1);
    for(std::uint8_t level=2;level<=8;++level) {
        auto current=TowerSystem::baseStats(building.type,level);
        require(current.damage>previous.damage && current.range>previous.range && current.attackInterval<previous.attackInterval,"eight tesla stat levels");previous=current;
    }
    auto definitions=GameBalance::defaults().enemies;
    for(auto& definition:definitions) definition.health=100000;
    EnemySystem packed(definitions);std::vector<EnemySpawn> spawns;
    for(int i=0;i<1000;++i)spawns.push_back({.type=EnemyType::Basic,.position={double(i%25)*.2-2,0,double(i/25)*.15-2}});
    packed.spawnWave(spawns);std::vector<BuildingInstance> battery;
    for(std::uint32_t i=0;i<24;++i){auto b=building;b.id={1000+i,1};battery.push_back(b);}
    TowerSystem stress;AttackModifiers upgrades;upgrades[AttackStat::Quantity].flat=29;upgrades[AttackStat::AttackSpeed].multiplier=5;
    upgrades[AttackStat::Bounces].flat=3;stress.setAttackModifiers(upgrades);stress.syncBuildings(battery);
    const auto start=std::chrono::steady_clock::now();std::size_t total=0;
    for(int frame=0;frame<60;++frame){const auto shots=stress.tick(1.0/60,battery,packed);total+=shots.size();require(shots.size()<=256*6,"per-tick chain budget bounds peak work");}
    require(total>1000 && stress.projectiles().empty(),"crowded batteries retain damage and avoid projectile allocations");
    std::cout<<"Tesla stress: 24 towers / 1000 enemies / Q30 / AS5 / 5 jumps: "<<
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/60<<" ms/tick\n";
}
void progressionChecks() {
    static_assert(static_cast<int>(BuildingType::TeslaTower)==17 && static_cast<int>(RunUpgradeEffect::UnlockTeslaTower)==62);
    require(runCombatBuilding(BuildingType::TeslaTower),"tesla defense slots and card registry");
    auto map=MapDefinition::defaults();map.obstacles.clear();auto world=WorldConfig::defaults();world.terrainAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    auto game=std::make_unique<Simulation>(GameBalance::defaults(),map,world);game->startRunFromSeed(PlayerClass::Vanguard,702314);
    require(game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockTeslaTower),"acquire tesla");
    require(game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockTeslaTower),"tesla may level before core placement");
    PlayerCommand god;god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};game->tick(0,god);
    PlayerCommand core;core.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};game->tick(0,core);
    PlayerCommand up;up.upgradeBuilding=UpgradeBuildingCommand{*game->snapshot().coreId,false};game->tick(0,up);game->tick(0,up);
    PlayerCommand place;place.placeBuilding=PlaceBuildingCommand{BuildingType::TeslaTower,{3,0},0};game->tick(0,place);game->tick(0,god);
    require(game->grantDebugRunUpgrade(RunUpgradeEffect::UnlockTeslaTower),"upgrade tesla independently of core level");
    const auto slot=static_cast<std::size_t>(BuildingType::TeslaTower);
    require(game->snapshot().buildingBlueprintLevels[slot]==3,"shared tesla blueprint level");
    require(std::ranges::any_of(game->snapshot().buildings,[](const auto& b){return b.type==BuildingType::TeslaTower && b.level==3;}),"tesla existing copies upgraded");
    auto saved=std::make_unique<SuspendedRunState>(game->saveSuspendedRunState());auto restored=std::make_unique<Simulation>(GameBalance::defaults(),map,world);
    require(restored->loadSuspendedRunState(*saved) && restored->snapshot().buildingBlueprintLevels[slot]==3,"tesla save/load");
}
}
int main(){chainChecks();towerChecks();progressionChecks();}
