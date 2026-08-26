#include "TestHarness.hpp"
#include "buildings/BuildingStats.hpp"
#include "economy/CrystalMineSystem.hpp"

void runBuildingStatsTests() {
    ian::CrystalMineSystem producers;
    const ian::BuildingInstance core{
        .id = ian::EntityId{9, 0},
        .type = ian::BuildingType::Core,
        .gridPosition = {0, 0},
        .level = 1,
        .health = 500.0,
        .maxHealth = 500.0,
    };
    const auto coreComparison = ian::compareBuildingStats(
        core, producers, ian::MaxBuildingLevel);
    require(
        coreComparison.current.defenseBuildingLimit == 3.0 &&
            coreComparison.next->defenseBuildingLimit == 5.0 &&
            coreComparison.current.producerPerTypeLimit == 1.0 &&
            coreComparison.next->producerPerTypeLimit == 2.0 &&
            coreComparison.current.woodCapacity == 120.0 &&
            coreComparison.current.stoneCapacity == 75.0 &&
            coreComparison.current.crystalCapacity == 75.0 &&
            coreComparison.next->woodCapacity == 240.0 &&
            coreComparison.next->stoneCapacity == 150.0 &&
            coreComparison.next->crystalCapacity == 150.0,
        "core upgrade preview exposes building and storage growth");
    const ian::BuildingInstance turret{
        .id = ian::EntityId{1, 0},
        .type = ian::BuildingType::Turret,
        .gridPosition = {0, 0},
        .level = 2,
        .health = 115.0,
        .maxHealth = 115.0,
    };
    const auto comparison = ian::compareBuildingStats(
        turret, producers, ian::MaxBuildingLevel);
    require(
        comparison.previous.has_value() &&
            comparison.next.has_value(),
        "middle building level exposes previous and next stats");
    require(
        comparison.current.attackDamage.has_value() &&
            comparison.current.attackRange.has_value() &&
            comparison.current.attackArcDegrees == 100.0 &&
            comparison.next->attackArcDegrees == 110.0 &&
            comparison.current.attacksPerSecond.has_value() &&
            comparison.current.piercingCount == 2.0 &&
            comparison.next->piercingCount == 3.0,
        "turret stats expose combat values and upgradeable firing arc");
    requireNear(
        comparison.previous->maxHealth, 100.0, 1e-9,
        "building stats recover base level health");
    require(
        comparison.next->maxHealth >
            comparison.current.maxHealth,
        "next building level increases health");

    const ian::BuildingInstance quarry{
        .id = ian::EntityId{2, 0},
        .type = ian::BuildingType::Quarry,
        .gridPosition = {0, 0},
        .level = ian::MaxBuildingLevel,
        .health = 205.0,
        .maxHealth = 205.0,
    };
    const auto maximum = ian::compareBuildingStats(
        quarry, producers, ian::MaxBuildingLevel);
    require(
        maximum.previous.has_value() &&
            !maximum.next.has_value(),
        "maximum building level has no upgrade preview");
    require(
        maximum.current.productionPerCycle.has_value() &&
            maximum.current.productionInterval.has_value(),
        "producer stats expose output and interval");
}
