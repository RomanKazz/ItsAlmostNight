#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

constexpr double PitchLimit = 1.5533430342749532;
constexpr double DashDuration = 0.18;
constexpr double DashCooldown = 0.90;
constexpr double DashStartSpeed = 19.5;
constexpr double DashEndSpeed = 10.5;

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

} // namespace

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
    dashCooldownRemaining_ = std::max(
        0.0, dashCooldownRemaining_ - deltaSeconds);
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
    const double speed =
        (command.sprint ? gameplay_.sprintSpeed : gameplay_.walkSpeed) *
        playerMoveSpeedMultiplier_ *
        terrain_.waterMovementMultiplier(
            playerPosition_.x, playerPosition_.z);
    const bool hasMovementInput =
        std::hypot(directionX, directionZ) > 1e-6;

    const bool dashUnlocked =
        skillTree_.hasEffect(SkillEffect::Dash);
    if (command.dash && dashUnlocked &&
        dashCooldownRemaining_ <= 0.0 &&
        dashRemaining_ <= 0.0) {
        dashDirection_ = hasMovementInput
            ? Vec3{directionX, 0.0, directionZ}
            : Vec3{sinYaw, 0.0, -cosYaw};
        const double length = std::hypot(
            dashDirection_.x, dashDirection_.z);
        dashDirection_.x /= length;
        dashDirection_.z /= length;
        dashRemaining_ = DashDuration;
        dashCooldownRemaining_ = DashCooldown;
        playerHorizontalVelocity_ = {
            dashDirection_.x * DashStartSpeed,
            0.0,
            dashDirection_.z * DashStartSpeed,
        };
        events_.push_back({
            .type = GameEventType::PlayerDashed,
            .position = playerPosition_,
            .intensity = 1.0,
        });
    }

    const bool dashing = dashRemaining_ > 0.0;
    if (dashing && hasMovementInput) {
        // Small amount of steering keeps the burst expressive without
        // turning it into fully controllable high-speed movement.
        constexpr double DashSteeringRate = 7.5;
        dashDirection_ = moveHorizontalToward(
            dashDirection_, {directionX, 0.0, directionZ},
            DashSteeringRate * deltaSeconds);
        const double length = std::hypot(
            dashDirection_.x, dashDirection_.z);
        if (length > 1e-9) {
            dashDirection_.x /= length;
            dashDirection_.z /= length;
        }
    }
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
    if (skillTree_.hasEffect(SkillEffect::LightFootwork)) {
        constexpr double LightFootworkResponseMultiplier = 1.55;
        velocityChangeRate *= LightFootworkResponseMultiplier;
    }
    if (!hasMovementInput &&
        autoJumpAssistRemaining_ > 0.0) {
        velocityChangeRate = 0.0;
    }
    if (dashing) {
        const double progress = std::clamp(
            1.0 - dashRemaining_ / DashDuration, 0.0, 1.0);
        const double ease = progress * progress;
        const double dashSpeed =
            DashStartSpeed + (DashEndSpeed - DashStartSpeed) * ease;
        const double waterScale = std::sqrt(
            terrain_.waterMovementMultiplier(
                playerPosition_.x, playerPosition_.z));
        playerHorizontalVelocity_ = {
            dashDirection_.x * dashSpeed * waterScale,
            0.0,
            dashDirection_.z * dashSpeed * waterScale,
        };
        dashRemaining_ = std::max(
            0.0, dashRemaining_ - deltaSeconds);
    } else {
        playerHorizontalVelocity_ = moveHorizontalToward(
            playerHorizontalVelocity_, targetVelocity,
            velocityChangeRate * deltaSeconds);
    }
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
    constexpr double AirbornePlatformEdgeBias = 0.22;
    constexpr double MinimumGroundSnapDown = 0.35;
    constexpr double MaximumGroundSnapDown = 0.85;
    const double groundSnapDown = std::clamp(
        std::hypot(movement.x, movement.z) + 0.10,
        MinimumGroundSnapDown,
        MaximumGroundSnapDown);
    const double currentFeetHeight =
        playerPosition_.y - gameplay_.eyeHeight;
    const double maximumWalkableSurfaceHeight =
        playerGrounded_
            ? currentFeetHeight + MaximumStepUp
            : currentFeetHeight +
                  AirbornePlatformEdgeBias;
    const Vec3 movementOrigin = playerPosition_;
    playerPosition_ = collisionWorld_.moveCircle(
        playerPosition_, movement,
        CollisionWorld::PlayerRadius,
        maximumWalkableSurfaceHeight);
    if (!playerGrounded_) {
        const auto edgeCatchSurface =
            collisionWorld_.playerSupportHeight(
                playerPosition_.x,
                playerPosition_.z,
                CollisionWorld::PlayerRadius,
                currentFeetHeight +
                    AirbornePlatformEdgeBias);
        if (edgeCatchSurface &&
            *edgeCatchSurface >
                currentFeetHeight + 1e-6) {
            playerPosition_.y =
                *edgeCatchSurface + gameplay_.eyeHeight;
            verticalVelocity_ = 0.0;
            playerGrounded_ = true;
            edgeSupportGraceRemaining_ = 0.085;
            lastGroundSurfaceHeight_ =
                *edgeCatchSurface;
        } else {
            // The forgiving sweep may only cross a raised side
            // when it can immediately place the feet on top.
            // Retry strictly so a failed jump never embeds the
            // player in the platform wall.
            playerPosition_ = collisionWorld_.moveCircle(
                movementOrigin, movement,
                CollisionWorld::PlayerRadius,
                currentFeetHeight);
        }
    }
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
    if (resources_.consumeCollisionGeometryDirty()) {
        collisionWorld_.syncResourceCylinders(
            resources_.nodes(), treeCollisionAssets_);
    }
    pickaxeCooldownRemaining_ = std::max(0.0, pickaxeCooldownRemaining_ - deltaSeconds);
    playerWeapons_.tick(deltaSeconds);
}

} // namespace ian
