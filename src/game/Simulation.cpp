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

constexpr double PitchLimit = 1.5533430342749532;
constexpr double DebugSpawnMinimumRadius = 18.0;
constexpr double DebugSpawnMaximumRadius = 22.0;
constexpr double DebugSpawnCollisionRadius = 0.6;
constexpr double ResourcePlacementClearance = 0.08;

double clampAxis(double value) {
    return std::clamp(value, -1.0, 1.0);
}

Vec3 moveHorizontalToward(
    Vec3 current, Vec3 target, double maximumChange) {
    const double deltaX = target.x - current.x;
    const double deltaZ = target.z - current.z;
    const double distance = std::hypot(deltaX, deltaZ);
    if (distance <= maximumChange || distance <= 1e-9) {
        return {target.x, 0.0, target.z};
    }
    const double scale = maximumChange / distance;
    return {
        current.x + deltaX * scale,
        0.0,
        current.z + deltaZ * scale,
    };
}

bool canAfford(ResourceCost cost, int wood, int stone,
               int gold) {
    return wood >= cost.wood &&
           stone >= cost.stone &&
           gold >= cost.gold;
}

bool resourceOverlapsBox(
    std::span<const ResourceNode> nodes,
    const CollisionBox& box) {
    return std::any_of(
        nodes.begin(), nodes.end(),
        [&box](const ResourceNode& node) {
            if (!node.active) {
                return false;
            }
            const double distanceX = std::max(
                0.0,
                std::max(
                    box.minX - node.position.x,
                    node.position.x - box.maxX));
            const double distanceY = std::max(
                0.0,
                std::max(
                    box.minimumBlockingEyeY -
                        node.position.y,
                    node.position.y -
                        box.maximumBlockingEyeY));
            const double distanceZ = std::max(
                0.0,
                std::max(
                    box.minZ - node.position.z,
                    node.position.z - box.maxZ));
            const double required =
                node.radius +
                ResourcePlacementClearance;
            return distanceX * distanceX +
                       distanceY * distanceY +
                       distanceZ * distanceZ <
                   required * required;
        });
}

CollisionBox platformFloorCollisionBox(
    const PlatformFramePlacement& placement,
    double cellSize) {
    constexpr double PlatformFloorThickness = 0.18;
    return {
        placement.anchor.x * cellSize,
        (placement.anchor.x +
         PlatformFrameWidthCells) *
            cellSize,
        placement.anchor.z * cellSize,
        (placement.anchor.z +
         PlatformFrameWidthCells) *
            cellSize,
        placement.floorHeight,
        placement.floorHeight -
            PlatformFloorThickness,
    };
}

