#include "game/Simulation.hpp"

#include "game/ChallengeArena.hpp"

#include "core/SaturatingArithmetic.hpp"
#include "game/ModularCombat.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ian {
namespace {

int remainingTimeUnits(double seconds) {
    constexpr double SecondsPerReward = 10.0;
    return static_cast<int>(std::floor(
        std::max(0.0, seconds) / SecondsPerReward));
}

} // namespace

int Simulation::earlyWaveBonus() const {
    if (unlimitedResources_ || !buildings_.hasCore() ||
        state_ != RunState::BuildPhase ||
        phaseTimeRemaining_ <= 0.0) {
        return 0;
    }
    const int reward = static_cast<int>(std::lround(
        static_cast<double>(remainingTimeUnits(phaseTimeRemaining_)) *
        std::max(0.0, 1.0 +
            skillTree_.effectValue("early.base_bonus"))));
    return std::min(
        reward,
        std::max(0,
            resourceCapacity(BuildingType::CrystalStorage) - crystals_));
}

int Simulation::earlyWaveCoinBonus() const {
    if (skillTree_.effectValue("early.coins") <= -0.99) return 0;
    const int hourglassStacks = lootStacks_[
        lootUpgradeIndex(LootUpgradeEffect::Hourglass)];
    const int rewardSources = hourglassStacks +
        (skillTree_.hasEffect("early.base_bonus") ? 1 : 0);
    const double multiplier = std::max(
        0.0, 1.0 + skillTree_.effectValue("early.base_bonus") +
            skillTree_.effectValue("early.coins"));
    return static_cast<int>(std::lround(
        static_cast<double>(remainingTimeUnits(phaseTimeRemaining_)) *
        static_cast<double>(rewardSources) * multiplier));
}

int Simulation::earlyWaveInsightBonus() const {
    if (skillTree_.effectValue("early.insight") <= -0.99) return 0;
    const int hourglassStacks = lootStacks_[
        lootUpgradeIndex(LootUpgradeEffect::Hourglass)];
    const int rewardSources = hourglassStacks +
        (skillTree_.hasEffect("early.base_bonus") ? 1 : 0);
    const double multiplier = std::max(
        0.0, 1.0 + skillTree_.effectValue("early.base_bonus") +
        skillTree_.effectValue("early.insight"));
    return static_cast<int>(std::lround(
        static_cast<double>(remainingTimeUnits(phaseTimeRemaining_)) *
        static_cast<double>(rewardSources) * multiplier));
}

