#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include <memory>
#include <set>
#include <limits>

namespace {
void chooseLevels(ian::Simulation& run) {
    while (run.snapshot().runUpgradeChoicePending)
        require(run.selectRunUpgrade(0), "shop fixture resolves level rewards first");
}
}
void runRunShopTests() {
    auto map = ian::MapDefinition::defaults(); map.resources.clear(); map.obstacles.clear();
    auto world = ian::WorldConfig::defaults(); world.terrainAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto balance = ian::GameBalance::defaults();
    balance.buildings[static_cast<std::size_t>(ian::BuildingType::Core)].wood = 0;
    auto run = std::make_unique<ian::Simulation>(balance,map,world);
    run->startRunFromSeed(ian::PlayerClass::Vanguard,12345);
    require(!run->openRunShop(), "shop unavailable before the first completed wave");
    require(run->openRunShop(true), "debug shortcut can inspect the shop before the first wave");
    require(run->snapshot().runShop.pending && run->snapshot().runShop.offers[0].present,
        "debug opening generates real offers instead of a blank shop");
    const auto earlyShop = run->saveSuspendedRunState();
    auto earlyResume = std::make_unique<ian::Simulation>(balance,map,world);
    require(earlyResume->loadSuspendedRunState(earlyShop) && earlyResume->snapshot().runShop.pending,
        "pre-wave debug shop can be saved and restored");
    require(run->closeRunShop(), "debug shop closes before ordinary preparation");
    ian::PlayerCommand core; core.placeBuilding = ian::PlaceBuildingCommand{ian::BuildingType::Core,{0,0},0};
    run->tick(0,core); chooseLevels(*run);
    ian::PlayerCommand start; start.startWaveEarly = ian::StartWaveEarlyCommand{};
    run->tick(0,start);
    require(run->openRunShop(true), "debug shortcut can inspect the shop during an active wave");
    const auto waveTime = run->snapshot().phaseTimeRemaining;
    run->tick(1);
    requireNear(run->snapshot().phaseTimeRemaining,waveTime,1e-12,"debug shop freezes active combat");
    require(run->closeRunShop() && run->snapshot().state == ian::RunState::Wave,
        "closing a combat shop resumes the same wave");
    ian::PlayerCommand clear; clear.defeatAllEnemies = ian::DefeatAllEnemiesCommand{};
    run->tick(0,clear); chooseLevels(*run);
    require(run->snapshot().runShop.pending && run->snapshot().runShop.wave == 1,
        "a completed regular wave opens the four-slot shop after level rewards");
    const double pausedTime = run->snapshot().phaseTimeRemaining;
    run->tick(1);
    requireNear(run->snapshot().phaseTimeRemaining,pausedTime,1e-12,"shop freezes the preparation clock");
    std::set<ian::RunUpgradeEffect> effects;
    for (std::size_t i = 0; i < ian::RunShopSlotCount; ++i) {
        const auto& offer = run->snapshot().runShop.offers[i];
        require(offer.present && run->snapshot().runShopBuyable[i],"shop uses the eligible level-up card pool");
        require(effects.insert(offer.effect).second,"shop has four distinct offers");
    }
    const auto poor = run->snapshot().runShop;
    require(!run->buyRunShopOffer(0) && !run->rerollRunShop(),"insufficient coins reject purchases and rerolls");
    require(run->snapshot().runShop == poor && run->snapshot().coins == 0,"failed spending is atomic");
    auto saved = run->saveSuspendedRunState(); saved.coins = 10000;
    run->returnToMainMenu(); require(run->loadSuspendedRunState(saved),"shop save restores before spending");
    require(run->toggleRunShopLock(0),"each offer supports a free lock");
    const auto locked = run->snapshot().runShop.offers[0];
    const int tokens = run->snapshot().runUpgradeRerollTokens;
    const int reroll = ian::runShopRerollPrice(run->snapshot().runShop);
    require(run->rerollRunShop(),"paid reroll refreshes unlocked slots");
    require(run->snapshot().coins == 10000-reroll && run->snapshot().runShop.offers[0] == locked &&
        run->snapshot().runUpgradeRerollTokens == tokens,"reroll spends only coins and preserves locked price/rarity");
    require(ian::runShopRerollPrice(run->snapshot().runShop) > reroll,"reroll price increases within the current visit");
    saved = run->saveSuspendedRunState();
    auto resumed = std::make_unique<ian::Simulation>(balance,map,world);
    require(resumed->loadSuspendedRunState(saved) && resumed->snapshot().runShop == run->snapshot().runShop,
        "save/load preserves shop offers, locks, prices and reroll counter");
    require(run->rerollRunShop() && resumed->rerollRunShop() &&
        run->snapshot().runShop == resumed->snapshot().runShop,"reroll RNG continues deterministically after load");
    const auto selected = run->snapshot().runShop.offers[0];
    const auto before = run->snapshot().runUpgradeStacks;
    const int coins = run->snapshot().coins;
    require(run->buyRunShopOffer(0),"shop purchase applies the existing upgrade hit/stat flow");
    require(run->snapshot().coins == coins-selected.price && !run->snapshot().runShop.offers[0].present &&
        run->snapshot().runUpgradeStacks[ian::runUpgradeIndex(selected.effect)] > before[ian::runUpgradeIndex(selected.effect)],
        "purchased offer disappears and applies once");
    require(!run->buyRunShopOffer(0),"a sold slot cannot charge or apply twice");
    require(run->closeRunShop() && run->openRunShop(),"daytime reopening keeps the same visit");
    require(!run->snapshot().runShop.offers[0].present,"reopening is not a free reroll");
    resumed->returnToMainMenu();
    auto invalid = run->saveSuspendedRunState(); invalid.runShop.offers[1].price = -1;
    require(!resumed->loadSuspendedRunState(invalid),"malformed shop price is rejected");
    invalid = run->saveSuspendedRunState(); invalid.runShop.offers[2] = invalid.runShop.offers[1];
    require(!resumed->loadSuspendedRunState(invalid),"duplicate offers are rejected on load");
    // Known cards isolate buy-out and tome-limit behavior from weighted randomness.
    saved = run->saveSuspendedRunState(); saved.runShop.pending = true; saved.runShop.rerolls = 7;
    saved.runUpgradeStacks.fill(0); saved.runUpgradePower.fill(0);
    const std::array cards{ian::RunUpgradeEffect::Damage,ian::RunUpgradeEffect::MoveSpeed,
        ian::RunUpgradeEffect::MaximumHealth,ian::RunUpgradeEffect::RecoverableArmor};
    for (std::size_t i=0;i<cards.size();++i) saved.runShop.offers[i] = {
        .effect=cards[i],.rarity=ian::RunUpgradeRarity::Common,.price=1,.present=true};
    resumed->returnToMainMenu();
    require(resumed->loadSuspendedRunState(saved),"buy-out fixture loads four known cards");
    const auto generation = resumed->snapshot().runShop.generation;
    const int beforeBuyout = resumed->snapshot().coins;
    for (std::size_t i=0;i<4;++i) require(resumed->buyRunShopOffer(i),"four purchases fill the four tome slots");
    require(resumed->snapshot().runShop.generation == generation+1 && resumed->snapshot().runShop.rerolls == 7 &&
        resumed->snapshot().coins == beforeBuyout-4,"buy-out refresh is free and does not advance paid reroll pricing");
    for (std::size_t i = 0; i < ian::RunShopSlotCount; ++i)
        require(!resumed->snapshot().runShop.offers[i].present || resumed->snapshot().runShopBuyable[i],"refresh respects acquired tome slots and unique unlocks");
    saved = resumed->saveSuspendedRunState();
    for (std::size_t i=0;i<cards.size();++i) saved.runShop.offers[i] = {
        .effect=cards[i],.rarity=ian::RunUpgradeRarity::Common,.price=1,.present=true,.locked=true};
    resumed->returnToMainMenu();
    require(resumed->loadSuspendedRunState(saved), "four locked offers restore");
    const auto carried = resumed->snapshot().runShop.offers;
    const int lockedCoins = resumed->snapshot().coins;
    require(!resumed->rerollRunShop() && resumed->snapshot().coins == lockedCoins,
        "all locked offers prevent charging for a no-op reroll");
    require(resumed->closeRunShop(), "next-wave fixture leaves the shop");
    resumed->tick(resumed->snapshot().phaseDuration);
    resumed->tick(0,start); resumed->tick(0,clear); chooseLevels(*resumed);
    require(resumed->snapshot().runShop.wave == 2 && resumed->snapshot().runShop.rerolls == 0 &&
        resumed->snapshot().runShop.offers == carried,
        "locks retain exact offers and prices across waves while reroll price resets");
    saved = resumed->saveSuspendedRunState();
    saved.runShop.offers[0] = {.effect=ian::RunUpgradeEffect::Luck,.price=1,.present=true};
    for (std::size_t i=1;i<4;++i) saved.runShop.offers[i] = {};
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(saved),"stale tome offer fixture loads");
    require(!run->snapshot().runShopBuyable[0] && !run->buyRunShopOffer(0),"shop cannot bypass the four tome slot limit");
    require(ian::runShopRerollPrice({.wave=std::numeric_limits<int>::max(),.rerolls=std::numeric_limits<int>::max()}) > 0,
        "large shop counters do not overflow price arithmetic");
    run->returnToMainMenu(); run->startRunFromSeed(ian::PlayerClass::Vanguard,12345);
    require(run->snapshot().runShop.wave == 0 && !run->snapshot().runShop.pending,"new run resets shop and locked inventory");
    ian::PlayerCommand godMode; godMode.enableUnlimitedResources = ian::EnableUnlimitedResourcesCommand{};
    run->tick(0,godMode);
    require(run->snapshot().unlimitedResources && run->snapshot().coins == 0,
        "godmode shop fixture has unlimited resources and no coins");
    require(run->openRunShop(true), "godmode can open the debug shop");
    const auto godOffer = run->snapshot().runShop.offers[0];
    const auto godStacks = run->snapshot().runUpgradeStacks;
    require(run->buyRunShopOffer(0) && run->snapshot().coins == 0 &&
        !run->snapshot().runShop.offers[0].present &&
        run->snapshot().runUpgradeStacks[ian::runUpgradeIndex(godOffer.effect)] > godStacks[ian::runUpgradeIndex(godOffer.effect)],
        "godmode purchases apply upgrades once without requiring or spending coins");
    const auto godGeneration = run->snapshot().runShop.generation;
    require(run->rerollRunShop() && run->snapshot().coins == 0 &&
        run->snapshot().runShop.generation == godGeneration+1,
        "godmode rerolls refresh offers without requiring or spending coins");
    require(run->closeRunShop(), "godmode shop closes before toggling off");
    run->tick(0,godMode);
    require(!run->snapshot().unlimitedResources && run->openRunShop(true),
        "godmode can be disabled without resetting the shop");
    const auto pricedShop = run->snapshot().runShop;
    require(!run->buyRunShopOffer(0) && !run->rerollRunShop() &&
        run->snapshot().coins == 0 && run->snapshot().runShop == pricedShop,
        "disabling godmode immediately restores normal costs and atomic rejection");
    auto fundedGodShop = run->saveSuspendedRunState();
    fundedGodShop.runShop.pending = false; fundedGodShop.coins = 50;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(fundedGodShop), "funded godmode fixture restores coins");
    run->tick(0,godMode);
    require(run->openRunShop(true) && run->buyRunShopOffer(0) &&
        run->rerollRunShop() && run->snapshot().coins == 50,
        "godmode does not spend existing coins on purchases or rerolls");
}

#ifdef IAN_RUN_SHOP_TEST_MAIN
int main() { runRunShopTests(); }
#endif