double enemyHeight(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        return 0.675;
    case EnemyType::Heavy:
        return 1.0;
    case EnemyType::Boss:
        return 1.6;
    case EnemyType::Ranged:
        return 0.85;
    case EnemyType::Sapper:
        return 0.78;
    case EnemyType::Flying:
        return 2.4;
    case EnemyType::Basic:
        return 0.8;
    }
    return 0.8;
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
    WorldConfig worldConfig)
    : map_(std::move(map)),
      worldConfig_(worldConfig),
      terrain_(worldConfig_),
      foundations_(terrain_, worldConfig_),
      platformCollisionAsset_(loadGlbCollisionAsset(
          "assets/models/platform.glb")),
      rampCollisionAsset_(loadGlbCollisionAsset(
          "assets/models/ramp.glb")),
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
          }),
      buildings_(balance.buildings, balance.economy, map_.coreBuildRadius),
      collisionWorld_(map_.worldLimit, mapCollisionBoxes(map_)),
      flowField_(mapCollisionBoxes(map_)),
      enemies_(balance.enemies),
      playerWeapons_(balance.weapons.rifle), bombs_(balance.weapons.bomb),
      goldMines_(balance.economy),
      waveDirector_(balance.waves, map_.enemySpawnAnchors),
      economy_(balance.economy), gameplay_(balance.gameplay) {
    playerPosition_ = {
        map_.playerSpawn.x,
        terrain_.getHeight(
            map_.playerSpawn.x,
            map_.playerSpawn.z) +
            map_.playerSpawn.y +
            gameplay_.eyeHeight,
        map_.playerSpawn.z};
    playerHealth_ = gameplay_.playerMaxHealth;
    waveSpawnQueue_.reserve(200);
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

void Simulation::resetRun(GameEventType eventType) {
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
    selectedBuilding_.reset();
    buildingRotation_ = 0;
    buildingPreview_.reset();
    buildings_.reset();
    foundations_.reset();
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
    bombs_.reset();
    goldMines_.reset();
    phaseTimeRemaining_ = 0.0;
    phaseDuration_ = 0.0;
    wave_ = 0;
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0;
    waveSpawnGroupSize_ = 1;
    waveSpawnInterval_ = 1.0;
    waveSpawnTimeRemaining_ = 0.0;
    upcomingAttackDirection_.reset();
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
    events_.push_back({.type = GameEventType::PauseChanged});
}

void Simulation::tick(double deltaSeconds, const PlayerCommand& command) {
    if (state_ == RunState::MainMenu || state_ == RunState::Paused ||
        state_ == RunState::Defeat || !std::isfinite(deltaSeconds) ||
        deltaSeconds < 0.0) {
        return;
    }

    updatePlayerRespawn(deltaSeconds);
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

    ++tick_;
    elapsedSeconds_ += deltaSeconds;
}

void Simulation::updatePlayer(double deltaSeconds,
                              const PlayerCommand& command) {
    constexpr double CoyoteTime = 0.10;
    constexpr double JumpBufferTime = 0.12;
    autoJumpAssistRemaining_ = std::max(
        0.0,
        autoJumpAssistRemaining_ - deltaSeconds);
    edgeSupportGraceRemaining_ = std::max(
        0.0,
        edgeSupportGraceRemaining_ - deltaSeconds);
    if (command.jump) {
        jumpBufferRemaining_ = JumpBufferTime;
    } else {
        jumpBufferRemaining_ = std::max(
            0.0, jumpBufferRemaining_ - deltaSeconds);
    }
    if (playerGrounded_) {
        coyoteTimeRemaining_ = CoyoteTime;
    } else {
        coyoteTimeRemaining_ = std::max(
            0.0, coyoteTimeRemaining_ - deltaSeconds);
    }

    playerYaw_ += command.lookYaw;
    playerPitch_ = std::clamp(playerPitch_ + command.lookPitch, -PitchLimit, PitchLimit);

    double forward = clampAxis(command.moveForward);
    double right = clampAxis(command.moveRight);
    const double inputLength = std::sqrt((forward * forward) + (right * right));
    if (inputLength > 1.0) {
        forward /= inputLength;
        right /= inputLength;
    }

    const double sinYaw = std::sin(playerYaw_);
    const double cosYaw = std::cos(playerYaw_);
    const double directionX = (sinYaw * forward) + (cosYaw * right);
    const double directionZ = (-cosYaw * forward) + (sinYaw * right);
    const double speed = command.sprint ? gameplay_.sprintSpeed : gameplay_.walkSpeed;
    const bool hasMovementInput =
        std::hypot(directionX, directionZ) > 1e-6;
    const Vec3 targetVelocity{
        directionX * speed,
        0.0,
        directionZ * speed,
    };
    double velocityChangeRate =
        hasMovementInput
            ? gameplay_.playerAcceleration
            : gameplay_.playerDeceleration;
    if (hasMovementInput &&
        playerHorizontalVelocity_.x * targetVelocity.x +
                playerHorizontalVelocity_.z * targetVelocity.z <
            0.0) {
        velocityChangeRate =
            gameplay_.playerDeceleration;
    }
    if (!playerGrounded_) {
        velocityChangeRate *=
            hasMovementInput ? 0.55 : 0.35;
    }
    if (!hasMovementInput &&
        autoJumpAssistRemaining_ > 0.0) {
        velocityChangeRate = 0.0;
    }
    playerHorizontalVelocity_ = moveHorizontalToward(
        playerHorizontalVelocity_, targetVelocity,
        velocityChangeRate * deltaSeconds);
    if (autoJumpAssistRemaining_ > 0.0) {
        const double inputAlongAssist =
            directionX * autoJumpAssistDirection_.x +
            directionZ * autoJumpAssistDirection_.z;
        if (hasMovementInput && inputAlongAssist < -0.1) {
            autoJumpAssistRemaining_ = 0.0;
            autoJumpAssistDirection_ = {};
        } else {
            const double minimumAutoJumpSpeed =
                gameplay_.walkSpeed * 0.75;
            const double speedAlongAssist =
                playerHorizontalVelocity_.x *
                    autoJumpAssistDirection_.x +
                playerHorizontalVelocity_.z *
                    autoJumpAssistDirection_.z;
            if (speedAlongAssist < minimumAutoJumpSpeed) {
                const double missingSpeed =
                    minimumAutoJumpSpeed - speedAlongAssist;
                playerHorizontalVelocity_.x +=
                    autoJumpAssistDirection_.x * missingSpeed;
                playerHorizontalVelocity_.z +=
                    autoJumpAssistDirection_.z * missingSpeed;
            }
        }
    }
    const Vec3 movement{
        playerHorizontalVelocity_.x * deltaSeconds,
        0.0,
        playerHorizontalVelocity_.z * deltaSeconds,
    };
    const bool autoJump =
        playerGrounded_ &&
        shouldAutoJumpGroundFrame(movement);

    constexpr double MaximumStepUp = 0.65;
    constexpr double MinimumGroundSnapDown = 0.35;
    constexpr double MaximumGroundSnapDown = 0.85;
    const double groundSnapDown = std::clamp(
        std::hypot(movement.x, movement.z) + 0.10,
        MinimumGroundSnapDown,
        MaximumGroundSnapDown);
    const double currentFeetHeight =
        playerPosition_.y - gameplay_.eyeHeight;
    const Vec3 movementOrigin = playerPosition_;
    playerPosition_ = collisionWorld_.moveCircle(
        playerPosition_, movement,
        CollisionWorld::PlayerRadius,
        currentFeetHeight + MaximumStepUp);
    if (deltaSeconds > 1e-9) {
        const double actualX =
            playerPosition_.x - movementOrigin.x;
        const double actualZ =
            playerPosition_.z - movementOrigin.z;
        if (std::abs(actualX - movement.x) > 1e-6) {
            playerHorizontalVelocity_.x =
                actualX / deltaSeconds;
        }
        if (std::abs(actualZ - movement.z) > 1e-6) {
            playerHorizontalVelocity_.z =
                actualZ / deltaSeconds;
        }
    }
    const double terrainSurface =
        terrain_.getHeight(
            playerPosition_.x,
            playerPosition_.z);
    double standingSurface = terrainSurface;
    const auto modularSurface =
        collisionWorld_.playerSupportHeight(
            playerPosition_.x,
            playerPosition_.z,
            CollisionWorld::PlayerRadius,
            currentFeetHeight + MaximumStepUp);
    if (modularSurface) {
        standingSurface =
            std::max(standingSurface, *modularSurface);
    }
    const double standingHeight =
        standingSurface + gameplay_.eyeHeight;
    const bool hasStandingSupport =
        standingSurface >=
        currentFeetHeight - groundSnapDown;
    if (playerGrounded_) {
        if (hasStandingSupport) {
            playerPosition_.y = standingHeight;
            constexpr double EdgeSupportGraceSeconds =
                0.085;
            edgeSupportGraceRemaining_ =
                EdgeSupportGraceSeconds;
            lastGroundSurfaceHeight_ = standingSurface;
        } else if (edgeSupportGraceRemaining_ > 0.0) {
            playerPosition_.y =
                lastGroundSurfaceHeight_ +
                gameplay_.eyeHeight;
        } else {
            playerGrounded_ = false;
            verticalVelocity_ = 0.0;
        }
    }

    const bool bufferedJump =
        jumpBufferRemaining_ > 0.0 &&
        (playerGrounded_ || coyoteTimeRemaining_ > 0.0);
    if (bufferedJump || autoJump) {
        if (autoJump) {
            constexpr double AutoJumpAssistSeconds = 0.65;
            const double horizontalSpeed = std::hypot(
                movement.x, movement.z);
            if (horizontalSpeed > 1e-9) {
                autoJumpAssistDirection_ = {
                    movement.x / horizontalSpeed,
                    0.0,
                    movement.z / horizontalSpeed,
                };
            }
            autoJumpAssistRemaining_ =
                AutoJumpAssistSeconds;
        }
        verticalVelocity_ = gameplay_.jumpSpeed;
        playerGrounded_ = false;
        edgeSupportGraceRemaining_ = 0.0;
        coyoteTimeRemaining_ = 0.0;
        jumpBufferRemaining_ = 0.0;
    }

    if (!playerGrounded_) {
        constexpr double HeadAboveEye = 0.15;
        const double previousEyeHeight =
            playerPosition_.y;
        const double previousFeetHeight =
            previousEyeHeight - gameplay_.eyeHeight;
        verticalVelocity_ -= gameplay_.gravity * deltaSeconds;
        const double nextEyeHeight =
            previousEyeHeight +
            verticalVelocity_ * deltaSeconds;
        if (verticalVelocity_ > 0.0) {
            const auto ceiling =
                collisionWorld_.modularCeilingHeight(
                    playerPosition_.x,
                    playerPosition_.z,
                    previousEyeHeight + HeadAboveEye,
                    nextEyeHeight + HeadAboveEye);
            if (ceiling) {
                playerPosition_.y =
                    *ceiling - HeadAboveEye;
                verticalVelocity_ = 0.0;
            } else {
                playerPosition_.y = nextEyeHeight;
            }
        } else {
            playerPosition_.y = nextEyeHeight;
        }
        double landingSurface = terrainSurface;
        if (verticalVelocity_ <= 0.0) {
            const auto sweptLanding =
                collisionWorld_.sweptPlayerLanding(
                    movementOrigin, playerPosition_,
                    CollisionWorld::PlayerRadius,
                    previousFeetHeight,
                    playerPosition_.y -
                        gameplay_.eyeHeight);
            if (sweptLanding) {
                playerPosition_.x =
                    sweptLanding->position.x;
                playerPosition_.z =
                    sweptLanding->position.z;
                landingSurface = std::max(
                    terrain_.getHeight(
                        playerPosition_.x,
                        playerPosition_.z),
                    sweptLanding->surfaceHeight);
            }
        }
        const double landingHeight =
            landingSurface + gameplay_.eyeHeight;
        if (verticalVelocity_ <= 0.0 &&
            playerPosition_.y <= landingHeight) {
            const double landingSpeed = -verticalVelocity_;
            playerPosition_.y = landingHeight;
            verticalVelocity_ = 0.0;
            playerGrounded_ = true;
            constexpr double EdgeSupportGraceSeconds =
                0.085;
            edgeSupportGraceRemaining_ =
                EdgeSupportGraceSeconds;
            lastGroundSurfaceHeight_ = landingSurface;
            if (landingSpeed > 1.0) {
                events_.push_back({
                    .type = GameEventType::PlayerLanded,
                    .position = playerPosition_,
                    .intensity = landingSpeed,
                });
            }
        }
    }

    resources_.tick(
        deltaSeconds, buildings_.buildings(),
        map_.worldLimit, playerPosition_);
    pickaxeCooldownRemaining_ = std::max(0.0, pickaxeCooldownRemaining_ - deltaSeconds);
    playerWeapons_.tick(deltaSeconds);
}

const TerrainHeightfield& Simulation::terrain() const {
    return terrain_;
}

void Simulation::regenerateTerrain(
    std::uint32_t seed) {
    terrain_.generate(seed);
    resources_ = ResourceSystem(
        scatterResources(
            map_.resources, map_.worldLimit,
            terrain_),
        [this](double x, double z) {
            return terrain_.getHeight(x, z);
        });
    foundations_.reset();
    syncModularStructures();
    playerHorizontalVelocity_ = {};
    playerPosition_.y =
        terrain_.getHeight(
            playerPosition_.x,
            playerPosition_.z) +
        gameplay_.eyeHeight;
    verticalVelocity_ = 0.0;
    coyoteTimeRemaining_ = 0.0;
    jumpBufferRemaining_ = 0.0;
    autoJumpAssistRemaining_ = 0.0;
    autoJumpAssistDirection_ = {};
    edgeSupportGraceRemaining_ = 0.0;
    lastGroundSurfaceHeight_ =
        playerPosition_.y - gameplay_.eyeHeight;
    playerGrounded_ = true;
}

PlatformFramePlacement
Simulation::previewFoundation(
    Vec3 terrainHit) const {
    PlatformFramePlacement placement =
        foundations_.previewFoundation(
        terrainHit, playerPosition_);
    const double cellSize =
        worldConfig_.cellSize;
    if (placement.valid() &&
        collisionWorld_.overlapsRampBox(
            platformFloorCollisionBox(
                placement, cellSize))) {
        placement.error =
            ModularPlacementError::Occupied;
    }
    if (placement.valid() &&
        resourceOverlapsRectangle(
            resources_.nodes(),
            placement.anchor.x * cellSize,
            (placement.anchor.x +
             PlatformFrameWidthCells) *
                cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z +
             PlatformFrameWidthCells) *
                cellSize)) {
        placement.error =
            ModularPlacementError::ResourceBlocked;
    }
    if (placement.valid() && !unlimitedResources_ &&
        !canAfford(
            modularBuildingCosts_[
                static_cast<std::size_t>(
                    ModularBuildPiece::Foundation)],
            wood_, stone_, gold_)) {
        placement.error =
            ModularPlacementError::InsufficientResources;
    }
    return placement;
}

