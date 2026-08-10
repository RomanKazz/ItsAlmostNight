#include "game/Simulation.hpp"

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
                *aimed, goldMines_, MaxBuildingLevel);
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
        .dashUnlocked = skillTree_.hasEffect(SkillEffect::Dash),
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
        .deathLostGold = deathLostGold_,
        .wood = wood_,
        .stone = stone_,
        .gold = gold_,
        .woodCapacity = resourceCapacity(BuildingType::WoodStorage),
        .stoneCapacity = resourceCapacity(BuildingType::StoneStorage),
        .goldCapacity = resourceCapacity(BuildingType::CrystalStorage),
        .coins = coins_,
        .coinPickups = coinPickups_.pickups(),
        .aimedChest = aimedChest_,
        .aimedLoot = aimedLoot_,
        .lootChests =
            std::span<const LootChestInstance>{lootChests_.chests()},
        .nearestChestPosition = nearestChestPosition,
        .nearestChestDistance = nearestChestPosition
            ? nearestChestDistance
            : 0.0,
        .lootStacks = lootStacks_,
        .playerDamageMultiplier = playerDamageMultiplier_,
        .playerMoveSpeedMultiplier = playerMoveSpeedMultiplier_,
        .playerArmorMultiplier = playerArmorMultiplier_,
        .playerTemporaryHealth = playerTemporaryHealth_,
        .chestOpeningCostMultiplier =
            chestOpeningCostMultiplier_,
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
            buildings_.configuredCost(BuildingType::GoldMine),
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
        .wave = wave_,
        .bestWave = bestWave_,
        .coreHealth = core ? core->health : 0.0,
        .coreMaxHealth = core ? core->maxHealth : 0.0,
        .coreId = core ? std::optional<EntityId>{core->id} : std::nullopt,
        .coreLevel = core ? core->level : static_cast<std::uint8_t>(0),
        .unlimitedResources = unlimitedResources_,
        .playerInvulnerable = playerInvulnerable_,
        .automaticToolSwitch = skillTree_.hasEffect(
            SkillEffect::AutoSwitchTools),
        .holdToGather = skillTree_.hasEffect(
            SkillEffect::HoldToGather),
        .unlockedWeapons = {
            true,
            unlimitedResources_ ||
                skillTree_.hasEffect(SkillEffect::UnlockAxe),
            unlimitedResources_ ||
                skillTree_.hasEffect(SkillEffect::UnlockPickaxe),
            unlimitedResources_ ||
                skillTree_.hasEffect(SkillEffect::UnlockClub),
            unlimitedResources_ ||
                skillTree_.hasEffect(SkillEffect::UnlockIceWand),
            unlimitedResources_ ||
                skillTree_.hasEffect(SkillEffect::UnlockFireWand),
            unlimitedResources_ ||
                skillTree_.hasEffect(SkillEffect::UnlockHammer),
            unlimitedResources_ ||
                skillTree_.hasEffect(SkillEffect::UnlockRifle),
        },
        .selectedWeapon = playerWeapons_.selectedWeapon(),
        .selectedWeaponDamage = heldDamage,
        .rifleLevel = playerWeapons_.rifleLevel(),
        .rifleAmmunition = playerWeapons_.ammunition(),
        .rifleMagazineSize = playerWeapons_.magazineSize(),
        .rifleUpgradeGoldCost = playerWeapons_.upgradeGoldCost(),
        .rifleReloading = playerWeapons_.reloading(),
        .rifleReloadRemaining = playerWeapons_.reloadRemaining(),
        .rifleReloadDuration = playerWeapons_.reloadDuration(),
        .bombsRemaining = unlimitedResources_
            ? std::numeric_limits<int>::max()
            : skillTree_.hasEffect(SkillEffect::UnlockBombs)
                ? bombs_.remainingBombs()
                : 0,
        .waveCompletionReward = saturatingMultiplyNonNegative(
            economy_.waveRewardPerWave, wave_),
        .tutorialWoodTarget = buildings_.configuredCost(BuildingType::Core).wood,
        .tutorialStoneTarget = buildings_.configuredCost(BuildingType::GoldMine).stone,
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
