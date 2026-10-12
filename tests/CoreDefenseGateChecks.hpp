#pragma once
#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include <memory>

inline void checkCoreDefenseGateAndRepairs() {
    using namespace ian;
    require(runDefenseLevelLimit(0) == 1 && runDefenseLevelLimit(4) == 4 &&
        runDefenseLevelLimit(8) == 8, "defense card ceiling is core level, with a pre-core floor of one");
    auto map = MapDefinition::defaults();
    map.resources.clear();
    map.obstacles.clear();
    auto world = WorldConfig::defaults();
    world.terrainAmplitude = 0;
    auto run = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
    run->startRunFromSeed(PlayerClass::Vanguard, 53218);
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockGunTurret),
        "pre-core starter cannot gain level two");
    for (const auto card : {RunUpgradeEffect::UnlockCannon, RunUpgradeEffect::UnlockTurret,
            RunUpgradeEffect::UnlockSpikeTrap})
        require(run->grantDebugRunUpgrade(card), "pre-core new combat types can be acquired at level one");
    auto saved = run->saveSuspendedRunState();
    const auto gun = runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret);
    saved.runUpgradeStacks[gun] = 4;
    saved.runUpgradePower[gun] = 4;
    saved.buildings = {
        {.id = {1000, 1}, .type = BuildingType::Core, .gridPosition = {0, 0}, .level = 4,
            .health = 480, .maxHealth = 500},
        {.id = {1001, 1}, .type = BuildingType::GunTurret, .gridPosition = {2, 0}, .level = 4,
            .health = 180, .maxHealth = 200},
    };
    saved.runUpgradeChoicePending = true;
    saved.runUpgradeChoiceCount = 1;
    saved.runUpgradeSelectionsRemaining = 1;
    saved.runUpgradeChoices[0] = RunUpgradeEffect::UnlockGunTurret;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(saved), "core-gated stale offer resumes");
    for (std::size_t i = 0; i < run->snapshot().runUpgradeChoiceCount; ++i) {
        const auto card = run->snapshot().runUpgradeChoices[i];
        require(card != RunUpgradeEffect::UnlockGunTurret, "capped defense is removed from pending level offers");
        if (runDefenseCard(card)) require(run->snapshot().runUpgradeStacks[runUpgradeIndex(card)] < 4,
            "level rewards cannot upgrade past the core ceiling");
    }
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockGunTurret),
        "normal application rejects an already core-capped defense");
    auto shopping = run->saveSuspendedRunState();
    shopping.runUpgradeChoicePending = false;
    shopping.runUpgradeChoiceCount = 0;
    shopping.runUpgradeSelectionsRemaining = 0;
    shopping.coins = 10;
    shopping.runShop.pending = true;
    shopping.runShop.generation = 1;
    shopping.runShop.offers.fill({});
    shopping.runShop.offers[0] = {.effect = RunUpgradeEffect::UnlockGunTurret,
        .rarity = RunUpgradeRarity::Common, .price = 1, .present = true};
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(shopping) && !run->snapshot().runShopBuyable[0] &&
        !run->buyRunShopOffer(0) && run->snapshot().coins == 10,
        "shop cannot charge for a card blocked by core level");
    shopping.buildings[0].level = 5;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(shopping) && run->snapshot().runShopBuyable[0] &&
        run->buyRunShopOffer(0) && run->snapshot().runUpgradeStacks[gun] == 5 &&
        run->snapshot().coins == 9, "upgrading the core reopens the same defense card");
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockGunTurret),
        "next core ceiling blocks another normal upgrade");
    require(run->closeRunShop(), "repair fixture closes the shop");
    const auto coreId = *run->snapshot().coreId;
    const auto coreHealth = std::ranges::find(run->snapshot().buildings, coreId, &BuildingInstance::id)->health;
    const auto stock = std::array{run->snapshot().wood, run->snapshot().stone, run->snapshot().crystals};
    PlayerCommand repair;
    repair.repairBuilding = RepairBuildingCommand{coreId};
    run->tick(0, repair);
    require(std::ranges::find(run->snapshot().buildings, coreId, &BuildingInstance::id)->health == coreHealth &&
        stock == std::array{run->snapshot().wood, run->snapshot().stone, run->snapshot().crystals},
        "core repair commands neither heal nor charge resources");
    require(std::ranges::any_of(run->takeEvents(), [](const auto& event) {
        return event.type == GameEventType::BuildingRepairRejected &&
            event.buildingActionError == BuildingActionError::Unsupported;
    }), "core repair is rejected explicitly");

    BuildingSystem buildings;
    const auto core = buildings.place(BuildingType::Core, {0, 0}, 0, 99999, 99999, 99999);
    const auto wall = buildings.place(BuildingType::Wall, {4, 0}, 0, 99999, 99999, 99999);
    require(core && wall, "single-repair fixture places a core and wall");
    static_cast<void>(buildings.damage(core->building.id, 20));
    static_cast<void>(buildings.damage(wall->building.id, 20));
    require(buildings.repair(core->building.id, 99999, 99999, 99999).error == BuildingActionError::Unsupported,
        "building repair flow excludes the core");
    require(buildings.repair(wall->building.id, 99999, 99999, 99999).valid(),
        "individual wall repair remains available");
    const auto upgraded = buildings.upgrade(core->building.id, 99999, 99999, 99999);
    require(upgraded.valid() && upgraded.building &&
        upgraded.building->maxHealth > core->building.maxHealth,
        "core upgrades retain health growth");
    requireNear(upgraded.building->maxHealth - upgraded.building->health, 20, 1e-9,
        "core upgrades preserve existing damage");
}
