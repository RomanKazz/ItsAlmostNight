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
    const auto quarryTypeIndex = static_cast<std::size_t>(BuildingType::Quarry);
    require(run->snapshot().nextProducerRatesPerSecond[quarryTypeIndex] >
        run->snapshot().producerRatesPerSecond[quarryTypeIndex],
        "upgrade snapshot exposes actual before/after production per second");
    const auto& producerPreview = run->snapshot().producerCards.front().stats;
    require(producerPreview.current.productionPerCycle && producerPreview.current.productionInterval &&
        producerPreview.next && producerPreview.next->productionPerCycle &&
        producerPreview.next->productionInterval &&
        *producerPreview.next->productionPerCycle / *producerPreview.next->productionInterval >
            *producerPreview.current.productionPerCycle / *producerPreview.current.productionInterval,
        "producer card has real before/after production rates for the UI");
    require(run->selectRunUpgrade(0) && run->snapshot().runUpgradeStacks[quarry] == 2 &&
        run->snapshot().buildingArsenalCount == 1,
        "producer card raises its shared level without using a defense slot");
    for (const auto& building : run->snapshot().buildings)
        if (building.type == BuildingType::Quarry) require(building.level == 2, "all producer copies upgrade together");
    auto upgraded = run->saveSuspendedRunState();
    const auto beforeBatchUpgrade = run->snapshot();
    require(beforeBatchUpgrade.producerAmountsPerCycle[quarryTypeIndex] == 2 &&
        beforeBatchUpgrade.nextProducerAmountsPerCycle[quarryTypeIndex] == 3,
        "producer preview explains the level-three batch increase separately from speed");
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockQuarry),
        "producer fixture applies the batch-increasing card");
    const auto afterBatchUpgrade = run->snapshot();
    require(afterBatchUpgrade.producerAmountsPerCycle[quarryTypeIndex] ==
            beforeBatchUpgrade.nextProducerAmountsPerCycle[quarryTypeIndex],
        "preview batch size matches the actual card upgrade");
    requireNear(afterBatchUpgrade.producerRatesPerSecond[quarryTypeIndex],
        beforeBatchUpgrade.nextProducerRatesPerSecond[quarryTypeIndex], 1e-12,
        "preview throughput matches actual production across a batch-size jump");
    restore(upgraded);
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
    for (int level = 3; level <= 9; ++level)
        require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockQuarry),
            "producer cards advance beyond level eight independently of the core");
    require(run->grantDebugRunUpgradeBatch(RunUpgradeEffect::UnlockQuarry, 1000),
        "producer cards continue to their representation limit");
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockQuarry),
        "producer card level does not overflow saved storage");
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

    for (int extra = 0; extra <= MaximumRunBuildingSlots-BaseRunBuildingSlots; ++extra) {
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
            require(run->grantDebugRunUpgrade(effect),
                "producer and wall upgrades remain available without using defense slots");
        }
        require(run->snapshot().buildingSlotLimit == BaseRunBuildingSlots + extra &&
            run->snapshot().buildingArsenalCount == 2,
            "only the starter and newly acquired Cannon use defense slots");
        require(run->grantDebugRunUpgrade(RunUpgradeEffect::UnlockQuarry), "full roster still permits owned producer upgrades");
        fixture = run->saveSuspendedRunState();
        restore(fixture);
        require(run->snapshot().buildingSlotLimit == BaseRunBuildingSlots + extra, "active structure capacity survives resume");
    }
}