void Simulation::updateRunPhase(
    double deltaSeconds, const PlayerCommand& command) {
    if (challengeActive()) {
        return;
    }
    if (state_ == RunState::BuildPhase) {
        const int earlyBonus = command.startWaveEarly
            ? earlyWaveBonus()
            : 0;
        const int earlyCoinBonus = command.startWaveEarly
            ? earlyWaveCoinBonus()
            : 0;
        const int earlyInsightBonus = command.startWaveEarly
            ? earlyWaveInsightBonus()
            : 0;
        phaseTimeRemaining_ = std::max(0.0, phaseTimeRemaining_ - deltaSeconds);
        if ((phaseTimeRemaining_ <= 0.0 || command.startWaveEarly) &&
            buildings_.hasCore()) {
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
                const int crystalsBeforeBonus = crystals_;
                addCrystals(earlyBonus);
                const int grantedBonus = crystals_ - crystalsBeforeBonus;
                coins_ = saturatingAdd(
                    coins_, earlyCoinBonus);
                if (earlyInsightBonus > 0) {
                    grantConfiguredInsight(
                        static_cast<double>(earlyInsightBonus),
                        InsightSource::Other,
                        InsightCategory::Exploration,
                        {.eventId =
                             0xe411000000000000ULL |
                             static_cast<std::uint64_t>(
                                 saturatingAdd(wave_, 1)),
                         .oneTime = true,
                         .bypassDiminishing = true});
                }
                if (grantedBonus > 0 || earlyCoinBonus > 0 ||
                    earlyInsightBonus > 0) {
                    events_.push_back({
                        .type = GameEventType::EarlyWaveBonusGranted,
                        .amount = grantedBonus,
                        .coinAmount = earlyCoinBonus,
                        .insightAmount = static_cast<double>(
                            earlyInsightBonus),
                    });
                }
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
            freeChestOpeningAvailable_ =
                lootStacks_[lootUpgradeIndex(
                    LootUpgradeEffect::Key)] > 0;
            phaseTimeRemaining_ =
                gameplay_.betweenWaveSeconds +
                std::max(
                    0.0,
                    skillTree_.effectValue(
                        "day.duration_seconds"));
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
            },
            challengeActive());
        if (challengeActive() && activeChallengeColumn_) {
            enemies_.constrainToArena(
                challengeColumns_[*activeChallengeColumn_].position,
                challenge_arena::FenceRadius -
                    challenge_arena::FenceHalfThickness);
        }
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
                        enemies_.clearProjectiles();
                        cannons_.clearProjectiles();
                        bombs_.clearProjectiles();
                        iceWand_.clearProjectiles();
                        fireWand_.clearProjectiles();
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
                    enemies_.clearProjectiles();
                    cannons_.clearProjectiles();
                    bombs_.clearProjectiles();
                    iceWand_.clearProjectiles();
                    fireWand_.clearProjectiles();
                    state_ = RunState::Defeat;
                    events_.push_back({.type = GameEventType::RunEnded});
                    break;
                }
            }
        }
        if (worldStructuresDirty) {
            syncWorldStructures();
        } else if (producerRuntimeDirty) {
            crystalMines_.syncBuildings(buildings_.buildings());
        }

    }
    // These runtimes also own smoothed visual orientation, so they must tick
    // during building phases even when there are no enemies.
    updateTowerCombat(deltaSeconds);
    updateCannonCombat(deltaSeconds);
}

void Simulation::collectEliteEnemyEvents() {
    for (const EliteEnemyEvent& event :
         enemies_.takeEliteSpawnEvents()) {
        events_.push_back({
            .type = GameEventType::EliteEnemySpawned,
            .entityId = event.id,
            .position = event.position,
            .amount = static_cast<int>(event.affixes),
        });
    }
    for (const EliteEnemyEvent& event :
         enemies_.takeEliteDeathEvents()) {
        if (!hasEliteAffix(
                event.affixes, EliteAffix::Volatile)) {
            continue;
        }
        pendingEliteExplosions_.push_back({
            .sourceId = event.id,
            .position = event.position,
            .remaining = 1.15,
        });
        events_.push_back({
            .type = GameEventType::EliteVolatilePrimed,
            .entityId = event.id,
            .position = event.position,
            .intensity = 1.15,
        });
    }
}