std::optional<PlatformFrameInstance>
Simulation::placeFoundation(Vec3 terrainHit) {
    const PlatformFramePlacement preview =
        previewFoundation(terrainHit);
    auto placed =
        foundations_.placePlatformFrame(preview);
    if (placed) {
        if (!unlimitedResources_) {
            const ResourceCost cost =
                modularBuildingCosts_[
                    static_cast<std::size_t>(
                        ModularBuildPiece::
                            Foundation)];
            wood_ -= cost.wood;
            stone_ -= cost.stone;
            gold_ -= cost.gold;
        }
        syncModularStructures();
        raisePlayerOntoGroundFrame(*placed);
    }
    return placed;
}

PlatformFramePlacement
Simulation::previewFloorPlatform(
    GridCoord anchor, int storey,
    double floorHeight) const {
    PlatformFramePlacement placement =
        foundations_.previewFloorPlatform(
            anchor, storey, floorHeight,
            playerPosition_);
    const double cellSize = worldConfig_.cellSize;
    const CollisionBox floorBox =
        platformFloorCollisionBox(
            placement, cellSize);
    if (placement.valid() &&
        collisionWorld_.overlapsRampBox(floorBox)) {
        placement.error =
            ModularPlacementError::Occupied;
    }
    if (placement.valid() &&
        resourceOverlapsBox(
            resources_.nodes(), floorBox)) {
        placement.error =
            ModularPlacementError::ResourceBlocked;
    }
    if (placement.valid() &&
        !unlimitedResources_ &&
        !canAfford(
            modularBuildingCosts_[
                static_cast<std::size_t>(
                    ModularBuildPiece::
                        FloorPlatform)],
            wood_, stone_, gold_)) {
        placement.error =
            ModularPlacementError::InsufficientResources;
    }
    return placement;
}

std::optional<PlatformFrameInstance>
Simulation::placeFloorPlatform(
    GridCoord anchor, int storey,
    double floorHeight) {
    const PlatformFramePlacement placement =
        previewFloorPlatform(
            anchor, storey, floorHeight);
    auto placed =
        foundations_.placePlatformFrame(
            placement);
    if (placed) {
        if (!unlimitedResources_) {
            const ResourceCost cost =
                modularBuildingCosts_[
                    static_cast<std::size_t>(
                        ModularBuildPiece::
                            FloorPlatform)];
            wood_ -= cost.wood;
            stone_ -= cost.stone;
            gold_ -= cost.gold;
        }
        syncModularStructures();
    }
    return placed;
}

WallPlacement Simulation::previewWall(
    Vec3 terrainHit, Rotation rotation) const {
    WallPlacement placement =
        foundations_.previewWall(
        terrainHit, playerPosition_, rotation);
    const double cellSize =
        worldConfig_.cellSize;
    if (placement.valid() &&
        resourceOverlapsRectangle(
            resources_.nodes(),
            placement.anchor.x * cellSize,
            (placement.anchor.x + 1) * cellSize,
            placement.anchor.z * cellSize,
            (placement.anchor.z + 1) * cellSize)) {
        placement.error =
            ModularPlacementError::ResourceBlocked;
    }
    if (placement.valid() && !unlimitedResources_ &&
        !canAfford(
            modularBuildingCosts_[
                static_cast<std::size_t>(
                    ModularBuildPiece::Wall)],
            wood_, stone_, gold_)) {
        placement.error =
            ModularPlacementError::InsufficientResources;
    }
    return placement;
}

std::optional<WallInstance>
Simulation::placeWall(
    Vec3 terrainHit, Rotation rotation) {
    const WallPlacement preview =
        previewWall(terrainHit, rotation);
    auto placed = foundations_.placeWall(preview);
    if (placed) {
        if (!unlimitedResources_) {
            const ResourceCost cost =
                modularBuildingCosts_[
                    static_cast<std::size_t>(
                        ModularBuildPiece::Wall)];
            wood_ -= cost.wood;
            stone_ -= cost.stone;
            gold_ -= cost.gold;
        }
        syncModularStructures();
    }
    return placed;
}

RampPlacement Simulation::previewRamp(
    Vec3 terrainHit, Rotation rotation) const {
    RampPlacement placement =
        foundations_.previewRamp(
        terrainHit, playerPosition_, rotation);
    const double cellSize =
        worldConfig_.cellSize;
    if (placement.valid()) {
        const auto rampBoxes =
            rampCollisionBoxes(
                placement.anchor,
                placement.rotation,
                placement.bottomHeight,
                placement.topHeight,
                cellSize);
        const bool blockedByWorld =
            std::any_of(
                rampBoxes.begin(), rampBoxes.end(),
                [this](const CollisionBox& box) {
                    return collisionWorld_
                        .overlapsBox(box);
                });
        const bool blockedByBuilding =
            std::any_of(
                buildings_.buildings().begin(),
                buildings_.buildings().end(),
                [&rampBoxes](
                    const BuildingInstance& building) {
                    const CollisionBox buildingBox =
                        buildingCollisionBox(
                            building.type,
                            building.gridPosition,
                            building.baseHeight);
                    return std::any_of(
                        rampBoxes.begin(),
                        rampBoxes.end(),
                        [&buildingBox](
                            const CollisionBox& rampBox) {
                            return collisionBoxesOverlap(
                                rampBox, buildingBox);
                        });
                });
        const bool blockedByResource =
            std::any_of(
                rampBoxes.begin(), rampBoxes.end(),
                [this](const CollisionBox& box) {
                    return resourceOverlapsBox(
                        resources_.nodes(), box);
                });
        if (blockedByWorld ||
            blockedByBuilding) {
            placement.error =
                ModularPlacementError::Occupied;
        } else if (blockedByResource) {
            placement.error =
                ModularPlacementError::ResourceBlocked;
        }
    }
    if (placement.valid() && !unlimitedResources_ &&
        !canAfford(
            modularBuildingCosts_[
                static_cast<std::size_t>(
                    ModularBuildPiece::Ramp)],
            wood_, stone_, gold_)) {
        placement.error =
            ModularPlacementError::InsufficientResources;
    }
    return placement;
}

