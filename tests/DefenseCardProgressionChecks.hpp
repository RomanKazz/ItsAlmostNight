#pragma once

#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "progression/RunUpgradeOfferSelection.hpp"
#include "core/DeterministicRandom.hpp"

#include <memory>

inline void checkDefenseCardProgression() {
    using namespace ian;
    // One owned defense must remain competitive against several new unlocks.
    std::array<int, RunUpgradeEffectCount> levels{};
    levels[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] = 7;
    const std::array pool{RunUpgradeEffect::UnlockGunTurret, RunUpgradeEffect::UnlockTurret,
        RunUpgradeEffect::UnlockCannon, RunUpgradeEffect::UnlockCatapult,
        RunUpgradeEffect::UnlockSpikeTrap, RunUpgradeEffect::UnlockSlowTrap};
    std::uint64_t random = 749123;
    std::array<int, pool.size()> draws{};
    for (int i = 0; i < 100000; ++i) ++draws[chooseRunUpgradeOffer(pool, levels, random)];
    require(draws[0] > 44000 && draws[0] < 46000, "owned defense receives the fixed existing-category chance");
    for (std::size_t i = 1; i < draws.size(); ++i)
        require(draws[i] > 10300 && draws[i] < 11700, "new defenses distribute evenly inside the new category");
    int onlyNewOffers = 0;
    for (int i = 0; i < 10000; ++i) {
        std::vector<RunUpgradeEffect> remaining(pool.begin(), pool.end());
        bool onlyNew = true;
        for (int choice = 0; choice < 3; ++choice) {
            const auto selected = chooseRunUpgradeOffer(remaining, levels, random);
            onlyNew &= remaining[selected] != RunUpgradeEffect::UnlockGunTurret;
            remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(selected));
        }
        onlyNewOffers += onlyNew;
    }
    require(onlyNewOffers > 1200 && onlyNewOffers < 2100,
        "the same pool can still offer three new defenses");
    levels[runUpgradeIndex(RunUpgradeEffect::UnlockTurret)] = 1;
    const std::array ownedPool{RunUpgradeEffect::UnlockGunTurret, RunUpgradeEffect::UnlockTurret};
    int gunDraws = 0;
    for (int i = 0; i < 100000; ++i) gunDraws += chooseRunUpgradeOffer(ownedPool, levels, random) == 0;
    require(gunDraws > 49000 && gunDraws < 51000, "defense levels do not bias owned-only offers");
    for (const auto type : RunBuildingTypes) require(runUpgradeMaximumLevel(*buildingRunUpgrade(type)) == MaxStructureLevel,
        "combat cards continue beyond eight authored model tiers");
    for (const auto type : RunEconomyBuildingTypes) require(runUpgradeMaximumLevel(*buildingRunUpgrade(type)) == MaxStructureLevel,
        "producer cards continue beyond eight authored model tiers");
    int oneStat = 0;
    int twoStats = 0;
    for (const auto type : RunBuildingTypes) {
        const auto statPool = buildingStatUpgradePool(type);
        if (statPool.empty()) continue;
        for (const auto stat : statPool) require(supportsAttackStat(type, stat),
            "building rolls contain only implemented attack stats");
        for (std::uint64_t seed = 1; seed <= 100; ++seed) {
            const auto common = rollBuildingStatUpgrade(type, RunUpgradeRarity::Common, seed);
            const auto rare = rollBuildingStatUpgrade(type, RunUpgradeRarity::Rare, seed);
            oneStat += common.count == 1;
            twoStats += common.count == 2;
            require(rare.count == 2 && rare.stats[0] != rare.stats[1],
                "rare building upgrade always offers two different stats");
        }
    }
    require(oneStat > 250 && twoStats > 250,
        "common building cards roll both one-stat and two-stat upgrades");
    require(retiredRunUpgrade(RunUpgradeEffect::Bounces) &&
        retiredRunUpgrade(RunUpgradeEffect::Pierce),
        "global bounce and pierce tomes are removed from normal offers");
    const auto expectPool = [](BuildingType type, std::initializer_list<AttackStat> expected) {
        const auto pool = buildingStatUpgradePool(type);
        require(pool.size() == expected.size() &&
            std::equal(pool.begin(), pool.end(), expected.begin()),
            "building upgrade pool matches its specialization");
    };
    using S = AttackStat;
    expectPool(BuildingType::GunTurret, {S::Damage, S::AttackSpeed, S::Quantity, S::Pierce, S::CritChance});
    expectPool(BuildingType::Turret, {S::Damage, S::Range, S::Quantity, S::Pierce, S::CritChance, S::CritDamage});
    expectPool(BuildingType::Cannon, {S::Damage, S::Size, S::Quantity, S::Knockback, S::CritDamage});
    expectPool(BuildingType::Catapult, {S::Damage, S::Range, S::Size, S::Quantity, S::Duration});
    expectPool(BuildingType::Mortar, {S::Damage, S::Range, S::Size, S::Quantity, S::CritDamage});
    expectPool(BuildingType::Flamethrower, {S::Damage, S::Size, S::Duration, S::Range});
    expectPool(BuildingType::MissileLauncher, {S::Damage, S::Size, S::Quantity, S::ProjectileSpeed, S::CritChance});
    expectPool(BuildingType::TeslaTower, {S::Damage, S::Bounces, S::Quantity, S::Range});
    expectPool(BuildingType::LaserTower, {S::Damage, S::Duration, S::Pierce, S::CritChance, S::CritDamage});
    expectPool(BuildingType::WindBlower, {S::Size, S::Range, S::Duration, S::Knockback});
    expectPool(BuildingType::SpikeTrap, {S::Damage, S::Size, S::CritChance, S::CritDamage});
    expectPool(BuildingType::SlowTrap, {S::Size, S::Duration, S::SlowStrength, S::Range});
    {
        BuildingUpgradePowers powers{};
        const BuildingStatRoll projectile{{S::Quantity},1};
        applyBuildingStatUpgrade(powers,BuildingType::GunTurret,projectile,RunUpgradeRarity::Epic);
        applyBuildingStatUpgrade(powers,BuildingType::GunTurret,projectile,RunUpgradeRarity::Rare);
        const auto modifier=buildingStatUpgradeModifiers(BuildingType::GunTurret,powers);
        requireNear(modifier[S::Quantity].flat,3.0,1e-12,
            "local discrete bonuses accumulate rarity fractions before floor");
        const BuildingStatRoll critical{{S::CritDamage},1};
        applyBuildingStatUpgrade(powers,BuildingType::Turret,critical,RunUpgradeRarity::Common);
        const auto crit=buildingStatUpgradeModifiers(BuildingType::Turret,powers);
        requireNear(crit[S::CritDamage].flat,0.25,1e-12,
            "local crit damage adds 25 percentage points to the base multiplier");
    }

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
    require(buildings.validateUpgrade(first->building.id, 99999, 99999, 99999).error == UpgradeError::CardRequired,
        "combat management requires cards regardless of core level");
    buildings.setCombatCardProgressionEnabled(false);
    for (int i = 0; i < 4; ++i)
        require(buildings.upgrade(core->building.id, 99999, 99999, 99999).valid(), "core fixture reaches level five");
    buildings.setCombatCardProgressionEnabled(true);
    require(buildings.validateUpgrade(first->building.id, 99999, 99999, 99999).error == UpgradeError::CardRequired,
        "resource upgrade cannot bypass combat cards after core upgrades");
    require(buildings.validateUpgrade(core->building.id, 99999, 99999, 99999).error == UpgradeError::CardRequired,
        "Core also uses card progression");
    const auto quarry = buildings.place(BuildingType::Quarry, {8, 0}, 0, 99999, 99999, 99999);
    require(quarry && buildings.setCombatBlueprintLevel(BuildingType::Quarry, 4),
        "producer copies use shared card levels too");
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
    const auto firstRoll = rollBuildingStatUpgrade(BuildingType::GunTurret,
        RunUpgradeRarity::Legendary,
        mixBits64(static_cast<std::uint64_t>(saved.terrainSeed) ^
            (static_cast<std::uint64_t>(BuildingType::GunTurret) << 32U) ^
            (2ULL * 0x9e3779b97f4a7c15ULL)));
    for (std::size_t i = 0; i < firstRoll.count; ++i)
        requireNear(run->snapshot().buildingUpgradePowers[static_cast<std::size_t>(BuildingType::GunTurret)]
                [static_cast<std::size_t>(firstRoll.stats[i])], 2.0, 1e-12,
            "legendary defense level applies two rarity-scaled stat rolls");
    for (const auto& building : run->snapshot().buildings) if (building.type == BuildingType::GunTurret)
        require(building.level == 2, "simulation synchronizes every existing copy");
    const auto upgraded = run->saveSuspendedRunState();
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(upgraded), "shared defense save restores");
    require(run->saveSuspendedRunState().runUpgradeStacks == upgraded.runUpgradeStacks,
        "save/load does not duplicate defense card levels");
    require(run->snapshot().buildingUpgradePowers == upgraded.buildingUpgradePowers,
        "save/load restores the exact rolled building bonuses");
    require(!run->openRunShop(true) && !run->snapshot().runShop.pending,
        "disabled shop cannot interrupt shared defense progression");
    auto oldShop = run->saveSuspendedRunState();
    oldShop.runShop.pending = true;
    oldShop.runShop.offers[0] = {.effect = RunUpgradeEffect::Bounces,
        .rarity = RunUpgradeRarity::Rare, .price = 10, .present = true};
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(oldShop), "old shop with retired bounce tome resumes");
    require(!run->snapshot().runShop.offers[0].present &&
        std::ranges::none_of(run->snapshot().runShop.offers,
            [](const auto& offer) { return offer.present && retiredRunUpgrade(offer.effect); }),
        "retired shop card is removed on resume");
    require(!run->snapshot().runShop.pending, "migrated disabled shop remains closed");
    require(run->grantDebugRunUpgradeBatch(RunUpgradeEffect::UnlockGunTurret, 500000),
        "debug bulk can max a defense in one transaction");
    require(run->snapshot().runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockGunTurret)] == MaxStructureLevel &&
        !run->grantDebugRunUpgradeBatch(RunUpgradeEffect::UnlockGunTurret, 1),
        "debug defense grants stop only at the storage limit");
    for (const auto& building : run->snapshot().buildings) if (building.type == BuildingType::GunTurret)
        require(building.level == MaxStructureLevel, "bulk grant updates every copy");

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
    require(run->snapshot().buildingUpgradePowers == BuildingUpgradePowers{},
        "new run clears the previous building stat rolls");
    auto pending = upgraded;
    pending.runUpgradeChoicePending = true;
    pending.runUpgradeChoiceCount = 1;
    pending.runUpgradeSelectionsRemaining = 1;
    pending.runUpgradeChoices[0] = RunUpgradeEffect::UnlockGunTurret;
    pending.runUpgradeRarities[0] = RunUpgradeRarity::Epic;
    pending.runUpgradeRollSeeds[0] = 98765;
    run->returnToMainMenu();
    require(run->loadSuspendedRunState(pending) && run->selectRunUpgrade(0),
        "saved building card can be selected after resume");
    const auto offerRoll = rollBuildingStatUpgrade(BuildingType::GunTurret,
        RunUpgradeRarity::Epic, 98765);
    for (std::size_t i = 0; i < offerRoll.count; ++i)
        requireNear(run->snapshot().buildingUpgradePowers[static_cast<std::size_t>(BuildingType::GunTurret)]
                [static_cast<std::size_t>(offerRoll.stats[i])],
            pending.buildingUpgradePowers[static_cast<std::size_t>(BuildingType::GunTurret)]
                [static_cast<std::size_t>(offerRoll.stats[i])] + 1.6, 1e-12,
            "selected card keeps its previewed stats after saving and loading");
}
