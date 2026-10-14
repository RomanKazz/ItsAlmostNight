#include "game/Simulation.hpp"

namespace ian {
void Simulation::updateTowerCombat(double deltaSeconds) {
    if (state_ == RunState::Defeat) return;
    const auto shots = towers_.tick(deltaSeconds, buildings_.buildings(), enemies_);
    for (const auto& launch : towers_.launches()) {
        events_.push_back({.type = GameEventType::TowerFired,
            .sourceId = launch.sourceId, .buildingType = launch.type,
            .position = launch.origin, .amount = launch.muzzleIndex});
    }
    for (const auto& blast : towers_.blasts()) events_.push_back({
        .type = GameEventType::MissileExplosion, .sourceId = blast.sourceId,
        .buildingType = BuildingType::MissileLauncher, .position = blast.position, .intensity = blast.radius});
    for (const auto& shot : shots) {
        if (shot.type == BuildingType::TeslaTower) events_.push_back({
            .type=GameEventType::ChainLightningHit,.entityId=shot.targetId,.sourceId=shot.towerId,
            .buildingType=shot.type,.position=shot.origin,.targetPosition=shot.hitPosition,
            .amount=shot.muzzleIndex,.secondaryImpact=true});
        events_.push_back({.type = shot.periodicBurn ? GameEventType::FlamethrowerBurnHit : GameEventType::ProjectileHit,
            .entityId = shot.targetId, .sourceId = shot.towerId,
            .buildingType = shot.type, .position = shot.hitPosition, .targetPosition = shot.origin,
            .amount = shot.shrapnel ? -1 : static_cast<int>(shot.muzzleIndex), .damage = shot.damage,
            .projectileDamage = shot.payloadDamage,
            .intensity = shot.type == BuildingType::Flamethrower && !shot.periodicBurn
                ? shot.stats.duration : 0.0, .critical = shot.critical,
            .secondaryImpact = shot.secondaryImpact});
        if (shot.killed) events_.push_back(enemyKilledEvent(
            shot.targetId, shot.targetType, shot.targetEliteAffixes,
            shot.hitPosition, shot.towerId, shot.type));
    }
}
} // namespace ian
