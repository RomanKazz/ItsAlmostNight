#include "game/Simulation.hpp"
#include "core/DeterministicRandom.hpp"
#include "core/SaturatingArithmetic.hpp"
#include "game/ModularCombat.hpp"
#include "game/ResourceWorld.hpp"

#include <algorithm>
#include <array>
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
    std::vector<SkillNodeDefinition> skills)
    : map_(constrainMapToPlayableTerrain(
          std::move(map), worldConfig)),
      worldConfig_(worldConfig),
      terrain_(worldConfig_),
      foundations_(terrain_, worldConfig_),
      platformCollisionAsset_(loadGlbCollisionAsset(
          "assets/models/platform.glb")),
      rampCollisionAsset_(loadGlbCollisionAsset(
          "assets/models/ramp.glb")),
      treeCollisionAssets_{{
          loadGlbCollisionAsset("assets/models/tree.glb"),
          loadGlbCollisionAsset("assets/models/tree_b.glb"),
          loadGlbCollisionAsset("assets/models/tree_c.glb"),
      }},
      modularBuildingCosts_{{
          {
              balance.modularBuildings[0].wood,
              balance.modularBuildings[0].stone,
              balance.modularBuildings[0].gold,
          },
          {
              balance.modularBuildings[0].wood,
              balance.modularBuildings[0].stone,
              balance.modularBuildings[0].gold,
          },
          {
              balance.modularBuildings[1].wood,
              balance.modularBuildings[1].stone,
              balance.modularBuildings[1].gold,
          },
          {
              balance.modularBuildings[2].wood,
              balance.modularBuildings[2].stone,
              balance.modularBuildings[2].gold,
          },
      }},
      resources_(scatterResources(
          map_.resources, map_.worldLimit,
          terrain_),
          [this](double x, double z) {
              return terrain_.getHeight(x, z);
          },
          [this](double x, double z, double radius) {
              return terrain_.waterSignedDistance(x, z) >=
                  radius + 0.8;
          }),
      buildings_(balance.buildings, balance.economy, map_.coreBuildRadius),
      collisionWorld_(map_.worldLimit, mapCollisionBoxes(map_)),
      flowField_(mapCollisionBoxes(map_), &terrain_),
      enemies_(balance.enemies),
      playerWeapons_(balance.weapons.rifle), skillTree_(std::move(skills)),
      bombs_(balance.weapons.bomb),
      iceWand_(balance.weapons.iceWand),
      goldMines_(balance.economy),
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
    lootChests_.setGoldCostMultiplier(
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
    state_ = RunState::Gathering;
    stateBeforePause_ = RunState::Gathering;
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
    deathLostGold_ = 0;
    wood_ = 0;
    stone_ = 0;
    gold_ = 0;
    pendingResourceGrants_.clear();
    unlimitedResources_ = false;
    playerInvulnerable_ = false;
    debugSpawnSequence_ = 0;
    pickaxeAttackSequence_ = 0;
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
    bareHandsWoodGathered_ = 0;
    bareHandsStoneGathered_ = 0;
    introSkillObjectiveCompleted_ = false;
    activeFortifications_.clear();
    bombs_.reset();
    iceWand_.reset();
    goldMines_.reset();
    goldMines_.setProductionSpeedMultiplier(1.0);
    goldMines_.setWoodYieldMultiplier(1.0);
    lootChests_.setGoldCostMultiplier(1.0);
    phaseTimeRemaining_ = 0.0;
    phaseDuration_ = 0.0;
    wave_ = 0;
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0;
    waveSpawnGroupSize_ = 1;
    waveSpawnInterval_ = 1.0;
    waveSpawnTimeRemaining_ = 0.0;
    upcomingAttackDirection_.reset();
    currentWaveHasBoss_ = false;
    modularTargetBuffer_.clear();
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
            resources_.nodes(), treeCollisionAssets_);
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
    updateLootEffects(deltaSeconds);

    ++tick_;
    elapsedSeconds_ += deltaSeconds;
}

void Simulation::updateFortifications(double deltaSeconds) {
    for (auto& fortification : activeFortifications_)
        fortification.remaining -= deltaSeconds;
    std::erase_if(activeFortifications_, [](const ActiveFortification& value) {
        return value.remaining <= 0.0;
    });
}

bool Simulation::isFortified(EntityId id) const {
    return std::ranges::any_of(activeFortifications_, [id](const ActiveFortification& value) {
        return value.id == id;
    });
}

std::vector<GameEvent> Simulation::takeEvents() {
    return std::exchange(events_, {});
}

} // namespace ian
