#include "game/Simulation.hpp"
#include "core/DeterministicRandom.hpp"
#include "core/SaturatingArithmetic.hpp"
#include "game/ModularCombat.hpp"
#include "game/ResourceWorld.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
#include <utility>

namespace ian {
namespace {

MapDefinition constrainMapToPlayableTerrain(
    MapDefinition map,
    const WorldConfig& worldConfig) {
    const double terrainLimit =
        worldConfig.terrainWorldSize * 0.5;
    const bool raisedBoundaryEnabled =
        worldConfig.terrainBoundaryRiseWidth > 0.0 &&
        worldConfig.terrainBoundaryRiseHeight > 0.0 &&
        worldConfig.terrainWorldSize >=
            worldConfig.terrainBoundaryRiseWidth * 4.0;
    const double mountainBase = raisedBoundaryEnabled
        ? terrainLimit -
              worldConfig.terrainBoundaryRiseWidth
        : terrainLimit;
    map.worldLimit = std::min(
        map.worldLimit,
        std::max(5.0, mountainBase));
    return map;
}

std::uint64_t initialRunSeedState(std::uint32_t configuredSeed) {
    static std::atomic<std::uint64_t> sequence{};
    const auto now = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count());
    return mixBits64(
        now ^
        (sequence.fetch_add(1, std::memory_order_relaxed) + 1U) *
            0x9e3779b97f4a7c15ULL ^
        static_cast<std::uint64_t>(configuredSeed));
}

} // namespace

Vec3 Simulation::lookDirection(double yaw, double pitch) {
    const double cosPitch = std::cos(pitch);
    return {
        std::sin(yaw) * cosPitch,
        std::sin(pitch),
        -std::cos(yaw) * cosPitch,
    };
}

