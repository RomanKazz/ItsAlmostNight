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
std::unique_ptr<Simulation> fixture() {
    auto world = WorldConfig::defaults();
    world.terrainResolution = 33;
    world.terrainAmplitude = 0;
    world.terrainSurfaceNoiseAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto map = MapDefinition::defaults();
    map.obstacles.clear(); map.resources.clear();
    auto result = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
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
    for (int wave : {6, 12, 1000000}) {
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
            require(bosses == (wave % 6 == 0 ? 1 : 0), "boss schedule is preserved");
        }
    }
    require(nightAct(.399) == 0 && nightAct(.4) == 1 && nightAct(.8) == 2 &&
        nightRegrouping(.38) && !nightRegrouping(.4) && nightRegrouping(.78),
        "brief warning windows precede each act change");

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
    for (int wave = 2; wave <= 6; ++wave) {
        resolveChoices(*restored);
        restored->tick(restored->snapshot().phaseDuration);
        startNight(*restored);
        if (wave < 6) restored->tick(0, clear);
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
