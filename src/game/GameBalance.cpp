#include "game/GameBalance.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace ian {
namespace {

using Json = nlohmann::json;

std::string readTextFile(std::string_view path) {
    std::ifstream stream{std::string(path)};
    if (!stream) {
        throw std::runtime_error("cannot open " + std::string(path));
    }
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

EnemyDefinition parseEnemy(const Json& value, bool boss) {
    const EnemyDefinition definition{
        .health = value.at("health").get<double>(),
        .speed = value.at("speed").get<double>(),
        .damage = value.at("damage").get<double>(),
        .ramWindup = value.value("ramWindup", 0.0),
        .ramDamageMultiplier = value.value("ramDamageMultiplier", 1.0),
        .ramCooldown = value.value("ramCooldown", 0.0),
    };
    if (definition.health <= 0.0 || definition.speed <= 0.0 ||
        definition.damage <= 0.0 || definition.ramWindup < 0.0 ||
        definition.ramDamageMultiplier < 1.0 || definition.ramCooldown < 0.0 ||
        (boss && (definition.ramWindup <= 0.0 ||
                  definition.ramDamageMultiplier <= 1.0 ||
                  definition.ramCooldown <= 0.0))) {
        throw std::runtime_error("enemy stats must be positive");
    }
    return definition;
}

WaveDefinition parseWave(const Json& value) {
    const WaveDefinition definition{
        .budget = value.at("budget").get<int>(),
        .basic = value.at("basic").get<int>(),
        .fast = value.at("fast").get<int>(),
        .heavy = value.at("heavy").get<int>(),
        .ranged = value.value("ranged", 0),
        .sapper = value.value("sapper", 0),
        .flying = value.value("flying", 0),
        .splitter = value.value("splitter", 0),
        .boss = value.value("boss", false),
        .groupSize = value.at("groupSize").get<int>(),
        .groupInterval = value.at("groupInterval").get<double>(),
    };
    if (definition.budget <= 0 || definition.basic < 0 || definition.fast < 0 ||
        definition.heavy < 0 || definition.ranged < 0 ||
        definition.sapper < 0 || definition.flying < 0 ||
        definition.splitter < 0 ||
        definition.groupSize <= 0 ||
        definition.groupInterval <= 0.0) {
        throw std::runtime_error("wave values must be non-negative");
    }
    const int calculatedBudget =
        definition.basic + definition.fast * 2 +
        definition.heavy * 5 + definition.ranged * 3 +
        definition.sapper * 4 + definition.flying * 3 +
        definition.splitter * 4;
    if (calculatedBudget != definition.budget) {
        throw std::runtime_error("wave budget does not match composition");
    }
    const int spawnCount =
        definition.basic + definition.fast + definition.heavy +
        definition.ranged + definition.sapper + definition.flying +
        definition.splitter +
        (definition.boss ? 1 : 0);
    if (spawnCount > 200) {
        throw std::runtime_error("wave exceeds enemy pool");
    }
    return definition;
}

BuildingBalanceDefinition parseBuilding(const Json& value) {
    const BuildingBalanceDefinition definition{
        .wood = value.at("wood").get<int>(),
        .stone = value.at("stone").get<int>(),
        .crystals = value.at("crystals").get<int>(),
        .maxHealth = value.at("maxHealth").get<double>(),
        .unlockCoreLevel = value.at("unlockCoreLevel").get<int>(),
        .maxCount = value.at("maxCount").get<int>(),
    };
    if (definition.wood < 0 || definition.stone < 0 || definition.crystals < 0 ||
        definition.maxHealth <= 0.0 || definition.unlockCoreLevel < 0 ||
        definition.unlockCoreLevel > 8 || definition.maxCount <= 0 ||
        definition.maxCount > 256) {
        throw std::runtime_error("invalid building definition");
    }
    return definition;
}

ModularBuildingBalanceDefinition
parseModularBuilding(const Json& value) {
    const ModularBuildingBalanceDefinition definition{
        .wood = value.at("wood").get<int>(),
        .stone = value.at("stone").get<int>(),
        .crystals = value.at("crystals").get<int>(),
    };
    if (definition.wood < 0 ||
        definition.stone < 0 ||
        definition.crystals < 0) {
        throw std::runtime_error(
            "invalid modular building definition");
    }
    return definition;
}

WeaponBalanceDefinition parseWeapons(const Json& document) {
    const Json& rifle = document.at("rifle");
    const Json& bomb = document.at("bomb");
    const Json club = document.value("club", Json::object());
    const Json iceWand = document.value("iceWand", Json::object());
    const Json fireWand = document.value("fireWand", Json::object());
    const WeaponBalanceDefinition definition{
        .rifle = {
            .range = rifle.at("range").get<double>(),
            .damage = rifle.at("damage").get<double>(),
            .damagePerLevel = rifle.at("damagePerLevel").get<double>(),
            .fireInterval = rifle.at("fireInterval").get<double>(),
            .fireRateBonusPerLevel = rifle.at("fireRateBonusPerLevel").get<double>(),
            .reloadDuration = rifle.at("reloadDuration").get<double>(),
            .reloadReductionPerLevel = rifle.at("reloadReductionPerLevel").get<double>(),
            .magazineSize = rifle.at("magazineSize").get<int>(),
            .magazineBonusPerLevel = rifle.at("magazineBonusPerLevel").get<int>(),
            .upgradeCrystal = rifle.at("upgradeCrystal").get<std::array<int, 2>>(),
        },
        .bomb = {
            .startingBombs = bomb.at("startingBombs").get<int>(),
            .throwSpeed = bomb.at("throwSpeed").get<double>(),
            .upwardSpeed = bomb.at("upwardSpeed").get<double>(),
            .gravity = bomb.at("gravity").get<double>(),
            .fuseDuration = bomb.at("fuseDuration").get<double>(),
            .groundHeight = bomb.at("groundHeight").get<double>(),
            .explosionRadius = bomb.at("explosionRadius").get<double>(),
            .explosionDamage = bomb.at("explosionDamage").get<double>(),
            .knockbackStrength = bomb.at("knockbackStrength").get<double>(),
        },
        .club = {
            .damageMultiplier = club.value("damageMultiplier", 1.35),
            .areaRadius = club.value("areaRadius", 1.25),
            .knockbackStrength = club.value("knockbackStrength", 0.0),
            .maxDamagePerAttack = club.value("maxDamagePerAttack", 4.0),
        },
        .iceWand = {
            .cooldown = iceWand.value("cooldown", 0.85),
            .directDamage = iceWand.value("directDamage", 16.0),
            .projectileSpeed = iceWand.value("projectileSpeed", 18.0),
            .projectileRadius = iceWand.value("projectileRadius", 0.22),
            .maxLifetime = iceWand.value("maxLifetime", 2.5),
            .explosionRadius = iceWand.value("explosionRadius", 3.5),
            .freezeDuration = iceWand.value("freezeDuration", 1.4),
            .eliteFreezeMultiplier = iceWand.value("eliteFreezeMultiplier", 0.65),
            .bossSlowAmount = iceWand.value("bossSlowAmount", 0.35),
            .chargeUpDuration = iceWand.value("chargeUpDuration", 0.12),
            .areaDamageMultiplier = iceWand.value("areaDamageMultiplier", 0.55),
        },
        .fireWand = {
            .cooldown = fireWand.value("cooldown", 0.9),
            .directDamage = fireWand.value("directDamage", 12.0),
            .projectileSpeed = fireWand.value("projectileSpeed", 18.0),
            .projectileRadius = fireWand.value("projectileRadius", 0.24),
            .maxLifetime = fireWand.value("maxLifetime", 2.5),
            .explosionRadius = fireWand.value("explosionRadius", 3.25),
            .burnDuration = fireWand.value("burnDuration", 4.0),
            .burnDamagePerSecond = fireWand.value("burnDamagePerSecond", 3.0),
            .burnTickInterval = fireWand.value("burnTickInterval", 0.5),
            .chargeUpDuration = fireWand.value("chargeUpDuration", 0.14),
            .areaDamageMultiplier = fireWand.value("areaDamageMultiplier", 0.5),
        },
    };
    const auto& configuredRifle = definition.rifle;
    const auto& configuredBomb = definition.bomb;
    const auto& configuredClub = definition.club;
    const auto& configuredIceWand = definition.iceWand;
    const auto& configuredFireWand = definition.fireWand;
    if (configuredRifle.range <= 0.0 || configuredRifle.damage <= 0.0 ||
        configuredRifle.damagePerLevel < 0.0 || configuredRifle.fireInterval <= 0.0 ||
        configuredRifle.fireRateBonusPerLevel < 0.0 || configuredRifle.reloadDuration <= 0.0 ||
        configuredRifle.reloadReductionPerLevel < 0.0 ||
        configuredRifle.reloadDuration - 2.0 * configuredRifle.reloadReductionPerLevel <= 0.0 ||
        configuredRifle.magazineSize <= 0 || configuredRifle.magazineBonusPerLevel < 0 ||
        configuredRifle.upgradeCrystal[0] < 0 || configuredRifle.upgradeCrystal[1] < 0 ||
        configuredBomb.startingBombs < 0 || configuredBomb.throwSpeed <= 0.0 ||
        configuredBomb.upwardSpeed < 0.0 || configuredBomb.gravity <= 0.0 ||
        configuredBomb.fuseDuration <= 0.0 || configuredBomb.groundHeight < 0.0 ||
        configuredBomb.explosionRadius <= 0.0 || configuredBomb.explosionDamage <= 0.0 ||
        configuredBomb.knockbackStrength < 0.0 ||
        configuredClub.damageMultiplier <= 0.0 ||
        configuredClub.areaRadius <= 0.0 ||
        configuredClub.knockbackStrength < 0.0 ||
        configuredClub.maxDamagePerAttack <= 0.0 ||
        configuredIceWand.cooldown <= 0.0 ||
        configuredIceWand.directDamage <= 0.0 ||
        configuredIceWand.projectileSpeed <= 0.0 ||
        configuredIceWand.projectileRadius <= 0.0 ||
        configuredIceWand.maxLifetime <= 0.0 ||
        configuredIceWand.explosionRadius <= 0.0 ||
        configuredIceWand.freezeDuration <= 0.0 ||
        configuredIceWand.eliteFreezeMultiplier <= 0.0 ||
        configuredIceWand.eliteFreezeMultiplier > 1.0 ||
        configuredIceWand.bossSlowAmount < 0.0 ||
        configuredIceWand.bossSlowAmount > 1.0 ||
        configuredIceWand.chargeUpDuration < 0.0 ||
        configuredIceWand.areaDamageMultiplier <= 0.0 ||
        configuredIceWand.areaDamageMultiplier > 1.0 ||
        configuredFireWand.cooldown <= 0.0 ||
        configuredFireWand.directDamage <= 0.0 ||
        configuredFireWand.projectileSpeed <= 0.0 ||
        configuredFireWand.projectileRadius <= 0.0 ||
        configuredFireWand.maxLifetime <= 0.0 ||
        configuredFireWand.explosionRadius <= 0.0 ||
        configuredFireWand.burnDuration <= 0.0 ||
        configuredFireWand.burnDamagePerSecond <= 0.0 ||
        configuredFireWand.burnTickInterval <= 0.0 ||
        configuredFireWand.chargeUpDuration < 0.0 ||
        configuredFireWand.areaDamageMultiplier <= 0.0 ||
        configuredFireWand.areaDamageMultiplier > 1.0) {
        throw std::runtime_error("invalid weapon definition");
    }
    return definition;
}

EconomyBalanceDefinition parseEconomy(const Json& value) {
    const EconomyBalanceDefinition definition{
        .crystalMineInterval = value.at("crystalMineInterval").get<double>(),
        .crystalMineAmount = value.at("crystalMineAmount").get<int>(),
        .waveRewardBase = value.value("waveRewardBase", 0),
        .waveRewardPerWave = value.at("waveRewardPerWave").get<int>(),
        .bombPurchaseCoinCost = value.value("bombPurchaseCoinCost", 30),
        .bombPurchaseCoinCostPerWave =
            value.value("bombPurchaseCoinCostPerWave", 3),
        .bombPurchaseAmount = value.value("bombPurchaseAmount", 2),
        .chestRerollCoinCosts = value.value(
            "chestRerollCoinCosts", std::array<int, 3>{15, 30, 60}),
        .chestOpeningCoinCostPerWave =
            value.value("chestOpeningCoinCostPerWave", 4),
        .repairAllCoinCost = value.value("repairAllCoinCost", 35),
        .repairAllCoinCostPerWave =
            value.value("repairAllCoinCostPerWave", 6),
        .chestRevealCoinCost = value.value("chestRevealCoinCost", 20),
        .repairCostFraction = value.at("repairCostFraction").get<double>(),
        .repairCooldownSeconds =
            value.value("repairCooldownSeconds", 3.0),
        .sellRefundFraction = value.at("sellRefundFraction").get<double>(),
        .buildingUpgradeCostMultiplier =
            value.at("buildingUpgradeCostMultiplier").get<std::array<double, 2>>(),
        .buildingUpgradeCrystalBonus =
            value.at("buildingUpgradeCrystalBonus").get<std::array<int, 2>>(),
        .coreUpgradeCrystals = value.at("coreUpgradeCrystals").get<std::array<int, 2>>(),
    };
    if (definition.crystalMineInterval <= 0.0 || definition.crystalMineAmount <= 0 ||
        definition.waveRewardBase < 0 || definition.waveRewardPerWave < 0 ||
        definition.bombPurchaseCoinCost < 0 ||
        definition.bombPurchaseCoinCostPerWave < 0 ||
        definition.bombPurchaseAmount <= 0 ||
        std::ranges::any_of(
            definition.chestRerollCoinCosts,
            [](int cost) { return cost < 0; }) ||
        definition.chestOpeningCoinCostPerWave < 0 ||
        definition.repairAllCoinCost < 0 ||
        definition.repairAllCoinCostPerWave < 0 ||
        definition.chestRevealCoinCost < 0 ||
        definition.repairCostFraction < 0.0 ||
        definition.repairCostFraction > 1.0 ||
        definition.repairCooldownSeconds < 0.0 ||
        definition.sellRefundFraction < 0.0 ||
        definition.sellRefundFraction > 1.0 ||
        definition.buildingUpgradeCostMultiplier[0] <= 0.0 ||
        definition.buildingUpgradeCostMultiplier[1] <= 0.0 ||
        definition.buildingUpgradeCrystalBonus[0] < 0 ||
        definition.buildingUpgradeCrystalBonus[1] < 0 ||
        definition.coreUpgradeCrystals[0] < 0 || definition.coreUpgradeCrystals[1] < 0) {
        throw std::runtime_error("invalid economy definition");
    }
    return definition;
}

GameplayBalanceDefinition parseGameplay(const Json& value) {
    const GameplayBalanceDefinition definition{
        .eyeHeight = value.at("eyeHeight").get<double>(),
        .walkSpeed = value.at("walkSpeed").get<double>(),
        .sprintSpeed = value.at("sprintSpeed").get<double>(),
        .playerAcceleration =
            value.value("playerAcceleration", 36.0),
        .playerDeceleration =
            value.value("playerDeceleration", 48.0),
        .jumpSpeed = value.at("jumpSpeed").get<double>(),
        .gravity = value.at("gravity").get<double>(),
        .playerMaxHealth = value.at("playerMaxHealth").get<double>(),
        .playerRespawnSeconds =
            value.value("playerRespawnSeconds", 5.0),
        .playerDeathResourceLossFraction =
            value.value("playerDeathResourceLossFraction", 0.25),
        .fallDamageSafeSpeed =
            value.value(
                "fallDamageSafeSpeed",
                value.at("jumpSpeed").get<double>() + 5.5),
        .fallDamagePerSpeedSquared =
            value.value("fallDamagePerSpeedSquared", 3.5),
        .ropeFallDamageReduction =
            value.value("ropeFallDamageReduction", 0.45),
        .pickaxeRange = value.at("pickaxeRange").get<double>(),
        .resourceGatherRange = value.value(
            "resourceGatherRange",
            value.at("pickaxeRange").get<double>()),
        .pickaxeDamage = value.at("pickaxeDamage").get<double>(),
        .resourceToolDamageMultiplier =
            value.value("resourceToolDamageMultiplier", 0.65),
        .pickaxeDamageVariation =
            value.value("pickaxeDamageVariation", 0.2),
        .pickaxeCriticalChance =
            value.value("pickaxeCriticalChance", 0.15),
        .pickaxeCooldown = value.at("pickaxeCooldown").get<double>(),
        .axeStoneEfficiency =
            value.value("axeStoneEfficiency", 0.25),
        .pickaxeWoodEfficiency =
            value.value("pickaxeWoodEfficiency", 0.30),
        .firstBuildPhaseSeconds = value.at("firstBuildPhaseSeconds").get<double>(),
        .betweenWaveSeconds = value.at("betweenWaveSeconds").get<double>(),
        .sunsetSeconds = value.at("sunsetSeconds").get<double>(),
        .nightDurationSeconds =
            value.value("nightDurationSeconds", 90.0),
        .dawnSeconds = value.at("dawnSeconds").get<double>(),
        .minimumPlacementDistance = value.at("minimumPlacementDistance").get<double>(),
        .maximumPlacementDistance = value.at("maximumPlacementDistance").get<double>(),
    };
    if (definition.eyeHeight <= 0.0 || definition.walkSpeed <= 0.0 ||
        definition.sprintSpeed < definition.walkSpeed ||
        definition.playerAcceleration <= 0.0 ||
        definition.playerDeceleration <= 0.0 ||
        definition.jumpSpeed <= 0.0 ||
        definition.gravity <= 0.0 || definition.playerMaxHealth <= 0.0 ||
        definition.playerRespawnSeconds <= 0.0 ||
        definition.playerDeathResourceLossFraction < 0.0 ||
        definition.playerDeathResourceLossFraction > 1.0 ||
        definition.fallDamageSafeSpeed <= definition.jumpSpeed ||
        definition.fallDamagePerSpeedSquared <= 0.0 ||
        definition.ropeFallDamageReduction <= 0.0 ||
        definition.ropeFallDamageReduction > 1.0 ||
        definition.pickaxeRange <= 0.0 ||
        definition.resourceGatherRange <= 0.0 ||
        definition.pickaxeDamage <= 0.0 ||
        definition.resourceToolDamageMultiplier <= 0.0 ||
        definition.resourceToolDamageMultiplier > 1.0 ||
        definition.pickaxeDamageVariation < 0.0 ||
        definition.pickaxeDamageVariation >= 1.0 ||
        definition.pickaxeCriticalChance < 0.0 ||
        definition.pickaxeCriticalChance > 1.0 ||
        definition.pickaxeCooldown <= 0.0 ||
        definition.axeStoneEfficiency <= 0.0 ||
        definition.axeStoneEfficiency > 1.0 ||
        definition.pickaxeWoodEfficiency <= 0.0 ||
        definition.pickaxeWoodEfficiency > 1.0 ||
        definition.firstBuildPhaseSeconds <= 0.0 ||
        definition.betweenWaveSeconds <= 0.0 || definition.sunsetSeconds <= 0.0 ||
        definition.nightDurationSeconds <= 0.0 ||
        definition.dawnSeconds <= 0.0 || definition.minimumPlacementDistance <= 0.0 ||
        definition.maximumPlacementDistance < definition.minimumPlacementDistance) {
        throw std::runtime_error("invalid gameplay definition");
    }
    return definition;
}

} // namespace

GameBalance GameBalance::defaults() {
    return {
        .enemies = {{
            {5.0, 2.2, 10.0, 0.0, 1.0, 0.0},
            {4.0, 3.4, 7.0, 0.0, 1.0, 0.0},
            {16.0, 1.2, 25.0, 0.0, 1.0, 0.0},
            {70.0, 0.9, 50.0, 1.5, 3.0, 6.0},
            {7.0, 1.5, 8.0, 0.0, 1.0, 0.0},
            {10.0, 1.8, 12.0, 0.0, 1.0, 0.0},
            {5.0, 2.6, 8.0, 0.0, 1.0, 0.0},
            {12.0, 1.55, 18.0, 0.0, 1.0, 0.0},
            {3.0, 2.8, 7.0, 0.0, 1.0, 0.0},
        }},
        .waves = {{
            {15, 15, 0, 0, 0, 0, 0, 0, false, 5, 2.0},
            {25, 12, 5, 0, 1, 0, 0, 0, false, 6, 2.0},
            {40, 10, 5, 2, 2, 0, 0, 1, false, 7, 1.8},
            {55, 11, 8, 2, 2, 2, 0, 1, false, 8, 1.6},
            {75, 11, 8, 4, 2, 2, 2, 2, false, 9, 1.4},
            {100, 12, 10, 6, 4, 3, 2, 2, true, 10, 1.2},
        }},
        .buildings = {{
            {30, 0, 0, 500.0, 0, 1},
            {10, 0, 0, 100.0, 1, 256},
            {25, 15, 0, 120.0, 1, 64},
            {20, 10, 0, 140.0, 1, 8},
            {40, 30, 25, 180.0, 2, 64},
            {15, 20, 10, 100.0, 2, 64},
            {15, 5, 0, 130.0, 1, 128},
            {40, 15, 10, 150.0, 2, 8},
            {30, 40, 15, 170.0, 2, 8},
            {20, 25, 15, 100.0, 2, 64},
            {20, 10, 0, 180.0, 1, 8},
            {25, 5, 0, 200.0, 1, 8},
            {30, 25, 0, 160.0, 1, 8},
            {35, 25, 10, 150.0, 1, 64},
            {50, 45, 35, 210.0, 4, 64},
        }},
        .modularBuildings = {{
            {20, 5, 0},
            {5, 10, 0},
            {16, 4, 0},
        }},
        .weapons = {
            .rifle = {30.0, 2.0, 1.5, 0.25, 0.2, 1.5, 0.25, 8, 2, {40, 80}},
            .bomb = {0, 6.0, 4.0, 9.8, 2.2, 0.28, 4.0, 6.0, 8.0},
            .club = {1.35, 1.25, 0.0, 4.0},
            .iceWand = {0.95, 11.0, 18.0, 0.22, 2.5, 2.8, 1.4, 0.65,
                        0.35, 0.12, 0.45},
            .fireWand = {1.0, 8.0, 18.0, 0.24, 2.5, 2.7, 3.5, 2.0,
                         0.5, 0.14, 0.4},
        },
        .economy = {
            .crystalMineInterval = 8.0,
            .crystalMineAmount = 4,
            .waveRewardBase = 10,
            .waveRewardPerWave = 5,
            .bombPurchaseCoinCost = 30,
            .bombPurchaseCoinCostPerWave = 3,
            .bombPurchaseAmount = 2,
            .chestRerollCoinCosts = {15, 30, 60},
            .chestOpeningCoinCostPerWave = 4,
            .repairAllCoinCost = 35,
            .repairAllCoinCostPerWave = 6,
            .chestRevealCoinCost = 20,
            .repairCostFraction = 0.5,
            .repairCooldownSeconds = 3.0,
            .sellRefundFraction = 0.5,
            .buildingUpgradeCostMultiplier = {0.5, 1.0},
            .buildingUpgradeCrystalBonus = {10, 25},
            .coreUpgradeCrystals = {50, 100},
        },
        .gameplay = {1.7, 5.0, 8.0, 36.0, 48.0, 6.5, 18.0, 100.0, 5.0, 0.25,
                     12.0, 3.5, 0.45, 4.0, 4.0,
                     1.0, 0.65, 0.2, 0.15, 0.45, 0.25, 0.30,
                     60.0, 45.0, 6.0, 90.0, 5.0, 1.0, 10.0},
    };
}

GameBalanceLoadResult parseGameBalance(std::string_view enemiesJson,
                                       std::string_view wavesJson,
                                       std::string_view buildingsJson,
                                       std::string_view weaponsJson,
                                       std::string_view economyJson,
                                       std::string_view gameplayJson) {
    GameBalanceLoadResult result{.balance = GameBalance::defaults()};
    try {
        const Json enemies = Json::parse(enemiesJson);
        std::array<EnemyDefinition, GameBalance::EnemyTypeCount> parsed{{
            parseEnemy(enemies.at("basic"), false),
            parseEnemy(enemies.at("fast"), false),
            parseEnemy(enemies.at("heavy"), false),
            parseEnemy(enemies.at("boss"), true),
            enemies.contains("ranged")
                ? parseEnemy(enemies.at("ranged"), false)
                : result.balance.enemies[4],
            enemies.contains("sapper")
                ? parseEnemy(enemies.at("sapper"), false)
                : result.balance.enemies[5],
            enemies.contains("flying")
                ? parseEnemy(enemies.at("flying"), false)
                : result.balance.enemies[6],
            enemies.contains("splitter")
                ? parseEnemy(enemies.at("splitter"), false)
                : result.balance.enemies[7],
            enemies.contains("splitling")
                ? parseEnemy(enemies.at("splitling"), false)
                : result.balance.enemies[8],
        }};
        result.balance.enemies = parsed;
    } catch (const std::exception& error) {
        result.errors.push_back("enemies.json: " + std::string(error.what()));
    }

    try {
        const Json document = Json::parse(wavesJson);
        const Json& waves = document.at("waves");
        if (!waves.is_array() || waves.size() != GameBalance::WaveCount) {
            throw std::runtime_error(
                "expected six configured wave templates");
        }
        std::array<WaveDefinition, GameBalance::WaveCount> parsed{};
        for (std::size_t index = 0; index < parsed.size(); ++index) {
            parsed[index] = parseWave(waves.at(index));
        }
        if (!parsed.back().boss) {
            throw std::runtime_error(
                "sixth wave template must contain boss");
        }
        result.balance.waves = parsed;
    } catch (const std::exception& error) {
        result.errors.push_back("waves.json: " + std::string(error.what()));
    }

    try {
        const Json buildings = Json::parse(buildingsJson);
        std::array<BuildingBalanceDefinition, GameBalance::BuildingTypeCount> parsed{{
            parseBuilding(buildings.at("core")),
            parseBuilding(buildings.at("wall")),
            parseBuilding(buildings.at("turret")),
            parseBuilding(buildings.at("crystalMine")),
            parseBuilding(buildings.at("cannon")),
            parseBuilding(buildings.at("slowTrap")),
            parseBuilding(buildings.at("gate")),
            buildings.contains("lumberMill")
                ? parseBuilding(buildings.at("lumberMill"))
                : result.balance.buildings[7],
            buildings.contains("quarry")
                ? parseBuilding(buildings.at("quarry"))
                : result.balance.buildings[8],
            buildings.contains("spikeTrap")
                ? parseBuilding(buildings.at("spikeTrap"))
                : result.balance.buildings[9],
            buildings.contains("woodStorage")
                ? parseBuilding(buildings.at("woodStorage"))
                : result.balance.buildings[10],
            buildings.contains("stoneStorage")
                ? parseBuilding(buildings.at("stoneStorage"))
                : result.balance.buildings[11],
            buildings.contains("crystalStorage")
                ? parseBuilding(buildings.at("crystalStorage"))
                : result.balance.buildings[12],
            buildings.contains("gunTurret")
                ? parseBuilding(buildings.at("gunTurret"))
                : result.balance.buildings[13],
            buildings.contains("catapult")
                ? parseBuilding(buildings.at("catapult"))
                : result.balance.buildings[14],
        }};
        if (parsed[0].maxCount != 1 || parsed[0].unlockCoreLevel != 0) {
            throw std::runtime_error("core must be unique and unlocked");
        }
        result.balance.buildings = parsed;
        if (buildings.contains("modular")) {
            const Json& modular =
                buildings.at("modular");
            result.balance.modularBuildings = {{
                parseModularBuilding(
                    modular.at("platform")),
                parseModularBuilding(
                    modular.at("wall")),
                parseModularBuilding(
                    modular.at("ramp")),
            }};
        }
    } catch (const std::exception& error) {
        result.errors.push_back("buildings.json: " + std::string(error.what()));
    }

    try {
        result.balance.weapons = parseWeapons(Json::parse(weaponsJson));
    } catch (const std::exception& error) {
        result.errors.push_back("weapons.json: " + std::string(error.what()));
    }
    try {
        result.balance.economy = parseEconomy(Json::parse(economyJson));
    } catch (const std::exception& error) {
        result.errors.push_back("economy.json: " + std::string(error.what()));
    }
    try {
        result.balance.gameplay = parseGameplay(Json::parse(gameplayJson));
    } catch (const std::exception& error) {
        result.errors.push_back("gameplay.json: " + std::string(error.what()));
    }
    return result;
}

GameBalanceLoadResult loadGameBalance(std::string_view enemiesPath,
                                      std::string_view wavesPath,
                                      std::string_view buildingsPath,
                                      std::string_view weaponsPath,
                                      std::string_view economyPath,
                                      std::string_view gameplayPath) {
    std::string enemiesJson;
    std::string wavesJson;
    std::string buildingsJson;
    std::string weaponsJson;
    std::string economyJson;
    std::string gameplayJson;
    std::vector<std::string> readErrors;
    try {
        enemiesJson = readTextFile(enemiesPath);
    } catch (const std::exception& error) {
        readErrors.push_back(error.what());
    }
    try {
        wavesJson = readTextFile(wavesPath);
    } catch (const std::exception& error) {
        readErrors.push_back(error.what());
    }
    try {
        buildingsJson = readTextFile(buildingsPath);
    } catch (const std::exception& error) {
        readErrors.push_back(error.what());
    }
    try {
        weaponsJson = readTextFile(weaponsPath);
    } catch (const std::exception& error) {
        readErrors.push_back(error.what());
    }
    try {
        economyJson = readTextFile(economyPath);
    } catch (const std::exception& error) {
        readErrors.push_back(error.what());
    }
    try {
        gameplayJson = readTextFile(gameplayPath);
    } catch (const std::exception& error) {
        readErrors.push_back(error.what());
    }
    if (!readErrors.empty()) {
        return {.balance = GameBalance::defaults(), .errors = std::move(readErrors)};
    }
    return parseGameBalance(enemiesJson, wavesJson, buildingsJson, weaponsJson, economyJson,
                            gameplayJson);
}

} // namespace ian
