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
        .boss = value.value("boss", false),
        .groupSize = value.at("groupSize").get<int>(),
        .groupInterval = value.at("groupInterval").get<double>(),
    };
    if (definition.budget <= 0 || definition.basic < 0 || definition.fast < 0 ||
        definition.heavy < 0 || definition.ranged < 0 ||
        definition.sapper < 0 || definition.flying < 0 ||
        definition.groupSize <= 0 ||
        definition.groupInterval <= 0.0) {
        throw std::runtime_error("wave values must be non-negative");
    }
    const int calculatedBudget =
        definition.basic + definition.fast * 2 +
        definition.heavy * 5 + definition.ranged * 3 +
        definition.sapper * 4 + definition.flying * 3;
    if (calculatedBudget != definition.budget) {
        throw std::runtime_error("wave budget does not match composition");
    }
    const int spawnCount =
        definition.basic + definition.fast + definition.heavy +
        definition.ranged + definition.sapper + definition.flying +
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
        .gold = value.at("gold").get<int>(),
        .maxHealth = value.at("maxHealth").get<double>(),
        .unlockCoreLevel = value.at("unlockCoreLevel").get<int>(),
        .maxCount = value.at("maxCount").get<int>(),
    };
    if (definition.wood < 0 || definition.stone < 0 || definition.gold < 0 ||
        definition.maxHealth <= 0.0 || definition.unlockCoreLevel < 0 ||
        definition.unlockCoreLevel > 3 || definition.maxCount <= 0 ||
        definition.maxCount > 256) {
        throw std::runtime_error("invalid building definition");
    }
    return definition;
}

WeaponBalanceDefinition parseWeapons(const Json& document) {
    const Json& rifle = document.at("rifle");
    const Json& bomb = document.at("bomb");
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
            .upgradeGold = rifle.at("upgradeGold").get<std::array<int, 2>>(),
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
    };
    const auto& configuredRifle = definition.rifle;
    const auto& configuredBomb = definition.bomb;
    if (configuredRifle.range <= 0.0 || configuredRifle.damage <= 0.0 ||
        configuredRifle.damagePerLevel < 0.0 || configuredRifle.fireInterval <= 0.0 ||
        configuredRifle.fireRateBonusPerLevel < 0.0 || configuredRifle.reloadDuration <= 0.0 ||
        configuredRifle.reloadReductionPerLevel < 0.0 ||
        configuredRifle.reloadDuration - 2.0 * configuredRifle.reloadReductionPerLevel <= 0.0 ||
        configuredRifle.magazineSize <= 0 || configuredRifle.magazineBonusPerLevel < 0 ||
        configuredRifle.upgradeGold[0] < 0 || configuredRifle.upgradeGold[1] < 0 ||
        configuredBomb.startingBombs < 0 || configuredBomb.throwSpeed <= 0.0 ||
        configuredBomb.upwardSpeed < 0.0 || configuredBomb.gravity <= 0.0 ||
        configuredBomb.fuseDuration <= 0.0 || configuredBomb.groundHeight < 0.0 ||
        configuredBomb.explosionRadius <= 0.0 || configuredBomb.explosionDamage <= 0.0 ||
        configuredBomb.knockbackStrength < 0.0) {
        throw std::runtime_error("invalid weapon definition");
    }
    return definition;
}

EconomyBalanceDefinition parseEconomy(const Json& value) {
    const EconomyBalanceDefinition definition{
        .goldMineInterval = value.at("goldMineInterval").get<double>(),
        .goldMineAmount = value.at("goldMineAmount").get<int>(),
        .waveRewardPerWave = value.at("waveRewardPerWave").get<int>(),
        .repairCostFraction = value.at("repairCostFraction").get<double>(),
        .sellRefundFraction = value.at("sellRefundFraction").get<double>(),
        .buildingUpgradeCostMultiplier =
            value.at("buildingUpgradeCostMultiplier").get<std::array<double, 2>>(),
        .buildingUpgradeGoldBonus =
            value.at("buildingUpgradeGoldBonus").get<std::array<int, 2>>(),
        .coreUpgradeGold = value.at("coreUpgradeGold").get<std::array<int, 2>>(),
    };
    if (definition.goldMineInterval <= 0.0 || definition.goldMineAmount <= 0 ||
        definition.waveRewardPerWave < 0 || definition.repairCostFraction < 0.0 ||
        definition.repairCostFraction > 1.0 || definition.sellRefundFraction < 0.0 ||
        definition.sellRefundFraction > 1.0 ||
        definition.buildingUpgradeCostMultiplier[0] <= 0.0 ||
        definition.buildingUpgradeCostMultiplier[1] <= 0.0 ||
        definition.buildingUpgradeGoldBonus[0] < 0 ||
        definition.buildingUpgradeGoldBonus[1] < 0 ||
        definition.coreUpgradeGold[0] < 0 || definition.coreUpgradeGold[1] < 0) {
        throw std::runtime_error("invalid economy definition");
    }
    return definition;
}

