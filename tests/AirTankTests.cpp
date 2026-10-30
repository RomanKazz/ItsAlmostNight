#include "TestHarness.hpp"
#include "combat/DefenseTargeting.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/AttackCollision.hpp"
#include "navigation/FlowField.hpp"
#include "progression/InsightSystem.hpp"
#include "waves/NightEncounter.hpp"
#include "waves/WaveDirector.hpp"
#include <array>
#include <chrono>
#include <fstream>

using namespace ian;
namespace {
constexpr double Step = 1.0 / 60;
const std::vector<BuildingInstance> Base{
    {.id={900,1}, .type=BuildingType::Core, .gridPosition={0,0}, .health=10000, .maxHealth=10000},
    {.id={901,1}, .type=BuildingType::Wall, .gridPosition={0,-4}, .health=1000, .maxHealth=1000}};
EnemySystem tank(Vec3 position = {0, 2.35, -7}) {
    EnemySystem enemies;
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::AirTank, .position=position}});
    return enemies;
}
void corePressure() {
    auto enemies = tank();
    auto buildings = Base;
    buildings[1].health = 20; // One real headbutt breaks the outer wall.
    FlowField flow;
    flow.rebuild({0,0}, buildings);
    double charge = 0;
    int coreHits = 0;
    double lastCoreHit = 0;
    bool hitWall = false;
    // A nearby player cannot kite this role away from the base.
    for (int tick = 0; tick < 1200 && coreHits < 2; ++tick) {
        const auto attacks = enemies.tick(Step, buildings, flow, Vec3{.5,1.7,-6});
        if (enemies.enemies()[0].airTankWindupRemaining > 0) charge += Step;
        require(enemies.playerAttacks().empty(), "air tank prioritizes base structures over a nearby player");
        bool wallDestroyed = false;
        for (const auto& hit : attacks) {
            require(!hit.ram, "tank headbutt does not emit boss-only ram events");
            requireNear(hit.damage, 36, 1e-9, "heavy strike uses configured damage");
            require(charge > .85, "a visible windup precedes every heavy strike");
            if (hit.targetId == Base[1].id) {
                require(coreHits == 0, "the wall on the flight path is attacked before the Core");
                hitWall = true;
                buildings[1].health -= hit.damage;
                wallDestroyed = buildings[1].health <= 0;
            } else {
                require(hit.targetId == Base[0].id && hitWall,
                    "after breaking the outer building, the tank continues towards the Core");
                if (coreHits > 0) require(tick * Step - lastCoreHit > 2.8,
                    "recovery leaves time to react between headbutts");
                lastCoreHit = tick * Step;
                ++coreHits;
            }
            charge = 0;
        }
        if (wallDestroyed) {
            buildings.erase(buildings.begin() + 1);
            flow.rebuild({0,0}, buildings);
        }
    }
    require(hitWall && coreHits == 2, "tank breaks buildings in its path and still pressures the Core");
}
void pathStructures() {
    for (const auto type : {BuildingType::GunTurret, BuildingType::Cannon, BuildingType::RepairStation}) {
        auto buildings = std::vector<BuildingInstance>{Base[0]};
        buildings.push_back({.id={911,1},.type=type,.gridPosition={0,-4},.health=1000,.maxHealth=1000});
        // A closer structure off the flight corridor must not pull it sideways.
        buildings.push_back({.id={912,1},.type=BuildingType::Wall,.gridPosition={-6,-6},.health=1000,.maxHealth=1000});
        FlowField flow; flow.rebuild({0,0},buildings);
        auto enemies = tank();
        bool hit = false;
        for (int tick=0; tick<600 && !hit; ++tick)
            for (const auto& attack : enemies.tick(Step,buildings,flow)) {
                require(attack.targetId==buildings[1].id,
                    "tank attacks an on-route defense or utility building before the Core, ignoring unrelated side walls");
                hit = true;
            }
        require(hit,"tank can hit every supported class of building in its flight corridor");
    }
    {
        const std::array structures{
            EnemyStructureTarget{.id={920,1},.position={.5,0,-4},.radius=.65,.modular=true},
            EnemyStructureTarget{.id={921,1},.position={.5,0,-5.5},.radius=1.2,.modular=true,
                .traversable=true,.attackable=false}};
        FlowField flow; flow.rebuild({0,0},Base);
        auto enemies = tank();
        bool hit = false;
        for (int tick=0; tick<600 && !hit; ++tick)
            for (const auto& attack : enemies.tick(Step,Base,flow,{},structures)) {
                require(attack.targetId==structures[0].id,
                    "tank attacks modular walls while flying over walkable platform surfaces");
                hit = true;
            }
        require(hit,"modular walls are valid tank targets");
    }
}
void routeProgression() {
    auto buildings = std::vector<BuildingInstance>{Base[0]};
    buildings.push_back({.id={930,1},.type=BuildingType::GunTurret,.gridPosition={0,-2},.health=20,.maxHealth=20});
    buildings.push_back({.id={931,1},.type=BuildingType::Wall,.gridPosition={0,-5},.health=20,.maxHealth=20});
    constexpr std::array order{EntityId{931,1},EntityId{930,1},EntityId{900,1}};
    auto enemies = tank({0,2.35,-9});
    FlowField flow; flow.rebuild({0,0},buildings);
    std::size_t destroyed = 0;
    for (int tick=0; tick<1200 && destroyed<order.size(); ++tick) {
        std::optional<EntityId> remove;
        for (const auto& attack : enemies.tick(Step,buildings,flow)) {
            require(attack.targetId==order[destroyed],
                "flight-path priority is geometric, not the building array order");
            ++destroyed;
            remove = attack.targetId;
        }
        if (remove) {
            std::erase_if(buildings,[&](const auto& building) { return building.id==*remove; });
            if (destroyed<order.size()) flow.rebuild({0,0},buildings);
        }
    }
    require(destroyed==order.size(),"tank destroys the wall, then the turret, and finally attacks the Core");
}
void controlAndDecoy() {
    auto enemies = tank({0,2.35,-3});
    const auto& stats = GameBalance::defaults().enemies;
    require(stats[10].health >= stats[6].health * 6 && stats[10].speed < stats[6].speed,
        "tank trades flight speed for substantially higher health");
    enemies.applyKnockback(enemies.enemies()[0].id, {1,0,0}, 8);
    requireNear(enemies.enemies()[0].knockbackVelocity.x, 2, 1e-9, "tank resists knockback but is not immune");
    const auto& enemy = enemies.enemies()[0];
    for (const auto type : {BuildingType::Catapult, BuildingType::Mortar, BuildingType::GunTurret})
        require(!projectileDefenseCanTarget(type, enemy, 0), "ground weapons cannot acquire an air tank");
    for (const auto type : {BuildingType::TeslaTower, BuildingType::MissileLauncher, BuildingType::LaserTower, BuildingType::WindBlower})
        require(projectileDefenseCanTarget(type, enemy, 0), "anti-air defenses and the blower can counter the tank");
    {
        auto gustTarget = tank({0,2.35,-4});
        TowerSystem wind;
        const std::vector<BuildingInstance> fan{{.id={912,1},.type=BuildingType::WindBlower}};
        wind.syncBuildings(fan);
        for (int tick=0; tick<90; ++tick) wind.tick(Step,fan,gustTarget);
        require(gustTarget.enemies()[0].knockbackVelocity.z < 0, "actual blower gusts affect the tank");
    }
    auto lure = tank({0,2.35,-5});
    auto buildings = std::vector<BuildingInstance>{Base[0]};
    buildings.push_back({.id={902,1},.type=BuildingType::DecoyBeacon,.gridPosition={-3,-4},.health=10000,.maxHealth=10000});
    FlowField flow;
    flow.rebuild({0,0}, buildings);
    bool distracted = false, hitDecoy = false;
    for (int tick=0; tick<900; ++tick) {
        for (const auto& hit : lure.tick(Step,buildings,flow)) hitDecoy |= hit.targetId == buildings.back().id;
        distracted |= lure.enemies()[0].decoyTarget == buildings.back().id;
        if (hitDecoy) break;
    }
    require(distracted && hitDecoy, "beacon redirects the tank into a real attack against the decoy");
    {
        EnemySystem committed;
        for (int tick=0; tick<420; ++tick) committed.tick(Step,buildings,flow);
        committed.spawnGroup(std::array{EnemySpawn{.type=EnemyType::AirTank,.position={0,2.35,-3}}});
        bool finished = false, pulseDuringWindup = false;
        for (int tick=0; tick<180 && !finished; ++tick) {
            const auto attacks = committed.tick(Step,buildings,flow);
            const auto& current = committed.enemies()[0];
            pulseDuringWindup |= current.airTankWindupRemaining > 0 &&
                current.decoyTarget == buildings.back().id;
            for (const auto& hit : attacks) {
                require(hit.targetId == Base[0].id,
                    "new attraction does not cancel or redirect a committed headbutt");
                finished = true;
            }
        }
        require(pulseDuringWindup && finished,
            "tank finishes its prepared strike before following a real beacon pulse");
    }
    auto frozen = tank({0,2.35,-3});
    flow.rebuild({0,0},Base);
    for (int tick=0; tick<200 && frozen.enemies()[0].airTankWindupRemaining<=0; ++tick)
        frozen.tick(Step,Base,flow);
    require(frozen.enemies()[0].airTankWindupRemaining>0, "tank enters its windup");
    require(frozen.applyStatus(frozen.enemies()[0].id,StatusEffectType::Freeze,std::nullopt,2,1), "tank can be frozen");
    for (int tick=0;tick<20;++tick)
        require(frozen.tick(Step,Base,flow).empty(), "freeze pauses an in-progress headbutt");
}
void arenaPlayer() {
    auto enemies = tank({0,2.35,-3});
    FlowField flow;
    flow.rebuild({0,0},Base);
    const Vec3 player{0,1.7,-2};
    bool hit = false;
    for (int tick=0;tick<300;++tick) {
        const auto attacks = enemies.tick(Step,Base,flow,player,{},nullptr,{},true);
        require(attacks.empty(), "challenge targeting overrides the distant Core");
        hit |= !enemies.playerAttacks().empty();
        if (hit) break;
    }
    require(hit,"challenge air tanks attack the arena player");
    auto dodge = tank({0,2.35,-3});
    for (int tick=0;tick<300 && dodge.enemies()[0].airTankWindupRemaining<=0;++tick)
        dodge.tick(Step,Base,flow,player,{},nullptr,{},true);
    require(dodge.enemies()[0].airTankWindupRemaining>0,"arena headbutt commits to its target point");
    for (int tick=0;tick<50;++tick) {
        dodge.tick(Step,Base,flow,Vec3{1.2,1.7,-2},{},nullptr,{},true);
        require(dodge.playerAttacks().empty(),"a sidestep dodges the committed arena headbutt");
    }
}
void wavesAndCompatibility() {
    WaveDirector director;
    require(director.composition(8).airTank==0 && director.composition(9).airTank>0,
        "heavy flyers unlock after the first boss night");
    for (int night=1;night<=24;++night) for (int act=0;act<3;++act) {
        const auto base=director.composition(night);
        const auto wave=nightComposition(base,NightPattern::AirRaid,act);
        const auto counts=waveEnemyCounts(wave);
        int budget=0;
        for (std::size_t i=0;i<counts.size();++i) if (i!=3)
            budget += counts[i]*enemyBudgetCost(static_cast<EnemyType>(i));
        require(budget==wave.budget, "air tank allocation preserves the night threat budget");
    }
    const auto path=std::filesystem::temp_directory_path()/
        ("ian-tank-insight-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    {std::ofstream file(path);file<<R"({"rewards":{"enemy":[1,2,3,4,5,6,7,8,9,10]}})";}
    const auto legacy=loadInsightConfig(path);
    std::filesystem::remove(path);
    for (std::size_t i=0;i<10;++i) requireNear(legacy.enemy[i],static_cast<double>(i+1),1e-9,
        "legacy ten-role config preserves every existing reward");
    requireNear(legacy.enemy[10],12,1e-9,"old configs add the default tank reward");
    static_assert(static_cast<int>(EnemyType::Bee)==9 && static_cast<int>(EnemyType::AirTank)==10);
}
}
int main() { corePressure(); pathStructures(); routeProgression(); controlAndDecoy(); arenaPlayer(); wavesAndCompatibility(); }