Simulation::Simulation(
    GameBalance balance, MapDefinition map,
    WorldConfig worldConfig,
    std::vector<SkillNodeDefinition> skills,
    InsightConfig insightConfig,
    std::vector<ObjectiveDefinition> objectiveDefinitions)
    : map_(constrainMapToPlayableTerrain(
          std::move(map), worldConfig)),
      worldConfig_(worldConfig),
      terrain_(worldConfig_),
      runSeedState_(initialRunSeedState(worldConfig_.terrainSeed)),
      foundations_(terrain_, worldConfig_),
      platformCollisionAsset_(loadGlbCollisionAsset(
          "assets/models/construction/platform.glb")),
      rampCollisionAsset_(loadGlbCollisionAsset(
          "assets/models/construction/ramp.glb")),
      treeCollisionAssets_{{
          loadGlbCollisionAsset("assets/models/environment/tree_1_a.glb"),
          loadGlbCollisionAsset("assets/models/environment/tree_1_b.glb"),
          loadGlbCollisionAsset("assets/models/environment/tree_1_c.glb"),
          loadGlbCollisionAsset("assets/models/environment/tree_2_a.glb"),
          loadGlbCollisionAsset("assets/models/environment/tree_2_b.glb"),
          loadGlbCollisionAsset("assets/models/environment/tree_2_c.glb"),
          loadGlbCollisionAsset("assets/models/environment/tree_3_a.glb"),
          loadGlbCollisionAsset("assets/models/environment/tree_3_b.glb"),
          loadGlbCollisionAsset("assets/models/environment/tree_3_c.glb"),
      }},
      stoneCollisionAssets_{{
          loadGlbCollisionAsset("assets/models/environment/stone_1.glb"),
          loadGlbCollisionAsset("assets/models/environment/stone_2.glb"),
          loadGlbCollisionAsset("assets/models/environment/stone_3.glb"),
      }},
      modularBuildingCosts_{{
          {
              balance.modularBuildings[0].wood,
              balance.modularBuildings[0].stone,
              balance.modularBuildings[0].crystals,
          },
          {
              balance.modularBuildings[0].wood,
              balance.modularBuildings[0].stone,
              balance.modularBuildings[0].crystals,
          },
          {
              balance.modularBuildings[1].wood,
              balance.modularBuildings[1].stone,
              balance.modularBuildings[1].crystals,
          },
          {
              balance.modularBuildings[2].wood,
              balance.modularBuildings[2].stone,
              balance.modularBuildings[2].crystals,
          },
      }},
      resources_(scatterResources(
          map_.resources, map_.worldLimit,
          terrain_, map_.obstacles),
          [this](double x, double z) {
              return terrain_.getHeight(x, z);
          },
          [this](double x, double z, double radius) {
              return resourceGroundPositionIsSafe(x, z, radius);
          }),
      buildings_(balance.buildings, balance.economy, map_.coreBuildRadius),
      collisionWorld_(map_.worldLimit, mapCollisionBoxes(map_)),
      flowField_(mapCollisionBoxes(map_), &terrain_),
      enemies_(balance.enemies),
      playerWeapons_(balance.weapons.rifle), skillTree_(std::move(skills)),
      insight_(std::move(insightConfig)),
      objectives_(std::move(objectiveDefinitions)),
      bombs_(balance.weapons.bomb),
      iceWand_(balance.weapons.iceWand),
      fireWand_(balance.weapons.fireWand),
      crystalMines_(balance.economy),
      waveDirector_(balance.waves, map_.enemySpawnAnchors),
      economy_(balance.economy), gameplay_(balance.gameplay),
      club_(balance.weapons.club) {
    collisionWorld_.syncPondLilySurfaces(
        generatePondLilyPlacements(terrain_));
    playerPosition_ = {
        map_.playerSpawn.x,
        terrain_.getHeight(
            map_.playerSpawn.x,
            map_.playerSpawn.z) +
            map_.playerSpawn.y +
            gameplay_.eyeHeight,
        map_.playerSpawn.z};
    playerHealth_ = gameplay_.playerMaxHealth;
    playerTemporaryHealth_ = 0.0;
    lootChests_.reset(
        terrain_.seed(), map_.worldLimit, terrain_,
        resources_.nodes(), playerPosition_);
    lootChests_.setCoinCostMultiplier(
        chestOpeningCostMultiplier_);
    waveSpawnQueue_.reserve(WaveDirector::MaximumWaveEnemies);
}

void Simulation::startRun() {
    if (state_ != RunState::MainMenu) {
        return;
    }
    resetRun(GameEventType::RunStarted);
}

void Simulation::restartRun() {
    resetRun(GameEventType::RunRestarted);
}

std::uint32_t Simulation::nextRunTerrainSeed() {
    runSeedState_ += 0x9e3779b97f4a7c15ULL;
    std::uint32_t seed = static_cast<std::uint32_t>(
        mixBits64(runSeedState_));
    if (seed == terrain_.seed()) {
        seed = static_cast<std::uint32_t>(
            mixBits64(runSeedState_ + 0x9e3779b97f4a7c15ULL));
        if (seed == terrain_.seed()) {
            ++seed;
        }
    }
    return seed;
}

bool Simulation::resourceGroundPositionIsSafe(
    double x, double z, double radius) const {
    if (terrain_.waterSignedDistance(x, z) < radius + 0.8) {
        return false;
    }
    return std::none_of(
        map_.obstacles.begin(), map_.obstacles.end(),
        [=](const MapObstacle& obstacle) {
            const double distanceX = std::max(
                0.0,
                std::max(obstacle.collision.minX - x,
                         x - obstacle.collision.maxX));
            const double distanceZ = std::max(
                0.0,
                std::max(obstacle.collision.minZ - z,
                         z - obstacle.collision.maxZ));
            const double required = radius + 0.30;
            return distanceX * distanceX + distanceZ * distanceZ <
                required * required;
        });
}

