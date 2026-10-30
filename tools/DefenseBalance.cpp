#include "buildings/BuildingSystem.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/RepairStation.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/CombatDisplayUnits.hpp"
#include "combat/DefenseTargeting.hpp"
#include "combat/LaserAttack.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "navigation/FlowField.hpp"
#include "progression/BuildingStatUpgrades.hpp"
#include "world/CollisionWorld.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace ian;
constexpr double Step = 1.0 / 60.0;
constexpr std::array Types{BuildingType::Turret, BuildingType::GunTurret,
    BuildingType::Cannon, BuildingType::Catapult, BuildingType::Mortar,
    BuildingType::Flamethrower, BuildingType::MissileLauncher,
    BuildingType::TeslaTower, BuildingType::LaserTower, BuildingType::WindBlower,
    BuildingType::SpikeTrap, BuildingType::SlowTrap};
constexpr std::array Names{"crossbow", "machine_gun", "cannon", "catapult", "mortar",
    "flamethrower", "rocket", "tesla", "laser", "wind_blower", "spikes", "slow_trap"};
constexpr std::array Scenes{"single_infinite", "single_infinite_heavy", "dense_line", "dense_cluster",
    "spread_group", "fast", "flying", "mixed", "passing", "single_normal",
    "single_fast", "single_heavy", "single_boss"};
constexpr EntityId CoreId{1000,1}, DefenseId{1001,1};

bool ballistic(BuildingType type) {
    return type == BuildingType::Cannon || type == BuildingType::Catapult || type == BuildingType::Mortar;
}
bool trap(BuildingType type) { return type == BuildingType::SpikeTrap || type == BuildingType::SlowTrap; }
bool airCapable(BuildingType type) {
    return type != BuildingType::SpikeTrap && defenseCanTargetFlying(type);
}
double rangeOf(BuildingType type) {
    return ballistic(type) ? CannonSystem::attackRange(type,1) :
        trap(type) ? (type == BuildingType::SpikeTrap ? TrapSystem::spikeTriggerRadius(1) : TrapSystem::triggerRadius(1)) :
        TowerSystem::attackRange(type,1);
}
double idealDistance(BuildingType type) {
    if (trap(type)) return 0.15;
    // Shared lane centre for the long-range weapons. Only short-range
    // defenses move closer so the ideal test remains an actual attack test.
    if(type==BuildingType::Flamethrower)return 2.6;
    if(type==BuildingType::WindBlower)return 4.5;
    return 6.5;
}
std::uint64_t key(EntityId id) { return (std::uint64_t(id.index)<<32U)|id.generation; }
int number(std::string_view text,int minimum,int maximum) {
    int value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||value<minimum||value>maximum)
        throw std::runtime_error("invalid numeric argument");
    return value;
}
void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }

struct Result {
    double raw{},effective{},overkill{},slowSeconds{},slowWeighted{},pushDistance{},exposure{};
    double direct{},burn{},primary{},secondary{},focusExtraEstimate{},focusMultiplierSum{};
    double idleValidSeconds{},firstAttackDelay{-1},ttkSeconds{-1},maxFocusMultiplier{1};
    double focusX2Seconds{-1},focusX3Seconds{-1},focusX4Seconds{-1};
    double focusAtKill{1};
    int spawned{},kills{},leaked{},alive{},hits{},directHits{},attacks{},projectiles{},activations{};
    int peakTargets{},uniqueHit{},affected{},taunted{},peakTaunted{},peakProjectiles{},dropped{};
    int validTicks{},attackTicks{},engagedTicks{},sampleTicks{},focusTicks{},slowTicks{},firstValidTick{-1};
};

EnemyType spawnType(int scene,int serial,int seed) {
    if(scene==1)return EnemyType::Heavy;
    if(scene==10)return EnemyType::Fast;
    if(scene==11)return EnemyType::Heavy;
    if(scene==12)return EnemyType::Boss;
    if(scene==5)return EnemyType::Fast;
    if(scene==6)return EnemyType::Flying;
    if(scene==7) {
        constexpr std::array mix{EnemyType::Basic,EnemyType::Basic,EnemyType::Basic,EnemyType::Basic,
            EnemyType::Basic,EnemyType::Basic,EnemyType::Basic,EnemyType::Basic,EnemyType::Basic,EnemyType::Basic,
            EnemyType::Fast,EnemyType::Fast,EnemyType::Fast,EnemyType::Fast,EnemyType::Fast,
            EnemyType::Heavy,EnemyType::Heavy,EnemyType::Heavy,EnemyType::Heavy,EnemyType::Ranged};
        return mix[static_cast<std::size_t>((serial*7+seed*11)%int(mix.size()))];
    }
    return EnemyType::Basic;
}
int population(int scene) {
    if(scene<=1||scene>=9)return 1;
    if(scene==2)return 16;
    if(scene==3||scene==4)return 24;
    return 16;
}
Vec3 spawnPosition(BuildingType type,int scene,int serial,int seed) {
    const double distance=idealDistance(type);
    // Fixed layout across buildings for each wave; single-target placement is
    // the only range-aware exception, so every weapon can actually engage.
    const double jitter=(double((serial*17+seed*13)%11)-5.0)*.033;
    if(scene<=1||scene>=9)return {jitter,0,-distance};
    if(scene==2)return {(double(serial%3)-1)*.28+jitter,0,-distance+(double((serial/3)%9)-4)*.42};
    if(scene==3)return {(double(serial%6)-2.5)*.35+jitter,0,-distance+(double(serial/6)-1.5)*.34};
    if(scene==4)return {(double(serial%6)-2.5)*1.15+jitter,0,-distance+(double(serial/6)-1.5)*1.2};
    // The passing path has a fixed lateral offset for all ranged weapons.
    // Traps are placed under the same path in their own relevant benchmark.
    if(scene==8)return {trap(type)?0.4:2.0,0,-14.0-double(serial%4)*1.0};
    return {(double(serial%5)-2)*.55+jitter,0,-10.0-double((serial/5)%4)*.6};
}