std::optional<RampInstance>
Simulation::placeRamp(
    Vec3 terrainHit, Rotation rotation) {
    const RampPlacement preview =
        previewRamp(terrainHit, rotation);
    auto placed = foundations_.placeRamp(preview);
    if (placed) {
        if (!unlimitedResources_) {
            const ResourceCost cost =
                modularBuildingCosts_[
                    static_cast<std::size_t>(
                        ModularBuildPiece::Ramp)];
            wood_ -= cost.wood;
            stone_ -= cost.stone;
            gold_ -= cost.gold;
        }
        syncModularStructures();
    }
    return placed;
}

void Simulation::setStructuralCollapseEnabled(
    bool enabled) {
    foundations_.setStructuralCollapseEnabled(enabled);
}

bool Simulation::structuralCollapseEnabled() const {
    return foundations_.structuralCollapseEnabled();
}

std::vector<EntityId> Simulation::structuralCollapseRisk(
    std::span<const EntityId> supports) const {
    std::vector<EntityId> result =
        foundations_.structuralGraph().collapseRiskIds(supports);
    std::vector<EntityId> affectedFrames = result;
    affectedFrames.insert(
        affectedFrames.end(), supports.begin(), supports.end());
    for (const BuildingInstance& building :
         buildings_.buildings()) {
        if (building.platformStorey < 0) {
            continue;
        }
        const bool twoByTwo =
            buildingFootprintHalfExtent(building.type) > 0.75;
        const int widthCells = twoByTwo ? 2 : 1;
        const int minimumX =
            twoByTwo ? building.gridPosition.x - 1
                     : building.gridPosition.x;
        const int minimumZ =
            twoByTwo ? building.gridPosition.z - 1
                     : building.gridPosition.z;
        const bool losesPlatform = std::ranges::any_of(
            foundations_.platformFrames(),
            [&](const PlatformFrameInstance& frame) {
                return std::ranges::find(
                           affectedFrames, frame.id) !=
                           affectedFrames.end() &&
                       frame.storey == building.platformStorey &&
                       std::abs(frame.floorHeight -
                                building.baseHeight) <= 0.05 &&
                       minimumX >= frame.anchor.x &&
                       minimumZ >= frame.anchor.z &&
                       minimumX + widthCells <=
                           frame.anchor.x +
                               PlatformFrameWidthCells &&
                       minimumZ + widthCells <=
                           frame.anchor.z +
                               PlatformFrameWidthCells;
            });
        if (losesPlatform) {
            result.push_back(building.id);
        }
    }
    return result;
}

std::size_t Simulation::clearModularBuildings() {
    aimedModularBuilding_.reset();
    const std::size_t removed = foundations_.clear();
    syncModularStructures();
    removeUnsupportedPlatformBuildings();
    return removed;
}

void Simulation::processDebugCommands(const PlayerCommand& command) {
    if (command.enableUnlimitedResources) {
        unlimitedResources_ = !unlimitedResources_;
        playerInvulnerable_ = unlimitedResources_;
        if (unlimitedResources_) {
            playerHealth_ = gameplay_.playerMaxHealth;
        }
    }
    if (command.toggleInvulnerability) {
        if (!unlimitedResources_) {
            playerInvulnerable_ = !playerInvulnerable_;
        }
    }
    if (command.damageCore && !unlimitedResources_) {
        const auto core = buildings_.core();
        if (core) {
            const auto damage =
                buildings_.damage(core->id, command.damageCore->amount);
            if (damage) {
                events_.push_back({
                    .type = GameEventType::CoreDamaged,
                    .entityId = damage->id,
                    .buildingType = damage->type,
                    .position =
                        buildingWorldPosition(*damage),
                    .amount = static_cast<int>(command.damageCore->amount),
                });
                if (damage->destroyed) {
                    syncWorldStructures();
                    state_ = RunState::Defeat;
                    events_.push_back({
                        .type = GameEventType::BuildingDestroyed,
                        .entityId = damage->id,
                        .buildingType = damage->type,
                        .position =
                            buildingWorldPosition(*damage),
                    });
                    events_.push_back({.type = GameEventType::RunEnded});
                }
            }
        }
    }
    if (command.damagePlayer) {
        damagePlayer(
            command.damagePlayer->amount, std::nullopt,
            playerPosition_);
    }
    if (command.spawnEnemy) {
        const auto core = buildings_.core();
        if (core) {
            constexpr double TwoPi = 6.28318530717958647692;
            const int requestedCount = std::clamp(
                command.spawnEnemy->count, 1, 1000);
            std::vector<EnemySpawn> spawns;
            spawns.reserve(
                static_cast<std::size_t>(requestedCount));
            for (int index = 0; index < requestedCount; ++index) {
                const std::uint64_t sequence =
                    debugSpawnSequence_++;
                for (std::uint64_t attempt = 0;
                     attempt < 12; ++attempt) {
                    const std::uint64_t seed =
                        mixBits64(
                            tick_ ^
                            (sequence *
                             0x9e3779b97f4a7c15ULL) ^
                            attempt);
                    const double angle =
                        unitRandom(seed) * TwoPi;
                    const double radius =
                        DebugSpawnMinimumRadius +
                        unitRandom(
                            seed ^
                            0xd1b54a32d192ed03ULL) *
                            (DebugSpawnMaximumRadius -
                             DebugSpawnMinimumRadius);
                    const Vec3 position{
                        static_cast<double>(
                            core->gridPosition.x) +
                            std::cos(angle) * radius,
                        enemyHeight(
                            command.spawnEnemy->type),
                        static_cast<double>(
                            core->gridPosition.z) +
                            std::sin(angle) * radius,
                    };
                    if (std::abs(position.x) >
                            map_.worldLimit -
                                DebugSpawnCollisionRadius ||
                        std::abs(position.z) >
                            map_.worldLimit -
                                DebugSpawnCollisionRadius) {
                        continue;
                    }
                    const bool blocked = std::any_of(
                        collisionWorld_.colliders().begin(),
                        collisionWorld_.colliders().end(),
                        [this, position](
                            const CollisionBox& collider) {
                            return collisionWorld_.overlapsCircle(
                                position,
                                DebugSpawnCollisionRadius,
                                collider);
                        });
                    if (blocked) {
                        continue;
                    }
                    spawns.push_back({
                        .type =
                            command.spawnEnemy->type,
                        .position = position,
                    });
                    break;
                }
            }
            if (!spawns.empty()) {
                enemies_.spawnGroup(spawns);
            }
        }
    }
    if (command.toggleWeapon) {
        playerWeapons_.toggleWeapon();
        selectedBuilding_.reset();
        buildingPreview_.reset();
    }
    if (command.upgradeWeapon) {
        const auto core = buildings_.core();
        const int coreLevel = core ? static_cast<int>(core->level) : 0;
        const int availableGold =
            unlimitedResources_ ? std::numeric_limits<int>::max() : gold_;
        const WeaponUpgradeResult result = playerWeapons_.upgrade(coreLevel, availableGold);
        if (result.valid()) {
            if (!unlimitedResources_) {
                gold_ -= result.goldCost;
            }
            events_.push_back({
                .type = GameEventType::WeaponUpgraded,
                .amount = result.level,
            });
        } else {
            events_.push_back({
                .type = GameEventType::WeaponUpgradeRejected,
                .weaponUpgradeError = result.error,
            });
        }
    }
    if (command.defeatAllEnemies && enemies_.activeCount() > 0) {
        if (state_ == RunState::Wave) {
            nextWaveSpawnIndex_ = waveSpawnQueue_.size();
        }
        enemies_.defeatAll();
    }
}

