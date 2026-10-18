#include "enemies/EnemySystem.hpp"
#include "enemies/EnemyCollision.hpp"

#include <algorithm>
#include <cmath>

namespace ian {

void EnemySystem::tickAirTank(EnemyInstance& enemy, double dt,
    std::optional<Vec3> playerPosition, bool prioritizePlayer) {
    // The tank pressures the Core instead of being endlessly kited by a player.
    // A beacon can redirect it. Challenge arenas still use their player target.
    const EnemyStructureTarget* target = nullptr;
    for (const auto& structure : structureBuffer_) {
        if (!structure.attackable || structure.traversable) continue;
        // A new beacon pulse affects the next attack, never cancels a
        // headbutt that has already committed to its target.
        if (enemy.airTankWindupRemaining > 0) {
            if (enemy.target == structure.id) { target = &structure; break; }
            continue;
        }
        if (enemy.decoyTarget && enemy.decoyRemaining > 0) {
            if (structure.id == *enemy.decoyTarget) { target = &structure; break; }
        } else if ((!prioritizePlayer || !playerPosition) &&
                   structure.buildingType == BuildingType::Core) {
            target = &structure;
            break;
        }
    }
    const bool playerTarget = !target && prioritizePlayer && playerPosition.has_value();
    if (!target && !playerTarget) {
        enemy.airTankWindupRemaining = 0;
        enemy.target.reset();
        enemy.state = EnemyState::MoveToCore;
        return;
    }
    const std::optional<EntityId> targetId = target ? std::optional{target->id} : std::nullopt;
    const Vec3 aim = target ? target->position : *playerPosition;
    const double dx = aim.x - enemy.position.x, dz = aim.z - enemy.position.z;
    const double distance = std::hypot(dx, dz);
    const double contact = enemyCollisionCylinder(enemy).radius + (target ? target->radius : .42);
    const double desiredYaw = std::atan2(dx, dz);
    const double turn = std::atan2(std::sin(desiredYaw - enemy.yaw), std::cos(desiredYaw - enemy.yaw));
    const double turnRate = std::min(2.0, enemy.turnRate);
    enemy.yaw += std::clamp(turn, -turnRate * dt, turnRate * dt);
    const EnemyState attackState = playerTarget ? EnemyState::AttackPlayer :
        target->buildingType == BuildingType::Core ? EnemyState::AttackCore : EnemyState::AttackBuilding;

    if (enemy.airTankWindupRemaining > 0) {
        if (enemy.target != targetId || distance > contact + .75) {
            enemy.airTankWindupRemaining = 0;
            enemy.attackCooldownRemaining = .5;
            return;
        }
        enemy.state = attackState;
        enemy.airTankWindupRemaining = std::max(0.0, enemy.airTankWindupRemaining - dt);
        if (enemy.airTankWindupRemaining > 0) return;
        if (playerTarget) {
            // The windup commits to a point; stepping away avoids the headbutt.
            if (std::hypot(aim.x - enemy.airTankImpactPosition.x,
                           aim.z - enemy.airTankImpactPosition.z) <= .9 &&
                std::abs(aim.y - enemy.airTankImpactPosition.y) <= 1.0)
                playerAttackBuffer_.push_back({enemy.id, enemy.damage});
        } else {
            attackBuffer_.push_back({enemy.id, target->id, enemy.damage, false});
        }
        enemy.attackCooldownRemaining = AirTankRecoverySeconds;
        return;
    }
    enemy.target = targetId;
    if (distance > contact + .25) {
        const double step = std::min(distance - contact - .15,
            enemy.speed * enemy.movementMultiplier * dt);
        if (distance > 1e-9) {
            enemy.position.x += dx / distance * step;
            enemy.position.z += dz / distance * step;
        }
        enemy.state = EnemyState::MoveToCore;
    } else {
        enemy.state = attackState;
        if (enemy.attackCooldownRemaining <= 0 && std::abs(turn) < .3) {
            enemy.airTankWindupRemaining = AirTankWindupSeconds;
            enemy.airTankImpactPosition = aim;
        }
    }
}

} // namespace ian