Result runCombat(const GameBalance& balance,BuildingType type,int scene,int seed,int seconds,
                 const BuildingUpgradePowers* upgrades=nullptr) {
    auto definitions=balance.enemies;
    const bool finite=scene>=9;
    const bool stationary=scene<=4||finite;
    if(stationary)for(auto& definition:definitions)definition.speed=0;
    auto enemies=std::make_unique<EnemySystem>(definitions);
    enemies->setDamageSampling(true);
    auto towers=std::make_unique<TowerSystem>();
    auto cannons=std::make_unique<CannonSystem>();
    auto traps=std::make_unique<TrapSystem>();
    const auto& price=balance.buildings[static_cast<std::size_t>(type)];
    std::vector<BuildingInstance> buildings{
        {.id=CoreId,.type=BuildingType::Core,.gridPosition={0,16},.health=1000000,.maxHealth=1000000},
        {.id=DefenseId,.type=type,.gridPosition={0,0},.level=1,.health=price.maxHealth,.maxHealth=price.maxHealth}};
    const std::vector<BuildingInstance> navigationBuildings{buildings.front()};
    FlowField flow(std::vector<CollisionBox>{});flow.rebuild({0,16},navigationBuildings);
    towers->syncBuildings(buildings);cannons->syncBuildings(buildings);traps->syncBuildings(buildings);
    if(upgrades) {
        std::array<AttackModifiers,GameBalance::BuildingTypeCount> modifiers{};
        modifiers[static_cast<std::size_t>(type)]=buildingStatUpgradeModifiers(type,*upgrades);
        towers->setBuildingUpgradeModifiers(modifiers);
        cannons->setBuildingUpgradeModifiers(modifiers);
        traps->setBuildingUpgradeModifiers(modifiers);
    }
    constexpr int MaximumTtkSeconds=6000;
    const int warmup=finite?0:30, ticks=(finite?MaximumTtkSeconds:seconds)*60;
    Result result;
    std::optional<EntityId> finiteTarget;
    std::set<std::uint64_t> uniqueHits,affected,taunted;
    std::array<double,EnemySystem::MaxEnemies> previous{};
    std::vector<EnemySpawn> spawns;
    int serial=0;
    const double baseSpeed=std::max(.1,balance.enemies[static_cast<std::size_t>(EnemyType::Basic)].speed);
    for(int tick=0;tick<ticks+warmup;++tick) {
        const bool measured=tick>=warmup;
        spawns.clear();
        const int needed=finite ? (serial==0?1:0) : population(scene)-int(enemies->activeCount());
        for(int i=0;i<needed;++i) {
            const int index=serial++;
            spawns.push_back({.type=spawnType(scene,index,seed),.position=spawnPosition(type,scene,index,seed),
                .healthMultiplier=scene<=1?1000000.0:1.0});
        }
        enemies->spawnGroup(spawns);
        if(finite&&!finiteTarget)for(const auto& enemy:enemies->enemies())
            if(enemy.active&&enemy.type==spawnType(scene,0,seed)) {finiteTarget=enemy.id;break;}
        if(measured)result.spawned+=needed;
        // Finite TTK probes are stationary, isolated archetype targets. Their
        // real HP, armor and damage pipeline remain intact, while Boss AI
        // cannot summon adds or leave the lane during this one-target test.
        if(!finite)static_cast<void>(enemies->tick(Step,navigationBuildings,flow));
        int valid=0;
        bool anySlow=false;
        for(const auto& enemy:enemies->enemies()) {
            if(!enemy.active)continue;
            const double dx=enemy.position.x, dz=enemy.position.z;
            const double distance=std::sqrt(dx*dx+dz*dz);
            const double radius=rangeOf(type);
            if(distance<=radius && (!ballistic(type) || distance>=CannonSystem::minimumRange(type,1)) &&
                (scene!=6||airCapable(type)) &&
                (!isDirectionalDefense(type)||directionInsideDefenseArc(type,{0,0,0},enemy.position,0,1)))++valid;
            if(measured) {
                const double slow=std::clamp(1.0-enemy.movementMultiplier,0.0,1.0);
                if(slow>0) {result.slowSeconds+=Step;result.slowWeighted+=slow*Step;
                    affected.insert(key(enemy.id));anySlow=true;}
                // Enemy velocity is the actual post-resistance knockback velocity.
                const double backwards=std::max(0.0,-enemy.knockbackVelocity.z)*Step;
                result.pushDistance+=backwards;
                if(backwards>0)affected.insert(key(enemy.id));
                if(!stationary) {
                    result.exposure+=slow>0 ? Step*slow/std::max(.05,1.0-slow) : 0;
                    result.exposure+=backwards/baseSpeed;
                }
            }
        }
        if(measured) {
            ++result.sampleTicks;
            if(anySlow)++result.slowTicks;
            if(valid) {++result.validTicks;if(result.firstValidTick<0)result.firstValidTick=tick-warmup;}
        }
        std::size_t slot=0;
        for(const auto& enemy:enemies->enemies())previous[slot++]=enemy.active?enemy.health:0;
        enemies->clearDamageSamples();
        const double effectiveBefore=result.effective;
        const auto towerHits=towers->tick(Step,buildings,*enemies);
        const int launches=int(towers->launches().size());
        const auto cannonExplosions=cannons->tick(Step,buildings,*enemies);
        const int cannonLaunches=int(cannons->shots().size());
        const auto activations=traps->tick(Step,buildings,*enemies);
        if(measured) {
            const int attackEvents=int(towers->attacks().size())+cannonLaunches+int(activations.size());
            result.attacks+=attackEvents;
            result.projectiles+=launches+cannonLaunches;
            result.activations+=int(activations.size());
            if(attackEvents>0)++result.attackTicks;
            const bool engaged=(!towers->towers().empty()&&towers->towers().front().targetId.has_value())||
                (!cannons->cannons().empty()&&cannons->cannons().front().targetId.has_value())||
                (!traps->traps().empty()&&traps->traps().front().activationRemaining>0);
            if(engaged)++result.engagedTicks;
            if(valid&&!engaged)result.idleValidSeconds+=Step;
            if(result.firstAttackDelay<0&&attackEvents>0&&result.firstValidTick>=0)
                result.firstAttackDelay=(tick-warmup-result.firstValidTick)*Step;
            int tickHits=0;
            for(const auto& hit:towerHits) {
                if(finite&&hit.targetId!=finiteTarget)continue;
                ++result.hits;++tickHits;uniqueHits.insert(key(hit.targetId));
                if(hit.periodicBurn)result.burn+=hit.damage;
                else {result.direct+=hit.damage;++result.directHits;}
                if(hit.secondaryImpact&&!hit.periodicBurn)result.secondary+=hit.damage;
                if(hit.type==BuildingType::TeslaTower&&!hit.secondaryImpact)
                    result.primary+=hit.damage;
                if(hit.type==BuildingType::LaserTower) {
                    const auto focus=towers->towers().empty()?1.0:
                        laserFocusMultiplier(towers->towers().front().laserFocusSeconds);
                    result.focusExtraEstimate+=hit.damage*(1.0-1.0/focus);
                }
                if(hit.killed)++result.kills;
            }
            for(const auto& hit:cannons->hits()) {
                if(finite&&hit.result.id!=finiteTarget)continue;
                ++result.hits;++result.directHits;++tickHits;uniqueHits.insert(key(hit.result.id));
                result.direct+=hit.result.damage;if(hit.result.killed)++result.kills;
            }
            for(const auto& hit:traps->hits()) {
                if(finite&&hit.result.id!=finiteTarget)continue;
                ++result.hits;++result.directHits;++tickHits;uniqueHits.insert(key(hit.result.id));
                result.direct+=hit.result.damage;if(hit.result.killed)++result.kills;
            }
            result.peakTargets=std::max(result.peakTargets,tickHits);
            for(const auto& explosion:cannonExplosions)
                result.peakTargets=std::max(result.peakTargets,explosion.hitCount);
            for(const auto& activation:activations)result.affected+=activation.affectedCount;
            for(const auto& sample:enemies->damageSamples()) {
                if(finite&&sample.id!=finiteTarget)continue;
                result.raw+=sample.raw;result.effective+=sample.effective;
                result.overkill+=std::max(0.0,sample.raw-sample.effective);
            }
            int activeProjectiles=0;
            for(const auto& p:towers->projectiles())activeProjectiles+=p.active?1:0;
            for(const auto& p:cannons->projectiles())activeProjectiles+=p.active?1:0;
            result.peakProjectiles=std::max(result.peakProjectiles,activeProjectiles);
            if(type==BuildingType::LaserTower&&!towers->towers().empty()&&
                towers->towers().front().laserFocusTarget) {
                const double focus=laserFocusMultiplier(towers->towers().front().laserFocusSeconds);
                result.focusMultiplierSum+=focus;
                ++result.focusTicks;
                result.maxFocusMultiplier=std::max(result.maxFocusMultiplier,focus);
                const double elapsed=(tick-warmup+1)*Step;
                if(focus>=2&&result.focusX2Seconds<0)result.focusX2Seconds=elapsed;
                if(focus>=3&&result.focusX3Seconds<0)result.focusX3Seconds=elapsed;
                if(focus>=4-1e-6&&result.focusX4Seconds<0)result.focusX4Seconds=elapsed;
            }
            double delta=0;slot=0;
            for(const auto& enemy:enemies->enemies()) {
                if(!finite||enemy.id==finiteTarget)
                    delta+=std::max(0.0,previous[slot]-(enemy.active?enemy.health:0));
                ++slot;
            }
            require(std::abs(delta-(result.effective-effectiveBefore))<1e-6*(1+delta),
                "hit-buffer/health accounting mismatch");
        }
        if(type==BuildingType::SlowTrap&&!activations.empty()&&buildings.size()>1) {
            for(const auto& activation:activations)
                buildings.back().health-=activation.wearDamage;
            if(buildings.back().health<=0) {
                buildings.pop_back();
                traps->syncBuildings(buildings);
            }
        }
        if(!stationary)for(const auto& enemy:enemies->enemies()) {
            if(enemy.active && (enemy.position.z>=14.0||enemy.state==EnemyState::AttackCore)) {
                if(measured)++result.leaked;
                enemies->clearDamageSamples();
                static_cast<void>(enemies->damage(enemy.id,enemy.health*100+1));
            }
        }
        if(finite&&finiteTarget&&!enemies->enemy(*finiteTarget)) {
            result.ttkSeconds=(tick+1)*Step;
            if(type==BuildingType::LaserTower&&!towers->towers().empty())
                result.focusAtKill=result.maxFocusMultiplier;
            break;
        }
    }
    result.alive=int(enemies->activeCount());
    result.uniqueHit=int(uniqueHits.size());result.affected=int(affected.size());
    result.dropped=int(towers->droppedProjectiles()+cannons->droppedProjectiles());
    // Warm-up actors may survive to the measurement interval, hence spawned is
    // informational rather than a conservation total.
    return result;
}