void Simulation::updatePlayerActions(
    double deltaSeconds, const PlayerCommand& command) {
    const auto production = goldMines_.tick(deltaSeconds);
    for (const auto& produced : production) {
        const auto building = std::find_if(
            buildings_.buildings().begin(),
            buildings_.buildings().end(),
            [&produced](const BuildingInstance& candidate) {
                return candidate.id == produced.mineId;
            });
        const Vec3 productionPosition =
            building != buildings_.buildings().end()
                ? buildingWorldPosition(*building)
                : Vec3{};
        if (produced.buildingType == BuildingType::GoldMine) {
            gold_ = saturatingAdd(gold_, produced.amount);
            events_.push_back({
                .type = GameEventType::GoldProduced,
                .entityId = produced.mineId,
                .buildingType = produced.buildingType,
                .position = productionPosition,
                .amount = produced.amount,
            });
        } else {
            const ResourceType resourceType =
                produced.buildingType ==
                        BuildingType::LumberMill
                    ? ResourceType::Wood
                    : ResourceType::Stone;
            if (resourceType == ResourceType::Wood) {
                wood_ = saturatingAdd(wood_, produced.amount);
            } else {
                stone_ = saturatingAdd(stone_, produced.amount);
            }
            events_.push_back({
                .type = GameEventType::ResourceGranted,
                .entityId = produced.mineId,
                .resourceType = resourceType,
                .buildingType = produced.buildingType,
                .position = productionPosition,
                .amount = produced.amount,
            });
        }
    }

    const Vec3 direction = lookDirection(playerYaw_, playerPitch_);
    aimedBuilding_ = buildings_.raycast(playerPosition_, direction, 4.0);
    aimedModularBuilding_ = foundations_.raycast(
        playerPosition_, direction, 6.0);
    if (command.overrideAimedBuilding) {
        aimedBuilding_ =
            command.aimedBuildingOverride;
    }
    if (command.overrideAimedModularBuilding) {
        aimedModularBuilding_ =
            command.aimedModularBuildingOverride;
    }
    aimedResource_ = playerWeapons_.selectedWeapon() == PlayerWeapon::Pickaxe
                         ? resources_.raycast(
                               playerPosition_, direction,
                               gameplay_.resourceGatherRange)
                         : std::nullopt;
    const double enemyAimRange = playerWeapons_.selectedWeapon() == PlayerWeapon::Rifle
                                     ? playerWeapons_.rifleRange()
                                     : gameplay_.pickaxeRange;
    aimedEnemy_ = enemies_.raycast(playerPosition_, direction, enemyAimRange);
    if (command.useConsumable && bombs_.throwBomb(playerPosition_, direction)) {
        events_.push_back({
            .type = GameEventType::ConsumableUsed,
            .position = playerPosition_,
        });
    }
    if (command.fireRifle && !selectedBuilding_) {
        const auto fire = playerWeapons_.fireRifle(playerPosition_, direction, enemies_);
        if (fire) {
            events_.push_back({
                .type = GameEventType::WeaponFired,
                .position = fire->hitPosition,
            });
            if (fire->targetId) {
                events_.push_back({
                    .type = GameEventType::ProjectileHit,
                    .entityId = fire->targetId,
                    .position = fire->hitPosition,
                });
                if (fire->killed) {
                    events_.push_back({
                        .type = GameEventType::EnemyKilled,
                        .entityId = fire->targetId,
                        .position = fire->hitPosition,
                    });
                    aimedEnemy_.reset();
                }
            }
        }
    }
    constexpr double PickaxeInputBufferSeconds = 0.14;
    if (command.usePickaxe &&
        playerWeapons_.selectedWeapon() == PlayerWeapon::Pickaxe &&
        !selectedBuilding_) {
        pickaxeInputBufferRemaining_ = PickaxeInputBufferSeconds;
    } else {
        pickaxeInputBufferRemaining_ = std::max(
            0.0,
            pickaxeInputBufferRemaining_ - deltaSeconds);
    }
    if (pickaxeInputBufferRemaining_ > 0.0 &&
        playerWeapons_.selectedWeapon() == PlayerWeapon::Pickaxe &&
        !selectedBuilding_ && pickaxeCooldownRemaining_ <= 0.0) {
        pickaxeInputBufferRemaining_ = 0.0;
        pickaxeCooldownRemaining_ = gameplay_.pickaxeCooldown;
        const std::uint64_t attackSeed = mixBits64(
            tick_ ^ (pickaxeAttackSequence_++ *
                     0x9e3779b97f4a7c15ULL));
        const double variation =
            (unitRandom(attackSeed) * 2.0 - 1.0) *
            gameplay_.pickaxeDamageVariation;
        const bool critical =
            unitRandom(
                attackSeed ^ 0xd1b54a32d192ed03ULL) <
            gameplay_.pickaxeCriticalChance;
        const double damage =
            gameplay_.pickaxeDamage * (1.0 + variation) *
            (critical ? 2.0 : 1.0);
        if (aimedEnemy_) {
            const auto result = enemies_.damage(*aimedEnemy_, damage);
            if (result) {
                events_.push_back({
                    .type = GameEventType::PickaxeHit,
                    .entityId = result->id,
                    .position = result->position,
                    .damage = damage,
                    .critical = critical,
                });
            }
            if (result && result->killed) {
                events_.push_back({
                    .type = GameEventType::EnemyKilled,
                    .entityId = result->id,
                    .position = result->position,
                });
                aimedEnemy_.reset();
            }
        } else if (aimedResource_) {
            const Vec3 impactPosition = resourceImpactPosition(
                resources_.nodes(), *aimedResource_,
                playerPosition_, direction);
            const auto hit =
                resources_.damage(*aimedResource_, damage);
            if (hit) {
                events_.push_back({
                    .type =
                        hit->collected ? GameEventType::ResourceCollected : GameEventType::ResourceHit,
                    .entityId = hit->nodeId,
                    .resourceType = hit->type,
                    .position = impactPosition,
                    .amount = hit->amount,
                    .damage = damage,
                    .critical = critical,
                });
                if (hit->amount > 0) {
                    pendingResourceGrants_.push_back({
                        .type = hit->type,
                        .position = impactPosition,
                        .amount = hit->amount,
                        .remaining =
                            ResourcePickupFlightSeconds,
                    });
                }
                if (hit->collected) {
                    aimedResource_.reset();
                }
            }
        }
    }

    const auto bombExplosions = bombs_.tick(deltaSeconds, enemies_);
    for (const auto& explosion : bombExplosions) {
        events_.push_back({
            .type = GameEventType::Explosion,
            .entityId = explosion.projectileId,
            .position = explosion.position,
            .amount = explosion.killedCount,
        });
    }
}

void Simulation::updatePendingResourceGrants(
    double deltaSeconds) {
    for (auto& grant : pendingResourceGrants_) {
        grant.remaining -= deltaSeconds;
        if (grant.remaining > 0.0) {
            continue;
        }
        if (grant.type == ResourceType::Wood) {
            wood_ = saturatingAdd(wood_, grant.amount);
        } else {
            stone_ = saturatingAdd(stone_, grant.amount);
        }
        events_.push_back({
            .type = GameEventType::ResourceGranted,
            .resourceType = grant.type,
            .position = grant.position,
            .amount = grant.amount,
        });
    }
    std::erase_if(
        pendingResourceGrants_,
        [](const PendingResourceGrant& grant) {
            return grant.remaining <= 0.0;
        });
}

