#include "game/Simulation.hpp"
#include "game/ChallengeArena.hpp"
#include "world/WorldSceneryLoot.hpp"

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

double landmarkRadius(WorldLandmarkType type) {
    if (landmarkShrineKind(type) != ShrineKind::None) return 1.6;
    return (type == WorldLandmarkType::Mine ? 3.6 : 4.1) *
        LandmarkVisualScale;
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

void Simulation::resetWorldScenery() {
    std::vector<WorldSceneryExclusion> exclusions;
    for (const auto& landmark : worldLandmarks_)
        exclusions.push_back({landmark.position, landmark.collisionRadius + 3.0});
    for (const auto& column : challengeColumns_)
        exclusions.push_back({column.position, challenge_arena::Radius + 4.0});
    for (const auto& chest : lootChests_.chests())
        if (!chest.looseLoot) exclusions.push_back({chest.position, 4.0});
    worldScenery_ = generateWorldScenery(terrain_, map_.worldLimit,
        map_.playerSpawn, resources_.nodes(), mapCollisionBoxes(map_), exclusions);
    for (const auto& placement : generateWorldSceneryLoot(
            terrain_, map_.worldLimit, worldScenery_, resources_.nodes(), lootChests_.chests())) {
        if (placement.kind == SceneryLootKind::Chest)
            static_cast<void>(lootChests_.relocateExplorationChest(
                placement.id, placement.groundPosition, terrain_, placement.yaw));
        else
            static_cast<void>(resources_.relocateDestructibleProp(
                placement.id, placement.groundPosition, placement.yaw));
    }
    syncWorldSceneryColliders();
}

void Simulation::syncWorldSceneryColliders() {
    std::vector<CollisionBox> colliders;
    colliders.reserve(worldScenery_.size());
    for (const auto& instance : worldScenery_) {
        const auto boxes = worldScenerySolidBoxes(instance);
        colliders.insert(colliders.end(), boxes.begin(), boxes.end());
    }
    collisionWorld_.syncWorldScenery(colliders);
    auto navigationObstacles = mapCollisionBoxes(map_);
    navigationObstacles.insert(navigationObstacles.end(), colliders.begin(), colliders.end());
    flowField_.setStaticObstacles(std::move(navigationObstacles));
    flowFieldDirty_ = true;
}

void Simulation::resetWorldLandmarks(bool includeShrines) {
    aimedWorldLandmark_.reset();
    worldLandmarks_.clear();
    worldLandmarks_.reserve(2 + MaximumShrineActivations);
    constexpr auto Types=[] {
        std::array<WorldLandmarkType, 2+MaximumShrineActivations> result{};
        result[0]=WorldLandmarkType::Mine;
        result[1]=WorldLandmarkType::LumberMill;
        for (int i=0;i<ShrineCountPerType;++i) {
            result[2+static_cast<std::size_t>(i)*2]=WorldLandmarkType::PlayerShrine;
            result[3+static_cast<std::size_t>(i)*2]=WorldLandmarkType::BuildingShrine;
        }
        return result;
    }();
    constexpr std::size_t MaximumAttemptsPerLandmark = 16384;
    const double maximumRadius = std::max(
        MinimumSpawnDistance + 1.0,
        map_.worldLimit - 8.0);
    const std::span<const TerrainPathDefinition> compositionRoutes =
        terrain_.paths();
    for (std::size_t typeIndex = 0;
         typeIndex < (includeShrines ? Types.size() : 2U); ++typeIndex) {
        const WorldLandmarkType type = Types[typeIndex];
        const double footprintRadius = landmarkRadius(type);
        const double minimumDistance=landmarkShrineKind(type)==ShrineKind::None ? MinimumSpawnDistance : 16.0;
        for (std::size_t attempt = 0;
             attempt < MaximumAttemptsPerLandmark; ++attempt) {
            const std::uint64_t seed = mixBits64(
                static_cast<std::uint64_t>(terrain_.seed()) ^
                (typeIndex + 1U) * 0xd1b54a32d192ed03ULL ^
                (attempt + 1U) * 0x9e3779b97f4a7c15ULL);
            Vec3 position{};
            if (typeIndex < 2 && attempt < compositionRoutes.size()) {
                // First try one of the four authored terrain-flow anchors.
                // A seed-dependent cyclic offset keeps reruns varied while
                // ensuring that landmarks have a real worn approach.
                const std::size_t routeOffset = static_cast<std::size_t>(
                    mixBits64(
                        static_cast<std::uint64_t>(terrain_.seed()) ^
                        (typeIndex + 1U) * 0xa4093822299f31d0ULL) %
                    compositionRoutes.size());
                const TerrainPathDefinition& route = compositionRoutes[
                    (routeOffset + attempt) % compositionRoutes.size()];
                position = {route.toX, 0.0, route.toZ};
            } else {
                // Preserve the previous deterministic free-placement search
                // as a safe fallback when all route anchors are obstructed.
                const double angle = unitRandom(
                    seed ^ 0x243f6a8885a308d3ULL) * Tau;
                const double radius = std::sqrt(
                    minimumDistance * minimumDistance +
                    unitRandom(seed ^ 0x13198a2e03707344ULL) *
                        (maximumRadius * maximumRadius -
                         minimumDistance * minimumDistance));
                position = {
                    map_.playerSpawn.x + std::cos(angle) * radius,
                    0.0,
                    map_.playerSpawn.z + std::sin(angle) * radius,
                };
            }
            if (!terrain_.isInside(position.x, position.z) ||
                terrain_.waterSignedDistance(
                    position.x, position.z) < footprintRadius + 3.0 ||
                !flatLandmarkSurface(
                    terrain_, position, footprintRadius)) {
                continue;
            }
            position.y = terrain_.getHeight(
                position.x, position.z);
            if (mapObstacleNear(
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
                    [position,type](
                        const WorldLandmarkInstance& landmark) {
                        return geometry::distanceSquared(
                                   position, landmark.position) <
                            (landmarkShrineKind(type) != ShrineKind::None ? 12.0*12.0 : LandmarkSeparation*LandmarkSeparation);
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
                .activationCoinCost = landmarkShrineKind(type) == ShrineKind::None
                    ? economy_.worldProducerRestoreCoins[type == WorldLandmarkType::Mine ? 0 : 1] : 0,
            });
            break;
        }
    }
    refreshWorldLandmarkEconomy();
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
        if (landmarkShrineKind(landmark.type) != ShrineKind::None) {
            addBox(landmark.position.x, landmark.position.z, 1.0, 1.0);
            colliders.back().maximumBlockingEyeY=landmark.position.y+2.8;
            continue;
        }
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
        if (landmark.activated && landmarkShrineKind(landmark.type)!=ShrineKind::None) continue;
        Vec3 center = landmark.position;
        const bool shrine=landmarkShrineKind(landmark.type)!=ShrineKind::None;
        center.y += shrine ? 1.4 : 3.4;
        const auto distance = geometry::raySphereDistance(
            playerPosition_, direction, center,
            shrine ? 1.6 : LandmarkRaycastRadius);
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
            (!landmark->activated || (landmarkShrineKind(landmark->type)==ShrineKind::None && landmark->level<5))) {
            if (landmarkShrineKind(landmark->type) != ShrineKind::None) {
                static_cast<void>(activateShrine(*landmark));
            } else if (unlimitedResources_ ||
                coins_ >= (landmark->activated ? landmark->upgradeCoinCost : landmark->activationCoinCost)) {
                const bool restoring=!landmark->activated;
                const int cost=restoring ? landmark->activationCoinCost : landmark->upgradeCoinCost;
                if (!unlimitedResources_) coins_ -= cost;
                if (restoring) {landmark->activated=true;landmark->productionProgress=0;}
                else ++landmark->level;
                events_.push_back({
                    .type = restoring ? GameEventType::WorldLandmarkActivated : GameEventType::WorldLandmarkUpgraded,
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
                    .amount = cost,
                });
            } else {
                events_.push_back({
                    .type = GameEventType::EconomyPurchaseRejected,
                    .entityId = landmark->id,
                    .position = landmark->position,
                    .amount = landmark->activated ? landmark->upgradeCoinCost : landmark->activationCoinCost,
                });
            }
        }
    }

    advanceWorldLandmarkProduction(deltaSeconds);
    refreshWorldLandmarkEconomy();
}

