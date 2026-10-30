#include "ui/MinimapMarkers.hpp"
#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace ian {
std::vector<MinimapMarker> collectMinimapMarkers(const SimulationSnapshot& snapshot) {
    std::vector<MinimapMarker> markers;
    markers.reserve(160);
    const bool reveal = snapshot.unlimitedResources ||
        snapshot.lootStacks[lootUpgradeIndex(LootUpgradeEffect::Map)] > 0;
    const auto nearby = [&](Vec3 p) {
        return std::hypot(p.x - snapshot.playerPosition.x, p.z - snapshot.playerPosition.z) <= 58;
    };
    for (const LootChestInstance& chest : snapshot.lootChests) {
        if (chest.looseLoot || (!reveal && !chest.revealed) ||
            (chest.state == LootChestState::Open && (!chest.loot.available || chest.loot.collected))) continue;
        markers.push_back({MapIcon::Chest, chest.position, 6, 1, chest.loot.available && !chest.loot.collected});
    }
    for (const ChallengeColumnInstance& column : snapshot.challengeColumns) {
        if (column.state == ChallengeColumnState::Completed ||
            (!reveal && !nearby(column.position) && column.state != ChallengeColumnState::Active)) continue;
        markers.push_back({MapIcon::Challenge, column.position, 5, 1, column.state == ChallengeColumnState::Active});
    }
    for (const WorldLandmarkInstance& landmark : snapshot.worldLandmarks) {
        if (!reveal && !nearby(landmark.position) && !landmark.activated) continue;
        markers.push_back({landmarkShrineKind(landmark.type)==ShrineKind::Player ? MapIcon::PlayerShrine
            : landmarkShrineKind(landmark.type)==ShrineKind::Buildings ? MapIcon::BuildingShrine
            : MapIcon::Industry,
            landmark.position, 4, 1, landmark.activated});
    }
    struct Group { Vec3 sum{}; int count{}; Vec3 anchor{}; double anchorDistance{1e30}; };
    std::map<std::size_t, Group> sites;
    for (const WorldSceneryInstance& piece : snapshot.worldScenery) {
        auto& site = sites[piece.site];
        site.sum.x += piece.position.x; site.sum.z += piece.position.z; ++site.count;
    }
    for (const auto& [id, site] : sites) {
        (void)id;
        markers.push_back({MapIcon::Ruins, {site.sum.x / site.count, 0, site.sum.z / site.count}, 3});
    }
    constexpr double CellSize = 24.0;
    const auto key = [](const ResourceNode& node) {
        return std::tuple{static_cast<int>(std::floor(node.position.x / CellSize)),
            static_cast<int>(std::floor(node.position.z / CellSize)),
            isDestructibleProp(node.type) ? 3 : static_cast<int>(node.type)};
    };
    const auto visible = [&](const ResourceNode& node) {
        return node.active && (isHarvestableResource(node.type) ||
            (isDestructibleProp(node.type) && (reveal || nearby(node.position))));
    };
    std::map<std::tuple<int, int, int>, Group> groups;
    for (const ResourceNode& node : snapshot.resourceNodes) {
        if (!visible(node)) continue;
        auto& group = groups[key(node)];
        group.sum.x += node.position.x; group.sum.z += node.position.z; ++group.count;
    }
    // Anchor on an actual resource, rather than putting a mean in water or a wall.
    for (const ResourceNode& node : snapshot.resourceNodes) {
        if (!visible(node)) continue;
        auto& group = groups.find(key(node))->second;
        const double distance = std::hypot(node.position.x - group.sum.x / group.count,
            node.position.z - group.sum.z / group.count);
        if (distance < group.anchorDistance) { group.anchor = node.position; group.anchorDistance = distance; }
    }
    constexpr MapIcon icons[]{MapIcon::Wood, MapIcon::Stone, MapIcon::Crystal, MapIcon::Supplies};
    constexpr int priorities[]{0, 1, 2, 2};
    for (const auto& [groupKey, group] : groups) {
        const int type = std::get<2>(groupKey);
        markers.push_back({icons[type], group.anchor, priorities[type], group.count});
    }
    std::stable_sort(markers.begin(), markers.end(), [](const auto& a, const auto& b) { return a.priority > b.priority; });
    return markers;
}
} // namespace ian