void Simulation::updateRunPhase(
    double deltaSeconds, const PlayerCommand& command) {
    if (state_ == RunState::BuildPhase) {
        phaseTimeRemaining_ = std::max(0.0, phaseTimeRemaining_ - deltaSeconds);
        if (phaseTimeRemaining_ <= 0.0 || command.startWaveEarly) {
            state_ = RunState::Sunset;
            phaseTimeRemaining_ = gameplay_.sunsetSeconds;
            phaseDuration_ = phaseTimeRemaining_;
            const auto core = buildings_.core();
            if (core) {
                const Vec3 horizontalView = lookDirection(playerYaw_, 0.0);
                const std::size_t firstAnchor = leastVisibleSpawnAnchor(
                    map_.enemySpawnAnchors, playerPosition_, horizontalView);
                const WavePlan plan =
                    waveDirector_.buildWave(
                        saturatingAdd(wave_, 1),
                        core->gridPosition, firstAnchor);
                prepareWave(plan, core->gridPosition, firstAnchor);
                events_.push_back({
                    .type = GameEventType::AttackDirectionWarned,
                    .position = map_.enemySpawnAnchors[firstAnchor],
                    .amount = saturatingAdd(wave_, 1),
                });
            }
            if (command.startWaveEarly) {
                wave_ = saturatingAdd(wave_, 1);
                beginPreparedWave();
                state_ = RunState::Wave;
                phaseTimeRemaining_ = 0.0;
                phaseDuration_ = 0.0;
                events_.push_back({
                    .type = GameEventType::WaveStarted,
                    .amount = wave_,
                });
            } else {
                events_.push_back({
                    .type = GameEventType::SunsetStarted,
                    .amount = saturatingAdd(wave_, 1),
                });
            }
        }
    } else if (state_ == RunState::Sunset) {
        phaseTimeRemaining_ = std::max(0.0, phaseTimeRemaining_ - deltaSeconds);
        if (phaseTimeRemaining_ <= 0.0 || command.startWaveEarly) {
            const auto core = buildings_.core();
            if (core) {
                wave_ = saturatingAdd(wave_, 1);
                beginPreparedWave();
                state_ = RunState::Wave;
                phaseDuration_ = 0.0;
                events_.push_back({
                    .type = GameEventType::WaveStarted,
                    .amount = wave_,
                });
            }
        }
    } else if (state_ == RunState::WaveComplete) {
        phaseTimeRemaining_ = std::max(0.0, phaseTimeRemaining_ - deltaSeconds);
        if (phaseTimeRemaining_ <= 0.0) {
            state_ = RunState::BuildPhase;
            phaseTimeRemaining_ = gameplay_.betweenWaveSeconds;
            phaseDuration_ = phaseTimeRemaining_;
        }
    }
}

void Simulation::updateCombat(double deltaSeconds) {
    if (state_ == RunState::Wave || enemies_.activeCount() > 0) {
        if (state_ == RunState::Wave) {
            tickWaveSpawning(deltaSeconds);
        }
        updateTrapCombat(deltaSeconds);

        buildModularEnemyTargets(
            foundations_, worldConfig_,
            modularTargetBuffer_);
        const auto& modularTargets =
            modularTargetBuffer_;
        const auto attacks = enemies_.tick(
            deltaSeconds, buildings_.buildings(), flowField_,
            playerRespawning_ ? std::nullopt
                              : std::optional<Vec3>{playerPosition_},
            modularTargets);
        for (const auto& attack : enemies_.playerAttacks()) {
            const auto attacker = enemies_.enemy(attack.enemyId);
            const Vec3 attackPosition =
                attacker ? attacker->position : playerPosition_;
            if (playerRespawning_ || unlimitedResources_ ||
                playerInvulnerable_) {
                continue;
            }
            damagePlayer(
                attack.damage, attack.enemyId,
                attackPosition);
            if (playerRespawning_) {
                break;
            }
        }
        for (const auto& attack : attacks) {
            const auto attacker = enemies_.enemy(attack.enemyId);
            if (unlimitedResources_) {
                const auto core = buildings_.core();
                if (core && core->id == attack.targetId) {
                    continue;
                }
            }
            const auto damage =
                buildings_.damage(
                    attack.targetId, attack.damage);
            if (!damage) {
                if (unlimitedResources_) {
                    continue;
                }
                const auto modularDamage =
                    foundations_.damage(
                        attack.targetId, attack.damage);
                if (!modularDamage) {
                    continue;
                }
                const auto target = std::find_if(
                    modularTargets.begin(),
                    modularTargets.end(),
                    [&attack](
                        const EnemyStructureTarget&
                            candidate) {
                        return candidate.id ==
                               attack.targetId;
                    });
                const Vec3 targetCenter =
                    target != modularTargets.end()
                        ? target->position
                        : Vec3{};
                Vec3 effectCenter = targetCenter;
                if (modularDamage->wall) {
                    effectCenter.y =
                        modularDamage->wall
                            ->bottomHeight;
                } else if (modularDamage->ramp) {
                    effectCenter.y =
                        modularDamage->ramp
                            ->bottomHeight;
                }
                events_.push_back({
                    .type =
                        GameEventType::
                            ModularBuildingDamaged,
                    .entityId = modularDamage->id,
                    .sourceId = attack.enemyId,
                    .platformFrame =
                        modularDamage->platformFrame,
                    .modularWall = modularDamage->wall,
                    .ramp = modularDamage->ramp,
                    .position = effectCenter,
                    .amount =
                        static_cast<int>(attack.damage),
                });
                if (attack.ram) {
                    events_.push_back({
                        .type = GameEventType::BossRamImpact,
                        .entityId =
                            modularDamage->id,
                        .sourceId = attack.enemyId,
                        .platformFrame =
                            modularDamage
                                ->platformFrame,
                        .modularWall =
                            modularDamage->wall,
                        .ramp =
                            modularDamage->ramp,
                        .position =
                            attacker
                                ? attacker->position
                                : effectCenter,
                        .amount =
                            static_cast<int>(
                                attack.damage),
                    });
                }
                if (modularDamage->destroyed) {
                    events_.push_back({
                        .type =
                            GameEventType::
                                ModularBuildingDestroyed,
                        .entityId =
                            modularDamage->id,
                        .platformFrame =
                            modularDamage
                                ->platformFrame,
                        .modularWall =
                            modularDamage->wall,
                        .ramp =
                            modularDamage->ramp,
                        .position = effectCenter,
                    });
                    syncModularStructures();
                    removeUnsupportedPlatformBuildings();
                    if (!buildings_.core()) {
                        cannons_.clearProjectiles();
                        bombs_.clearProjectiles();
                        state_ = RunState::Defeat;
                        events_.push_back({
                            .type =
                                GameEventType::RunEnded,
                        });
                        break;
                    }
                }
                continue;
            }
            goldMines_.syncBuildings(
                buildings_.buildings());
            const Vec3 attackPosition =
                attacker ? attacker->position
                         : buildingWorldPosition(*damage);
            events_.push_back({
                .type = GameEventType::BuildingDamaged,
                .entityId = damage->id,
                .sourceId = attack.enemyId,
                .buildingType = damage->type,
                .position =
                    buildingWorldPosition(*damage),
                .amount = static_cast<int>(attack.damage),
            });
            if (attack.ram) {
                events_.push_back({
                    .type = GameEventType::BossRamImpact,
                    .entityId = damage->id,
                    .sourceId = attack.enemyId,
                    .buildingType = damage->type,
                    .position = attackPosition,
                    .amount = static_cast<int>(attack.damage),
                });
            }
            if (damage->type == BuildingType::Core) {
                events_.push_back({
                    .type = GameEventType::CoreDamaged,
                    .entityId = damage->id,
                    .sourceId = attack.enemyId,
                    .buildingType = damage->type,
                    .position = attackPosition,
                    .amount = static_cast<int>(attack.damage),
                });
            }
            if (damage->destroyed) {
                events_.push_back({
                    .type = GameEventType::BuildingDestroyed,
                    .entityId = damage->id,
                    .buildingType = damage->type,
                    .position =
                        buildingWorldPosition(*damage),
                });
                syncWorldStructures();
                if (damage->type == BuildingType::Core) {
                    cannons_.clearProjectiles();
                    bombs_.clearProjectiles();
                    state_ = RunState::Defeat;
                    events_.push_back({.type = GameEventType::RunEnded});
                    break;
                }
            }
        }

        updateTowerCombat(deltaSeconds);
        updateCannonCombat(deltaSeconds);
    }
}

