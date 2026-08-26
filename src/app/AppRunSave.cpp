#include "app/App.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>

namespace ian {
namespace {

using Json = nlohmann::json;
constexpr std::string_view SuspendedRunPath =
    "user_settings/suspended_run.json";
constexpr int SuspendedRunVersion = 4;
constexpr std::uintmax_t MaximumSuspendedRunBytes = 4U * 1024U * 1024U;

Json entityJson(EntityId id) {
    return {{"index", id.index}, {"generation", id.generation}};
}

EntityId readEntity(const Json& value) {
    return {
        value.at("index").get<std::uint32_t>(),
        value.at("generation").get<std::uint32_t>(),
    };
}

Json vectorJson(Vec3 value) {
    return {value.x, value.y, value.z};
}

Vec3 readVector(const Json& value) {
    if (!value.is_array() || value.size() != 3U) {
        throw Json::type_error::create(302, "invalid vector", &value);
    }
    return {
        value.at(0).get<double>(),
        value.at(1).get<double>(),
        value.at(2).get<double>(),
    };
}

template <typename T, std::size_t Size>
bool readFixedArray(const Json& document, std::string_view key,
                    std::array<T, Size>& destination) {
    const auto iterator = document.find(key);
    if (iterator == document.end() || !iterator->is_array() ||
        iterator->size() != Size) {
        return false;
    }
    for (std::size_t index = 0; index < Size; ++index) {
        destination[index] = iterator->at(index).get<T>();
    }
    return true;
}

template <typename T, std::size_t Size>
bool readExpandableFixedArray(
    const Json& document, std::string_view key,
    std::array<T, Size>& destination) {
    const auto iterator = document.find(key);
    if (iterator == document.end() || !iterator->is_array() ||
        iterator->size() > Size) {
        return false;
    }
    destination.fill(T{});
    for (std::size_t index = 0; index < iterator->size(); ++index) {
        destination[index] = iterator->at(index).get<T>();
    }
    return true;
}

Json buildingJson(const BuildingInstance& building) {
    return {
        {"idIndex", building.id.index},
        {"idGeneration", building.id.generation},
        {"type", static_cast<int>(building.type)},
        {"gridX", building.gridPosition.x},
        {"gridZ", building.gridPosition.z},
        {"rotation", building.rotation},
        {"level", building.level},
        {"health", building.health},
        {"maxHealth", building.maxHealth},
        {"open", building.open},
        {"baseHeight", building.baseHeight},
        {"platformStorey", building.platformStorey},
        {"foundationBottomHeight", building.foundationBottomHeight},
        {"anvilEnhanced", building.anvilEnhanced},
        {"anvilStacks", building.anvilStacks},
    };
}

BuildingInstance readBuilding(const Json& value) {
    return {
        .id = {value.at("idIndex").get<std::uint32_t>(),
               value.at("idGeneration").get<std::uint32_t>()},
        .type = static_cast<BuildingType>(value.at("type").get<int>()),
        .gridPosition = {value.at("gridX").get<int>(),
                         value.at("gridZ").get<int>()},
        .rotation = value.at("rotation").get<std::uint8_t>(),
        .level = value.at("level").get<std::uint8_t>(),
        .health = value.at("health").get<double>(),
        .maxHealth = value.at("maxHealth").get<double>(),
        .open = value.at("open").get<bool>(),
        .baseHeight = value.at("baseHeight").get<double>(),
        .platformStorey = value.at("platformStorey").get<int>(),
        .foundationBottomHeight =
            value.at("foundationBottomHeight").get<double>(),
        .anvilEnhanced = value.value("anvilEnhanced", false),
        .anvilStacks = static_cast<std::uint8_t>(
            value.value("anvilStacks", 0U)),
    };
}

Json platformFrameJson(const PlatformFrameInstance& frame) {
    return {
        {"id", entityJson(frame.id)},
        {"anchor", {frame.anchor.x, frame.anchor.yLevel, frame.anchor.z}},
        {"floorHeight", frame.floorHeight},
        {"storey", frame.storey},
        {"health", frame.health},
        {"maxHealth", frame.maxHealth},
        {"supportState", static_cast<int>(frame.supportState)},
    };
}

PlatformFrameInstance readPlatformFrame(const Json& value) {
    const Json& anchor = value.at("anchor");
    if (!anchor.is_array() || anchor.size() != 3U) {
        throw Json::type_error::create(302, "invalid grid anchor", &anchor);
    }
    return {
        .id = readEntity(value.at("id")),
        .anchor = {anchor.at(0).get<int>(), anchor.at(1).get<int>(),
                   anchor.at(2).get<int>()},
        .floorHeight = value.at("floorHeight").get<double>(),
        .storey = value.at("storey").get<int>(),
        .health = value.at("health").get<double>(),
        .maxHealth = value.at("maxHealth").get<double>(),
        .supportState = static_cast<StructuralSupportState>(
            value.at("supportState").get<int>()),
    };
}

Json wallJson(const WallInstance& wall) {
    return {
        {"id", entityJson(wall.id)},
        {"anchor", {wall.anchor.x, wall.anchor.yLevel, wall.anchor.z}},
        {"rotation", static_cast<int>(wall.rotation)},
        {"bottomHeight", wall.bottomHeight},
        {"topHeight", wall.topHeight},
        {"storey", wall.storey},
        {"health", wall.health},
        {"maxHealth", wall.maxHealth},
        {"supportState", static_cast<int>(wall.supportState)},
    };
}

WallInstance readWall(const Json& value) {
    const Json& anchor = value.at("anchor");
    if (!anchor.is_array() || anchor.size() != 3U) {
        throw Json::type_error::create(302, "invalid grid anchor", &anchor);
    }
    return {
        .id = readEntity(value.at("id")),
        .anchor = {anchor.at(0).get<int>(), anchor.at(1).get<int>(),
                   anchor.at(2).get<int>()},
        .rotation = static_cast<Rotation>(value.at("rotation").get<int>()),
        .bottomHeight = value.at("bottomHeight").get<double>(),
        .topHeight = value.at("topHeight").get<double>(),
        .storey = value.at("storey").get<int>(),
        .health = value.at("health").get<double>(),
        .maxHealth = value.at("maxHealth").get<double>(),
        .supportState = static_cast<StructuralSupportState>(
            value.at("supportState").get<int>()),
    };
}

Json rampJson(const RampInstance& ramp) {
    return {
        {"id", entityJson(ramp.id)},
        {"anchor", {ramp.anchor.x, ramp.anchor.yLevel, ramp.anchor.z}},
        {"rotation", static_cast<int>(ramp.rotation)},
        {"bottomHeight", ramp.bottomHeight},
        {"topHeight", ramp.topHeight},
        {"targetStorey", ramp.targetStorey},
        {"health", ramp.health},
        {"maxHealth", ramp.maxHealth},
        {"supportState", static_cast<int>(ramp.supportState)},
    };
}

RampInstance readRamp(const Json& value) {
    const Json& anchor = value.at("anchor");
    if (!anchor.is_array() || anchor.size() != 3U) {
        throw Json::type_error::create(302, "invalid grid anchor", &anchor);
    }
    return {
        .id = readEntity(value.at("id")),
        .anchor = {anchor.at(0).get<int>(), anchor.at(1).get<int>(),
                   anchor.at(2).get<int>()},
        .rotation = static_cast<Rotation>(value.at("rotation").get<int>()),
        .bottomHeight = value.at("bottomHeight").get<double>(),
        .topHeight = value.at("topHeight").get<double>(),
        .targetStorey = value.at("targetStorey").get<int>(),
        .health = value.at("health").get<double>(),
        .maxHealth = value.at("maxHealth").get<double>(),
        .supportState = static_cast<StructuralSupportState>(
            value.at("supportState").get<int>()),
    };
}

Json resourceNodeJson(const ResourceNode& node) {
    return {
        {"id", entityJson(node.id)},
        {"type", static_cast<int>(node.type)},
        {"position", vectorJson(node.position)},
        {"radius", node.radius},
        {"groundOffset", node.groundOffset},
        {"health", node.health},
        {"maxHealth", node.maxHealth},
        {"yield", node.yield},
        {"yieldRemaining", node.yieldRemaining},
        {"respawnSeconds", node.respawnSeconds},
        {"respawnRemaining", node.respawnRemaining},
        {"respawnGeneration", node.respawnGeneration},
        {"visualYaw", node.visualYaw},
        {"visualScale", node.visualScale},
        {"visualVariant", node.visualVariant},
        {"active", node.active},
    };
}

ResourceNode readResourceNode(const Json& value) {
    return {
        .id = readEntity(value.at("id")),
        .type = static_cast<ResourceType>(value.at("type").get<int>()),
        .position = readVector(value.at("position")),
        .radius = value.at("radius").get<double>(),
        .groundOffset = value.at("groundOffset").get<double>(),
        .health = value.at("health").get<double>(),
        .maxHealth = value.at("maxHealth").get<double>(),
        .yield = value.at("yield").get<int>(),
        .yieldRemaining = value.at("yieldRemaining").get<int>(),
        .respawnSeconds = value.at("respawnSeconds").get<double>(),
        .respawnRemaining = value.at("respawnRemaining").get<double>(),
        .respawnGeneration =
            value.at("respawnGeneration").get<std::uint32_t>(),
        .visualYaw = value.at("visualYaw").get<double>(),
        .visualScale = value.at("visualScale").get<double>(),
        .visualVariant = value.at("visualVariant").get<std::size_t>(),
        .active = value.at("active").get<bool>(),
    };
}

Json chestJson(const LootChestInstance& chest) {
    return {
        {"id", entityJson(chest.id)},
        {"type", static_cast<int>(chest.type)},
        {"purpose", static_cast<int>(chest.purpose)},
        {"state", static_cast<int>(chest.state)},
        {"position", vectorJson(chest.position)},
        {"surfaceNormal", vectorJson(chest.surfaceNormal)},
        {"yaw", chest.yaw},
        {"coinCost", chest.coinCost},
        {"rerollCount", chest.rerollCount},
        {"rerollProgress", chest.rerollProgress},
        {"rerollTargetEffect", static_cast<int>(chest.rerollTargetEffect)},
        {"rerollTargetRarity", static_cast<int>(chest.rerollTargetRarity)},
        {"rerolling", chest.rerolling},
        {"openingProgress", chest.openingProgress},
        {"disappearanceDelayRemaining", chest.disappearanceDelayRemaining},
        {"disappearanceProgress", chest.disappearanceProgress},
        {"looseLoot", chest.looseLoot},
        {"revealed", chest.revealed},
        {"loot", {
            {"id", entityJson(chest.loot.id)},
            {"rarity", static_cast<int>(chest.loot.rarity)},
            {"effect", static_cast<int>(chest.loot.effect)},
            {"position", vectorJson(chest.loot.position)},
            {"revealProgress", chest.loot.revealProgress},
            {"hoverTime", chest.loot.hoverTime},
            {"pickupDelayRemaining", chest.loot.pickupDelayRemaining},
            {"proximityPickupRadius", chest.loot.proximityPickupRadius},
            {"available", chest.loot.available},
            {"collected", chest.loot.collected},
        }},
    };
}

LootChestInstance readChest(const Json& value) {
    const Json& loot = value.at("loot");
    return {
        .id = readEntity(value.at("id")),
        .type = static_cast<LootChestType>(value.at("type").get<int>()),
        .purpose = static_cast<LootChestPurpose>(
            value.at("purpose").get<int>()),
        .state = static_cast<LootChestState>(value.at("state").get<int>()),
        .position = readVector(value.at("position")),
        .surfaceNormal = readVector(value.at("surfaceNormal")),
        .yaw = value.at("yaw").get<double>(),
        .coinCost = value.at("coinCost").get<int>(),
        .rerollCount = value.at("rerollCount").get<std::uint32_t>(),
        .rerollProgress = value.at("rerollProgress").get<double>(),
        .rerollTargetEffect = static_cast<LootUpgradeEffect>(
            value.at("rerollTargetEffect").get<int>()),
        .rerollTargetRarity = static_cast<LootRarity>(
            value.at("rerollTargetRarity").get<int>()),
        .rerolling = value.at("rerolling").get<bool>(),
        .openingProgress = value.at("openingProgress").get<double>(),
        .disappearanceDelayRemaining =
            value.at("disappearanceDelayRemaining").get<double>(),
        .disappearanceProgress =
            value.at("disappearanceProgress").get<double>(),
        .looseLoot = value.at("looseLoot").get<bool>(),
        .revealed = value.at("revealed").get<bool>(),
        .loot = {
            .id = readEntity(loot.at("id")),
            .rarity = static_cast<LootRarity>(loot.at("rarity").get<int>()),
            .effect = static_cast<LootUpgradeEffect>(
                loot.at("effect").get<int>()),
            .position = readVector(loot.at("position")),
            .revealProgress = loot.at("revealProgress").get<double>(),
            .hoverTime = loot.at("hoverTime").get<double>(),
            .pickupDelayRemaining =
                loot.at("pickupDelayRemaining").get<double>(),
            .proximityPickupRadius =
                loot.at("proximityPickupRadius").get<double>(),
            .available = loot.at("available").get<bool>(),
            .collected = loot.at("collected").get<bool>(),
        },
    };
}

Json challengeColumnJson(const ChallengeColumnInstance& column) {
    return {
        {"id", entityJson(column.id)},
        {"position", vectorJson(column.position)},
        {"yaw", column.yaw},
        {"state", static_cast<int>(column.state)},
        {"completionProgress", column.completionProgress},
        {"fenceProgress", column.fenceProgress},
        {"enemyBudget", column.enemyBudget},
    };
}

ChallengeColumnInstance readChallengeColumn(const Json& value) {
    return {
        .id = readEntity(value.at("id")),
        .position = readVector(value.at("position")),
        .yaw = value.at("yaw").get<double>(),
        .state = static_cast<ChallengeColumnState>(
            value.at("state").get<int>()),
        .completionProgress =
            value.at("completionProgress").get<double>(),
        .fenceProgress = value.at("fenceProgress").get<double>(),
        .enemyBudget = value.at("enemyBudget").get<int>(),
    };
}

Json worldLandmarkJson(const WorldLandmarkInstance& landmark) {
    return {
        {"id", entityJson(landmark.id)},
        {"type", static_cast<int>(landmark.type)},
        {"position", vectorJson(landmark.position)},
        {"yaw", landmark.yaw},
        {"collisionRadius", landmark.collisionRadius},
        {"activationCoinCost", landmark.activationCoinCost},
        {"activated", landmark.activated},
        {"productionProgress", landmark.productionProgress},
    };
}

WorldLandmarkInstance readWorldLandmark(const Json& value) {
    return {
        .id = readEntity(value.at("id")),
        .type = static_cast<WorldLandmarkType>(value.at("type").get<int>()),
        .position = readVector(value.at("position")),
        .yaw = value.at("yaw").get<double>(),
        .collisionRadius = value.at("collisionRadius").get<double>(),
        .activationCoinCost = value.at("activationCoinCost").get<int>(),
        .activated = value.at("activated").get<bool>(),
        .productionProgress = value.at("productionProgress").get<double>(),
    };
}

Json insightJson(const InsightRunState& insight) {
    return {
        {"progress", {
            {"current", insight.progress.currentInsight},
            {"required", insight.progress.requiredInsight},
            {"treePoints", insight.progress.totalLevelsEarned},
            {"total", insight.progress.totalInsightEarned},
        }},
        {"cycleBaseEarned", insight.cycleBaseEarned},
        {"earnedByCategory", insight.earnedByCategory},
        {"earnedBySource", insight.earnedBySource},
        {"consumedEventIds", insight.consumedEventIds},
        {"blockedDuplicateEvents", insight.blockedDuplicateEvents},
    };
}

InsightRunState readInsight(const Json& value) {
    InsightRunState insight;
    const Json& progress = value.at("progress");
    insight.progress = {
        .currentInsight = progress.at("current").get<double>(),
        .requiredInsight = progress.at("required").get<double>(),
        .totalLevelsEarned = progress.at("treePoints").get<int>(),
        .totalInsightEarned = progress.at("total").get<double>(),
    };
    if (!readFixedArray(value, "cycleBaseEarned", insight.cycleBaseEarned) ||
        !readFixedArray(value, "earnedByCategory", insight.earnedByCategory) ||
        !readFixedArray(value, "earnedBySource", insight.earnedBySource)) {
        throw Json::type_error::create(302, "invalid insight array", &value);
    }
    insight.consumedEventIds =
        value.at("consumedEventIds").get<std::vector<std::uint64_t>>();
    insight.blockedDuplicateEvents =
        value.at("blockedDuplicateEvents").get<std::uint64_t>();
    return insight;
}

Json objectivesJson(const ObjectiveRunState& objectives) {
    Json statuses = Json::array();
    for (const ObjectiveSavedStatus& status : objectives.statuses) {
        statuses.push_back({
            {"id", status.id}, {"progress", status.progress},
            {"completed", status.completed}, {"active", status.active},
            {"cycle", status.cycle},
        });
    }
    Json recent = Json::array();
    for (const auto& [time, amount] : objectives.recentGathering) {
        recent.push_back({time, amount});
    }
    return {
        {"statuses", std::move(statuses)},
        {"challengeCycle", objectives.challengeCycle},
        {"totalTreesDestroyed", objectives.totalTreesDestroyed},
        {"totalStonesDestroyed", objectives.totalStonesDestroyed},
        {"totalCrystalsGathered", objectives.totalCrystalsGathered},
        {"totalResourcesGathered", objectives.totalResourcesGathered},
        {"dayWoodGathered", objectives.dayWoodGathered},
        {"dayStoneGathered", objectives.dayStoneGathered},
        {"dayCrystalsGathered", objectives.dayCrystalsGathered},
        {"consecutiveDepletions", objectives.consecutiveDepletions},
        {"largeDepositsDepleted", objectives.largeDepositsDepleted},
        {"bareHandsDepletions", objectives.bareHandsDepletions},
        {"nightResourcesGathered", objectives.nightResourcesGathered},
        {"farResourcesGathered", objectives.farResourcesGathered},
        {"recentGathering", std::move(recent)},
        {"eventMetricProgress", objectives.eventMetricProgress},
    };
}

ObjectiveRunState readObjectives(const Json& value) {
    ObjectiveRunState objectives;
    for (const Json& status : value.at("statuses")) {
        objectives.statuses.push_back({
            .id = status.at("id").get<std::string>(),
            .progress = status.at("progress").get<double>(),
            .completed = status.at("completed").get<bool>(),
            .active = status.at("active").get<bool>(),
            .cycle = status.at("cycle").get<int>(),
        });
    }
    objectives.challengeCycle = value.at("challengeCycle").get<int>();
    objectives.totalTreesDestroyed =
        value.at("totalTreesDestroyed").get<int>();
    objectives.totalStonesDestroyed =
        value.at("totalStonesDestroyed").get<int>();
    objectives.totalCrystalsGathered =
        value.at("totalCrystalsGathered").get<int>();
    objectives.totalResourcesGathered =
        value.at("totalResourcesGathered").get<int>();
    objectives.dayWoodGathered = value.at("dayWoodGathered").get<int>();
    objectives.dayStoneGathered = value.at("dayStoneGathered").get<int>();
    objectives.dayCrystalsGathered =
        value.at("dayCrystalsGathered").get<int>();
    objectives.consecutiveDepletions =
        value.at("consecutiveDepletions").get<int>();
    objectives.largeDepositsDepleted =
        value.at("largeDepositsDepleted").get<int>();
    objectives.bareHandsDepletions =
        value.at("bareHandsDepletions").get<int>();
    objectives.nightResourcesGathered =
        value.at("nightResourcesGathered").get<int>();
    objectives.farResourcesGathered =
        value.at("farResourcesGathered").get<int>();
    for (const Json& recent : value.at("recentGathering")) {
        if (!recent.is_array() || recent.size() != 2U) {
            throw Json::type_error::create(
                302, "invalid recent gathering entry", &recent);
        }
        objectives.recentGathering.emplace_back(
            recent.at(0).get<double>(), recent.at(1).get<int>());
    }
    objectives.eventMetricProgress =
        value.at("eventMetricProgress").get<std::vector<int>>();
    return objectives;
}

Json suspendedRunJson(const SuspendedRunState& saved) {
    Json buildings = Json::array();
    for (const BuildingInstance& building : saved.buildings) {
        buildings.push_back(buildingJson(building));
    }
    Json platformFrames = Json::array();
    for (const PlatformFrameInstance& frame : saved.platformFrames) {
        platformFrames.push_back(platformFrameJson(frame));
    }
    Json walls = Json::array();
    for (const WallInstance& wall : saved.modularWalls) {
        walls.push_back(wallJson(wall));
    }
    Json ramps = Json::array();
    for (const RampInstance& ramp : saved.ramps) {
        ramps.push_back(rampJson(ramp));
    }
    Json resources = Json::array();
    for (const ResourceNode& node : saved.resourceNodes) {
        resources.push_back(resourceNodeJson(node));
    }
    Json chests = Json::array();
    for (const LootChestInstance& chest : saved.lootChests) {
        chests.push_back(chestJson(chest));
    }
    Json challenges = Json::array();
    for (const ChallengeColumnInstance& column : saved.challengeColumns) {
        challenges.push_back(challengeColumnJson(column));
    }
    Json landmarks = Json::array();
    for (const WorldLandmarkInstance& landmark : saved.worldLandmarks) {
        landmarks.push_back(worldLandmarkJson(landmark));
    }
    Json upgradeChoices = Json::array();
    for (const ProgressionCardId choice : saved.runUpgradeChoices) {
        upgradeChoices.push_back(choice);
    }
    return {
        {"version", SuspendedRunVersion},
        {"playerClass", static_cast<int>(saved.playerClass)},
        {"terrainSeed", saved.terrainSeed},
        {"resume", {
            {"state", static_cast<int>(saved.resumeState)},
            {"tick", saved.tick},
            {"elapsedSeconds", saved.elapsedSeconds},
            {"phaseTimeRemaining", saved.phaseTimeRemaining},
            {"phaseDuration", saved.phaseDuration},
            {"stageCleared", saved.stageCleared},
            {"finalNight", saved.finalNight},
        }},
        {"wave", saved.wave},
        {"bestWave", saved.bestWave},
        {"playerPosition", {saved.playerPosition.x,
                            saved.playerPosition.y,
                            saved.playerPosition.z}},
        {"playerYaw", saved.playerYaw},
        {"playerPitch", saved.playerPitch},
        {"playerHealth", saved.playerHealth},
        {"wood", saved.wood}, {"stone", saved.stone},
        {"crystals", saved.crystals}, {"coins", saved.coins},
        {"bombs", saved.bombs},
        {"selectedWeapon", static_cast<int>(saved.selectedWeapon)},
        {"rifleLevel", saved.rifleLevel},
        {"skillPoints", saved.skillTree.points},
        {"unlockedNodeIds", saved.skillTree.unlockedNodeIds},
        {"lootStacks", saved.lootStacks},
        {"runUpgradeStacks", saved.runUpgradeStacks},
        {"buildings", std::move(buildings)},
        {"modularStructures", {
            {"platformFrames", std::move(platformFrames)},
            {"walls", std::move(walls)},
            {"ramps", std::move(ramps)},
        }},
        {"world", {
            {"resourceNodes", std::move(resources)},
            {"lootChests", std::move(chests)},
            {"challengeColumns", std::move(challenges)},
            {"landmarks", std::move(landmarks)},
        }},
        {"buildingBlueprintLevels", saved.buildingBlueprintLevels},
        {"coreBuildRadius", saved.coreBuildRadius},
        {"multipliers", {
            {"playerDamage", saved.playerDamageMultiplier},
            {"runPlayerDamage", saved.runPlayerDamageMultiplier},
            {"playerAttackSpeed", saved.playerAttackSpeedMultiplier},
            {"playerMoveSpeed", saved.playerMoveSpeedMultiplier},
            {"runPlayerMoveSpeed", saved.runPlayerMoveSpeedMultiplier},
            {"playerArmor", saved.playerArmorMultiplier},
            {"playerMaxHealth", saved.playerMaxHealthMultiplier},
            {"buildingMaxHealth", saved.buildingMaxHealthMultiplier},
            {"runBuildingMaxHealth", saved.runBuildingMaxHealthMultiplier},
            {"productionSpeed", saved.productionSpeedMultiplier},
            {"runProductionSpeed", saved.runProductionSpeedMultiplier},
            {"defenseDamage", saved.defenseDamageMultiplier},
            {"defenseFireRate", saved.defenseFireRateMultiplier},
            {"woodYield", saved.woodYieldMultiplier},
            {"chestOpeningCost", saved.chestOpeningCostMultiplier},
        }},
        {"runtime", {
            {"playerBonusMaxHealth", saved.playerBonusMaxHealth},
            {"playerTemporaryHealth", saved.playerTemporaryHealth},
            {"playerRecoverableArmor", saved.playerRecoverableArmor},
            {"playerMaxRecoverableArmor", saved.playerMaxRecoverableArmor},
            {"appleAvailable", saved.appleAvailable},
            {"breadWellFed", saved.breadWellFed},
            {"battlePotionAvailable", saved.battlePotionAvailable},
            {"freeChestOpeningAvailable", saved.freeChestOpeningAvailable},
            {"freeChestRerollsRemaining", saved.freeChestRerollsRemaining},
            {"runNightlyBombBonus", saved.runNightlyBombBonus},
            {"runUpgradeRerollTokens", saved.runUpgradeRerollTokens},
            {"runUpgradeLockUnlocked", saved.runUpgradeLockUnlocked},
            {"lockedRunUpgrade", saved.lockedRunUpgrade
                ? Json(static_cast<int>(*saved.lockedRunUpgrade))
                : Json(nullptr)},
            {"bonusSelectionsNextReward", saved.bonusSelectionsNextReward},
            {"riskyInvestmentPending", saved.riskyInvestmentPending},
            {"bloodHarvestKillProgress", saved.bloodHarvestKillProgress},
            {"bareHandsWoodGathered", saved.bareHandsWoodGathered},
            {"bareHandsStoneGathered", saved.bareHandsStoneGathered},
            {"introSkillObjectiveCompleted",
             saved.introSkillObjectiveCompleted},
        }},
        {"runUpgradeOffer", {
            {"pending", saved.runUpgradeChoicePending},
            {"count", saved.runUpgradeChoiceCount},
            {"choices", std::move(upgradeChoices)},
            {"selectionsRemaining", saved.runUpgradeSelectionsRemaining},
            {"generation", saved.runUpgradeOfferGeneration},
        }},
        {"progression", {
            {"insight", insightJson(saved.insight)},
            {"objectives", objectivesJson(saved.objectives)},
        }},
        {"statistics", {
            {"wavesSurvived", saved.runStatistics.wavesSurvived},
            {"enemiesDefeated", saved.runStatistics.enemiesDefeated},
            {"playerDamageDealt", saved.runStatistics.playerDamageDealt},
            {"defenseDamageDealt", saved.runStatistics.defenseDamageDealt},
            {"woodAcquired", saved.runStatistics.woodAcquired},
            {"stoneAcquired", saved.runStatistics.stoneAcquired},
            {"crystalsAcquired", saved.runStatistics.crystalsAcquired},
            {"coinsCollected", saved.runStatistics.coinsCollected},
            {"structuresBuilt", saved.runStatistics.structuresBuilt},
            {"structuresLost", saved.runStatistics.structuresLost},
        }},
    };
}

SuspendedRunState readSuspendedRun(const Json& document) {
    SuspendedRunState saved;
    saved.playerClass = static_cast<PlayerClass>(
        document.at("playerClass").get<int>());
    saved.terrainSeed = document.at("terrainSeed").get<std::uint32_t>();
    const Json& resume = document.at("resume");
    saved.resumeState = static_cast<RunState>(
        resume.at("state").get<int>());
    saved.tick = resume.at("tick").get<std::uint64_t>();
    saved.elapsedSeconds = resume.at("elapsedSeconds").get<double>();
    saved.phaseTimeRemaining =
        resume.at("phaseTimeRemaining").get<double>();
    saved.phaseDuration = resume.at("phaseDuration").get<double>();
    saved.stageCleared = resume.at("stageCleared").get<bool>();
    saved.finalNight = resume.at("finalNight").get<bool>();
    saved.wave = document.at("wave").get<int>();
    saved.bestWave = document.at("bestWave").get<int>();
    saved.playerPosition = readVector(document.at("playerPosition"));
    saved.playerYaw = document.at("playerYaw").get<double>();
    saved.playerPitch = document.at("playerPitch").get<double>();
    saved.playerHealth = document.at("playerHealth").get<double>();
    saved.wood = document.at("wood").get<int>();
    saved.stone = document.at("stone").get<int>();
    saved.crystals = document.at("crystals").get<int>();
    saved.coins = document.at("coins").get<int>();
    saved.bombs = document.at("bombs").get<int>();
    saved.selectedWeapon = static_cast<PlayerWeapon>(
        document.at("selectedWeapon").get<int>());
    saved.rifleLevel = document.at("rifleLevel").get<int>();
    saved.skillTree.points = document.at("skillPoints").get<int>();
    saved.skillTree.unlockedNodeIds =
        document.at("unlockedNodeIds").get<std::vector<std::string>>();
    if (!readFixedArray(document, "lootStacks", saved.lootStacks) ||
        !readExpandableFixedArray(
            document, "runUpgradeStacks",
            saved.runUpgradeStacks) ||
        !readFixedArray(document, "buildingBlueprintLevels",
                        saved.buildingBlueprintLevels)) {
        throw Json::type_error::create(302, "invalid checkpoint array", &document);
    }
    for (const Json& building : document.at("buildings")) {
        saved.buildings.push_back(readBuilding(building));
    }
    const Json& structures = document.at("modularStructures");
    for (const Json& frame : structures.at("platformFrames")) {
        saved.platformFrames.push_back(readPlatformFrame(frame));
    }
    for (const Json& wall : structures.at("walls")) {
        saved.modularWalls.push_back(readWall(wall));
    }
    for (const Json& ramp : structures.at("ramps")) {
        saved.ramps.push_back(readRamp(ramp));
    }
    const Json& world = document.at("world");
    for (const Json& resource : world.at("resourceNodes")) {
        saved.resourceNodes.push_back(readResourceNode(resource));
    }
    for (const Json& chest : world.at("lootChests")) {
        saved.lootChests.push_back(readChest(chest));
    }
    for (const Json& column : world.at("challengeColumns")) {
        saved.challengeColumns.push_back(readChallengeColumn(column));
    }
    for (const Json& landmark : world.at("landmarks")) {
        saved.worldLandmarks.push_back(readWorldLandmark(landmark));
    }
    saved.coreBuildRadius = document.at("coreBuildRadius").get<int>();
    const Json& multipliers = document.at("multipliers");
    saved.playerDamageMultiplier = multipliers.at("playerDamage");
    saved.runPlayerDamageMultiplier = multipliers.at("runPlayerDamage");
    saved.playerAttackSpeedMultiplier = multipliers.at("playerAttackSpeed");
    saved.playerMoveSpeedMultiplier = multipliers.at("playerMoveSpeed");
    saved.runPlayerMoveSpeedMultiplier = multipliers.at("runPlayerMoveSpeed");
    saved.playerArmorMultiplier = multipliers.at("playerArmor");
    saved.playerMaxHealthMultiplier = multipliers.at("playerMaxHealth");
    saved.buildingMaxHealthMultiplier = multipliers.at("buildingMaxHealth");
    saved.runBuildingMaxHealthMultiplier = multipliers.at("runBuildingMaxHealth");
    saved.productionSpeedMultiplier = multipliers.at("productionSpeed");
    saved.runProductionSpeedMultiplier = multipliers.at("runProductionSpeed");
    saved.defenseDamageMultiplier = multipliers.at("defenseDamage");
    saved.defenseFireRateMultiplier = multipliers.at("defenseFireRate");
    saved.woodYieldMultiplier = multipliers.at("woodYield");
    saved.chestOpeningCostMultiplier = multipliers.at("chestOpeningCost");
    const Json& runtime = document.at("runtime");
    saved.playerBonusMaxHealth = runtime.at("playerBonusMaxHealth");
    saved.playerTemporaryHealth = runtime.at("playerTemporaryHealth");
    saved.playerRecoverableArmor = runtime.at("playerRecoverableArmor");
    saved.playerMaxRecoverableArmor = runtime.at("playerMaxRecoverableArmor");
    saved.appleAvailable = runtime.at("appleAvailable");
    saved.breadWellFed = runtime.at("breadWellFed");
    saved.battlePotionAvailable = runtime.at("battlePotionAvailable");
    saved.freeChestOpeningAvailable = runtime.at("freeChestOpeningAvailable");
    saved.freeChestRerollsRemaining = runtime.at("freeChestRerollsRemaining");
    saved.runNightlyBombBonus = runtime.at("runNightlyBombBonus");
    saved.runUpgradeRerollTokens = runtime.at("runUpgradeRerollTokens");
    saved.runUpgradeLockUnlocked = runtime.at("runUpgradeLockUnlocked");
    if (!runtime.at("lockedRunUpgrade").is_null()) {
        saved.lockedRunUpgrade = static_cast<RunUpgradeEffect>(
            runtime.at("lockedRunUpgrade").get<int>());
    }
    saved.bonusSelectionsNextReward = runtime.at("bonusSelectionsNextReward");
    saved.riskyInvestmentPending = runtime.at("riskyInvestmentPending");
    saved.bloodHarvestKillProgress = runtime.at("bloodHarvestKillProgress");
    saved.bareHandsWoodGathered = runtime.at("bareHandsWoodGathered");
    saved.bareHandsStoneGathered = runtime.at("bareHandsStoneGathered");
    saved.introSkillObjectiveCompleted =
        runtime.at("introSkillObjectiveCompleted");
    const Json& upgradeOffer = document.at("runUpgradeOffer");
    saved.runUpgradeChoicePending = upgradeOffer.at("pending").get<bool>();
    saved.runUpgradeChoiceCount = upgradeOffer.at("count").get<std::size_t>();
    if (!readFixedArray(
            upgradeOffer, "choices", saved.runUpgradeChoices)) {
        throw Json::type_error::create(
            302, "invalid run upgrade choices", &upgradeOffer);
    }
    saved.runUpgradeSelectionsRemaining =
        upgradeOffer.at("selectionsRemaining").get<int>();
    saved.runUpgradeOfferGeneration =
        upgradeOffer.at("generation").get<std::uint32_t>();
    const Json& progression = document.at("progression");
    saved.insight = readInsight(progression.at("insight"));
    saved.objectives = readObjectives(progression.at("objectives"));
    const Json& statistics = document.at("statistics");
    saved.runStatistics = {
        .wavesSurvived = statistics.at("wavesSurvived").get<int>(),
        .enemiesDefeated = statistics.at("enemiesDefeated").get<int>(),
        .playerDamageDealt =
            statistics.at("playerDamageDealt").get<double>(),
        .defenseDamageDealt =
            statistics.at("defenseDamageDealt").get<double>(),
        .woodAcquired = statistics.at("woodAcquired").get<int>(),
        .stoneAcquired = statistics.at("stoneAcquired").get<int>(),
        .crystalsAcquired = statistics.at("crystalsAcquired").get<int>(),
        .coinsCollected = statistics.at("coinsCollected").get<int>(),
        .structuresBuilt = statistics.at("structuresBuilt").get<int>(),
        .structuresLost = statistics.at("structuresLost").get<int>(),
    };
    return saved;
}

std::filesystem::path suspendedRunPath() {
    return std::filesystem::path{SuspendedRunPath};
}

std::filesystem::path suspendedRunTemporaryPath() {
    return suspendedRunPath().string() + ".tmp";
}

std::filesystem::path suspendedRunBackupPath() {
    return suspendedRunPath().string() + ".bak";
}

std::optional<SuspendedRunState> readSuspendedRunFile(
    const std::filesystem::path& path) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size == 0U || size > MaximumSuspendedRunBytes) {
        return std::nullopt;
    }
    try {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) return std::nullopt;
        Json document;
        stream >> document;
        if (!stream || !document.is_object() ||
            document.value("version", 0) != SuspendedRunVersion) {
            return std::nullopt;
        }
        return readSuspendedRun(document);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<SuspendedRunState> readStoredSuspendedRun() {
    if (auto saved = readSuspendedRunFile(suspendedRunPath())) {
        return saved;
    }
    return readSuspendedRunFile(suspendedRunBackupPath());
}

bool replaceSuspendedRunFile(
    const std::filesystem::path& temporary,
    const std::filesystem::path& destination) {
    const std::filesystem::path backup = suspendedRunBackupPath();
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (!error) {
        std::filesystem::remove(backup, error);
        return true;
    }

    error.clear();
    if (!std::filesystem::exists(destination, error) || error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    std::filesystem::remove(backup, error);
    error.clear();
    std::filesystem::rename(destination, backup, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    std::filesystem::rename(temporary, destination, error);
    if (!error) {
        std::filesystem::remove(backup, error);
        return true;
    }

    std::error_code restoreError;
    std::filesystem::rename(backup, destination, restoreError);
    std::filesystem::remove(temporary, restoreError);
    return false;
}

} // namespace

bool App::saveSuspendedRun() {
    const SimulationSnapshot& snapshot = simulation_.snapshot();
    const RunState state = snapshot.state;
    if (snapshot.sandboxMode ||
        state == RunState::MainMenu || state == RunState::Victory ||
        state == RunState::Defeat ||
        snapshot.playerClass == PlayerClass::None) {
        return false;
    }
    const std::filesystem::path path = suspendedRunPath();
    const std::filesystem::path temporary = suspendedRunTemporaryPath();
    try {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
        const std::string payload =
            suspendedRunJson(simulation_.saveSuspendedRunState()).dump();
        if (payload.empty() || payload.size() > MaximumSuspendedRunBytes) {
            return false;
        }
        std::ofstream stream(
            temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream.write(
            payload.data(), static_cast<std::streamsize>(payload.size()));
        stream.flush();
        stream.close();
        if (!stream.good() || !replaceSuspendedRunFile(temporary, path)) {
            std::filesystem::remove(temporary, error);
            return false;
        }
    } catch (...) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        return false;
    }
    suspendedRunAvailabilityCache_ = true;
    persistMetaProgression();
    return true;
}

bool App::suspendedRunAvailable() const {
    if (suspendedRunAvailabilityCache_) {
        return *suspendedRunAvailabilityCache_;
    }
    suspendedRunAvailabilityCache_ = readStoredSuspendedRun().has_value();
    return *suspendedRunAvailabilityCache_;
}

std::optional<std::uint32_t> App::suspendedRunTerrainSeed() const {
    const auto saved = readStoredSuspendedRun();
    suspendedRunAvailabilityCache_ = saved.has_value();
    return !saved || saved->terrainSeed == 0U
        ? std::nullopt
        : std::optional<std::uint32_t>{saved->terrainSeed};
}

void App::discardSuspendedRun() {
    std::error_code error;
    std::filesystem::remove(suspendedRunPath(), error);
    std::filesystem::remove(suspendedRunTemporaryPath(), error);
    std::filesystem::remove(suspendedRunBackupPath(), error);
    std::filesystem::remove(
        std::filesystem::path{"user_settings/suspended_run.bin"}, error);
    suspendedRunAvailabilityCache_ = false;
}

bool App::loadSuspendedRun() {
    try {
        const auto stored = readStoredSuspendedRun();
        if (!stored) return false;
        const SuspendedRunState& saved = *stored;
        const bool terrainGraphicsAlreadyMatch =
            simulation_.terrain().seed() == saved.terrainSeed;
        if (!simulation_.loadSuspendedRunState(saved)) return false;
        selectedPlayerClass_ = saved.playerClass;
        fixedStep_.reset();
        resetRunInputState();
        resetEquipmentActionMode(simulation_.snapshot());
        if (terrainGraphicsAlreadyMatch) {
            refreshDecorationExclusions(simulation_.snapshot());
        } else {
            rebuildTerrainGraphics();
        }
        simulation_.takeEvents(gameEventBuffer_);
        gameEventBuffer_.clear();
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace ian
