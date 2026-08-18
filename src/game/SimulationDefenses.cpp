#include "game/Simulation.hpp"

namespace ian {

void Simulation::updateTrapCombat(double deltaSeconds) {
    const auto activations =
        traps_.tick(deltaSeconds, buildings_.buildings(), enemies_);
    for (const auto& activation : activations) {
        events_.push_back({
            .type = GameEventType::TrapActivated,
            .entityId = activation.trapId,
            .position = activation.position,
            .amount = activation.affectedCount,
        });
        const auto wear = buildings_.damage(
            activation.trapId, activation.wearDamage);
        if (wear && wear->destroyed) {
            events_.push_back({
                .type = GameEventType::BuildingDestroyed,
                .entityId = wear->id,
                .buildingType = wear->type,
                .position = buildingWorldPosition(*wear),
            });
            syncWorldStructures();
        }
    }
    for (const TrapHit& hit : traps_.hits()) {
        events_.push_back({
            .type = GameEventType::TrapHit,
            .entityId = hit.result.id,
            .sourceId = hit.trapId,
            .position = hit.result.position,
            .damage = hit.result.damage,
        });
        if (hit.result.killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = hit.result.id,
                .sourceId = hit.trapId,
                .position = hit.result.position,
            });
        }
    }
}

void Simulation::updateTowerCombat(double deltaSeconds) {
    if (state_ == RunState::Defeat) {
        return;
    }
    const auto shots =
        towers_.tick(
            deltaSeconds, buildings_.buildings(), enemies_);
    for (const auto& shot : shots) {
        events_.push_back({
            .type = GameEventType::ProjectileHit,
            .entityId = shot.targetId,
            .sourceId = shot.towerId,
            .buildingType = shot.type,
            .position = shot.hitPosition,
            .amount = static_cast<int>(shot.muzzleIndex),
            .damage = shot.damage,
            .secondaryImpact = shot.secondaryImpact,
        });
        if (shot.killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = shot.targetId,
                .position = shot.hitPosition,
            });
        }
    }
}

void Simulation::updateCannonCombat(double deltaSeconds) {
    if (state_ == RunState::Defeat) {
        return;
    }
    const auto explosions =
        cannons_.tick(
            deltaSeconds, buildings_.buildings(), enemies_);
    for (const auto& shot : cannons_.shots()) {
        events_.push_back({
            .type = GameEventType::CannonFired,
            .entityId = shot.projectileId,
            .sourceId = shot.cannonId,
            .buildingType = shot.type,
            .position = shot.position,
        });
    }
    for (const auto& explosion : explosions) {
        events_.push_back({
            .type = GameEventType::Explosion,
            .entityId = explosion.projectileId,
            .position = explosion.position,
            .amount = explosion.killedCount,
            .intensity = explosion.radius,
        });
    }
    for (const CannonHit& hit : cannons_.hits()) {
        events_.push_back({
            .type = GameEventType::CannonHit,
            .entityId = hit.result.id,
            .sourceId = hit.cannonId,
            .buildingType = hit.type,
            .position = hit.result.position,
            .damage = hit.result.damage,
        });
    }
    if (state_ == RunState::Wave &&
        enemies_.activeCount() == 0 &&
        nextWaveSpawnIndex_ >= waveSpawnQueue_.size()) {
        completeWave();
    }
}

} // namespace ian