void Simulation::updateEliteEffects(double deltaSeconds) {
    constexpr double Radius = 3.5;
    constexpr double EnemyDamage = 18.0;
    constexpr double PlayerDamage = 22.0;
    constexpr double BuildingDamage = 14.0;
    bool worldStructuresDirty = false;
    bool modularStructuresDirty = false;

    for (PendingEliteExplosion& pending :
         pendingEliteExplosions_) {
        pending.remaining -= deltaSeconds;
        if (pending.remaining > 0.0) {
            continue;
        }

        int killedCount = 0;
        for (const EnemyDamageResult& hit :
             enemies_.damageInRadius(
                 pending.position, Radius, EnemyDamage,
                 2.2, pending.position, 0.0,
                 pending.sourceId)) {
            events_.push_back({
                .type = GameEventType::ProjectileHit,
                .entityId = hit.id,
                .sourceId = pending.sourceId,
                .position = hit.position,
                .damage = hit.damage,
            });
            if (hit.killed) {
                ++killedCount;
                events_.push_back({
                    .type = GameEventType::EnemyKilled,
                    .entityId = hit.id,
                    .sourceId = pending.sourceId,
                    .position = hit.position,
                });
            }
        }

        const double playerDistance = std::hypot(
            playerPosition_.x - pending.position.x,
            playerPosition_.z - pending.position.z);
        if (playerDistance < Radius && !playerRespawning_) {
            const double falloff =
                1.0 - playerDistance / Radius;
            damagePlayer(
                PlayerDamage * falloff,
                pending.sourceId, pending.position);
        }

        std::vector<std::pair<EntityId, Vec3>> buildingTargets;
        for (const BuildingInstance& building :
             buildings_.buildings()) {
            const Vec3 center = buildingWorldPosition(building);
            if (std::hypot(
                    center.x - pending.position.x,
                    center.z - pending.position.z) < Radius) {
                buildingTargets.emplace_back(building.id, center);
            }
        }
        for (const auto& [id, center] : buildingTargets) {
            const auto result =
                buildings_.damage(id, BuildingDamage);
            if (!result) {
                continue;
            }
            events_.push_back({
                .type = GameEventType::BuildingDamaged,
                .entityId = result->id,
                .sourceId = pending.sourceId,
                .buildingType = result->type,
                .position = center,
                .amount = static_cast<int>(BuildingDamage),
            });
            if (result->type == BuildingType::Core) {
                events_.push_back({
                    .type = GameEventType::CoreDamaged,
                    .entityId = result->id,
                    .sourceId = pending.sourceId,
                    .buildingType = result->type,
                    .position = center,
                    .amount = static_cast<int>(BuildingDamage),
                });
            }
            if (result->destroyed) {
                events_.push_back({
                    .type = GameEventType::BuildingDestroyed,
                    .entityId = result->id,
                    .buildingType = result->type,
                    .position = center,
                });
                worldStructuresDirty = true;
                if (result->type == BuildingType::Core) {
                    state_ = RunState::Defeat;
                    events_.push_back({
                        .type = GameEventType::RunEnded,
                    });
                }
            }
        }

        std::vector<EnemyStructureTarget> modularTargets;
        buildModularEnemyTargets(
            foundations_, worldConfig_, modularTargets);
        std::vector<std::pair<EntityId, Vec3>> nearbyModularTargets;
        nearbyModularTargets.reserve(modularTargets.size());
        for (const EnemyStructureTarget& target : modularTargets) {
            if (std::hypot(
                    target.position.x - pending.position.x,
                    target.position.z - pending.position.z) < Radius) {
                nearbyModularTargets.emplace_back(
                    target.id, target.position);
            }
        }
        for (const auto& [id, center] : nearbyModularTargets) {
            const auto result =
                foundations_.damage(id, BuildingDamage);
            if (!result) {
                continue;
            }
            Vec3 effectCenter = center;
            if (result->wall) {
                effectCenter.y = result->wall->bottomHeight;
            } else if (result->ramp) {
                effectCenter.y = result->ramp->bottomHeight;
            }
            events_.push_back({
                .type = GameEventType::ModularBuildingDamaged,
                .entityId = result->id,
                .sourceId = pending.sourceId,
                .platformFrame = result->platformFrame,
                .modularWall = result->wall,
                .ramp = result->ramp,
                .position = effectCenter,
                .amount = static_cast<int>(BuildingDamage),
            });
            if (result->destroyed) {
                events_.push_back({
                    .type = GameEventType::ModularBuildingDestroyed,
                    .entityId = result->id,
                    .sourceId = pending.sourceId,
                    .platformFrame = result->platformFrame,
                    .modularWall = result->wall,
                    .ramp = result->ramp,
                    .position = effectCenter,
                });
                modularStructuresDirty = true;
            }
        }
        events_.push_back({
            .type = GameEventType::Explosion,
            .entityId = pending.sourceId,
            .sourceId = pending.sourceId,
            .position = pending.position,
            .amount = killedCount,
            .damage = EnemyDamage,
            .intensity = Radius,
        });
    }
    std::erase_if(
        pendingEliteExplosions_,
        [](const PendingEliteExplosion& pending) {
            return pending.remaining <= 0.0;
        });
    if (worldStructuresDirty) {
        syncWorldStructures();
    }
    if (modularStructuresDirty) {
        syncModularStructures();
        removeUnsupportedPlatformBuildings();
    }
}

} // namespace ian