void writeHeader(std::ostream& out) {
    out<<"building,scenario,seed,seconds,wood,stone,crystals,hp,can_target,raw_damage_ui,effective_damage_ui,overkill_ui,overkill_pct,dps_ui,kills,attacks,projectiles,hits,hit_events_per_emission,unique_enemies,avg_targets_per_attack,peak_targets,attack_event_tick_fraction,target_engagement_fraction,idle_with_valid_enemy_seconds,first_attack_delay_seconds,slow_enemy_seconds,slow_weighted_seconds,slow_presence_fraction,push_distance,added_exposure_seconds,affected,activations,direct_damage_ui,burn_damage_ui,primary_damage_ui,secondary_damage_ui,focus_extra_ui_estimate,mean_focus_multiplier,peak_projectiles,dropped_projectiles,leaked,ttk_seconds,ttk_complete,focus_max_multiplier,focus_at_kill,focus_x2_seconds,focus_x3_seconds,focus_x4_seconds\n";
}
void writeRow(std::ostream& out,const GameBalance& balance,BuildingType type,int scene,int seed,int seconds,const Result& r) {
    const auto& cost=balance.buildings[static_cast<std::size_t>(type)];
    out<<Names[static_cast<std::size_t>(std::find(Types.begin(),Types.end(),type)-Types.begin())]<<','<<Scenes[static_cast<std::size_t>(scene)]<<','<<seed<<','<<seconds<<','
        <<cost.wood<<','<<cost.stone<<','<<cost.crystals<<','<<cost.maxHealth<<','<<(scene!=6||airCapable(type))<<','
        <<r.raw*CombatDisplayScale<<','<<r.effective*CombatDisplayScale<<','<<r.overkill*CombatDisplayScale<<','
        <<(r.raw>0?r.overkill/r.raw*100:0)<<','<<r.effective*CombatDisplayScale/(scene>=9?(r.ttkSeconds>0?r.ttkSeconds:6000.0):seconds)<<','<<r.kills<<','
        <<r.attacks<<','<<r.projectiles<<','<<r.hits<<','<<(r.projectiles>0?double(r.hits)/r.projectiles:0)<<','
        <<r.uniqueHit<<','<<(r.attacks>0?double(r.directHits)/r.attacks:0)<<','<<r.peakTargets<<','
        <<double(r.attackTicks)/std::max(1,r.sampleTicks)<<','
        <<double(r.engagedTicks)/std::max(1,r.sampleTicks)<<','<<r.idleValidSeconds<<','
        <<r.firstAttackDelay<<','<<r.slowSeconds<<','<<r.slowWeighted<<','
        <<double(r.slowTicks)/std::max(1,r.sampleTicks)<<','
        <<r.pushDistance<<','<<r.exposure<<','<<r.affected<<','<<r.activations<<','
        <<r.direct*CombatDisplayScale<<','<<r.burn*CombatDisplayScale<<','
        <<r.primary*CombatDisplayScale<<','<<r.secondary*CombatDisplayScale<<','
        <<r.focusExtraEstimate*CombatDisplayScale<<','
        <<(r.focusTicks?r.focusMultiplierSum/r.focusTicks:0)<<','
        <<r.peakProjectiles<<','<<r.dropped<<','<<r.leaked<<','<<r.ttkSeconds<<','
        <<(r.ttkSeconds>0)<<','<<r.maxFocusMultiplier<<','<<r.focusAtKill<<','
        <<r.focusX2Seconds<<','<<r.focusX3Seconds<<','<<r.focusX4Seconds<<'\n';
}

