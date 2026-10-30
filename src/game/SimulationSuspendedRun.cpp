#include "game/Simulation.hpp"

#include "buildings/BuildingOrientation.hpp"
#include "buildings/CoreProgression.hpp"
#include "core/DeterministicRandom.hpp"

#include <algorithm>
#include <set>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace ian {

namespace {

// Preserve stage-clear and endless-night checkpoints from the old schedule.
constexpr int LegacyStageClearWave = 10;
constexpr int MaximumSavedCounter = 1'000'000'000;
constexpr int MaximumSavedStack = 1000;
constexpr double MaximumSavedTime = 1'000'000'000.0;

RunState effectiveRunState(RunState state, RunState stateBeforePause) {
    return state == RunState::Paused ? stateBeforePause : state;
}

bool finiteNonNegative(double value, double maximum = MaximumSavedTime) {
    return std::isfinite(value) && value >= 0.0 && value <= maximum;
}

bool validRunUpgrade(RunUpgradeEffect effect) {
    const int value = static_cast<int>(effect);
    return value >= 0 &&
           value < static_cast<int>(RunUpgradeEffectCount);
}

bool samePosition(Vec3 left, Vec3 right, double tolerance = 1e-5) {
    return std::abs(left.x - right.x) <= tolerance &&
           std::abs(left.y - right.y) <= tolerance &&
           std::abs(left.z - right.z) <= tolerance;
}

} // namespace

SuspendedRunState Simulation::saveSuspendedRunState() const {
    const RunState activeState = effectiveRunState(
        state_, stateBeforePause_);
    if (!finalNight_ && (activeState == RunState::Sunset ||
         activeState == RunState::Wave) &&
        waveStartCheckpoint_) {
        auto checkpoint = *waveStartCheckpoint_;
        // Replaying a night must not rewind the accumulated pressure clock.
        checkpoint.overtimeSeconds = overtimeSeconds_;
        checkpoint.runUpgradeRerollTokens=runUpgradeRerollTokens_;
        checkpoint.runUpgradeSkipTokens=runUpgradeSkipTokens_;
        checkpoint.runUpgradeBanishTokens=runUpgradeBanishTokens_;
        checkpoint.banishedRunUpgrades=banishedRunUpgrades_;
        checkpoint.metaSilverPickups = coinPickups_.metaSilverState();
        return checkpoint;
    }
    SuspendedRunState saved{
        .buildingArsenalVersion = 5,
        .playerClass = playerClass_,
        .metaBonuses = activeMetaBonuses_,
        .terrainSeed = terrain_.seed(),
        .resumeState = activeState,
        .tick = tick_,
        .elapsedSeconds = elapsedSeconds_,
        .overtimeSeconds = overtimeSeconds_,
        .phaseTimeRemaining = phaseTimeRemaining_,
        .phaseDuration = phaseDuration_,
        .wave = wave_,
        .bestWave = bestWave_,
        .playerPosition = playerPosition_,
        .playerYaw = playerYaw_,
        .playerPitch = playerPitch_,
        .playerHealth = playerHealth_,
        .wood = wood_,
        .stone = stone_,
        .crystals = crystals_,
        .coins = coins_,
        .bombs = 0,
        .bombCooldownRemaining = bombs_.cooldownRemaining(),
        .selectedWeapon = playerWeapons_.selectedWeapon(),
        .rifleLevel = playerWeapons_.rifleLevel(),
        .classAbilityState = classAbilityState_,
        .powerups = powerups_.state(),
        .metaSilverPickups = coinPickups_.metaSilverState(),
        .lootStacks = lootStacks_,
        .relics = relics_,
        .runUpgradeStacks = runUpgradeStacks_,
        .runUpgradePower = runUpgradePower_,
        .buildingUpgradePowers = buildingUpgradePowers_,
        .repairStationCycleRemaining = repairStationCycleRemaining_,
        .buildings = buildings_.buildings(),
        .producerRuntimes = crystalMines_.mines(),
        .resourceNodes = resources_.nodes(),
        .lootChests = lootChests_.chests(),
        .challengeColumns = challengeColumns_,
        .worldLandmarks = worldLandmarks_,
        .buildingBlueprintLevels = buildings_.blueprintLevels(),
        .coreBuildRadius = buildings_.coreBuildRadius(),
        .coreBuildRadiusLevel = buildings_.core() ? buildings_.core()->level : 1,
        .coreCardPowers = coreCardPowers_,
        .coreHealthBaseLevel = coreHealthBaseLevel_,
        .coreBuildingBaseLevel = coreBuildingBaseLevel_,
        .coreStorageBaseLevel = coreStorageBaseLevel_,
        .playerWeaponArsenalVersion = 2,
        .resourceNodeCapacityVersion = 1,
        .playerDamageMultiplier = playerDamageMultiplier_,
        .runPlayerDamageMultiplier = runPlayerDamageMultiplier_,
        .playerAttackSpeedMultiplier = playerAttackSpeedMultiplier_,
        .toolAttackSpeedMultiplier = toolAttackSpeedMultiplier_,
        .playerMoveSpeedMultiplier = playerMoveSpeedMultiplier_,
        .runPlayerMoveSpeedMultiplier = runPlayerMoveSpeedMultiplier_,
        .playerArmorMultiplier = playerArmorMultiplier_,
        .playerMaxHealthMultiplier = playerMaxHealthMultiplier_,
        .buildingMaxHealthMultiplier = buildingMaxHealthMultiplier_,
        .runBuildingMaxHealthMultiplier = runBuildingMaxHealthMultiplier_,
        .productionSpeedMultiplier = productionSpeedMultiplier_,
        .runProductionSpeedMultiplier = runProductionSpeedMultiplier_,
        .defenseDamageMultiplier = defenseDamageMultiplier_,
        .defenseFireRateMultiplier = defenseFireRateMultiplier_,
        .woodYieldMultiplier = woodYieldMultiplier_,
        .chestOpeningCostMultiplier = chestOpeningCostMultiplier_,
        .openedPaidChestCount = lootChests_.openedPaidChestCount(),
        .playerBonusMaxHealth = playerBonusMaxHealth_,
        .playerTemporaryHealth = playerTemporaryHealth_,
        .playerRecoverableArmor = playerRecoverableArmor_,
        .playerMaxRecoverableArmor = playerMaxRecoverableArmor_,
        .appleAvailable = appleAvailable_,
        .breadWellFed = breadWellFed_,
        .battlePotionAvailable = battlePotionAvailable_,
        .freeChestOpeningAvailable = freeChestOpeningAvailable_,
        .battlePotionBerserkRemaining = battlePotionBerserkRemaining_,
        .battlePotionBerserkDuration = battlePotionBerserkDuration_,
        .battlePotionLifestealRemaining = battlePotionLifestealRemaining_,
        .freeChestRerollsRemaining = freeChestRerollsRemaining_,
        .runNightlyBombBonus = runNightlyBombBonus_,
        .runUpgradeRerollTokens = runUpgradeRerollTokens_,
        .runUpgradeSkipTokens=runUpgradeSkipTokens_,
        .runUpgradeBanishTokens=runUpgradeBanishTokens_,
        .banishedRunUpgrades=banishedRunUpgrades_,
        .runUpgradeLockUnlocked = runUpgradeLockUnlocked_,
        .lockedRunUpgrade = lockedRunUpgrade_,
        .bonusSelectionsNextReward = bonusSelectionsNextReward_,
        .riskyInvestmentPending = 0,
        .bloodHarvestKillProgress = bloodHarvestKillProgress_,
        .insight = insight_.saveState(),
        .objectives = objectives_.saveState(),
        .runUpgradeChoicePending = runUpgradeChoicePending_,
        .runShop = runShop_,
        .shrines = shrines_,
        .runUpgradeChoiceCount = runUpgradeChoiceCount_,
        .runUpgradePaidChoicePrice = runUpgradePaidChoicePrice_,
        .runUpgradeChoices = runUpgradeChoices_,
        .runUpgradeRarities = runUpgradeRarities_,
        .runUpgradeRollSeeds = runUpgradeRollSeeds_,
        .runUpgradeSelectionsRemaining = runUpgradeSelectionsRemaining_,
        .runUpgradeOfferGeneration = runUpgradeOfferGeneration_,
        .stageCleared = stageCleared_,
        .finalNight = finalNight_,
        .runStatistics = runStatistics_,
    };
    saved.platformFrames.assign(
        foundations_.platformFrames().begin(),
        foundations_.platformFrames().end());
    saved.modularWalls.assign(
        foundations_.walls().begin(), foundations_.walls().end());
    saved.ramps.assign(
        foundations_.ramps().begin(), foundations_.ramps().end());

    // Combat actors are intentionally transient. An interrupted regular
    // night is replayed from its preceding build phase; an endless-night
    // pressure tier resumes automatically without a dawn or a choice menu.
    if (activeState == RunState::Sunset) {
        saved.resumeState = RunState::BuildPhase;
        saved.phaseTimeRemaining = 0.0;
        saved.phaseDuration = 0.0;
    } else if (activeState == RunState::Wave) {
        if (finalNight_) {
            saved.resumeState = RunState::StageClear;
            saved.wave = std::max(LegacyStageClearWave, wave_ - 1);
            saved.stageCleared = true;
            saved.finalNight = true;
            saved.phaseTimeRemaining = 0.0;
            saved.phaseDuration = 0.0;
        } else {
            saved.resumeState = RunState::BuildPhase;
            saved.wave = std::max(0, wave_ - 1);
            saved.phaseTimeRemaining = gameplay_.betweenWaveSeconds;
            saved.phaseDuration = saved.phaseTimeRemaining;
        }
    }
    return saved;
}

