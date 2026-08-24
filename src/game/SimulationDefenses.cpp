#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

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
                .enemyType = hit.result.type,
                .enemyEliteAffixes = hit.result.eliteAffixes,
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
    ricochetedTowerBuffer_.clear();
    const int ricochetStacks = runUpgradeStacks_[runUpgradeIndex(
        RunUpgradeEffect::Ricochet)];
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
                .enemyType = shot.targetType,
                .enemyEliteAffixes = shot.targetEliteAffixes,
                .position = shot.hitPosition,
            });
        }
        if (ricochetStacks <= 0 ||
            std::ranges::find(ricochetedTowerBuffer_, shot.towerId) !=
                ricochetedTowerBuffer_.end()) {
            continue;
        }
        ricochetedTowerBuffer_.push_back(shot.towerId);
        const EnemyInstance* nearest = nullptr;
        double nearestDistance = 6.0;
        for (const EnemyInstance& enemy : enemies_.enemies()) {
            if (!enemy.active || enemy.id == shot.targetId) continue;
            const double distance = std::hypot(
                enemy.position.x - shot.hitPosition.x,
                enemy.position.z - shot.hitPosition.z);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearest = &enemy;
            }
        }
        if (!nearest) continue;
        const double fraction = std::min(
            0.80, 0.45 + 0.10 * static_cast<double>(ricochetStacks - 1));
        const auto result = enemies_.damage(
            nearest->id, shot.damage * fraction);
        if (!result) continue;
        events_.push_back({
            .type = GameEventType::ProjectileHit,
            .entityId = result->id,
            .sourceId = shot.towerId,
            .buildingType = shot.type,
            .position = result->position,
            .damage = result->damage,
            .intensity = fraction,
            .secondaryImpact = true,
        });
        if (result->killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = result->id,
                .sourceId = shot.towerId,
                .enemyType = result->type,
                .enemyEliteAffixes = result->eliteAffixes,
                .position = result->position,
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
        if (hit.result.killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = hit.result.id,
                .sourceId = hit.cannonId,
                .enemyType = hit.result.type,
                .enemyEliteAffixes = hit.result.eliteAffixes,
                .position = hit.result.position,
            });
        }
    }
    if (state_ == RunState::Wave &&
        enemies_.activeCount() == 0 &&
        nextWaveSpawnIndex_ >= waveSpawnQueue_.size()) {
        completeWave();
    }
}

} // namespace ian
