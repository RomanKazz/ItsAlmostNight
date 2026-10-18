#include "TestHarness.hpp"
#include "enemies/EnemySystem.hpp"
#include "enemies/EnemyCollision.hpp"
#include "combat/TrapSystem.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/CannonSystem.hpp"
#include "progression/DefenseAttackModifiers.hpp"
#include "game/Simulation.hpp"
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
void lightImpactsAllowApproach() {
    const std::vector<BuildingInstance> core{{.id={1500,1},.type=BuildingType::Core,
        .gridPosition={0,10},.health=10000,.maxHealth=10000}};
    FlowField flow; flow.rebuild({0,10},core);
    EnemySystem enemies;
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={0,0,-8}}});
    const auto id = enemies.enemies()[0].id;
    const double light = TowerSystem::baseStats(BuildingType::Turret,1).knockback;
    for (int tick=0;tick<60;++tick) {
        if (tick % 6 == 0) enemies.applyKnockback(id,{0,0,-1},light);
        enemies.tick(Step,core,flow);
    }
    require(enemies.enemies()[0].position.z > -7,
        "even repeated light tower impacts cannot keep chase switched off");
    const auto before = enemies.enemies()[0].position;
    enemies.applyKnockback(id,{0,0,-1},8);
    enemies.tick(Step,core,flow);
    require(enemies.enemies()[0].position.z < before.z,
        "a strong deliberate impulse still pushes against chase movement");
    require(TowerSystem::baseStats(BuildingType::WindBlower,1).knockback >= 8 &&
        CannonSystem::baseStats(BuildingType::Cannon,1).knockback < 1,
        "native cannon impact is light while the blower retains its control role");
    std::array<double,RunUpgradeEffectCount> powers{};
    powers[runUpgradeIndex(RunUpgradeEffect::AttackKnockback)] = 5;
    const auto modifiers = defenseAttackCardModifiers(powers);
    const auto base = CannonSystem::baseStats(BuildingType::Cannon,1);
    const std::array sources{modifiers};
    requireNear(resolveAttackStats(base,sources).knockback,base.knockback*2,1e-9,
        "impact tome still doubles the new baseline at five powers");
}
void rangedBaseTargets() {
    // The player is nearby, but already reachable structures should be besieged.
    const std::vector<BuildingInstance> base{
        {.id={1600,1},.type=BuildingType::Core,.gridPosition={0,0},.health=1000,.maxHealth=1000},
        {.id={1601,1},.type=BuildingType::GunTurret,.gridPosition={0,-2},.health=1000,.maxHealth=1000}};
    FlowField flow; flow.rebuild({0,0},base);
    EnemySystem siege;
    siege.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Ranged,.position={.5,.85,-7}}});
    bool fired = false, hit = false;
    for (int frame=0; frame<240 && !hit; ++frame) {
        const auto attacks = siege.tick(Step,base,flow,Vec3{3,1.7,-6});
        for (const auto& shot : siege.projectiles()) {
            require(!shot.targetsPlayer && shot.targetId==base[1].id,
                "wizard besieges the nearest building instead of always chasing the nearby player");
            fired = true;
        }
        for (const auto& attack : attacks) hit |= attack.targetId==base[1].id;
    }
    require(fired && hit,"wizard damages a real base building after projectile travel");

    // Same rule for a Core above a platform, independent of melee-storey access.
    for (const double height : {.6, 4.0}) {
        auto raised = std::vector<BuildingInstance>{base[0]};
        raised[0].baseHeight=height; raised[0].platformStorey=height<1 ? 0 : 1;
        flow.rebuild({0,0},raised);
        EnemySystem wizard;
        wizard.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Ranged,.position={.5,.85,-6}}});
        bool damaged = false;
        for (int frame=0; frame<240 && !damaged; ++frame) {
            for (const auto& attack : wizard.tick(Step,raised,flow))
                damaged |= attack.targetId==raised[0].id;
            for (const auto& shot : wizard.projectiles())
                require(shot.targetPosition.y>height+.6,
                    "wizard aims at the raised building body instead of its ground support");
        }
        require(damaged,"ground wizard can shoot a raised Core within three-dimensional range");
    }
    // Tall foundations do not move the actual projectile target below the ground.
    {
        auto raised = std::vector<BuildingInstance>{base[0]};
        raised[0].baseHeight=1.2; raised[0].foundationBottomHeight=-3;
        flow.rebuild({0,0},raised);
        EnemySystem wizard;
        wizard.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Ranged,.position={.5,.85,-6}}});
        wizard.tick(Step,raised,flow);
        require(!wizard.projectiles().empty() && wizard.projectiles()[0].targetPosition.y>2,
            "ranged foundation shot aims above the building base, not below terrain");
    }
    // The new height rule must not grant unlimited vertical range.
    {
        auto raised = std::vector<BuildingInstance>{base[0]};
        raised[0].baseHeight=20; raised[0].platformStorey=5;
        flow.rebuild({0,0},raised);
        EnemySystem wizard;
        wizard.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Ranged,.position={.5,.85,-6}}});
        for (int frame=0; frame<60; ++frame) {
            require(wizard.tick(Step,raised,flow).empty(),"wizard cannot damage an out-of-range high building");
            require(wizard.projectiles().empty(),"wizard respects vertical projectile range");
        }
    }
}

