#include "game/Simulation.hpp"
#include "game/PlayerRespawn.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {

void Simulation::respawnPlayer() {
    const auto core = buildings_.core();
    const std::optional<Vec3> corePosition = core
        ? std::optional<Vec3>{Vec3{static_cast<double>(core->gridPosition.x), 0,
                                  static_cast<double>(core->gridPosition.z)}}
        : std::nullopt;
    const Vec3 respawn = findPlayerRespawn(map_.playerSpawn, corePosition,
        terrain_, collisionWorld_, enemies_.enemies(), gameplay_.eyeHeight);
    playerPosition_ = respawn;
    playerHorizontalVelocity_ = {};
    verticalVelocity_ = 0.0;
    dashRemaining_ = 0.0;
    dashCooldownRemaining_ = 0.0;
    dashDirection_ = {};
    flightTapRemaining_ = 0.0;
    playerFlying_ = false;
    coyoteTimeRemaining_ = 0.0;
    jumpBufferRemaining_ = 0.0;
    autoJumpAssistRemaining_ = 0.0;
    autoJumpAssistDirection_ = {};
    edgeSupportGraceRemaining_ = 0.0;
    lastGroundSurfaceHeight_ =
        playerPosition_.y - gameplay_.eyeHeight;
    playerGrounded_ = true;
    playerTemporaryHealth_ = 0.0;
    classAbilityState_ = {};
    refreshRunUpgradeRuntimeEffects();
    playerHealth_ = playerPermanentMaxHealth();
    playerRecoverableArmor_ = playerArmorCapacity();
    secondsSincePlayerDamage_ = 5.0;
    playerPitch_ = 0.0;
    playerRespawnProtectionRemaining_ = PlayerRespawnProtectionSeconds;
}

void Simulation::damagePlayer(
    double damage, std::optional<EntityId> attackerId,
    Vec3 attackPosition, bool ignoreArmor) {
    if (damage <= 0.0 || playerRespawning_ ||
        unlimitedResources_ || playerInvulnerable_ ||
        playerRespawnProtectionRemaining_ > 0.0) {
        return;
    }
    const double mitigatedDamage =
        ignoreArmor
            ? damage
            : damage /
                  std::max(playerArmorMultiplier_, 1.0);
    const double armorBeforeHit = playerRecoverableArmor_;
    const double absorbedByArmor = ignoreArmor
        ? 0.0
        : std::min(playerRecoverableArmor_, mitigatedDamage);
    playerRecoverableArmor_ = std::max(
        0.0, playerRecoverableArmor_ - absorbedByArmor);
    const double healthDamage = mitigatedDamage - absorbedByArmor;
    const double temporaryDamage = std::min(
        playerTemporaryHealth_, healthDamage);
    playerTemporaryHealth_ = std::max(
        0.0, playerTemporaryHealth_ - temporaryDamage);
    playerHealth_ = std::max(
        0.0, playerHealth_ - healthDamage);
    if (breadWellFed_) {
        breadWellFed_ = false;
        refreshRunUpgradeRuntimeEffects();
    }
    if (!ignoreArmor && armorBeforeHit > 0.0 &&
        playerRecoverableArmor_ <= 0.0 &&
        (lootStacks_[lootUpgradeIndex(
             LootUpgradeEffect::IronBar)] > 0 ||
         playerClass_ == PlayerClass::Vanguard)) {
        const int stacks = std::max(1, lootStacks_[lootUpgradeIndex(
            LootUpgradeEffect::IronBar)]);
        const double radius = std::min(
            8.0, 4.5 + 0.7 * static_cast<double>(stacks - 1));
        const double strength = std::min(
            14.0, 8.0 + 1.2 * static_cast<double>(stacks - 1));
        const auto knockedBack = enemies_.knockbackInRadius(
            playerPosition_, radius, strength);
        const std::size_t knockedBackCount = knockedBack.size();
        const auto slowed = enemies_.applySlowInRadius(
            playerPosition_, radius, 0.15, 0.65);
        events_.push_back({
            .type = GameEventType::IronArmorBroken,
            .position = playerPosition_,
            .amount = static_cast<int>(std::max(
                knockedBackCount, slowed.size())),
            .intensity = radius,
        });
    }
    tryConsumeApple();
    secondsSincePlayerDamage_ = 0.0;
    events_.push_back({
        .type = GameEventType::PlayerDamaged,
        .sourceId = attackerId,
        .position = attackPosition,
        .amount = static_cast<int>(std::clamp(damage, 0.0, static_cast<double>(std::numeric_limits<int>::max()))),
    });
    if (playerHealth_ <= 0.0) {
        beginPlayerRespawn(attackerId);
    }
}

void Simulation::applyFallDamage(double landingSpeed) {
    if (landingSpeed > 1.0) {
        events_.push_back({
            .type = GameEventType::PlayerLanded,
            .position = playerPosition_,
            .intensity = landingSpeed,
        });
    }
    const double excessSpeed = std::max(
        0.0,
        landingSpeed - gameplay_.fallDamageSafeSpeed);
    if (excessSpeed <= 0.0 || playerRespawning_ ||
        unlimitedResources_ || playerInvulnerable_ ||
        playerRespawnProtectionRemaining_ > 0.0) {
        return;
    }
    int& ropeStacks = lootStacks_[
        lootUpgradeIndex(LootUpgradeEffect::Rope)];
    const double ropeReduction = std::min(
        0.80,
        gameplay_.ropeFallDamageReduction *
            static_cast<double>(ropeStacks));
    double damage = excessSpeed * excessSpeed *
        gameplay_.fallDamagePerSpeedSquared *
        (1.0 - ropeReduction);
    if (damage < 0.5) {
        return;
    }
    if (damage >= playerHealth_ && ropeStacks > 0) {
        --ropeStacks;
        damage = std::max(0.0, playerHealth_ - 1.0);
        events_.push_back({
            .type = GameEventType::RopeFallSaved,
            .position = playerPosition_,
            .amount = ropeStacks,
            .intensity = landingSpeed,
        });
    }
    damagePlayer(
        damage, std::nullopt, playerPosition_, true);
}

void Simulation::beginPlayerRespawn(
    std::optional<EntityId> attackerId) {
    failActiveChallenge();
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
    deathLostCrystals_ = loss(crystals_);
    wood_ -= deathLostWood_;
    stone_ -= deathLostStone_;
    crystals_ -= deathLostCrystals_;
    playerRespawnProtectionRemaining_ = 0.0;
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
    playerTemporaryHealth_ = 0.0;
    playerRecoverableArmor_ = 0.0;
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
                  deathLostCrystals_,
    });
}

void Simulation::updatePlayerRespawn(double deltaSeconds) {
    playerRespawnProtectionRemaining_ = std::max(
        0.0, playerRespawnProtectionRemaining_ - deltaSeconds);
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
    return TutorialObjective::PrepareForNight;
}

} // namespace ian
