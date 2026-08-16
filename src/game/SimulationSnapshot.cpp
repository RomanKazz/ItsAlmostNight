#include "game/Simulation.hpp"

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
    std::optional<ResourceCost> aimedUpgradeCost;
    std::optional<BuildingStatComparison> aimedStats;
    if (aimedBuilding_) {
        const auto aimed =
            std::find_if(buildings_.buildings().begin(), buildings_.buildings().end(),
                         [this](const BuildingInstance& building) {
                             return building.id == *aimedBuilding_;
                         });
        if (aimed != buildings_.buildings().end() &&
            aimed->level < MaxBuildingLevel) {
            aimedUpgradeCost = buildings_.upgradeCost(*aimed);
        }
        if (aimed != buildings_.buildings().end()) {
            aimedStats = compareBuildingStats(
                *aimed, crystalMines_, MaxBuildingLevel);
        }
    }
    const WaveDefinition upcomingComposition =
        waveDirector_.composition(saturatingAdd(wave_, 1));
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
    case PlayerWeapon::BareHands: heldDamage *= 0.25; break;
    case PlayerWeapon::Club: heldDamage *= club_.damageMultiplier; break;
    case PlayerWeapon::IceWand: heldDamage = iceWand_.directDamage(); break;
    case PlayerWeapon::FireWand: heldDamage = fireWand_.directDamage(); break;
    case PlayerWeapon::Hammer: heldDamage *= 0.75; break;
    case PlayerWeapon::Rifle: heldDamage = playerWeapons_.rifleDamage(); break;
    case PlayerWeapon::Bomb:
        heldDamage = 6.0;
        break;
    case PlayerWeapon::Axe:
    case PlayerWeapon::Pickaxe: break;
    }
    heldDamage *= playerDamageMultiplier_;
    std::array<int, 3> recommendedObjectives{-1, -1, -1};
    const auto recommended = objectives_.recommended(recommendedObjectives.size());
    for (std::size_t index = 0; index < recommended.size(); ++index)
        recommendedObjectives[index] = static_cast<int>(recommended[index]);
    snapshotCache_ = SimulationSnapshot{
        .state = state_,
        .tick = tick_,
        .elapsedSeconds = elapsedSeconds_,
        .playerPosition = playerPosition_,
        .playerYaw = playerYaw_,
        .playerPitch = playerPitch_,
        .playerGrounded = playerGrounded_,
        .playerHorizontalVelocity =
            playerHorizontalVelocity_,
        .dashUnlocked = skillTree_.hasEffect("dash.unlock"),
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
        .deathLostWood = deathLostWood_,
        .deathLostStone = deathLostStone_,
        .deathLostCrystals = deathLostCrystals_,
        .wood = wood_,
        .stone = stone_,
        .crystals = crystals_,
        .woodCapacity = resourceCapacity(BuildingType::WoodStorage),
        .stoneCapacity = resourceCapacity(BuildingType::StoneStorage),
        .crystalCapacity = resourceCapacity(BuildingType::CrystalStorage),
        .crystalStorageFull = !unlimitedResources_ &&
            crystals_ >= resourceCapacity(BuildingType::CrystalStorage),
        .coins = coins_,
        .coinPickups = coinPickups_.pickups(),
        .aimedChest = aimedChest_,
        .aimedLoot = aimedLoot_,
        .lootChests =
            std::span<const LootChestInstance>{lootChests_.chests()},
        .aimedChallengeColumn = aimedChallengeColumn_,
        .challengeColumns =
            std::span<const ChallengeColumnInstance>{challengeColumns_},
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
        .playerDamageMultiplier = playerDamageMultiplier_,
        .playerMoveSpeedMultiplier = playerMoveSpeedMultiplier_,
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
        .chestOpeningCostMultiplier =
            chestOpeningCostMultiplier_,
        .freeChestOpeningAvailable =
            freeChestOpeningAvailable_,
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
            if (playerWeapons_.selectedWeapon() ==
                PlayerWeapon::BareHands) {
                return 0.25;
            }
            return resource != resources_.nodes().end()
                ? resourceToolEfficiency(
                      playerWeapons_.selectedWeapon(),
                      resource->type)
                : 1.0;
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
        },
        .modularBuildingCosts = modularBuildingCosts_,
        .buildingPreview = buildingPreview_,
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
                6.0),
        .aimedEnemy = aimedEnemy_,
        .aimedBuilding = aimedBuilding_,
        .aimedBuildingUpgradeCost = aimedUpgradeCost,
        .aimedBuildingStats = aimedStats,
        .enemies = std::span<const EnemyInstance>{enemies_.enemies()},
        .enemyProjectiles = enemies_.projectiles(),
        .towers = std::span<const TowerRuntime>{towers_.towers()},
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
        .pendingEnemyCount = waveSpawnQueue_.size() - nextWaveSpawnIndex_,
        .upcomingEnemyCounts = {
            upcomingComposition.basic,
            upcomingComposition.fast,
            upcomingComposition.heavy,
            upcomingComposition.boss ? 1 : 0,
            upcomingComposition.ranged,
            upcomingComposition.sapper,
            upcomingComposition.flying,
        },
        .upcomingWaveHasBoss = upcomingComposition.boss,
        .upcomingAttackDirection = upcomingAttackDirection_,
        .phaseTimeRemaining = phaseTimeRemaining_,
        .phaseDuration = phaseDuration_,
        .earlyWaveBonus = earlyWaveBonus(),
        .earlyWaveCoinBonus = earlyWaveCoinBonus(),
        .earlyWaveInsightBonus = earlyWaveInsightBonus(),
        .wave = wave_,
        .bestWave = bestWave_,
        .coreHealth = core ? core->health : 0.0,
        .coreMaxHealth = core ? core->maxHealth : 0.0,
        .coreId = core ? std::optional<EntityId>{core->id} : std::nullopt,
        .coreLevel = core ? core->level : static_cast<std::uint8_t>(0),
        .unlimitedResources = unlimitedResources_,
        .playerInvulnerable = playerInvulnerable_,
        .automaticToolSwitch = true,
        .holdToGather = true,
        .unlockedWeapons = {
            true,
            unlimitedResources_ ||
                skillTree_.hasEffect("unlock.axe"),
            unlimitedResources_ ||
                skillTree_.hasEffect("unlock.pickaxe"),
            unlimitedResources_ ||
                skillTree_.hasEffect("unlock.club"),
            unlimitedResources_ ||
                skillTree_.hasEffect("unlock.ice_wand"),
            unlimitedResources_ ||
                skillTree_.hasEffect("unlock.fire_wand"),
            unlimitedResources_ ||
                skillTree_.hasEffect("unlock.hammer"),
            unlimitedResources_ ||
                skillTree_.hasEffect("unlock.rifle"),
            unlimitedResources_ ||
                skillTree_.hasEffect("unlock.bombs"),
        },
        .selectedWeapon = playerWeapons_.selectedWeapon(),
        .selectedWeaponDamage = heldDamage,
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
        .repairAllCoinCost = saturatingAdd(
            economy_.repairAllCoinCost,
            saturatingMultiplyNonNegative(
                economy_.repairAllCoinCostPerWave, wave_)),
        .chestRevealCoinCost = economy_.chestRevealCoinCost,
        .waveCompletionReward = saturatingAdd(
            economy_.waveRewardBase,
            saturatingMultiplyNonNegative(
                economy_.waveRewardPerWave, wave_)),
        .tutorialWoodTarget = buildings_.configuredCost(BuildingType::Core).wood,
        .tutorialStoneTarget = buildings_.configuredCost(BuildingType::CrystalMine).stone,
        .tutorialObjective = tutorialObjective(),
        .skillPoints = unlimitedResources_
            ? std::numeric_limits<int>::max()
            : skillTree_.points(),
        .currentInsight = insight_.progress().currentInsight,
        .requiredInsight = insight_.progress().requiredInsight,
        .totalInsightEarned = insight_.progress().totalInsightEarned,
        .totalTreePointsEarned = insight_.progress().totalTreePointsEarned,
        .objectives = objectives_.statuses(),
        .recommendedObjectives = recommendedObjectives,
        .bareHandsWoodGathered = std::min(bareHandsWoodGathered_, 15),
        .bareHandsStoneGathered = std::min(bareHandsStoneGathered_, 10),
        .introSkillObjectiveCompleted = introSkillObjectiveCompleted_,
    };
    return *snapshotCache_;
}

void Simulation::invalidateSnapshotCache() {
    snapshotCache_.reset();
}

} // namespace ian
