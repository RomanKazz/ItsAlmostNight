#pragma once
#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include <memory>

inline void checkCoreDefenseGateAndRepairs() {
    using namespace ian;
    require(runWeaponLevelLimit(0) == 1 && runWeaponLevelLimit(4) == 4 &&
        runWeaponLevelLimit(8) == 8, "player weapon upgrades retain their core-level limit");
    auto map = MapDefinition::defaults();
    map.resources.clear();
    map.obstacles.clear();
    auto world = WorldConfig::defaults();
    world.terrainAmplitude = 0;
    auto run = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
    MetaRunBonuses bonuses; bonuses.additionalBuildingSlots=2;
    require(run->configureNextRunBonuses(bonuses), "core-gate fixture expands defense slots");
    run->startRunFromSeed(PlayerClass::Vanguard, 53218);
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockGunTurret),
        "starter defense gains level two independently of the core");
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
    require(run->loadSuspendedRunState(saved), "higher-level defense offer resumes");
    bool gunOffered = false;
    for (std::size_t i = 0; i < run->snapshot().runUpgradeChoiceCount; ++i) {
        const auto card = run->snapshot().runUpgradeChoices[i];
        gunOffered |= card == RunUpgradeEffect::UnlockGunTurret;
    }
    require(gunOffered, "defense card remains eligible above core level");
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockGunTurret),
        "normal application raises defense above core level");
    auto continued = run->saveSuspendedRunState();
    continued.runUpgradeChoicePending = false;
    continued.runUpgradeChoiceCount = 0;
    continued.runUpgradeSelectionsRemaining = 0;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(continued) &&
        run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockGunTurret) &&
        run->snapshot().runUpgradeStacks[gun] == 6,
        "defense progression continues above core level without the disabled shop");
    for (int level = 7; level <= 9; ++level)
        require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockGunTurret),
            "defense continues beyond the eighth visual tier independently of the core");
    require(run->grantDebugRunUpgradeBatch(RunUpgradeEffect::UnlockGunTurret, 1000),
        "bulk upgrade reaches the building storage limit");
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockGunTurret),
        "building level does not overflow its saved representation");
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
