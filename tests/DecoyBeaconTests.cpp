#include "TestHarness.hpp"
#include "enemies/EnemySystem.hpp"
#include "buildings/BuildingStats.hpp"
#include "buildings/BuildingHotbarLayout.hpp"
#include "game/Simulation.hpp"
#include <algorithm>
#include <memory>

namespace {
using namespace ian;
constexpr EntityId BeaconId{1001,1}, CoreId{1000,1};
constexpr double Step=1.0/60;
std::vector<BuildingInstance> structures() {
    return {{.id=CoreId,.type=BuildingType::Core,.gridPosition={0,-16},.health=500,.maxHealth=500},
        {.id=BeaconId,.type=BuildingType::DecoyBeacon,.gridPosition={0,0},.health=220,.maxHealth=220}};
}
FlowField navigation(const std::vector<BuildingInstance>& buildings) {
    FlowField field(std::vector<CollisionBox>{});field.rebuild({0,-16},buildings);return field;
}
void selectionAndExpiry() {
    auto buildings=structures();auto flow=navigation(buildings);EnemySystem enemies;
    enemies.tick(7.99,buildings,flow);
    std::vector<EnemySpawn> wave;
    for(int i=0;i<10;++i)wave.push_back({.type=EnemyType::Basic,.position={double(i%5-2),0,3.0+double(i/5)}});
    wave.push_back({.type=EnemyType::Boss,.position={5,0,0}});
    enemies.spawnGroup(wave);enemies.tick(.01,buildings,flow,Vec3{0,1.7,4});
    require(std::ranges::count_if(enemies.enemies(),[](const auto& e){return e.decoyTarget==BeaconId;})==8,"pulse selects eight nearest eligible enemies");
    require(!enemies.enemies().back().decoyTarget,"boss immune to attraction");
    const auto closest=enemies.enemies()[2].id;
    require(enemies.enemy(closest)->decoyTarget==BeaconId,"near enemies take priority over far enemies");
    requireNear(enemies.enemy(closest)->decoyRemaining,5,1e-8,"base attraction lasts five seconds");
    require(enemies.decoyBeacons().size()==1 && enemies.decoyBeacons()[0].pulseAge<1e-8,"pulse presentation follows real activation");
    for(int i=0;i<299;++i)enemies.tick(Step,buildings,flow);
    require(enemies.enemy(closest)->decoyTarget==BeaconId,"taunt persists until duration ends");
    enemies.tick(Step,buildings,flow);
    require(!enemies.enemy(closest)->decoyTarget,"taunt expires without a replacement pulse");
    requireNear(enemies.enemy(closest)->decoyRemaining,0,1e-9,"no attraction remains after expiry");
}
void targetingAndAttacks() {
    auto buildings=structures();auto flow=navigation(buildings);EnemySystem enemies;
    enemies.tick(7.9,buildings,flow);
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={0,0,4}}});
    const auto id=enemies.enemies()[0].id;
    enemies.tick(.1,buildings,flow,Vec3{1,1.7,4});
    require(enemies.enemy(id)->decoyTarget==BeaconId,"beacon overrides player aggro");
    bool struck=false;
    for(int i=0;i<240;++i)for(const auto& hit:enemies.tick(Step,buildings,flow,Vec3{1,1.7,4})) {
        require(hit.targetId==BeaconId,"attracted enemy attacks the beacon instead of core/player");struck=true;
    }
    require(struck,"attracted enemy reaches and damages beacon");
    const double cooldown=enemies.enemy(id)->attackCooldownRemaining;
    buildings.erase(buildings.begin()+1);flow.rebuild({0,-16},buildings);
    enemies.tick(Step,buildings,flow);
    require(!enemies.enemy(id)->decoyTarget && enemies.enemy(id)->target!=BeaconId,"destroyed or sold beacon releases its enemies");
    require(enemies.enemy(id)->attackCooldownRemaining<=cooldown,"release does not reset attack cadence");
    require(enemies.decoyBeacons().empty(),"destroyed source leaves no pulse or stale path");
}
void overlapAndElite() {
    auto buildings=structures();buildings.push_back({.id={1002,1},.type=BuildingType::DecoyBeacon,.gridPosition={4,0},.health=220,.maxHealth=220});
    auto flow=navigation(buildings);EnemySystem enemies;
    enemies.tick(7.99,buildings,flow);
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={1,0,4},.eliteAffixes=1}});
    const auto id=enemies.enemies()[0].id;enemies.tick(.01,buildings,flow);
    require(enemies.enemy(id)->decoyTarget==BeaconId,"first beacon retains ownership across overlapping pulses");
    requireNear(enemies.enemy(id)->decoyRemaining,2.5,1e-8,"elite duration is halved");
    for(int i=0;i<150;++i)enemies.tick(Step,buildings,flow);
    require(!enemies.enemy(id)->decoyTarget,"elite is released before ordinary enemies");
    enemies.reset();require(enemies.decoyBeacons().empty(),"new run clears beacon clocks and fields");
}
void staggeredPulsesAndFreeze() {
    auto buildings=structures();auto flow=navigation(buildings);EnemySystem enemies;
    enemies.tick(4,buildings,flow);
    buildings.push_back({.id={1002,1},.type=BuildingType::DecoyBeacon,.gridPosition={4,0},.health=220,.maxHealth=220});
    flow.rebuild({0,-16},buildings);enemies.tick(3.99,buildings,flow);
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={1,0,4}}});
    const auto id=enemies.enemies()[0].id;
    require(enemies.applyStatus(id,StatusEffectType::Freeze,{},20),"freeze fixture applies a real status");
    enemies.tick(.01,buildings,flow);
    require(enemies.enemy(id)->decoyTarget==BeaconId,"first source owns frozen enemy");
    enemies.tick(4,buildings,flow);
    require(enemies.enemy(id)->decoyTarget==BeaconId,"staggered competing pulse cannot steal an active taunt");
    requireNear(enemies.enemy(id)->decoyRemaining,1,1e-8,"other pulse does not refresh duration");
    enemies.tick(1,buildings,flow);
    require(!enemies.enemy(id)->decoyTarget,"taunt timer expires even while frozen");
}
void reachablePaths() {
    auto buildings=structures();
    for(int z=-2;z<=2;++z)for(int x=-2;x<=2;++x)if(std::abs(x)==2 || std::abs(z)==2)
        buildings.push_back({.id={static_cast<std::uint32_t>(1010+buildings.size()),1},.type=BuildingType::Wall,.gridPosition={x,z},.health=100,.maxHealth=100});
    auto flow=navigation(buildings);EnemySystem sealed;
    sealed.tick(7.99,buildings,flow);
    sealed.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={0,0,4}}});
    sealed.tick(.01,buildings,flow);
    require(sealed.enemies()[0].decoyTarget==BeaconId,"beacon attracts through a closed destructible enclosure");
    bool hitWall=false;
    EntityId breachedWall{};
    for(int frame=0;frame<120;++frame)for(const auto& hit:sealed.tick(Step,buildings,flow)) {
        require(hit.targetId!=BeaconId,"closed enclosure prevents damaging the beacon through walls");
        hitWall|=std::ranges::any_of(buildings,[&](const auto& b){return b.id==hit.targetId && b.type==BuildingType::Wall;});
        breachedWall=hit.targetId;
    }
    require(hitWall,"attracted enemy attacks an intervening wall");
    const auto breach=std::ranges::find(buildings,breachedWall,&BuildingInstance::id)->gridPosition;
    std::erase_if(buildings,[&](const auto& b){return b.type==BuildingType::Wall &&
        (std::abs(breach.x)==2 ? b.gridPosition.x==breach.x : b.gridPosition.z==breach.z);});
    flow.rebuild({0,-16},buildings);
    bool reachedAfterBreach=false;
    for(int frame=0;frame<150;++frame)for(const auto& hit:sealed.tick(Step,buildings,flow))reachedAfterBreach|=hit.targetId==BeaconId;
    require(reachedAfterBreach,"enemy continues to beacon after the wall is destroyed");
    EnemySystem open;open.tick(7.99,buildings,flow);
    open.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={0,0,4}},EnemySpawn{.type=EnemyType::Flying,.position={4,0,0}}});
    open.tick(.01,buildings,flow);
    require(open.enemies()[0].decoyTarget==BeaconId && open.enemies()[1].decoyTarget==BeaconId,"accessible ground and flying targets are attracted");
    // A barrier with a route around its end is reachable; navigation must
    // actually carry the enemy around it instead of just facing the lamp.
    buildings=structures();
    for(int x=-2;x<=2;++x)buildings.push_back({.id={static_cast<std::uint32_t>(1100+x+2),1},.type=BuildingType::Wall,.gridPosition={x,2},.health=100,.maxHealth=100});
    flow.rebuild({0,-16},buildings);EnemySystem around;around.tick(7.99,buildings,flow);
    around.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Fast,.position={0,0,5}}});
    around.tick(.01,buildings,flow);require(around.enemies()[0].decoyTarget==BeaconId,"path around barrier remains eligible");
    bool arrived=false;
    for(int i=0;i<298;++i)for(const auto& hit:around.tick(Step,buildings,flow))arrived|=hit.targetId==BeaconId;
    require(arrived,"ordinary crowd navigation follows cached route around barrier");
    FlowField blocked(std::vector<CollisionBox>{{-1.5,1.5,2.5,5.5}});blocked.rebuild({0,-16},structures());
    EnemySystem obstacle;auto clear=structures();obstacle.tick(7.99,clear,blocked);
    obstacle.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={0,0,4}}});obstacle.tick(.01,clear,blocked);
    require(!obstacle.enemies()[0].decoyTarget,"static blocked cells are not reachable");
}
void structuralAndStaticBarriers() {
    auto buildings=structures();
    buildings.push_back({.id={1020,1},.type=BuildingType::Wall,.gridPosition={0,2},.health=100,.maxHealth=100});
    CollisionWorld collision(48,{});collision.syncBuildings(buildings);
    require(!collision.canOccupyCircle({.5,1.7,2.5},.5),"ordinary collision query still blocks walls");
    require(collision.canOccupyCircleIgnoringStructures({.5,1.7,2.5},.5),"breach planning can cross destructible colliders");
    collision.syncWorldScenery(std::array{CollisionBox{-.5,.5,3.5,4.5}});
    require(!collision.canOccupyCircleIgnoringStructures({0,1.7,4},.5),"immutable scenery stays solid during breach planning");
    collision.syncWorldScenery({});
    auto flow=navigation(buildings);
    EnemySystem enclosed;enclosed.tick(7.99,buildings,flow,{}, {},nullptr,{{},{},1,&collision});
    enclosed.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={0,0,4.5}}});
    enclosed.tick(.01,buildings,flow,{}, {},nullptr,{{},{},1,&collision});
    require(enclosed.enemies()[0].decoyTarget==BeaconId,"real collision-world walls do not suppress attraction");
    for(const auto type:{EnemyType::Basic,EnemyType::Ranged,EnemyType::Sapper}) {
        auto clear=structures();auto field=navigation(clear);
        std::vector<EnemyStructureTarget> barriers;
        for(int z=-2;z<=2;++z)for(int x=-2;x<=2;++x)if(std::abs(x)==2 || std::abs(z)==2)
            barriers.push_back({.id={static_cast<std::uint32_t>(31001+barriers.size()),1},
                .position={x+.5,0,z+.5},.radius=.55,.buildingType=std::nullopt,.modular=true,.structuralImpact=3});
        EnemySystem enemies;enemies.tick(7.99,clear,field,{},barriers);
        enemies.spawnGroup(std::array{EnemySpawn{.type=type,.position={-4,0,0}}});
        enemies.tick(.01,clear,field,{},barriers);
        require(enemies.enemies()[0].decoyTarget==BeaconId,"modular barrier allows a taunt to be acquired");
        bool struck=false;
        for(int frame=0;frame<180;++frame)for(const auto& hit:enemies.tick(Step,clear,field,{},barriers)) {
            require(hit.targetId!=BeaconId,"melee and ranged attacks cannot skip an intact modular barrier");
            struck|=std::ranges::any_of(barriers,[&](const auto& wall){return wall.id==hit.targetId;});
        }
        require(struck,"ordinary, ranged and sapper enemies attack the modular barrier while lured");
    }
}
void progression() {
    require(static_cast<int>(BuildingType::DecoyBeacon)==21 && static_cast<int>(RunUpgradeEffect::UnlockDecoyBeacon)==66,"append-only stable IDs");
    auto map=MapDefinition::defaults();map.obstacles.clear();auto world=WorldConfig::defaults();world.terrainAmplitude=0;world.pondMinimumCount=world.pondMaximumCount=0;
    auto run=std::make_unique<Simulation>(GameBalance::defaults(),map,world);run->startRunFromSeed(PlayerClass::Vanguard,73421);
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockDecoyBeacon),"decoy acquired through shared card pool");
    require(run->snapshot().buildingArsenalCount==2,"decoy takes one defense type slot");
    PlayerCommand god;god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};run->tick(0,god);
    PlayerCommand core;core.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};run->tick(0,core);
    PlayerCommand up;up.upgradeBuilding=UpgradeBuildingCommand{*run->snapshot().coreId,false};run->tick(0,up);
    PlayerCommand place;place.placeBuilding=PlaceBuildingCommand{BuildingType::DecoyBeacon,{3,0},0};run->tick(0,place);run->tick(0,god);
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockDecoyBeacon),"core unlocks shared upgrade");
    require(std::ranges::any_of(run->snapshot().buildings,[](const auto& b){return b.type==BuildingType::DecoyBeacon && b.level==2;}),"card upgrades constructed beacons");
    while(run->snapshot().runUpgradeChoicePending)require(run->selectRunUpgrade(0),"resolve pending choices");
    const auto saved=std::make_unique<SuspendedRunState>(run->saveSuspendedRunState());
    auto restored=std::make_unique<Simulation>(GameBalance::defaults(),map,world);require(restored->loadSuspendedRunState(*saved),"new building and blueprint save/restore");
    require(restored->snapshot().buildingBlueprintLevels[21]==2 && restored->snapshot().unlockedBuildings[21],"beacon level and unlock survive resume");
    CrystalMineSystem producers;BuildingInstance beacon{.type=BuildingType::DecoyBeacon,.maxHealth=220};
    for(std::uint8_t level=1;level<=8;++level) {
        const auto stats=buildingStatsAtLevel(beacon,level,producers);
        require(stats.effectRadius==decoyBeaconRadius(level) && stats.effectDuration==5 && stats.cooldown==8 && stats.attractionTargets==decoyBeaconTargets(level) && !stats.attackDamage,"all eight levels expose support stats without damage");
    }
    std::array<bool,GameBalance::BuildingTypeCount> unlocked;unlocked.fill(true);
    const auto hotbar=makeBuildingHotbarLayout(unlocked,BuildingHotbarCategory::Defense,true,8,true);
    require(hotbar.count==DefenseBuildingOrder.size() && hotbar.indexOf(BuildingType::DecoyBeacon)==10 &&
        hotbar.types[13]==BuildingType::SlowTrap && !hotbar.indexOf(BuildingType::Wall),
        "beacon follows the repair station and precedes traps; walls are modular");
}
}
int main(){selectionAndExpiry();targetingAndAttacks();overlapAndElite();staggeredPulsesAndFreeze();reachablePaths();structuralAndStaticBarriers();progression();}
