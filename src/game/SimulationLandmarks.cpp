#include "game/Simulation.hpp"

#include "core/DeterministicRandom.hpp"
#include "core/Geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace ian {
namespace {

constexpr double Tau = 6.28318530717958647692;
constexpr double MinimumSpawnDistance = 38.0;
constexpr double LandmarkSeparation = 54.0;
constexpr double MaximumHeightVariation = 0.48;
constexpr double MinimumSurfaceNormalY = 0.92;
constexpr double LandmarkVisualScale = 1.30;
constexpr double LandmarkInteractionDistance = 8.0;
constexpr double LandmarkRaycastRadius = 3.5;

int landmarkActivationCost(WorldLandmarkType type) {
    return type == WorldLandmarkType::Mine ? 120 : 90;
}

int landmarkProductionAmount(WorldLandmarkType type) {
    return type == WorldLandmarkType::Mine ? 4 : 6;
}

double landmarkProductionInterval(WorldLandmarkType type) {
    return type == WorldLandmarkType::Mine ? 10.0 : 8.0;
}

double landmarkRadius(WorldLandmarkType type) {
    return (type == WorldLandmarkType::Mine ? 3.6 : 4.1) *
        LandmarkVisualScale;
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

bool flatLandmarkSurface(
    const TerrainHeightfield& terrain, Vec3 center,
    double footprintRadius) {
    double minimumHeight = std::numeric_limits<double>::infinity();
    double maximumHeight = -std::numeric_limits<double>::infinity();
    constexpr int SampleCount = 12;
    for (int sample = 0; sample <= SampleCount; ++sample) {
        const double angle = Tau * static_cast<double>(sample) /
            static_cast<double>(SampleCount);
        const double radius = sample == SampleCount
            ? 0.0 : footprintRadius;
        const double x = center.x + std::cos(angle) * radius;
        const double z = center.z + std::sin(angle) * radius;
        if (!terrain.isInside(x, z) ||
            terrain.getNormal(x, z).y < MinimumSurfaceNormalY) {
            return false;
        }
        const double height = terrain.getHeight(x, z);
        minimumHeight = std::min(minimumHeight, height);
        maximumHeight = std::max(maximumHeight, height);
    }
    return maximumHeight - minimumHeight <= MaximumHeightVariation;
}

} // namespace

void Simulation::resetWorldLandmarks() {
    aimedWorldLandmark_.reset();
    worldLandmarks_.clear();
    worldLandmarks_.reserve(2);
    constexpr std::array<WorldLandmarkType, 2> Types{{
        WorldLandmarkType::Mine,
        WorldLandmarkType::LumberMill,
    }};
    constexpr std::size_t MaximumAttemptsPerLandmark = 16384;
    const double maximumRadius = std::max(
        MinimumSpawnDistance + 1.0,
        map_.worldLimit - 8.0);
    for (std::size_t typeIndex = 0;
         typeIndex < Types.size(); ++typeIndex) {
        const WorldLandmarkType type = Types[typeIndex];
        const double footprintRadius = landmarkRadius(type);
        for (std::size_t attempt = 0;
             attempt < MaximumAttemptsPerLandmark; ++attempt) {
            const std::uint64_t seed = mixBits64(
                static_cast<std::uint64_t>(terrain_.seed()) ^
                (typeIndex + 1U) * 0xd1b54a32d192ed03ULL ^
                (attempt + 1U) * 0x9e3779b97f4a7c15ULL);
            const double angle = unitRandom(
                seed ^ 0x243f6a8885a308d3ULL) * Tau;
            const double radius = std::sqrt(
                MinimumSpawnDistance * MinimumSpawnDistance +
                unitRandom(seed ^ 0x13198a2e03707344ULL) *
                    (maximumRadius * maximumRadius -
                     MinimumSpawnDistance * MinimumSpawnDistance));
            Vec3 position{
                map_.playerSpawn.x + std::cos(angle) * radius,
                0.0,
                map_.playerSpawn.z + std::sin(angle) * radius,
            };
            if (!terrain_.isInside(position.x, position.z) ||
                terrain_.waterSignedDistance(
                    position.x, position.z) < footprintRadius + 3.0 ||
                !flatLandmarkSurface(
                    terrain_, position, footprintRadius)) {
                continue;
            }
            position.y = terrain_.getHeight(
                position.x, position.z);
            if (obstacleNear(
                    position, footprintRadius + 1.0,
                    map_.obstacles) ||
                std::any_of(
                    resources_.nodes().begin(),
                    resources_.nodes().end(),
                    [position, footprintRadius](
                        const ResourceNode& resource) {
                        const double clearance =
                            footprintRadius +
                            std::max(0.5, resource.radius);
                        return geometry::distanceSquared(
                                   position, resource.position) <
                            clearance * clearance;
                    }) ||
                std::any_of(
                    lootChests_.chests().begin(),
                    lootChests_.chests().end(),
                    [position, footprintRadius](
                        const LootChestInstance& chest) {
                        const double clearance =
                            footprintRadius + 4.0;
                        return !chest.looseLoot &&
                            geometry::distanceSquared(
                                position, chest.position) <
                            clearance * clearance;
                    }) ||
                std::any_of(
                    worldLandmarks_.begin(),
                    worldLandmarks_.end(),
                    [position](
                        const WorldLandmarkInstance& landmark) {
                        return geometry::distanceSquared(
                                   position, landmark.position) <
                            LandmarkSeparation * LandmarkSeparation;
                    })) {
                continue;
            }
            worldLandmarks_.push_back({
                .id = {
                    static_cast<std::uint32_t>(
                        910000U + typeIndex),
                    1U,
                },
                .type = type,
                .position = position,
                .yaw = unitRandom(
                    seed ^ 0xa4093822299f31d0ULL) * Tau,
                .collisionRadius = footprintRadius,
                .activationCoinCost = landmarkActivationCost(type),
            });
            break;
        }
    }
    syncWorldLandmarkColliders();
}

void Simulation::syncWorldLandmarkColliders() {
    std::vector<CollisionBox> colliders;
    colliders.reserve(worldLandmarks_.size() * 3U);
    for (const WorldLandmarkInstance& landmark : worldLandmarks_) {
        const auto addBox = [&](double centerX, double centerZ,
                                double halfX, double halfZ) {
            colliders.push_back({
                .minX = centerX - halfX,
                .maxX = centerX + halfX,
                .minZ = centerZ - halfZ,
                .maxZ = centerZ + halfZ,
                .maximumBlockingEyeY = landmark.position.y + 6.9,
                .minimumBlockingEyeY = landmark.position.y - 0.20,
            });
        };
        if (landmark.type == WorldLandmarkType::Mine) {
            // The mine is compact; do not block the empty corners of its
            // much larger visual/placement footprint.
            addBox(
                landmark.position.x, landmark.position.z,
                2.15, 2.0);
            continue;
        }
        // The lumber mill is long and asymmetric. Three compact boxes follow
        // its local length instead of turning one enclosing circle into a
        // huge square collider.
        constexpr std::array<double, 3> LocalOffsets{{-2.2, 0.0, 2.2}};
        const double sine = std::sin(landmark.yaw);
        const double cosine = std::cos(landmark.yaw);
        for (const double offset : LocalOffsets) {
            addBox(
                landmark.position.x + sine * offset,
                landmark.position.z + cosine * offset,
                1.25, 1.25);
        }
    }
    collisionWorld_.syncWorldLandmarks(colliders);
}

void Simulation::updateWorldLandmarks(
    double deltaSeconds, const PlayerCommand& command) {
    aimedWorldLandmark_.reset();
    const Vec3 direction = lookDirection(playerYaw_, playerPitch_);
    double nearest = LandmarkInteractionDistance;
    for (const WorldLandmarkInstance& landmark : worldLandmarks_) {
        if (landmark.activated) continue;
        Vec3 center = landmark.position;
        center.y += 3.4;
        const auto distance = geometry::raySphereDistance(
            playerPosition_, direction, center,
            LandmarkRaycastRadius);
        if (distance && *distance <= nearest) {
            nearest = *distance;
            aimedWorldLandmark_ = landmark.id;
        }
    }
    if (command.overrideAimedWorldLandmark) {
        aimedWorldLandmark_ = command.aimedWorldLandmarkOverride;
    }

    if (command.interact && aimedWorldLandmark_) {
        const auto landmark = std::ranges::find(
            worldLandmarks_, *aimedWorldLandmark_,
            &WorldLandmarkInstance::id);
        if (landmark != worldLandmarks_.end() &&
            !landmark->activated) {
            if (unlimitedResources_ ||
                coins_ >= landmark->activationCoinCost) {
                if (!unlimitedResources_) {
                    coins_ -= landmark->activationCoinCost;
                }
                landmark->activated = true;
                landmark->productionProgress = 0.0;
                events_.push_back({
                    .type = GameEventType::WorldLandmarkActivated,
                    .entityId = landmark->id,
                    .resourceType =
                        landmark->type == WorldLandmarkType::Mine
                            ? ResourceType::Stone
                            : ResourceType::Wood,
                    .buildingType =
                        landmark->type == WorldLandmarkType::Mine
                            ? BuildingType::Quarry
                            : BuildingType::LumberMill,
                    .position = landmark->position,
                    .amount = landmark->activationCoinCost,
                });
                aimedWorldLandmark_.reset();
            } else {
                events_.push_back({
                    .type = GameEventType::EconomyPurchaseRejected,
                    .entityId = landmark->id,
                    .position = landmark->position,
                    .amount = landmark->activationCoinCost,
                });
            }
        }
    }

    double productionDeltaSeconds = deltaSeconds;
    if (challengeActive()) productionDeltaSeconds = 0.0;
    if (state_ == RunState::Wave) {
        productionDeltaSeconds *= unlimitedResources_
            ? 1.0
            : std::clamp(
                  skillTree_.effectValue("production.night_speed"),
                  0.0, 1.0);
    }
    productionDeltaSeconds *= productionSpeedMultiplier_ *
        runProductionSpeedMultiplier_ * std::max(
        0.05, 1.0 + skillTree_.effectValue("production.speed"));
    if (!std::isfinite(productionDeltaSeconds) ||
        productionDeltaSeconds <= 0.0) {
        return;
    }
    for (WorldLandmarkInstance& landmark : worldLandmarks_) {
        if (!landmark.activated) continue;
        const double interval = landmarkProductionInterval(landmark.type);
        landmark.productionProgress += productionDeltaSeconds;
        const int completed = static_cast<int>(std::floor(
            landmark.productionProgress / interval));
        if (completed <= 0) continue;
        landmark.productionProgress = std::fmod(
            landmark.productionProgress, interval);
        const int attempted = completed *
            landmarkProductionAmount(landmark.type);
        const int before = landmark.type == WorldLandmarkType::Mine
            ? stone_ : wood_;
        if (landmark.type == WorldLandmarkType::Mine) {
            addStone(attempted);
        } else {
            addWood(attempted);
        }
        const int granted =
            (landmark.type == WorldLandmarkType::Mine ? stone_ : wood_) -
            before;
        if (granted <= 0) continue;
        events_.push_back({
            .type = GameEventType::ResourceGranted,
            .entityId = landmark.id,
            .resourceType = landmark.type == WorldLandmarkType::Mine
                ? ResourceType::Stone : ResourceType::Wood,
            .buildingType = landmark.type == WorldLandmarkType::Mine
                ? BuildingType::Quarry : BuildingType::LumberMill,
            .position = landmark.position,
            .amount = granted,
        });
    }
}

} // namespace ian