void Simulation::raisePlayerOntoGroundFrame(
    const PlatformFrameInstance& frame) {
    if (frame.storey != 0) {
        return;
    }
    const double cellSize =
        worldConfig_.cellSize;
    const double minimumX =
        frame.anchor.x * cellSize;
    const double maximumX =
        (frame.anchor.x +
         PlatformFrameWidthCells) *
        cellSize;
    const double minimumZ =
        frame.anchor.z * cellSize;
    const double maximumZ =
        (frame.anchor.z +
         PlatformFrameWidthCells) *
        cellSize;
    const double playerFeet =
        playerPosition_.y -
        gameplay_.eyeHeight;
    if (playerPosition_.x < minimumX ||
        playerPosition_.x > maximumX ||
        playerPosition_.z < minimumZ ||
        playerPosition_.z > maximumZ ||
        playerFeet >= frame.floorHeight - 1e-6) {
        return;
    }
    playerPosition_.y =
        frame.floorHeight +
        gameplay_.eyeHeight;
    verticalVelocity_ = 0.0;
    coyoteTimeRemaining_ = 0.0;
    jumpBufferRemaining_ = 0.0;
    autoJumpAssistRemaining_ = 0.0;
    autoJumpAssistDirection_ = {};
    edgeSupportGraceRemaining_ = 0.085;
    lastGroundSurfaceHeight_ = frame.floorHeight;
    playerGrounded_ = true;
}

bool Simulation::shouldAutoJumpGroundFrame(
    Vec3 movement) const {
    const double movementLength =
        std::hypot(movement.x, movement.z);
    if (movementLength <= 1e-9) {
        return false;
    }
    constexpr double MaximumStepUp = 0.65;
    const double maximumJumpRise =
        gameplay_.jumpSpeed *
            gameplay_.jumpSpeed /
        (2.0 * gameplay_.gravity);
    const double playerFeet =
        playerPosition_.y -
        gameplay_.eyeHeight;
    const Vec3 next{
        playerPosition_.x + movement.x,
        playerPosition_.y,
        playerPosition_.z + movement.z,
    };
    const double cellSize =
        worldConfig_.cellSize;
    const auto distanceSquared =
        [](Vec3 point, double minimumX,
           double maximumX, double minimumZ,
           double maximumZ) {
            const double closestX =
                std::clamp(
                    point.x, minimumX, maximumX);
            const double closestZ =
                std::clamp(
                    point.z, minimumZ, maximumZ);
            const double deltaX =
                point.x - closestX;
            const double deltaZ =
                point.z - closestZ;
            return deltaX * deltaX +
                   deltaZ * deltaZ;
        };
    const double radiusSquared =
        CollisionWorld::PlayerRadius *
        CollisionWorld::PlayerRadius;
    for (const PlatformFrameInstance& frame :
         foundations_.platformFrames()) {
        if (frame.storey != 0) {
            continue;
        }
        const double rise =
            frame.floorHeight - playerFeet;
        if (rise <= MaximumStepUp ||
            rise > maximumJumpRise + 0.10) {
            continue;
        }
        const double minimumX =
            frame.anchor.x * cellSize;
        const double maximumX =
            (frame.anchor.x +
             PlatformFrameWidthCells) *
            cellSize;
        const double minimumZ =
            frame.anchor.z * cellSize;
        const double maximumZ =
            (frame.anchor.z +
             PlatformFrameWidthCells) *
            cellSize;
        const double currentDistance =
            distanceSquared(
                playerPosition_, minimumX,
                maximumX, minimumZ, maximumZ);
        const double nextDistance =
            distanceSquared(
                next, minimumX, maximumX,
                minimumZ, maximumZ);
        if (currentDistance >
                radiusSquared &&
            nextDistance <= radiusSquared &&
            nextDistance < currentDistance) {
            return true;
        }
    }
    return false;
}

bool Simulation::modularRemovalWouldDestroyCore(
    EntityId id) const {
    const auto core = buildings_.core();
    if (!core || core->platformStorey < 0) {
        return false;
    }
    const auto frame = std::find_if(
        foundations_.platformFrames().begin(),
        foundations_.platformFrames().end(),
        [id](const PlatformFrameInstance& candidate) {
            return candidate.id == id;
        });
    if (frame ==
        foundations_.platformFrames().end()) {
        return false;
    }
    const int coreAnchorX =
        snapPlatformFrameAxis(
            core->gridPosition.x - 1);
    const int coreAnchorZ =
        snapPlatformFrameAxis(
            core->gridPosition.z - 1);
    return frame->anchor.x == coreAnchorX &&
           frame->anchor.z == coreAnchorZ &&
           frame->storey <= core->platformStorey;
}

void Simulation::syncWorldStructures() {
    resources_.tick(
        0.0, buildings_.buildings(),
        map_.worldLimit, playerPosition_);
    collisionWorld_.syncBuildings(buildings_.buildings());
    towers_.syncBuildings(buildings_.buildings());
    cannons_.syncBuildings(buildings_.buildings());
    traps_.syncBuildings(buildings_.buildings());
    goldMines_.syncBuildings(buildings_.buildings());
    const auto core = buildings_.core();
    if (core) {
        flowField_.rebuild(core->gridPosition, buildings_.buildings());
        flowDebugVectors_ = flowField_.debugVectors();
    } else {
        flowField_.reset();
        flowDebugVectors_.clear();
    }
}

void Simulation::syncModularStructures() {
    collisionWorld_.syncModularBuildings({
        foundations_.platformFrames(),
        foundations_.walls(),
        foundations_.ramps(),
        worldConfig_.cellSize,
        platformCollisionAsset_.colliders,
        rampCollisionAsset_.colliders,
    });
}

void Simulation::removeUnsupportedPlatformBuildings() {
    std::vector<EntityId> unsupported;
    for (const BuildingInstance& building :
         buildings_.buildings()) {
        if (building.platformStorey < 0) {
            continue;
        }
        const bool twoByTwo =
            buildingFootprintHalfExtent(
                building.type) > 0.75;
        const int widthCells = twoByTwo ? 2 : 1;
        const int minimumX =
            twoByTwo
                ? building.gridPosition.x - 1
                : building.gridPosition.x;
        const int minimumZ =
            twoByTwo
                ? building.gridPosition.z - 1
                : building.gridPosition.z;
        const auto surface =
            foundations_.buildingSurface(
                minimumX, minimumZ, widthCells);
        if (!surface ||
            surface->storey !=
                building.platformStorey ||
            std::abs(
                surface->height -
                building.baseHeight) > 0.05) {
            unsupported.push_back(building.id);
        }
    }
    if (unsupported.empty()) {
        return;
    }
    for (EntityId id : unsupported) {
        const auto removed = buildings_.remove(id);
        if (!removed) {
            continue;
        }
        events_.push_back({
            .type = GameEventType::BuildingDestroyed,
            .entityId = removed->id,
            .buildingType = removed->type,
            .building = *removed,
            .position =
                buildingWorldPosition(*removed),
        });
    }
    aimedBuilding_.reset();
    syncWorldStructures();
}