void supportTests(const GameBalance& balance,const std::filesystem::path& output,int seconds,int seeds) {
    const auto path=output.parent_path()/"support.csv";
    std::ofstream out(path);out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<std::setprecision(12);
    out<<"building,scenario,seed,seconds,targets,total_hp_restored,hp_per_second_per_target,aggregate_hp_per_second,simultaneous_targets,taunted_unique,mean_simultaneously_taunted,peak_simultaneously_taunted,taunt_enemy_seconds,taunt_uptime,damage_absorbed_ui,survival_seconds\n";
    for(int count:{1,3,5}) {
        BuildingSystem buildings(balance.buildings,balance.economy);
        std::vector<BuildingInstance> setup{{.id=CoreId,.type=BuildingType::Core,
            .gridPosition={0,16},.health=1000000,.maxHealth=1000000},
            {.id=DefenseId,.type=BuildingType::RepairStation,.gridPosition={0,0},.health=200,.maxHealth=200}};
        for(int i=0;i<count;++i)setup.push_back({.id={static_cast<std::uint32_t>(1100+i),1},
            .type=BuildingType::GunTurret,.gridPosition={i-2,2},.health=1000,.maxHealth=10000});
        std::array<std::uint8_t,GameBalance::BuildingTypeCount> levels{};levels.fill(1);
        require(buildings.restoreBuildings(setup,levels,12),"support building setup");
        const std::array sources{RepairStationSource{DefenseId,buildingWorldPosition(setup[1]),1}};
        double restored=0;int simultaneous=0;
        for(int pulse=0;pulse<int(seconds/RepairStationInterval);++pulse) {
            int served=0;
            for(int i=0;i<count;++i) {
                const auto& target=setup[static_cast<std::size_t>(i+2)];
                const auto dose=repairStationDose(sources,buildingWorldPosition(target));
                if(dose.source) {
                    const double applied=buildings.restoreHealth(target.id,dose.healing);
                    restored+=applied;served+=applied>0;
                }
            }
            simultaneous=std::max(simultaneous,served);
        }
        out<<"repair_station,"<<count<<"_targets,1,"<<seconds<<','<<count<<','<<restored<<','
            <<restored/seconds/count<<','<<restored/seconds<<','<<simultaneous<<",0,0,0,0,0,0,0\n";
    }
    for(int seed=1;seed<=seeds;++seed) {
        auto enemies=std::make_unique<EnemySystem>(balance.enemies);
        std::vector<BuildingInstance> buildings{{.id=CoreId,.type=BuildingType::Core,
            .gridPosition={0,16},.health=1000000,.maxHealth=1000000},
            {.id=DefenseId,.type=BuildingType::DecoyBeacon,.gridPosition={0,0},.health=balance.buildings[static_cast<std::size_t>(BuildingType::DecoyBeacon)].maxHealth,
             .maxHealth=balance.buildings[static_cast<std::size_t>(BuildingType::DecoyBeacon)].maxHealth}};
        FlowField flow(std::vector<CollisionBox>{});flow.rebuild({0,16},buildings);
        CollisionWorld collision(48,{});collision.syncBuildings(buildings);
        // Charge the beacon without attackers; the measured interval begins
        // immediately before its first normal eight-second pulse.
        static_cast<void>(enemies->tick(DecoyBeaconInterval-Step,buildings,flow,{}, {},nullptr,
            {{},{},1.0,&collision}));
        std::set<std::uint64_t> unique;
        double secondsTaunted=0,absorbed=0,survival=seconds;
        int peak=0;double simultaneous=0,uptime=0,serial=0;
        for(int tick=0;tick<seconds*60;++tick) {
            std::vector<EnemySpawn> spawns;
            const int needed=16-int(enemies->activeCount());
            for(int i=0;i<needed;++i) {
                const int index=int(serial++);
                spawns.push_back({.type=index%4==0?EnemyType::Fast:EnemyType::Basic,
                    .position={(double((index*7+seed)%7)-3)*.7,0,-4.0-double(index%3)*.25}});
            }
            enemies->spawnGroup(spawns);
            const auto attacks=enemies->tick(Step,buildings,flow,{}, {},nullptr,
                {{},{},1.0,&collision});
            int now=0;
            for(const auto& enemy:enemies->enemies())if(enemy.active&&enemy.decoyTarget==DefenseId) {
                ++now;unique.insert(key(enemy.id));
            }
            secondsTaunted+=now*Step;simultaneous+=now*Step;
            peak=std::max(peak,now);if(now>0)uptime+=Step;
            for(const auto& attack:attacks)if(attack.targetId==DefenseId&&buildings[1].health>0) {
                const double applied=std::min(buildings[1].health,attack.damage);
                buildings[1].health-=applied;absorbed+=applied;
                if(buildings[1].health<=0)survival=(tick+1)*Step;
            }
            if(buildings[1].health<=0) {
                // Continue the paired clock for survival, but the decoy can no longer taunt.
                break;
            }
            for(const auto& enemy:enemies->enemies())if(enemy.active&&enemy.position.z>=14.0)
                static_cast<void>(enemies->damage(enemy.id,enemy.health*100+1));
        }
        out<<"decoy,flow,"<<seed<<','<<seconds<<",0,0,0,0,0,"<<unique.size()<<','
            <<simultaneous/std::max(survival,Step)<<','<<peak<<','<<secondsTaunted<<','
            <<uptime/std::max(survival,Step)<<','<<absorbed*CombatDisplayScale<<','<<survival<<'\n';
    }
}

