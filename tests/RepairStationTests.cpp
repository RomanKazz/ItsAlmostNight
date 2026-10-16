#include "TestHarness.hpp"
#include "buildings/BuildingHotbarLayout.hpp"
#include "buildings/BuildingStats.hpp"
#include "game/Simulation.hpp"

#include <algorithm>
#include <limits>
#include <memory>

namespace {
using namespace ian;
constexpr EntityId StationId{1001, 1}, WallId{1002, 1}, CannonId{1003, 1};
std::unique_ptr<Simulation> fixture() {
    auto balance = GameBalance::defaults();
    balance.buildings[0].wood = balance.buildings[0].stone = balance.buildings[0].crystals = 0;
    auto map = MapDefinition::defaults(); map.resources.clear(); map.obstacles.clear();
    auto world = WorldConfig::defaults();
    world.terrainResolution = 33; world.terrainAmplitude = world.terrainSurfaceNoiseAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto run = std::make_unique<Simulation>(balance, map, world);
    run->startRunFromSeed(PlayerClass::Vanguard, 81875);
    for (const auto card : {RunUpgradeEffect::UnlockRepairStation, RunUpgradeEffect::UnlockWall,
            RunUpgradeEffect::UnlockCannon})
        require(run->grantDebugRunUpgrade(card), "repair station shares the four building arsenal slots");
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockRepairStation), "station level cannot exceed the core limit");
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMortar), "station consumes a real defense slot");
    PlayerCommand place; place.placeBuilding = PlaceBuildingCommand{BuildingType::Core, {0,0}, 0};
    run->tick(0, place);
    require(run->snapshot().coreId.has_value(), "repair fixture has a core");
    return run;
}
void load(Simulation& run, const SuspendedRunState& saved) {
    run.returnToMainMenu();
    require(run.loadSuspendedRunState(saved), "damaged station checkpoint restores");
}
double health(const Simulation& run, EntityId id) {
    const auto& buildings = run.snapshot().buildings;
    const auto it = std::ranges::find(buildings, id, &BuildingInstance::id);
    require(it != buildings.end(), "repair target is alive");
    return it->health;
}
SuspendedRunState damaged(Simulation& run, double delay = 0) {
    auto saved = run.saveSuspendedRunState();
    saved.buildings[0].health -= 50;
    saved.buildings.push_back({.id=StationId,.type=BuildingType::RepairStation,
        .gridPosition={3,0},.health=80,.maxHealth=160});
    saved.buildings.push_back({.id=WallId,.type=BuildingType::Wall,
        .gridPosition={4,2},.health=50,.maxHealth=100,.repairDelayRemaining=delay});
    saved.buildings.push_back({.id=CannonId,.type=BuildingType::Cannon,
        .gridPosition={3,-3},.health=90,.maxHealth=180,.repairDelayRemaining=delay});
    return saved;
}
void doseAndStats() {
    const std::array sources{RepairStationSource{StationId, {}, 1, 0},
        RepairStationSource{{1009,1}, {}, 8, 0}};
    requireNear(repairStationDose(sources,{0,0,2},0,1).healing,5.5,1e-9,"overlapping sources use the strongest rate, never a sum");
    require(!repairStationDose(sources,{0,0,7},0,1).source,"outside range is never repaired");
    require(!repairStationDose(sources,{0,7,0},0,1).source,"height matters for coverage");
    requireNear(repairStationDose(sources,{},.75,1).healing,1.375,1e-9,"partial cooldown expiry repairs only the remaining quarter tick");
    const std::array paused{RepairStationSource{StationId,{},1,6}};
    requireNear(repairStationDose(std::span{sources}.first(1),{5,0,0},0,1).healing,2,1e-9,"coverage includes the exact outer edge");
    require(!repairStationDose(std::span{sources}.first(1),{5.001,0,0},0,1).source,"coverage stops immediately beyond the edge");
    require(!repairStationDose(paused,{},0,1).source,"damaged station also pauses its work");
    for (auto type : {BuildingType::Core,BuildingType::RepairStation,BuildingType::CrystalMine,
            BuildingType::SlowTrap,BuildingType::SpikeTrap}) require(!repairStationTarget(type),"station cannot sustain core, itself, producers or traps");
    CrystalMineSystem producers;
    BuildingInstance station{.id=StationId,.type=BuildingType::RepairStation,.maxHealth=160};
    double radius=0,rate=0;
    for (std::uint8_t level=1;level<=8;++level) {
        const auto stats=buildingStatsAtLevel(station,level,producers);
        require(stats.effectRadius && stats.repairPerSecond && stats.repairDelay &&
            !stats.attackDamage && !stats.finalAttack,"support stats do not pretend to be an attack");
        require(*stats.effectRadius>radius && *stats.repairPerSecond>rate,"all eight card levels improve repair range and rate");
        radius=*stats.effectRadius;rate=*stats.repairPerSecond;
    }
}
void realSimulation() {
    auto run=fixture(); auto saved=damaged(*run,6);
    FoundationSystem modular(run->terrain(),run->terrain().config());
    require(modular.restoreStructures(saved.platformFrames,saved.modularWalls,saved.ramps),"modular repair fixture retains core supports");
    const Vec3 player{0,1.7,4};
    const auto frame=modular.placePlatformFrame(modular.previewFoundation({6.2,0,.2},player));
    const auto wall=modular.placeWall(modular.previewWall({6.2,0,1.2},player,Rotation::Deg90));
    require(frame && wall,"modular repair fixture places a supported wall");
    require(modular.damage(wall->id,20).has_value(),"modular wall receives real damage");
    requireNear(modular.walls().back().repairDelayRemaining,6,1e-9,"modular damage also starts the repair pause");
    saved.platformFrames.assign(modular.platformFrames().begin(),modular.platformFrames().end());
    saved.modularWalls.assign(modular.walls().begin(),modular.walls().end());
    load(*run,saved);
    const auto initialModular=run->snapshot().modularWalls.back().health;
    const auto initialWall=health(*run,WallId), initialCannon=health(*run,CannonId);
    const auto initialCore=health(*run,*run->snapshot().coreId), initialStation=health(*run,StationId);
    run->tick(5.5,{});
    requireNear(health(*run,WallId),initialWall,1e-8,"no repair before the target quiet period");
    auto checkpoint=run->saveSuspendedRunState();load(*run,checkpoint);
    run->tick(.5,{});
    requireNear(health(*run,WallId),initialWall,1e-8,"resume retains the unfinished pause exactly");
    run->tick(1,{});
    requireNear(health(*run,WallId),initialWall+2,1e-8,"station repairs a nearby wall at two HP per second");
    requireNear(health(*run,CannonId),initialCannon+2,1e-8,"same station repairs its neighboring weapon");
    requireNear(health(*run,*run->snapshot().coreId),initialCore,1e-8,"core stays damaged");
    requireNear(health(*run,StationId),initialStation,1e-8,"station never repairs itself");
    requireNear(run->snapshot().modularWalls.back().health,initialModular+2,1e-8,"station also repairs the supported modular wall");
    require(run->snapshot().repairStationLinks.size()==3,"only real healing produces repair visuals");
    const auto beforePause=health(*run,WallId);run->togglePause();run->tick(20,{});
    requireNear(health(*run,WallId),beforePause,1e-8,"paused game never repairs");run->togglePause();
    checkpoint=run->saveSuspendedRunState();
    auto station=std::ranges::find(checkpoint.buildings,StationId,&BuildingInstance::id);
    station->repairDelayRemaining=6;load(*run,checkpoint);run->tick(1,{});
    requireNear(health(*run,WallId),beforePause,1e-8,"damage to station pauses repair of otherwise quiet neighbors");
    checkpoint=run->saveSuspendedRunState();
    std::erase_if(checkpoint.buildings,[](const auto& b){return b.id==StationId;});load(*run,checkpoint);run->tick(1,{});
    requireNear(health(*run,WallId),beforePause,1e-8,"selling or destroying a station immediately stops repair");
    require(run->snapshot().repairStationLinks.empty(),"removed stations leave no stale effects");
    checkpoint.buildings[1].repairDelayRemaining=std::numeric_limits<double>::quiet_NaN();
    run->returnToMainMenu();require(!run->loadSuspendedRunState(checkpoint),"invalid repair timers are rejected on restore");
}
void coverageAndCaps() {
    auto run=fixture();auto saved=damaged(*run);
    auto& wall=*std::ranges::find(saved.buildings,WallId,&BuildingInstance::id);
    wall.health=wall.maxHealth-.5;
    auto& cannon=*std::ranges::find(saved.buildings,CannonId,&BuildingInstance::id);
    cannon.gridPosition={-5,-3};
    load(*run,saved);const auto outside=health(*run,CannonId);
    run->tick(1,{});
    const auto& healed=*std::ranges::find(run->snapshot().buildings,WallId,&BuildingInstance::id);
    requireNear(healed.health,healed.maxHealth,1e-8,"repair never exceeds max health");
    requireNear(health(*run,CannonId),outside,1e-8,"real simulation respects the repair radius");
    auto other=fixture();saved=damaged(*other);
    saved.buildings.push_back({.id={1004,1},.type=BuildingType::RepairStation,
        .gridPosition={3,3},.health=80,.maxHealth=160});
    load(*other,saved);const auto before=health(*other,WallId);other->tick(1,{});
    requireNear(health(*other,WallId),before+2,1e-8,"two physical stations still provide a single repair rate");
}
void damageAndRegistration() {
    BuildingSystem buildings;
    auto definitions=GameBalance::defaults().buildings;
    definitions[0].wood=definitions[0].stone=definitions[0].crystals=0;
    buildings=BuildingSystem(definitions,GameBalance::defaults().economy,20);
    require(buildings.place(BuildingType::Core,{0,0},0,999,999,999).has_value(),"damage fixture places core");
    const auto wall=buildings.place(BuildingType::Wall,{3,0},0,999,999,999);
    require(wall.has_value(),"damage fixture places wall");
    require(buildings.damage(wall->building.id,1).has_value(),"any positive damage hits the wall");
    requireNear(buildings.buildings().back().repairDelayRemaining,6,1e-9,"real building damage starts six-second pause");
    buildings.advanceRepairDelays(2);
    require(buildings.damage(wall->building.id,1).has_value(),"repeated damage hits");
    requireNear(buildings.buildings().back().repairDelayRemaining,6,1e-9,"every new hit restarts the full pause");
    std::array<bool,GameBalance::BuildingTypeCount> unlocked;unlocked.fill(true);
    const auto layout=makeBuildingHotbarLayout(unlocked,BuildingHotbarCategory::Defense,true,8,true);
    require(layout.count==13 && layout.indexOf(BuildingType::RepairStation)==9 &&
        layout.types[12]==BuildingType::SlowTrap,"station is selectable before walls and traps without overflowing the hotbar");
    require(runUpgradeBuilding(RunUpgradeEffect::UnlockRepairStation)==BuildingType::RepairStation &&
        runDefenseCard(RunUpgradeEffect::UnlockRepairStation),"station is registered as a leveled defense card");
}
}
int main(){doseAndStats();damageAndRegistration();realSimulation();coverageAndCaps();}
