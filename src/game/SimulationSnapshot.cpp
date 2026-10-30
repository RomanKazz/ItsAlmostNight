#include "game/Simulation.hpp"

#include "combat/CombatDisplayUnits.hpp"
#include "progression/DefenseAttackModifiers.hpp"

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
    if (aimedBuilding_) {
        const auto building = std::ranges::find(buildings_.buildings(), *aimedBuilding_, &BuildingInstance::id);
        if (building != buildings_.buildings().end()) {
            if (building->type == BuildingType::Turret || building->type == BuildingType::GunTurret ||
                building->type == BuildingType::Flamethrower || building->type == BuildingType::MissileLauncher || building->type == BuildingType::TeslaTower || building->type == BuildingType::LaserTower || building->type == BuildingType::WindBlower)
                aimedFinalStats = towers_.finalStats(*building);
            else if (building->type == BuildingType::Cannon || building->type == BuildingType::Catapult || building->type == BuildingType::Mortar)
                aimedFinalStats = cannons_.finalStats(*building);
            else if (building->type == BuildingType::SpikeTrap || building->type == BuildingType::SlowTrap)
                aimedFinalStats = traps_.finalStats(*building);
        }
    }
    std::optional<FinalAttackStats> previewFinalStats;
    if (selectedBuilding_ == BuildingType::Flamethrower || selectedBuilding_ == BuildingType::MissileLauncher || selectedBuilding_ == BuildingType::TeslaTower || selectedBuilding_ == BuildingType::LaserTower || selectedBuilding_ == BuildingType::WindBlower) {
        BuildingInstance previewBuilding;
        previewBuilding.type = *selectedBuilding_;
        previewBuilding.level = buildings_.blueprintLevels()[static_cast<std::size_t>(*selectedBuilding_)];
        previewFinalStats = towers_.finalStats(previewBuilding);
    }
    if (selectedBuilding_ == BuildingType::Mortar) {
        BuildingInstance preview;
        preview.type = BuildingType::Mortar;
        preview.level = buildings_.blueprintLevels()[static_cast<std::size_t>(BuildingType::Mortar)];
        previewFinalStats = cannons_.finalStats(preview);
    }
    const auto defenseModifiers = shrineDefenseAttackModifiers(runUpgradePower_,shrines_);
    const auto comparisonFor = [&](const BuildingInstance& source) {
        auto comparison = compareBuildingStats(source, crystalMines_, buildingLevelLimit(source.type),
            buildings_.baseMaxHealth(source.type));
        const auto apply = [&](BuildingStats& view, std::uint8_t level) {
            auto building = source; building.level = level;
            std::optional<FinalAttackStats> stats;
            if (source.type == BuildingType::Turret || source.type == BuildingType::GunTurret || source.type == BuildingType::Flamethrower || source.type == BuildingType::MissileLauncher || source.type == BuildingType::TeslaTower || source.type == BuildingType::LaserTower || source.type == BuildingType::WindBlower) stats = towers_.finalStats(building);
            else if (source.type == BuildingType::Cannon || source.type == BuildingType::Catapult || source.type == BuildingType::Mortar) stats = cannons_.finalStats(building);
            else if (source.type == BuildingType::SpikeTrap || source.type == BuildingType::SlowTrap) stats = traps_.finalStats(building);
            if (!stats) return;
            view.finalAttack = stats;
            if (view.pushStrength) view.pushStrength = stats->knockback;
            if (view.attackDamage) view.attackDamage = stats->damage * CombatDisplayScale;
            if (view.attackRange) view.attackRange = stats->range *
                ((source.type == BuildingType::Flamethrower || source.type == BuildingType::WindBlower) ? stats->size : 1.0);
            if (view.attacksPerSecond) view.attacksPerSecond = 1.0 / stats->attackInterval;
            if (supportsAttackStat(source.type, AttackStat::Pierce)) {
                view.piercingCount = static_cast<double>(stats->pierce);
            }
            if (view.effectRadius) *view.effectRadius *= stats->size;
            if (view.cooldown) view.cooldown = stats->attackInterval;
            if (view.effectDuration) view.effectDuration = stats->duration;
            if (view.slowPercent) *view.slowPercent = std::clamp(
                *view.slowPercent * stats->slowStrength, 0.0, 95.0);
        };
        apply(comparison.current, source.level);
        if (comparison.previous) apply(*comparison.previous, static_cast<std::uint8_t>(source.level - 1));
        if (comparison.next) apply(*comparison.next, static_cast<std::uint8_t>(source.level + 1));
        return comparison;
    };
    std::optional<ResourceCost> aimedUpgradeCost;
    std::optional<BuildingStatComparison> aimedStats;
    if (aimedBuilding_) {
        const auto aimed =
            std::find_if(buildings_.buildings().begin(), buildings_.buildings().end(),
                         [this](const BuildingInstance& building) {
                             return building.id == *aimedBuilding_;
                         });
        if (aimed != buildings_.buildings().end() &&
            aimed->level < buildingLevelLimit(aimed->type) &&
            (sandboxMode_ || unlimitedResources_ || aimed->type == BuildingType::Core)) {
            aimedUpgradeCost = buildings_.upgradeCost(*aimed);
        }
        if (aimed != buildings_.buildings().end()) {
            aimedStats = comparisonFor(*aimed);
        }
    }
    const int availableWood = unlimitedResources_
        ? std::numeric_limits<int>::max() : wood_;
    const int availableStone = unlimitedResources_
        ? std::numeric_limits<int>::max() : stone_;
    const int availableCrystals = unlimitedResources_
        ? std::numeric_limits<int>::max() : crystals_;
    auto defenseCards = std::move(defenseCardBuffer_);
    defenseCards.clear();
    defenseCards.reserve(towers_.towers().size() + cannons_.cannons().size());
    for (const BuildingInstance& building : buildings_.buildings()) {
        const BuildingType type = building.type;
        if (type != BuildingType::GunTurret &&
            type != BuildingType::Turret &&
            type != BuildingType::Cannon &&
            type != BuildingType::Catapult && type != BuildingType::Mortar && type != BuildingType::Flamethrower && type != BuildingType::MissileLauncher && type != BuildingType::TeslaTower && type != BuildingType::LaserTower && type != BuildingType::WindBlower) {
            continue;
        }
        const UpgradeError upgradeError = buildings_.validateUpgrade(
            building.id, availableWood, availableStone,
            availableCrystals).error;
        defenseCards.push_back({
            .buildingId = building.id,
            .upgradeError = upgradeError,
            .upgradeCost = buildings_.upgradeCost(building),
            .stats = comparisonFor(building),
        });
    }
    auto producerCards = std::move(producerCardBuffer_);
    producerCards.clear();
    producerCards.reserve(crystalMines_.mines().size());
    for (const CrystalMineRuntime& runtime : crystalMines_.mines()) {
        const auto building = std::ranges::find(
            buildings_.buildings(), runtime.buildingId,
            &BuildingInstance::id);
        if (building == buildings_.buildings().end()) continue;
        const UpgradeError upgradeError = buildings_.validateUpgrade(
            building->id, availableWood, availableStone,
            availableCrystals).error;
        producerCards.push_back({
            .buildingId = building->id,
            .stats = compareBuildingStats(
                *building, crystalMines_, buildingLevelLimit(building->type),
                buildings_.baseMaxHealth(building->type)),
            .upgradeCost = buildings_.upgradeCost(*building),
            .upgradeError = upgradeError,
        });
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
    case PlayerWeapon::LegacyBareHands:
    case PlayerWeapon::LegacyHammer: heldDamage = 0.0; break;
    case PlayerWeapon::Club: heldDamage *= club_.damageMultiplier * playerWeaponLevelDamage(PlayerWeapon::Club,playerWeaponLevel(PlayerWeapon::Club)); break;
    case PlayerWeapon::IceWand: heldDamage = iceWand_.directDamage(); break;
    case PlayerWeapon::FireWand: heldDamage = fireWand_.directDamage(); break;
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
    heldDamage *= playerDamageMultiplier_ * runPlayerDamageMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::Damage) *
        playerClassDamageMultiplier();
    const auto heldWeapon = playerWeapons_.selectedWeapon();
    if (heldWeapon == PlayerWeapon::Club || isPlayerTool(heldWeapon)) heldDamage *= classLevelBonuses().meleeDamage;
    if (heldWeapon == PlayerWeapon::Rifle) heldDamage *= classLevelBonuses().rangedDamage;
    if (heldWeapon == PlayerWeapon::Bomb && isSelectablePlayerClass(playerClass_))
        heldDamage *= classLevelBonuses().rangedDamage;
    // Wand damage already contains the class factor in its runtime definition.
    if ((heldWeapon == PlayerWeapon::IceWand || heldWeapon == PlayerWeapon::FireWand) &&
        isSelectablePlayerClass(playerClass_)) {
        const auto* character = playerClassDefinition(playerClass_);
        heldDamage /= character ? character->damageMultiplier : 1.0;
    }
    std::array<int, 3> recommendedObjectives{-1, -1, -1};
    const auto producerRates = [this](int levelOffset) {
        std::array<double, GameBalance::BuildingTypeCount> rates{};
        for (const BuildingType type : RunEconomyBuildingTypes) {
            const auto index = static_cast<std::size_t>(type);
            const auto level = static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(buildings_.blueprintLevels()[index]) + levelOffset,
                1, static_cast<int>(MaxStructureLevel)));
            rates[index] = static_cast<double>(crystalMines_.productionAmount(level, type)) /
                crystalMines_.productionInterval(type, level);
        }
        return rates;
    };
    const auto producerAmounts = [this](int levelOffset) {
        std::array<int, GameBalance::BuildingTypeCount> amounts{};
        for (const BuildingType type : RunEconomyBuildingTypes) {
            const auto index = static_cast<std::size_t>(type);
            const auto level = static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(buildings_.blueprintLevels()[index]) + levelOffset,
                1, static_cast<int>(MaxStructureLevel)));
            amounts[index] = crystalMines_.productionAmount(level, type);
        }
        return amounts;
    };
    snapshotCache_ = SimulationSnapshot{
        .state = state_,
        .stateBeforePause = stateBeforePause_,
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
        .powerups = powerups_.state(),
        .aimedChest = aimedChest_,
        .aimedLoot = aimedLoot_,
        .lootChests =
            std::span<const LootChestInstance>{lootChests_.chests()},
        .itemPoisonClouds = chestItems_.clouds(),
        .relicGhosts = relics_.ghosts,
        .relicCombatBuildings = relicCombatBuildings_,
        .aimedChallengeColumn = aimedChallengeColumn_,
        .challengeColumns =
            std::span<const ChallengeColumnInstance>{challengeColumns_},
        .aimedWorldLandmark = aimedWorldLandmark_,
        .worldLandmarks =
            std::span<const WorldLandmarkInstance>{worldLandmarks_},
        .worldScenery = std::span<const WorldSceneryInstance>{worldScenery_},
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
            playerDamageMultiplier_ * runPlayerDamageMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::Damage) *
                playerClassDamageMultiplier(),
        .playerMoveSpeedMultiplier =
            playerMoveSpeedMultiplier_ * runPlayerMoveSpeedMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::MoveSpeed) * classLevelBonuses().moveSpeed,
        .playerArmorMultiplier = playerArmorMultiplier_,
        .playerTemporaryHealth = playerTemporaryHealth_,
        .playerRecoverableArmor = playerRecoverableArmor_,
        .playerMaxRecoverableArmor = playerArmorCapacity(),
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
        .chestOpeningCostSurcharge = 0,
        .openedPaidChestCount = lootChests_.openedPaidChestCount(),
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
        .terrainPaths = terrain_.paths(),
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
        .buildingBlueprintLevels = buildings_.blueprintLevels(),
        .wallBaseMaxHealth = buildings_.baseMaxHealth(BuildingType::Wall),
        .producerRatesPerSecond = producerRates(0),
        .nextProducerRatesPerSecond = producerRates(1),
        .producerAmountsPerCycle = producerAmounts(0),
        .nextProducerAmountsPerCycle = producerAmounts(1),
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
        .coreCardPowers = coreCardPowers_,
        .coreHealthBaseLevel = coreHealthBaseLevel_,
        .coreBuildingBaseLevel = coreBuildingBaseLevel_,
        .coreStorageBaseLevel = coreStorageBaseLevel_,
        .placedBuildingCount = buildings_.placedBuildingCount(),
        .totalBuildingLimit = buildings_.totalBuildingLimit(),
        .buildingLimitBonus = buildings_.buildingLimitBonus(),
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
        .aimedBuildingUpgradeCost = aimedUpgradeCost,
        .aimedBuildingStats = aimedStats,
        .defenseCards = std::move(defenseCards),
        .producerCards = std::move(producerCards),
        .enemies = std::span<const EnemyInstance>{enemies_.enemies()},
        .enemyProjectiles = enemies_.projectiles(),
        .decoyBeacons = enemies_.decoyBeacons(),
        .repairStationPulseAge = RepairStationInterval - repairStationCycleRemaining_,
        .repairStationLinks = repairStationLinks_,
        .towers = std::span<const TowerRuntime>{towers_.towers()},
        .laserBeams = towers_.laserBeams(),
        .defenseProjectiles = towers_.projectiles(),
        .aimedFinalAttackStats = aimedFinalStats,
        .droppedDefenseProjectiles = towers_.droppedProjectiles() + cannons_.droppedProjectiles(),
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
            : std::array<std::optional<Vec3>, MaximumAttackPortals>{},
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
            playerWeaponUnlocked(PlayerWeapon::LegacyHammer),
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
        .bombsRemaining = std::numeric_limits<int>::max(),
        .bombCooldownRemaining = bombs_.cooldownRemaining(),
        .bombCooldownDuration = bombs_.cooldownDuration(),
        .bombPurchaseCoinCost = 0,
        .bombPurchaseAmount = 0,
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
        .tutorialObjective = tutorialObjective(),
        .playerLevel = insight_.progress().totalLevelsEarned + 1,
        .currentInsight = insight_.progress().currentInsight,
        .requiredInsight = insight_.progress().requiredInsight,
        .totalInsightEarned = insight_.progress().totalInsightEarned,
        .objectives = {},
        .recommendedObjectives = recommendedObjectives,
        .buildingArsenalCount = runBuildingArsenalCount(),
        .buildingSlotLimit = runBuildingSlotLimit(activeMetaBonuses_),
        .tomeCount = runTomeCount(runUpgradeStacks_),
        .tomeSlotLimit = runTomeSlotLimit(activeMetaBonuses_),
        .weaponSlotLimit = runWeaponSlotLimit(activeMetaBonuses_),
        .runUpgradeChoicePending = runUpgradeChoicePending_,
        .runShop = runShop_,
        .shrines = shrines_,
        .runShopBuyable = [this] {
            std::array<bool, RunShopSlotCount> buyable{};
            for (std::size_t i = 0; i < buyable.size(); ++i)
                buyable[i] = runShop_.offers[i].present && runUpgradeAvailable(runShop_.offers[i].effect);
            return buyable;
        }(),
        .runUpgradeChoiceCount = runUpgradeChoiceCount_,
        .runUpgradePaidChoicePrice = runUpgradePaidChoicePrice_,
        .runUpgradeChoices = runUpgradeChoices_,
        .runUpgradeStacks = runUpgradeStacks_,
        .runUpgradePower = runUpgradePower_,
        .runUpgradeRarities = runUpgradeRarities_,
        .runUpgradeRollSeeds = runUpgradeRollSeeds_,
        .buildingUpgradePowers = buildingUpgradePowers_,
        .upgradeStats = {
            .defenseDamage = defenseDamageMultiplier_ * classLevelBonuses().defenseDamage * powerups_.defenseDamageMultiplier(),
            .defenseFireRate = defenseFireRateMultiplier_ * powerups_.defenseSpeedMultiplier(),
            .buildingHealth = buildingMaxHealthMultiplier_ * runBuildingMaxHealthMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::BuildingHealth),
            .productionSpeed = productionSpeedMultiplier_ * runProductionSpeedMultiplier_ * shrineMultiplier(shrines_, RunUpgradeEffect::ProductionSpeed) * classLevelBonuses().productionSpeed,
            .experienceGain = runExperienceGainMultiplier(playerPassivePower(RunUpgradeEffect::ExperienceGain)),
            .resourceYield = activeMetaBonuses_.resourceYield * classLevelBonuses().gatheringYield,
            .resourceCapacity = 1.0 + .20 * playerPassivePower(RunUpgradeEffect::ResourceCapacity),
            .playerAttackSpeed = playerAttackSpeedMultiplier_ * temporaryAttackSpeedMultiplier() * classLevelBonuses().meleeSpeed *
                (battlePotionBerserkRemaining_ > 0.0 ? 1.35 : 1.0),
            .toolAttackSpeed = toolAttackSpeedMultiplier_ * temporaryAttackSpeedMultiplier() * classLevelBonuses().meleeSpeed *
                (1.0 + 0.15 * playerPassivePower(RunUpgradeEffect::GatheringSpeed)) *
                (playerClass_ == PlayerClass::Berserker ? playerClassDamageMultiplier() : 1.0),
            .luck = activeMetaBonuses_.luck + 0.10 * playerPassivePower(RunUpgradeEffect::Luck),
            .healthRegeneration = 0.5 * playerPassivePower(RunUpgradeEffect::Regeneration),
            .projectileCountBonus = defenseModifiers[AttackStat::Quantity].flat,
            .pierce = defenseModifiers[AttackStat::Pierce].flat,
            .bounces = defenseModifiers[AttackStat::Bounces].flat,
            .clusterExplosions = 2.0 * runUpgradePower(RunUpgradeEffect::ClusterPayload),
            .projectileSpeed = defenseModifiers[AttackStat::ProjectileSpeed].multiplier,
            .attackDuration = defenseModifiers[AttackStat::Duration].multiplier,
            .attackSize = defenseModifiers[AttackStat::Size].multiplier,
            .critChance = std::clamp(defenseModifiers[AttackStat::CritChance].flat, 0.0, 1.0),
            .critDamage = 2.0 * defenseModifiers[AttackStat::CritDamage].multiplier,
            .knockback = defenseModifiers[AttackStat::Knockback].multiplier,
            .cannonAttackStyle = runCannonAttackStyle(runUpgradeStacks_),
        },
        .classAbilityState = classAbilityState_,
        .runUpgradeSelectionsRemaining =
            runUpgradeSelectionsRemaining_,
        .runUpgradeRerollTokens = runUpgradeRerollTokens_,
        .runUpgradeSkipTokens=runUpgradeSkipTokens_,
        .runUpgradeBanishTokens=runUpgradeBanishTokens_,
        .banishedRunUpgrades=banishedRunUpgrades_,
        .runUpgradeLockUnlocked = runUpgradeLockUnlocked_,
        .lockedRunUpgrade = lockedRunUpgrade_,
    };
    return *snapshotCache_;
}

void Simulation::invalidateSnapshotCache() {
    if (snapshotCache_) {
        // Reclaim only the owned card arrays; snapshot spans still borrow world data.
        defenseCardBuffer_ = std::move(snapshotCache_->defenseCards);
        producerCardBuffer_ = std::move(snapshotCache_->producerCards);
        snapshotCache_.reset();
    }
}

} // namespace ian
