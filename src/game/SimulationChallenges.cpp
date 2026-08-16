#include "game/Simulation.hpp"

#include "game/ChallengeArena.hpp"

#include "core/DeterministicRandom.hpp"
#include "core/Geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ian {
namespace {

constexpr int ChallengeColumnCount = 5;
constexpr double ColumnInteractionDistance = 5.0;
constexpr double ColumnRaycastRadius = 1.15;
constexpr double ColumnMinimumSpawnDistance = 34.0;
constexpr double ColumnSeparation = 42.0;
constexpr double Tau = 6.28318530717958647692;

bool challengeActivationPhase(RunState state) {
    return state == RunState::Gathering ||
           state == RunState::BuildPhase;
}

bool obstacleNear(
    Vec3 position, double radius,
    std::span<const MapObstacle> obstacles) {
    return std::any_of(
        obstacles.begin(), obstacles.end(),
        [position, radius](const MapObstacle& obstacle) {
            const double dx = std::max(
                0.0, std::max(
                    obstacle.collision.minX - position.x,
                    position.x - obstacle.collision.maxX));
            const double dz = std::max(
                0.0, std::max(
                    obstacle.collision.minZ - position.z,
                    position.z - obstacle.collision.maxZ));
            return dx * dx + dz * dz < radius * radius;
        });
}

EliteAffixMask challengeEliteAffix(
    std::uint64_t seed) {
    constexpr std::array<EliteAffix, 3> Affixes{{
        EliteAffix::Berserker,
        EliteAffix::Warden,
        EliteAffix::Volatile,
    }};
    return eliteAffixMask(
        Affixes[mixBits64(seed) % Affixes.size()]);
}

double challengeEnemyCenterHeight(EnemyType type) {
    switch (type) {
    case EnemyType::Fast: return 0.675;
    case EnemyType::Heavy: return 1.0;
    case EnemyType::Ranged: return 0.85;
    case EnemyType::Sapper: return 0.78;
    case EnemyType::Flying: return 2.4;
    case EnemyType::Splitter: return 1.05;
    case EnemyType::Splitling: return 0.55;
    case EnemyType::Boss: return 1.6;
    case EnemyType::Basic: return 0.8;
    }
    return 0.8;
}

} // namespace

bool Simulation::challengeActive() const {
    return activeChallengeColumn_.has_value();
}

void Simulation::resetChallengeColumns() {
    ++challengeRunGeneration_;
    if (challengeRunGeneration_ == 0U) {
        ++challengeRunGeneration_;
    }
    challengeColumns_.clear();
    challengeColumns_.reserve(ChallengeColumnCount);
    activeChallengeColumn_.reset();
    aimedChallengeColumn_.reset();

    constexpr std::size_t MaximumAttempts = 8192;
    const double maximumRadius = std::max(
        ColumnMinimumSpawnDistance + 1.0,
        map_.worldLimit - challenge_arena::Radius - 5.0);
    for (std::size_t attempt = 0;
         attempt < MaximumAttempts &&
         challengeColumns_.size() < ChallengeColumnCount;
         ++attempt) {
        const std::uint64_t seed = mixBits64(
            static_cast<std::uint64_t>(terrain_.seed()) ^
            (attempt + 1U) * 0x9e3779b97f4a7c15ULL);
        const double angle = unitRandom(
            seed ^ 0x243f6a8885a308d3ULL) * Tau;
        const double radius = std::sqrt(
            ColumnMinimumSpawnDistance * ColumnMinimumSpawnDistance +
            unitRandom(seed ^ 0x13198a2e03707344ULL) *
                (maximumRadius * maximumRadius -
                 ColumnMinimumSpawnDistance * ColumnMinimumSpawnDistance));
        Vec3 position{
            map_.playerSpawn.x + std::cos(angle) * radius,
            0.0,
            map_.playerSpawn.z + std::sin(angle) * radius,
        };
        if (!terrain_.isInside(position.x, position.z) ||
            terrain_.waterSignedDistance(position.x, position.z) < 5.0 ||
            terrain_.getNormal(position.x, position.z).y < 0.88) {
            continue;
        }
        position.y = terrain_.getHeight(position.x, position.z);
        if (obstacleNear(position, 5.0, map_.obstacles) ||
            std::any_of(
                resources_.nodes().begin(), resources_.nodes().end(),
                [position](const ResourceNode& resource) {
                    return resource.active &&
                        geometry::distanceSquared(
                            position, resource.position) < 36.0;
                }) ||
            std::any_of(
                lootChests_.chests().begin(), lootChests_.chests().end(),
                [position](const LootChestInstance& chest) {
                    return !chest.looseLoot &&
                        geometry::distanceSquared(
                            position, chest.position) < 100.0;
                }) ||
            std::any_of(
                challengeColumns_.begin(), challengeColumns_.end(),
                [position](const ChallengeColumnInstance& column) {
                    return geometry::distanceSquared(
                        position, column.position) <
                        ColumnSeparation * ColumnSeparation;
                })) {
            continue;
        }
        challengeColumns_.push_back({
            .id = {
                static_cast<std::uint32_t>(900000U +
                    challengeColumns_.size()),
                challengeRunGeneration_,
            },
            .position = position,
            .yaw = unitRandom(seed ^ 0xa4093822299f31d0ULL) * Tau,
        });
    }
}

