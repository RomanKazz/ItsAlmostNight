#pragma once

#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "progression/RunUpgradeOfferSelection.hpp"

#include <memory>

inline void checkDefenseCardProgression() {
    using namespace ian;
    // Distinguish a uniform choice of types from a 50/50 owned/new group draw.
    std::array<int, RunUpgradeEffectCount> levels{};
    levels[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] = 7;
    const std::array pool{RunUpgradeEffect::UnlockGunTurret, RunUpgradeEffect::UnlockTurret,
        RunUpgradeEffect::UnlockCannon, RunUpgradeEffect::UnlockCatapult,
        RunUpgradeEffect::UnlockSpikeTrap, RunUpgradeEffect::UnlockSlowTrap};
    std::uint64_t random = 749123;
    std::array<int, pool.size()> draws{};
    for (int i = 0; i < 100000; ++i) ++draws[chooseRunUpgradeOffer(pool, levels, random)];
    require(draws[0] > 49000 && draws[0] < 51000, "defense offers split owned/new 50/50 with free slots");
    for (std::size_t i = 1; i < draws.size(); ++i)
        require(draws[i] > 9400 && draws[i] < 10600, "new defense group distributes types evenly");
    levels[runUpgradeIndex(RunUpgradeEffect::UnlockTurret)] = 1;
    const std::array ownedPool{RunUpgradeEffect::UnlockGunTurret, RunUpgradeEffect::UnlockTurret};
    int gunDraws = 0;
    for (int i = 0; i < 100000; ++i) gunDraws += chooseRunUpgradeOffer(ownedPool, levels, random) == 0;
    require(gunDraws > 49000 && gunDraws < 51000, "defense levels do not bias owned-only offers");
    for (const auto type : RunBuildingTypes) require(runUpgradeMaximumLevel(*buildingRunUpgrade(type)) == 8,
        "every combat card has eight levels");
    for (const auto type : RunEconomyBuildingTypes) require(runUpgradeMaximumLevel(*buildingRunUpgrade(type)) == 1,
        "producer acquisitions remain unique");

    BuildingSystem buildings;
    buildings.setCombatCardProgressionEnabled(true);
    auto core = buildings.place(BuildingType::Core, {0, 0}, 0, 99999, 99999, 99999);
    auto first = buildings.place(BuildingType::GunTurret, {2, 0}, 0, 99999, 99999, 99999);
    auto second = buildings.place(BuildingType::GunTurret, {4, 0}, 0, 99999, 99999, 99999);
    require(core && first && second, "shared-level fixture places two defenses");
    static_cast<void>(buildings.damage(first->building.id, 17));
    require(buildings.setCombatBlueprintLevel(BuildingType::GunTurret, 4), "global card upgrades existing copies");
    for (const auto& building : buildings.buildings()) if (building.type == BuildingType::GunTurret) {
        require(building.level == 4, "all copies inherit the shared defense level independent of core");
        requireNear(building.maxHealth - building.health, building.id == first->building.id ? 17 : 0,
            1e-9, "global level changes preserve absolute damage taken");
    }
    require(!buildings.setCombatBlueprintLevel(BuildingType::GunTurret, 4), "repeated runtime refresh is idempotent");
    auto future = buildings.place(BuildingType::GunTurret, {6, 0}, 0, 99999, 99999, 99999);
    require(future && future->building.level == 4, "future copies inherit shared level");
    require(buildings.validateUpgrade(first->building.id, 99999, 99999, 99999).error == UpgradeError::CoreLevelRequired,
        "combat management explains the core gate at the shared level ceiling");
    for (int i = 0; i < 4; ++i)
        require(buildings.upgrade(core->building.id, 99999, 99999, 99999).valid(), "core fixture reaches level five");
    require(buildings.validateUpgrade(first->building.id, 99999, 99999, 99999).error == UpgradeError::CardRequired,
        "resource upgrade cannot bypass combat cards after the core gate opens");
    require(buildings.validateUpgrade(core->building.id, 99999, 99999, 99999).valid(),
        "core keeps resource upgrades");
    require(!buildings.setCombatBlueprintLevel(BuildingType::Quarry, 4), "producers are not global card types");
    buildings.setCombatCardProgressionEnabled(false);
    require(buildings.upgrade(core->building.id, 99999, 99999, 99999).valid(), "sandbox retains manual progression");
    buildings.reset();
    buildings.setCombatCardProgressionEnabled(true);
    require(buildings.blueprintLevels()[static_cast<std::size_t>(BuildingType::GunTurret)] == 1,
        "reset clears shared placement levels");

    auto world = WorldConfig::defaults();
    world.terrainAmplitude = 0;
    auto map = MapDefinition::defaults();
    map.resources.clear();
    map.obstacles.clear();
    auto run = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
    run->startRunFromSeed(PlayerClass::Vanguard, 54321);
    auto saved = run->saveSuspendedRunState();
    saved.buildings = {
        {.id = {1000, 1}, .type = BuildingType::Core, .gridPosition = {0, 0}, .level = 3, .health = 500, .maxHealth = 500},
        {.id = {1001, 1}, .type = BuildingType::GunTurret, .gridPosition = {2, 0}, .health = 90, .maxHealth = 100},
        {.id = {1002, 1}, .type = BuildingType::GunTurret, .gridPosition = {4, 0}, .health = 100, .maxHealth = 100},
    };
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(saved), "shared combat fixture resumes");
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockGunTurret, RunUpgradeRarity::Legendary),
        "starter card upgrades rather than occupying another slot");
    require(run->snapshot().buildingArsenalCount == 1 &&
        run->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] == 2,
        "rarity cannot skip defense levels or spend slots");
    for (const auto& building : run->snapshot().buildings) if (building.type == BuildingType::GunTurret)
        require(building.level == 2, "simulation synchronizes every existing copy");
    const auto upgraded = run->saveSuspendedRunState();
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(upgraded), "shared defense save restores");
    require(run->saveSuspendedRunState().runUpgradeStacks == upgraded.runUpgradeStacks,
        "save/load does not duplicate defense card levels");
    require(run->openRunShop(true), "shared-level run opens its shop");
    auto shopping = run->saveSuspendedRunState();
    shopping.coins = 10;
    shopping.runShop.offers.fill({});
    shopping.runShop.offers[0] = {.effect = RunUpgradeEffect::UnlockGunTurret,
        .rarity = RunUpgradeRarity::Common, .price = 1, .present = true};
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(shopping) && run->snapshot().runShopBuyable[0] &&
        run->buyRunShopOffer(0), "shop buys repeated combat cards through the shared upgrade flow");
    require(run->snapshot().coins == 9 && run->snapshot().buildingArsenalCount == 1 &&
        run->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] == 3,
        "shop charges once, raises shared level and retains type slots");
    require(run->closeRunShop(), "combat shop closes");
    require(run->grantDebugRunUpgradeBatch(RunUpgradeEffect::UnlockGunTurret, 500000),
        "debug bulk can max a defense in one transaction");
    require(run->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] == 8 &&
        !run->grantDebugRunUpgradeBatch(RunUpgradeEffect::UnlockGunTurret, 1),
        "debug defense grants stop at authored level eight");
    for (const auto& building : run->snapshot().buildings) if (building.type == BuildingType::GunTurret)
        require(building.level == 8, "bulk grant updates every copy");

    auto legacy = upgraded;
    legacy.buildingArsenalVersion = 2;
    legacy.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] = 0;
    legacy.runUpgradePower[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] = 0;
    legacy.buildings[1].level = 3;
    legacy.buildings[2].level = 5;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(legacy), "individual legacy defense upgrades migrate");
    require(run->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] == 5,
        "legacy migration keeps the strongest built level");
    for (const auto& building : run->snapshot().buildings) if (building.type == BuildingType::GunTurret)
        require(building.level == 5, "legacy copies share migrated level");
    run->returnToMainMenu();
    run->startRunFromSeed(PlayerClass::Vanguard, 54321);
    require(run->snapshot().buildingArsenalCount == 1 &&
        run->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] == 1,
        "new run resets cards to class starter level one");
}
