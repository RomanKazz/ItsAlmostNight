#include "TestHarness.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/AttackStatCompatibility.hpp"
#include "buildings/BuildingHotbarLayout.hpp"
#include "game/Simulation.hpp"
#include <memory>
using namespace ian;
namespace {
constexpr double Step=1.0/60;
constexpr EntityId FanId{9001,1};
std::vector<BuildingInstance> structures() {
    return {{.id=FanId,.type=BuildingType::WindBlower,.gridPosition={0,0},.health=160,.maxHealth=160}};
}
void gust() {
    auto buildings=structures();EnemySystem enemies;
    enemies.spawnGroup(std::array{
        EnemySpawn{.type=EnemyType::Basic,.position={0,0,-3}},
        EnemySpawn{.type=EnemyType::Heavy,.position={1,0,-3}},
        EnemySpawn{.type=EnemyType::Basic,.position={0,0,3}},
        EnemySpawn{.type=EnemyType::Basic,.position={4,0,-1}},
        EnemySpawn{.type=EnemyType::Boss,.position={-1,0,-4}},
        EnemySpawn{.type=EnemyType::Flying,.position={0,0,-4}}});
    TowerSystem towers;towers.syncBuildings(buildings);
    int pulses=0;
    for(int frame=0;frame<60;++frame) {
        require(towers.tick(Step,buildings,enemies).empty(),"wind never creates damage impacts");
        pulses+=static_cast<int>(towers.launches().size());
    }
    require(pulses==1,"one real gust followed by cooldown");
    const auto& all=enemies.enemies();
    requireNear(all[0].knockbackVelocity.z,-8,1e-7,"ordinary enemy receives full outward push");
    require(all[1].knockbackVelocity.z<0 && std::abs(all[1].knockbackVelocity.z)<3,"heavy enemy resists wind");
    for(int i:{2,3,4})requireNear(geometry::length(all[static_cast<std::size_t>(i)].knockbackVelocity),0,1e-9,"rear, side and boss enemies unaffected");
    requireNear(all[5].knockbackVelocity.z,-5.6,1e-7,"flying enemy receives outward push with its normal resistance");
    for(const auto& enemy:all)requireNear(enemy.health,enemy.maxHealth,1e-9,"gust preserves all enemy health");
    require(towers.projectiles().empty(),"no projectile allocations for wind");
    for(int frame=0;frame<120;++frame){towers.tick(Step,buildings,enemies);pulses+=static_cast<int>(towers.launches().size());}
    require(pulses==2,"gust repeats at its configured period");
    requireNear(enemies.enemies()[0].knockbackVelocity.z,-8,1e-7,"overlapping gust cannot stack full speed indefinitely");
    towers.clearProjectiles();require(towers.towers()[0].windAge>=WindBlowerPulseSeconds,"clear removes residual wind visual");
    buildings.clear();towers.syncBuildings(buildings);require(towers.towers().empty(),"selling removes rotor and gust runtime");
}
void targetingAndAnimation() {
    auto buildings=structures();EnemySystem enemies;TowerSystem towers;towers.syncBuildings(buildings);
    for(int frame=0;frame<60;++frame){towers.tick(Step,buildings,enemies);require(towers.launches().empty(),"idle fan never fires");}
    require(std::abs(towers.towers()[0].windRotor)>0.1,"idle rotor spins");
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={2,0,-4}}});
    for(int frame=0;frame<120;++frame)towers.tick(Step,buildings,enemies);
    require(towers.towers()[0].yaw<-.2,"head smoothly aims at off-axis target");
    const double yaw=towers.towers()[0].yaw;enemies.reset();
    for(int frame=0;frame<60;++frame)towers.tick(Step,buildings,enemies);
    requireNear(towers.towers()[0].yaw,yaw,1e-9,"head holds aim after target loss");
    buildings[0].rotation=2;towers.syncBuildings(buildings);
    for(int frame=0;frame<120;++frame)towers.tick(Step,buildings,enemies);
    requireNear(towers.towers()[0].yaw,PiRadians/2,1e-6,"manual rotation moves idle fan");
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Boss,.position={-3,0,0}}});
    for(int frame=0;frame<60;++frame){towers.tick(Step,buildings,enemies);require(towers.launches().empty() && !towers.towers()[0].targetId,"immune boss does not consume gusts");}
}
void flyingTarget() {
    auto buildings=structures();EnemySystem enemies;TowerSystem towers;towers.syncBuildings(buildings);
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Flying,.position={1,0,-4}}});
    bool launched=false;
    for(int frame=0;frame<120 && !launched;++frame) {
        towers.tick(Step,buildings,enemies);launched=!towers.launches().empty();
    }
    require(launched && towers.towers()[0].targetId==enemies.enemies()[0].id,
        "fan acquires a lone flying target and turns toward it");
    const auto before=enemies.enemies()[0].position;
    FlowField flow;
    const std::vector<BuildingInstance> targets{{.id={9002,1},.type=BuildingType::Core,.gridPosition={0,6}}};
    flow.rebuild({0,6},targets);
    enemies.tick(Step,targets,flow);
    require(enemies.enemies()[0].position.z<before.z,
        "gust moves a flying enemy away from the core in the real movement tick");
    requireNear(enemies.enemies()[0].health,enemies.enemies()[0].maxHealth,1e-9,
        "airborne control does not deal damage");
}
void modifiersAndLevels() {
    TowerSystem towers;auto building=structures()[0];AttackModifiers modifiers;
    modifiers[AttackStat::Quantity].flat=20;modifiers[AttackStat::Damage].flat=100;
    modifiers[AttackStat::Bounces].flat=20;modifiers[AttackStat::Pierce].flat=20;
    modifiers[AttackStat::Knockback].multiplier=1.5;modifiers[AttackStat::Range].multiplier=1.2;
    modifiers[AttackStat::Size].multiplier=1.3;modifiers[AttackStat::AttackSpeed].multiplier=1.4;
    towers.setAttackModifiers(modifiers);const auto final=towers.finalStats(building);
    requireNear(final.quantity,1,1e-9,"quantity does not duplicate a support impulse");
    requireNear(final.damage,0,1e-9,"damage bonuses cannot turn wind into damage");
    requireNear(final.bounces+final.pierce,0,1e-9,"unsupported count stats ignored");
    requireNear(final.knockback,12,1e-8,"knockback bonuses affect wind");
    requireNear(windBlowerRange(final),5.5*1.2*1.3,1e-8,"range and size affect actual cone reach");
    requireNear(final.attackInterval,2.8/1.4,1e-8,"fire rate affects real interval");
    CrystalMineSystem producers;double previousRange=0,previousPush=0,previousInterval=10;
    for(std::uint8_t level=1;level<=8;++level) {
        const auto stats=buildingStatsAtLevel(building,level,producers);
        require(stats.attackRange>previousRange && stats.pushStrength>previousPush && stats.cooldown<previousInterval && !stats.attackDamage,"all eight levels improve meaningful support stats");
        previousRange=*stats.attackRange;previousPush=*stats.pushStrength;previousInterval=*stats.cooldown;
    }
}
void progression() {
    require(static_cast<int>(BuildingType::WindBlower)==22 && static_cast<int>(RunUpgradeEffect::UnlockWindBlower)==67,"append-only IDs");
    auto map=MapDefinition::defaults();map.obstacles.clear();auto world=WorldConfig::defaults();world.terrainAmplitude=0;world.pondMinimumCount=world.pondMaximumCount=0;
    auto run=std::make_unique<Simulation>(GameBalance::defaults(),map,world);run->startRunFromSeed(PlayerClass::Vanguard,73421);
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockWindBlower),"shared card unlocks fan");
    require(run->snapshot().buildingArsenalCount==2 && run->snapshot().buildingBlueprintLevels[22]==1,"takes a defense slot and is available in snapshot");
    require(run->snapshot().buildingCosts[22].wood==30,"construction price exposed");
    PlayerCommand god;god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};run->tick(0,god);
    PlayerCommand core;core.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};run->tick(0,core);
    PlayerCommand up;up.upgradeBuilding=UpgradeBuildingCommand{*run->snapshot().coreId,false};run->tick(0,up);
    PlayerCommand place;place.placeBuilding=PlaceBuildingCommand{BuildingType::WindBlower,{3,0},0};run->tick(0,place);run->tick(0,god);
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockWindBlower),"core permits level two card");
    require(std::ranges::any_of(run->snapshot().buildings,[](const auto& b){return b.type==BuildingType::WindBlower && b.level==2;}),"card upgrades built fan");
    while(run->snapshot().runUpgradeChoicePending)require(run->selectRunUpgrade(0),"resolve choices");
    const auto saved=std::make_unique<SuspendedRunState>(run->saveSuspendedRunState());
    auto restored=std::make_unique<Simulation>(GameBalance::defaults(),map,world);require(restored->loadSuspendedRunState(*saved),"fan save restores");
    require(restored->snapshot().buildingBlueprintLevels[22]==2 && restored->snapshot().unlockedBuildings[22],"fan level and unlock persist");
    std::array<bool,GameBalance::BuildingTypeCount> unlocked;unlocked.fill(true);
    const auto bar=makeBuildingHotbarLayout(unlocked,BuildingHotbarCategory::Defense,true,8,true);
    require(bar.count==DefenseBuildingOrder.size() && bar.types[11]==BuildingType::WindBlower &&
        bar.types[13]==BuildingType::SlowTrap && !bar.indexOf(BuildingType::Wall),
        "support fan precedes defense traps while walls remain in the modular section");
}
}
int main(){gust();targetingAndAnimation();flyingTarget();modifiersAndLevels();progression();}