void Simulation::returnToMainMenu() {
    invalidateSnapshotCache();
    state_ = RunState::MainMenu;
    stateBeforePause_ = RunState::Gathering;
    selectedBuilding_.reset();
    buildingPreview_.reset();
    events_.push_back({.type = GameEventType::PauseChanged});
}

void Simulation::resetRun(GameEventType eventType) {
    invalidateSnapshotCache();
    ++structuralRevision_;
    terrain_.generate(nextRunTerrainSeed());
    resources_ = ResourceSystem(
        scatterResources(
            map_.resources, map_.worldLimit, terrain_, map_.obstacles),
        [this](double x, double z) {
            return terrain_.getHeight(x, z);
        },
        [this](double x, double z, double radius) {
            return resourceGroundPositionIsSafe(x, z, radius);
        });
    state_ = RunState::BuildPhase;
    stateBeforePause_ = RunState::BuildPhase;
    tick_ = 0;
    elapsedSeconds_ = 0.0;
    playerPosition_ = {
        map_.playerSpawn.x,
        terrain_.getHeight(
            map_.playerSpawn.x,
            map_.playerSpawn.z) +
            map_.playerSpawn.y +
            gameplay_.eyeHeight,
        map_.playerSpawn.z};
    playerHorizontalVelocity_ = {};
    verticalVelocity_ = 0.0;
    coyoteTimeRemaining_ = 0.0;
    jumpBufferRemaining_ = 0.0;
    dashRemaining_ = 0.0;
    dashCooldownRemaining_ = 0.0;
    dashDirection_ = {};
    autoJumpAssistRemaining_ = 0.0;
    autoJumpAssistDirection_ = {};
    edgeSupportGraceRemaining_ = 0.0;
    lastGroundSurfaceHeight_ =
        playerPosition_.y - gameplay_.eyeHeight;
    playerYaw_ = 0.0;
    playerPitch_ = 0.0;
    playerGrounded_ = true;
    playerHealth_ = gameplay_.playerMaxHealth;
    playerRespawning_ = false;
    playerRespawnTimeRemaining_ = 0.0;
    deathLostWood_ = 0;
    deathLostStone_ = 0;
    deathLostCrystals_ = 0;
    wood_ = 0;
    stone_ = 0;
    crystals_ = 0;
    crystalStorageFullNotified_ = false;
    coins_ = 0;
    coinPickups_.reset();
    rewardedEnemyCoins_.clear();
    pendingResourceGrants_.clear();
    unlimitedResources_ = false;
    playerInvulnerable_ = false;
    debugSpawnSequence_ = 0;
    pickaxeAttackSequence_ = 0;
    powerSwingResourceHits_ = 0;
    pickaxeCooldownRemaining_ = 0.0;
    pickaxeInputBufferRemaining_ = 0.0;
    aimedResource_.reset();
    resources_.reset();
    resources_.setWoodYieldMultiplier(1.0);
    lootChests_.reset(
        terrain_.seed(), map_.worldLimit, terrain_,
        resources_.nodes(), playerPosition_);
    aimedChest_.reset();
    aimedLoot_.reset();
    playerDamageMultiplier_ = 1.0;
    playerMoveSpeedMultiplier_ = 1.0;
    playerArmorMultiplier_ = 1.0;
    playerMaxHealthMultiplier_ = 1.0;
    buildingMaxHealthMultiplier_ = 1.0;
    productionSpeedMultiplier_ = 1.0;
    woodYieldMultiplier_ = 1.0;
    chestOpeningCostMultiplier_ = 1.0;
    playerBonusMaxHealth_ = 0.0;
    playerTemporaryHealth_ = 0.0;
    secondsSincePlayerDamage_ = 0.0;
    lootStacks_.fill(0);
    selectedBuilding_.reset();
    buildingRotation_ = 0;
    buildingPreview_.reset();
    buildings_.reset();
    buildings_.setMaxHealthMultiplier(1.0);
    buildings_.setNewTowerBonusEnabled(false);
    foundations_.reset();
    foundations_.setMaxHealthMultiplier(1.0);
    modularPlacementBatchDepth_ = 0;
    modularStructuresDirty_ = false;
    collisionWorld_.reset();
    flowField_.reset();
    flowDebugVectors_.clear();
    aimedEnemy_.reset();
    aimedBuilding_.reset();
    aimedModularBuilding_.reset();
    enemies_.reset();
    towers_.reset();
    cannons_.reset();
    traps_.reset();
    playerWeapons_.reset();
    skillTree_.reset();
    insight_.reset();
    objectives_.reset();
    insightRewardedEnemyIds_.clear();
    bareHandsWoodGathered_ = 0;
    bareHandsStoneGathered_ = 0;
    introSkillObjectiveCompleted_ = false;
    activeFortifications_.clear();
    activeRepairCooldowns_.clear();
    bombs_.reset();
    iceWand_.reset();
    fireWand_.reset();
    crystalMines_.reset();
    crystalMines_.setProductionSpeedMultiplier(1.0);
    crystalMines_.setWoodYieldMultiplier(1.0);
    lootChests_.setCoinCostMultiplier(1.0);
    refreshSkillRuntimeEffects();
    phaseTimeRemaining_ = gameplay_.firstBuildPhaseSeconds;
    phaseDuration_ = phaseTimeRemaining_;
    wave_ = 0;
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0;
    waveSpawnGroupSize_ = 1;
    waveSpawnInterval_ = 1.0;
    waveSpawnTimeRemaining_ = 0.0;
    upcomingAttackDirection_.reset();
    currentWaveHasBoss_ = false;
    modularTargetBuffer_.clear();
    collisionWorld_.syncPondLilySurfaces(
        generatePondLilyPlacements(terrain_));
    events_.clear();
    events_.push_back({.type = eventType});
}

