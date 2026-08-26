#include "game/Simulation.hpp"

#include "buildings/BuildingOrientation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace ian {

namespace {

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

bool validProgressionCard(
    ProgressionCardId card, std::size_t skillCount) {
    if (isSkillProgressionCard(card)) {
        return progressionCardSkillIndex(card) < skillCount;
    }
    return validRunUpgrade(progressionCardRunUpgrade(card));
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
    if ((activeState == RunState::Sunset ||
         activeState == RunState::Wave) &&
        waveStartCheckpoint_) {
        return *waveStartCheckpoint_;
    }
    SuspendedRunState saved{
        .playerClass = playerClass_,
        .terrainSeed = terrain_.seed(),
        .resumeState = activeState,
        .tick = tick_,
        .elapsedSeconds = elapsedSeconds_,
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
        .bombs = bombs_.remainingBombs(),
        .selectedWeapon = playerWeapons_.selectedWeapon(),
        .rifleLevel = playerWeapons_.rifleLevel(),
        .skillTree = skillTree_.saveState(),
        .lootStacks = lootStacks_,
        .runUpgradeStacks = runUpgradeStacks_,
        .buildings = buildings_.buildings(),
        .resourceNodes = resources_.nodes(),
        .lootChests = lootChests_.chests(),
        .challengeColumns = challengeColumns_,
        .worldLandmarks = worldLandmarks_,
        .buildingBlueprintLevels = buildings_.blueprintLevels(),
        .coreBuildRadius = buildings_.coreBuildRadius(),
        .playerDamageMultiplier = playerDamageMultiplier_,
        .runPlayerDamageMultiplier = runPlayerDamageMultiplier_,
        .playerAttackSpeedMultiplier = playerAttackSpeedMultiplier_,
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
        .playerBonusMaxHealth = playerBonusMaxHealth_,
        .playerTemporaryHealth = playerTemporaryHealth_,
        .playerRecoverableArmor = playerRecoverableArmor_,
        .playerMaxRecoverableArmor = playerMaxRecoverableArmor_,
        .appleAvailable = appleAvailable_,
        .breadWellFed = breadWellFed_,
        .battlePotionAvailable = battlePotionAvailable_,
        .freeChestOpeningAvailable = freeChestOpeningAvailable_,
        .freeChestRerollsRemaining = freeChestRerollsRemaining_,
        .runNightlyBombBonus = runNightlyBombBonus_,
        .runUpgradeRerollTokens = runUpgradeRerollTokens_,
        .runUpgradeLockUnlocked = runUpgradeLockUnlocked_,
        .lockedRunUpgrade = lockedRunUpgrade_,
        .bonusSelectionsNextReward = bonusSelectionsNextReward_,
        .riskyInvestmentPending = riskyInvestmentPending_,
        .bloodHarvestKillProgress = bloodHarvestKillProgress_,
        .insight = insight_.saveState(),
        .objectives = objectives_.saveState(),
        .runUpgradeChoicePending = runUpgradeChoicePending_,
        .runUpgradeChoiceCount = runUpgradeChoiceCount_,
        .runUpgradeChoices = runUpgradeChoices_,
        .runUpgradeSelectionsRemaining = runUpgradeSelectionsRemaining_,
        .runUpgradeOfferGeneration = runUpgradeOfferGeneration_,
        .stageCleared = stageCleared_,
        .finalNight = finalNight_,
        .bareHandsWoodGathered = bareHandsWoodGathered_,
        .bareHandsStoneGathered = bareHandsStoneGathered_,
        .introSkillObjectiveCompleted = introSkillObjectiveCompleted_,
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
    // wave returns to the stage-clear choice before being replayed.
    if (activeState == RunState::Sunset) {
        saved.resumeState = RunState::BuildPhase;
        saved.phaseTimeRemaining = 0.0;
        saved.phaseDuration = 0.0;
    } else if (activeState == RunState::Wave) {
        if (riskyInvestmentActive_ > 0 &&
            saved.riskyInvestmentPending <=
                MaximumSavedCounter - riskyInvestmentActive_) {
            saved.riskyInvestmentPending += riskyInvestmentActive_;
        }
        if (finalNight_) {
            saved.resumeState = RunState::StageClear;
            saved.wave = std::max(StageClearWave, wave_ - 1);
            saved.stageCleared = true;
            saved.finalNight = false;
            saved.phaseTimeRemaining = 0.0;
            saved.phaseDuration = 0.0;
        } else {
            saved.resumeState = RunState::BuildPhase;
            saved.wave = std::max(0, wave_ - 1);
            saved.phaseTimeRemaining =
                gameplay_.betweenWaveSeconds + std::max(
                    0.0, skillTree_.effectValue(
                        "day.duration_seconds"));
            saved.phaseDuration = saved.phaseTimeRemaining;
        }
    }
    return saved;
}

bool Simulation::loadSuspendedRunState(
    const SuspendedRunState& saved) {
    const auto invalidMultiplier = [](double value) {
        return !std::isfinite(value) || value <= 0.0 || value > 1000.0;
    };
    const std::array multipliers{
        saved.playerDamageMultiplier,
        saved.runPlayerDamageMultiplier,
        saved.playerAttackSpeedMultiplier,
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
        std::ranges::any_of(saved.runUpgradeStacks, invalidStack);
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
                    building.level > MaxBuildingLevel ||
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
    const int selectedWeapon = static_cast<int>(saved.selectedWeapon);
    const bool invalidLockedUpgrade = saved.lockedRunUpgrade &&
        !validRunUpgrade(*saved.lockedRunUpgrade);
    const bool validResumeState =
        saved.resumeState == RunState::Gathering ||
        saved.resumeState == RunState::BuildPhase ||
        saved.resumeState == RunState::WaveComplete ||
        saved.resumeState == RunState::StageClear;
    bool invalidChoices =
        saved.runUpgradeChoiceCount > MaximumRunUpgradeChoices ||
        saved.runUpgradeSelectionsRemaining < 0 ||
        saved.runUpgradeSelectionsRemaining > MaximumSavedStack;
    std::unordered_set<ProgressionCardId> offeredUpgrades;
    for (std::size_t index = 0;
         index < saved.runUpgradeChoiceCount; ++index) {
        const ProgressionCardId card = saved.runUpgradeChoices[index];
        invalidChoices = invalidChoices ||
            !validProgressionCard(card, skillTree_.nodes().size()) ||
            !offeredUpgrades.insert(card).second;
    }
    if (saved.runUpgradeChoicePending) {
        invalidChoices = invalidChoices ||
            saved.runUpgradeChoiceCount < MinimumRunUpgradeChoices ||
            saved.runUpgradeSelectionsRemaining <= 0;
    } else if (saved.runUpgradeSelectionsRemaining != 0) {
        invalidChoices = true;
    }
    const auto invalidCounter = [](int value) {
        return value < 0 || value > MaximumSavedCounter;
    };
    const std::array counters{
        saved.wood, saved.stone, saved.crystals, saved.coins,
        saved.bombs, saved.freeChestRerollsRemaining,
        saved.runNightlyBombBonus, saved.runUpgradeRerollTokens,
        saved.bonusSelectionsNextReward, saved.riskyInvestmentPending,
        saved.bloodHarvestKillProgress, saved.bareHandsWoodGathered,
        saved.bareHandsStoneGathered,
    };
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
        !finiteNonNegative(saved.phaseTimeRemaining) ||
        !finiteNonNegative(saved.phaseDuration) ||
        saved.phaseTimeRemaining > saved.phaseDuration + 1e-6 ||
        saved.finalNight ||
        (saved.resumeState == RunState::StageClear
             ? (!saved.stageCleared || saved.wave < StageClearWave)
             : saved.stageCleared);
    if (state_ != RunState::MainMenu ||
        saved.playerClass == PlayerClass::None ||
        saved.playerClass > PlayerClass::Chronomancer ||
        saved.terrainSeed == 0U || saved.wave < 0 ||
        saved.wave > MaximumSavedCounter || saved.bestWave < 0 ||
        saved.bestWave > MaximumSavedCounter ||
        std::ranges::any_of(counters, invalidCounter) ||
        saved.rifleLevel < 1 || saved.rifleLevel > MaximumSavedStack ||
        selectedWeapon < 0 ||
        selectedWeapon >= static_cast<int>(PlayerWeaponCount) ||
        invalidStacks || invalidBuildings || invalidLockedUpgrade ||
        invalidChoices || invalidStatistics || invalidWorldCollections ||
        invalidPhase || saved.coreBuildRadius < map_.coreBuildRadius ||
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
        !finiteNonNegative(saved.playerBonusMaxHealth) ||
        !finiteNonNegative(saved.playerTemporaryHealth) ||
        !finiteNonNegative(saved.playerRecoverableArmor) ||
        !finiteNonNegative(saved.playerMaxRecoverableArmor) ||
        saved.playerRecoverableArmor > saved.playerMaxRecoverableArmor ||
        saved.skillTree.unlockedNodeIds.size() > 4096U ||
        saved.insight.consumedEventIds.size() > 100'000U ||
        saved.objectives.statuses.size() > 4096U ||
        saved.objectives.recentGathering.size() > 100'000U) {
        return false;
    }

    SkillTree skillTreeValidation = skillTree_;
    InsightSystem insightValidation = insight_;
    ObjectiveSystem objectiveValidation = objectives_;
    if (!skillTreeValidation.loadState(saved.skillTree) ||
        !insightValidation.loadState(saved.insight) ||
        !objectiveValidation.loadState(saved.objectives)) {
        return false;
    }
    startRunFromSeed(saved.playerClass, saved.terrainSeed);
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
    lootStacks_ = saved.lootStacks;
    runUpgradeStacks_ = saved.runUpgradeStacks;
    skillTree_ = std::move(skillTreeValidation);
    insight_ = std::move(insightValidation);
    objectives_ = std::move(objectiveValidation);
    std::fill(insightRewardedEnemyIds_.begin(),
              insightRewardedEnemyIds_.end(), EntityId{});

    playerDamageMultiplier_ = std::max(0.05, saved.playerDamageMultiplier);
    runPlayerDamageMultiplier_ = std::max(
        0.05, saved.runPlayerDamageMultiplier);
    playerAttackSpeedMultiplier_ = std::max(
        0.05, saved.playerAttackSpeedMultiplier);
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
    playerBonusMaxHealth_ = std::max(0.0, saved.playerBonusMaxHealth);
    playerTemporaryHealth_ = std::max(0.0, saved.playerTemporaryHealth);
    playerRecoverableArmor_ = std::max(0.0, saved.playerRecoverableArmor);
    playerMaxRecoverableArmor_ = std::max(
        playerRecoverableArmor_, saved.playerMaxRecoverableArmor);
    appleAvailable_ = saved.appleAvailable;
    breadWellFed_ = saved.breadWellFed;
    battlePotionAvailable_ = saved.battlePotionAvailable;
    freeChestOpeningAvailable_ = saved.freeChestOpeningAvailable;
    freeChestRerollsRemaining_ = std::max(
        0, saved.freeChestRerollsRemaining);
    runNightlyBombBonus_ = std::max(0, saved.runNightlyBombBonus);
    runUpgradeRerollTokens_ = std::max(0, saved.runUpgradeRerollTokens);
    runUpgradeLockUnlocked_ = true;
    lockedRunUpgrade_ = saved.lockedRunUpgrade;
    bonusSelectionsNextReward_ = std::max(
        0, saved.bonusSelectionsNextReward);
    riskyInvestmentPending_ = std::max(0, saved.riskyInvestmentPending);
    riskyInvestmentActive_ = 0;
    bloodHarvestKillProgress_ = std::max(
        0, saved.bloodHarvestKillProgress);
    bareHandsWoodGathered_ = saved.bareHandsWoodGathered;
    bareHandsStoneGathered_ = saved.bareHandsStoneGathered;
    introSkillObjectiveCompleted_ = saved.introSkillObjectiveCompleted;
    runStatistics_ = saved.runStatistics;
    const int migratedLevelChoices = skillTree_.takePoints();

    refreshSkillRuntimeEffects();
    if (!buildings_.restoreBuildings(
            saved.buildings, saved.buildingBlueprintLevels,
            saved.coreBuildRadius) ||
        !foundations_.restoreStructures(
            saved.platformFrames, saved.modularWalls, saved.ramps) ||
        !resources_.restoreNodes(
            saved.resourceNodes, saved.woodYieldMultiplier) ||
        !lootChests_.restoreChests(saved.lootChests)) {
        return failLoad();
    }

    if (saved.challengeColumns.size() != challengeColumns_.size() ||
        saved.worldLandmarks.size() != worldLandmarks_.size()) {
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
    for (std::size_t index = 0;
         index < worldLandmarks_.size(); ++index) {
        const WorldLandmarkInstance& source =
            saved.worldLandmarks[index];
        WorldLandmarkInstance& target = worldLandmarks_[index];
        if (source.id.index != target.id.index ||
            source.type != target.type ||
            !samePosition(source.position, target.position) ||
            !finiteNonNegative(source.productionProgress, 1000.0)) {
            return failLoad();
        }
        target.activated = source.activated;
        target.productionProgress = source.productionProgress;
    }

    syncWorldLandmarkColliders();
    syncModularStructures();
    syncWorldStructures();
    crystalMines_.setWoodYieldMultiplier(woodYieldMultiplier_);
    clampResourcesToCapacity();

    bombs_.reset();
    bombs_.addBombs(saved.bombs);
    playerWeapons_.restoreState(saved.selectedWeapon, saved.rifleLevel);
    playerHealth_ = std::clamp(
        saved.playerHealth, 1.0,
        playerPermanentMaxHealth() + playerTemporaryHealth_);
    playerRespawning_ = false;
    playerRespawnTimeRemaining_ = 0.0;
    enemies_.reset();
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0U;
    waveSpawnGroupSize_ = 1;
    waveSpawnInterval_ = 1.0;
    waveSpawnTimeRemaining_ = 0.0;
    waveSpawnGroupsDue_ = 0U;
    waveSpawnCycle_ = 0U;
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
    runUpgradeChoiceCount_ = saved.runUpgradeChoiceCount;
    runUpgradeChoices_ = saved.runUpgradeChoices;
    runUpgradeSelectionsRemaining_ =
        saved.runUpgradeSelectionsRemaining;
    runUpgradeOfferGeneration_ = saved.runUpgradeOfferGeneration;
    if (migratedLevelChoices > 0) {
        runUpgradeSelectionsRemaining_ += migratedLevelChoices;
        if (!runUpgradeChoicePending_) {
            generateRunUpgradeChoices();
            runUpgradeChoicePending_ = true;
        }
    }
    stageCleared_ = saved.stageCleared;
    finalNight_ = false;
    waveStartCheckpoint_.reset();
    phaseTimeRemaining_ = saved.phaseTimeRemaining;
    phaseDuration_ = saved.phaseDuration;
    state_ = saved.resumeState;
    stateBeforePause_ = saved.resumeState;
    selectedBuilding_.reset();
    buildingPreview_.reset();
    events_.clear();
    invalidateSnapshotCache();
    return true;
}

} // namespace ian
