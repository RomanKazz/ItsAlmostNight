#include "TestHarness.hpp"
#include "combat/BombSystem.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "economy/GoldMineSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "game/GameBalance.hpp"
#include "game/Simulation.hpp"
#include "waves/WaveDirector.hpp"

#include <array>

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
        "core": {"wood": 35, "stone": 0, "gold": 0, "maxHealth": 600,
                 "unlockCoreLevel": 0, "maxCount": 1},
        "wall": {"wood": 12, "stone": 0, "gold": 0, "maxHealth": 110,
                 "unlockCoreLevel": 1, "maxCount": 200},
        "turret": {"wood": 25, "stone": 15, "gold": 0, "maxHealth": 120,
                   "unlockCoreLevel": 1, "maxCount": 64},
        "goldMine": {"wood": 20, "stone": 10, "gold": 0, "maxHealth": 140,
                     "unlockCoreLevel": 1, "maxCount": 4},
        "cannon": {"wood": 40, "stone": 30, "gold": 25, "maxHealth": 180,
                   "unlockCoreLevel": 2, "maxCount": 64},
        "slowTrap": {"wood": 15, "stone": 20, "gold": 10, "maxHealth": 100,
                     "unlockCoreLevel": 2, "maxCount": 64},
        "gate": {"wood": 15, "stone": 5, "gold": 0, "maxHealth": 130,
                 "unlockCoreLevel": 1, "maxCount": 128}
    })json";
    constexpr std::string_view Weapons = R"json({
        "rifle": {
            "range": 12, "damage": 5, "damagePerLevel": 2,
            "fireInterval": 0.4, "fireRateBonusPerLevel": 0.1,
            "reloadDuration": 2, "reloadReductionPerLevel": 0.2,
            "magazineSize": 4, "magazineBonusPerLevel": 1,
            "upgradeGold": [5, 9]
        },
        "bomb": {
            "startingBombs": 5, "throwSpeed": 7, "upwardSpeed": 3,
            "gravity": 8, "fuseDuration": 0.1, "groundHeight": 0.2,
            "explosionRadius": 5, "explosionDamage": 20,
            "knockbackStrength": 4
        }
    })json";
    constexpr std::string_view Economy = R"json({
        "goldMineInterval": 2,
        "goldMineAmount": 7,
        "waveRewardPerWave": 11,
        "repairCostFraction": 0.25,
        "sellRefundFraction": 0.75,
        "buildingUpgradeCostMultiplier": [0.25, 0.8],
        "buildingUpgradeGoldBonus": [3, 4],
        "coreUpgradeGold": [20, 30]
    })json";
    constexpr std::string_view Gameplay = R"json({
        "eyeHeight": 1.8,
        "walkSpeed": 4,
        "sprintSpeed": 7,
        "jumpSpeed": 5,
        "gravity": 16,
        "playerMaxHealth": 125,
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
    require(loaded.balance.gameplay.playerMaxHealth == 125.0 &&
                loaded.balance.gameplay.maximumPlacementDistance == 12.0 &&
                loaded.balance.gameplay.pickaxeDamageVariation == 0.25 &&
                loaded.balance.gameplay.pickaxeCriticalChance == 0.2,
            "gameplay parameters come from JSON");
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
    require(coreUpgrade.valid() && coreUpgrade.cost.gold == 20,
            "core upgrade consumes loaded economy price");

    const auto mine =
        buildings.place(ian::BuildingType::GoldMine, {4, 0}, 0, 20, 10);
    require(mine.has_value(), "configured mine placement succeeds");
    ian::GoldMineSystem mines{loaded.balance.economy};
    mines.syncBuildings(buildings.buildings());
    const auto configuredProduction = mines.tick(2.0);
    require(configuredProduction.size() == 1 && configuredProduction[0].amount == 7,
            "mine consumes loaded interval and amount");

    ian::PlayerWeaponSystem rifle{loaded.balance.weapons.rifle};
    require(rifle.rifleRange() == 12.0 && rifle.rifleDamage() == 5.0 &&
                rifle.magazineSize() == 4 && rifle.upgradeGoldCost() == 5,
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
        "goldMineInterval": 0
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
                fallback.balance.economy.goldMineAmount ==
                    ian::GameBalance::defaults().economy.goldMineAmount &&
                fallback.balance.gameplay.playerMaxHealth ==
                    ian::GameBalance::defaults().gameplay.playerMaxHealth,
            "invalid config preserves safe fallback balance");
}
