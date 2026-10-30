#include "TestHarness.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "combat/AttackProjectile.hpp"
#include "combat/AttackStatsDebug.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "progression/DefenseAttackModifiers.hpp"
#include "game/Simulation.hpp"
#include "navigation/FlowField.hpp"
#include <array>
#include <memory>

using namespace ian;
namespace {
constexpr double Step = 1.0 / 120.0;
FinalAttackStats projectileStats() {
    AttackStats base;
    base.damage = 0.1; base.projectileSpeed = 10.0; base.duration = 4.0;
    base.spreadDegrees = 0.0;
    return resolveAttackStats(base);
}
std::vector<ProjectileImpact> fly(AttackProjectileSystem& system, EnemySystem& enemies, double seconds) {
    std::vector<ProjectileImpact> hits;
    for (int frame = 0; frame < static_cast<int>(seconds / Step); ++frame) {
        const auto stepHits = system.tick(Step, enemies);
        hits.insert(hits.end(), stepHits.begin(), stepHits.end());
    }
    return hits;
}
void spawn(EnemySystem& enemies, std::initializer_list<Vec3> positions) {
    std::vector<EnemySpawn> spawns;
    for (const auto position : positions) spawns.push_back({.type = EnemyType::Basic, .position = position});
    enemies.spawnWave(spawns);
}
void statsAndPatterns() {
    {
        AttackStats base; base.quantity = 1; base.bounces = base.pierce = 0;
        AttackModifiers first, second;
        for (const auto stat : {AttackStat::Quantity, AttackStat::Bounces, AttackStat::Pierce}) {
            first[stat].flat = 1.6; second[stat].flat = 1.4;
        }
        const auto initial = resolveAttackStats(base, std::array{first});
        requireNear(initial.quantity, 2.6, 1e-12, "final quantity keeps its fractional remainder");
        requireNear(initial.bounces, 1.6, 1e-12, "final bounce stat keeps its fractional remainder");
        require(effectiveAttackCount(initial.quantity,1)==2 && effectiveAttackCount(initial.bounces)==1 &&
            effectiveAttackCount(initial.pierce)==1, "consumers use floor for every count stat");
        const auto total = resolveAttackStats(base, std::array{first,second});
        requireNear(total.quantity,4,1e-12,"fractional quantities sum across independent sources");
        require(effectiveAttackCount(total.bounces)==3 && effectiveAttackCount(total.pierce)==3,
            "1.6 plus 1.4 retains exactly three additional interactions");
        require(effectiveAttackCount(1.999999)==1 && effectiveAttackCount(.999)==0,
            "count conversion never rounds or turns a remainder into probability");
        EnemySystem enemies; spawn(enemies, {{0,0,-5}});
        AttackProjectileSystem projectiles;
        require(projectiles.spawn({100,1},BuildingType::GunTurret,{}, {0,0,-1},initial,0),
            "fractional-count projectile can spawn");
        const auto& runtime=projectiles.projectiles().front().runtime;
        require(runtime.remainingBounces==1 && runtime.remainingPierce==1,
            "actual projectile counters floor accumulated stats");
        auto gun = resolveAttackStats(TowerSystem::baseStats(BuildingType::GunTurret,1),std::array{first});
        const auto timing=gunBurstTiming(gun);
        requireNear(timing.duration,timing.interval*3,1e-12,"fractional quantity produces only whole burst gaps");
    }
    AttackStats base;
    base.damage = 10; base.attackInterval = 1; base.projectileSpeed = 20;
    AttackModifiers global, local;
    global[AttackStat::Damage] = {2, 1.5}; local[AttackStat::Damage] = {3, 1.2};
    global[AttackStat::AttackSpeed].multiplier = 2;
    global[AttackStat::Quantity].flat = 3;
    global[AttackStat::ProjectileSpeed] = {4, 1.5};
    global[AttackStat::Duration].multiplier = 2;
    global[AttackStat::Size].multiplier = 1.2;
    const auto stats = resolveAttackStats(base, std::array{global, local});
    requireNear(stats.damage, 23, 1e-12, "base damage times independent factors plus flats");
    requireNear(stats.attackInterval, 0.5, 1e-12, "100% speed halves event interval");
    require(stats.quantity == 4, "quantity is independent of event interval");
    requireNear(stats.projectileSpeed, 34, 1e-12, "speed central formula includes flat bonus");
    requireNear(stats.duration, 6, 1e-12, "duration doubles lifetime");
    requireNear(stats.size, 1.2, 1e-12, "size multiplier resolves");
    require(base.damage == 10 && base.quantity == 1, "resolution never mutates base stats");
    require(fanAngleDegrees(0, 5, 5) == -10 && fanAngleDegrees(2, 5, 5) == 0 &&
        fanAngleDegrees(4, 5, 5) == 10 && fanAngleDegrees(0, 1, 5) == 0,
        "universal fan distributes around forward direction");
    global[AttackStat::AttackSpeed].multiplier = 1e12;
    global[AttackStat::Quantity].flat = -50;
    global[AttackStat::ProjectileSpeed].flat = -100;
    global[AttackStat::Duration].multiplier = -1;
    global[AttackStat::Size].multiplier = 0;
    global[AttackStat::CritChance].flat = 3;
    global[AttackStat::Pierce].flat = -4;
    global[AttackStat::Bounces].flat = -4;
    const auto safe = resolveAttackStats(base, std::array{global});
    require(safe.attackInterval == MinimumAttackCooldown && safe.quantity == 1 && safe.projectileSpeed == 0 &&
        safe.duration > 0 && safe.size > 0 && safe.critChance == 1 && safe.pierce == 0 && safe.bounces == 0,
        "unsafe values clamp without small count caps");
    global[AttackStat::Quantity].flat = 50;
    require(resolveAttackStats(base, std::array{global}).quantity == 51, "late builds can exceed five instances");
    require(describeAttackStats(stats).find("Projectile Count: 4") != std::string::npos, "debug stats contain resolved quantity");
}
void collisionAndCounters() {
    for (const int bounces : {0, 1, 3}) {
        EnemySystem enemies;
        spawn(enemies, {{0,0.8,-3}, {2.5,0.8,-3}, {5,0.8,-3}, {7.5,0.8,-3}});
        AttackProjectileSystem system;
        auto stats = projectileStats(); stats.bounces = bounces;
        system.spawn({1,1}, BuildingType::GunTurret, {0,0.8,0}, {0,0,-1}, stats, 0);
        require(system.tick(0.01, enemies).empty(), "launch does not instantly hit a distant enemy");
        const auto hits = fly(system, enemies, 3);
        require(hits.size() == static_cast<std::size_t>(bounces + 1), "bounces count additional targets");
        for (std::size_t i=0; i<hits.size(); ++i) for (std::size_t j=i+1; j<hits.size(); ++j)
            require(hits[i].hit.result.id != hits[j].hit.result.id, "bounce never revisits a hit target");
    }
    {
        EnemySystem enemies; spawn(enemies, {{0,0.8,-3}, {2.5,0.8,-3}, {5,0.8,-3}});
        AttackProjectileSystem system;
        auto stats = projectileStats(); stats.bounces = 2;
        for (int instance=0; instance<5; ++instance) system.spawn({1,1}, BuildingType::GunTurret,
            {0,0.8,0}, {0,0,-1}, stats, 0);
        require(fly(system, enemies, 3).size() == 15, "five projectiles each have three independent hits");
    }
    {
        EnemySystem enemies; spawn(enemies, {{0,0.8,-3}, {0,0.8,-6}, {0,0.8,-9}, {3,0.8,-9}});
        AttackProjectileSystem system;
        auto stats = projectileStats(); stats.pierce=2; stats.bounces=1;
        system.spawn({1,1}, BuildingType::Turret, {0,0.8,0}, {0,0,-1}, stats, 0);
        const auto hits = fly(system, enemies, 3);
        require(hits.size() == 4 && !hits[0].secondary && hits[1].secondary,
            "pierce spends first, then bounce continues, one hit per enemy");
    }
    {
        EnemySystem enemies; spawn(enemies, {{0,0.8,-3}, {0,0.8,-6}});
        AttackProjectileSystem system;
        auto stats = projectileStats(); stats.projectileSpeed=1000; stats.pierce=1;
        system.spawn({1,1}, BuildingType::GunTurret, {0,0.8,0}, {0,0,-1}, stats, 0);
        require(system.tick(0.02, enemies).size()==2, "swept collision catches several targets in a fast physics step");
    }
    // Size expands both visual size and collision radius. Another enemy can
    // catch a shot which missed its originally selected direction/target.
    for (const double size : {1.0, 20.0}) {
        EnemySystem enemies; spawn(enemies, {{1.8,0.8,-3}});
        AttackProjectileSystem system;
        auto stats = projectileStats(); stats.size = size;
        system.spawn({1,1}, BuildingType::GunTurret, {0,0.8,0}, {0,0,-1}, stats, 0);
        require(fly(system, enemies, 1).size() == (size > 1 ? 1U : 0U), "large collider turns a physical miss into contact");
    }
    {
        EnemySystem enemies; spawn(enemies, {{0,0.8,-3}});
        AttackProjectileSystem system;
        auto stats = projectileStats(); stats.critChance=1; stats.critDamage=2; stats.knockback=3;
        system.spawn({1,1}, BuildingType::GunTurret, {0,0.8,0}, {0,0,-1}, stats, 0);
        stats.damage=100; // Launched configuration must be a snapshot.
        const auto hits=fly(system,enemies,1);
        require(hits.size()==1 && hits.front().hit.critical, "critical roll happens in the common hit flow");
        requireNear(hits.front().hit.result.damage,0.2,1e-12,"snapshot damage survives later stat changes");
        require(enemies.enemy(hits.front().hit.result.id)->knockbackVelocity.z < -2, "hit passes direction and strength to enemy");
    }
}
void poolingAndLifetime() {
    EnemySystem enemies;
    AttackProjectileSystem system;
    {
        auto rangeStats=projectileStats();
        rangeStats.range=9.0;
        system.spawn({1,1},BuildingType::Turret,{0,1,0},{0,0,-1},rangeStats,0);
        system.tick(0.9,enemies);
        require(system.projectiles().front().active,
            "straight projectile remains active inside useful attack range");
        system.tick(0.3,enemies);
        require(!system.projectiles().front().active,
            "straight projectile expires near attack range, before duration ends");
    }
    auto stats=projectileStats(); stats.duration=0.05; stats.size=5; stats.bounces=3; stats.pierce=4;
    system.spawn({1,1},BuildingType::GunTurret,{0,1,0},{0,0,-1},stats,0);
    const auto id=system.projectiles().front().id;
    system.tick(0.02,enemies);
    require(system.projectiles().front().active,"projectile stays alive before lifetime");
    system.tick(0.1,enemies);
    require(!system.projectiles().front().active,"projectile expires after lifetime");
    stats=projectileStats();
    system.spawn({2,1},BuildingType::Turret,{2,2,2},{1,0,0},stats,0);
    const auto& reused=system.projectiles().front();
    require(system.projectiles().size()==1 && reused.id.index==id.index && reused.id.generation==id.generation+1,
        "pool reuses a slot with fresh generation");
    require(reused.runtime.remainingBounces==0 && reused.runtime.remainingPierce==0 && reused.runtime.elapsedLifetime==0 &&
        reused.runtime.travelledDistance==0 && reused.runtime.previousTravelledDistance==0 &&
        reused.runtime.alreadyHitTargets.empty() && !reused.runtime.currentTarget && reused.configuration.stats.size==1 &&
        reused.configuration.stats.critChance==0 && reused.sourceId==EntityId{2,1} && reused.runtime.velocity.x==10,
        "pool resets counters, history, velocity, source, crit, lifetime and scale");
    for(int cycle=0;cycle<1000;++cycle) {
        system.reset();system.spawn({2,1},BuildingType::Turret,{},{0,0,-1},stats,0);
    }
    require(system.projectiles().size()==1,"steady state pooling does not grow per shot");
}
void weaponCombinations() {
    const std::vector<BuildingInstance> buildings{{.id={8,1},.type=BuildingType::GunTurret,.gridPosition={0,0}}};
    for (const int quantity : {3,5,10,30}) for(const double speed : {1.0,2.0,100.0}) for(const int bounces : {0,1,3}) {
        EnemySystem enemies;spawn(enemies,{{0,0.8,-6},{2.5,0.8,-6},{5,0.8,-6}});
        TowerSystem tower;
        AttackModifiers modifiers;
        modifiers[AttackStat::Quantity].flat=quantity-3;
        modifiers[AttackStat::AttackSpeed].multiplier=speed;
        modifiers[AttackStat::Bounces].flat=bounces;
        modifiers[AttackStat::Damage].multiplier=0.01;
        modifiers[AttackStat::Spread].multiplier=0;
        tower.setAttackModifiers(modifiers);tower.syncBuildings(buildings);
        double sinceEvent=0;
        bool started=false;int launches=0;
        for(int frame=0;frame<480;++frame) {
            tower.tick(Step,buildings,enemies);
            if(!tower.attacks().empty()) {
                if(started) {
                    require(sinceEvent+2*Step+1e-12>=std::max(tower.finalStats(buildings[0]).attackInterval,
                        gunBurstTiming(tower.finalStats(buildings[0])).duration),"long burst never overlaps next event");
                    launches += static_cast<int>(tower.launches().size()) - 1;
                    break;
                }
                started=true;sinceEvent=0;
            } else if(started) sinceEvent+=Step;
            if(started) launches+=static_cast<int>(tower.launches().size());
        }
        require(launches==quantity,"quantity emits exact burst length independent of speed and bounces");
    }
    for(const auto type:{BuildingType::Turret,BuildingType::Cannon,BuildingType::Catapult}) {
        const std::vector<BuildingInstance> registry{{.id={9,1},.type=type}};
        EnemySystem enemies;spawn(enemies,{{0,0.8,-8}});
        AttackModifiers modifiers;modifiers[AttackStat::Quantity].flat=2;
        if(type==BuildingType::Turret) {
            TowerSystem tower;tower.setAttackModifiers(modifiers);tower.syncBuildings(registry);
            for(int frame=0;frame<240 && tower.attacks().empty();++frame)tower.tick(Step,registry,enemies);
            require(tower.projectiles().size()==3,"crossbow interprets quantity as a simultaneous fan");
        } else {
            CannonSystem cannon;cannon.setAttackModifiers(modifiers);cannon.syncBuildings(registry);
            for(int frame=0;frame<240 && cannon.shots().empty();++frame)cannon.tick(Step,registry,enemies);
            require(cannon.projectiles().size()==3,"ballistic weapons use the same quantity modifier");
        }
    }
}
void gunBalanceAndTiming() {
    TowerSystem tower;
    BuildingInstance gun{.id={800,1},.type=BuildingType::GunTurret};
    for (int level=1;level<=MaxBuildingLevel;++level) {
        gun.level=static_cast<std::uint8_t>(level);
        const auto stats=tower.finalStats(gun);
        require(stats.quantity==3,"gun is a three-bullet burst at every authored level");
        const double oldDps=TowerSystem::attackDamage(gun.type,gun.level)*(2.5+0.1*(level-1));
        requireNear(stats.damage*stats.quantity/stats.attackInterval,oldDps,1e-12,
            "burst conversion preserves old per-level raw DPS");
    }
    gun.level=1;
    const auto base=tower.finalStats(gun);
    requireNear(base.attackInterval,1.2,1e-12,"level one event interval is 1.2 seconds");
    requireNear(gunBurstTiming(base).interval,0.08,1e-12,"base burst retains an audible 80ms rhythm");
    requireNear(gunBurstTiming(base).duration,0.16,1e-12,"three bullets occupy 160ms");
    for (const auto [quantity,speed] : std::array<std::pair<int,double>,5>{{
        {3,1},{10,1},{30,1},{10,2},{30,5}}}) {
        AttackModifiers modifiers;
        modifiers[AttackStat::Quantity].flat=quantity-3;
        modifiers[AttackStat::AttackSpeed].multiplier=speed;
        tower.setAttackModifiers(modifiers);
        const auto stats=tower.finalStats(gun);
        const auto timing=gunBurstTiming(stats);
        require(timing.duration <= stats.attackInterval*GunBurstDurationFraction+1e-12,
            "every burst leaves the configured event pause even at high quantity/speed");
        requireNear(timing.interval,std::min(0.08/std::sqrt(speed),
            stats.attackInterval*0.6/(quantity-1)),1e-12,"dynamic timing matches density and weak speed scaling");
        const auto description=describeAttackStats(stats);
        for(const auto label:{"Base Burst Interval:","Effective Burst Interval:","Total Burst Duration:"})
            require(description.find(label)!=std::string::npos,"debug contains all burst timing fields");
        std::cout << "Gun timing Q=" << quantity << " AS=" << speed
            << " interval=" << stats.attackInterval << " spacing=" << timing.interval
            << " duration=" << timing.duration << " bullets/sec=" << quantity/stats.attackInterval << '\n';
    }
}
void gunThroughputAndSnapshot() {
    const std::vector<BuildingInstance> registry{{.id={810,1},.type=BuildingType::GunTurret}};
    for(const double step:{1.0/60.0,Step}) {
        double previousThroughput=0;
        for(const auto [quantity,speed]:std::array<std::pair<int,double>,4>{{{3,1},{3,2},{10,2},{30,5}}}) {
            EnemySystem enemies;spawn(enemies,{{0,0.8,-6}});
            TowerSystem tower;AttackModifiers modifiers;
            modifiers[AttackStat::Quantity].flat=quantity-3;
            modifiers[AttackStat::AttackSpeed].multiplier=speed;
            modifiers[AttackStat::Damage].multiplier=0;
            tower.setAttackModifiers(modifiers);tower.syncBuildings(registry);
            const auto stats=tower.finalStats(registry.front());
            int launchesInEvent=0,completedEvents=0,totalLaunches=0;
            double time=0,firstEvent=-1,lastEvent=-1;
            for(int frame=0;frame<static_cast<int>(30/step);++frame) {
                time+=step;tower.tick(step,registry,enemies);
                require(tower.attacks().size()<=1,"one gun starts at most one controlled sequence per step");
                if(!tower.attacks().empty()) {
                    if(firstEvent<0) firstEvent=time;
                    else {
                        require(launchesInEvent==quantity,"all previous instances finish before the next event");
                        requireNear(time-lastEvent,stats.attackInterval,step+1e-9,
                            "start-to-start cadence tracks Attack Speed, not sequence length");
                        ++completedEvents;
                    }
                    lastEvent=time;launchesInEvent=0;
                }
                launchesInEvent+=static_cast<int>(tower.launches().size());
                totalLaunches+=static_cast<int>(tower.launches().size());
                for(const auto& projectile:tower.projectiles()) {
                    if(!projectile.active || projectile.runtime.elapsedLifetime>step+1e-12)continue;
                    const auto& gun=tower.towers().front();
                    const Vec3 aimed{-std::sin(gun.sequenceYaw)*std::cos(gun.sequencePitch),
                        std::sin(gun.sequencePitch),-std::cos(gun.sequenceYaw)*std::cos(gun.sequencePitch)};
                    const auto direction=geometry::normalizedOr(projectile.runtime.velocity);
                    const double cosine=geometry::dot(aimed,direction);
                    require(cosine>=std::cos(2.0*PiRadians/180.0)-1e-9,
                        "all burst bullets retain independent small spread even at quantity thirty");
                }
                require(launchesInEvent<=quantity,"no overlapping or excess burst instances");
            }
            require(completedEvents>10,"throughput measures many complete events");
            const double throughput=(totalLaunches-launchesInEvent)/(lastEvent-firstEvent);
            requireNear(throughput,quantity/stats.attackInterval,0.1,
                "measured physical spawn throughput keeps count times speed scaling");
            require(throughput>previousThroughput,"each quantity/speed combination improves real throughput");
            previousThroughput=throughput;
        }
    }
    EnemySystem enemies;spawn(enemies,{{0,0.8,-6}});
    TowerSystem tower;tower.syncBuildings(registry);
    while(tower.attacks().empty())tower.tick(Step,registry,enemies);
    AttackModifiers modifiers;modifiers[AttackStat::Quantity].flat=27;
    modifiers[AttackStat::AttackSpeed].multiplier=5;tower.setAttackModifiers(modifiers);
    enemies.reset(); // Committed burst survives loss of target and later modifiers.
    int count=1;
    for(int frame=0;frame<30;++frame) {
        tower.tick(Step,registry,enemies);count+=static_cast<int>(tower.launches().size());
    }
    require(count==3,"current event retains quantity and timing snapshot after an upgrade and target loss");
    tower.clearProjectiles();
    require(tower.towers().front().sequenceRemaining==0 && tower.towers().front().sequenceInterval==0,
        "clear cancels the sequence and its derived timing");
}
void compatibilityAndSlowCoverage() {
    constexpr std::array physical{BuildingType::GunTurret,BuildingType::Turret,
        BuildingType::Cannon,BuildingType::Catapult};
    constexpr std::array universal{AttackStat::Damage,AttackStat::AttackSpeed,AttackStat::Quantity,
        AttackStat::ProjectileSpeed,AttackStat::Duration,AttackStat::Size,AttackStat::Bounces,
        AttackStat::Pierce,AttackStat::CritChance,AttackStat::CritDamage,AttackStat::Knockback};
    require(DefenseAttackCompatibility.size()==12,"all twelve attack types have metadata");
    for(const auto type:physical)for(const auto stat:universal) {
        const bool artillery = type==BuildingType::Cannon || type==BuildingType::Catapult;
        const bool supported = !artillery || (stat!=AttackStat::Bounces && stat!=AttackStat::Pierce);
        require(supportsAttackStat(type,stat)==supported,
            "artillery excludes ricochet and piercing while direct weapons retain them");
    }
    for(const auto stat:universal) {
        const bool spike=stat!=AttackStat::ProjectileSpeed && stat!=AttackStat::Bounces && stat!=AttackStat::Pierce && stat!=AttackStat::Duration;
        const bool slow=stat==AttackStat::AttackSpeed || stat==AttackStat::Duration || stat==AttackStat::Size;
        require(supportsAttackStat(BuildingType::SpikeTrap,stat)==spike,"spike flags match shared pulse hit flow");
        require(supportsAttackStat(BuildingType::SlowTrap,stat)==slow,"slow flags describe activation and coverage only");
        for(const auto type:{BuildingType::Core,BuildingType::Wall,BuildingType::Gate,
            BuildingType::CrystalMine,BuildingType::Quarry,BuildingType::LumberMill})
            require(!supportsAttackStat(type,stat),"non-attacking buildings never qualify as an attack weapon");
    }
    require(!supportsAttackStat(BuildingType::GunTurret,AttackStat::FanSpread) &&
        supportsAttackStat(BuildingType::Turret,AttackStat::FanSpread),"gun count does not widen a fan");
    const std::vector<BuildingInstance> traps{{.type=BuildingType::SlowTrap},{.type=BuildingType::SpikeTrap}};
    require(!defenseUpgradeAppliesToAny(RunUpgradeEffect::Bounces,traps) &&
        !defenseUpgradeAppliesToAny(RunUpgradeEffect::ProjectileSpeed,traps) &&
        defenseUpgradeAppliesToAny(RunUpgradeEffect::ProjectileCount,traps) &&
        defenseUpgradeAppliesToAny(RunUpgradeEffect::AttackSize,traps),"card API qualifies the player's current weapon set");
    require(!defenseUpgradeAppliesToAny(RunUpgradeEffect::Bounces,{}),"empty weapon set cannot consume bounce");
    const std::vector<BuildingInstance> slow{{.id={820,1},.type=BuildingType::SlowTrap}};
    for(const int quantity:{1,30})for(const double size:{1.0,2.0}) {
        EnemySystem enemies;spawn(enemies,{{2.4,0.8,0}});
        TrapSystem system;AttackModifiers modifiers;
        modifiers[AttackStat::Quantity].flat=quantity-1;
        modifiers[AttackStat::Size].multiplier=size;
        system.setAttackModifiers(modifiers);system.syncBuildings(slow);
        require(system.finalStats(slow.front()).quantity==1,"unsupported slow quantity is neutral in FinalAttackStats");
        require(system.tick(Step,slow,enemies).empty()==(size==1),
            "Quantity never changes coverage, Size brings the outside target inside");
    }
    for(const int quantity:{1,3}) {
        const std::vector<BuildingInstance> spikes{{.id={821,1},.type=BuildingType::SpikeTrap}};
        EnemySystem enemies;spawn(enemies,{{0,0.8,0}});
        TrapSystem system;AttackModifiers modifiers;
        modifiers[AttackStat::Quantity].flat=quantity-1;
        modifiers[AttackStat::Damage].multiplier=0.001;
        modifiers[AttackStat::CritChance].flat=1;
        system.setAttackModifiers(modifiers);system.syncBuildings(spikes);system.tick(Step,spikes,enemies);
        require(system.hits().size()==static_cast<std::size_t>(quantity),"spike count repeats independent weighted area hits");
        for(std::size_t index=0;index<system.hits().size();++index) {
            const auto& hit=system.hits()[index];
            require(hit.critical,"spikes use shared crit flow");
            requireNear(hit.result.damage,0.024*(index==0?1.0:.35),1e-12,
                "native spike pulse stays full strength while extra pulses use reduced damage");
        }
        require(enemies.enemies().front().knockbackVelocity.y!=0 ||
            geometry::length(enemies.enemies().front().knockbackVelocity)>0,"spikes use shared knockback");
    }
}
void denseBurstPoolStress() {
    const std::vector<BuildingInstance> registry{{.id={830,1},.type=BuildingType::GunTurret}};
    EnemySystem enemies;spawn(enemies,{{0,0.8,-6}});
    TowerSystem tower;AttackModifiers modifiers;
    modifiers[AttackStat::Quantity].flat=27;
    modifiers[AttackStat::AttackSpeed].multiplier=5;
    modifiers[AttackStat::Bounces].flat=5;
    modifiers[AttackStat::Duration].multiplier=5;
    modifiers[AttackStat::Damage].multiplier=0;
    modifiers[AttackStat::Spread].flat=178; // Most shots miss and occupy their entire lifetime.
    tower.setAttackModifiers(modifiers);tower.syncBuildings(registry);
    std::size_t peakSpawn=0,peakActive=0;
    for(int frame=0;frame<3600;++frame) {
        tower.tick(1.0/60,registry,enemies);
        peakSpawn=std::max(peakSpawn,tower.launches().size());
        peakActive=std::max(peakActive,static_cast<std::size_t>(std::ranges::count_if(tower.projectiles(),
            [](const AttackProjectile& shot){return shot.active;})));
    }
    require(peakSpawn<=5,"dense burst spreads spawns across steps instead of launching thirty in one frame");
    require(peakActive>30 && peakActive<300 && tower.projectiles().size()<300 && tower.droppedProjectiles()==0,
        "missed late-build shots expire near weapon range rather than occupying the pool for the full effect duration");
    const auto highWater=tower.projectiles().size();
    tower.clearProjectiles();
    require(std::ranges::none_of(tower.projectiles(),[](const AttackProjectile& shot){return shot.active;}),
        "stress clear releases all pooled projectiles");
    for(int frame=0;frame<600;++frame)tower.tick(1.0/60,registry,enemies);
    require(tower.projectiles().size()==highWater,"second warm run reuses the allocated high-water storage");
    std::cout << "Gun stress peak active=" << peakActive << " slots=" << highWater
        << " max spawns/frame=" << peakSpawn << '\n';
}
void sequenceSaturation() {
    const std::vector<BuildingInstance> registry{{.id={840,1},.type=BuildingType::GunTurret}};
    EnemySystem enemies;spawn(enemies,{{0,0.8,-6}});
    TowerSystem tower;AttackModifiers modifiers;
    modifiers[AttackStat::Quantity].flat=1000000;
    modifiers[AttackStat::ProjectileSpeed].multiplier=0;
    tower.setAttackModifiers(modifiers);tower.syncBuildings(registry);
    for(int frame=0;frame<10 && tower.droppedProjectiles()==0;++frame)tower.tick(Step,registry,enemies);
    require(tower.projectiles().size()==ProjectilePool<AttackProjectile>::Capacity &&
        tower.droppedProjectiles()==1 && tower.towers().front().sequenceRemaining==0,
        "a saturated dense sequence stops failed spawns without iterating the entire huge quantity");
    tower.reset();
    require(tower.projectiles().size()==ProjectilePool<AttackProjectile>::Capacity &&
        tower.droppedProjectiles()==0 && tower.towers().empty(),"reset retains bounded allocation and clears saturation state");
}
void physicalShrapnel() {
    const std::vector<BuildingInstance> registry{{.id={750,1},.type=BuildingType::GunTurret}};
    EnemySystem enemies;
    const std::array<EnemySpawn,3> targets{{
        {.type=EnemyType::Basic,.position={0,0.8,-6},.healthMultiplier=0.01},
        {.type=EnemyType::Basic,.position={2,0.8,-6}},
        {.type=EnemyType::Basic,.position={-2,0.8,-6}}}};
    enemies.spawnWave(targets);
    TowerSystem tower;tower.setShrapnelCount(3);tower.syncBuildings(registry);
    bool killed=false,shardSpawned=false,shardHit=false;
    for(int frame=0;frame<240;++frame) {
        for(const auto& hit:tower.tick(Step,registry,enemies)) {
            killed|=hit.killed;
            if(hit.shrapnel) {
                shardHit=true;
                require(hit.secondaryImpact && hit.origin.z<0,"shards retain secondary impact attribution and physical origin");
                requireNear(hit.payloadDamage,0.45,1e-12,"shards retain the parent snapshot damage fraction");
            }
        }
        for(const auto& projectile:tower.projectiles())if(projectile.active && projectile.shrapnel)shardSpawned=true;
    }
    require(killed && shardSpawned && shardHit,"gun kill emits travelling, damaging shards via the common pool");
}
void artilleryAndCapacity() {
    {
        const std::vector<BuildingInstance> registry{{.id={700,1},.type=BuildingType::Cannon}};
        EnemySystem enemies;spawn(enemies,{{0,0.8,-6},{4.5,0.8,-6},{9,0.8,-6}});
        CannonSystem cannon;
        AttackModifiers modifiers; modifiers[AttackStat::Bounces].flat=2;
        modifiers[AttackStat::Damage].multiplier=0.01;
        cannon.setAttackModifiers(modifiers);cannon.syncBuildings(registry);
        int hits=0;bool launched=false;
        for(int frame=0;frame<600;++frame) {
            cannon.tick(Step,registry,enemies);
            if(!cannon.shots().empty()) {
                if(launched)break;
                launched=true;
            }
            hits+=static_cast<int>(cannon.hits().size());
        }
        require(hits==1 && cannon.finalStats(registry.front()).bounces==0,
            "obsolete global bounce does not create extra cannon explosions");
    }
    {
        const std::vector<BuildingInstance> registry{{.id={701,1},.type=BuildingType::Cannon}};
        EnemySystem enemies;spawn(enemies,{{0,0.8,-3},{0,0.8,-6}});
        CannonSystem cannon;
        AttackModifiers modifiers; modifiers[AttackStat::Pierce].flat=1;
        modifiers[AttackStat::Damage].multiplier=0.01;
        cannon.setAttackModifiers(modifiers);cannon.syncBuildings(registry);
        int directContacts=0;
        for(int frame=0;frame<240;++frame) {
            const auto explosions=cannon.tick(Step,registry,enemies);
            if(explosions.empty())directContacts+=static_cast<int>(cannon.hits().size());
            else break;
        }
        require(directContacts==0 && cannon.finalStats(registry.front()).pierce==0,
            "obsolete global pierce does not pass through cannon targets");
    }
    {
        const std::vector<BuildingInstance> registry{{.id={702,1},.type=BuildingType::Cannon}};
        EnemySystem enemies;spawn(enemies,{{0,0.8,-8}});
        CannonSystem cannon;
        AttackModifiers modifiers; modifiers[AttackStat::ProjectileSpeed].multiplier=0;
        modifiers[AttackStat::Duration].multiplier=0.02;
        cannon.setAttackModifiers(modifiers);cannon.syncBuildings(registry);
        bool launched=false;
        for(int frame=0;frame<240 && !launched;++frame) {
            cannon.tick(Step,registry,enemies);launched=!cannon.shots().empty();
        }
        require(launched,"zero-speed shell remains a safe launch configuration");
        const auto& shell=cannon.projectiles().front();
        require(shell.velocity.x==0 && shell.velocity.z==0 && std::isfinite(shell.velocity.y),
            "zero projectile speed cannot generate an enormous ballistic impulse");
        for(int frame=0;frame<24;++frame)cannon.tick(Step,registry,enemies);
        require(!cannon.projectiles().front().active,"short duration despawns an unarrived shell");
    }
    {
        const std::vector<BuildingInstance> registry{{.id={703,1},.type=BuildingType::Catapult}};
        EnemySystem enemies;spawn(enemies,{{0,0.8,-8}});
        CannonSystem cannon;
        AttackModifiers modifiers; modifiers[AttackStat::Quantity].flat=2;
        cannon.setAttackModifiers(modifiers);cannon.syncBuildings(registry);
        cannon.tick(Step,registry,enemies);
        modifiers[AttackStat::Quantity].flat=8;cannon.setAttackModifiers(modifiers);
        for(int frame=0;frame<240 && cannon.shots().empty();++frame)cannon.tick(Step,registry,enemies);
        require(cannon.projectiles().size()==3,"catapult windup uses the event snapshot despite subsequent upgrades");
    }
    {
        AttackProjectileSystem system;auto stats=projectileStats();
        for(std::size_t index=0;index<ProjectilePool<AttackProjectile>::Capacity;++index)
            require(system.spawn({1,1},BuildingType::GunTurret,{},{0,0,-1},stats,0),"high capacity accepts a late build");
        require(!system.spawn({1,1},BuildingType::GunTurret,{},{0,0,-1},stats,0) && system.dropped()==1,
            "pool saturation is observable and bounded");
        system.reset();
        require(system.spawn({1,1},BuildingType::GunTurret,{},{0,0,-1},stats,0) && system.dropped()==0,
            "capacity is reusable after reset without stale saturation state");
    }
    {
        BuildingInstance building{.id={704,1},.type=BuildingType::Turret};
        TowerSystem tower;AttackModifiers modifiers;modifiers[AttackStat::Damage].multiplier=1.5;
        modifiers[AttackStat::Quantity].flat=2;tower.setAttackModifiers(modifiers);
        const auto levelOne=tower.finalStats(building);
        building.level=3;building.anvilStacks=2;
        const auto upgraded=tower.finalStats(building);
        requireNear(upgraded.damage,TowerSystem::baseStats(building.type,3).damage*1.5*1.2,1e-12,
            "building level and anvil recompute from immutable weapon data");
        require(upgraded.quantity==levelOne.quantity && upgraded.pierce==3,
            "building upgrades retain global quantity and gain authored pierce");
        tower.reset();require(tower.finalStats(building).quantity==1,"weapon reset clears its card modifiers");
    }
}
void movingTargets() {
    for (const auto type : {EnemyType::Basic,EnemyType::Fast}) {
        int fastHits=0,slowHits=0;
        for(const double speed : {1.0,100.0}) {
            EnemySystem enemies;
            std::array<EnemySpawn,1> spawns{{{.type=type,.position={0,0.8,-5}}}};
            enemies.spawnWave(spawns);
            const std::vector<BuildingInstance> registry{{.id={500,1},.type=BuildingType::Core,
                .gridPosition={10,-5},.health=10000,.maxHealth=10000}};
            FlowField flow;flow.rebuild({10,-5},registry);
            AttackProjectileSystem system;
            auto stats=projectileStats();stats.projectileSpeed=speed;stats.duration=6;
            system.spawn({1,1},BuildingType::GunTurret,{0,0.8,0},{0,0,-1},stats,0);
            int hits=0;
            for(int frame=0;frame<720;++frame) {
                enemies.tick(Step,registry,flow);hits+=static_cast<int>(system.tick(Step,enemies).size());
            }
            if(speed>1)fastHits=hits;else slowHits=hits;
        }
        require(fastHits==1 && slowHits==0,"finite speed naturally changes accuracy against moving slow/fast enemies");
    }
    // Original target moves away; another enemy crosses the unchanged flight.
    EnemySystem enemies;spawn(enemies,{{3,0.8,-3},{0,0.8,-6}});
    AttackProjectileSystem system;auto stats=projectileStats();
    system.spawn({1,1},BuildingType::GunTurret,{0,0.8,0},{0,0,-1},stats,0);
    auto hits=fly(system,enemies,1);
    require(hits.size()==1 && hits.front().hit.result.id==enemies.enemies()[1].id,"a shot can hit an enemy other than its original target");
}
void cardsSaveAndReset() {
    auto simulation=std::make_unique<Simulation>();simulation->startRunFromSeed(PlayerClass::Vanguard,12345);
    for(const auto effect:{RunUpgradeEffect::DefenseDamage,RunUpgradeEffect::DefenseFireRate,
        RunUpgradeEffect::ProjectileCount,RunUpgradeEffect::ProjectileCount,
        RunUpgradeEffect::ProjectileSpeed,RunUpgradeEffect::AttackDuration,RunUpgradeEffect::AttackSize,
        RunUpgradeEffect::CriticalChance,RunUpgradeEffect::CriticalDamage,RunUpgradeEffect::AttackKnockback}) {
        require(simulation->grantDebugRunUpgrade(effect),"stat card grants through existing progression");
    }
    const auto saved=simulation->saveSuspendedRunState();
    auto restored=std::make_unique<Simulation>();
    require(restored->loadSuspendedRunState(saved),"expanded stat card powers restore from suspended run");
    require(restored->snapshot().upgradeStats.projectileCountBonus==2 &&
        restored->snapshot().upgradeStats.bounces==0 && restored->snapshot().upgradeStats.pierce==0,
        "retired global ricochet and piercing stats stay absent");
    requireNear(restored->snapshot().upgradeStats.projectileSpeed,1.2,1e-12,"new modifiers restore without double application");
    restored->returnToMainMenu();
    require(restored->loadSuspendedRunState(saved),"loading twice remains idempotent");
    requireNear(restored->snapshot().upgradeStats.defenseDamage,simulation->snapshot().upgradeStats.defenseDamage,1e-12,
        "damage rebuild is idempotent and respects class/meta factors");
    restored->returnToMainMenu();restored->startRunFromSeed(PlayerClass::Vanguard,12345);
    require(restored->snapshot().upgradeStats.projectileCountBonus==0 && restored->snapshot().upgradeStats.bounces==0 &&
        restored->snapshot().upgradeStats.projectileSpeed==1 && restored->snapshot().defenseProjectiles.empty(),
        "new run resets modifiers and projectile state");
    require(static_cast<int>(RunUpgradeEffect::Bounces)==14 && static_cast<int>(RunUpgradeEffect::ProjectileCount)==20 &&
        static_cast<int>(RunUpgradeEffect::Pierce)==23,"old save upgrade indices never move");
}
}
int main() {
    statsAndPatterns(); gunBalanceAndTiming(); gunThroughputAndSnapshot(); compatibilityAndSlowCoverage(); denseBurstPoolStress(); sequenceSaturation(); collisionAndCounters(); poolingAndLifetime();weaponCombinations();physicalShrapnel();artilleryAndCapacity();movingTargets();cardsSaveAndReset();
    std::cout << "Attack system tests passed\n";
}