bool Simulation::loadSuspendedRunState(
    const SuspendedRunState& saved) {
    if (saved.classAbilityState.stumpCombo < 0 || saved.classAbilityState.stumpCombo > 3) return false;
    if (!validRelicState(saved.relics,map_.worldLimit+ModularRampRunCells*worldConfig_.cellSize,worldConfig_.maxStoreys)) return false;
    if (!std::isfinite(saved.repairStationCycleRemaining) || saved.repairStationCycleRemaining <= 0 ||
        saved.repairStationCycleRemaining > RepairStationInterval) return false;
    auto metaBonuses=saved.metaBonuses;
    const auto legacySlotBonus=[](int value,int maximum) {
        return value>=0 && value<=4 ? std::min(value,maximum) : value;
    };
    metaBonuses.additionalTomeSlots=legacySlotBonus(metaBonuses.additionalTomeSlots,
        MaximumRunTomeSlots-BaseRunTomeSlots);
    metaBonuses.additionalBuildingSlots=legacySlotBonus(metaBonuses.additionalBuildingSlots,
        MaximumRunBuildingSlots-BaseRunBuildingSlots);
    if (!validMetaRunBonuses(metaBonuses) || !PowerupSystem::validState(saved.powerups)) return false;
    if (!CoinPickupSystem::validMetaSilverState(saved.metaSilverPickups)) return false;
    const auto invalidMultiplier = [](double value) {
        return !std::isfinite(value) || value <= 0.0 || value > static_cast<double>(std::numeric_limits<int>::max()) / 16.0;
    };
    const std::array multipliers{
        saved.playerDamageMultiplier,
        saved.runPlayerDamageMultiplier,
        saved.playerAttackSpeedMultiplier,
        saved.toolAttackSpeedMultiplier,
        saved.playerMoveSpeedMultiplier,
        saved.runPlayerMoveSpeedMultiplier,
        saved.playerArmorMultiplier,
        saved.playerMaxHealthMultiplier,
        saved.buildingMaxHealthMultiplier,
        saved.runBuildingMaxHealthMultiplier,
        saved.productionSpeedMultiplier,
        saved.runProductionSpeedMultiplier,
        saved.defenseDamageMultiplier,
        saved.defenseFireRateMultiplier,
        saved.woodYieldMultiplier,
        saved.chestOpeningCostMultiplier,
    };
    const auto invalidStack = [](int value) {
        return value < 0 || value > MaximumSavedStack;
    };
    const bool invalidStacks =
        std::ranges::any_of(saved.lootStacks, invalidStack) ||
        std::ranges::any_of(saved.runUpgradeStacks, [](int value) { return value < 0; });
    const bool invalidUpgradePower = std::ranges::any_of(saved.runUpgradePower,
        [](double power) { return !std::isfinite(power) || power < 0.0 || power > static_cast<double>(std::numeric_limits<int>::max()) / 16.0; });
    const bool invalidBuildingUpgradePower = std::ranges::any_of(saved.buildingUpgradePowers,
        [](const auto& stats) {
            return std::ranges::any_of(stats, [](double power) {
                return !std::isfinite(power) || power < 0.0 || power > MaximumSavedStack;
            });
        });
    const bool invalidArsenal = saved.buildingArsenalVersion < 0 || saved.buildingArsenalVersion > 5 ||
        std::ranges::any_of(RunUpgradeDefinitions, [&](const RunUpgradeDefinition& definition) {
            if (!runUpgradeBuilding(definition.effect)) return false;
            const auto index = runUpgradeIndex(definition.effect);
            const int maximum = saved.buildingArsenalVersion >= 3 && runDefenseCard(definition.effect) &&
                (runCombatBuilding(*runUpgradeBuilding(definition.effect)) || saved.buildingArsenalVersion >= 5)
                ? MaxStructureLevel : 1;
            if (definition.effect == RunUpgradeEffect::UnlockWall)
                return saved.runUpgradeStacks[index] > maximum ||
                    saved.runUpgradePower[index] > maximum * runUpgradeRarityMultiplier(RunUpgradeRarity::Legendary);
            return saved.runUpgradeStacks[index] > maximum || saved.runUpgradePower[index] > maximum ||
                (saved.runUpgradePower[index] != 0.0 &&
                 saved.runUpgradePower[index] != static_cast<double>(saved.runUpgradeStacks[index]));
        });
    const bool invalidWeapons = saved.resourceNodeCapacityVersion < 0 || saved.resourceNodeCapacityVersion > 1 ||
        saved.playerWeaponArsenalVersion < 0 || saved.playerWeaponArsenalVersion > 2 ||
        std::ranges::any_of(RunUpgradeDefinitions, [&](const RunUpgradeDefinition& definition) {
            if (!runUpgradeWeapon(definition.effect)) return false;
            const auto index = runUpgradeIndex(definition.effect);
            const int maximum=saved.playerWeaponArsenalVersion>=2 && runWeaponLevelCard(definition.effect) ? MaximumPlayerWeaponLevel : 1;
            return saved.runUpgradeStacks[index] > maximum || saved.runUpgradePower[index] > maximum ||
                (saved.runUpgradePower[index] != 0.0 &&
                 saved.runUpgradePower[index] != static_cast<double>(saved.runUpgradeStacks[index]));
        });
    int cannonStyles = 0;
    bool invalidCannonStyle = false;
    for (const auto& definition : RunUpgradeDefinitions) {
        if (!runUpgradeCannonStyle(definition.effect)) continue;
        const auto index = runUpgradeIndex(definition.effect);
        cannonStyles += saved.runUpgradeStacks[index] > 0 ? 1 : 0;
        invalidCannonStyle = invalidCannonStyle || saved.runUpgradeStacks[index] > 1 ||
            saved.runUpgradePower[index] != static_cast<double>(saved.runUpgradeStacks[index]);
    }
    invalidCannonStyle = invalidCannonStyle || cannonStyles > 1;
    const double coordinateLimit = map_.worldLimit +
        static_cast<double>(ModularRampRunCells) * worldConfig_.cellSize;
    std::size_t coreCount = 0U;
    const bool invalidBuildings = saved.buildings.size() > 1024U ||
        std::ranges::any_of(
            saved.buildings, [&](const BuildingInstance& building) {
                const int type = static_cast<int>(building.type);
                if (building.type == BuildingType::Core) {
                    ++coreCount;
                }
                return type < 0 ||
                    type >= static_cast<int>(GameBalance::BuildingTypeCount) ||
                    building.rotation >=
                        buildingRotationStepCount(building.type) ||
                    building.level == 0U ||
                    building.level > buildingLevelLimit(building.type) ||
                    !std::isfinite(building.health) ||
                    !std::isfinite(building.maxHealth) ||
                    building.health <= 0.0 || building.maxHealth <= 0.0 ||
                    building.maxHealth > 1'000'000'000.0 ||
                    building.health > building.maxHealth ||
                    std::abs(static_cast<double>(building.gridPosition.x)) >
                        coordinateLimit ||
                    std::abs(static_cast<double>(building.gridPosition.z)) >
                        coordinateLimit ||
                    !std::isfinite(building.baseHeight) ||
                    std::abs(building.baseHeight) > 1000.0 ||
                    building.platformStorey < -1 ||
                    building.platformStorey >= worldConfig_.maxStoreys ||
                    !std::isfinite(building.foundationBottomHeight) ||
                    std::abs(building.foundationBottomHeight) > 1000.0;
            }) ||
        coreCount > 1U;
    const auto savedCore = std::ranges::find(
        saved.buildings, BuildingType::Core, &BuildingInstance::type);
    const int savedCoreLevel = savedCore != saved.buildings.end() ? savedCore->level : 1;
    const bool invalidCoreCards =
        std::ranges::any_of(saved.coreCardPowers, [](double power) {
            return !std::isfinite(power) || power < 0.0 || power > 510.0;
        }) ||
        saved.coreHealthBaseLevel < 1 || saved.coreHealthBaseLevel > MaxStructureLevel ||
        saved.coreBuildingBaseLevel < 1 || saved.coreBuildingBaseLevel > MaxStructureLevel ||
        saved.coreStorageBaseLevel < 1 || saved.coreStorageBaseLevel > MaxBuildingLevel;
    const bool invalidRadiusLevel = saved.coreBuildRadiusLevel < 1 ||
        saved.coreBuildRadiusLevel > savedCoreLevel;
    const long long restoredBuildRadius = static_cast<long long>(saved.coreBuildRadius) +
        CoreBuildRadiusPerLevel * (static_cast<long long>(savedCoreLevel) - saved.coreBuildRadiusLevel);
    const int selectedWeapon = static_cast<int>(saved.selectedWeapon);
    const bool invalidLockedUpgrade = saved.lockedRunUpgrade &&
        !validRunUpgrade(*saved.lockedRunUpgrade);
    const bool invalidShop = saved.runShop.wave < 0 || saved.runShop.wave > saved.wave ||
        saved.runShop.rerolls < 0 || saved.runShop.rerolls > MaximumSavedCounter ||
        (saved.runShop.pending && saved.runShop.wave != saved.wave) ||
        std::ranges::any_of(saved.runShop.offers, [&](const auto& offer) {
            return !validRunUpgrade(offer.effect) ||
                static_cast<std::size_t>(offer.rarity) >= RunUpgradeRarityCount ||
                offer.price < 0 || (offer.present && offer.price == 0) ||
                (offer.locked && !offer.present);
        });
    if (invalidShop) return false;
    std::unordered_set<RunUpgradeEffect> shopEffects;
    for (const auto& offer : saved.runShop.offers)
        if (offer.present && !shopEffects.insert(offer.effect).second) return false;
    const bool validResumeState =
        saved.resumeState == RunState::Gathering ||
        saved.resumeState == RunState::BuildPhase ||
        saved.resumeState == RunState::WaveComplete ||
        saved.resumeState == RunState::StageClear;
    bool invalidChoices =
        saved.runUpgradeChoiceCount > MaximumRunUpgradeChoices ||
        saved.runUpgradePaidChoicePrice < 0 ||
        saved.runUpgradeSelectionsRemaining < 0 ||
        saved.runUpgradeSelectionsRemaining > MaximumSavedStack;
    std::unordered_set<RunUpgradeEffect> offeredUpgrades;
    for (std::size_t index = 0;
         index < saved.runUpgradeChoiceCount; ++index) {
        const RunUpgradeEffect card = saved.runUpgradeChoices[index];
        invalidChoices = invalidChoices ||
            !validRunUpgrade(card) ||
            static_cast<std::size_t>(saved.runUpgradeRarities[index]) >= RunUpgradeRarityCount ||
            !offeredUpgrades.insert(card).second;
    }
    if (saved.runUpgradeChoicePending) {
        invalidChoices = invalidChoices ||
            saved.runUpgradeChoiceCount == 0 ||
            saved.runUpgradeSelectionsRemaining <= 0;
    } else if (saved.runUpgradeSelectionsRemaining != 0) {
        invalidChoices = true;
    }
    if (!validShrineRunState(saved.shrines,saved.lootStacks[lootUpgradeIndex(LootUpgradeEffect::UpgradeMirror)]>0?4.0:2.0) || (saved.shrines.offer!=ShrineKind::None &&
        (!saved.runUpgradeChoicePending || saved.runUpgradeChoiceCount!=3))) return false;
    if (saved.shrines.offer!=ShrineKind::None)
        for (std::size_t i=0;i<3;++i)
            if (!shrineBlessing(saved.shrines.offer,saved.runUpgradeChoices[i])) return false;
    const auto invalidCounter = [](int value) {
        return value < 0 || value > MaximumSavedCounter;
    };
    const std::array counters{
        saved.wood, saved.stone, saved.crystals, saved.coins,
        saved.bombs, saved.freeChestRerollsRemaining,
        saved.runNightlyBombBonus, saved.runUpgradeRerollTokens,
        saved.runUpgradeSkipTokens, saved.runUpgradeBanishTokens,
        saved.bonusSelectionsNextReward, saved.riskyInvestmentPending,
        saved.bloodHarvestKillProgress,
    };
    if (!finiteNonNegative(saved.bombCooldownRemaining) ||
        saved.bombCooldownRemaining > bombs_.baseCooldownDuration() /
            playerClassLevelBonuses(saved.playerClass, 1).rangedSpeed) return false;
    const RunCombatStatistics& statistics = saved.runStatistics;
    const bool invalidStatistics =
        invalidCounter(statistics.wavesSurvived) ||
        invalidCounter(statistics.enemiesDefeated) ||
        invalidCounter(statistics.woodAcquired) ||
        invalidCounter(statistics.stoneAcquired) ||
        invalidCounter(statistics.crystalsAcquired) ||
        invalidCounter(statistics.coinsCollected) ||
        invalidCounter(statistics.structuresBuilt) ||
        invalidCounter(statistics.structuresLost) ||
        !finiteNonNegative(statistics.playerDamageDealt) ||
        !finiteNonNegative(statistics.defenseDamageDealt);
    const bool invalidWorldCollections =
        saved.resourceNodes.size() > 4096U ||
        saved.lootChests.size() > 2048U ||
        saved.openedPaidChestCount < 0 ||
        saved.openedPaidChestCount > 1'000'000 ||
        saved.challengeColumns.size() > 64U ||
        saved.worldLandmarks.size() > 64U ||
        std::ranges::any_of(
            saved.resourceNodes, [coordinateLimit](const ResourceNode& node) {
                return std::abs(node.position.x) > coordinateLimit ||
                       std::abs(node.position.z) > coordinateLimit ||
                       std::abs(node.position.y) > 1000.0;
            }) ||
        std::ranges::any_of(
            saved.lootChests,
            [coordinateLimit](const LootChestInstance& chest) {
                return std::abs(chest.position.x) > coordinateLimit ||
                       std::abs(chest.position.z) > coordinateLimit ||
                       std::abs(chest.position.y) > 1000.0;
            });
    const bool invalidPhase =
        !validResumeState ||
        !finiteNonNegative(saved.elapsedSeconds) ||
        !finiteNonNegative(saved.overtimeSeconds) ||
        !finiteNonNegative(saved.phaseTimeRemaining) ||
        !finiteNonNegative(saved.phaseDuration) ||
        saved.phaseTimeRemaining > saved.phaseDuration + 1e-6 ||
        (saved.finalNight && saved.resumeState!=RunState::StageClear) ||
        (saved.resumeState == RunState::StageClear
             ? (!saved.stageCleared || saved.wave < LegacyStageClearWave)
             : saved.stageCleared);
    if (state_ != RunState::MainMenu ||
        saved.playerClass == PlayerClass::None ||
        playerClassDefinition(saved.playerClass) == nullptr ||
        saved.terrainSeed == 0U || saved.wave < 0 ||
        saved.wave > MaximumSavedCounter || saved.bestWave < 0 ||
        saved.bestWave > MaximumSavedCounter ||
        std::ranges::any_of(counters, invalidCounter) ||
        saved.rifleLevel < 1 || saved.rifleLevel > MaximumSavedStack ||
        selectedWeapon < 0 ||
        selectedWeapon >= static_cast<int>(PlayerWeaponCount) ||
        invalidWeapons || invalidCannonStyle || invalidArsenal || invalidStacks || invalidUpgradePower || invalidBuildingUpgradePower || invalidBuildings || invalidLockedUpgrade ||
        invalidChoices || invalidStatistics || invalidWorldCollections ||
        invalidPhase || invalidRadiusLevel || invalidCoreCards || saved.coreBuildRadius < map_.coreBuildRadius ||
        restoredBuildRadius > static_cast<long long>(std::ceil(map_.worldLimit * 2.0)) ||
        saved.coreBuildRadius >
            static_cast<int>(std::ceil(map_.worldLimit * 2.0)) ||
        std::ranges::any_of(multipliers, invalidMultiplier) ||
        !std::isfinite(saved.playerHealth) || saved.playerHealth <= 0.0 ||
        saved.playerHealth > 1'000'000'000.0 ||
        !std::isfinite(saved.playerPosition.x) ||
        !std::isfinite(saved.playerPosition.y) ||
        !std::isfinite(saved.playerPosition.z) ||
        std::abs(saved.playerPosition.x) > coordinateLimit ||
        std::abs(saved.playerPosition.z) > coordinateLimit ||
        std::abs(saved.playerPosition.y) > 1000.0 ||
        !std::isfinite(saved.playerYaw) ||
        !std::isfinite(saved.playerPitch) ||
        std::abs(saved.playerYaw) > MaximumSavedTime ||
        std::abs(saved.playerPitch) > 3.14159265358979323846 ||
        !finiteNonNegative(saved.classAbilityState.rageRemaining) ||
        !finiteNonNegative(saved.classAbilityState.exhaustionRemaining) ||
        !finiteNonNegative(saved.classAbilityState.stumpPulseRemaining) ||
        saved.classAbilityState.stumpPulseRemaining > .65 ||
        saved.classAbilityState.rageRemaining > 60 ||
        saved.classAbilityState.exhaustionRemaining > 60 ||
        !finiteNonNegative(saved.battlePotionBerserkRemaining) ||
        !finiteNonNegative(saved.battlePotionBerserkDuration) ||
        !finiteNonNegative(saved.battlePotionLifestealRemaining) ||
        saved.battlePotionBerserkRemaining > saved.battlePotionBerserkDuration ||
        saved.battlePotionBerserkDuration > MaximumSavedTime ||
        saved.battlePotionLifestealRemaining > 1000000 ||
        !finiteNonNegative(saved.playerBonusMaxHealth) ||
        !finiteNonNegative(saved.playerTemporaryHealth) ||
        !finiteNonNegative(saved.playerRecoverableArmor) ||
        !finiteNonNegative(saved.playerMaxRecoverableArmor) ||
        saved.playerRecoverableArmor > saved.playerMaxRecoverableArmor +
            (saved.playerClass == PlayerClass::Vanguard
                ? 2.0 * static_cast<double>(std::max(0, saved.insight.progress.totalLevelsEarned)) : 0.0) ||
        saved.insight.consumedEventIds.size() > 100'000U ||
        saved.objectives.statuses.size() > 4096U ||
        saved.objectives.recentGathering.size() > 100'000U) {
        return false;
    }

    InsightSystem insightValidation = insight_;
    ObjectiveSystem objectiveValidation = objectives_;
    if (!insightValidation.loadState(saved.insight) ||
        !objectiveValidation.loadState(saved.objectives)) {
        return false;
    }
    static_cast<void>(configureNextRunBonuses(metaBonuses));
    // Generate legacy maps with their original two landmarks before challenge
    // sites/scenery are selected, preserving all seeded placements on resume.
    forcedRunTerrainSeed_=saved.terrainSeed;
    sandboxMode_=false;
    playerClass_=saved.playerClass;
    resetRun(GameEventType::RunStarted,saved.worldLandmarks.size()!=2U);
    const auto failLoad = [this]() {
        enemies_.reset();
        waveStartCheckpoint_.reset();
        state_ = RunState::MainMenu;
        stateBeforePause_ = RunState::BuildPhase;
        selectedBuilding_.reset();
        buildingPreview_.reset();
        events_.clear();
        invalidateSnapshotCache();
        return false;
    };

    tick_ = saved.tick;
    elapsedSeconds_ = saved.elapsedSeconds;
    wave_ = saved.wave;
    bestWave_ = std::max(saved.bestWave, saved.wave);
    playerPosition_ = saved.playerPosition;
    playerYaw_ = saved.playerYaw;
    playerPitch_ = saved.playerPitch;
    wood_ = saved.wood;
    stone_ = saved.stone;
    crystals_ = saved.crystals;
    coins_ = saved.coins;
    chestItems_.reset();
    lootStacks_ = saved.lootStacks;
    relics_ = saved.relics;
    relicTowerClockIndices_.clear();
    classAbilityState_ = saved.classAbilityState;
    static_cast<void>(powerups_.loadState(saved.powerups));
    static_cast<void>(coinPickups_.loadMetaSilverState(saved.metaSilverPickups));
    runUpgradeStacks_ = saved.runUpgradeStacks;
    coreCardPowers_ = saved.coreCardPowers;
    coreHealthBaseLevel_ = saved.coreHealthBaseLevel;
    coreBuildingBaseLevel_ = saved.coreBuildingBaseLevel;
    coreStorageBaseLevel_ = saved.coreStorageBaseLevel;
    runUpgradeStacks_[runUpgradeIndex(RunUpgradeEffect::RiskyInvestment)] = 0;
    runUpgradePower_ = saved.runUpgradePower;
    buildingUpgradePowers_ = saved.buildingUpgradePowers;
    // Retired global bounce/pierce tomes become weapon-specific bonuses.
    // Transfer their fractional power once, then clear the old ownership so
    // future saves cannot re-apply it or occupy a tome slot.
    for (const auto [effect, stat] : {std::pair{RunUpgradeEffect::Bounces, AttackStat::Bounces},
            std::pair{RunUpgradeEffect::Pierce, AttackStat::Pierce}}) {
        const auto oldIndex = runUpgradeIndex(effect);
        if (runUpgradePower_[oldIndex] > 0.0) {
            for (std::size_t index = 0; index < buildingUpgradePowers_.size(); ++index) {
                if (supportsAttackStat(static_cast<BuildingType>(index), stat))
                    buildingUpgradePowers_[index][static_cast<std::size_t>(stat)] =
                        std::min(static_cast<double>(MaximumSavedStack),
                            buildingUpgradePowers_[index][static_cast<std::size_t>(stat)] + runUpgradePower_[oldIndex]);
            }
        }
        runUpgradeStacks_[oldIndex] = 0;
        runUpgradePower_[oldIndex] = 0.0;
    }
    runUpgradePower_[runUpgradeIndex(RunUpgradeEffect::RiskyInvestment)] = 0;
    if (saved.playerWeaponArsenalVersion == 0) {
        for (const auto& definition : RunUpgradeDefinitions) {
            const auto weapon = runUpgradeWeapon(definition.effect);
            if (weapon && savedCoreLevel >= playerWeaponRequiredCoreLevel(*weapon)) {
                const auto index = runUpgradeIndex(definition.effect);
                runUpgradeStacks_[index] = 1;
                runUpgradePower_[index] = 1.0;
            }
        }
    }
    if (saved.playerWeaponArsenalVersion < 2) {
        const auto* character=playerClassDefinition(saved.playerClass);
        for (const auto& definition:RunUpgradeDefinitions) {
            const auto weapon=runUpgradeWeapon(definition.effect);
            if (!weapon || !runWeaponLevelCard(definition.effect)) continue;
            const auto index=runUpgradeIndex(definition.effect);
            const bool owned=isPlayerTool(*weapon) || runUpgradeStacks_[index]>0 ||
                (character && character->startingWeapon==*weapon);
            if (owned) {
                const int level=*weapon==PlayerWeapon::Rifle ? std::clamp(saved.rifleLevel,1,MaximumPlayerWeaponLevel) : 1;
                runUpgradeStacks_[index]=level;runUpgradePower_[index]=level;
            }
        }
    }
    // Checkpoints from before rarities contain only selection counts.
    for (std::size_t index = 0; index < runUpgradePower_.size(); ++index) {
        if (runUpgradePower_[index] == 0.0 && runUpgradeStacks_[index] > 0) {
            runUpgradePower_[index] = static_cast<double>(runUpgradeStacks_[index]);
        }
    }
    // Preserve constructed types from before their blueprint gating was introduced.
    if (saved.buildingArsenalVersion < 2) {
        for (const auto& building : saved.buildings) {
            const auto effect = buildingRunUpgrade(building.type);
            if (!effect || ownsRunBuilding(building.type) ||
                (saved.buildingArsenalVersion == 1 && runCombatBuilding(building.type))) continue;
            runUpgradeStacks_[runUpgradeIndex(*effect)] = 1;
            runUpgradePower_[runUpgradeIndex(*effect)] = 1.0;
        }
    }
    // Before v3, combat buildings had individual resource-funded levels.
    // Before v5, producers did too. Preserve every acquired type even when
    // migration exceeds the new slot cap, and keep the strongest built copy.
    const auto* savedClass = playerClassDefinition(playerClass_);
    const auto startingDefense = savedClass ? savedClass->startingBuilding : BuildingType::GunTurret;
    for (const auto type : RunBuildingTypes) {
        const auto index = runUpgradeIndex(*buildingRunUpgrade(type));
        int level = runUpgradeStacks_[index];
        if (type == startingDefense || type == BuildingType::Wall ||
            (std::ranges::find(RunEconomyBuildingTypes, type) != RunEconomyBuildingTypes.end() &&
             std::ranges::any_of(saved.buildings,
                 [type](const BuildingInstance& placed) { return placed.type == type; })))
            level = std::max(1, level);
        if (type == BuildingType::Wall && saved.buildingArsenalVersion < 4 &&
            runBuildingArsenalCount() < static_cast<std::size_t>(runBuildingSlotLimit(activeMetaBonuses_))) {
            for (const auto& building : saved.buildings)
                if (building.type == type) level = std::max(level,static_cast<int>(building.level));
        }
        if (saved.buildingArsenalVersion < 3 ||
            (saved.buildingArsenalVersion < 5 && !runCombatBuilding(type))) {
            for (const auto& building : saved.buildings)
                if (building.type == type) level = std::max(level, static_cast<int>(building.level));
        }
        runUpgradeStacks_[index] = level;
        runUpgradePower_[index] = type == BuildingType::Wall
            ? std::max(static_cast<double>(level), runUpgradePower_[index])
            : static_cast<double>(level);
    }
    insight_ = std::move(insightValidation);
    objectives_ = std::move(objectiveValidation);
    std::fill(insightRewardedEnemyIds_.begin(),
              insightRewardedEnemyIds_.end(), EntityId{});

    shrines_ = saved.shrines;
    playerDamageMultiplier_ = std::max(0.05, saved.playerDamageMultiplier);
    runPlayerDamageMultiplier_ = std::max(
        0.05, saved.runPlayerDamageMultiplier);
    // Older saves include the former Rapid Tome player-speed bonus. The tome
    // now affects defenses only; restore the class baseline on load.
    playerAttackSpeedMultiplier_ = savedClass
        ? savedClass->attackSpeedMultiplier : 1.0;
    toolAttackSpeedMultiplier_ = playerAttackSpeedMultiplier_;
    playerMoveSpeedMultiplier_ = std::max(
        0.05, saved.playerMoveSpeedMultiplier);
    runPlayerMoveSpeedMultiplier_ = std::max(
        0.05, saved.runPlayerMoveSpeedMultiplier);
    playerArmorMultiplier_ = std::max(0.05, saved.playerArmorMultiplier);
    playerMaxHealthMultiplier_ = std::max(
        0.05, saved.playerMaxHealthMultiplier);
    buildingMaxHealthMultiplier_ = std::max(
        0.05, saved.buildingMaxHealthMultiplier);
    runBuildingMaxHealthMultiplier_ = std::max(
        0.05, saved.runBuildingMaxHealthMultiplier);
    productionSpeedMultiplier_ = std::max(
        0.05, saved.productionSpeedMultiplier);
    runProductionSpeedMultiplier_ = std::max(
        0.05, saved.runProductionSpeedMultiplier);
    defenseDamageMultiplier_ = std::max(
        0.05, saved.defenseDamageMultiplier);
    defenseFireRateMultiplier_ = std::max(
        0.05, saved.defenseFireRateMultiplier);
    woodYieldMultiplier_ = std::max(0.05, saved.woodYieldMultiplier);
    chestOpeningCostMultiplier_ = std::max(
        0.05, saved.chestOpeningCostMultiplier);
    const int extraKeys = std::max(0,
        lootStacks_[lootUpgradeIndex(LootUpgradeEffect::Key)] - 1);
    chestOpeningCostMultiplier_ = std::min(
        chestOpeningCostMultiplier_,
        std::max(0.5, 1.0 - 0.15 * extraKeys));
    playerBonusMaxHealth_ = std::max(0.0, saved.playerBonusMaxHealth);
    playerTemporaryHealth_ = std::max(0.0, saved.playerTemporaryHealth);
    playerRecoverableArmor_ = std::max(0.0, saved.playerRecoverableArmor);
    playerMaxRecoverableArmor_ = std::max(0.0, saved.playerMaxRecoverableArmor);
    playerRecoverableArmor_ = std::min(playerRecoverableArmor_, playerArmorCapacity());
    appleAvailable_ = saved.appleAvailable;
    breadWellFed_ = saved.breadWellFed;
    battlePotionAvailable_ = saved.battlePotionAvailable;
    freeChestOpeningAvailable_ = saved.freeChestOpeningAvailable;
    battlePotionBerserkRemaining_ = saved.battlePotionBerserkRemaining;
    battlePotionBerserkDuration_ = saved.battlePotionBerserkDuration;
    battlePotionLifestealRemaining_ = saved.battlePotionLifestealRemaining;
    freeChestRerollsRemaining_ = std::max(
        0, saved.freeChestRerollsRemaining);
    runNightlyBombBonus_ = std::max(0, saved.runNightlyBombBonus);
    runUpgradeRerollTokens_ = std::max(0, saved.runUpgradeRerollTokens);
    runUpgradeSkipTokens_=saved.runUpgradeSkipTokens;
    runUpgradeBanishTokens_=saved.runUpgradeBanishTokens;
    banishedRunUpgrades_=saved.banishedRunUpgrades;
    runUpgradeLockUnlocked_ = false;
    lockedRunUpgrade_.reset();
    bonusSelectionsNextReward_ = std::max(
        0, saved.bonusSelectionsNextReward);
    bloodHarvestKillProgress_ = std::max(
        0, saved.bloodHarvestKillProgress);
    runStatistics_ = saved.runStatistics;

    refreshRunUpgradeRuntimeEffects();
    repairStationCycleRemaining_ = saved.repairStationCycleRemaining;
    repairStationSources_.clear();
    repairStationLinks_.clear();
    if (!buildings_.restoreBuildings(
            saved.buildings, saved.buildingBlueprintLevels,
            static_cast<int>(restoredBuildRadius)) ||
        !foundations_.restoreStructures(
            saved.platformFrames, saved.modularWalls, saved.ramps) ||
        !resources_.restoreNodes(
            saved.resourceNodes, saved.woodYieldMultiplier * classLevelBonuses().woodYield,
            activeMetaBonuses_.resourceYield * classLevelBonuses().gatheringYield *
                (saved.playerWeaponArsenalVersion<2 ? 1.0+.15*runUpgradePower(RunUpgradeEffect::ResourceCapacity) : 1.0),
            saved.resourceNodeCapacityVersion > 0
                ? 1.0 + .20 * playerPassivePower(RunUpgradeEffect::ResourceCapacity) : 1.0) ||
        !lootChests_.restoreChests(saved.lootChests)) {
        return failLoad();
    }
    lootChests_.setOpenedPaidChestCount(saved.openedPaidChestCount);

    buildings_.setCoreCardBonuses(coreHealthBaseLevel_, coreCardPowers_[0],
        coreBuildingBaseLevel_, coreCardPowers_[2]);

    refreshRunUpgradeRuntimeEffects();
    if (saved.challengeColumns.size() != challengeColumns_.size() ||
        (saved.worldLandmarks.size() != worldLandmarks_.size() && saved.worldLandmarks.size()!=2U)) {
        return failLoad();
    }
    for (std::size_t index = 0;
         index < challengeColumns_.size(); ++index) {
        const ChallengeColumnInstance& source =
            saved.challengeColumns[index];
        ChallengeColumnInstance& target = challengeColumns_[index];
        const int challengeState = static_cast<int>(source.state);
        if (source.id.index != target.id.index ||
            !samePosition(source.position, target.position) ||
            !std::isfinite(source.yaw) ||
            challengeState < 0 ||
            challengeState >
                static_cast<int>(ChallengeColumnState::Completed) ||
            !finiteNonNegative(source.completionProgress, 1.0) ||
            !finiteNonNegative(source.fenceProgress, 1.0) ||
            source.enemyBudget < 0 ||
            source.enemyBudget > MaximumSavedCounter) {
            return failLoad();
        }
        if (source.state == ChallengeColumnState::Active) {
            target.state = ChallengeColumnState::Dormant;
            target.completionProgress = 0.0;
            target.fenceProgress = 0.0;
            target.enemyBudget = 0;
        } else {
            target.state = source.state;
            target.completionProgress = source.completionProgress;
            target.fenceProgress = source.fenceProgress;
            target.enemyBudget = source.enemyBudget;
        }
    }
    if (saved.worldLandmarks.size()==2U)
        std::erase_if(worldLandmarks_,[](const auto& value) {return landmarkShrineKind(value.type)!=ShrineKind::None;});
    std::array<int,2> usedShrines{};
    std::set<std::uint64_t> restoredLandmarks;
    for (std::size_t index = 0; index < saved.worldLandmarks.size(); ++index) {
        const auto& source=saved.worldLandmarks[index];
        if (!restoredLandmarks.insert((static_cast<std::uint64_t>(source.id.index)<<32)|source.id.generation).second) return failLoad();
        auto target=std::ranges::find(worldLandmarks_,source.id,&WorldLandmarkInstance::id);
        if (target==worldLandmarks_.end() || source.type!=target->type ||
            !samePosition(source.position,target->position) ||
            !finiteNonNegative(source.productionProgress,1000.0) || source.level<1 || source.level>5 ||
            ((!source.activated || landmarkShrineKind(source.type)!=ShrineKind::None) && source.level!=1)) return failLoad();
        target->activated=source.activated;
        target->productionProgress=source.productionProgress;
        target->level=source.level;
        if (const auto kind=landmarkShrineKind(source.type);kind!=ShrineKind::None && source.activated)
            ++usedShrines[kind==ShrineKind::Player ? 0 : 1];
    }
    for (const auto kind:{ShrineKind::Player,ShrineKind::Buildings}) {
        int earned=shrines_.offer==kind ? 1 : 0;
        for (const int count:shrineBonusCounts(shrines_,kind)) earned+=count;
        if (earned!=usedShrines[kind==ShrineKind::Player ? 0 : 1]) return failLoad();
    }
    if (shrines_.source) {
        const auto source=std::ranges::find(worldLandmarks_,*shrines_.source,&WorldLandmarkInstance::id);
        if (source==worldLandmarks_.end() || !source->activated ||
            landmarkShrineKind(source->type)!=shrines_.offer) return failLoad();
    }

    // Older saves have no scenery reservations. Preserve their structures,
    // relocated resources and player location instead of introducing a wall
    // through an existing base when Continue regenerates the terrain.
    std::erase_if(worldScenery_, [this](const WorldSceneryInstance& instance) {
        const CollisionBox box=worldSceneryCollisionBox(instance);
        const auto touches=[&](double x,double z,double radius) {
            const double dx=std::max({box.minX-x,0.0,x-box.maxX});
            const double dz=std::max({box.minZ-z,0.0,z-box.maxZ});
            return dx*dx+dz*dz<(radius+.5)*(radius+.5);
        };
        if (touches(playerPosition_.x,playerPosition_.z,1.0)) return true;
        for (const auto& building : buildings_.buildings())
            if (collisionBoxesOverlap(box,buildingCollisionBox(
                building.type,building.gridPosition,building.baseHeight))) return true;
        for (const auto& frame : foundations_.platformFrames()) {
            const double cell=worldConfig_.cellSize;
            if (box.minX<(frame.anchor.x+PlatformFrameWidthCells)*cell &&
                box.maxX>frame.anchor.x*cell &&
                box.minZ<(frame.anchor.z+PlatformFrameWidthCells)*cell &&
                box.maxZ>frame.anchor.z*cell) return true;
        }
        for (const auto& wall : foundations_.walls())
            if (touches((wall.anchor.x+.5)*worldConfig_.cellSize,
                        (wall.anchor.z+.5)*worldConfig_.cellSize,
                        worldConfig_.cellSize*.75)) return true;
        for (const auto& ramp : foundations_.ramps()) {
            const bool alongZ=ramp.rotation==Rotation::Deg0 ||
                ramp.rotation==Rotation::Deg180;
            const double width=(alongZ ? ModularRampWidthCells : ModularRampRunCells)*
                worldConfig_.cellSize;
            const double depth=(alongZ ? ModularRampRunCells : ModularRampWidthCells)*
                worldConfig_.cellSize;
            const double x=ramp.anchor.x*worldConfig_.cellSize;
            const double z=ramp.anchor.z*worldConfig_.cellSize;
            if (box.minX<x+width && box.maxX>x &&
                box.minZ<z+depth && box.maxZ>z) return true;
        }
        for (const auto& node : resources_.nodes())
            if (touches(node.position.x,node.position.z,node.radius*node.visualScale)) return true;
        for (const auto& chest : lootChests_.chests())
            if (!chest.looseLoot && touches(chest.position.x,chest.position.z,.82)) return true;
        return false;
    });
    syncWorldSceneryColliders();
    syncWorldLandmarkColliders();
    syncModularStructures();
    syncWorldStructures();
    if (!crystalMines_.restoreRuntimes(saved.producerRuntimes)) {
        return failLoad();
    }
    crystalMines_.setWoodYieldMultiplier(woodYieldMultiplier_);
    clampResourcesToCapacity();

    bombs_.reset();
    const auto* bombClass = playerClassDefinition(playerClass_);
    bombs_.setModifiers(classLevelBonuses().rangedDamage *
        (isSelectablePlayerClass(playerClass_) && bombClass ? bombClass->damageMultiplier : 1.0),
        classLevelBonuses().rangedSpeed);
    bombs_.restoreCooldown(saved.bombCooldownRemaining);
    playerWeapons_.restoreState(saved.selectedWeapon, playerWeaponLevel(PlayerWeapon::Rifle));
    playerHealth_ = std::clamp(
        saved.playerHealth, 1.0,
        playerPermanentMaxHealth() + playerTemporaryHealth_);
    playerRespawning_ = false;
    playerRespawnTimeRemaining_ = 0.0;
    playerRespawnProtectionRemaining_ = 0.0;
    enemies_.reset();
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0U;
    waveSpawnGroupSize_ = 1;
    waveSpawnInterval_ = 1.0;
    waveSpawnTimeRemaining_ = 0.0;
    waveSpawnGroupsDue_ = 0U;
    waveSpawnCycle_ = 0U;
    resetNightEncounter();
    waveBossSpawn_.reset();
    waveSurgeWarningMask_ = 0U;
    waveHealthScale_ = 1.0;
    waveDamageScale_ = 1.0;
    forceWaveCompletion_ = false;
    upcomingAttackDirection_.reset();
    upcomingAttackDirections_.fill(false);
    currentWaveHasBoss_ = false;
    pendingEliteExplosions_.clear();
    activeChallengeColumn_.reset();
    aimedChallengeColumn_.reset();
    runUpgradeChoicePending_ = saved.runUpgradeChoicePending;
    runShop_ = saved.runShop;
    runShop_.pending = false;
    for (auto& offer : runShop_.offers)
        if (offer.present && retiredRunUpgrade(offer.effect)) {
            offer = {};
        }
    runUpgradeChoiceCount_ = saved.runUpgradeChoiceCount;
    runUpgradePaidChoicePrice_ = saved.runUpgradeChoiceCount > DefaultRunUpgradeChoices &&
        shrines_.offer == ShrineKind::None ? saved.runUpgradePaidChoicePrice : 0;
    runUpgradeChoices_ = saved.runUpgradeChoices;
    runUpgradeRarities_ = saved.runUpgradeRarities;
    runUpgradeRollSeeds_ = saved.runUpgradeRollSeeds;
    runUpgradeSelectionsRemaining_ =
        saved.runUpgradeSelectionsRemaining;
    runUpgradeOfferGeneration_ = saved.runUpgradeOfferGeneration;
    if (shrines_.offer != ShrineKind::None &&
        (std::ranges::any_of(runUpgradeChoices_.begin(), runUpgradeChoices_.begin() + 3,
            [](RunUpgradeEffect effect) { return !availableShrineBlessing(effect); }) ||
         (shrines_.offer == ShrineKind::Buildings &&
          runUpgradeAvailable(RunUpgradeEffect::UpgradeCore) &&
          std::ranges::find(runUpgradeChoices_.begin(),runUpgradeChoices_.begin()+3,
              RunUpgradeEffect::UpgradeCore)==runUpgradeChoices_.begin()+3)))
        generateShrineChoices();
    if (shrines_.offer==ShrineKind::None && runUpgradeChoicePending_ &&
        (runUpgradeChoiceCount_ == 0 ||
         std::ranges::any_of(
            runUpgradeChoices_.begin(),
            runUpgradeChoices_.begin() +
                static_cast<std::ptrdiff_t>(runUpgradeChoiceCount_),
            [this, &saved](RunUpgradeEffect effect) {
                return retiredRunUpgrade(effect) ||
                    ((saved.buildingArsenalVersion < 2 || runUpgradeBuilding(effect) || runUpgradeWeapon(effect) || runUpgradeCannonStyle(effect) || runTomeDefinition(effect)) &&
                     !runUpgradeAvailable(effect));
            }))) {
        generateRunUpgradeChoices();
    }
    refreshWorldLandmarkEconomy();
    stageCleared_ = saved.stageCleared;
    finalNight_ = false;
    overtimeSeconds_ = saved.overtimeSeconds;
    enemies_.setOvertimeDifficulty(overtimeDifficulty(overtimeSeconds_, gameplay_.overtime));
    waveStartCheckpoint_.reset();
    phaseTimeRemaining_ = saved.phaseTimeRemaining;
    phaseDuration_ = saved.phaseDuration;
    state_ = saved.resumeState;
    stateBeforePause_ = saved.resumeState;
    selectedBuilding_.reset();
    buildingPreview_.reset();
    events_.clear();
    if(saved.finalNight && !enterFinalNight()) return failLoad();
    invalidateSnapshotCache();
    return true;
}

} // namespace ian