struct UpgradeCase {
    BuildingType type;
    std::string name;
    std::vector<AttackStat> stats;
};

std::vector<UpgradeCase> upgradeCases() {
    using S=AttackStat;
    std::vector<UpgradeCase> cases;
    const auto add=[&](BuildingType type,const char* name,std::initializer_list<S> stats) {
        cases.push_back({type,name,stats});
    };
    for(const auto type:Types) {
        add(type,"base",{});
        switch(type) {
        case BuildingType::Turret:
            add(type,"mid",{S::Quantity,S::Pierce});
            add(type,"high",{S::Quantity,S::Pierce,S::CritChance,S::CritDamage});
            add(type,"count",{S::Quantity});add(type,"pierce",{S::Pierce});
            add(type,"crit",{S::CritChance,S::CritDamage});
            add(type,"count_pierce",{S::Quantity,S::Pierce});
            add(type,"count_pierce_crit",{S::Quantity,S::Pierce,S::CritChance,S::CritDamage});
            break;
        case BuildingType::GunTurret:
            add(type,"mid",{S::Damage,S::AttackSpeed});
            add(type,"high",{S::Damage,S::AttackSpeed,S::Quantity,S::Pierce});break;
        case BuildingType::Cannon: case BuildingType::Catapult: case BuildingType::Mortar:
            add(type,"mid",{S::Quantity,S::Size});
            if(type==BuildingType::Cannon)
                add(type,"high",{S::Quantity,S::Size,S::Damage,S::Knockback});
            else if(type==BuildingType::Catapult)
                add(type,"high",{S::Quantity,S::Size,S::Damage,S::Duration});
            else add(type,"high",{S::Quantity,S::Size,S::Damage,S::CritDamage});
            add(type,"count",{S::Quantity});add(type,"size",{S::Size});
            add(type,"count_size",{S::Quantity,S::Size});break;
        case BuildingType::Flamethrower:
            add(type,"mid",{S::Size,S::Duration});
            add(type,"high",{S::Size,S::Duration,S::Damage,S::Range});
            add(type,"size",{S::Size});add(type,"duration",{S::Duration});
            add(type,"size_duration",{S::Size,S::Duration});break;
        case BuildingType::MissileLauncher:
            add(type,"mid",{S::Quantity,S::Size});
            add(type,"high",{S::Quantity,S::Size,S::Damage,S::CritChance});
            add(type,"count",{S::Quantity});add(type,"size",{S::Size});
            add(type,"count_size",{S::Quantity,S::Size});break;
        case BuildingType::TeslaTower:
            add(type,"mid",{S::Bounces,S::Quantity});
            add(type,"high",{S::Bounces,S::Quantity,S::Damage,S::Range});
            add(type,"bounces",{S::Bounces});add(type,"attack_count",{S::Quantity});
            add(type,"bounces_attack_count",{S::Bounces,S::Quantity});break;
        case BuildingType::LaserTower:
            add(type,"mid",{S::Duration,S::Pierce});
            add(type,"high",{S::Duration,S::Pierce,S::CritChance,S::CritDamage});
            add(type,"duration",{S::Duration});add(type,"pierce",{S::Pierce});
            add(type,"crit",{S::CritChance,S::CritDamage});
            add(type,"duration_pierce",{S::Duration,S::Pierce});
            add(type,"duration_pierce_crit",{S::Duration,S::Pierce,S::CritChance,S::CritDamage});break;
        case BuildingType::WindBlower:
            add(type,"mid",{S::Size,S::Knockback});
            add(type,"high",{S::Size,S::Knockback,S::Duration,S::Range});break;
        case BuildingType::SpikeTrap:
            add(type,"mid",{S::Damage,S::Size});
            add(type,"high",{S::Damage,S::Size,S::CritChance,S::CritDamage});break;
        case BuildingType::SlowTrap:
            add(type,"mid",{S::Size,S::Duration});
            add(type,"high",{S::Size,S::Duration,S::SlowStrength,S::Range});break;
        default:break;
        }
    }
    return cases;
}

