#pragma once

#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "progression/QuestSystem.hpp"
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

inline void checkRetiredBuildingBranches() {
    using namespace ian;
    auto run = std::make_unique<Simulation>();
    run->startRunFromSeed(PlayerClass::Vanguard, 93627);
    const auto initial = run->saveSuspendedRunState();
    for (const auto effect : {RunUpgradeEffect::ClusterPayload, RunUpgradeEffect::ReactiveTraps,
            RunUpgradeEffect::GunShrapnel, RunUpgradeEffect::CannonHeavyShell,
            RunUpgradeEffect::CannonScattershot, RunUpgradeEffect::CannonClusterShell}) {
        require(legacyBuildingBranchUpgrade(effect) && retiredRunUpgrade(effect),
            "standalone building branches are retired card IDs");
        auto legacy = initial;
        const auto index = runUpgradeIndex(effect);
        legacy.runUpgradeStacks[index] = 1;
        legacy.runUpgradePower[index] = 1;
        legacy.runUpgradeChoicePending = true;
        legacy.runUpgradeChoiceCount = 1;
        legacy.runUpgradeSelectionsRemaining = 1;
        legacy.runUpgradeRerollTokens = 1;
        legacy.runUpgradeChoices[0] = effect;
        legacy.runUpgradeRarities.fill(RunUpgradeRarity::Common);
        legacy.runShop.pending = true;
        legacy.runShop.offers = {};
        legacy.runShop.offers[0] = {.effect=effect, .rarity=RunUpgradeRarity::Common,
            .price=12, .present=true, .locked=true};
        legacy.runShop.offers[1] = {.effect=RunUpgradeEffect::Damage, .rarity=RunUpgradeRarity::Common,
            .price=12, .present=true};
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(legacy), "saved building branches remain loadable");
        auto resumed = run->saveSuspendedRunState();
        require(resumed.runUpgradeStacks[index] == 1 && resumed.runUpgradePower[index] == 1,
            "already earned branch powers survive resume");
        require(resumed.runUpgradeChoicePending && resumed.runUpgradeChoiceCount > 0,
            "obsolete pending branch card is replaced by active choices");
        for (std::size_t i = 0; i < resumed.runUpgradeChoiceCount; ++i)
            require(!legacyBuildingBranchUpgrade(resumed.runUpgradeChoices[i]),
                "replacement level choices exclude every building branch");
        require(!resumed.runShop.offers[0].present && !resumed.runShop.offers[0].locked &&
                resumed.runShop.offers[1].present && resumed.runShop.offers[1].effect == RunUpgradeEffect::Damage,
            "resume removes retired locked shop offers and preserves active offers");
        require(run->rerollRunUpgrades(), "replacement offer rerolls normally");
        for (std::size_t i = 0; i < run->snapshot().runUpgradeChoiceCount; ++i)
            require(!legacyBuildingBranchUpgrade(run->snapshot().runUpgradeChoices[i]),
                "rerolls cannot restore building branches");
    }
    require(!retiredRunUpgrade(RunUpgradeEffect::UnlockCannon) &&
            !retiredRunUpgrade(RunUpgradeEffect::UnlockSpikeTrap) &&
            !retiredRunUpgrade(RunUpgradeEffect::ProjectileCount),
        "building levels and general attack tomes remain active");
    for (const auto& quest : QuestSystem::defaultDefinitions())
        require(quest.id != "upgrade_cluster", "fallback quest catalog drops the unattainable cluster card objective");
}
