#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "waves/NightEncounter.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <set>
#include <vector>

namespace {
using namespace ian;
void resolveChoices(Simulation& simulation) {
    while (simulation.snapshot().runUpgradeChoicePending)
        require(simulation.selectRunUpgrade(0), "night fixture resolves pending choices");
    if (simulation.snapshot().runShop.pending)
        require(simulation.closeRunShop(), "night fixture closes intermission shop");
}
std::unique_ptr<Simulation> fixture(double nightSeconds = 60.0) {
    auto world = WorldConfig::defaults();
    world.terrainResolution = 33;
    world.terrainAmplitude = 0;
    world.terrainSurfaceNoiseAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto map = MapDefinition::defaults();
    map.obstacles.clear(); map.resources.clear();
    auto balance = GameBalance::defaults();
    balance.gameplay.nightDurationSeconds = nightSeconds;
    auto result = std::make_unique<Simulation>(balance, map, world);
    result->startRunFromSeed(PlayerClass::Vanguard, 73);
    PlayerCommand setup;
    setup.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
    result->tick(0, setup);
    setup = {};
    setup.placeBuilding = PlaceBuildingCommand{BuildingType::Core, {0, 0}, 0};
    result->tick(0, setup);
    require(result->snapshot().coreHealth > 0, "night fixture has a core");
    resolveChoices(*result);
    return result;
}
void startNight(Simulation& simulation) {
    resolveChoices(simulation);
    PlayerCommand start; start.startWaveEarly = StartWaveEarlyCommand{};
    simulation.tick(0, start);
    require(simulation.snapshot().state == RunState::Wave, "night starts");
}
}