BuildingUpgradePowers powersFor(const UpgradeCase& selected) {
    BuildingUpgradePowers powers{};
    for(const auto stat:selected.stats) {
        require(std::ranges::find(buildingStatUpgradePool(selected.type),stat)!=
            buildingStatUpgradePool(selected.type).end(),"upgrade profile uses ineligible stat");
        BuildingStatRoll roll;roll.stats[0]=stat;roll.count=1;
        applyBuildingStatUpgrade(powers,selected.type,roll,RunUpgradeRarity::Common);
    }
    return powers;
}

FinalAttackStats finalStatsFor(const GameBalance& balance,const UpgradeCase& selected,
                              const BuildingUpgradePowers& powers) {
    const auto& price=balance.buildings[static_cast<std::size_t>(selected.type)];
    BuildingInstance building{.id=DefenseId,.type=selected.type,.gridPosition={0,0},.level=1,
                              .health=price.maxHealth,.maxHealth=price.maxHealth};
    std::array<AttackModifiers,GameBalance::BuildingTypeCount> modifiers{};
    modifiers[static_cast<std::size_t>(selected.type)]=buildingStatUpgradeModifiers(selected.type,powers);
    if(ballistic(selected.type)) {
        CannonSystem system;system.setBuildingUpgradeModifiers(modifiers);return system.finalStats(building);
    }
    if(trap(selected.type)) {
        TrapSystem system;system.setBuildingUpgradeModifiers(modifiers);return system.finalStats(building);
    }
    TowerSystem system;system.setBuildingUpgradeModifiers(modifiers);return system.finalStats(building);
}

void runUpgradeProfiles(const GameBalance& balance,const std::filesystem::path& output,
                        int seconds,int seeds) {
    std::ofstream out(output);out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<std::setprecision(12);
    out<<"building,profile,scenario,seed,seconds,stat_count,quantity,pierce,bounces,crit_chance,crit_damage,size,duration,effective_damage_ui,dps_ui,kills,attacks,hits,secondary_damage_ui,control_exposure_seconds\n";
    for(const auto& selected:upgradeCases()) {
        const auto powers=powersFor(selected);
        const auto stats=finalStatsFor(balance,selected,powers);
        const auto index=std::find(Types.begin(),Types.end(),selected.type)-Types.begin();
        for(const int scene:{0,2,3,7})for(int seed=1;seed<=seeds;++seed) {
            const auto result=runCombat(balance,selected.type,scene,seed,seconds,&powers);
            out<<Names[static_cast<std::size_t>(index)]<<','<<selected.name<<','
               <<Scenes[static_cast<std::size_t>(scene)]<<','<<seed<<','<<seconds<<','
               <<selected.stats.size()<<','<<stats.quantity<<','<<stats.pierce<<','<<stats.bounces<<','
               <<stats.critChance<<','<<stats.critDamage<<','<<stats.size<<','<<stats.duration<<','
               <<result.effective*CombatDisplayScale<<','
               <<result.effective*CombatDisplayScale/seconds<<','<<result.kills<<','<<result.attacks<<','
               <<result.hits<<','<<result.secondary*CombatDisplayScale<<','<<result.exposure<<'\n';
        }
    }
}

int probeCrossbow(const GameBalance& balance,Vec3 target) {
    auto definitions=balance.enemies;
    definitions[static_cast<std::size_t>(EnemyType::Basic)].speed=0;
    EnemySystem enemies(definitions);
    const std::vector<BuildingInstance> buildings{
        {.id=CoreId,.type=BuildingType::Core,.gridPosition={0,16},.health=1000,.maxHealth=1000},
        {.id=DefenseId,.type=BuildingType::Turret,.gridPosition={0,0},.level=1,.health=120,.maxHealth=120}};
    FlowField flow(std::vector<CollisionBox>{});flow.rebuild({0,16},buildings);
    TowerSystem tower;tower.syncBuildings(buildings);
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position=target,.healthMultiplier=100000}});
    int attacks=0;
    for(int tick=0;tick<300;++tick) {
        static_cast<void>(enemies.tick(Step,buildings,flow));
        static_cast<void>(tower.tick(Step,buildings,enemies));
        attacks+=int(tower.attacks().size());
    }
    return attacks;
}