void Simulation::advanceWorldLandmarkProduction(
    double productionDeltaSeconds) {
    if (!std::isfinite(productionDeltaSeconds) ||
        productionDeltaSeconds <= 0.0 || challengeActive()) {
        return;
    }
    // World producers level through their own E interaction. Production cards
    // and shrine building bonuses apply only to player-built producers.
    productionDeltaSeconds *= productionSpeedMultiplier_ * classLevelBonuses().productionSpeed;
    for (WorldLandmarkInstance& landmark : worldLandmarks_) {
        if (!landmark.activated || landmarkShrineKind(landmark.type)!=ShrineKind::None) continue;
        const auto index=landmark.type==WorldLandmarkType::Mine ? 0U : 1U;
        const auto type=landmark.type==WorldLandmarkType::Mine ? ResourceType::Stone : ResourceType::Wood;
        const double interval=economy_.worldProducerInterval[index];
        landmark.productionProgress += productionDeltaSeconds * (1+.5*(landmark.level-1)) *
            relicProductionMultiplier(type);
        const int completed = static_cast<int>(std::floor(
            landmark.productionProgress / interval));
        if (completed <= 0) continue;
        landmark.productionProgress = std::fmod(
            landmark.productionProgress, interval);
        const int attempted = completed *
            economy_.worldProducerBatch[index];
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

void Simulation::refreshWorldLandmarkEconomy() {
    const double global=productionSpeedMultiplier_ * classLevelBonuses().productionSpeed;
    for(auto& landmark:worldLandmarks_) {
        if(landmarkShrineKind(landmark.type)!=ShrineKind::None) continue;
        const auto index=landmark.type==WorldLandmarkType::Mine ? 0U : 1U;
        const auto type=index==0 ? ResourceType::Stone : ResourceType::Wood;
        const double base=60.0*economy_.worldProducerBatch[index]/economy_.worldProducerInterval[index] * global *
            relicProductionMultiplier(type);
        landmark.activationCoinCost=economy_.worldProducerRestoreCoins[index];
        landmark.upgradeCoinCost=landmark.level<5 ? economy_.worldProducerUpgradeCoins[static_cast<std::size_t>(landmark.level-1)] : 0;
        landmark.incomePerMinute=base*(1+.5*(landmark.level-1));
        landmark.nextIncomePerMinute=landmark.level<5 ? base*(1+.5*landmark.level) : landmark.incomePerMinute;
    }
}

} // namespace ian
