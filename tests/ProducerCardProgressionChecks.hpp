#pragma once

#include "TestHarness.hpp"
#include "game/Simulation.hpp"

#include <memory>

inline void checkProducerCardProgression() {
    using namespace ian;
    auto world = WorldConfig::defaults();
    world.terrainResolution = 33;
    world.terrainAmplitude = world.terrainSurfaceNoiseAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto map = MapDefinition::defaults();
    map.resources.clear(); map.obstacles.clear();
    auto run = std::make_unique<Simulation>(GameBalance::defaults(), map, world);
    const auto restore = [&](const SuspendedRunState& saved) {
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(saved), "producer checkpoint restores");
    };
    run->startRunFromSeed(PlayerClass::Vanguard, 4242);
    auto saved = run->saveSuspendedRunState();
    saved.buildings = {
        {.id={1000,1}, .type=BuildingType::Core, .gridPosition={0,0}, .level=3, .health=500, .maxHealth=500},
        {.id={1001,1}, .type=BuildingType::Quarry, .gridPosition={2,0}, .health=100, .maxHealth=100},
        {.id={1002,1}, .type=BuildingType::Quarry, .gridPosition={4,0}, .health=100, .maxHealth=100},
    };
    const auto quarry = runUpgradeIndex(RunUpgradeEffect::UnlockQuarry);
    saved.runUpgradeStacks[quarry] = 1; saved.runUpgradePower[quarry] = 1;
    saved.runUpgradeChoicePending = true; saved.runUpgradeChoiceCount = 3;
    saved.runUpgradeSelectionsRemaining = 1;
    saved.runUpgradeChoices = {RunUpgradeEffect::UnlockQuarry, RunUpgradeEffect::Damage, RunUpgradeEffect::MoveSpeed};
    saved.runUpgradeRarities.fill(RunUpgradeRarity::Legendary);
    restore(saved);
    require(run->selectRunUpgrade(0) && run->snapshot().runUpgradeStacks[quarry] == 2 &&
        run->snapshot().buildingArsenalCount == 2,
        "producer card raises its shared level once regardless of rarity or copy count");
    for (const auto& building : run->snapshot().buildings)
        if (building.type == BuildingType::Quarry) require(building.level == 2, "all producer copies upgrade together");
    auto upgraded = run->saveSuspendedRunState();
    require(upgraded.producerRuntimes.size() == 2 && upgraded.producerRuntimes[0].level == 2 &&
        upgraded.producerRuntimes[1].level == 2, "card upgrade updates production runtimes too");
    upgraded.wood = upgraded.stone = upgraded.crystals = 999;
    restore(upgraded);
    const ResourceCost budget{run->snapshot().wood, run->snapshot().stone, run->snapshot().crystals};
    PlayerCommand manual;
    manual.upgradeBuilding = UpgradeBuildingCommand{{1001,1}};
    run->tick(0, manual);
    require(run->snapshot().runUpgradeStacks[quarry] == 2 && run->snapshot().wood == budget.wood &&
        run->snapshot().stone == budget.stone && run->snapshot().crystals == budget.crystals,
        "normal direct producer upgrade cannot spend resources or bypass cards");
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockQuarry) &&
        !run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockQuarry), "producer cards stop at the core level");
    auto legacy = upgraded;
    legacy.buildingArsenalVersion = 4;
    legacy.runUpgradeStacks[quarry] = 1; legacy.runUpgradePower[quarry] = 1;
    legacy.buildings[1].level = 3; legacy.buildings[2].level = 5;
    restore(legacy);
    require(run->snapshot().runUpgradeStacks[quarry] == 5, "legacy producer keeps its strongest built level");
    for (const auto& building : run->snapshot().buildings)
        if (building.type == BuildingType::Quarry) require(building.level == 5, "legacy producer copies converge on the preserved level");
    auto migrated = run->saveSuspendedRunState();
    restore(migrated);
    require(run->saveSuspendedRunState().runUpgradeStacks == migrated.runUpgradeStacks,
        "producer save migration is idempotent");

    for (int extra = 0; extra <= 2; ++extra) {
        run->returnToMainMenu();
        MetaRunBonuses bonuses; bonuses.additionalBuildingSlots = extra;
        require(run->configureNextRunBonuses(bonuses), "structure meta slots configure");
        run->startRunFromSeed(PlayerClass::Vanguard, 4242);
        auto fixture = run->saveSuspendedRunState();
        fixture.buildings = {{.id={1000,1}, .type=BuildingType::Core, .gridPosition={0,0},
            .level=8, .health=500, .maxHealth=500}};
        restore(fixture);
        for (const auto effect : {RunUpgradeEffect::UnlockLumberMill, RunUpgradeEffect::UnlockQuarry,
                 RunUpgradeEffect::UnlockCrystalMine, RunUpgradeEffect::UnlockCannon, RunUpgradeEffect::UnlockWall}) {
            const bool room = run->snapshot().buildingArsenalCount < static_cast<std::size_t>(4 + extra);
            require(run->grantDebugRunUpgrade(effect) == room, "all producer/defense acquisitions use one common slot cap");
        }
        require(run->snapshot().buildingSlotLimit == 4 + extra &&
            run->snapshot().buildingArsenalCount == static_cast<std::size_t>(4 + extra),
            "meta ranks expose four, five or six structure slots");
        require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockQuarry), "full roster still permits owned producer upgrades");
        fixture = run->saveSuspendedRunState();
        restore(fixture);
        require(run->snapshot().buildingSlotLimit == 4 + extra, "active structure capacity survives resume");
    }
}