void probeMortarDeadZone(const GameBalance& balance) {
    auto definitions=balance.enemies;
    definitions[static_cast<std::size_t>(EnemyType::Basic)].speed=0;
    EnemySystem enemies(definitions);
    const auto hp=balance.buildings[static_cast<std::size_t>(BuildingType::Mortar)].maxHealth;
    const std::vector<BuildingInstance> buildings{
        {.id=CoreId,.type=BuildingType::Core,.gridPosition={0,16},.health=1000,.maxHealth=1000},
        {.id=DefenseId,.type=BuildingType::Mortar,.gridPosition={0,0},.level=1,.health=hp,.maxHealth=hp}};
    FlowField flow(std::vector<CollisionBox>{});flow.rebuild({0,16},buildings);
    CannonSystem cannon;cannon.syncBuildings(buildings);
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={0,0,-4.0},
                                             .healthMultiplier=100000}});
    const EntityId near=enemies.enemies().front().id;
    for(int tick=0;tick<600;++tick) {
        static_cast<void>(cannon.tick(Step,buildings,enemies));
        require(cannon.shots().empty(),"mortar fired inside minimum range");
    }
    enemies.applyKnockback(near,{0,0,-1},8.0);
    bool fired=false;
    for(int tick=0;tick<600;++tick) {
        static_cast<void>(enemies.tick(Step,buildings,flow));
        static_cast<void>(cannon.tick(Step,buildings,enemies));
        fired|=!cannon.shots().empty();
    }
    require(fired,"mortar did not fire after the same target passed minimum range");
}

void probeLaserFocusSwitch(const GameBalance& balance) {
    auto definitions=balance.enemies;
    definitions[static_cast<std::size_t>(EnemyType::Basic)].speed=0;
    EnemySystem enemies(definitions);
    const auto hp=balance.buildings[static_cast<std::size_t>(BuildingType::LaserTower)].maxHealth;
    const std::vector<BuildingInstance> buildings{
        {.id=CoreId,.type=BuildingType::Core,.gridPosition={0,16},.health=1000,.maxHealth=1000},
        {.id=DefenseId,.type=BuildingType::LaserTower,.gridPosition={0,0},.level=1,.health=hp,.maxHealth=hp}};
    TowerSystem tower;tower.syncBuildings(buildings);
    enemies.spawnGroup(std::array{EnemySpawn{.type=EnemyType::Basic,.position={0,0,-6.0},
                                             .healthMultiplier=100000},
                                  EnemySpawn{.type=EnemyType::Basic,.position={1,0,-7.0},
                                             .healthMultiplier=100000}});
    for(int tick=0;tick<300;++tick)static_cast<void>(tower.tick(Step,buildings,enemies));
    const auto focused=tower.towers().front();
    require(focused.laserFocusTarget.has_value()&&focused.laserFocusSeconds>1,
            "laser did not acquire focus");
    static_cast<void>(enemies.damage(*focused.laserFocusTarget,1000000));
    for(int tick=0;tick<60;++tick)static_cast<void>(tower.tick(Step,buildings,enemies));
    const auto switched=tower.towers().front();
    require(switched.laserFocusTarget&&switched.laserFocusTarget!=focused.laserFocusTarget&&
            switched.laserFocusSeconds<focused.laserFocusSeconds,
            "laser focus did not reset on target switch");
}
}