void Simulation::togglePause() {
    if (state_ == RunState::MainMenu) {
        return;
    }

    if (state_ == RunState::Paused) {
        state_ = stateBeforePause_;
    } else {
        stateBeforePause_ = state_;
        state_ = RunState::Paused;
    }
    invalidateSnapshotCache();
    events_.push_back({.type = GameEventType::PauseChanged});
}

void Simulation::tick(double deltaSeconds, const PlayerCommand& command) {
    if (state_ == RunState::MainMenu || state_ == RunState::Paused ||
        state_ == RunState::Defeat || !std::isfinite(deltaSeconds) ||
        deltaSeconds < 0.0) {
        return;
    }
    invalidateSnapshotCache();
    const std::size_t firstInsightEvent = events_.size();

    updatePlayerRespawn(deltaSeconds);
    updateFortifications(deltaSeconds);
    updatePlayer(
        deltaSeconds,
        playerRespawning_ ? PlayerCommand{} : command);
    updatePendingResourceGrants(deltaSeconds);
    if (!playerRespawning_) {
        processDebugCommands(command);
        processBuildingCommands(command);
    }
    if (state_ == RunState::Defeat) {
        crystals_ = 0;
        coins_ = 0;
        coinPickups_.reset();
        rewardedEnemyCoins_.clear();
        ++tick_;
        elapsedSeconds_ += deltaSeconds;
        return;
    }
    if (foundations_.updateStructuralSupport(
            deltaSeconds)) {
        for (const ModularBuildingDamageResult& collapsed :
             foundations_.takeCollapsedBuildings()) {
            events_.push_back({
                .type =
                    GameEventType::
                        ModularBuildingDestroyed,
                .entityId = collapsed.id,
                .platformFrame =
                    collapsed.platformFrame,
                .modularWall = collapsed.wall,
                .ramp = collapsed.ramp,
                .position = modularBaseCenter(
                    collapsed, worldConfig_),
            });
        }
        syncModularStructures();
        const bool coreWasSupported =
            buildings_.hasCore();
        removeUnsupportedPlatformBuildings();
        if (coreWasSupported && !buildings_.hasCore()) {
            cannons_.clearProjectiles();
            bombs_.clearProjectiles();
            state_ = RunState::Defeat;
            crystals_ = 0;
            coins_ = 0;
            coinPickups_.reset();
            rewardedEnemyCoins_.clear();
            events_.push_back({
                .type = GameEventType::RunEnded,
            });
            ++tick_;
            elapsedSeconds_ += deltaSeconds;
            return;
        }
    }
    updatePlayerActions(
        deltaSeconds,
        playerRespawning_ ? PlayerCommand{} : command);
    // Resource damage happens after the regular movement update. Flush a
    // changed node collider before the next frame so a depleted tree/rock
    // cannot remain a physical obstacle until its respawn.
    if (resources_.consumeCollisionGeometryDirty()) {
        collisionWorld_.syncResourceCylinders(
            resources_.nodes(), treeCollisionAssets_,
            stoneCollisionAssets_);
    }
    if (playerRespawning_) {
        aimedResource_.reset();
        aimedEnemy_.reset();
        aimedBuilding_.reset();
        aimedModularBuilding_.reset();
    }
    updateRunPhase(
        deltaSeconds,
        playerRespawning_ ? PlayerCommand{} : command);
    updateCombat(deltaSeconds);
    if (state_ == RunState::Defeat) {
        crystals_ = 0;
        coins_ = 0;
        coinPickups_.reset();
        rewardedEnemyCoins_.clear();
        ++tick_;
        elapsedSeconds_ += deltaSeconds;
        return;
    }
    for (const EnemySplitResult& split : enemies_.takeSplitEvents()) {
        events_.push_back({
            .type = GameEventType::EnemySplit,
            .entityId = split.parentId,
            .position = split.position,
            .amount = split.childCount,
            .intensity = 1.0,
        });
    }
    updateLootEffects(deltaSeconds);
    updateCoinPickups(deltaSeconds);
    processObjectiveEvents(firstInsightEvent);
    processInsightEvents(firstInsightEvent, command.defeatAllEnemies.has_value());

    ++tick_;
    elapsedSeconds_ += deltaSeconds;
}

