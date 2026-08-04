#include "game/Simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {

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
                static_cast<double>(
                    core->gridPosition.x + offset.x),
                0.0,
                static_cast<double>(
                    core->gridPosition.z + offset.z),
            };
            candidate.y = terrain_.getHeight(
                              candidate.x, candidate.z) +
                          gameplay_.eyeHeight;
            const bool blocked = std::any_of(
                collisionWorld_.colliders().begin(),
                collisionWorld_.colliders().end(),
                [this, candidate](
                    const CollisionBox& collider) {
                    return collisionWorld_.overlapsCircle(
                        candidate,
                        CollisionWorld::PlayerRadius,
                        collider);
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
                gameplay_
                    .playerDeathResourceLossFraction)));
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

std::optional<TutorialObjective>
Simulation::tutorialObjective() const {
    const RunState effectiveState =
        state_ == RunState::Paused ? stateBeforePause_ : state_;
    if (effectiveState == RunState::MainMenu ||
        effectiveState == RunState::Defeat ||
        effectiveState == RunState::WaveComplete || wave_ > 1) {
        return std::nullopt;
    }
    if (!introSkillObjectiveCompleted_ && !unlimitedResources_)
        return TutorialObjective::BareHandsTraining;
    if (wave_ == 1) {
        return effectiveState == RunState::Wave
            ? std::optional<TutorialObjective>{
                  TutorialObjective::SurviveFirstWave}
            : std::nullopt;
    }
    if (!buildings_.hasCore()) {
        if (!unlimitedResources_ &&
            wood_ < buildings_
                        .configuredCost(BuildingType::Core)
                        .wood) {
            return TutorialObjective::MineWood;
        }
        return TutorialObjective::PlaceCore;
    }
    const bool hasGoldMine = std::any_of(
        buildings_.buildings().begin(),
        buildings_.buildings().end(),
        [](const BuildingInstance& building) {
            return building.type == BuildingType::GoldMine;
        });
    if (!hasGoldMine) {
        if (!unlimitedResources_ &&
            stone_ < buildings_
                         .configuredCost(BuildingType::GoldMine)
                         .stone) {
            return TutorialObjective::MineStone;
        }
        return TutorialObjective::BuildGoldMine;
    }
    return TutorialObjective::PrepareForNight;
}

} // namespace ian
