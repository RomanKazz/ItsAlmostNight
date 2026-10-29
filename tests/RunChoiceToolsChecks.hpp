#pragma once
#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "progression/ContentUnlocks.hpp"
#include <filesystem>
#include <memory>

inline void checkRunChoiceTools() {
    using namespace ian;
    MetaProgression profile; profile.silver=100;
    profile.quests.ownedUnlocks={"building.mortar","item.ghost_candle","meta.toggler"};
    require(!toggleContent(profile,"building.mortar"),"pool toggles require the purchased Toggler");
    require(purchaseMetaShopUpgrade(profile,MetaShopUpgrade::Toggler)==MetaShopPurchaseResult::Purchased && profile.silver==50,
        "Toggler is one permanent fifty-silver purchase");
    require(!toggleContent(profile,"building.gun_turret") && !toggleContent(profile,"building.laser"),
        "default and unowned content cannot be disabled");
    const auto original=runContentAccess(profile);
    require(toggleContent(profile,"building.mortar") && toggleContent(profile,"item.ghost_candle"),
        "owned exotic defenses and items can be disabled");
    const auto disabled=runContentAccess(profile);
    require(original.cards[runUpgradeIndex(RunUpgradeEffect::UnlockMortar)] &&
        !disabled.cards[runUpgradeIndex(RunUpgradeEffect::UnlockMortar)] &&
        !disabled.items[lootUpgradeIndex(LootUpgradeEffect::GhostCandle)],"toggles remove content from future pools");
    const auto path=std::filesystem::temp_directory_path()/"ian-choice-tools-profile.json";
    require(saveMetaProgression(path.string(),profile),"toggle choices save");
    MetaProgression loaded;
    require(loadMetaProgression(path.string(),loaded) && loaded==profile &&
        !runContentAccess(loaded).items[lootUpgradeIndex(LootUpgradeEffect::GhostCandle)],"toggles survive profile reload");
    require(toggleContent(loaded,"building.mortar") && runContentAccess(loaded).cards[runUpgradeIndex(RunUpgradeEffect::UnlockMortar)],
        "disabled content can be re-enabled without buying it again");
    std::filesystem::remove(path);

    auto map=MapDefinition::defaults();map.resources.clear();map.obstacles.clear();
    auto world=WorldConfig::defaults();world.terrainResolution=33;world.terrainAmplitude=world.terrainSurfaceNoiseAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    auto run=std::make_unique<Simulation>(GameBalance::defaults(),map,world);
    MetaRunBonuses bonuses;bonuses.rerolls=2;bonuses.skips=2;bonuses.banishes=2;
    require(run->configureNextRunBonuses(bonuses),"utility budgets configure the next run");
    run->startRunFromSeed(PlayerClass::Miner,71);
    require(!run->skipRunUpgrade() && !run->banishRunUpgrade(0),"tools cannot spend uses without an offer");
    auto saved=run->saveSuspendedRunState();
    saved.coins=0;saved.runUpgradeChoicePending=true;saved.runUpgradeChoiceCount=4;saved.runUpgradeSelectionsRemaining=2;
    saved.runUpgradePaidChoicePrice=20;
    saved.runUpgradeChoices={RunUpgradeEffect::Damage,RunUpgradeEffect::Luck,RunUpgradeEffect::MoveSpeed,RunUpgradeEffect::AttackSize};
    saved.runUpgradeRarities.fill(RunUpgradeRarity::Common);
    run->returnToMainMenu();require(run->loadSuspendedRunState(saved),"queued utility offers restore");
    const auto stacks=run->snapshot().runUpgradeStacks;
    require(!run->banishRunUpgrade(99) && run->snapshot().runUpgradeBanishTokens==2,"invalid banish never spends a charge");
    require(run->banishRunUpgrade(3) && run->snapshot().coins==0 && run->snapshot().runUpgradeBanishTokens==1 &&
        run->snapshot().runUpgradeSelectionsRemaining==1 && run->snapshot().runUpgradeStacks==stacks &&
        run->snapshot().banishedRunUpgrades[runUpgradeIndex(RunUpgradeEffect::AttackSize)],
        "banish consumes one choice and use, excludes even a paid card, and grants no upgrade");
    for(std::size_t i=0;i<run->snapshot().runUpgradeChoiceCount;++i)
        require(run->snapshot().runUpgradeChoices[i]!=RunUpgradeEffect::AttackSize,"queued choices respect the new banish");
    require(run->skipRunUpgrade() && !run->snapshot().runUpgradeChoicePending && run->snapshot().runUpgradeSkipTokens==1 &&
        run->snapshot().runUpgradeStacks==stacks,"skip consumes the next choice without gaining or banning any upgrade");
    auto spent=run->saveSuspendedRunState();
    run->returnToMainMenu();require(run->loadSuspendedRunState(spent),"spent tools and bans restore");
    require(run->snapshot().runUpgradeBanishTokens==1 && run->snapshot().runUpgradeSkipTokens==1 &&
        run->snapshot().banishedRunUpgrades[runUpgradeIndex(RunUpgradeEffect::AttackSize)],"resume never replenishes tools or forgets bans");
    spent.runUpgradeChoicePending=true;spent.runUpgradeChoiceCount=1;spent.runUpgradeSelectionsRemaining=1;
    spent.runUpgradeChoices[0]=RunUpgradeEffect::AttackSize;
    run->returnToMainMenu();require(run->loadSuspendedRunState(spent),"old offered banished card is sanitized on resume");
    for(int reroll=0;reroll<2;++reroll) {
        require(run->rerollRunUpgrades(),"purchased reroll uses remain independent");
        for(std::size_t i=0;i<run->snapshot().runUpgradeChoiceCount;++i)
            require(run->snapshot().runUpgradeChoices[i]!=RunUpgradeEffect::AttackSize,"rerolls cannot bring a banned card back");
    }
    auto corrupt=run->saveSuspendedRunState();corrupt.runUpgradeSkipTokens=-1;
    run->returnToMainMenu();require(!run->loadSuspendedRunState(corrupt),"negative saved tool budgets are rejected");
    run->startRunFromSeed(PlayerClass::Miner,72);
    require(run->snapshot().runUpgradeSkipTokens==2 && run->snapshot().runUpgradeBanishTokens==2 &&
        !run->snapshot().banishedRunUpgrades[runUpgradeIndex(RunUpgradeEffect::AttackSize)],"a new run resets bans and refreshes purchased uses");
}