void Simulation::activateChallengeColumn(EntityId id) {
    if (challengeActive() || !challengeActivationPhase(state_)) {
        return;
    }
    const auto column = std::find_if(
        challengeColumns_.begin(), challengeColumns_.end(),
        [id](const ChallengeColumnInstance& candidate) {
            return candidate.id == id &&
                candidate.state == ChallengeColumnState::Dormant;
        });
    if (column == challengeColumns_.end()) {
        return;
    }
    const std::size_t columnIndex = static_cast<std::size_t>(
        std::distance(challengeColumns_.begin(), column));
    const int challengeWave = std::max(1, wave_ + 1);
    const WaveDefinition composition =
        waveDirector_.composition(challengeWave);
    int budget = std::max(
        8, static_cast<int>(std::ceil(
               static_cast<double>(composition.budget) * 0.55)));
    column->state = ChallengeColumnState::Active;
    column->fenceProgress = 0.0;
    column->enemyBudget = budget;
    activeChallengeColumn_ = columnIndex;
    selectedBuilding_.reset();
    buildingPreview_.reset();

    struct Candidate {
        EnemyType type;
        int count;
    };
    std::array<Candidate, 7> candidates{{
        {EnemyType::Basic, composition.basic},
        {EnemyType::Fast, composition.fast},
        {EnemyType::Heavy, composition.heavy},
        {EnemyType::Ranged, composition.ranged},
        {EnemyType::Sapper, composition.sapper},
        {EnemyType::Flying, composition.flying},
        {EnemyType::Splitter, composition.splitter},
    }};
    std::vector<EnemyType> pool;
    for (const Candidate candidate : candidates) {
        for (int count = 0; count < candidate.count; ++count) {
            pool.push_back(candidate.type);
        }
    }
    if (pool.empty()) {
        pool.push_back(EnemyType::Basic);
    }

    std::vector<EnemySpawn> spawns;
    spawns.reserve(static_cast<std::size_t>(budget));
    const std::uint64_t baseSeed = mixBits64(
        (static_cast<std::uint64_t>(terrain_.seed()) << 32U) ^
        id.index ^ static_cast<std::uint64_t>(challengeWave));
    int remainingBudget = budget;
    int eliteCount = 0;
    const int maximumElites = challengeWave < 3
        ? 0
        : 1 + std::max(0, challengeWave - 4) / 3;
    for (int iteration = 0;
         remainingBudget > 0 && iteration < 512;
         ++iteration) {
        const std::uint64_t seed = mixBits64(
            baseSeed + static_cast<std::uint64_t>(iteration) *
                0x9e3779b97f4a7c15ULL);
        EnemyType type = pool[seed % pool.size()];
        int cost = enemyBudgetCost(type);
        if (cost <= 0 || cost > remainingBudget) {
            type = EnemyType::Basic;
            cost = 1;
        }
        const double angle =
            Tau * static_cast<double>(iteration) /
                static_cast<double>(std::max(1, budget)) +
            (unitRandom(seed ^ 0x082efa98ec4e6c89ULL) - 0.5) * 0.42;
        const double spawnRadius =
            9.0 + unitRandom(seed ^ 0x452821e638d01377ULL) * 5.0;
        Vec3 position{
            column->position.x + std::cos(angle) * spawnRadius,
            challengeEnemyCenterHeight(type),
            column->position.z + std::sin(angle) * spawnRadius,
        };
        const bool elite = eliteCount < maximumElites &&
            unitRandom(seed ^ 0xbe5466cf34e90c6cULL) < 0.14;
        spawns.push_back({
            .type = type,
            .position = position,
            .healthMultiplier =
                1.0 + static_cast<double>(challengeWave - 1) * 0.08,
            .damageMultiplier =
                1.0 + static_cast<double>(challengeWave - 1) * 0.04,
            .eliteAffixes = elite
                ? challengeEliteAffix(seed)
                : EliteAffixMask{},
        });
        if (elite) {
            ++eliteCount;
        }
        remainingBudget -= cost;
    }
    enemies_.spawnGroup(spawns);
    aimedChallengeColumn_.reset();
}

