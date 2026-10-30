#pragma once

#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include <memory>

inline void checkExperienceTome() {
    using namespace ian;
    auto world = WorldConfig::defaults();
    world.terrainResolution = 33;
    world.terrainAmplitude = world.terrainSurfaceNoiseAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto map = MapDefinition::defaults();
    map.resources.clear(); map.obstacles.clear();
    auto run = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
    run->startRunFromSeed(PlayerClass::Vanguard, 31923);
    const auto restore = [&](const SuspendedRunState& state) {
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(state), "experience tome checkpoint restores");
    };
    const auto index = runUpgradeIndex(RunUpgradeEffect::ExperienceGain);
    auto offer = run->saveSuspendedRunState();
    offer.runUpgradeChoicePending = true;
    offer.runUpgradeChoiceCount = 1;
    offer.runUpgradeSelectionsRemaining = 1;
    offer.runUpgradeChoices[0] = RunUpgradeEffect::ExperienceGain;
    offer.runUpgradeRarities[0] = RunUpgradeRarity::Common;
    restore(offer);
    require(run->selectRunUpgrade(0), "experience tome can be chosen as a normal level reward");
    require(run->snapshot().tomeCount == 1 && run->snapshot().runUpgradeStacks[index] == 1,
        "experience tome occupies one normal tome slot");
    requireNear(run->snapshot().upgradeStats.experienceGain, 1.10, 1e-12,
        "common experience tome gives ten percent extra experience");
    for (int grant = 0; grant < 10; ++grant) {
        PlayerCommand command;
        command.grantInsight = GrantInsightCommand{1.0};
        run->tick(0, command);
    }
    requireNear(run->snapshot().totalInsightEarned, 11.0, 1e-10,
        "ten small experience pickups retain the fractional ten-percent bonus");
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::ExperienceGain, RunUpgradeRarity::Rare) &&
        run->grantDebugRunUpgrade(RunUpgradeEffect::ExperienceGain, RunUpgradeRarity::Legendary),
        "experience tome accepts rarity-scaled additional copies");
    requireNear(run->snapshot().upgradeStats.experienceGain, 1.44, 1e-12,
        "common, rare and legendary experience bonuses add instead of compounding");
    const auto saved = run->saveSuspendedRunState();
    restore(saved);
    restore(run->saveSuspendedRunState());
    requireNear(run->snapshot().upgradeStats.experienceGain, 1.44, 1e-12,
        "repeated resume does not reapply or lose the experience multiplier");
    requireNear(run->snapshot().totalInsightEarned, 11.0, 1e-10,
        "obtaining and restoring the tome never awards retroactive experience");
    auto blessed = run->saveSuspendedRunState();
    const auto shrine = std::ranges::find_if(blessed.worldLandmarks, [](const auto& landmark) {
        return landmark.type == WorldLandmarkType::PlayerShrine;
    });
    require(shrine != blessed.worldLandmarks.end(), "experience fixture has a player shrine");
    shrine->activated = true;
    blessed.shrines.activations = 1;
    blessed.shrines.playerBonuses[index] = 1;
    blessed.shrines.playerPowers[index] = 1.4;
    restore(blessed);
    requireNear(run->snapshot().upgradeStats.experienceGain, 1.58, 1e-12,
        "experience blessing and tome share their additive bonus");
    PlayerCommand grant;
    grant.grantInsight = GrantInsightCommand{10.0};
    run->tick(0, grant);
    requireNear(run->snapshot().totalInsightEarned, 26.8, 1e-10,
        "actual experience grants include both the tome and shrine bonus");
    run->returnToMainMenu();
    run->startRunFromSeed(PlayerClass::Vanguard, 31923);
    requireNear(run->snapshot().upgradeStats.experienceGain, 1.0, 1e-12,
        "new runs reset experience bonuses");
    requireNear(run->snapshot().totalInsightEarned, 0.0, 1e-12,
        "new run starts without prior experience");
}
