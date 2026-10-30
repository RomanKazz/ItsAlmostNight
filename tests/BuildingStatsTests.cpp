#include "TestHarness.hpp"
#include "buildings/BuildingStats.hpp"
#include "combat/CannonSystem.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/TrapSystem.hpp"
#include "economy/CrystalMineSystem.hpp"
#include "progression/BuildingStatUpgrades.hpp"

#include <array>
#include <cmath>

void runBuildingStatsTests() {
    {
        ian::BuildingUpgradePowers powers{};
        for (const auto [building, stat] : {
                std::pair{ian::BuildingType::GunTurret, ian::AttackStat::Quantity},
                std::pair{ian::BuildingType::TeslaTower, ian::AttackStat::Bounces},
                std::pair{ian::BuildingType::Turret, ian::AttackStat::Pierce}}) {
            const auto [before, after] = ian::buildingStatBonusPreview(
                powers, building, stat, ian::RunUpgradeRarity::Common);
            requireNear(before, 0.0, 1e-12, "first discrete card starts at zero bonus despite built-in attacks");
            requireNear(after, 1.0, 1e-12, "first discrete card adds one local bonus");
            powers[static_cast<std::size_t>(building)][static_cast<std::size_t>(stat)] = 1.0;
            const auto [existing, upgraded] = ian::buildingStatBonusPreview(
                powers, building, stat, ian::RunUpgradeRarity::Common);
            requireNear(existing, 1.0, 1e-12, "later card shows the previously earned bonus");
            requireNear(upgraded, 2.0, 1e-12, "later card previews cumulative bonus");
        }
    }
    constexpr std::array ActiveBuildings{
        ian::BuildingType::Core, ian::BuildingType::Wall,
        ian::BuildingType::Turret, ian::BuildingType::CrystalMine,
        ian::BuildingType::Cannon, ian::BuildingType::SlowTrap,
        ian::BuildingType::Gate, ian::BuildingType::LumberMill,
        ian::BuildingType::Quarry, ian::BuildingType::SpikeTrap,
        ian::BuildingType::GunTurret,
        ian::BuildingType::Catapult};
    const auto balance = ian::GameBalance::defaults();
    for (const ian::BuildingType type : ActiveBuildings) {
        const double baseHealth = balance.buildings[
            static_cast<std::size_t>(type)].maxHealth;
        double previousHealth = 0.0;
        int previousCost = -1;
        for (int level = 1; level <= ian::MaxBuildingLevel; ++level) {
            const double health = ian::buildingMaxHealthAtLevel(
                baseHealth, static_cast<std::uint8_t>(level));
            require(std::fmod(health, 5.0) == 0.0 &&
                        health > previousHealth,
                    "every building level has increasing five-point health");
            previousHealth = health;
            if (level == ian::MaxBuildingLevel) {
                require(health >= baseHealth * 3.0,
                        "level-eight buildings have at least triple health");
                continue;
            }
            const ian::BuildingInstance building{
                .type = type,
                .level = static_cast<std::uint8_t>(level),
            };
            const ian::ResourceCost cost =
                ian::buildingUpgradeCost(building);
            const int total = cost.wood + cost.stone + cost.crystals;
            if (type == ian::BuildingType::Core) {
                require(total == 0, "Core has no resource upgrade cost");
                continue;
            }
            require(cost.wood % 5 == 0 && cost.stone % 5 == 0 &&
                        cost.crystals % 5 == 0 && total > previousCost,
                    "upgrade costs are rounded and rise every level");
            previousCost = total;
        }
    }
    require(
        ian::TowerSystem::attackDamage(ian::BuildingType::Turret, 8) >=
            3.0 * ian::TowerSystem::attackDamage(
                ian::BuildingType::Turret, 1) &&
        ian::TowerSystem::attackDamage(ian::BuildingType::GunTurret, 8) >=
            3.0 * ian::TowerSystem::attackDamage(
                ian::BuildingType::GunTurret, 1) &&
        ian::CannonSystem::explosionDamage(ian::BuildingType::Cannon, 8) >=
            3.0 * ian::CannonSystem::explosionDamage(
                ian::BuildingType::Cannon, 1) &&
        ian::CannonSystem::explosionDamage(ian::BuildingType::Catapult, 8) >=
            3.0 * ian::CannonSystem::explosionDamage(
                ian::BuildingType::Catapult, 1) &&
        ian::TrapSystem::spikeDamage(8) >=
            3.0 * ian::TrapSystem::spikeDamage(1),
        "combat building damage grows at least threefold");

    ian::CrystalMineSystem producers;
    for (const auto type : {ian::BuildingType::Turret, ian::BuildingType::GunTurret,
                           ian::BuildingType::Cannon, ian::BuildingType::Catapult}) {
        ian::BuildingInstance enhanced{.type = type, .level = 2};
        const auto ordinary = ian::buildingStatsAtLevel(enhanced, 2, producers);
        enhanced.anvilEnhanced = true;
        const auto legacy = ian::buildingStatsAtLevel(enhanced, 2, producers);
        requireNear(*legacy.attackDamage, *ordinary.attackDamage * 1.10, 1e-9,
            "legacy anvil saves retain their damage bonus in building cards");
        enhanced.anvilStacks = 3;
        const auto stacked = ian::buildingStatsAtLevel(enhanced, 2, producers);
        requireNear(*stacked.attackDamage, *ordinary.attackDamage * 1.30, 1e-9,
            "anvil stacks replace the legacy flag instead of applying twice");
    }
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
        !coreComparison.current.buildingLimit &&
            !coreComparison.next->buildingLimit &&
            coreComparison.current.woodCapacity == 250.0 &&
            coreComparison.current.stoneCapacity == 150.0 &&
            coreComparison.current.crystalCapacity == 100.0 &&
            coreComparison.next->woodCapacity == 250.0 &&
            coreComparison.next->stoneCapacity == 150.0 &&
            coreComparison.next->crystalCapacity == 150.0,
        "core upgrade preview shows storage growth without a building limit");
    const ian::BuildingInstance turret{
        .id = ian::EntityId{1, 0},
        .type = ian::BuildingType::Turret,
        .gridPosition = {0, 0},
        .level = 2,
        .health = 155.0,
        .maxHealth = 155.0,
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
        comparison.previous->maxHealth, 120.0, 1e-9,
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
        .health = 510.0,
        .maxHealth = 510.0,
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
        "producer stats expose output and interval without local capacity");
    const auto extended = ian::compareBuildingStats(
        quarry, producers, ian::MaxStructureLevel);
    require(extended.next &&
        extended.next->maxHealth > extended.current.maxHealth &&
        extended.next->productionPerCycle > extended.current.productionPerCycle,
        "level-nine producer preview continues health and production progression");
}
