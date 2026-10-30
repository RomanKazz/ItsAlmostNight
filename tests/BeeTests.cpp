#include "TestHarness.hpp"
#include "enemies/EnemySystem.hpp"
#include "combat/DefenseTargeting.hpp"
#include "combat/WindBlowerAttack.hpp"
#include "combat/TowerSystem.hpp"
#include "navigation/FlowField.hpp"
#include "waves/NightEncounter.hpp"
#include "waves/WaveDirector.hpp"
#include "progression/InsightSystem.hpp"
#include <array>
#include <chrono>
#include <fstream>
using namespace ian;
namespace {
constexpr double Step=1.0/60;
const std::vector<BuildingInstance> Base{{.id={910,1},.type=BuildingType::Core,.gridPosition={0,0},.health=10000,.maxHealth=10000}};
EnemySystem bee(bool elite=false,Vec3 position={0,0,-5.5}) {
    EnemySystem enemies;enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Bee,.position=position,
        .eliteAffixes=elite?eliteAffixMask(EliteAffix::Warden):EliteAffixMask{0}}});return enemies;
}
void behavior(bool elite) {
    auto enemies=bee(elite);FlowField flow;flow.rebuild({0,0},Base);
    double charge=0;bool launched=false,repositioned=false,impact=false;int hits=0;
    Vec3 launch{};
    for(int tick=0;tick<500;++tick) {
        const auto attacks=enemies.tick(Step,Base,flow);
        if(!launched && enemies.enemies()[0].beeWindupRemaining>0)charge+=Step;
        for(const auto& hit:attacks){++hits;impact|=hit.targetId==Base[0].id;}
        if(!launched && !enemies.projectiles().empty()) {
            launched=true;launch=enemies.enemies()[0].position;
            require(enemies.projectiles().size()==(elite?3U:1U),"normal bee fires one stinger; evolved elite fires a three-stinger fan");
            require(charge>=(elite?.95:.70),"actual attacks are preceded by readable charge time");
            for(const auto& shot:enemies.projectiles()) {
                require(shot.stinger && !shot.royal && !shot.targetsPlayer,"stingers have their own visual identity and structure target");
                requireNear(geometry::length(shot.velocity),7,1e-7,"needles use a fixed, dodgeable flight speed");
            }
        }
        if(launched && std::abs(enemies.enemies()[0].position.x-launch.x)>.2)repositioned=true;
        if(impact && repositioned)break;
    }
    require(launched && impact && hits>0 && repositioned,"bee threatens the Core with physical shots and repositions between volleys");
}
void playerAndWalls() {
    FlowField flow;flow.rebuild({0,0},Base);
    auto enemies=bee(false,{0,0,-5.5});
    Vec3 player{0,1.7,-2};bool fired=false;
    for(int frame=0;frame<240 && !fired;++frame) {
        enemies.tick(Step,Base,flow,player);
        fired=!enemies.projectiles().empty();
    }
    require(fired && enemies.projectiles()[0].targetsPlayer,"nearby players can draw bee fire");
    int hits=0;
    for(int frame=0;frame<50;++frame){enemies.tick(Step,Base,flow,Vec3{5,1.7,-2});hits+=static_cast<int>(enemies.playerAttacks().size());}
    require(hits==0,"strafing away after charge dodges a locked stinger");
    auto blocked=bee();bool wallHit=false;
    const std::array<EnemyStructureTarget,1> walls{{{.id={911,1},.position={0,0,-2.5},.radius=1,
        .modular=true,.projectileBlocker=EnemyProjectileBlocker{{-2,0,-2.6},{2,3,-2.4}}}}};
    for(int frame=0;frame<180;++frame)for(const auto& hit:blocked.tick(Step,Base,flow,std::nullopt,walls)) {
        require(hit.targetId!=Base[0].id,"a tall intervening wall catches a structure-directed stinger");
        wallHit|=hit.targetId==walls[0].id;
    }
    require(wallHit,"stingers use swept wall collision rather than damaging through cover");
}
void countersAndWaves() {
    auto enemies=bee();const auto enemy=enemies.enemies()[0];
    require(enemyIsFlying(enemy.type),"bee is classified as airborne in shared navigation and targeting");
    for(auto type:{BuildingType::Catapult,BuildingType::Mortar,BuildingType::GunTurret})
        require(!projectileDefenseCanTarget(type,enemy,0),"ground-only weapons cannot select or collide with bees");
    require(windBlowerCanTarget(enemy,0),"blower can control the bee");
    {
        auto airborne=bee(false,{0,0,-4});TowerSystem wind;
        const std::vector<BuildingInstance> fan{{.id={913,1},.type=BuildingType::WindBlower}};
        wind.syncBuildings(fan);
        for(int frame=0;frame<90;++frame)wind.tick(Step,fan,airborne);
        require(airborne.enemies()[0].knockbackVelocity.z<0,"real gust pushes bees away with their flying resistance");
    }
    require(projectileDefenseCanTarget(BuildingType::TeslaTower,enemy,0) &&
        projectileDefenseCanTarget(BuildingType::MissileLauncher,enemy,0),"anti-air weapons retain bee eligibility");
    const auto balance=GameBalance::defaults();WaveDirector waves(balance.waves);
    require(waves.composition(6).bee==0 && waves.composition(7).bee>0,"bees enter after the ordinary flying tutorial");
    for(auto pattern:{NightPattern::Horde,NightPattern::AirRaid,NightPattern::Crossfire,NightPattern::RoyalCourt}) {
        const auto wave=nightComposition(waves.composition(7),pattern,1);
        const auto counts=waveEnemyCounts(wave);int budget=0;
        for(std::size_t i=0;i<counts.size();++i)if(i!=3)budget+=counts[i]*enemyBudgetCost(static_cast<EnemyType>(i));
        require(budget==wave.budget,"new airborne role stays within each night threat budget");
    }
    require(InsightConfig::defaults().enemy[9]>0,"bee kill rewards have a valid experience entry");
    const auto path=std::filesystem::temp_directory_path()/
        ("ian-bee-insight-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
    {std::ofstream file(path);file<<R"({"rewards":{"enemy":[1,2,3,4,5,6,7,8,9]}})";}
    const auto legacy=loadInsightConfig(path);std::filesystem::remove(path);
    for(std::size_t i=0;i<9;++i)requireNear(legacy.enemy[i],static_cast<double>(i+1),1e-9,
        "legacy nine-role XP configuration retains every existing reward");
    requireNear(legacy.enemy[9],InsightConfig::defaults().enemy[9],1e-9,
        "legacy XP configuration adds bee rewards from the default");
    static_assert(static_cast<int>(EnemyType::Splitling)==8 && static_cast<int>(EnemyType::Bee)==9);
}
}
int main(){behavior(false);behavior(true);playerAndWalls();countersAndWaves();}
