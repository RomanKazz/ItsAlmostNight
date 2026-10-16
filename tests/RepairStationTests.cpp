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
    for (const auto card : {RunUpgradeEffect::UnlockRepairStation, RunUpgradeEffect::UnlockWall, RunUpgradeEffect::UnlockCannon})
        require(run->grantDebugRunUpgrade(card), "station shares the four building arsenal slots");
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockRepairStation), "station level cannot exceed core limit");
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockMortar), "station consumes a defense slot");
    PlayerCommand place; place.placeBuilding = PlaceBuildingCommand{BuildingType::Core, {0,0}, 0};
    run->tick(0, place);
    require(run->snapshot().coreId.has_value(), "repair fixture has a core");
    return run;
}
void load(Simulation& run, const SuspendedRunState& saved) {
    run.returnToMainMenu();require(run.loadSuspendedRunState(saved), "repair checkpoint restores");
}
double health(const Simulation& run, EntityId id) {
    const auto& buildings=run.snapshot().buildings;
    const auto it=std::ranges::find(buildings,id,&BuildingInstance::id);
    require(it!=buildings.end(),"repair target is alive");return it->health;
}
SuspendedRunState damaged(Simulation& run) {
    auto saved=run.saveSuspendedRunState();saved.buildings[0].health-=50;
    saved.buildings.push_back({.id=StationId,.type=BuildingType::RepairStation,.gridPosition={3,0},.health=80,.maxHealth=160});
    saved.buildings.push_back({.id=WallId,.type=BuildingType::Wall,.gridPosition={4,2},.health=50,.maxHealth=100});
    saved.buildings.push_back({.id=CannonId,.type=BuildingType::Cannon,.gridPosition={3,-3},.health=90,.maxHealth=180});
    return saved;
}
void doseAndStats() {
    const std::array sources{RepairStationSource{StationId,{},1},RepairStationSource{{1009,1},{},8}};
    requireNear(repairStationDose(sources,{0,0,2}).healing,11,1e-9,"overlapping stations use strongest pulse, never a sum");
    requireNear(repairStationDose(sources,{0,0,2},3).healing,33,1e-9,"long updates preserve every scheduled pulse");
    require(!repairStationDose(sources,{0,0,7}).source && !repairStationDose(sources,{0,7,0}).source,"3D coverage excludes remote targets");
    requireNear(repairStationDose(std::span{sources}.first(1),{5,0,0}).healing,4,1e-9,"base pulse restores four HP at radius edge");
    require(!repairStationDose(std::span{sources}.first(1),{5.001,0,0}).source,"coverage stops beyond edge");
    for(auto type:{BuildingType::Core,BuildingType::RepairStation,BuildingType::CrystalMine,BuildingType::SlowTrap,BuildingType::SpikeTrap})
        require(!repairStationTarget(type),"core, stations, producers and traps cannot be repaired");
    CrystalMineSystem producers;BuildingInstance station{.id=StationId,.type=BuildingType::RepairStation,.maxHealth=160};
    double radius=0,rate=0;
    for(std::uint8_t level=1;level<=8;++level) {
        const auto stats=buildingStatsAtLevel(station,level,producers);
        require(stats.effectRadius && stats.repairPerSecond && stats.repairInterval==2 && !stats.attackDamage,"support UI exposes rate and interval");
        require(*stats.effectRadius>radius && *stats.repairPerSecond>rate,"all eight levels improve range and rate");
        radius=*stats.effectRadius;rate=*stats.repairPerSecond;
    }
}
void realSimulation() {
    auto run=fixture();auto saved=damaged(*run);
    FoundationSystem modular(run->terrain(),run->terrain().config());
    require(modular.restoreStructures(saved.platformFrames,saved.modularWalls,saved.ramps),"modular fixture retains core supports");
    const Vec3 player{0,1.7,4};
    const auto frame=modular.placePlatformFrame(modular.previewFoundation({6.2,0,.2},player));
    const auto wall=modular.placeWall(modular.previewWall({6.2,0,1.2},player,Rotation::Deg90));
    require(frame && wall && modular.damage(wall->id,20),"modular wall has just received real damage");
    saved.platformFrames.assign(modular.platformFrames().begin(),modular.platformFrames().end());
    saved.modularWalls.assign(modular.walls().begin(),modular.walls().end());load(*run,saved);
    const auto initialWall=health(*run,WallId),initialCannon=health(*run,CannonId);
    const auto initialModular=run->snapshot().modularWalls.back().health;
    const auto initialCore=health(*run,*run->snapshot().coreId),initialStation=health(*run,StationId);
    run->tick(1.9,{});
    requireNear(health(*run,WallId),initialWall,1e-8,"no continuous healing between pulses");
    require(run->snapshot().repairStationLinks.empty(),"no effect without a healing pulse");
    auto checkpoint=run->saveSuspendedRunState();
    // Both the station and its target take another hit just before the pulse.
    std::ranges::find(checkpoint.buildings,WallId,&BuildingInstance::id)->health-=1;
    std::ranges::find(checkpoint.buildings,StationId,&BuildingInstance::id)->health-=1;
    load(*run,checkpoint);run->tick(.1,{});
    requireNear(health(*run,WallId),initialWall-1+4,1e-8,"fresh damage and resume never postpone the scheduled pulse");
    requireNear(health(*run,CannonId),initialCannon+4,1e-8,"neighboring weapon receives four HP in same pulse");
    requireNear(run->snapshot().modularWalls.back().health,initialModular+4,1e-8,"recently hit modular wall also repairs");
    requireNear(health(*run,*run->snapshot().coreId),initialCore,1e-8,"core stays damaged");
    requireNear(health(*run,StationId),initialStation-1,1e-8,"station never repairs itself");
    require(run->snapshot().repairStationLinks.size()==3,"actual healing produces one shared visual pulse");
    const double before=health(*run,WallId);run->tick(.25,{});
    requireNear(health(*run,WallId),before,1e-8,"effects persist without additional healing");
    require(!run->snapshot().repairStationLinks.empty(),"pulse has a visible animation tail");
    run->togglePause();run->tick(20,{});run->togglePause();
    requireNear(health(*run,WallId),before,1e-8,"game pause freezes repair");
    run->tick(1.5,{});require(run->snapshot().repairStationLinks.size()==3,"visual stays for its authored lifetime");
    run->tick(.1,{});require(run->snapshot().repairStationLinks.empty(),"visual fades before next pulse");
    run->tick(.15,{});requireNear(health(*run,WallId),before+4,1e-8,"next pulse keeps two-second cadence");
    checkpoint=run->saveSuspendedRunState();
    std::erase_if(checkpoint.buildings,[](const auto& b){return b.id==StationId;});load(*run,checkpoint);
    const double stopped=health(*run,WallId);run->tick(4,{});
    requireNear(health(*run,WallId),stopped,1e-8,"removed station cannot repair");
    require(run->snapshot().repairStationLinks.empty(),"removed station leaves no stale effect");
    checkpoint.repairStationCycleRemaining=std::numeric_limits<double>::quiet_NaN();
    run->returnToMainMenu();require(!run->loadSuspendedRunState(checkpoint),"invalid repair clock rejected");
}
void coverageAndCadence() {
    auto run=fixture();auto saved=damaged(*run);
    auto& wall=*std::ranges::find(saved.buildings,WallId,&BuildingInstance::id);wall.health=wall.maxHealth-.5;
    std::ranges::find(saved.buildings,CannonId,&BuildingInstance::id)->gridPosition={-5,-3};
    load(*run,saved);const auto outside=health(*run,CannonId);run->tick(2,{});
    const auto& healed=*std::ranges::find(run->snapshot().buildings,WallId,&BuildingInstance::id);
    requireNear(healed.health,healed.maxHealth,1e-8,"repair is capped at max health");
    requireNear(health(*run,CannonId),outside,1e-8,"real simulation respects range");
    auto other=fixture();saved=damaged(*other);load(*other,saved);other->tick(1,{});
    saved=other->saveSuspendedRunState();saved.buildings.push_back({.id={1004,1},.type=BuildingType::RepairStation,.gridPosition={3,3},.health=80,.maxHealth=160});
    load(*other,saved);const auto before=health(*other,WallId);other->tick(1,{});
    requireNear(health(*other,WallId),before+4,1e-8,"staggered construction cannot multiply repair pulses");
    auto bulk=fixture();saved=damaged(*bulk);load(*bulk,saved);
    auto stepped=fixture();load(*stepped,saved);bulk->tick(6.25,{});
    for(int i=0;i<375;++i)stepped->tick(1.0/60,{});
    requireNear(health(*bulk,WallId),health(*stepped,WallId),1e-7,"pulse count is independent of frame size");
    requireNear(bulk->saveSuspendedRunState().repairStationCycleRemaining,stepped->saveSuspendedRunState().repairStationCycleRemaining,1e-7,"clock phase is independent of frame size");
    std::array<bool,GameBalance::BuildingTypeCount> unlocked;unlocked.fill(true);
    const auto layout=makeBuildingHotbarLayout(unlocked,BuildingHotbarCategory::Defense,true,8,true);
    require(layout.count==13 && layout.indexOf(BuildingType::RepairStation)==9 && layout.types[12]==BuildingType::SlowTrap,"station remains before walls and traps");
}
}
int main(){doseAndStats();realSimulation();coverageAndCadence();}
