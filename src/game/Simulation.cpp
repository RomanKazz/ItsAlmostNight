#include "game/Simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace ian {
namespace {

constexpr double PitchLimit = 1.5533430342749532;
constexpr double DebugSpawnMinimumRadius = 7.0;
constexpr double DebugSpawnMaximumRadius = 12.0;
constexpr double DebugSpawnCollisionRadius = 0.6;

double clampAxis(double value) {
    return std::clamp(value, -1.0, 1.0);
}

Vec3 lookDirection(double yaw, double pitch) {
    const double cosPitch = std::cos(pitch);
    return {
        std::sin(yaw) * cosPitch,
        std::sin(pitch),
        -std::cos(yaw) * cosPitch,
    };
}

std::uint64_t mixBits(std::uint64_t value) {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

double randomUnit(std::uint64_t seed) {
    constexpr double Scale = 1.0 / 9007199254740992.0;
    return static_cast<double>(mixBits(seed) >> 11U) * Scale;
}

double enemyHeight(EnemyType type) {
    switch (type) {
    case EnemyType::Fast:
        return 0.675;
    case EnemyType::Heavy:
        return 1.0;
    case EnemyType::Boss:
        return 1.6;
    case EnemyType::Basic:
        return 0.8;
    }
    return 0.8;
}

AttackDirection attackDirection(Vec3 anchor, GridPosition corePosition) {
    const double deltaX = anchor.x - static_cast<double>(corePosition.x);
    const double deltaZ = anchor.z - static_cast<double>(corePosition.z);
    if (std::abs(deltaX) > std::abs(deltaZ)) {
        return deltaX >= 0.0 ? AttackDirection::East : AttackDirection::West;
    }
    return deltaZ >= 0.0 ? AttackDirection::South : AttackDirection::North;
}

} // namespace

Simulation::Simulation(GameBalance balance, MapDefinition map)
    : map_(std::move(map)), resources_(map_.resources),
      buildings_(balance.buildings, balance.economy, map_.coreBuildRadius),
      collisionWorld_(map_.worldLimit, mapCollisionBoxes(map_)),
      flowField_(mapCollisionBoxes(map_)),
      enemies_(balance.enemies),
      playerWeapons_(balance.weapons.rifle), bombs_(balance.weapons.bomb),
      goldMines_(balance.economy),
      waveDirector_(balance.waves, map_.enemySpawnAnchors),
      economy_(balance.economy), gameplay_(balance.gameplay) {
    playerPosition_ = {map_.playerSpawn.x, map_.playerSpawn.y + gameplay_.eyeHeight,
                       map_.playerSpawn.z};
    playerHealth_ = gameplay_.playerMaxHealth;
    waveSpawnQueue_.reserve(200);
}

void Simulation::startRun() {
    if (state_ != RunState::MainMenu) {
        return;
    }

    state_ = RunState::Gathering;
    tick_ = 0;
    elapsedSeconds_ = 0.0;
    playerPosition_ = {map_.playerSpawn.x, map_.playerSpawn.y + gameplay_.eyeHeight,
                       map_.playerSpawn.z};
    verticalVelocity_ = 0.0;
    playerYaw_ = 0.0;
    playerPitch_ = 0.0;
    playerGrounded_ = true;
    playerHealth_ = gameplay_.playerMaxHealth;
    wood_ = 0;
    stone_ = 0;
    gold_ = 0;
    unlimitedResources_ = false;
    playerInvulnerable_ = false;
    debugSpawnSequence_ = 0;
    pickaxeCooldownRemaining_ = 0.0;
    aimedResource_.reset();
    resources_.reset();
    selectedBuilding_.reset();
    buildingRotation_ = 0;
    buildingPreview_.reset();
    buildings_.reset();
    collisionWorld_.reset();
    flowField_.reset();
    flowDebugVectors_.clear();
    aimedEnemy_.reset();
    aimedBuilding_.reset();
    enemies_.reset();
    towers_.reset();
    cannons_.reset();
    traps_.reset();
    playerWeapons_.reset();
    bombs_.reset();
    goldMines_.reset();
    phaseTimeRemaining_ = 0.0;
    phaseDuration_ = 0.0;
    wave_ = 0;
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0;
    waveSpawnTimeRemaining_ = 0.0;
    upcomingAttackDirection_.reset();
    events_.push_back({.type = GameEventType::RunStarted});
}

void Simulation::restartRun() {
    state_ = RunState::Gathering;
    tick_ = 0;
    elapsedSeconds_ = 0.0;
    playerPosition_ = {map_.playerSpawn.x, map_.playerSpawn.y + gameplay_.eyeHeight,
                       map_.playerSpawn.z};
    verticalVelocity_ = 0.0;
    playerYaw_ = 0.0;
    playerPitch_ = 0.0;
    playerGrounded_ = true;
    playerHealth_ = gameplay_.playerMaxHealth;
    wood_ = 0;
    stone_ = 0;
    gold_ = 0;
    unlimitedResources_ = false;
    playerInvulnerable_ = false;
    debugSpawnSequence_ = 0;
    pickaxeCooldownRemaining_ = 0.0;
    aimedResource_.reset();
    resources_.reset();
    selectedBuilding_.reset();
    buildingRotation_ = 0;
    buildingPreview_.reset();
    buildings_.reset();
    collisionWorld_.reset();
    flowField_.reset();
    flowDebugVectors_.clear();
    aimedEnemy_.reset();
    aimedBuilding_.reset();
    enemies_.reset();
    towers_.reset();
    cannons_.reset();
    traps_.reset();
    playerWeapons_.reset();
    bombs_.reset();
    goldMines_.reset();
    phaseTimeRemaining_ = 0.0;
    phaseDuration_ = 0.0;
    wave_ = 0;
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0;
    waveSpawnTimeRemaining_ = 0.0;
    upcomingAttackDirection_.reset();
    events_.push_back({.type = GameEventType::RunRestarted});
}

void Simulation::togglePause() {
    if (state_ == RunState::MainMenu) {
        return;
    }

    if (state_ == RunState::Paused) {
        state_ = stateBeforePause_;
    } else {
        stateBeforePause_ = state_;
        state_ = RunState::Paused;
    }
    events_.push_back({.type = GameEventType::PauseChanged});
}

void Simulation::tick(double deltaSeconds, const PlayerCommand& command) {
    if (state_ == RunState::MainMenu || state_ == RunState::Paused ||
        state_ == RunState::Victory || state_ == RunState::Defeat) {
        return;
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
    const double speed = command.sprint ? gameplay_.sprintSpeed : gameplay_.walkSpeed;

    playerPosition_ = collisionWorld_.moveCircle(
        playerPosition_, {directionX * speed * deltaSeconds, 0.0,
                          directionZ * speed * deltaSeconds},
        CollisionWorld::PlayerRadius);

    if (command.jump && playerGrounded_) {
        verticalVelocity_ = gameplay_.jumpSpeed;
        playerGrounded_ = false;
    }

    if (!playerGrounded_) {
        verticalVelocity_ -= gameplay_.gravity * deltaSeconds;
        playerPosition_.y += verticalVelocity_ * deltaSeconds;
        const double standingHeight = map_.playerSpawn.y + gameplay_.eyeHeight;
        if (playerPosition_.y <= standingHeight) {
            playerPosition_.y = standingHeight;
            verticalVelocity_ = 0.0;
            playerGrounded_ = true;
        }
    }

    resources_.tick(deltaSeconds);
    pickaxeCooldownRemaining_ = std::max(0.0, pickaxeCooldownRemaining_ - deltaSeconds);
    playerWeapons_.tick(deltaSeconds);

    if (command.enableUnlimitedResources) {
        unlimitedResources_ = true;
    }
    if (command.toggleInvulnerability) {
        playerInvulnerable_ = !playerInvulnerable_;
    }
    if (command.damageCore) {
        const auto core = buildings_.core();
        if (core) {
            const auto damage =
                buildings_.damage(core->id, command.damageCore->amount);
            if (damage) {
                events_.push_back({
                    .type = GameEventType::CoreDamaged,
                    .entityId = damage->id,
                    .buildingType = damage->type,
                    .position = {static_cast<double>(damage->gridPosition.x), 0.0,
                                 static_cast<double>(damage->gridPosition.z)},
                    .amount = static_cast<int>(command.damageCore->amount),
                });
                if (damage->destroyed) {
                    syncWorldStructures();
                    state_ = RunState::Defeat;
                    events_.push_back({
                        .type = GameEventType::BuildingDestroyed,
                        .entityId = damage->id,
                        .buildingType = damage->type,
                        .position = {
                            static_cast<double>(damage->gridPosition.x),
                            0.0,
                            static_cast<double>(damage->gridPosition.z),
                        },
                    });
                    events_.push_back({.type = GameEventType::RunEnded});
                }
            }
        }
    }
    if (command.spawnEnemy) {
        const auto core = buildings_.core();
        if (core) {
            constexpr double TwoPi = 6.28318530717958647692;
            const std::uint64_t sequence = debugSpawnSequence_++;
            for (std::uint64_t attempt = 0; attempt < 12; ++attempt) {
                const std::uint64_t seed =
                    mixBits(tick_ ^ (sequence * 0x9e3779b97f4a7c15ULL) ^
                            attempt);
                const double angle = randomUnit(seed) * TwoPi;
                const double radius =
                    DebugSpawnMinimumRadius +
                    randomUnit(seed ^ 0xd1b54a32d192ed03ULL) *
                        (DebugSpawnMaximumRadius - DebugSpawnMinimumRadius);
                const Vec3 position{
                    static_cast<double>(core->gridPosition.x) +
                        std::cos(angle) * radius,
                    enemyHeight(command.spawnEnemy->type),
                    static_cast<double>(core->gridPosition.z) +
                        std::sin(angle) * radius,
                };
                if (std::abs(position.x) >
                        map_.worldLimit - DebugSpawnCollisionRadius ||
                    std::abs(position.z) >
                        map_.worldLimit - DebugSpawnCollisionRadius) {
                    continue;
                }
                const bool blocked = std::any_of(
                    collisionWorld_.colliders().begin(),
                    collisionWorld_.colliders().end(),
                    [this, position](const CollisionBox& collider) {
                        return collisionWorld_.overlapsCircle(
                            position, DebugSpawnCollisionRadius, collider);
                    });
                if (blocked) {
                    continue;
                }
                const EnemySpawn spawn{
                    .type = command.spawnEnemy->type,
                    .position = position,
                };
                enemies_.spawnGroup(
                    std::span<const EnemySpawn>{&spawn, 1});
                break;
            }
        }
    }
    if (command.toggleWeapon) {
        playerWeapons_.toggleWeapon();
        selectedBuilding_.reset();
        buildingPreview_.reset();
    }
    if (command.upgradeWeapon) {
        const auto core = buildings_.core();
        const int coreLevel = core ? static_cast<int>(core->level) : 0;
        const int availableGold =
            unlimitedResources_ ? std::numeric_limits<int>::max() : gold_;
        const WeaponUpgradeResult result = playerWeapons_.upgrade(coreLevel, availableGold);
        if (result.valid()) {
            if (!unlimitedResources_) {
                gold_ -= result.goldCost;
            }
            events_.push_back({
                .type = GameEventType::WeaponUpgraded,
                .amount = result.level,
            });
        } else {
            events_.push_back({
                .type = GameEventType::WeaponUpgradeRejected,
                .weaponUpgradeError = result.error,
            });
        }
    }
    if (command.defeatAllEnemies && enemies_.activeCount() > 0) {
        if (state_ == RunState::Wave) {
            nextWaveSpawnIndex_ = waveSpawnQueue_.size();
        }
        enemies_.defeatAll();
    }

    if (command.selectBuilding) {
        selectedBuilding_ = command.selectBuilding;
    }
    if (command.cancelBuilding) {
        selectedBuilding_.reset();
    }
    if (selectedBuilding_ && command.rotateBuilding != 0) {
        const int rotation = static_cast<int>(buildingRotation_) + command.rotateBuilding;
        buildingRotation_ = static_cast<std::uint8_t>((rotation % 4 + 4) % 4);
    }

    if (selectedBuilding_) {
        const GridPosition gridPosition =
            aimedBuildingGridPosition(playerPosition_, playerYaw_, playerPitch_,
                                      gameplay_.minimumPlacementDistance,
                                      gameplay_.maximumPlacementDistance);
        buildingPreview_ = BuildingPreview{
            .type = *selectedBuilding_,
            .gridPosition = gridPosition,
            .rotation = buildingRotation_,
            .placement = validatePlacement(*selectedBuilding_, gridPosition),
        };
    } else {
        buildingPreview_.reset();
    }

    if (command.placeBuilding) {
        const PlacementResult placement =
            validatePlacement(command.placeBuilding->type, command.placeBuilding->gridPosition);
        if (placement.valid()) {
            const auto placed = buildings_.place(
                command.placeBuilding->type, command.placeBuilding->gridPosition,
                command.placeBuilding->rotation,
                unlimitedResources_ ? std::numeric_limits<int>::max() : wood_,
                unlimitedResources_ ? std::numeric_limits<int>::max() : stone_,
                unlimitedResources_ ? std::numeric_limits<int>::max() : gold_);
            if (placed) {
                if (!unlimitedResources_) {
                    wood_ -= placed->cost.wood;
                    stone_ -= placed->cost.stone;
                    gold_ -= placed->cost.gold;
                }
                syncWorldStructures();
                events_.push_back({
                    .type = GameEventType::BuildingPlaced,
                    .entityId = placed->building.id,
                    .buildingType = placed->building.type,
                    .position = {static_cast<double>(placed->building.gridPosition.x), 0.0,
                                 static_cast<double>(placed->building.gridPosition.z)},
                });
                if (placed->building.type == BuildingType::Core) {
                    selectedBuilding_.reset();
                    buildingPreview_.reset();
                    state_ = RunState::BuildPhase;
                    phaseTimeRemaining_ = gameplay_.firstBuildPhaseSeconds;
                    phaseDuration_ = phaseTimeRemaining_;
                }
            }
        } else {
            events_.push_back({
                .type = GameEventType::BuildingRejected,
                .buildingType = command.placeBuilding->type,
                .placementError = placement.error,
                .position = {static_cast<double>(command.placeBuilding->gridPosition.x), 0.0,
                             static_cast<double>(command.placeBuilding->gridPosition.z)},
            });
        }
    }

    if (command.upgradeBuilding) {
        const int availableWood =
            unlimitedResources_ ? std::numeric_limits<int>::max() : wood_;
        const int availableStone =
            unlimitedResources_ ? std::numeric_limits<int>::max() : stone_;
        const int availableGold =
            unlimitedResources_ ? std::numeric_limits<int>::max() : gold_;
        const UpgradeResult result =
            buildings_.upgrade(command.upgradeBuilding->buildingId, availableWood, availableStone,
                               availableGold);
        if (result.valid() && result.building) {
            if (!unlimitedResources_) {
                wood_ -= result.cost.wood;
                stone_ -= result.cost.stone;
                gold_ -= result.cost.gold;
            }
            syncWorldStructures();
            events_.push_back({
                .type = GameEventType::BuildingUpgraded,
                .entityId = result.building->id,
                .buildingType = result.building->type,
            });
        } else {
            events_.push_back({
                .type = GameEventType::BuildingUpgradeRejected,
                .entityId = command.upgradeBuilding->buildingId,
                .upgradeError = result.error,
            });
        }
    }

    if (command.repairBuilding) {
        const int availableWood =
            unlimitedResources_ ? std::numeric_limits<int>::max() : wood_;
        const int availableStone =
            unlimitedResources_ ? std::numeric_limits<int>::max() : stone_;
        const int availableGold =
            unlimitedResources_ ? std::numeric_limits<int>::max() : gold_;
        const RepairResult result =
            buildings_.repair(command.repairBuilding->buildingId, availableWood, availableStone,
                              availableGold);
        if (result.valid() && result.building) {
            if (!unlimitedResources_) {
                wood_ -= result.cost.wood;
                stone_ -= result.cost.stone;
                gold_ -= result.cost.gold;
            }
            events_.push_back({
                .type = GameEventType::BuildingRepaired,
                .entityId = result.building->id,
                .buildingType = result.building->type,
                .amount = static_cast<int>(result.repairedHealth),
            });
        } else {
            events_.push_back({
                .type = GameEventType::BuildingRepairRejected,
                .entityId = command.repairBuilding->buildingId,
                .buildingActionError = result.error,
            });
        }
    }

    if (command.sellBuilding) {
        const SellResult result = buildings_.sell(command.sellBuilding->buildingId);
        if (result.valid() && result.building) {
            if (!unlimitedResources_) {
                wood_ += result.refund.wood;
                stone_ += result.refund.stone;
                gold_ += result.refund.gold;
            }
            aimedBuilding_.reset();
            syncWorldStructures();
            events_.push_back({
                .type = GameEventType::BuildingSold,
                .entityId = result.building->id,
                .buildingType = result.building->type,
                .position = {static_cast<double>(result.building->gridPosition.x), 0.0,
                             static_cast<double>(result.building->gridPosition.z)},
            });
        } else {
            events_.push_back({
                .type = GameEventType::BuildingSellRejected,
                .entityId = command.sellBuilding->buildingId,
                .buildingActionError = result.error,
            });
        }
    }

    if (command.toggleGate) {
        const auto gate =
            std::find_if(buildings_.buildings().begin(), buildings_.buildings().end(),
                         [&command](const BuildingInstance& building) {
                             return building.id == command.toggleGate->gateId &&
                                    building.type == BuildingType::Gate;
                         });
        bool rejected = gate == buildings_.buildings().end();
        if (!rejected && gate->open) {
            const CollisionBox gateBox =
                buildingCollisionBox(gate->type, gate->gridPosition);
            rejected = collisionWorld_.overlapsCircle(
                playerPosition_, CollisionWorld::PlayerRadius, gateBox);
        }
        if (rejected) {
            events_.push_back({
                .type = GameEventType::GateToggleRejected,
                .entityId = command.toggleGate->gateId,
            });
        } else {
            const auto toggled = buildings_.toggleGate(command.toggleGate->gateId);
            if (toggled) {
                syncWorldStructures();
                events_.push_back({
                    .type = GameEventType::GateToggled,
                    .entityId = toggled->id,
                    .buildingType = toggled->type,
                    .position = {static_cast<double>(toggled->gridPosition.x), 0.0,
                                 static_cast<double>(toggled->gridPosition.z)},
                    .amount = toggled->open ? 1 : 0,
                });
            }
        }
    }

    const auto production = goldMines_.tick(deltaSeconds);
    for (const auto& produced : production) {
        gold_ += produced.amount;
        events_.push_back({
            .type = GameEventType::GoldProduced,
            .entityId = produced.mineId,
            .amount = produced.amount,
        });
    }

    const Vec3 direction = lookDirection(playerYaw_, playerPitch_);
    aimedBuilding_ = buildings_.raycast(playerPosition_, direction, 4.0);
    aimedResource_ = playerWeapons_.selectedWeapon() == PlayerWeapon::Pickaxe
                         ? resources_.raycast(playerPosition_, direction, gameplay_.pickaxeRange)
                         : std::nullopt;
    const double enemyAimRange = playerWeapons_.selectedWeapon() == PlayerWeapon::Rifle
                                     ? playerWeapons_.rifleRange()
                                     : gameplay_.pickaxeRange;
    aimedEnemy_ = enemies_.raycast(playerPosition_, direction, enemyAimRange);
    if (command.useConsumable && bombs_.throwBomb(playerPosition_, direction)) {
        events_.push_back({
            .type = GameEventType::ConsumableUsed,
            .position = playerPosition_,
        });
    }
    if (command.fireRifle && !selectedBuilding_) {
        const auto fire = playerWeapons_.fireRifle(playerPosition_, direction, enemies_);
        if (fire) {
            events_.push_back({
                .type = GameEventType::WeaponFired,
                .position = fire->hitPosition,
            });
            if (fire->targetId) {
                events_.push_back({
                    .type = GameEventType::ProjectileHit,
                    .entityId = fire->targetId,
                    .position = fire->hitPosition,
                });
                if (fire->killed) {
                    events_.push_back({
                        .type = GameEventType::EnemyKilled,
                        .entityId = fire->targetId,
                        .position = fire->hitPosition,
                    });
                    aimedEnemy_.reset();
                }
            }
        }
    }
    if (command.usePickaxe && playerWeapons_.selectedWeapon() == PlayerWeapon::Pickaxe &&
        !selectedBuilding_ && pickaxeCooldownRemaining_ <= 0.0) {
        pickaxeCooldownRemaining_ = gameplay_.pickaxeCooldown;
        if (aimedEnemy_) {
            const auto damage = enemies_.damage(*aimedEnemy_, gameplay_.pickaxeDamage);
            if (damage && damage->killed) {
                events_.push_back({
                    .type = GameEventType::EnemyKilled,
                    .entityId = damage->id,
                    .position = damage->position,
                });
                aimedEnemy_.reset();
            }
        } else if (aimedResource_) {
            const auto hit = resources_.damage(*aimedResource_, gameplay_.pickaxeDamage);
            if (hit) {
                events_.push_back({
                    .type =
                        hit->collected ? GameEventType::ResourceCollected : GameEventType::ResourceHit,
                    .entityId = hit->nodeId,
                    .resourceType = hit->type,
                    .position = hit->position,
                    .amount = hit->amount,
                });
                if (hit->collected) {
                    if (hit->type == ResourceType::Wood) {
                        wood_ += hit->amount;
                    } else {
                        stone_ += hit->amount;
                    }
                    aimedResource_.reset();
                }
            }
        }
    }

    const auto bombExplosions = bombs_.tick(deltaSeconds, enemies_);
    for (const auto& explosion : bombExplosions) {
        events_.push_back({
            .type = GameEventType::Explosion,
            .entityId = explosion.projectileId,
            .position = explosion.position,
            .amount = explosion.killedCount,
        });
    }

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
                    waveDirector_.buildWave(wave_ + 1, core->gridPosition, firstAnchor);
                prepareWave(plan, core->gridPosition, firstAnchor);
                events_.push_back({
                    .type = GameEventType::AttackDirectionWarned,
                    .position = map_.enemySpawnAnchors[firstAnchor],
                    .amount = wave_ + 1,
                });
            }
            events_.push_back({
                .type = GameEventType::SunsetStarted,
                .amount = wave_ + 1,
            });
        }
    } else if (state_ == RunState::Sunset) {
        phaseTimeRemaining_ = std::max(0.0, phaseTimeRemaining_ - deltaSeconds);
        if (phaseTimeRemaining_ <= 0.0) {
            const auto core = buildings_.core();
            if (core) {
                ++wave_;
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

    if (state_ == RunState::Wave || enemies_.activeCount() > 0) {
        if (state_ == RunState::Wave) {
            tickWaveSpawning(deltaSeconds);
        }
        const auto activations =
            traps_.tick(deltaSeconds, buildings_.buildings(), enemies_);
        for (const auto& activation : activations) {
            events_.push_back({
                .type = GameEventType::TrapActivated,
                .entityId = activation.trapId,
                .position = activation.position,
                .amount = activation.affectedCount,
            });
            const auto wear = buildings_.damage(activation.trapId, activation.wearDamage);
            if (wear && wear->destroyed) {
                events_.push_back({
                    .type = GameEventType::BuildingDestroyed,
                    .entityId = wear->id,
                    .buildingType = wear->type,
                    .position = {static_cast<double>(wear->gridPosition.x), 0.0,
                                 static_cast<double>(wear->gridPosition.z)},
                });
                syncWorldStructures();
            }
        }

        const auto attacks =
            enemies_.tick(deltaSeconds, buildings_.buildings(), flowField_, playerPosition_);
        for (const auto& attack : enemies_.playerAttacks()) {
            const auto attacker = enemies_.enemy(attack.enemyId);
            const Vec3 attackPosition =
                attacker ? attacker->position : playerPosition_;
            if (playerInvulnerable_) {
                continue;
            }
            playerHealth_ = std::max(0.0, playerHealth_ - attack.damage);
            events_.push_back({
                .type = GameEventType::PlayerDamaged,
                .sourceId = attack.enemyId,
                .position = attackPosition,
                .amount = static_cast<int>(attack.damage),
            });
            if (playerHealth_ <= 0.0) {
                events_.push_back({
                    .type = GameEventType::PlayerDied,
                    .sourceId = attack.enemyId,
                    .position = playerPosition_,
                });
                respawnPlayer();
                events_.push_back({
                    .type = GameEventType::PlayerRespawned,
                    .position = playerPosition_,
                });
                break;
            }
        }
        for (const auto& attack : attacks) {
            const auto attacker = enemies_.enemy(attack.enemyId);
            const auto damage = buildings_.damage(attack.targetId, attack.damage);
            if (!damage) {
                continue;
            }
            const Vec3 attackPosition =
                attacker ? attacker->position
                         : Vec3{static_cast<double>(damage->gridPosition.x), 0.0,
                                static_cast<double>(damage->gridPosition.z)};
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
                    .position = {static_cast<double>(damage->gridPosition.x), 0.0,
                                 static_cast<double>(damage->gridPosition.z)},
                });
                syncWorldStructures();
                if (damage->type == BuildingType::Core) {
                    cannons_.clearProjectiles();
                    bombs_.clearProjectiles();
                    state_ = RunState::Defeat;
                    events_.push_back({.type = GameEventType::RunEnded});
                    break;
                }
            }
        }

        if (state_ != RunState::Defeat &&
            enemies_.activeCount() > 0) {
            const auto shots = towers_.tick(deltaSeconds, buildings_.buildings(), enemies_);
            for (const auto& shot : shots) {
                events_.push_back({
                    .type = GameEventType::ProjectileHit,
                    .entityId = shot.targetId,
                    .sourceId = shot.towerId,
                    .position = shot.hitPosition,
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

        if (state_ != RunState::Defeat &&
            (state_ == RunState::Wave ||
             enemies_.activeCount() > 0)) {
            const auto explosions =
                cannons_.tick(deltaSeconds, buildings_.buildings(), enemies_);
            for (const auto& explosion : explosions) {
                events_.push_back({
                    .type = GameEventType::Explosion,
                    .entityId = explosion.projectileId,
                    .position = explosion.position,
                    .amount = explosion.killedCount,
                });
            }
            if (state_ == RunState::Wave &&
                enemies_.activeCount() == 0 &&
                nextWaveSpawnIndex_ >= waveSpawnQueue_.size()) {
                completeWave();
            }
        }
    }

    ++tick_;
    elapsedSeconds_ += deltaSeconds;
}

PlacementResult Simulation::validatePlacement(BuildingType type, GridPosition position) const {
    const int availableWood =
        unlimitedResources_ ? std::numeric_limits<int>::max() : wood_;
    const int availableStone =
        unlimitedResources_ ? std::numeric_limits<int>::max() : stone_;
    const int availableGold =
        unlimitedResources_ ? std::numeric_limits<int>::max() : gold_;
    const PlacementResult buildingValidation =
        buildings_.validate(type, position, availableWood, availableStone, availableGold);
    if (!buildingValidation.valid()) {
        return buildingValidation;
    }

    const double deltaX = static_cast<double>(position.x) - playerPosition_.x;
    const double deltaZ = static_cast<double>(position.z) - playerPosition_.z;
    const double distance = std::sqrt((deltaX * deltaX) + (deltaZ * deltaZ));
    if (distance > gameplay_.maximumPlacementDistance + 0.75) {
        return {PlacementError::OutOfRange, buildingValidation.cost};
    }

    const CollisionBox candidate = buildingCollisionBox(type, position);
    if (collisionWorld_.overlapsCircle(playerPosition_, CollisionWorld::PlayerRadius, candidate)) {
        return {PlacementError::PlayerOverlap, buildingValidation.cost};
    }
    if (collisionWorld_.overlapsBox(candidate)) {
        return {PlacementError::WorldCollision, buildingValidation.cost};
    }
    return buildingValidation;
}

void Simulation::syncWorldStructures() {
    collisionWorld_.syncBuildings(buildings_.buildings());
    towers_.syncBuildings(buildings_.buildings());
    cannons_.syncBuildings(buildings_.buildings());
    traps_.syncBuildings(buildings_.buildings());
    goldMines_.syncBuildings(buildings_.buildings());
    const auto core = buildings_.core();
    if (core) {
        flowField_.rebuild(core->gridPosition, buildings_.buildings());
        flowDebugVectors_ = flowField_.debugVectors();
    } else {
        flowField_.reset();
        flowDebugVectors_.clear();
    }
}

void Simulation::respawnPlayer() {
    constexpr std::array<GridPosition, 4> RespawnOffsets{{
        {0, 3},
        {3, 0},
        {0, -3},
        {-3, 0},
    }};
    Vec3 respawn{map_.playerSpawn.x, map_.playerSpawn.y + gameplay_.eyeHeight,
                 map_.playerSpawn.z};
    const auto core = buildings_.core();
    if (core) {
        for (const GridPosition offset : RespawnOffsets) {
            const Vec3 candidate{
                static_cast<double>(core->gridPosition.x + offset.x),
                gameplay_.eyeHeight,
                static_cast<double>(core->gridPosition.z + offset.z),
            };
            const bool blocked = std::any_of(
                collisionWorld_.colliders().begin(), collisionWorld_.colliders().end(),
                [this, candidate](const CollisionBox& collider) {
                    return collisionWorld_.overlapsCircle(
                        candidate, CollisionWorld::PlayerRadius, collider);
                });
            if (!blocked) {
                respawn = candidate;
                break;
            }
        }
    }
    playerPosition_ = respawn;
    verticalVelocity_ = 0.0;
    playerGrounded_ = true;
    playerHealth_ = gameplay_.playerMaxHealth;
}

void Simulation::prepareWave(const WavePlan& plan, GridPosition corePosition,
                             std::size_t firstAnchorIndex) {
    waveSpawnQueue_.assign(plan.spawns.begin(), plan.spawns.end());
    waveSpawnGroupSize_ = plan.groupSize;
    waveSpawnInterval_ = plan.groupInterval;
    nextWaveSpawnIndex_ = 0;
    waveSpawnTimeRemaining_ = waveSpawnInterval_;
    upcomingAttackDirection_ =
        attackDirection(map_.enemySpawnAnchors[firstAnchorIndex], corePosition);
}

void Simulation::beginPreparedWave() {
    const std::size_t firstGroupSize =
        std::min(static_cast<std::size_t>(waveSpawnGroupSize_), waveSpawnQueue_.size());
    enemies_.spawnWave(
        std::span<const EnemySpawn>{waveSpawnQueue_.data(), firstGroupSize});
    nextWaveSpawnIndex_ = firstGroupSize;
}

void Simulation::tickWaveSpawning(double deltaSeconds) {
    if (nextWaveSpawnIndex_ >= waveSpawnQueue_.size()) {
        return;
    }
    waveSpawnTimeRemaining_ -= deltaSeconds;
    while (waveSpawnTimeRemaining_ <= 0.0 &&
           nextWaveSpawnIndex_ < waveSpawnQueue_.size()) {
        const std::size_t remaining = waveSpawnQueue_.size() - nextWaveSpawnIndex_;
        const std::size_t groupSize =
            std::min(static_cast<std::size_t>(waveSpawnGroupSize_), remaining);
        enemies_.spawnGroup(std::span<const EnemySpawn>{
            waveSpawnQueue_.data() + nextWaveSpawnIndex_, groupSize});
        nextWaveSpawnIndex_ += groupSize;
        waveSpawnTimeRemaining_ += waveSpawnInterval_;
    }
}

void Simulation::completeWave() {
    cannons_.clearProjectiles();
    bombs_.clearProjectiles();
    waveSpawnQueue_.clear();
    nextWaveSpawnIndex_ = 0;
    upcomingAttackDirection_.reset();
    events_.push_back({
        .type = GameEventType::WaveCompleted,
        .amount = wave_,
    });
    const int reward = economy_.waveRewardPerWave * wave_;
    gold_ += reward;
    events_.push_back({
        .type = GameEventType::WaveRewardGranted,
        .amount = reward,
    });
    if (wave_ >= WaveDirector::WaveCount) {
        state_ = RunState::Victory;
        phaseTimeRemaining_ = 0.0;
        phaseDuration_ = 0.0;
        events_.push_back({.type = GameEventType::RunEnded});
    } else {
        state_ = RunState::WaveComplete;
        phaseTimeRemaining_ = gameplay_.dawnSeconds;
        phaseDuration_ = phaseTimeRemaining_;
    }
}

std::optional<TutorialObjective> Simulation::tutorialObjective() const {
    const RunState effectiveState =
        state_ == RunState::Paused ? stateBeforePause_ : state_;
    if (effectiveState == RunState::MainMenu || effectiveState == RunState::Victory ||
        effectiveState == RunState::Defeat || effectiveState == RunState::WaveComplete ||
        wave_ > 1) {
        return std::nullopt;
    }
    if (wave_ == 1) {
        return effectiveState == RunState::Wave
                   ? std::optional<TutorialObjective>{TutorialObjective::SurviveFirstWave}
                   : std::nullopt;
    }
    if (!buildings_.hasCore()) {
        if (!unlimitedResources_ &&
            wood_ < buildings_.configuredCost(BuildingType::Core).wood) {
            return TutorialObjective::MineWood;
        }
        return TutorialObjective::PlaceCore;
    }
    const bool hasGoldMine =
        std::any_of(buildings_.buildings().begin(), buildings_.buildings().end(),
                    [](const BuildingInstance& building) {
                        return building.type == BuildingType::GoldMine;
                    });
    if (!hasGoldMine) {
        if (!unlimitedResources_ &&
            stone_ < buildings_.configuredCost(BuildingType::GoldMine).stone) {
            return TutorialObjective::MineStone;
        }
        return TutorialObjective::BuildGoldMine;
    }
    return TutorialObjective::PrepareForNight;
}

SimulationSnapshot Simulation::snapshot() const {
    const auto core = buildings_.core();
    std::optional<ResourceCost> aimedUpgradeCost;
    if (aimedBuilding_) {
        const auto aimed =
            std::find_if(buildings_.buildings().begin(), buildings_.buildings().end(),
                         [this](const BuildingInstance& building) {
                             return building.id == *aimedBuilding_;
                         });
        if (aimed != buildings_.buildings().end() && aimed->level < 3) {
            aimedUpgradeCost = buildings_.upgradeCost(*aimed);
        }
    }
    return {
        .state = state_,
        .tick = tick_,
        .elapsedSeconds = elapsedSeconds_,
        .playerPosition = playerPosition_,
        .playerYaw = playerYaw_,
        .playerPitch = playerPitch_,
        .playerGrounded = playerGrounded_,
        .playerHealth = playerHealth_,
        .playerMaxHealth = gameplay_.playerMaxHealth,
        .wood = wood_,
        .stone = stone_,
        .gold = gold_,
        .pickaxeCooldownRemaining = pickaxeCooldownRemaining_,
        .aimedResource = aimedResource_,
        .resourceNodes = std::span<const ResourceNode>{resources_.nodes()},
        .worldLimit = map_.worldLimit,
        .mapObstacles = std::span<const MapObstacle>{map_.obstacles},
        .collisionBoxes =
            std::span<const CollisionBox>{collisionWorld_.colliders()},
        .flowDebugVectors =
            std::span<const FlowDebugVector>{flowDebugVectors_},
        .selectedBuilding = selectedBuilding_,
        .buildingPreview = buildingPreview_,
        .buildings = std::span<const BuildingInstance>{buildings_.buildings()},
        .aimedEnemy = aimedEnemy_,
        .aimedBuilding = aimedBuilding_,
        .aimedBuildingUpgradeCost = aimedUpgradeCost,
        .enemies = std::span<const EnemyInstance>{enemies_.enemies()},
        .towers = std::span<const TowerRuntime>{towers_.towers()},
        .cannons = std::span<const CannonRuntime>{cannons_.cannons()},
        .cannonProjectiles =
            std::span<const CannonProjectile>{cannons_.projectiles()},
        .bombProjectiles = std::span<const BombProjectile>{bombs_.projectiles()},
        .activeEnemyCount = enemies_.activeCount(),
        .pendingEnemyCount = waveSpawnQueue_.size() - nextWaveSpawnIndex_,
        .upcomingAttackDirection = upcomingAttackDirection_,
        .phaseTimeRemaining = phaseTimeRemaining_,
        .phaseDuration = phaseDuration_,
        .wave = wave_,
        .coreHealth = core ? core->health : 0.0,
        .coreMaxHealth = core ? core->maxHealth : 0.0,
        .coreId = core ? std::optional<EntityId>{core->id} : std::nullopt,
        .coreLevel = core ? core->level : static_cast<std::uint8_t>(0),
        .unlimitedResources = unlimitedResources_,
        .playerInvulnerable = playerInvulnerable_,
        .selectedWeapon = playerWeapons_.selectedWeapon(),
        .rifleLevel = playerWeapons_.rifleLevel(),
        .rifleAmmunition = playerWeapons_.ammunition(),
        .rifleMagazineSize = playerWeapons_.magazineSize(),
        .rifleUpgradeGoldCost = playerWeapons_.upgradeGoldCost(),
        .rifleReloading = playerWeapons_.reloading(),
        .rifleReloadRemaining = playerWeapons_.reloadRemaining(),
        .bombsRemaining = bombs_.remainingBombs(),
        .waveCompletionReward = economy_.waveRewardPerWave * wave_,
        .tutorialWoodTarget = buildings_.configuredCost(BuildingType::Core).wood,
        .tutorialStoneTarget = buildings_.configuredCost(BuildingType::GoldMine).stone,
        .tutorialObjective = tutorialObjective(),
    };
}

std::vector<GameEvent> Simulation::takeEvents() {
    return std::exchange(events_, {});
}

} // namespace ian
