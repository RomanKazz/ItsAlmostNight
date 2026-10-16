#include "TestHarness.hpp"
#include "enemies/EnemySystem.hpp"
#include "enemies/EnemyCollision.hpp"
#include "combat/TrapSystem.hpp"
#include "world/CollisionWorld.hpp"
#include "world/TerrainHeightfield.hpp"
#include <array>
#include <algorithm>
#include <memory>
using namespace ian;
namespace {
constexpr double Step=1.0/60;
void meleeGapAndSpikes() {
    const std::vector<BuildingInstance> buildings{
        {.id={1000,1},.type=BuildingType::Core,.gridPosition={0,10},.health=500,.maxHealth=500},
        {.id={1001,1},.type=BuildingType::Wall,.gridPosition={0,0},.health=100,.maxHealth=100}};
    FlowField flow;flow.rebuild({0,10},buildings);
    EnemySystem enemies;enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={.5,0,-1.4}}});
    const auto first=enemies.tick(Step,buildings,flow);
    require(std::ranges::none_of(first,[](const auto& a){return a.targetId==EntityId{1001,1};}),"melee cannot hit a wall across a visible half-meter gap");
    enemies.reset();enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={.5,0,-.9}}});
    bool hitWall=false;
    for(int i=0;i<240 && !hitWall;++i) {
        for(const auto& attack:enemies.tick(Step,buildings,flow)) {
            if(attack.targetId!=EntityId{1001,1})continue;
            const auto& enemy=enemies.enemies()[0];
            const double gap=std::hypot(enemy.position.x-.5,enemy.position.z-.5)-.5-enemyCollisionCylinder(enemy).radius;
            require(gap<=.151,"wall hit requires close body contact");hitWall=true;
        }
    }
    require(hitWall,"shorter range still reaches and attacks the wall");
    std::vector<BuildingInstance> withSpikes=buildings;
    withSpikes.push_back({.id={1002,1},.type=BuildingType::SpikeTrap,.gridPosition={0,-1},.health=100,.maxHealth=100});
    EnemySystem crossing;crossing.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={.5,0,-1.7}}});
    TrapSystem traps;traps.syncBuildings(withSpikes);
    require(!traps.tick(Step,withSpikes,crossing).empty(),"spikes trigger when a real mob cylinder overlaps their tile");
    const auto before=crossing.enemies()[0].health;
    require(before<crossing.enemies()[0].maxHealth,"spike contact damages the wall attacker");
}
void diagonalReach() {
    const std::vector<BuildingInstance> buildings{
        {.id={1100,1},.type=BuildingType::Core,.gridPosition={0,10},.health=500,.maxHealth=500},
        {.id={1101,1},.type=BuildingType::Wall,.gridPosition={0,0},.health=100,.maxHealth=100}};
    FlowField flow;flow.rebuild({0,10},buildings);EnemySystem enemies;
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={1.2,0,-1.2}}});
    for(int i=0;i<60;++i)for(const auto& attack:enemies.tick(Step,buildings,flow))if(attack.targetId==EntityId{1101,1}) {
        const auto& e=enemies.enemies()[0];
        require(std::hypot(e.position.x-.5,e.position.z-.5)-.5-enemyCollisionCylinder(e).radius<=.151,"diagonal hit uses actual distance, not forward projection");
    }
}
void supportQueries() {
    CollisionWorld collision(48,{});
    const std::vector<BuildingInstance> fence{{.id={1300,1},.type=BuildingType::Wall,.gridPosition={0,0},.health=100,.maxHealth=100}};
    collision.syncBuildings(fence);
    require(!collision.enemySupportHeight(.5,.5,.9,1.15),"ground mobs do not stand on fence caps");
    require(collision.playerSupportHeight(.5,.5,.35,1.15).has_value(),"player can still land on the fence");
    const std::array upper{PlatformFrameInstance{.id={1301,1},.anchor={0,0,0},.floorHeight=4,.storey=1}};
    collision.syncModularBuildings({upper,{}, {},1});
    require(!collision.enemySupportHeight(1,1,.9,1.15),"footprint support cannot lift a ground mob through a high floor");
    requireNear(collision.enemySupportHeight(2.5,1,.9,4.1).value_or(-1),4,1e-8,"body retains partial support at a platform edge");
    require(!collision.enemySupportHeight(3.1,1,.9,4.1),"support disappears once the body leaves the platform");
    EnemySystem airborne;airborne.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Flying,.position={.5,0,.5}}});
    std::vector<BuildingInstance> spikes{{.id={1302,1},.type=BuildingType::SpikeTrap,.gridPosition={0,0},.health=100,.maxHealth=100}};
    TrapSystem traps;traps.syncBuildings(spikes);
    require(traps.tick(Step,spikes,airborne).empty(),"ground spikes do not hit flyers above the tile");
}
void raisedCore() {
    auto config=WorldConfig::defaults();config.terrainAmplitude=config.terrainSurfaceNoiseAmplitude=0;
    config.pondMinimumCount=config.pondMaximumCount=0;config.terrainResolution=33;
    TerrainHeightfield terrain(config);terrain.generate(5123);
    const double ground=terrain.getHeight(1,1),floor=ground+.8;
    const std::array frames{PlatformFrameInstance{.id={1200,1},.anchor={0,0,0},.floorHeight=floor,.storey=0}};
    const std::vector<BuildingInstance> buildings{{.id={1201,1},.type=BuildingType::Core,
        .gridPosition={1,1},.health=500,.maxHealth=500,.baseHeight=floor,.platformStorey=0,.foundationBottomHeight=ground}};
    CollisionWorld collision(48,{});collision.syncModularBuildings({frames,{}, {},1});
    FlowField flow(std::vector<CollisionBox>{},&terrain);flow.rebuild({1,1},buildings);
    // Several approaches expose the core's circular collision radius standing
    // outside the 2x2 floor. Height must not alternate with target-facing yaw.
    for(Vec3 spawn:std::array<Vec3,4>{{{1,0,-5},{-5,0,1},{7,0,1},{1,0,7}}}) {
        auto enemies=std::make_unique<EnemySystem>();enemies->spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position=spawn}});
        bool attacked=false;int reversals=0;double previous=ground,previousDelta=0;
        for(int tick=0;tick<600;++tick) {
            const auto attacks=enemies->tick(Step,buildings,flow,{}, {},&terrain,{frames,{},1,&collision});
            const auto& enemy=enemies->enemies()[0];
            const double height=enemy.worldSurfaceHeight,delta=height-previous;
            if(previousDelta>.015 && delta<-.015)++reversals;
            if(std::abs(delta)>.015)previousDelta=delta;
            for(const auto& hit:attacks)if(hit.targetId==EntityId{1201,1}) {
                attacked=true;
                requireNear(height,floor,.025,"attacker stays on supported platform height");
            }
            if(attacked)require(height>floor-.05,"core attack does not repeatedly fall off the platform");
            previous=height;
        }
        require(attacked,"ground enemy reaches a raised ground-storey core");
        require(reversals==0,"platform approach has no repeated up/down oscillations");
    }
}
}
int main(){meleeGapAndSpikes();diagonalReach();supportQueries();raisedCore();}
