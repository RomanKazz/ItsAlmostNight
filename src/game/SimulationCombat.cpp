#include "game/Simulation.hpp"

#include "core/SaturatingArithmetic.hpp"
#include "game/ModularCombat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
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
                applyPotionWaveStart();
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
                applyPotionWaveStart();
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
            modularTargets, &terrain_,
            {
                foundations_.platformFrames(),
                foundations_.ramps(),
                worldConfig_.cellSize,
                &collisionWorld_,
                structuralRevision_,
            });
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
        bool producerRuntimeDirty = false;
        bool worldStructuresDirty = false;
        for (const auto& attack : attacks) {
            const auto attacker = enemies_.enemy(attack.enemyId);
            if (unlimitedResources_) {
                const auto core = buildings_.core();
                if (core && core->id == attack.targetId) {
                    continue;
                }
            }
            const double fortifiedDamage = isFortified(attack.targetId)
                ? attack.damage * 0.65 : attack.damage;
            const auto damage =
                buildings_.damage(
                    attack.targetId, fortifiedDamage);
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
                        iceWand_.clearProjectiles();
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
            producerRuntimeDirty = true;
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
                worldStructuresDirty = true;
                if (damage->type == BuildingType::Core) {
                    cannons_.clearProjectiles();
                    bombs_.clearProjectiles();
                    iceWand_.clearProjectiles();
                    state_ = RunState::Defeat;
                    events_.push_back({.type = GameEventType::RunEnded});
                    break;
                }
            }
        }
        if (worldStructuresDirty) {
            syncWorldStructures();
        } else if (producerRuntimeDirty) {
            goldMines_.syncBuildings(buildings_.buildings());
        }

        updateTowerCombat(deltaSeconds);
        updateCannonCombat(deltaSeconds);
    }
}

} // namespace ian