void Simulation::failActiveChallenge() {
    if (!activeChallengeColumn_) {
        return;
    }
    ChallengeColumnInstance& column =
        challengeColumns_[*activeChallengeColumn_];
    column.state = ChallengeColumnState::Dormant;
    column.completionProgress = 0.0;
    column.enemyBudget = 0;
    activeChallengeColumn_.reset();
    aimedChallengeColumn_.reset();
    enemies_.spawnWave(std::span<const EnemySpawn>{});
}

void Simulation::updateChallengeColumns(
    double deltaSeconds, const PlayerCommand& command) {
    const Vec3 direction = lookDirection(playerYaw_, playerPitch_);
    aimedChallengeColumn_.reset();
    double nearest = ColumnInteractionDistance;
    if (!challengeActive() && challengeActivationPhase(state_)) {
        for (const ChallengeColumnInstance& column : challengeColumns_) {
            if (column.state != ChallengeColumnState::Dormant) {
                continue;
            }
            Vec3 center = column.position;
            center.y += 1.35;
            const auto distance = geometry::raySphereDistance(
                playerPosition_, direction, center,
                ColumnRaycastRadius);
            if (distance && *distance <= nearest) {
                nearest = *distance;
                aimedChallengeColumn_ = column.id;
            }
        }
    }
    if (command.interact && aimedChallengeColumn_) {
        activateChallengeColumn(*aimedChallengeColumn_);
    }

    for (ChallengeColumnInstance& column : challengeColumns_) {
        if (column.state == ChallengeColumnState::Active) {
            column.fenceProgress = std::min(
                1.0, column.fenceProgress + deltaSeconds / 1.05);
        }
        if (column.state == ChallengeColumnState::Dormant &&
            column.fenceProgress > 0.0) {
            column.fenceProgress = std::max(
                0.0, column.fenceProgress - deltaSeconds / 0.62);
        }
        if (column.state == ChallengeColumnState::Completed) {
            column.completionProgress = std::min(
                1.0,
                column.completionProgress + deltaSeconds / 0.48);
            column.fenceProgress = std::max(
                0.0, column.fenceProgress - deltaSeconds / 0.62);
        }
    }
    if (!activeChallengeColumn_) {
        return;
    }
    if (enemies_.activeCount() == 0U) {
        ChallengeColumnInstance& column =
            challengeColumns_[*activeChallengeColumn_];
        column.state = ChallengeColumnState::Completed;
        column.completionProgress = 0.0;
        lootChests_.spawnRewardChest(
            column.position, terrain_, LootChestType::Stone);
        activeChallengeColumn_.reset();
        enemies_.clearProjectiles();
    }
}

void Simulation::constrainPlayerToChallengeArena() {
    constexpr double ColumnCollisionRadius = 1.05;
    for (const ChallengeColumnInstance& column : challengeColumns_) {
        if (column.state == ChallengeColumnState::Completed) {
            continue;
        }
        const double columnDx = playerPosition_.x - column.position.x;
        const double columnDz = playerPosition_.z - column.position.z;
        const double columnDistance = std::hypot(columnDx, columnDz);
        if (columnDistance >= ColumnCollisionRadius ||
            columnDistance <= 1e-9) {
            continue;
        }
        const double nx = columnDx / columnDistance;
        const double nz = columnDz / columnDistance;
        playerPosition_.x = column.position.x + nx * ColumnCollisionRadius;
        playerPosition_.z = column.position.z + nz * ColumnCollisionRadius;
        const double inwardSpeed =
            playerHorizontalVelocity_.x * -nx +
            playerHorizontalVelocity_.z * -nz;
        if (inwardSpeed > 0.0) {
            playerHorizontalVelocity_.x += nx * inwardSpeed;
            playerHorizontalVelocity_.z += nz * inwardSpeed;
        }
    }
    if (!activeChallengeColumn_) {
        return;
    }
    const Vec3 center =
        challengeColumns_[*activeChallengeColumn_].position;
    constexpr double maximumDistance =
        challenge_arena::InteriorActorRadius;
    const double dx = playerPosition_.x - center.x;
    const double dz = playerPosition_.z - center.z;
    const double distance = std::hypot(dx, dz);
    if (distance <= maximumDistance || distance <= 1e-9) {
        return;
    }
    const double nx = dx / distance;
    const double nz = dz / distance;
    playerPosition_.x = center.x + nx * maximumDistance;
    playerPosition_.z = center.z + nz * maximumDistance;
    const double outwardSpeed =
        playerHorizontalVelocity_.x * nx +
        playerHorizontalVelocity_.z * nz;
    if (outwardSpeed > 0.0) {
        playerHorizontalVelocity_.x -= nx * outwardSpeed;
        playerHorizontalVelocity_.z -= nz * outwardSpeed;
    }
}

} // namespace ian