void Simulation::respawnPlayer() {
    constexpr std::array<GridPosition, 4> RespawnOffsets{{
        {0, 3},
        {3, 0},
        {0, -3},
        {-3, 0},
    }};
    Vec3 respawn{
        map_.playerSpawn.x,
        terrain_.getHeight(
            map_.playerSpawn.x,
            map_.playerSpawn.z) +
            gameplay_.eyeHeight,
        map_.playerSpawn.z};
    const auto core = buildings_.core();
    if (core) {
        for (const GridPosition offset : RespawnOffsets) {
            Vec3 candidate{
                static_cast<double>(core->gridPosition.x + offset.x),
                0.0,
                static_cast<double>(core->gridPosition.z + offset.z),
            };
            candidate.y =
                terrain_.getHeight(
                    candidate.x, candidate.z) +
                gameplay_.eyeHeight;
            const bool blocked = std::any_of(
                collisionWorld_.colliders().begin(), collisionWorld_.colliders().end(),
                [this, candidate](const CollisionBox& collider) {
                    return collisionWorld_.overlapsCircle(
                        candidate, CollisionWorld::PlayerRadius, collider);
                });
            if (!blocked) {
                respawn = candidate;
                break;
            }
        }
    }
    playerPosition_ = respawn;
    playerHorizontalVelocity_ = {};
    verticalVelocity_ = 0.0;
    coyoteTimeRemaining_ = 0.0;
    jumpBufferRemaining_ = 0.0;
    autoJumpAssistRemaining_ = 0.0;
    autoJumpAssistDirection_ = {};
    edgeSupportGraceRemaining_ = 0.0;
    lastGroundSurfaceHeight_ =
        playerPosition_.y - gameplay_.eyeHeight;
    playerGrounded_ = true;
    playerHealth_ = gameplay_.playerMaxHealth;
}

void Simulation::damagePlayer(
    double damage, std::optional<EntityId> attackerId,
    Vec3 attackPosition) {
    if (damage <= 0.0 || playerRespawning_ ||
        unlimitedResources_ || playerInvulnerable_) {
        return;
    }
    playerHealth_ = std::max(0.0, playerHealth_ - damage);
    events_.push_back({
        .type = GameEventType::PlayerDamaged,
        .sourceId = attackerId,
        .position = attackPosition,
        .amount = static_cast<int>(damage),
    });
    if (playerHealth_ <= 0.0) {
        beginPlayerRespawn(attackerId);
    }
}

void Simulation::beginPlayerRespawn(
    std::optional<EntityId> attackerId) {
    const auto loss = [this](int carried) {
        if (carried <= 0) {
            return 0;
        }
        return std::min(
            carried,
            static_cast<int>(std::ceil(
                static_cast<double>(carried) *
                gameplay_.playerDeathResourceLossFraction)));
    };
    deathLostWood_ = loss(wood_);
    deathLostStone_ = loss(stone_);
    deathLostGold_ = loss(gold_);
    wood_ -= deathLostWood_;
    stone_ -= deathLostStone_;
    gold_ -= deathLostGold_;
    playerRespawning_ = true;
    playerRespawnTimeRemaining_ =
        gameplay_.playerRespawnSeconds;
    verticalVelocity_ = 0.0;
    playerHorizontalVelocity_ = {};
    coyoteTimeRemaining_ = 0.0;
    jumpBufferRemaining_ = 0.0;
    autoJumpAssistRemaining_ = 0.0;
    autoJumpAssistDirection_ = {};
    edgeSupportGraceRemaining_ = 0.0;
    playerGrounded_ = true;
    buildingPreview_.reset();
    aimedResource_.reset();
    aimedEnemy_.reset();
    aimedBuilding_.reset();
    aimedModularBuilding_.reset();
    events_.push_back({
        .type = GameEventType::PlayerDied,
        .sourceId = attackerId,
        .position = playerPosition_,
        .amount = deathLostWood_ + deathLostStone_ +
                  deathLostGold_,
    });
}

void Simulation::updatePlayerRespawn(double deltaSeconds) {
    if (!playerRespawning_) {
        return;
    }
    playerRespawnTimeRemaining_ = std::max(
        0.0, playerRespawnTimeRemaining_ - deltaSeconds);
    if (playerRespawnTimeRemaining_ > 0.0) {
        return;
    }
    respawnPlayer();
    playerRespawning_ = false;
    events_.push_back({
        .type = GameEventType::PlayerRespawned,
        .position = playerPosition_,
    });
}

std::optional<TutorialObjective> Simulation::tutorialObjective() const {
    const RunState effectiveState =
        state_ == RunState::Paused ? stateBeforePause_ : state_;
    if (effectiveState == RunState::MainMenu ||
        effectiveState == RunState::Defeat || effectiveState == RunState::WaveComplete ||
        wave_ > 1) {
        return std::nullopt;
    }
    if (wave_ == 1) {
        return effectiveState == RunState::Wave
                   ? std::optional<TutorialObjective>{TutorialObjective::SurviveFirstWave}
                   : std::nullopt;
    }
    if (!buildings_.hasCore()) {
        if (!unlimitedResources_ &&
            wood_ < buildings_.configuredCost(BuildingType::Core).wood) {
            return TutorialObjective::MineWood;
        }
        return TutorialObjective::PlaceCore;
    }
    const bool hasGoldMine =
        std::any_of(buildings_.buildings().begin(), buildings_.buildings().end(),
                    [](const BuildingInstance& building) {
                        return building.type == BuildingType::GoldMine;
                    });
    if (!hasGoldMine) {
        if (!unlimitedResources_ &&
            stone_ < buildings_.configuredCost(BuildingType::GoldMine).stone) {
            return TutorialObjective::MineStone;
        }
        return TutorialObjective::BuildGoldMine;
    }
    return TutorialObjective::PrepareForNight;
}

SimulationSnapshot Simulation::snapshot() const {
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
    return {
        .state = state_,
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
        .playerMaxHealth = gameplay_.playerMaxHealth,
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
        .pickaxeCooldownRemaining = pickaxeCooldownRemaining_,
        .aimedResource = aimedResource_,
        .resourceNodes = std::span<const ResourceNode>{resources_.nodes()},
        .worldLimit = map_.worldLimit,
        .worldCellSize = worldConfig_.cellSize,
        .terrainSeed = terrain_.seed(),
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
        .cannonProjectiles =
            std::span<const CannonProjectile>{cannons_.projectiles()},
        .bombProjectiles = std::span<const BombProjectile>{bombs_.projectiles()},
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
        .wave = wave_,
        .bestWave = bestWave_,
        .coreHealth = core ? core->health : 0.0,
        .coreMaxHealth = core ? core->maxHealth : 0.0,
        .coreId = core ? std::optional<EntityId>{core->id} : std::nullopt,
        .coreLevel = core ? core->level : static_cast<std::uint8_t>(0),
        .unlimitedResources = unlimitedResources_,
        .playerInvulnerable = playerInvulnerable_,
        .selectedWeapon = playerWeapons_.selectedWeapon(),
        .selectedWeaponDamage =
            playerWeapons_.selectedWeapon() ==
                    PlayerWeapon::Pickaxe
                ? gameplay_.pickaxeDamage
                : playerWeapons_.rifleDamage(),
        .rifleLevel = playerWeapons_.rifleLevel(),
        .rifleAmmunition = playerWeapons_.ammunition(),
        .rifleMagazineSize = playerWeapons_.magazineSize(),
        .rifleUpgradeGoldCost = playerWeapons_.upgradeGoldCost(),
        .rifleReloading = playerWeapons_.reloading(),
        .rifleReloadRemaining = playerWeapons_.reloadRemaining(),
        .rifleReloadDuration = playerWeapons_.reloadDuration(),
        .bombsRemaining = bombs_.remainingBombs(),
        .waveCompletionReward = saturatingMultiplyNonNegative(
            economy_.waveRewardPerWave, wave_),
        .tutorialWoodTarget = buildings_.configuredCost(BuildingType::Core).wood,
        .tutorialStoneTarget = buildings_.configuredCost(BuildingType::GoldMine).stone,
        .tutorialObjective = tutorialObjective(),
    };
}

std::vector<GameEvent> Simulation::takeEvents() {
    return std::exchange(events_, {});
}

} // namespace ian
