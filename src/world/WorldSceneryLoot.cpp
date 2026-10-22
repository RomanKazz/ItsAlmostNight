#include "world/WorldSceneryLoot.hpp"

#include "core/DeterministicRandom.hpp"
#include "game/LootChestSystem.hpp"
#include "resources/ResourceSystem.hpp"
#include "world/CollisionWorld.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/WorldScenery.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

namespace ian {
namespace {
constexpr double Tau = 6.28318530717958647692;
constexpr double ChestChance = .45;
constexpr double PropSlotChance = .55;
constexpr int PropSlots = 3;

double rectangleDistance(Vec3 position, const CollisionBox& box) {
    return std::hypot(std::max({box.minX-position.x, 0.0, position.x-box.maxX}),
                      std::max({box.minZ-position.z, 0.0, position.z-box.maxZ}));
}
double distance(Vec3 a, Vec3 b) { return std::hypot(a.x-b.x, a.z-b.z); }
} // namespace

std::vector<SceneryLootPlacement> generateWorldSceneryLoot(
    const TerrainHeightfield& terrain, double worldLimit,
    std::span<const WorldSceneryInstance> scenery,
    std::span<const ResourceNode> resources,
    std::span<const LootChestInstance> chests) {
    std::vector<SceneryLootPlacement> result;
    if (scenery.empty()) return result;
    // Work on copies so accepted slots reserve space before the next roll.
    std::vector<ResourceNode> placedResources(resources.begin(), resources.end());
    std::vector<LootChestInstance> placedChests(chests.begin(), chests.end());
    std::vector<std::size_t> propPool, chestPool;
    for (std::size_t i = 0; i < resources.size(); ++i)
        if (resources[i].active && isDestructibleProp(resources[i].type))
            propPool.push_back(i);
    for (std::size_t i = 0; i < chests.size(); ++i)
        if (!chests[i].looseLoot && chests[i].state == LootChestState::Closed &&
            chests[i].purpose == LootChestPurpose::Exploration) chestPool.push_back(i);
    const auto shuffleKey = [&](EntityId id) {
        return mixBits64(static_cast<std::uint64_t>(terrain.seed()) ^
            (static_cast<std::uint64_t>(id.index)+1U)*0x94d049bb133111ebULL);
    };
    std::ranges::sort(propPool, [&](auto a, auto b) {
        return shuffleKey(resources[a].id) < shuffleKey(resources[b].id);
    });
    std::ranges::sort(chestPool, [&](auto a, auto b) {
        return shuffleKey(chests[a].id) < shuffleKey(chests[b].id);
    });
    struct Site { Vec3 sum{}; std::size_t count{}; };
    std::map<std::size_t, Site> sites;
    for (const auto& piece : scenery) {
        auto& site = sites[piece.site];
        site.sum.x += piece.position.x;
        site.sum.z += piece.position.z;
        ++site.count;
    }
    std::size_t nextProp = 0, nextChest = 0;
    for (const auto& [siteIndex, site] : sites) {
        const Vec3 center{site.sum.x/static_cast<double>(site.count), 0.0,
                          site.sum.z/static_cast<double>(site.count)};
        const std::uint64_t siteSeed = mixBits64(terrain.seed() ^
            (siteIndex+1U)*0xd1b54a32d192ed03ULL ^ 0x617a807555bb51c3ULL);
        const auto positionFor = [&](SceneryLootKind kind, std::size_t index,
                                     std::uint64_t seed) -> std::optional<Vec3> {
            const bool isChest = kind == SceneryLootKind::Chest;
            const double radius = isChest ? .82 :
                placedResources[index].radius*placedResources[index].visualScale;
            for (std::uint64_t attempt = 0; attempt < 128; ++attempt) {
                const auto roll = mixBits64(seed ^ (attempt+1U)*0x9e3779b97f4a7c15ULL);
                const double angle = unitRandom(roll)*Tau;
                const double offset = std::sqrt(unitRandom(roll ^ 0xa4093822299f31d0ULL))*6.0;
                Vec3 position{center.x+std::cos(angle)*offset, 0.0,
                              center.z+std::sin(angle)*offset};
                if (std::abs(position.x)+radius > worldLimit-3.0 ||
                    std::abs(position.z)+radius > worldLimit-3.0 ||
                    !terrain.isInside(position.x,position.z) ||
                    terrain.waterSignedDistance(position.x,position.z) < radius+1.5 ||
                    terrain.getNormal(position.x,position.z).y < .94) continue;
                position.y = terrain.getHeight(position.x,position.z);
                if (std::ranges::any_of(scenery, [&](const auto& piece) {
                        return rectangleDistance(position, worldSceneryCollisionBox(piece)) <
                            (isChest ? 1.4 : radius+.75);
                    })) continue;
                bool blocked = false;
                for (std::size_t i = 0; i < placedResources.size(); ++i) {
                    if (!isChest && i == index) continue;
                    const auto& node = placedResources[i];
                    // Inactive resource positions remain reserved until respawn.
                    if (distance(position,node.position) <
                        radius + std::max(.5,node.radius*node.visualScale) + .75) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) continue;
                for (std::size_t i = 0; i < placedChests.size(); ++i) {
                    if ((isChest && i == index) || placedChests[i].looseLoot) continue;
                    if (distance(position,placedChests[i].position) < (isChest ? 4.0 : radius+1.5)) {
                        blocked = true;
                        break;
                    }
                }
                if (!blocked) return position;
            }
            return std::nullopt;
        };
        if (unitRandom(siteSeed ^ 0xe7037ed1a0b428dbULL) < ChestChance &&
            nextChest < chestPool.size()) {
            const auto index = chestPool[nextChest];
            if (const auto position = positionFor(SceneryLootKind::Chest,index,siteSeed)) {
                const double yaw = unitRandom(siteSeed ^ 0x452821e638d01377ULL)*Tau;
                result.push_back({SceneryLootKind::Chest,placedChests[index].id,*position,yaw,siteIndex});
                placedChests[index].position = *position;
                ++nextChest;
            }
        }
        for (int slot = 0; slot < PropSlots && nextProp < propPool.size(); ++slot) {
            const auto seed = mixBits64(siteSeed ^
                static_cast<std::uint64_t>(slot+1)*0xa0761d6478bd642fULL);
            if (unitRandom(seed) >= PropSlotChance) continue;
            const auto index = propPool[nextProp];
            if (const auto position = positionFor(SceneryLootKind::DestructibleProp,index,seed)) {
                const double yaw = unitRandom(seed ^ 0x452821e638d01377ULL)*Tau;
                result.push_back({SceneryLootKind::DestructibleProp,placedResources[index].id,
                                  *position,yaw,siteIndex});
                placedResources[index].position = *position;
                ++nextProp;
            }
        }
    }
    return result;
}
} // namespace ian