int main() {
    using namespace ian;
    WaveDirector director;
    std::set<NightPattern> patterns;
    bool coherent = false, actsDiffer = false, cyclesDiffer = false;
    for (std::uint32_t seed = 0; seed < 100; ++seed) {
        for (int night = 1; night <= 9; ++night) {
            const auto forecast = director.forecast(night, seed);
            if (night <= 5) require(forecast.pattern == NightPattern::Horde,
                "onboarding nights cannot roll disruptive encounter themes");
            for (int act = 0; act < 3; ++act) {
                const auto early = director.buildWave(night, {}, 0,
                    {.runSeed = seed, .act = act});
                const EnemyType introduced = night == 6 ? EnemyType::Flying :
                    night == 7 ? EnemyType::Bee : EnemyType::AirTank;
                if (night == 6 || night == 7 || night == 9)
                    require(std::ranges::any_of(early.spawns,
                        [introduced](const auto& spawn) { return spawn.type == introduced; }),
                        "first night after an unlock visibly introduces its new flying role");
                if (night == 6 || night == 8) {
                    const auto count = [&](EnemyType type) {
                        return std::ranges::count_if(early.spawns,
                            [type](const auto& spawn) { return spawn.type == type; });
                    };
                    require(night == 6
                            ? count(EnemyType::Flying) <= 3 &&
                                count(EnemyType::Heavy) <= 5
                            : count(EnemyType::Heavy) <= 4 &&
                                count(EnemyType::Ranged) <= 5,
                        "first air night and first boss night avoid excessive heavy support");
                }
                for (const auto& spawn : early.spawns) {
                    int unlock = 1;
                    switch (spawn.type) {
                    case EnemyType::Fast: unlock = 2; break;
                    case EnemyType::Heavy: unlock = 3; break;
                    case EnemyType::Ranged: unlock = 4; break;
                    case EnemyType::Splitter: unlock = 5; break;
                    case EnemyType::Flying: unlock = 6; break;
                    case EnemyType::Bee: unlock = 7; break;
                    case EnemyType::Boss: unlock = 8; break;
                    case EnemyType::AirTank: unlock = 9; break;
                    default: break;
                    }
                    require(night >= unlock,
                        "encounter themes and acts cannot bypass enemy unlock nights");
                    if (night < 5) require(spawn.eliteAffixes == 0,
                        "first four nights contain no elite enemies");
                }
            }
        }
        const auto forecast = director.forecast(7, seed);
        patterns.insert(forecast.pattern);
        require(forecast.pattern == director.forecast(7, seed).pattern,
            "forecast is stable for a run seed");
        std::vector<EnemyType> firstCycle;
        for (int act = 0; act < 3; ++act) {
            const auto plan = director.buildWave(7, {0, 0}, 0, {.runSeed = seed, .act = act});
            require(plan.pattern == forecast.pattern, "all acts use the advertised night theme");
            require(plan.regularBudget == director.composition(7).budget,
                "specialized composition preserves configured threat budget");
            for (const auto& spawn : plan.spawns)
                require(forecast.enemies[static_cast<std::size_t>(spawn.type)] > 0,
                    "forecast includes every enemy role across all acts");
            for (std::size_t group = 0; group + static_cast<std::size_t>(plan.groupSize) <= plan.spawns.size();
                 group += static_cast<std::size_t>(plan.groupSize)) {
                const auto begin = plan.spawns.begin() + static_cast<std::ptrdiff_t>(group);
                coherent |= std::all_of(begin, begin + plan.groupSize,
                    [type = begin->type](const auto& spawn) { return spawn.type == type; });
            }
            if (act == 0) for (const auto& spawn : plan.spawns) firstCycle.push_back(spawn.type);
            else {
                std::vector<EnemyType> later;
                for (const auto& spawn : plan.spawns) later.push_back(spawn.type);
                actsDiffer |= firstCycle != later;
            }
        }
        const auto refill = director.buildWave(7, {0, 0}, 0, {.runSeed = seed, .cycle = 1});
        std::vector<EnemyType> nextCycle;
        for (const auto& spawn : refill.spawns) nextCycle.push_back(spawn.type);
        cyclesDiffer |= firstCycle != nextCycle;
        for (int wave = 1; wave <= 5; ++wave) {
            const auto base = waveEnemyCounts(director.composition(wave));
            const auto early = director.forecast(wave, seed);
            for (std::size_t role = 0; role < base.size(); ++role)
                require(base[role] > 0 || early.enemies[role] == 0,
                    "randomized nights never introduce a role before its unlock wave");
        }
    }
    require(patterns.size() == 5 && coherent && actsDiffer && cyclesDiffer,
        "run seeds cover five distinct scenarios, coherent groups, changing acts and refill order");
    for (int wave : {6, 8, 12, 16, 1000000}) {
        for (int act : {-2, 0, 1, 2, 99}) {
            const auto plan = director.buildWave(wave, {}, 0, {.runSeed = 123, .act = act});
            require(plan.spawns.size() <= WaveDirector::MaximumWaveEnemies,
                "large wave IDs and invalid acts keep bounded spawn storage");
            const auto stage = waveEnemyCounts(nightComposition(director.composition(wave), plan.pattern, act));
            for (std::size_t role = 0; role < stage.size(); ++role)
                if (stage[role] > 0) require(std::ranges::any_of(plan.spawns,
                    [role](const auto& spawn) { return static_cast<std::size_t>(spawn.type) == role; }),
                    "bounded late rosters preserve every advertised stage role");
            const auto bosses = std::count_if(plan.spawns.begin(), plan.spawns.end(),
                [](const auto& spawn) { return spawn.type == EnemyType::Boss; });
            require(bosses == (wave % WaveDirector::BossWaveInterval == 0 ? 1 : 0), "boss schedule is preserved");
        }
    }
    require(nightAct(.399) == 0 && nightAct(.4) == 1 && nightAct(.8) == 2 &&
        nightRegrouping(.38) && !nightRegrouping(.4) && nightRegrouping(.78),
        "brief warning windows precede each act change");

    {
        auto cleanup = fixture(12.0);
        startNight(*cleanup);
        cleanup->tick(11.5);
        std::vector<EntityId> survivors;
        for (const auto& enemy : cleanup->snapshot().enemies)
            if (enemy.active) survivors.push_back(enemy.id);
        require(!survivors.empty(), "cleanup fixture has survivors before dawn");
        cleanup->takeEvents();
        cleanup->tick(.5);
        require(cleanup->snapshot().state == RunState::Wave &&
            cleanup->snapshot().phaseTimeRemaining == 0 &&
            cleanup->snapshot().pendingEnemyCount == 0 &&
            !cleanup->snapshot().runShop.pending,
            "countdown expiry stops reinforcements without opening the shop or ending combat");
        require(cleanup->snapshot().activeEnemyCount == survivors.size() &&
            std::ranges::all_of(cleanup->snapshot().enemies, [&](const auto& enemy) {
                return !enemy.active || std::ranges::find(survivors, enemy.id) != survivors.end();
            }), "dawn preserves exactly the surviving enemies instead of banishing or replacing them");
        require(std::ranges::none_of(cleanup->snapshot().attackPortalPositions,
            [](const auto& portal) { return portal.has_value(); }),
            "portals close when the reinforcement countdown ends");
        for (const auto& event : cleanup->takeEvents())
            require(event.type != GameEventType::EnemyBanished && event.type != GameEventType::WaveCompleted &&
                event.type != GameEventType::WaveRewardGranted,
                "dawn neither banishes enemies nor grants completion rewards early");
        const Vec3 before = cleanup->snapshot().enemies.front().position;
        cleanup->tick(2);
        const Vec3 after = cleanup->snapshot().enemies.front().position;
        require(cleanup->snapshot().activeEnemyCount == survivors.size() &&
            (before.x != after.x || before.z != after.z),
            "survivors continue moving after dawn without new arrivals");
        int killsAfterDawn = 0, xpAfterDawn = 0, completions = 0, rewards = 0;
        for (const EntityId id : survivors) {
            PlayerCommand attack;
            attack.castChainLightning = CastChainLightningCommand{
                .firstTarget = id, .damage = 1e12, .maximumTargets = 1};
            cleanup->tick(0, attack);
            for (const auto& event : cleanup->takeEvents()) {
                if (event.type == GameEventType::EnemyKilled) ++killsAfterDawn;
                if (event.type == GameEventType::ExperienceCollected) xpAfterDawn += event.amount;
                if (event.type == GameEventType::WaveCompleted) ++completions;
                if (event.type == GameEventType::WaveRewardGranted) ++rewards;
            }
            while (cleanup->snapshot().runUpgradeChoicePending)
                require(cleanup->selectRunUpgrade(0), "cleanup fixture resolves earned XP choices");
            if (cleanup->snapshot().activeEnemyCount > 0) {
                require(cleanup->snapshot().state == RunState::Wave &&
                    !cleanup->snapshot().runShop.pending,
                    "partial cleanup neither ends the wave nor opens the shop");
                cleanup->tick(.25);
                require(cleanup->snapshot().activeEnemyCount < survivors.size(),
                    "kills after dawn never request replacement groups");
            }
        }
        for (const auto& pickup : cleanup->snapshot().coinPickups)
            if (pickup.kind == PickupKind::Experience) xpAfterDawn += pickup.value;
        require(killsAfterDawn == static_cast<int>(survivors.size()) &&
            xpAfterDawn == killsAfterDawn * static_cast<int>(std::lround(cleanup->insightSystem().config().enemy[0])),
            "all post-countdown kills retain their ordinary XP rewards");
        require(cleanup->snapshot().state == RunState::WaveComplete &&
            cleanup->snapshot().activeEnemyCount == 0 && !cleanup->snapshot().runShop.pending &&
            completions == 1 && rewards == 1,
            "the last kill completes the night and grants its reward once without opening the disabled shop");
        cleanup->tick(1);
        for (const auto& event : cleanup->takeEvents())
            require(event.type != GameEventType::WaveCompleted && event.type != GameEventType::WaveRewardGranted,
                "completed cleanup cannot pay the reward twice");
    }
    {
        auto splitCleanup = fixture(12.0);
        startNight(*splitCleanup);
        splitCleanup->tick(12);
        PlayerCommand summon;
        summon.spawnEnemy = SpawnEnemyCommand{.type = EnemyType::Splitter, .count = 1};
        splitCleanup->tick(0, summon);
        std::vector<EntityId> targets;
        std::optional<EntityId> parent;
        for (const auto& enemy : splitCleanup->snapshot().enemies) {
            if (!enemy.active) continue;
            if (enemy.type == EnemyType::Splitter) parent = enemy.id;
            else targets.push_back(enemy.id);
        }
        require(parent.has_value(), "cleanup fixture spawns a splitter");
        targets.push_back(*parent);
        for (const EntityId id : targets) {
            PlayerCommand attack;
            attack.castChainLightning = CastChainLightningCommand{
                .firstTarget = id, .damage = 1e12, .maximumTargets = 1};
            splitCleanup->tick(0, attack);
            resolveChoices(*splitCleanup);
        }
        require(splitCleanup->snapshot().activeEnemyCount == 0 &&
            splitCleanup->snapshot().state == RunState::Wave && !splitCleanup->snapshot().runShop.pending,
            "a dying last splitter keeps the night active until its delayed children arrive");
        splitCleanup->tick(.4);
        require(splitCleanup->snapshot().activeEnemyCount == 3 &&
            std::ranges::all_of(splitCleanup->snapshot().enemies, [](const auto& enemy) {
                return !enemy.active || enemy.type == EnemyType::Splitling;
            }), "cleanup permits splitter children without resuming portal reinforcements");
        targets.clear();
        for (const auto& enemy : splitCleanup->snapshot().enemies)
            if (enemy.active) targets.push_back(enemy.id);
        for (const EntityId id : targets) {
            PlayerCommand attack;
            attack.castChainLightning = CastChainLightningCommand{
                .firstTarget = id, .damage = 1e12, .maximumTargets = 1};
            splitCleanup->tick(0, attack);
            while (splitCleanup->snapshot().runUpgradeChoicePending)
                require(splitCleanup->selectRunUpgrade(0), "split cleanup resolves XP choices");
        }
        require(splitCleanup->snapshot().state == RunState::WaveComplete,
            "killing all delayed children completes the night");
    }
    {
        auto pressure = fixture(12.0);
        auto checkpoint = pressure->saveSuspendedRunState();
        checkpoint.wave = checkpoint.bestWave = 10;
        pressure->returnToMainMenu();
        require(pressure->loadSuspendedRunState(checkpoint), "endless pressure checkpoint loads");
        PlayerCommand god;
        god.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
        pressure->tick(0, god);
        startNight(*pressure);
        pressure->tick(12);
        const auto alive = pressure->snapshot().activeEnemyCount;
        pressure->tick(3);
        require(pressure->snapshot().state == RunState::Wave &&
            pressure->snapshot().finalNight && pressure->snapshot().phaseDuration==0 &&
            pressure->snapshot().activeEnemyCount > alive,
            "night eleven keeps replenishing enemies beyond the ordinary dawn time");
        requireNear(pressure->snapshot().overtimeSeconds, 15.0, 1e-12,
            "continuous night keeps advancing overtime after its former duration");
    }

    auto strong = fixture();
    const auto checkpoint = strong->saveSuspendedRunState();
    const auto forecast = strong->snapshot().upcomingEnemyCounts;
    auto restored = fixture();
    restored->returnToMainMenu();
    require(restored->loadSuspendedRunState(checkpoint) && restored->snapshot().upcomingEnemyCounts == forecast,
        "checkpoint preserves the run's upcoming enemy forecast");
    startNight(*strong);
    strong->takeEvents();
    int kills = 0, collectedXp = 0;
    std::size_t peakAlive = 0;
    for (int frame = 0; frame < 600; ++frame) {
        PlayerCommand attack;
        const auto& snapshot = strong->snapshot();
        const auto target = std::ranges::find_if(snapshot.enemies, [](const auto& enemy) { return enemy.active; });
        if (frame % 6 == 0 && target != snapshot.enemies.end()) {
            attack.castChainLightning = CastChainLightningCommand{
                .firstTarget = target->id, .damage = 1e9, .jumpRadius = 1000,
                .damageFalloff = 1, .maximumTargets = 128};
        }
        strong->tick(1.0 / 60, attack);
        peakAlive = std::max(peakAlive, strong->snapshot().activeEnemyCount);
        for (const auto& event : strong->takeEvents()) {
            if (event.type == GameEventType::EnemyKilled) ++kills;
            if (event.type == GameEventType::ExperienceCollected) collectedXp += event.amount;
        }
        resolveChoices(*strong);
    }
    std::cout << "rapid-clear kills=" << kills << " peak alive=" << peakAlive << '\n';
    require(kills > 60, "strong defense earns many more kills than the old 21-enemy ten-second cadence");
    int remainingXp = 0;
    for (const auto& pickup : strong->snapshot().coinPickups)
        if (pickup.kind == PickupKind::Experience) remainingXp += pickup.value;
    require(remainingXp + collectedXp == kills * static_cast<int>(std::lround(strong->insightSystem().config().enemy[0])),
        "rapid replacement and enemy-slot reuse preserve the full XP reward for every kill");
    require(peakAlive <= 16 && strong->snapshot().state == RunState::Wave,
        "rapid replenishment stays bounded and clearing enemies does not prematurely end the night");

    PlayerCommand god; god.enableUnlimitedResources = EnableUnlimitedResourcesCommand{};
    restored->tick(0, god);
    startNight(*restored);
    restored->tick(10);
    require(restored->snapshot().activeEnemyCount <= 16,
        "weak defense does not accrue an unbounded backlog of attackers");
    PlayerCommand clear; clear.defeatAllEnemies = DefeatAllEnemiesCommand{};
    restored->tick(0, clear);
    require(restored->snapshot().state == RunState::WaveComplete && restored->snapshot().activeEnemyCount == 0,
        "debug completion cannot accidentally trigger replacement spawning");

    // Exercise actual act transitions and multiple queue refills on a boss night.
    for (int wave = 2; wave <= WaveDirector::BossWaveInterval; ++wave) {
        resolveChoices(*restored);
        restored->tick(restored->snapshot().phaseDuration);
        startNight(*restored);
        if (wave < WaveDirector::BossWaveInterval) restored->tick(0, clear);
    }
    int acts = 0, bossKills = 0, warnings = 0;
    restored->takeEvents();
    for (int step = 0; step < 400 && restored->snapshot().state == RunState::Wave; ++step) {
        acts |= 1 << restored->snapshot().currentNightAct;
        const auto& enemies = restored->snapshot().enemies;
        const auto target = std::ranges::find_if(enemies, [](const auto& enemy) { return enemy.active; });
        PlayerCommand attack;
        if (target != enemies.end()) attack.castChainLightning = CastChainLightningCommand{
            .firstTarget = target->id, .damage = 1e12, .jumpRadius = 1000,
            .damageFalloff = 1, .maximumTargets = 128};
        restored->tick(.25, attack);
        for (const auto& event : restored->takeEvents()) {
            if (event.type == GameEventType::EnemyKilled && event.enemyType == EnemyType::Boss) ++bossKills;
            if (event.type == GameEventType::AttackDirectionWarned && event.critical) ++warnings;
        }
        resolveChoices(*restored);
    }
    require(acts == 7 && warnings == 2 && bossKills == 1 && restored->snapshot().state == RunState::WaveComplete,
        "boss night changes through three warned acts, replenishes groups and never repeats its boss");
}
