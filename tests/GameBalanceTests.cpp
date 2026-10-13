#include "TestHarness.hpp"
#include "buildings/CoreProgression.hpp"
#include "combat/BombSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "economy/CrystalMineSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"
#include "game/Simulation.hpp"
#include "waves/WaveDirector.hpp"

#include <array>
#include <limits>
#include <nlohmann/json.hpp>

void runGameBalanceTests() {
    const auto assetBalance =
        ian::loadGameBalance("assets/data/enemies.json", "assets/data/waves.json",
                             "assets/data/buildings.json", "assets/data/weapons.json",
                             "assets/data/economy.json", "assets/data/gameplay.json");
    require(assetBalance.valid(), "copied asset JSON files load from runtime path");
    require(
        assetBalance.balance.modularBuildings[0].wood == 20 &&
            assetBalance.balance.modularBuildings[1].stone == 10 &&
            assetBalance.balance.modularBuildings[2].wood == 16,
        "asset balance exposes modular construction prices");
    constexpr std::array ActiveProgressionBuildings{
        ian::BuildingType::Wall,
        ian::BuildingType::Turret,
        ian::BuildingType::CrystalMine,
        ian::BuildingType::Cannon,
        ian::BuildingType::SlowTrap,
        ian::BuildingType::Gate,
        ian::BuildingType::LumberMill,
        ian::BuildingType::Quarry,
        ian::BuildingType::SpikeTrap,
        ian::BuildingType::GunTurret,
        ian::BuildingType::Catapult,
    };
    for (const auto type : ActiveProgressionBuildings) {
        require(
            assetBalance.balance.buildings[static_cast<std::size_t>(type)]
                    .unlockCoreLevel ==
                ian::buildingRequiredCoreLevel(type),
            "asset and runtime building unlock levels stay synchronized");
        require(assetBalance.balance.buildings[static_cast<std::size_t>(type)].crystals ==
                    ian::GameBalance::defaults().buildings[static_cast<std::size_t>(type)].crystals,
                "asset and fallback crystal construction prices stay synchronized");
    }
    const auto defaults = ian::GameBalance::defaults();
    require(assetBalance.balance.economy.coreUpgradeCrystals ==
                defaults.economy.coreUpgradeCrystals &&
            assetBalance.balance.economy.buildingUpgradeCrystalBonus ==
                defaults.economy.buildingUpgradeCrystalBonus &&
            assetBalance.balance.weapons.rifle.upgradeCrystal ==
                defaults.weapons.rifle.upgradeCrystal,
            "asset and fallback crystal upgrade curves stay synchronized");
    {
        ian::BuildingSystem economyBuildings{
            assetBalance.balance.buildings, assetBalance.balance.economy};
        constexpr std::array CorePrices{30, 50, 75, 110, 150, 200, 260};
        constexpr std::array CannonPrices{15, 25, 40, 55, 80, 110, 145};
        constexpr std::array CatapultPrices{20, 35, 55, 80, 115, 150, 200};
        constexpr std::array BasicPrices{5, 10, 15, 20, 30, 40, 55};
        constexpr std::array CrossbowPrices{10, 15, 25, 35, 50, 65, 85};
        require(economyBuildings.configuredCost(ian::BuildingType::Turret) == ian::ResourceCost{25, 15, 5},
                "crossbow construction requires wood, stone and crystals");
        for (std::uint8_t level = 1; level < ian::MaxBuildingLevel; ++level) {
            const auto priceAt = [&](ian::BuildingType type) {
                return economyBuildings.upgradeCost({.type = type, .level = level});
            };
            const auto corePrice = priceAt(ian::BuildingType::Core);
            require(corePrice == ian::ResourceCost{0, 0, CorePrices[level - 1]} &&
                        corePrice.crystals <= ian::coreResourceCapacity(
                            ian::ResourceCapacityType::Crystal, level),
                    "each Core upgrade uses the affordable full crystal curve");
            require(priceAt(ian::BuildingType::Cannon).crystals == CannonPrices[level - 1] &&
                        priceAt(ian::BuildingType::Catapult).crystals == CatapultPrices[level - 1],
                    "heavy weapon upgrades retain progressively higher crystal prices");
            require(priceAt(ian::BuildingType::Turret).crystals == CrossbowPrices[level - 1],
                    "crossbow upgrade prices include its crystal construction cost");
            for (const auto type : {ian::BuildingType::CrystalMine,
                                    ian::BuildingType::Quarry,
                                    ian::BuildingType::LumberMill}) {
                require(priceAt(type).crystals == BasicPrices[level - 1],
                        "basic defense and production share the lower upgrade curve");
            }
            require(priceAt(ian::BuildingType::Wall).crystals == 0 &&
                        priceAt(ian::BuildingType::Gate).crystals == 0,
                    "perimeter upgrades do not compete for progression crystals");
        }
        for (const auto type : ActiveProgressionBuildings) {
            const auto definition = assetBalance.balance.buildings[static_cast<std::size_t>(type)];
            for (const double missingHealth : {1.0, definition.maxHealth * 0.5,
                                               definition.maxHealth - 1.0}) {
                const ian::BuildingInstance damaged{
                    .type = type,
                    .health = definition.maxHealth - missingHealth,
                    .maxHealth = definition.maxHealth,
                };
                const auto repair = ian::buildingRepairCost(damaged);
                require(repair.crystals == 0 && repair.wood > 0,
                        "both small and extensive repairs only consume common materials");
                require(ian::buildingSellRefund(damaged).crystals <= definition.crystals,
                        "selling cannot create more crystals than construction consumed");
            }
        }
        const auto repairCore = economyBuildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
        require(repairCore.has_value(), "repair budget fixture creates its Core");
        const auto paidDefense = economyBuildings.place(
            ian::BuildingType::Cannon, {4, 0}, 0, 1000, 1000, 15);
        require(paidDefense.has_value(), "Cannon accepts its reduced construction budget");
        economyBuildings.damage(paidDefense->building.id, 90.0);
        const auto repaired = economyBuildings.repair(paidDefense->building.id, 10, 8, 0);
        require(repaired.valid() && repaired.cost == ian::ResourceCost{10, 8, 0},
                "a crystal-funded defense can be repaired with zero crystals available");
        const int firstWaveReward = defaults.economy.waveRewardBase +
                                    defaults.economy.waveRewardPerWave;
        require(firstWaveReward + ian::coreResourceCapacity(
                    ian::ResourceCapacityType::Crystal, 0) >= CorePrices[0] &&
                    firstWaveReward >= ian::buildingCost(ian::BuildingType::Cannon).crystals,
                "first wave and starter deposit allow progression or a heavy defense");
        ian::CrystalMineSystem dayIncome{assetBalance.balance.economy};
        const ian::BuildingInstance earlyMine{
            .id = {1001, 0}, .type = ian::BuildingType::CrystalMine,
            .health = 140.0, .maxHealth = 140.0,
        };
        dayIncome.syncBuildings({earlyMine});
        static_cast<void>(dayIncome.tick(assetBalance.balance.gameplay.firstBuildPhaseSeconds));
        const int collected = dayIncome.collect(earlyMine.id, 100);
        require(collected == 8, "first-day mine output reflects two-thirds production speed");
        static_cast<void>(dayIncome.tick(assetBalance.balance.gameplay.nightDurationSeconds));
        require(collected + dayIncome.collect(earlyMine.id, 100) + firstWaveReward >= CorePrices[0],
                "day and night production fund the first Core upgrade after wave one");
    }
    require(
        assetBalance.balance.buildings[static_cast<std::size_t>(
            ian::BuildingType::LumberMill)].crystals == 0 &&
            assetBalance.balance.buildings[static_cast<std::size_t>(
                ian::BuildingType::Quarry)].crystals == 0,
        "level-two producers remain affordable after the Core upgrade");
    require(
        assetBalance.balance.enemies[static_cast<std::size_t>(
            ian::EnemyType::Basic)].damage == 15.0 &&
            assetBalance.balance.enemies[static_cast<std::size_t>(
                ian::EnemyType::Heavy)].damage == 36.0 &&
            assetBalance.balance.enemies[static_cast<std::size_t>(
                ian::EnemyType::Boss)].damage == 30.0,
        "asset enemy damage keeps increased difficulty tuning");
    require(
        assetBalance.balance.gameplay.resourceGatherRange == 2.6 &&
            assetBalance.balance.gameplay.pickaxeRange == 4.0 &&
            assetBalance.balance.gameplay.nightDurationSeconds == 60.0 &&
            assetBalance.balance.gameplay.firstNightDurationSeconds == 60.0 &&
            assetBalance.balance.gameplay.nightDurationGrowthSeconds == 0.0 &&
            assetBalance.balance.gameplay.firstBuildPhaseSeconds == 75.0 &&
            assetBalance.balance.gameplay.betweenWaveSeconds == 75.0 &&
            assetBalance.balance.gameplay.resourceToolDamageMultiplier ==
                0.65,
        "resource gathering range is tuned separately from melee range");
    require(
        assetBalance.balance.weapons.club.areaRadius == 1.25 &&
            assetBalance.balance.weapons.club.knockbackStrength == 0.0 &&
            assetBalance.balance.weapons.club.maxDamagePerAttack == 4.0,
        "club balance exposes area, knockback, and total damage cap");

    constexpr std::string_view Enemies = R"json({
        "basic": {"health": 9, "speed": 2, "damage": 4},
        "fast": {"health": 2, "speed": 5, "damage": 3},
        "heavy": {"health": 12, "speed": 1, "damage": 8},
        "boss": {
            "health": 60, "speed": 0.8, "damage": 20,
            "ramWindup": 1, "ramDamageMultiplier": 4, "ramCooldown": 5
        }
    })json";
    constexpr std::string_view Waves = R"json({
        "waves": [
            {"budget": 1, "basic": 1, "fast": 0, "heavy": 0,
             "groupSize": 1, "groupInterval": 0.5},
            {"budget": 2, "basic": 0, "fast": 1, "heavy": 0,
             "groupSize": 1, "groupInterval": 0.5},
            {"budget": 5, "basic": 0, "fast": 0, "heavy": 1,
             "groupSize": 1, "groupInterval": 0.5},
            {"budget": 6, "basic": 1, "fast": 0, "heavy": 1,
             "groupSize": 1, "groupInterval": 0.5},
            {"budget": 7, "basic": 0, "fast": 1, "heavy": 1,
             "groupSize": 1, "groupInterval": 0.5},
            {"budget": 8, "basic": 1, "fast": 1, "heavy": 1, "boss": true,
             "groupSize": 1, "groupInterval": 0.5}
        ]
    })json";
    constexpr std::string_view Buildings = R"json({
        "core": {"wood": 35, "stone": 0, "crystals": 0, "maxHealth": 600,
                 "unlockCoreLevel": 0, "maxCount": 1},
        "wall": {"wood": 12, "stone": 0, "crystals": 0, "maxHealth": 110,
                 "unlockCoreLevel": 1, "maxCount": 200},
        "turret": {"wood": 25, "stone": 15, "crystals": 0, "maxHealth": 120,
                   "unlockCoreLevel": 1, "maxCount": 64},
        "crystalMine": {"wood": 20, "stone": 10, "crystals": 0, "maxHealth": 140,
                     "unlockCoreLevel": 1, "maxCount": 4},
        "cannon": {"wood": 40, "stone": 30, "crystals": 25, "maxHealth": 180,
                   "unlockCoreLevel": 2, "maxCount": 64},
        "slowTrap": {"wood": 15, "stone": 20, "crystals": 10, "maxHealth": 100,
                     "unlockCoreLevel": 2, "maxCount": 64},
        "gate": {"wood": 15, "stone": 5, "crystals": 0, "maxHealth": 130,
                 "unlockCoreLevel": 1, "maxCount": 128}
    })json";
    constexpr std::string_view Weapons = R"json({
        "rifle": {
            "range": 12, "damage": 5, "damagePerLevel": 2,
            "fireInterval": 0.4, "fireRateBonusPerLevel": 0.1,
            "reloadDuration": 2, "reloadReductionPerLevel": 0.2,
            "magazineSize": 4, "magazineBonusPerLevel": 1,
            "upgradeCrystal": [5, 9]
        },
        "bomb": {
            "startingBombs": 5, "throwSpeed": 7, "upwardSpeed": 3,
            "gravity": 8, "fuseDuration": 0.1, "groundHeight": 0.2,
            "explosionRadius": 5, "explosionDamage": 20,
            "knockbackStrength": 4
        }
    })json";
    constexpr std::string_view Economy = R"json({
        "crystalMineInterval": 2,
        "crystalMineAmount": 7,
        "waveRewardPerWave": 11,
        "repairCostFraction": 0.25,
        "repairCooldownSeconds": 2.5,
        "sellRefundFraction": 0.75,
        "buildingUpgradeCostMultiplier": [0.25, 0.8],
        "buildingUpgradeCrystalBonus": [3, 4],
        "coreUpgradeCrystals": [20, 30]
    })json";
    constexpr std::string_view Gameplay = R"json({
        "eyeHeight": 1.8,
        "walkSpeed": 4,
        "sprintSpeed": 7,
        "playerAcceleration": 30,
        "playerDeceleration": 45,
        "jumpSpeed": 5,
        "gravity": 16,
        "playerMaxHealth": 125,
        "playerRespawnSeconds": 4,
        "playerDeathResourceLossFraction": 0.4,
        "pickaxeRange": 5,
        "pickaxeDamage": 2,
        "pickaxeDamageVariation": 0.25,
        "pickaxeCriticalChance": 0.2,
        "pickaxeCooldown": 0.3,
        "firstBuildPhaseSeconds": 25,
        "betweenWaveSeconds": 70,
        "sunsetSeconds": 8,
        "dawnSeconds": 4,
        "minimumPlacementDistance": 0.5,
        "maximumPlacementDistance": 12
    })json";

    const auto loaded =
        ian::parseGameBalance(Enemies, Waves, Buildings, Weapons, Economy, Gameplay);
    require(loaded.valid(), "valid JSON balance parses");
    {
        require(loaded.balance.gameplay.overtime.afterWave == 10,
            "legacy gameplay JSON defaults to pressure after wave ten");
        auto gameplay = nlohmann::json::parse(Gameplay);
        gameplay["overtime"] = {{"afterWave", 11}, {"healthDoublingSeconds", 90.0}};
        const auto custom = ian::parseGameBalance(Enemies, Waves, Buildings, Weapons, Economy, gameplay.dump());
        require(custom.valid() && custom.balance.gameplay.overtime.afterWave == 11 &&
            custom.balance.gameplay.overtime.healthDoublingSeconds == 90,
            "overtime balance is data driven");
        gameplay["overtime"]["healthDoublingSeconds"] = 0;
        require(!ian::parseGameBalance(Enemies, Waves, Buildings, Weapons, Economy, gameplay.dump()).valid(),
            "zero overtime doubling interval is rejected");
        gameplay["overtime"] = {{"maximumSpawnRate", 100}};
        require(!ian::parseGameBalance(Enemies, Waves, Buildings, Weapons, Economy, gameplay.dump()).valid(),
            "unsafe population pressure is rejected");
    }
    require(loaded.balance.economy.coreUpgradeCrystals ==
                std::array<int, 7>{20, 30, 150, 200, 250, 300, 350} &&
            loaded.balance.economy.buildingUpgradeCrystalBonus ==
                std::array<int, 7>{3, 4, 14, 24, 34, 44, 54},
            "legacy two-entry balance preserves its previous late-level prices");
    {
        auto fullEconomy = nlohmann::json::parse(Economy);
        const std::array<int, 7> customCore{11, 22, 33, 44, 55, 66, 77};
        const std::array<int, 7> customBonus{1, 2, 3, 4, 5, 6, 7};
        fullEconomy["coreUpgradeCrystals"] = customCore;
        fullEconomy["buildingUpgradeCrystalBonus"] = customBonus;
        const auto customBalance = ian::parseGameBalance(
            Enemies, Waves, Buildings, Weapons, fullEconomy.dump(), Gameplay);
        require(customBalance.valid(), "full custom crystal curves parse");
        ian::BuildingSystem customBuildings{
            customBalance.balance.buildings, customBalance.balance.economy};
        for (std::uint8_t level = 1; level < ian::MaxBuildingLevel; ++level) {
            require(customBuildings.upgradeCost({.type = ian::BuildingType::Core,
                                                .level = level}).crystals == customCore[level - 1] &&
                        customBuildings.upgradeCost({.type = ian::BuildingType::Turret,
                                                     .level = level}).crystals == customBonus[level - 1],
                    "runtime honors configured prices through the final upgrade");
        }
        for (const auto field : {"coreUpgradeCrystals", "buildingUpgradeCrystalBonus"}) {
            auto invalidEconomy = fullEconomy;
            invalidEconomy[field][6] = -1;
            require(!ian::parseGameBalance(Enemies, Waves, Buildings, Weapons,
                                           invalidEconomy.dump(), Gameplay).valid(),
                    "negative late-level crystal prices are rejected");
            invalidEconomy[field] = std::array<int, 3>{1, 2, 3};
            require(!ian::parseGameBalance(Enemies, Waves, Buildings, Weapons,
                                           invalidEconomy.dump(), Gameplay).valid(),
                    "incomplete crystal price curves are rejected");
        }
        fullEconomy["buildingUpgradeCrystalBonus"] =
            std::array<int, 2>{1, std::numeric_limits<int>::max()};
        require(!ian::parseGameBalance(Enemies, Waves, Buildings, Weapons,
                                       fullEconomy.dump(), Gameplay).valid(),
                "legacy crystal extrapolation cannot overflow its price type");
    }
    require(
        loaded.balance.gameplay.resourceGatherRange ==
            loaded.balance.gameplay.pickaxeRange &&
            loaded.balance.gameplay.resourceToolDamageMultiplier == 0.65,
        "older gameplay balance uses safe gathering defaults");
    require(
        loaded.balance.modularBuildings[0].wood == 20,
        "older building balance keeps default modular prices");
    require(loaded.balance.enemies[0].health == 9.0,
            "enemy stats come from JSON");
    require(loaded.balance.enemies[3].ramWindup == 1.0 &&
                loaded.balance.enemies[3].ramDamageMultiplier == 4.0 &&
                loaded.balance.enemies[3].ramCooldown == 5.0,
            "boss ram parameters come from JSON");
    require(loaded.balance.waves[0].groupSize == 1 &&
                loaded.balance.waves[0].groupInterval == 0.5,
            "wave group schedule comes from JSON");
    requireNear(ian::GameBalance::defaults().gameplay.walkSpeed, 6.5, 1e-12,
                "default movement speed uses the tuned constant speed without sprint");
    requireNear(assetBalance.balance.gameplay.walkSpeed, 6.5, 1e-12,
                "runtime gameplay asset uses the same faster movement speed");
    require(loaded.balance.gameplay.playerMaxHealth == 125.0 &&
                loaded.balance.gameplay.playerAcceleration == 30.0 &&
                loaded.balance.gameplay.playerDeceleration == 45.0 &&
                loaded.balance.gameplay.playerRespawnSeconds == 4.0 &&
                loaded.balance.gameplay.playerDeathResourceLossFraction == 0.4 &&
                loaded.balance.gameplay.maximumPlacementDistance == 12.0 &&
                loaded.balance.gameplay.pickaxeDamageVariation == 0.25 &&
                loaded.balance.gameplay.pickaxeCriticalChance == 0.2,
            "gameplay parameters come from JSON");
    require(
        loaded.balance.economy.repairCooldownSeconds == 2.5,
        "repair cooldown comes from economy JSON");
    ian::Simulation configuredSimulation{loaded.balance};
    configuredSimulation.startRun();
    require(configuredSimulation.snapshot().playerHealth == 125.0 &&
                configuredSimulation.snapshot().playerPosition.y == 1.8,
            "simulation consumes loaded player definition");
    const auto configuredStart = configuredSimulation.snapshot().playerPosition;
    ian::PlayerCommand configuredMovement;
    configuredMovement.moveForward = 1.0;
    configuredSimulation.tick(1.0, configuredMovement);
    requireNear(configuredStart.z - configuredSimulation.snapshot().playerPosition.z, 4.0,
                1e-12, "simulation consumes loaded walk speed");

    ian::EnemySystem enemies{loaded.balance.enemies};
    constexpr std::array<ian::Vec3, 1> Spawn{{{0.0, 0.8, -4.0}}};
    enemies.spawnWave(Spawn);
    require(enemies.enemies()[0].health == 9.0 && enemies.enemies()[0].damage == 4.0,
            "enemy system consumes loaded stats");

    ian::WaveDirector director{loaded.balance.waves};
    const auto plan = director.buildWave(2, {0, 0});
    require(plan.regularBudget == 2 && plan.spawns.size() == 1 &&
                plan.spawns[0].type == ian::EnemyType::Fast,
            "wave director consumes loaded composition");

    ian::BuildingSystem buildings{loaded.balance.buildings, loaded.balance.economy};
    require(buildings.validate(ian::BuildingType::Core, {0, 0}, 34, 0).error ==
                ian::PlacementError::InsufficientResources,
            "building system consumes loaded cost");
    const auto configuredCore =
        buildings.place(ian::BuildingType::Core, {0, 0}, 0, 35, 0);
    require(configuredCore.has_value() && configuredCore->building.maxHealth == 600.0,
            "building system consumes loaded health");
    const auto configuredWall =
        buildings.place(ian::BuildingType::Wall, {4, 0}, 0, 12, 0);
    require(configuredWall.has_value(), "configured wall placement succeeds");
    buildings.damage(configuredWall->building.id, 55.0);
    const auto configuredRepair =
        buildings.repair(configuredWall->building.id, 2, 0, 0);
    require(configuredRepair.valid() && configuredRepair.cost.wood == 2,
            "repair consumes loaded economy fraction");
    const auto configuredSale = buildings.sell(configuredWall->building.id);
    require(configuredSale.valid() && configuredSale.refund.wood == 9,
            "sale consumes loaded economy fraction");
    const auto coreUpgrade =
        buildings.upgrade(configuredCore->building.id, 0, 0, 20);
    require(coreUpgrade.valid() && coreUpgrade.cost.crystals == 20,
            "core upgrade consumes loaded economy price");

    const auto mine =
        buildings.place(ian::BuildingType::CrystalMine, {4, 0}, 0, 20, 10);
    require(mine.has_value(), "configured mine placement succeeds");
    ian::CrystalMineSystem mines{loaded.balance.economy};
    mines.syncBuildings(buildings.buildings());
    const auto configuredProduction = mines.tick(3.0);
    require(configuredProduction.size() == 1 && configuredProduction[0].amount == 7,
            "mine consumes loaded interval at two-thirds speed and preserves amount");

    ian::PlayerWeaponSystem rifle{loaded.balance.weapons.rifle};
    require(rifle.rifleRange() == 12.0 && rifle.rifleDamage() == 5.0 &&
                rifle.magazineSize() == 4 && rifle.upgradeCrystalCost() == 5,
            "rifle consumes loaded weapon definition");
    ian::BombSystem bombs{loaded.balance.weapons.bomb};
    require(bombs.remainingBombs() == 5 &&
                bombs.throwBomb({0.0, 1.7, 0.0}, {0.0, 0.0, -1.0}) &&
                bombs.projectiles()[0].fuseRemaining == 0.1,
            "bomb consumes loaded weapon definition");

    constexpr std::string_view InvalidEnemies = R"json({
        "basic": {"health": -1, "speed": 2, "damage": 4}
    })json";
    constexpr std::string_view InvalidWaves = R"json({
        "waves": [{"budget": 99, "basic": 1, "fast": 0, "heavy": 0}]
    })json";
    constexpr std::string_view InvalidBuildings = R"json({"core": {}})json";
    constexpr std::string_view InvalidWeapons = R"json({
        "rifle": {"range": -1},
        "bomb": {}
    })json";
    constexpr std::string_view InvalidEconomy = R"json({
        "crystalMineInterval": 0
    })json";
    constexpr std::string_view InvalidGameplay = R"json({
        "eyeHeight": 0
    })json";
    const auto fallback =
        ian::parseGameBalance(InvalidEnemies, InvalidWaves, InvalidBuildings, InvalidWeapons,
                              InvalidEconomy, InvalidGameplay);
    require(!fallback.valid() && fallback.errors.size() == 6,
            "invalid config reports all files");
    require(fallback.balance.enemies[0].health ==
                ian::GameBalance::defaults().enemies[0].health &&
                fallback.balance.waves[0].budget ==
                    ian::GameBalance::defaults().waves[0].budget &&
                fallback.balance.buildings[0].maxHealth ==
                    ian::GameBalance::defaults().buildings[0].maxHealth &&
                fallback.balance.weapons.rifle.damage ==
                    ian::GameBalance::defaults().weapons.rifle.damage &&
                fallback.balance.economy.crystalMineAmount ==
                    ian::GameBalance::defaults().economy.crystalMineAmount &&
                fallback.balance.gameplay.playerMaxHealth ==
                    ian::GameBalance::defaults().gameplay.playerMaxHealth,
            "invalid config preserves safe fallback balance");
}
