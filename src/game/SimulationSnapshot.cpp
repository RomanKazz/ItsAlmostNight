#include "game/Simulation.hpp"

#include "combat/CombatDisplayUnits.hpp"

#include "game/ChallengeArena.hpp"

#include "core/SaturatingArithmetic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
const SimulationSnapshot& Simulation::snapshot() const {
    if (snapshotCache_) {
        return *snapshotCache_;
    }
    const auto core = buildings_.core();
    std::optional<FinalAttackStats> aimedFinalStats;
    std::optional<FinalAttackStats> previewFinalStats;
    if (selectedBuilding_ == BuildingType::WindBlower) {
        BuildingInstance preview;
        preview.type = BuildingType::WindBlower;
        preview.level = buildings_.blueprintLevels()[static_cast<std::size_t>(preview.type)];
        previewFinalStats = towers_.finalStats(preview);
    }
    std::optional<ResourceCost> aimedUpgradeCost;
    std::optional<BuildingStatComparison> aimedStats;
    if (aimedBuilding_) {
        const auto aimed =
            std::find_if(buildings_.buildings().begin(), buildings_.buildings().end(),
                         [this](const BuildingInstance& building) {
                             return building.id == *aimedBuilding_;
                         });
        if (aimed != buildings_.buildings().end() &&
            !BuildingSystem::usesGlobalBlueprint(aimed->type) &&
            aimed->level < MaxBuildingLevel &&
            (sandboxMode_ || unlimitedResources_ || !runCombatBuilding(aimed->type))) {
            aimedUpgradeCost = buildings_.upgradeCost(*aimed);
        }
        if (aimed != buildings_.buildings().end()) {
            aimedStats = compareBuildingStats(
                *aimed, crystalMines_, MaxBuildingLevel);
            if (aimed->type == BuildingType::WindBlower) {
                aimedFinalStats = towers_.finalStats(*aimed);
                const auto updateWindStats = [&](BuildingStats& view, std::uint8_t level) {
                    auto copy = *aimed; copy.level = level;
                    const auto stats = towers_.finalStats(copy);
                    view.attackRange = stats.range * stats.size;
                    view.pushStrength = stats.knockback;
                    view.cooldown = stats.attackInterval;
                };
                updateWindStats(aimedStats->current, aimed->level);
                if (aimedStats->previous) updateWindStats(*aimedStats->previous, static_cast<std::uint8_t>(aimed->level - 1));
                if (aimedStats->next) updateWindStats(*aimedStats->next, static_cast<std::uint8_t>(aimed->level + 1));
            }
        }
    }
    constexpr std::array DefenseTypes{
        BuildingType::GunTurret,
        BuildingType::Turret,
        BuildingType::Cannon,
        BuildingType::Catapult,
    };
    std::array<DefenseBlueprintStatus, DefenseBlueprintCount>
        defenseBlueprints{};
    for (std::size_t index = 0; index < DefenseTypes.size(); ++index) {
        const BuildingType type = DefenseTypes[index];
        const bool unlocked = buildingUnlocked(type);
        const int availableWood = unlimitedResources_
            ? std::numeric_limits<int>::max() : wood_;
        const int availableStone = unlimitedResources_
            ? std::numeric_limits<int>::max() : stone_;
        const int availableCrystals = unlimitedResources_
            ? std::numeric_limits<int>::max() : crystals_;
        UpgradeResult validation = buildings_.validateBlueprintUpgrade(
            type, availableWood, availableStone, availableCrystals);
        if (!unlocked) {
            validation.error = UpgradeError::Unsupported;
        }
        const BuildingInstance preview = buildings_.blueprintPreview(type);
        defenseBlueprints[index] = {
            .type = type,
            .level = preview.level,
            .existingBuildingCount =
                buildings_.blueprintBuildingCount(type),
            .unlocked = unlocked,
            .upgradeError = validation.error,
            .upgradeCost = buildings_.blueprintUpgradeCost(type),
            .stats = compareBuildingStats(
                preview, crystalMines_, MaxBuildingLevel),
        };
    }
    const NightForecast upcomingNight =
        waveDirector_.forecast(saturatingAdd(wave_, 1), terrain_.seed());
    std::optional<Vec3> nearestChestPosition;
    double nearestChestDistance =
        std::numeric_limits<double>::infinity();
    const int compassStacks = lootStacks_[
        lootUpgradeIndex(LootUpgradeEffect::Compass)];
    const double compassSearchRadius =
        36.0 + static_cast<double>(compassStacks) * 24.0;
    if (compassStacks > 0) {
        for (const LootChestInstance& chest : lootChests_.chests()) {
            if (chest.state != LootChestState::Closed ||
                chest.loot.collected) {
                continue;
            }
            const double distance = std::hypot(
                chest.position.x - playerPosition_.x,
                chest.position.z - playerPosition_.z);
            if (distance <= compassSearchRadius &&
                distance < nearestChestDistance) {
                nearestChestDistance = distance;
                nearestChestPosition = chest.position;
            }
        }
    }
    double heldDamage = gameplay_.pickaxeDamage;
    switch (playerWeapons_.selectedWeapon()) {
    case PlayerWeapon::LegacyBareHands: heldDamage = 0.0; break;
    case PlayerWeapon::Club: heldDamage *= club_.damageMultiplier * playerWeaponLevelDamage(PlayerWeapon::Club,playerWeaponLevel(PlayerWeapon::Club)); break;
    case PlayerWeapon::IceWand: heldDamage = iceWand_.directDamage(); break;
    case PlayerWeapon::FireWand: heldDamage = fireWand_.directDamage(); break;
    case PlayerWeapon::Hammer: heldDamage *= 0.75; break;
    case PlayerWeapon::Rifle: heldDamage = playerWeapons_.rifleDamage(); break;
    case PlayerWeapon::Bomb:
        heldDamage = 6.0;
        break;
    case PlayerWeapon::Axe:
    case PlayerWeapon::Pickaxe:
        heldDamage *= meleeEnemyDamageMultiplier(
            playerWeapons_.selectedWeapon()) * playerWeaponLevelDamage(playerWeapons_.selectedWeapon(),playerWeaponLevel(playerWeapons_.selectedWeapon()));
        break;
    }
    heldDamage *= playerDamageMultiplier_ * runPlayerDamageMultiplier_ *
        playerClassDamageMultiplier();
    std::array<int, 3> recommendedObjectives{-1, -1, -1};
    if (!sandboxMode_) {
        const auto recommended = objectives_.recommended(
            recommendedObjectives.size());
        for (std::size_t index = 0; index < recommended.size(); ++index) {
            recommendedObjectives[index] =
                static_cast<int>(recommended[index]);
        }
    }
    snapshotCache_ = SimulationSnapshot{
        .state = state_,
        .sandboxMode = sandboxMode_,
        .playerClass = playerClass_,
        .tick = tick_,
        .elapsedSeconds = elapsedSeconds_,
        .playerPosition = playerPosition_,
        .playerYaw = playerYaw_,
        .playerPitch = playerPitch_,
        .playerGrounded = playerGrounded_,
        .playerHorizontalVelocity =
            playerHorizontalVelocity_,
        .dashUnlocked = true,
        .dashing = dashRemaining_ > 0.0,
        .dashCooldownRemaining = dashCooldownRemaining_,
        .dashCooldownDuration = 0.90,
        .playerVerticalVelocity = verticalVelocity_,
        .playerHealth = playerHealth_,
        .playerMaxHealth =
            playerPermanentMaxHealth() + playerTemporaryHealth_,
        .playerRespawning = playerRespawning_,
        .playerRespawnTimeRemaining =
            playerRespawnTimeRemaining_,
        .playerRespawnDuration =
            gameplay_.playerRespawnSeconds,
        .playerRespawnProtectionRemaining = playerRespawnProtectionRemaining_,
        .deathLostWood = deathLostWood_,
        .deathLostStone = deathLostStone_,
        .deathLostCrystals = deathLostCrystals_,
        .wood = wood_,
        .stone = stone_,
        .crystals = crystals_,
        .woodCapacity = resourceCapacity(ResourceCapacityType::Wood),
        .stoneCapacity = resourceCapacity(ResourceCapacityType::Stone),
        .crystalCapacity = resourceCapacity(ResourceCapacityType::Crystal),
        .crystalStorageFull = !unlimitedResources_ &&
            crystals_ >= resourceCapacity(ResourceCapacityType::Crystal),
        .coins = coins_,
        .coinPickups = coinPickups_.pickups(),
        .aimedChest = aimedChest_,
        .aimedLoot = aimedLoot_,
        .lootChests =
            std::span<const LootChestInstance>{lootChests_.chests()},
        .aimedChallengeColumn = aimedChallengeColumn_,
        .challengeColumns =
            std::span<const ChallengeColumnInstance>{challengeColumns_},
        .aimedWorldLandmark = aimedWorldLandmark_,
        .worldLandmarks =
            std::span<const WorldLandmarkInstance>{worldLandmarks_},
        .activeChallengeCenter = activeChallengeColumn_
            ? std::optional<Vec3>{
                  challengeColumns_[*activeChallengeColumn_].position}
            : std::nullopt,
        .activeChallengeRadius = activeChallengeColumn_
            ? challenge_arena::FenceRadius
            : 0.0,
        .nearestChestPosition = nearestChestPosition,
        .nearestChestDistance = nearestChestPosition
            ? nearestChestDistance
            : 0.0,
        .lootStacks = lootStacks_,
        .playerDamageMultiplier =
            playerDamageMultiplier_ * runPlayerDamageMultiplier_ *
                playerClassDamageMultiplier(),
        .playerMoveSpeedMultiplier =
            playerMoveSpeedMultiplier_ * runPlayerMoveSpeedMultiplier_,
        .playerArmorMultiplier = playerArmorMultiplier_,
        .playerTemporaryHealth = playerTemporaryHealth_,
        .playerRecoverableArmor = playerRecoverableArmor_,
        .playerMaxRecoverableArmor = playerMaxRecoverableArmor_,
        .playerArmorRechargeDelayRemaining = std::max(
            0.0, 5.0 - secondsSincePlayerDamage_),
        .battlePotionAvailable = battlePotionAvailable_,
        .battlePotionBerserkRemaining =
            battlePotionBerserkRemaining_,
        .battlePotionBerserkDuration =
            battlePotionBerserkDuration_,
        .appleAvailable = appleAvailable_,
        .breadWellFed = breadWellFed_,
        .stageCleared = stageCleared_,
        .finalNight = finalNight_,
        .overtimeActive = !sandboxMode_ && (wave_ > gameplay_.overtime.afterWave || overtimeSeconds_ > 0.0),
        .overtimeSeconds = overtimeSeconds_,
        .overtime = overtimeDifficulty(overtimeSeconds_, gameplay_.overtime),
        .chestOpeningCostMultiplier =
            chestOpeningCostMultiplier_,
        .freeChestOpeningAvailable =
            freeChestOpeningAvailable_,
        .freeChestRerollsRemaining =
            freeChestRerollsRemaining_,
        .chestOpeningCostSurcharge =
            saturatingMultiplyNonNegative(
                economy_.chestOpeningCoinCostPerWave, wave_),
        .pickaxeCooldownRemaining = pickaxeCooldownRemaining_,
        .aimedResource = aimedResource_,
        .aimedResourceEfficiency = [this]() {
            if (!aimedResource_) return 1.0;
            const auto resource = std::ranges::find(
                resources_.nodes(), *aimedResource_,
                &ResourceNode::id);
            if (resource == resources_.nodes().end()) return 1.0;
            const PlayerWeapon weapon =
                playerWeapons_.selectedWeapon();
            return resourceToolEfficiency(weapon, resource->type);
        }(),
        .resourceNodes = std::span<const ResourceNode>{resources_.nodes()},
        .worldLimit = map_.worldLimit,
        .worldCellSize = worldConfig_.cellSize,
        .terrainSeed = terrain_.seed(),
        .terrainResolution = terrain_.resolution(),
        .terrainWorldSize =
            terrain_.config().terrainWorldSize,
        .terrainSamples = terrain_.samples(),
        .ponds = terrain_.ponds(),
        .mapObstacles = std::span<const MapObstacle>{map_.obstacles},
        .collisionBoxes =
            std::span<const CollisionBox>{collisionWorld_.colliders()},
        .flowDebugVectors =
            std::span<const FlowDebugVector>{flowDebugVectors_},
        .selectedBuilding = selectedBuilding_,
        .buildingCosts = {
            buildings_.configuredCost(BuildingType::Core),
            buildings_.configuredCost(BuildingType::Wall),
            buildings_.configuredCost(BuildingType::Turret),
            buildings_.configuredCost(BuildingType::CrystalMine),
            buildings_.configuredCost(BuildingType::Cannon),
            buildings_.configuredCost(BuildingType::SlowTrap),
            buildings_.configuredCost(BuildingType::Gate),
            buildings_.configuredCost(BuildingType::LumberMill),
            buildings_.configuredCost(BuildingType::Quarry),
            buildings_.configuredCost(BuildingType::SpikeTrap),
            buildings_.configuredCost(BuildingType::WoodStorage),
            buildings_.configuredCost(BuildingType::StoneStorage),
            buildings_.configuredCost(BuildingType::CrystalStorage),
            buildings_.configuredCost(BuildingType::GunTurret),
            buildings_.configuredCost(BuildingType::Catapult),
            buildings_.configuredCost(BuildingType::Flamethrower),
            buildings_.configuredCost(BuildingType::MissileLauncher),
            buildings_.configuredCost(BuildingType::TeslaTower),
            buildings_.configuredCost(BuildingType::LaserTower),
            buildings_.configuredCost(BuildingType::Mortar),
            buildings_.configuredCost(BuildingType::RepairStation),
            buildings_.configuredCost(BuildingType::DecoyBeacon),
            buildings_.configuredCost(BuildingType::WindBlower),
        },
        .unlockedBuildings = {
            buildingUnlocked(BuildingType::Core),
            buildingUnlocked(BuildingType::Wall),
            buildingUnlocked(BuildingType::Turret),
            buildingUnlocked(BuildingType::CrystalMine),
            buildingUnlocked(BuildingType::Cannon),
            buildingUnlocked(BuildingType::SlowTrap),
            buildingUnlocked(BuildingType::Gate),
            buildingUnlocked(BuildingType::LumberMill),
            buildingUnlocked(BuildingType::Quarry),
            buildingUnlocked(BuildingType::SpikeTrap),
            buildingUnlocked(BuildingType::WoodStorage),
            buildingUnlocked(BuildingType::StoneStorage),
            buildingUnlocked(BuildingType::CrystalStorage),
            buildingUnlocked(BuildingType::GunTurret),
            buildingUnlocked(BuildingType::Catapult),
            buildingUnlocked(BuildingType::Flamethrower),
            buildingUnlocked(BuildingType::MissileLauncher),
            buildingUnlocked(BuildingType::TeslaTower),
            buildingUnlocked(BuildingType::LaserTower),
            buildingUnlocked(BuildingType::Mortar),
            buildingUnlocked(BuildingType::RepairStation),
            buildingUnlocked(BuildingType::DecoyBeacon),
            buildingUnlocked(BuildingType::WindBlower),
        },
        .coreBuildRadius = sandboxMode_
            ? 0
            : buildings_.coreBuildRadius(),
        .modularBuildingCosts = modularBuildingCosts_,
        .buildingPreview = buildingPreview_,
        .buildingPreviewFinalAttackStats = previewFinalStats,
        .buildings = std::span<const BuildingInstance>{buildings_.buildings()},
        .platformFrames =
            foundations_.platformFrames(),
        .sharedSupports =
            foundations_.supportSystem().supports(),
        .modularWalls = foundations_.walls(),
        .ramps = foundations_.ramps(),
        .aimedModularBuilding =
            aimedModularBuilding_,
        .aimedModularBuildingCandidate =
            foundations_.raycast(
                playerPosition_,
                lookDirection(playerYaw_, playerPitch_),
                std::max(
                    6.0,
                    worldConfig_.buildPreviewDistance +
                        modularStoreyHeight(worldConfig_))),
        .aimedEnemy = aimedEnemy_,
        .aimedBuilding = aimedBuilding_,
        .aimedFinalAttackStats = aimedFinalStats,
        .aimedBuildingUpgradeCost = aimedUpgradeCost,
        .aimedBuildingStats = aimedStats,
        .defenseBlueprints = defenseBlueprints,
        .enemies = std::span<const EnemyInstance>{enemies_.enemies()},
        .enemyProjectiles = enemies_.projectiles(),
        .decoyBeacons = enemies_.decoyBeacons(),
        .repairStationPulseAge = RepairStationInterval - repairStationCycleRemaining_,
        .repairStationLinks = repairStationLinks_,
        .towers = std::span<const TowerRuntime>{towers_.towers()},
        .laserBeams = towers_.laserBeams(),
        .defenseProjectiles = towers_.projectiles(),
        .cannons = std::span<const CannonRuntime>{cannons_.cannons()},
        .traps = std::span<const TrapRuntime>{traps_.traps()},
        .cannonProjectiles =
            std::span<const CannonProjectile>{cannons_.projectiles()},
        .bombProjectiles = std::span<const BombProjectile>{bombs_.projectiles()},
        .iceWandProjectiles = std::span<const IceWandProjectile>{iceWand_.projectiles()},
        .iceWandChargeRemaining = iceWand_.chargeRemaining(),
        .iceWandChargeDuration = iceWand_.chargeDuration(),
        .iceWandCooldownRemaining = iceWand_.cooldownRemaining(),
        .fireWandProjectiles =
            std::span<const IceWandProjectile>{fireWand_.projectiles()},
        .fireWandChargeRemaining = fireWand_.chargeRemaining(),
        .fireWandChargeDuration = fireWand_.chargeDuration(),
        .fireWandCooldownRemaining = fireWand_.cooldownRemaining(),
        .activeEnemyCount = enemies_.activeCount(),
        .pendingEnemyCount = waveSpawnQueue_.size() - nextWaveSpawnIndex_ +
            (waveBossSpawn_ ? 1U : 0U),
        .upcomingEnemyCounts = upcomingNight.enemies,
        .upcomingWaveHasBoss = upcomingNight.enemies[static_cast<std::size_t>(EnemyType::Boss)] > 0,
        .upcomingNightPattern = upcomingNight.pattern,
        .currentNightPattern = wavePattern_,
        .currentNightAct = waveEncounterAct_,
        .upcomingAttackDirection = upcomingAttackDirection_,
        .upcomingAttackDirections = upcomingAttackDirections_,
        .attackPortalPositions = wavePortalsActive_
            ? wavePortalPositions_
            : std::array<std::optional<Vec3>, 4>{},
        .phaseTimeRemaining = phaseTimeRemaining_,
        .phaseDuration = phaseDuration_,
        .earlyWaveBonus = earlyWaveBonus(),
        .earlyWaveCoinBonus = earlyWaveCoinBonus(),
        .earlyWaveInsightBonus = earlyWaveInsightBonus(),
        .wave = wave_,
        .bestWave = bestWave_,
        .runStatistics = runStatistics_,
        .coreHealth = core ? core->health : 0.0,
        .coreMaxHealth = core ? core->maxHealth : 0.0,
        .coreId = core ? std::optional<EntityId>{core->id} : std::nullopt,
        .coreLevel = core ? core->level : static_cast<std::uint8_t>(0),
        .unlimitedResources = unlimitedResources_,
        .playerInvulnerable = playerInvulnerable_ || playerRespawnProtectionRemaining_ > 0.0,
        .automaticToolSwitch = true,
        .holdToGather = true,
        .unlockedWeapons = {
            playerWeaponUnlocked(PlayerWeapon::LegacyBareHands),
            playerWeaponUnlocked(PlayerWeapon::Axe),
            playerWeaponUnlocked(PlayerWeapon::Pickaxe),
            playerWeaponUnlocked(PlayerWeapon::Club),
            playerWeaponUnlocked(PlayerWeapon::IceWand),
            playerWeaponUnlocked(PlayerWeapon::FireWand),
            playerWeaponUnlocked(PlayerWeapon::Hammer),
            playerWeaponUnlocked(PlayerWeapon::Rifle),
            playerWeaponUnlocked(PlayerWeapon::Bomb),
        },
        .weaponLevels = {0,playerWeaponLevel(PlayerWeapon::Axe),playerWeaponLevel(PlayerWeapon::Pickaxe),
            playerWeaponLevel(PlayerWeapon::Club),playerWeaponLevel(PlayerWeapon::IceWand),
            playerWeaponLevel(PlayerWeapon::FireWand),0,playerWeaponLevel(PlayerWeapon::Rifle),playerWeaponLevel(PlayerWeapon::Bomb)},
        .selectedWeapon = playerWeapons_.selectedWeapon(),
        .selectedWeaponDamage = combatDisplayScalar(heldDamage),
        .rifleLevel = playerWeapons_.rifleLevel(),
        .rifleAmmunition = playerWeapons_.ammunition(),
        .rifleMagazineSize = playerWeapons_.magazineSize(),
        .rifleUpgradeCrystalCost = playerWeapons_.upgradeCrystalCost(),
        .rifleReloading = playerWeapons_.reloading(),
        .rifleReloadRemaining = playerWeapons_.reloadRemaining(),
        .rifleReloadDuration = playerWeapons_.reloadDuration(),
        .bombsRemaining = unlimitedResources_
            ? std::numeric_limits<int>::max()
            : bombs_.remainingBombs(),
        .bombPurchaseCoinCost = saturatingAdd(
            economy_.bombPurchaseCoinCost,
            saturatingMultiplyNonNegative(
                economy_.bombPurchaseCoinCostPerWave, wave_)),
        .bombPurchaseAmount = economy_.bombPurchaseAmount,
        .chestRerollCoinCost = economy_.chestRerollCoinCosts.at(
            [&]() -> std::size_t {
                if (!aimedLoot_) return 0U;
                const auto chest = std::ranges::find(
                    lootChests_.chests(), *aimedLoot_,
                    [](const LootChestInstance& value) {
                        return value.loot.id;
                    });
                return chest == lootChests_.chests().end()
                    ? 0U
                    : std::min<std::size_t>(
                          chest->rerollCount,
                          economy_.chestRerollCoinCosts.size() - 1U);
            }()),
        .chestRevealCoinCost = economy_.chestRevealCoinCost,
        .waveCompletionReward = saturatingAdd(
            economy_.waveRewardBase,
            saturatingMultiplyNonNegative(
                economy_.waveRewardPerWave, wave_)),
        .tutorialWoodTarget = buildings_.configuredCost(BuildingType::Core).wood,
        .tutorialObjective = sandboxMode_
            ? std::optional<TutorialObjective>{}
            : tutorialObjective(),
        .playerLevel = insight_.progress().totalLevelsEarned + 1,
        .currentInsight = insight_.progress().currentInsight,
        .requiredInsight = insight_.progress().requiredInsight,
        .totalInsightEarned = insight_.progress().totalInsightEarned,
        .objectives = objectives_.statuses(),
        .recommendedObjectives = recommendedObjectives,
        .buildingArsenalCount = runBuildingArsenalCount(),
        .tomeCount = runTomeCount(runUpgradeStacks_),
        .tomeSlotLimit = runTomeSlotLimit(activeMetaBonuses_),
        .weaponSlotLimit = runWeaponSlotLimit(activeMetaBonuses_),
        .runUpgradeChoicePending = runUpgradeChoicePending_,
        .runShop = runShop_,
        .runShopBuyable = [this] {
            std::array<bool, RunShopSlotCount> buyable{};
            for (std::size_t i = 0; i < buyable.size(); ++i)
                buyable[i] = runShop_.offers[i].present && runUpgradeAvailable(runShop_.offers[i].effect);
            return buyable;
        }(),
        .runUpgradeChoiceCount = runUpgradeChoiceCount_,
        .runUpgradeChoices = runUpgradeChoices_,
        .runUpgradeStacks = runUpgradeStacks_,
        .runUpgradePower = runUpgradePower_,
        .runUpgradeRarities = runUpgradeRarities_,
        .upgradeStats = {
            .defenseDamage = defenseDamageMultiplier_,
            .defenseFireRate = defenseFireRateMultiplier_,
            .buildingHealth = buildingMaxHealthMultiplier_ * runBuildingMaxHealthMultiplier_,
            .productionSpeed = productionSpeedMultiplier_ * runProductionSpeedMultiplier_,
            .resourceYield = activeMetaBonuses_.resourceYield * (playerClass_ == PlayerClass::Prospector ? 1.35 : 1.0),
            .resourceCapacity = 1.0 + .20 * runUpgradePower(RunUpgradeEffect::ResourceCapacity),
            .playerAttackSpeed = playerAttackSpeedMultiplier_ * temporaryAttackSpeedMultiplier() *
                (battlePotionBerserkRemaining_ > 0.0 ? 1.35 : 1.0),
            .toolAttackSpeed = toolAttackSpeedMultiplier_ * temporaryAttackSpeedMultiplier() * classLevelBonuses().meleeSpeed *
                (1.0 + 0.15 * runUpgradePower(RunUpgradeEffect::GatheringSpeed)) *
                (playerClass_ == PlayerClass::Berserker ? playerClassDamageMultiplier() : 1.0),
            .luck = activeMetaBonuses_.luck + 0.10 * runUpgradePower(RunUpgradeEffect::Luck),
            .healthRegeneration = 0.5 * runUpgradePower(RunUpgradeEffect::Regeneration),
            .sideShots = runUpgradeCount(RunUpgradeEffect::TwinBatteries),
            .gunPiercing = runUpgradeCount(RunUpgradeEffect::GunPiercing),
            .ricochets = runUpgradeCount(RunUpgradeEffect::Ricochet),
            .clusterExplosions = runUpgradeCount(RunUpgradeEffect::ClusterPayload),
            .cannonAttackStyle = runCannonAttackStyle(runUpgradeStacks_),
        },
        .runUpgradeSelectionsRemaining =
            runUpgradeSelectionsRemaining_,
        .runUpgradeRerollTokens = runUpgradeRerollTokens_,
        .runUpgradeLockUnlocked = runUpgradeLockUnlocked_,
        .lockedRunUpgrade = lockedRunUpgrade_,
    };
    return *snapshotCache_;
}

void Simulation::invalidateSnapshotCache() {
    snapshotCache_.reset();
}

} // namespace ian
