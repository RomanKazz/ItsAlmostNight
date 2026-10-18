#pragma once

#include "TestHarness.hpp"
#include "combat/CombatDisplayUnits.hpp"
#include "enemies/EnemyDifficulty.hpp"
#include "game/Simulation.hpp"

#include <limits>
#include <memory>

inline void checkOvertime() {
    using namespace ian;
    const auto minute = overtimeDifficulty(60.0);
    requireNear(minute.health, 2.0, 1e-12, "overtime doubles health each combat minute");
    requireNear(minute.damage, std::sqrt(2.0), 1e-12, "damage doubles every two minutes");
    const auto five = overtimeDifficulty(300.0);
    requireNear(five.health, 32.0, 1e-12, "strong builds face exponential overtime health");
    requireNear(five.speed, 1.6, 1e-12, "speed rises gradually");
    requireNear(five.spawnRate, 2.0, 1e-12, "spawn pressure is capped instead of flooding containers");
    const auto longRun = overtimeDifficulty(1e9);
    require(std::isfinite(longRun.health) && longRun.speed == 1.75 && longRun.spawnRate == 2.0,
        "long runs keep finite health and bounded movement/density");
    require(overtimeDifficulty(0.0) == OvertimeDifficulty{} &&
        overtimeDifficulty(std::numeric_limits<double>::quiet_NaN()) == OvertimeDifficulty{},
        "zero/invalid clocks are safe");
    require(combatDisplayValue(1e30) == std::numeric_limits<int>::max() &&
        combatDisplayValue(std::numeric_limits<double>::quiet_NaN()) == 0,
        "late-game health/damage labels saturate without integer overflow");
    OvertimeConfig invalid;
    invalid.healthDoublingSeconds = 0;
    require(!invalid.valid(), "invalid overtime curves are rejected");

    EnemySystem enemies;
    const std::array spawns{
        EnemySpawn{.type=EnemyType::Basic, .position={0,0,0}},
        EnemySpawn{.type=EnemyType::Heavy, .position={10,0,0}, .eliteAffixes=eliteAffixMask(EliteAffix::Berserker)},
        EnemySpawn{.type=EnemyType::Boss, .position={20,0,0}}};
    enemies.spawnWave(spawns);
    const auto base = enemies.enemies();
    require(enemies.damage(base[0].id, 2.0).has_value(), "fixture damages an enemy before pressure rises");
    enemies.setOvertimeDifficulty(minute);
    requireNear(enemies.enemies()[0].health / enemies.enemies()[0].maxHealth, 0.6, 1e-12,
        "live scaling preserves damage fraction instead of healing to full");
    for (std::size_t i=0; i<base.size(); ++i) {
        requireNear(enemies.enemies()[i].maxHealth, base[i].maxHealth * minute.health, 1e-8,
            "regular, elite and boss actors strengthen while alive");
        requireNear(enemies.enemies()[i].speed, base[i].speed * minute.speed, 1e-8,
            "all existing enemy types receive movement scaling");
    }
    enemies.spawnGroup(std::span<const EnemySpawn>{spawns}.first(1));
    requireNear(enemies.enemies().back().maxHealth, base[0].maxHealth * minute.health, 1e-8,
        "new enemies receive the same pressure exactly once");
    const auto once = enemies.enemies().back();
    enemies.setOvertimeDifficulty(minute);
    requireNear(enemies.enemies().back().maxHealth, once.maxHealth, 1e-12,
        "refreshing identical difficulty never stacks it again");
    enemies.setOvertimeDifficulty({.health=std::numeric_limits<double>::infinity()});
    requireNear(enemies.enemies().back().maxHealth, once.maxHealth, 1e-12,
        "invalid difficulty cannot poison live actors");
    enemies.reset();
    const std::array splitSpawn{EnemySpawn{.type=EnemyType::Splitter, .position={0,0,0}}};
    enemies.setOvertimeDifficulty(minute);
    enemies.spawnWave(splitSpawn);
    const auto parent = enemies.enemies().front();
    require(enemies.damage(parent.id, 1e6)->killed, "overtime splitter fixture dies");
    enemies.setOvertimeDifficulty(overtimeDifficulty(120.0));
    FlowField flow;
    flow.rebuild({0,0}, {});
    enemies.tick(.4, {}, flow);
    for (const auto& child : enemies.enemies()) if (child.active) {
        require(child.type == EnemyType::Splitling, "split creates only children");
        requireNear(child.maxHealth,
            GameBalance::defaults().enemies[static_cast<std::size_t>(EnemyType::Splitling)].health * 4.0, 1e-8,
            "delayed split inherits current overtime without squaring the parent's multiplier");
    }
    require(enemies.activeCount()==3, "three children remain under overtime");
    enemies.reset();
    enemies.spawnWave(std::span<const EnemySpawn>{spawns}.first(1));
    requireNear(enemies.enemies().front().maxHealth, base[0].maxHealth, 1e-12,
        "reset clears overtime state from reused enemy slots");

    // Pressure updates reuse actor storage even at the existing population limit.
    std::vector<EnemySpawn> stress(EnemySystem::MaxActiveEnemies);
    for (std::size_t i=0; i<stress.size(); ++i)
        stress[i] = {.type=static_cast<EnemyType>(i % GameBalance::EnemyTypeCount),
            .position={static_cast<double>(i),0,100}};
    enemies.spawnWave(stress);
    const auto* actorStorage = enemies.enemies().data();
    const auto actorCapacity = enemies.enemies().capacity();
    for (int second=0; second<600; ++second) enemies.setOvertimeDifficulty(overtimeDifficulty(second));
    require(enemies.enemies().data()==actorStorage && enemies.enemies().capacity()==actorCapacity &&
        enemies.activeCount()==EnemySystem::MaxActiveEnemies,
        "continuous difficulty does not allocate, spawn or grow runtime containers");
    for (const auto& actor : enemies.enemies()) require(std::isfinite(actor.health) && std::isfinite(actor.damage),
        "all enemy archetypes retain finite statistics under long pressure");
    enemies.reset();

    BuildingSystem courtBuildings;
    courtBuildings.setSandboxMode(true);
    require(courtBuildings.place(BuildingType::Core, {0,0}, 0, 30, 0).has_value(),
        "pressure court fixture places its core");
    flow.rebuild({0,0}, courtBuildings.buildings());
    enemies.setOvertimeDifficulty(minute);
    const std::array royalSpawn{EnemySpawn{.type=EnemyType::Boss, .position={0,3,-9}}};
    enemies.spawnWave(royalSpawn);
    bool sawCourt = false;
    for (int step=0; step<1000 && !(sawCourt && !enemies.projectiles().empty()); ++step) {
        enemies.tick(1.0/60.0, courtBuildings.buildings(), flow, Vec3{0,1.7,0});
        for (const auto& action : enemies.takeBossActionEvents()) if (action.type==BossActionType::WarCry) {
            sawCourt = true;
            const auto battle = GameBalance::defaults().enemies[static_cast<std::size_t>(EnemyType::Boss)].bossBattle;
            requireNear(action.healthScale, battle.summonHealthMultiplier, 1e-8,
                "royal summons inherit ordinary health scaling before applying pressure once");
            requireNear(action.damageScale, battle.summonDamageMultiplier, 1e-8,
                "royal summons never square overtime damage");
        }
    }
    require(sawCourt && !enemies.projectiles().empty(), "overtime king performs summons and a physical volley");
    const auto shot = enemies.projectiles().front();
    enemies.setOvertimeDifficulty(overtimeDifficulty(120));
    requireNear(enemies.projectiles().front().damage, shot.damage * std::sqrt(2.0), 1e-8,
        "already flying hostile shots strengthen without being replaced");
    require(enemies.projectiles().front().id==shot.id,
        "pressure never restarts projectile flight");
    enemies.reset();

    auto map = MapDefinition::defaults();
    map.obstacles.clear();
    auto world = WorldConfig::defaults();
    world.terrainAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto run = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
    run->startRunFromSeed(PlayerClass::Vanguard, 548231);
    PlayerCommand god;
    god.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
    run->tick(0, god);
    PlayerCommand place;
    place.placeBuilding = PlaceBuildingCommand{BuildingType::Core, {0,0}, 0};
    run->tick(0, place);
    require(run->snapshot().coreId.has_value(), "overtime fixture places a core");
    while (run->snapshot().runUpgradeChoicePending) require(run->selectRunUpgrade(0), "fixture resolves rewards");
    auto saved = run->saveSuspendedRunState();
    auto producerCheckpoint = saved;
    const auto definitions = GameBalance::defaults().buildings;
    for (const auto type : {BuildingType::CrystalMine, BuildingType::LumberMill, BuildingType::Quarry}) {
        const auto i = static_cast<std::size_t>(type);
        const auto id = EntityId{static_cast<std::uint32_t>(10000 + i), 1};
        producerCheckpoint.buildingBlueprintLevels[i] = 1;
        producerCheckpoint.buildings.push_back({.id=id, .type=type, .gridPosition={4+static_cast<int>(i)*2, 4},
            .level=1, .health=definitions[i].maxHealth, .maxHealth=definitions[i].maxHealth});
    }
    const auto sampleProduction = [&](int wave) {
        auto peaceful = GameBalance::defaults();
        for (auto& enemy : peaceful.enemies) { enemy.speed = .01; enemy.damage = .01; }
        auto sample = std::make_unique<Simulation>(peaceful, map, world);
        auto checkpoint = producerCheckpoint;
        checkpoint.wave = checkpoint.bestWave = wave;
        checkpoint.phaseTimeRemaining = checkpoint.phaseDuration = 75;
        checkpoint.wood = checkpoint.stone = 0;
        for (auto& landmark : checkpoint.worldLandmarks) landmark.activated = true;
        require(sample->loadSuspendedRunState(checkpoint), "producer day/night fixture loads");
        if (wave == 10) {
            sample->tick(0, god);
            PlayerCommand startNight;
            startNight.startWaveEarly = StartWaveEarlyCommand{};
            sample->tick(0, startNight);
            sample->tick(0, god);
        }
        require(!sample->snapshot().unlimitedResources, "production test uses ordinary gameplay");
        sample->tick(18);
        require(sample->snapshot().producerCards.size()==3, "all three producers remain active");
        require(sample->snapshot().stone > 0 && sample->snapshot().wood > 0,
            "activated world mine and mill work both by day and at night");
        return std::vector<ProducerCardStatus>(sample->snapshot().producerCards.begin(), sample->snapshot().producerCards.end());
    };
    const auto day = sampleProduction(9);
    const auto night = sampleProduction(10);
    for (std::size_t i=0; i<day.size(); ++i) {
        requireNear(*day[i].stats.current.productionInterval, *night[i].stats.current.productionInterval, 1e-12,
            "producer speed is identical in daytime and nighttime");
        require(day[i].stored > 0 && day[i].stored == night[i].stored,
            "night production grants the same real output without godmode");
    }
    saved.wave = saved.bestWave = 9;
    saved.overtimeSeconds = 0;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(saved), "legacy checkpoint starts with no overtime");
    run->tick(0, god);
    PlayerCommand start;
    start.startWaveEarly = StartWaveEarlyCommand{};
    run->tick(0, start);
    require(run->snapshot().wave==10 && !run->snapshot().overtimeActive,
        "wave ten keeps ordinary difficulty");
    run->tick(1);
    requireNear(run->snapshot().overtimeSeconds, 0.0, 1e-12, "no early overtime during wave ten");

    saved.wave = saved.bestWave = 10;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(saved), "checkpoint reaches wave eleven");
    run->tick(0, god);
    run->tick(0, start);
    require(run->snapshot().wave==11 && run->snapshot().overtimeActive,
        "timer activates at start of wave eleven");
    const auto first = run->snapshot().enemies.front();
    run->tick(1);
    requireNear(run->snapshot().overtimeSeconds, 1.0, 1e-12, "combat advances the pressure clock");
    requireNear(run->snapshot().enemies.front().maxHealth,
        first.maxHealth * overtimeDifficulty(1).health, 1e-8, "simulation scales actors continuously");
    run->togglePause();
    run->tick(20);
    requireNear(run->snapshot().overtimeSeconds, 1.0, 1e-12, "pause does not strengthen enemies");
    run->togglePause();
    require(run->openRunShop(true), "debug shop can pause overtime");
    run->tick(20);
    requireNear(run->snapshot().overtimeSeconds, 1.0, 1e-12, "shop stops overtime");
    require(run->closeRunShop(), "fixture closes shop");
    run->tick(1);
    const auto replay = run->saveSuspendedRunState();
    require(replay.wave==10 && replay.overtimeSeconds==2.0,
        "combat checkpoint rewinds rewards but never pressure time");
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(replay), "pressure survives checkpoint load");
    run->tick(5);
    requireNear(run->snapshot().overtimeSeconds, 2.0, 1e-12, "daytime freezes the retained pressure");
    run->tick(0, god);
    run->tick(0, start);
    requireNear(run->snapshot().enemies.front().maxHealth,
        first.maxHealth * overtimeDifficulty(2).health, 1e-8, "replayed night resumes existing pressure");
    run->tick(1);
    requireNear(run->snapshot().overtimeSeconds, 3.0, 1e-12, "resumed combat continues the clock");
    saved.wave = saved.bestWave = Simulation::StageClearWave - 1;
    saved.overtimeSeconds = 120;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(saved), "boss pressure checkpoint loads");
    run->tick(0, god);
    run->tick(0, start);
    run->tick(61);
    require(run->snapshot().state == RunState::Wave && run->snapshot().phaseTimeRemaining == 0,
        "living stage boss keeps combat active after the dawn countdown");
    requireNear(run->snapshot().overtimeSeconds, 181.0, 1e-12,
        "boss stall cannot stop the pressure clock at dawn");
    run->tick(1);
    requireNear(run->snapshot().overtimeSeconds, 182.0, 1e-12, "boss overtime keeps rising");
    saved.wave = saved.bestWave = Simulation::StageClearWave;
    saved.resumeState = RunState::StageClear;
    saved.stageCleared = true;
    saved.phaseTimeRemaining = saved.phaseDuration = 0;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(saved), "stage clear retains accumulated pressure");
    run->tick(20);
    requireNear(run->snapshot().overtimeSeconds, 120.0, 1e-12, "stage-clear choice freezes overtime");
    require(run->enterFinalNight(), "final night accepts the retained clock");
    const auto enteringCount = run->snapshot().activeEnemyCount;
    run->tick(5);
    requireNear(run->snapshot().overtimeSeconds, 125.0, 1e-12, "endless night continues pressure");
    require(run->snapshot().activeEnemyCount > enteringCount,
        "endless-night reinforcement spawning runs despite its intentionally zero countdown");
    saved.wave = saved.bestWave = 12;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(saved) && run->snapshot().state == RunState::StageClear,
        "old completed twelfth-night checkpoints remain loadable");
    require(run->enterFinalNight(), "legacy completed stages can continue into endless night");
    const auto legacy = run->saveSuspendedRunState();
    require(legacy.wave == 12 && legacy.stageCleared && legacy.resumeState == RunState::StageClear,
        "legacy endless checkpoint does not skip ahead to the new sixteenth-night clear");
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(legacy), "legacy endless checkpoint round-trips");
    auto corrupt = replay;
    corrupt.overtimeSeconds = -1;
    run->returnToMainMenu();
    require(!run->loadSuspendedRunState(corrupt), "negative saved pressure is rejected");
    corrupt.overtimeSeconds = std::numeric_limits<double>::infinity();
    require(!run->loadSuspendedRunState(corrupt), "nonfinite saved pressure is rejected");
    run->startRunFromSeed(PlayerClass::Vanguard, 548232);
    require(!run->snapshot().overtimeActive && run->snapshot().overtimeSeconds==0,
        "new run starts with a clean clock");
}