GameplayBalanceDefinition parseGameplay(const Json& value) {
    const GameplayBalanceDefinition definition{
        .eyeHeight = value.at("eyeHeight").get<double>(),
        .walkSpeed = value.at("walkSpeed").get<double>(),
        .sprintSpeed = value.at("sprintSpeed").get<double>(),
        .jumpSpeed = value.at("jumpSpeed").get<double>(),
        .gravity = value.at("gravity").get<double>(),
        .playerMaxHealth = value.at("playerMaxHealth").get<double>(),
        .pickaxeRange = value.at("pickaxeRange").get<double>(),
        .pickaxeDamage = value.at("pickaxeDamage").get<double>(),
        .pickaxeDamageVariation =
            value.value("pickaxeDamageVariation", 0.2),
        .pickaxeCriticalChance =
            value.value("pickaxeCriticalChance", 0.15),
        .pickaxeCooldown = value.at("pickaxeCooldown").get<double>(),
        .firstBuildPhaseSeconds = value.at("firstBuildPhaseSeconds").get<double>(),
        .betweenWaveSeconds = value.at("betweenWaveSeconds").get<double>(),
        .sunsetSeconds = value.at("sunsetSeconds").get<double>(),
        .dawnSeconds = value.at("dawnSeconds").get<double>(),
        .minimumPlacementDistance = value.at("minimumPlacementDistance").get<double>(),
        .maximumPlacementDistance = value.at("maximumPlacementDistance").get<double>(),
    };
    if (definition.eyeHeight <= 0.0 || definition.walkSpeed <= 0.0 ||
        definition.sprintSpeed < definition.walkSpeed || definition.jumpSpeed <= 0.0 ||
        definition.gravity <= 0.0 || definition.playerMaxHealth <= 0.0 ||
        definition.pickaxeRange <= 0.0 || definition.pickaxeDamage <= 0.0 ||
        definition.pickaxeDamageVariation < 0.0 ||
        definition.pickaxeDamageVariation >= 1.0 ||
        definition.pickaxeCriticalChance < 0.0 ||
        definition.pickaxeCriticalChance > 1.0 ||
        definition.pickaxeCooldown <= 0.0 || definition.firstBuildPhaseSeconds <= 0.0 ||
        definition.betweenWaveSeconds <= 0.0 || definition.sunsetSeconds <= 0.0 ||
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
        }},
        .waves = {{
            {15, 15, 0, 0, 0, 0, 0, false, 5, 2.0},
            {25, 12, 5, 0, 1, 0, 0, false, 6, 2.0},
            {40, 14, 5, 2, 2, 0, 0, false, 7, 1.8},
            {55, 15, 8, 2, 2, 2, 0, false, 8, 1.6},
            {75, 19, 8, 4, 2, 2, 2, false, 9, 1.4},
            {100, 20, 10, 6, 4, 3, 2, true, 10, 1.2},
        }},
        .buildings = {{
            {30, 0, 0, 500.0, 0, 1},
            {10, 0, 0, 100.0, 1, 256},
            {25, 15, 0, 120.0, 1, 64},
            {20, 10, 0, 140.0, 1, 4},
            {40, 30, 25, 180.0, 2, 64},
            {15, 20, 10, 100.0, 2, 64},
            {15, 5, 0, 130.0, 1, 128},
            {40, 15, 10, 150.0, 2, 2},
            {30, 40, 15, 170.0, 2, 2},
        }},
        .weapons = {
            .rifle = {30.0, 2.0, 1.5, 0.25, 0.2, 1.5, 0.25, 8, 2, {40, 80}},
            .bomb = {3, 6.0, 4.0, 9.8, 1.2, 0.2, 4.0, 6.0, 8.0},
        },
        .economy = {5.0, 5, 15, 0.5, 0.5, {0.5, 1.0}, {10, 25}, {50, 100}},
        .gameplay = {1.7, 5.0, 8.0, 6.5, 18.0, 100.0, 4.0, 1.0,
                     0.2, 0.15, 0.45,
                     15.0, 45.0, 6.0, 5.0, 1.0, 10.0},
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
        }};
        result.balance.enemies = parsed;
    } catch (const std::exception& error) {
        result.errors.push_back("enemies.json: " + std::string(error.what()));
    }

    try {
        const Json document = Json::parse(wavesJson);
        const Json& waves = document.at("waves");
        if (!waves.is_array() || waves.size() != GameBalance::WaveCount) {
            throw std::runtime_error("expected exactly six waves");
        }
        std::array<WaveDefinition, GameBalance::WaveCount> parsed{};
        for (std::size_t index = 0; index < parsed.size(); ++index) {
            parsed[index] = parseWave(waves.at(index));
        }
        if (!parsed.back().boss) {
            throw std::runtime_error("final wave must contain boss");
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
            parseBuilding(buildings.at("goldMine")),
            parseBuilding(buildings.at("cannon")),
            parseBuilding(buildings.at("slowTrap")),
            parseBuilding(buildings.at("gate")),
            buildings.contains("lumberMill")
                ? parseBuilding(buildings.at("lumberMill"))
                : result.balance.buildings[7],
            buildings.contains("quarry")
                ? parseBuilding(buildings.at("quarry"))
                : result.balance.buildings[8],
        }};
        if (parsed[0].maxCount != 1 || parsed[0].unlockCoreLevel != 0) {
            throw std::runtime_error("core must be unique and unlocked");
        }
        result.balance.buildings = parsed;
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
