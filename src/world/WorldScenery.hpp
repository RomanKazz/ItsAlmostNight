#pragma once

#include "core/Types.hpp"

#include <array>
#include <span>
#include <string_view>
#include <vector>

namespace ian {
class TerrainHeightfield;
struct ResourceNode;
struct CollisionBox;

enum class WorldSceneryModel : std::size_t {
    StoneFence, WoodFence, StoneWall, RuinedHouse, RuinedFrame,
    Well, Tent, LumberStack, Wheelbarrow, Crate, Count,
};
inline constexpr std::size_t WorldSceneryModelCount =
    static_cast<std::size_t>(WorldSceneryModel::Count);
struct WorldSceneryDefinition {
    std::string_view path;
    double width, height, depth;
};
// Models are baked to meters, centered in X/Z and grounded at Y=0 on import.
// Keep these bounds shared by placement, collision, culling and rendering.
inline constexpr std::array<WorldSceneryDefinition, WorldSceneryModelCount>
WorldSceneryDefinitions{{
    {"assets/models/world_scenery/stone_fence.glb", .856, 1.15, 4.938},
    {"assets/models/world_scenery/wood_fence.glb", .328, 1.80, 3.780},
    {"assets/models/world_scenery/stone_wall.glb", 6.364, 3.50, 2.546},
    {"assets/models/world_scenery/ruined_house.glb", 6.350, 4.10, 5.388},
    {"assets/models/world_scenery/ruined_frame.glb", 4.495, 2.80, 4.979},
    {"assets/models/world_scenery/well.glb", 1.973, 2.50, 2.273},
    {"assets/models/world_scenery/tent.glb", 2.300, 2.30, 2.300},
    {"assets/models/world_scenery/lumber_stack.glb", 2.777, .85, 1.339},
    {"assets/models/world_scenery/wheelbarrow.glb", 1.074, .85, 2.282},
    {"assets/models/world_scenery/crate.glb", .850, .85, .850},
}};
struct WorldSceneryInstance {
    EntityId id;
    WorldSceneryModel model{};
    Vec3 position{};
    double yaw{};
    std::size_t site{};
};
struct WorldSceneryExclusion {
    Vec3 position;
    double radius;
};

[[nodiscard]] CollisionBox worldSceneryCollisionBox(
    const WorldSceneryInstance& instance);
[[nodiscard]] std::vector<CollisionBox> worldScenerySolidBoxes(
    const WorldSceneryInstance& instance);
[[nodiscard]] std::vector<WorldSceneryInstance> generateWorldScenery(
    const TerrainHeightfield& terrain, double worldLimit, Vec3 spawn,
    std::span<const ResourceNode> resources,
    std::span<const CollisionBox> obstacles,
    std::span<const WorldSceneryExclusion> exclusions);
} // namespace ian