void Simulation::updateFortifications(double deltaSeconds) {
    for (auto& fortification : activeFortifications_)
        fortification.remaining -= deltaSeconds;
    std::erase_if(activeFortifications_, [](const ActiveFortification& value) {
        return value.remaining <= 0.0;
    });
    for (auto& cooldown : activeRepairCooldowns_)
        cooldown.remaining -= deltaSeconds;
    std::erase_if(
        activeRepairCooldowns_,
        [](const ActiveRepairCooldown& value) {
            return value.remaining <= 0.0;
        });
}

bool Simulation::isFortified(EntityId id) const {
    return std::ranges::any_of(activeFortifications_, [id](const ActiveFortification& value) {
        return value.id == id;
    });
}

double Simulation::repairCooldownRemaining(EntityId id) const {
    const auto cooldown = std::ranges::find(
        activeRepairCooldowns_, id,
        &ActiveRepairCooldown::id);
    return cooldown == activeRepairCooldowns_.end()
        ? 0.0 : std::max(0.0, cooldown->remaining);
}

void Simulation::startRepairCooldown(EntityId id) {
    if (economy_.repairCooldownSeconds <= 0.0) return;
    auto cooldown = std::ranges::find(
        activeRepairCooldowns_, id,
        &ActiveRepairCooldown::id);
    if (cooldown == activeRepairCooldowns_.end()) {
        activeRepairCooldowns_.push_back(
            {id, economy_.repairCooldownSeconds});
    } else {
        cooldown->remaining = economy_.repairCooldownSeconds;
    }
}

std::vector<GameEvent> Simulation::takeEvents() {
    return std::exchange(events_, {});
}

} // namespace ian
