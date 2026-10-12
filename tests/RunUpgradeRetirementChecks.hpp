#pragma once

#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include <memory>

inline void checkRetiredHighStakes() {
    using namespace ian;
    constexpr auto effect = RunUpgradeEffect::RiskyInvestment;
    const auto index = runUpgradeIndex(effect);
    require(retiredRunUpgrade(effect), "high stakes is excluded from upgrade pools");
    auto run = std::make_unique<Simulation>();
    run->startRunFromSeed(PlayerClass::Vanguard, 573219);
    require(!run->grantDebugRunUpgrade(effect) && !run->grantDebugRunUpgradeBatch(effect, 3),
        "debug grants cannot activate retired high stakes");
    auto legacy = run->saveSuspendedRunState();
    legacy.runUpgradeStacks[index] = 2;
    legacy.runUpgradePower[index] = 2;
    legacy.riskyInvestmentPending = 2;
    legacy.bonusSelectionsNextReward = 3;
    legacy.runUpgradeChoicePending = true;
    legacy.runUpgradeChoiceCount = 1;
    legacy.runUpgradeSelectionsRemaining = 1;
    legacy.runUpgradeChoices[0] = effect;
    legacy.runUpgradeRarities.fill(RunUpgradeRarity::Common);
    legacy.runShop.offers = {};
    legacy.runShop.offers[0] = {.effect=effect, .rarity=RunUpgradeRarity::Common,
        .price=18, .present=true, .locked=true};
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(legacy), "old high stakes checkpoint remains compatible");
    const auto resumed = run->saveSuspendedRunState();
    require(resumed.runUpgradeStacks[index] == 0 && resumed.runUpgradePower[index] == 0 &&
        resumed.riskyInvestmentPending == 0, "resume discards old copies and pending activation");
    require(resumed.bonusSelectionsNextReward == 3, "resume preserves already earned generic rewards");
    require(resumed.runUpgradeChoicePending && resumed.runUpgradeChoiceCount > 0,
        "resume replaces an obsolete pending reward");
    for (std::size_t i=0; i<resumed.runUpgradeChoiceCount; ++i)
        require(resumed.runUpgradeChoices[i] != effect, "replacement choices cannot offer high stakes");
    for (const auto& offer : resumed.runShop.offers)
        require(!offer.present || offer.effect != effect, "resume removes obsolete shop offers");
    require(!resumed.runShop.offers[0].locked && !run->buyRunShopOffer(0),
        "removed locked shop offer cannot be purchased");
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(resumed) && run->saveSuspendedRunState().riskyInvestmentPending == 0,
        "repeated resume cannot reactivate high stakes");
}