int main(int argc,char** argv) {
    try {
        std::filesystem::path data="assets/data",output="output/defense_balance/raw.csv",upgradesOutput;
        int seconds=60,seeds=3;bool test=false;
        for(int i=1;i<argc;++i) {
            const std::string_view arg=argv[i];
            if(arg=="--self-test"){test=true;continue;}
            if(arg=="--help") {std::cout<<"ian_defense_balance [--output CSV] [--upgrades-output CSV] [--data DIR] [--seconds 1..120] [--seeds 1..8] [--self-test]\n";return 0;}
            if(++i>=argc)throw std::runtime_error("missing option value");
            const std::string_view value=argv[i];
            if(arg=="--output")output=value;
            else if(arg=="--upgrades-output")upgradesOutput=value;
            else if(arg=="--data")data=value;
            else if(arg=="--seconds")seconds=number(value,1,120);
            else if(arg=="--seeds")seeds=number(value,1,8);
            else throw std::runtime_error("unknown option");
        }
        const auto path=[&](const char* name){return(data/name).string();};
        const auto loaded=loadGameBalance(path("enemies.json"),path("waves.json"),path("buildings.json"),
            path("weapons.json"),path("economy.json"),path("gameplay.json"));
        if(!loaded.valid()){for(const auto& error:loaded.errors)std::cerr<<error<<'\n';return 1;}
        if(test) {
            const auto a=runCombat(loaded.balance,BuildingType::GunTurret,0,1,3);
            const auto b=runCombat(loaded.balance,BuildingType::GunTurret,0,1,3);
            require(a.effective>0&&a.effective==b.effective&&a.hits==b.hits,"deterministic real combat");
            for(const auto type:{BuildingType::Cannon,BuildingType::Catapult,BuildingType::Mortar}) {
                const auto air=runCombat(loaded.balance,type,6,1,3);
                require(air.hits==0&&air.attacks==0,"ground artillery cannot target flying");
            }
            const auto gunAir=runCombat(loaded.balance,BuildingType::GunTurret,6,1,3);
            require(gunAir.hits==0&&gunAir.attacks==0,"gun turret air restriction");
            for(const auto type:{BuildingType::Turret,BuildingType::MissileLauncher,
                    BuildingType::TeslaTower,BuildingType::LaserTower}) {
                const auto air=runCombat(loaded.balance,type,6,1,5);
                require(air.hits>0,"anti-air building does not hit flying target");
            }
            require(probeCrossbow(loaded.balance,{0,0,-5})>0,"front target fires");
            require(probeCrossbow(loaded.balance,{0,0,15})==0,"range limit");
            require(probeCrossbow(loaded.balance,{0,0,5})==0,"firing arc");
            probeMortarDeadZone(loaded.balance);
            probeLaserFocusSwitch(loaded.balance);
            const auto normal=runCombat(loaded.balance,BuildingType::GunTurret,9,1,3);
            require(normal.ttkSeconds>0&&normal.kills==1&&
                std::abs(normal.effective*CombatDisplayScale-50)<1e-6,"normal TTK accounting");
            UpgradeCase count{BuildingType::Turret,"count",{AttackStat::Quantity}};
            const auto countPowers=powersFor(count);
            const auto baseStats=finalStatsFor(loaded.balance,{BuildingType::Turret,"base",{}},{});
            const auto countStats=finalStatsFor(loaded.balance,count,countPowers);
            require(std::abs(countStats.quantity-(baseStats.quantity+1))<1e-9,
                "upgrade profile did not apply exactly one count card");
            auto half=countPowers;
            half[static_cast<std::size_t>(BuildingType::Turret)]
                [static_cast<std::size_t>(AttackStat::Quantity)]=0.5;
            const auto fractional=finalStatsFor(loaded.balance,count,half);
            require(std::abs(fractional.quantity-(baseStats.quantity+0.5))<1e-9&&
                effectiveAttackCount(fractional.quantity,1)==effectiveAttackCount(baseStats.quantity,1),
                "fractional count should be stored, then floored by real consumer");
            UpgradeCase teslaCount{BuildingType::TeslaTower,"attack_count",{AttackStat::Quantity}};
            const auto teslaBase=finalStatsFor(loaded.balance,{BuildingType::TeslaTower,"base",{}},{});
            auto teslaHalf=powersFor(teslaCount);
            teslaHalf[static_cast<std::size_t>(BuildingType::TeslaTower)]
                [static_cast<std::size_t>(AttackStat::Quantity)]=0.5;
            const auto teslaFractional=finalStatsFor(loaded.balance,teslaCount,teslaHalf);
            require(std::abs(teslaFractional.quantity-(teslaBase.quantity+0.5))<1e-9&&
                effectiveAttackCount(teslaFractional.quantity,1)==effectiveAttackCount(teslaBase.quantity,1),
                "fractional Tesla attack count differs from production flooring");
            const auto boss=runCombat(loaded.balance,BuildingType::LaserTower,12,1,3);
            require(boss.ttkSeconds>0&&boss.kills==1&&boss.maxFocusMultiplier>3.9&&
                std::abs(boss.effective*CombatDisplayScale-24000)<20,
                "isolated base boss TTK/focus accounting");
            const auto pierce=runCombat(loaded.balance,BuildingType::Turret,2,1,5);
            require(pierce.hits>pierce.attacks,"crossbow pierce hits multiple enemies");
            const auto bounce=runCombat(loaded.balance,BuildingType::TeslaTower,3,1,5);
            require(bounce.primary>0&&bounce.secondary>0&&bounce.hits>bounce.attacks,
                "tesla bounce accounting");
            const auto explosion=runCombat(loaded.balance,BuildingType::Mortar,3,1,8);
            require(explosion.peakTargets>1,"explosion hit count");
            const auto spike=runCombat(loaded.balance,BuildingType::SpikeTrap,7,1,5);
            require(spike.raw>spike.effective&&spike.overkill>0,"overkill accounting");
            const auto control=runCombat(loaded.balance,BuildingType::SlowTrap,8,1,5);
            require(control.effective==0&&control.slowSeconds>0&&control.exposure>0,
                "control is independent of damage");
            require(control.slowWeighted<=control.slowSeconds+1e-9&&control.affected>0,
                "slow seconds count each affected enemy at most once per tick");
            const auto wind=runCombat(loaded.balance,BuildingType::WindBlower,8,1,5);
            require(wind.effective==0&&wind.pushDistance>0&&wind.slowSeconds==0,
                "wind knockback does not double-count slow");
            const auto source=RepairStationSource{DefenseId,{0,0,0},1};
            require(repairStationDose(std::array{source},{1,0,0}).healing==4,
                "repair station real pulse value");
            require(repairStationDose(std::array{source,source},{1,0,0}).healing==4,
                "overlapping repair stations do not stack");
            std::cout<<"Defense balance checks passed\n";return 0;
        }
        if(!output.parent_path().empty())std::filesystem::create_directories(output.parent_path());
        std::ofstream out(output);out.exceptions(std::ios::badbit|std::ios::failbit);
        out<<std::setprecision(12);writeHeader(out);
        for(std::size_t scene=0;scene<Scenes.size();++scene) {
            for(std::size_t type=0;type<Types.size();++type)for(int seed=1;seed<=seeds;++seed)
                writeRow(out,loaded.balance,Types[type],int(scene),seed,seconds,
                    runCombat(loaded.balance,Types[type],int(scene),seed,seconds));
            std::cerr<<Scenes[scene]<<" complete\n";
        }
        supportTests(loaded.balance,output,seconds,seeds);
        if(!upgradesOutput.empty()) {
            if(!upgradesOutput.parent_path().empty())
                std::filesystem::create_directories(upgradesOutput.parent_path());
            runUpgradeProfiles(loaded.balance,upgradesOutput,seconds,seeds);
        }
        return 0;
    }catch(const std::exception& error){std::cerr<<"Defense balance: "<<error.what()<<'\n';return 1;}
}