void enemyDamageAtPlayerBoundary() {
    for (const auto type : {EnemyType::Basic,EnemyType::Ranged}) {
        auto balance = GameBalance::defaults();
        balance.gameplay.nightDurationSeconds = 120;
        auto& wave = balance.waves[0];
        wave.basic = type==EnemyType::Basic ? 1 : 0;
        wave.ranged = type==EnemyType::Ranged ? 1 : 0;
        wave.budget = type==EnemyType::Basic ? 1 : 3;
        auto map = MapDefinition::defaults();
        map.playerSpawn = {0,0,-4};
        map.obstacles.clear(); map.resources.clear();
        map.enemySpawnAnchors = {{0,0,-24}};
        auto world = WorldConfig::defaults();
        world.terrainResolution = 33;
        world.terrainAmplitude = world.terrainSurfaceNoiseAmplitude = 0;
        world.pondMinimumCount = world.pondMaximumCount = 0;
        auto run = std::make_unique<Simulation>(balance,map,world);
        run->startRunFromSeed(PlayerClass::Vanguard,73);
        PlayerCommand god; god.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
        run->tick(0,god);
        PlayerCommand place; place.placeBuilding = PlaceBuildingCommand{BuildingType::Core,{0,0},0};
        run->tick(0,place);
        while (run->snapshot().runUpgradeChoicePending) require(run->selectRunUpgrade(0),"fixture resolves upgrades");
        if (run->snapshot().runShop.pending) require(run->closeRunShop(),"fixture closes shop");
        run->tick(0,god);
        PlayerCommand start; start.startWaveEarly = StartWaveEarlyCommand{};
        run->tick(0,start);
        run->takeEvents();
        bool hit = false;
        for (int frame=0;frame<1500 && !hit;++frame) {
            run->tick(Step);
            for (const auto& event : run->takeEvents()) {
                if (event.type!=GameEventType::PlayerDamaged || !event.sourceId) continue;
                const auto attacker = std::ranges::find(run->snapshot().enemies,*event.sourceId,&EnemyInstance::id);
                require(attacker!=run->snapshot().enemies.end(),"melee/projectile damage retains its attacker");
                require(event.amount==static_cast<int>(attacker->damage*2),
                    "real enemy melee and projectile hits double only at the player boundary");
                hit = true;
            }
        }
        require(hit,"both a melee enemy and a ranged enemy can hit the player fixture");
        PlayerCommand debug; debug.damagePlayer = DamagePlayerCommand{3};
        run->tick(0,debug);
        const auto events = run->takeEvents();
        require(std::ranges::any_of(events,[](const auto& event) {
            return event.type==GameEventType::PlayerDamaged && !event.sourceId && event.amount==3;
        }),"debug damage is not doubled together with enemy attacks");
    }
}

}
int main(){meleeGapAndSpikes();diagonalReach();supportQueries();raisedCore();lightImpactsAllowApproach();rangedBaseTargets();enemyDamageAtPlayerBoundary();}
