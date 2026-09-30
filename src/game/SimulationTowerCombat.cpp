#include "game/Simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {

void Simulation::updateTowerCombat(double deltaSeconds) {
    if (state_ == RunState::Defeat) {
        return;
    }
    const auto shots =
        towers_.tick(
            deltaSeconds, buildings_.buildings(), enemies_);
    const int ricochetStacks = runUpgradeCount(RunUpgradeEffect::Ricochet);
    const int shrapnelShards = runUpgradeCount(RunUpgradeEffect::GunShrapnel);
    const auto emitSecondaryHit = [this](
        const TowerShot& shot, EntityId targetId, Vec3 origin,
        double damage, double intensity, int visualSequence)
        -> std::optional<EnemyDamageResult> {
        const auto result = enemies_.damage(targetId, damage);
        if (!result) return std::nullopt;
        events_.push_back({
            .type = GameEventType::ProjectileHit,
            .entityId = result->id,
            .sourceId = shot.towerId,
            .buildingType = shot.type,
            .position = result->position,
            .targetPosition = origin,
            .amount = visualSequence,
            .damage = result->damage,
            .intensity = intensity,
            .secondaryImpact = true,
        });
        if (result->killed) {
            events_.push_back({
                .type = GameEventType::EnemyKilled,
                .entityId = result->id,
                .sourceId = shot.towerId,
                .buildingType = shot.type,
                .enemyType = result->type,
                .enemyEliteAffixes = result->eliteAffixes,
                .position = result->position,
            });
        }
        return result;
    };
    for (const auto& shot : shots) {
        const auto source = std::ranges::find_if(
            buildings_.buildings(), [&shot](const BuildingInstance& building) {
                return building.id == shot.towerId;
            });
        const bool sourceExists = source != buildings_.buildings().end();
        const auto validSecondaryTarget = [&](const EnemyInstance& enemy) {
            if (!enemy.active) return false;
            if (shot.type != BuildingType::GunTurret) return true;
            return sourceExists && enemy.type != EnemyType::Flying &&
                std::abs(enemy.worldSurfaceHeight - source->baseHeight) <=
                    1.25;
        };
        const auto fireShrapnel = [&](EntityId killedId,
                                      Vec3 impactPosition,
                                      double impactDamage) {
            if (shrapnelShards <= 0 ||
                shot.type != BuildingType::GunTurret) return;
            constexpr std::size_t MaximumShards = 6;
            constexpr double ShrapnelRadius = 5.0;
            std::array<const EnemyInstance*, MaximumShards> targets{};
            std::array<double, MaximumShards> distances{};
            distances.fill(ShrapnelRadius * ShrapnelRadius);
            const std::size_t shardCount = std::min<std::size_t>(
                MaximumShards,
                static_cast<std::size_t>(shrapnelShards));
            for (const EnemyInstance& enemy : enemies_.enemies()) {
                if (!validSecondaryTarget(enemy) ||
                    enemy.id == killedId) continue;
                const double dx = enemy.position.x - impactPosition.x;
                const double dz = enemy.position.z - impactPosition.z;
                const double distance = dx * dx + dz * dz;
                const auto insertion = std::lower_bound(
                    distances.begin(),
                    distances.begin() +
                        static_cast<std::ptrdiff_t>(shardCount),
                    distance);
                if (insertion == distances.begin() +
                        static_cast<std::ptrdiff_t>(shardCount)) continue;
                const std::size_t index = static_cast<std::size_t>(
                    insertion - distances.begin());
                for (std::size_t shift = shardCount - 1;
                     shift > index; --shift) {
                    targets[shift] = targets[shift - 1];
                    distances[shift] = distances[shift - 1];
                }
                targets[index] = &enemy;
                distances[index] = distance;
            }
            for (std::size_t index = 0; index < shardCount; ++index) {
                if (!targets[index]) break;
                static_cast<void>(emitSecondaryHit(
                    shot, targets[index]->id, impactPosition,
                    impactDamage * 0.45, 0.45, -1));
            }
        };
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
                .sourceId = shot.towerId,
                .buildingType = shot.type,
                .enemyType = shot.targetType,
                .enemyEliteAffixes = shot.targetEliteAffixes,
                .position = shot.hitPosition,
            });
            fireShrapnel(
                shot.targetId, shot.hitPosition,
                shot.payloadDamage);
        }
        if (ricochetStacks <= 0 || shot.secondaryImpact) continue;
        std::array<EntityId, 6> hitIds{};
        hitIds[0] = shot.targetId;
        std::size_t hitCount = 1;
        Vec3 bounceOrigin = shot.hitPosition;
        for (int bounce = 0;
             bounce < ricochetStacks && hitCount < hitIds.size();
             ++bounce) {
            const EnemyInstance* nearest = nullptr;
            double nearestDistanceSquared = 6.0 * 6.0;
            for (const EnemyInstance& enemy : enemies_.enemies()) {
                if (!validSecondaryTarget(enemy) ||
                    std::ranges::find(
                        hitIds.begin(), hitIds.begin() +
                            static_cast<std::ptrdiff_t>(hitCount),
                        enemy.id) != hitIds.begin() +
                            static_cast<std::ptrdiff_t>(hitCount)) continue;
                const double dx = enemy.position.x - bounceOrigin.x;
                const double dz = enemy.position.z - bounceOrigin.z;
                const double distanceSquared = dx * dx + dz * dz;
                if (distanceSquared < nearestDistanceSquared) {
                    nearestDistanceSquared = distanceSquared;
                    nearest = &enemy;
                }
            }
            if (!nearest) break;
            const double fraction = 0.50 * std::pow(0.70, bounce);
            const auto result = emitSecondaryHit(
                shot, nearest->id, bounceOrigin,
                shot.payloadDamage * fraction, fraction,
                bounce + 1);
            if (!result) break;
            hitIds[hitCount++] = result->id;
            bounceOrigin = result->position;
            if (result->killed) {
                fireShrapnel(
                    result->id, result->position,
                    shot.payloadDamage * fraction);
            }
        }
    }
}

} // namespace ian
