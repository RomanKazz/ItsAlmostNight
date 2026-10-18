#pragma once
#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include <memory>

inline void checkBuildingCapacityProgression() {
    using namespace ian;
    require(runUpgradeIndex(RunUpgradeEffect::UnlockWindBlower) == 67 &&
        runUpgradeIndex(RunUpgradeEffect::BuildingCapacity) == 68 &&
        runUpgradeIndex(RunUpgradeEffect::ExtraBuilding) == 69,
        "capacity cards append save IDs without reordering the catalog");
    auto map = MapDefinition::defaults(); map.resources.clear(); map.obstacles.clear();
    auto world = WorldConfig::defaults(); world.terrainAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto run = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
    run->startRunFromSeed(PlayerClass::Vanguard, 3825);
    const auto restore = [&](SuspendedRunState state) {
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(state), "capacity run restores");
    };
    const auto choose = [&](RunUpgradeEffect effect, RunUpgradeRarity rarity) {
        auto offer = run->saveSuspendedRunState();
        offer.runUpgradeChoicePending = true; offer.runUpgradeChoiceCount = 3;
        offer.runUpgradeSelectionsRemaining = 1;
        offer.runUpgradeChoices = {effect, RunUpgradeEffect::MaximumHealth, RunUpgradeEffect::Luck};
        offer.runUpgradeRarities.fill(rarity);
        restore(offer);
        require(run->selectRunUpgrade(0), "capacity tome is selected through normal card flow");
    };
    require(run->snapshot().totalBuildingLimit == 3 && run->snapshot().placedBuildingCount == 0,
        "new run starts with three construction places");
    choose(RunUpgradeEffect::BuildingCapacity, RunUpgradeRarity::Epic);
    require(run->snapshot().totalBuildingLimit == 4 && run->snapshot().tomeCount == 1,
        "one 1.6-power tome grants one place and occupies one tome slot");
    choose(RunUpgradeEffect::BuildingCapacity, RunUpgradeRarity::Rare);
    requireNear(run->snapshot().runUpgradePower[runUpgradeIndex(RunUpgradeEffect::BuildingCapacity)],
        3.0, 1e-12, "fractional building power accumulates without per-card rounding");
    require(run->snapshot().totalBuildingLimit == 6 && run->snapshot().tomeCount == 1,
        "1.6 plus 1.4 grants three total places while retaining one tome");
    for (auto effect : {RunUpgradeEffect::Damage, RunUpgradeEffect::MoveSpeed, RunUpgradeEffect::Luck})
        require(run->grantDebugRunUpgrade(effect), "capacity fixture fills remaining tome slots");
    auto shopping = run->saveSuspendedRunState();
    shopping.runShop.pending = true; shopping.runShop.wave = 0;
    shopping.runShop.generation = 1; shopping.coins = 100;
    shopping.runShop.offers = {};
    shopping.runShop.offers[0] = {.effect = RunUpgradeEffect::ExtraBuilding,
        .rarity = RunUpgradeRarity::Common, .price = 12, .present = true};
    shopping.runShop.offers[1] = {.effect = RunUpgradeEffect::MaximumHealth,
        .rarity = RunUpgradeRarity::Common, .price = 12, .present = true};
    restore(shopping);
    const auto before = run->snapshot();
    require(run->buyRunShopOffer(0), "slot-free shop capacity can be purchased with all tome slots full");
    require(run->snapshot().totalBuildingLimit == before.totalBuildingLimit + 1 &&
        run->snapshot().coins == before.coins - 12 && run->snapshot().tomeCount == before.tomeCount &&
        run->snapshot().buildingArsenalCount == before.buildingArsenalCount,
        "shop purchase adds exactly one place, charges once and consumes neither slot type");
    const auto bought = run->saveSuspendedRunState();
    restore(bought);
    require(run->snapshot().totalBuildingLimit == 7 && run->snapshot().buildingLimitBonus == 4,
        "tome and shop capacity rebuild once after loading");
    require(run->closeRunShop(), "capacity fixture closes shop");
    choose(RunUpgradeEffect::Damage, RunUpgradeRarity::Common);
    auto rerolls = run->saveSuspendedRunState();
    rerolls.runUpgradeChoicePending = true; rerolls.runUpgradeChoiceCount = 3;
    rerolls.runUpgradeSelectionsRemaining = 1; rerolls.runUpgradeRerollTokens = 10;
    restore(rerolls);
    for (int reroll = 0; reroll < 10; ++reroll) {
        require(run->rerollRunUpgrades(), "capacity fixture rerolls normal level choices");
        for (std::size_t i = 0; i < run->snapshot().runUpgradeChoiceCount; ++i)
            require(run->snapshot().runUpgradeChoices[i] != RunUpgradeEffect::ExtraBuilding,
                "slot-free shop purchase never appears among level-up cards");
    }
    run->returnToMainMenu(); run->startRunFromSeed(PlayerClass::Vanguard, 3825);
    require(run->snapshot().totalBuildingLimit == 3 && run->snapshot().buildingLimitBonus == 0,
        "all previous capacity bonuses clear on a new run");
}
