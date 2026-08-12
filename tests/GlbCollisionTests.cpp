#include "TestHarness.hpp"
#include "assets/GlbCollision.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

using namespace ian;

void runGlbCollisionTests() {
    constexpr std::string_view Document = R"json({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [
            {"name": "Root", "translation": [0, 2, 0], "children": [1, 2, 3]},
            {"name": "COLLIDER_BOX_WALK.001", "mesh": 2, "translation": [1, 0, 0], "scale": [2, 1, 1]},
            {"name": "Visual", "mesh": 0},
            {"name": "AnyName", "mesh": 1, "extras": {"collision_type": "CYLINDER", "walkable": false}}
        ],
        "meshes": [
            {"primitives": [{"attributes": {"POSITION": 0}}]},
            {"primitives": [{"attributes": {"POSITION": 1}}]},
            {"primitives": [
                {"attributes": {"POSITION": 2}},
                {"mode": 1, "attributes": {"POSITION": 2}}
            ]}
        ],
        "accessors": [
            {"min": [0, 0, 0], "max": [1, 1, 1]},
            {"min": [-0.5, -1, -0.5], "max": [0.5, 1, 0.5]},
            {"min": [-1, -1, -1], "max": [1, 1, 1]}
        ]
    })json";

    const GlbCollisionAsset asset =
        parseGlbCollisionJson(Document);
    require(asset.valid(), "valid glTF collision JSON must parse");
    require(asset.colliders.size() == 2,
            "name and extras colliders must be discovered");
    require(asset.renderMeshIndices.size() == 2,
            "only collider triangle primitives must be hidden");
    require(asset.renderMeshIndices[0] == 0 &&
                asset.renderMeshIndices[1] == 2,
            "hidden mesh indices must follow raylib node order");

    const ModelCollider& box = asset.colliders[0];
    require(box.type == ModelColliderType::Box && box.walkable,
            "COLLIDER_BOX suffix convention must set box and walkable");
    requireNear(box.minimum.x, -1.0, 1e-9,
                "parent, translation, and scale must affect minimum x");
    requireNear(box.maximum.x, 3.0, 1e-9,
                "parent, translation, and scale must affect maximum x");
    requireNear(box.minimum.y, 1.0, 1e-9,
                "parent translation must affect minimum y");
    requireNear(box.maximum.y, 3.0, 1e-9,
                "parent translation must affect maximum y");

    const ModelCollider& cylinder = asset.colliders[1];
    require(cylinder.type == ModelColliderType::Cylinder &&
                !cylinder.walkable,
            "custom properties must override name convention");

    const GlbCollisionAsset invalid =
        parseGlbCollisionJson("not json");
    require(!invalid.valid() && !invalid.errors.empty(),
            "invalid collision JSON must report an error");

    const GlbCollisionAsset platform =
        loadGlbCollisionAsset(
            IAN_SOURCE_DIR "/assets/models/construction/platform.glb");
    require(platform.valid() && platform.colliders.size() == 1,
            "shipped platform GLB must contain one valid collider");
    require(platform.renderMeshIndices.size() == 1 &&
                platform.renderMeshIndices[0] == 0,
            "shipped platform collider mesh must be hidden");
    const ModelCollider& platformBox = platform.colliders[0];
    require(platformBox.type == ModelColliderType::Box &&
                platformBox.walkable,
            "shipped platform collider must be walkable box");
    requireNear(platformBox.minimum.x, -1.0, 1e-4,
                "platform collider minimum x must match model");
    requireNear(platformBox.maximum.x, 1.0, 1e-4,
                "platform collider maximum x must match model");
    requireNear(platformBox.minimum.y, -0.126734, 1e-4,
                "platform collider underside must match model");
    requireNear(platformBox.maximum.y, 0.0, 1e-4,
                "platform collider top must stay on grid plane");

    const GlbCollisionAsset ramp =
        loadGlbCollisionAsset(
            IAN_SOURCE_DIR "/assets/models/construction/ramp.glb");
    require(ramp.valid() && ramp.colliders.size() == 1,
            "shipped ramp GLB must contain one valid collider");
    require(ramp.renderMeshIndices.size() == 1 &&
                ramp.renderMeshIndices[0] == 0,
            "shipped ramp collider mesh must be hidden");
    const ModelCollider& rampSlope = ramp.colliders[0];
    require(rampSlope.type == ModelColliderType::Slope &&
                rampSlope.walkable,
            "COL_RAMP_WALK alias must create walkable slope");
    requireNear(rampSlope.minimum.y, -0.065073, 1e-4,
                "ramp collider underside must match model");
    requireNear(rampSlope.maximum.y, 3.999996, 1e-4,
                "ramp collider high edge must match model");

    constexpr std::array<std::string_view, 9> TreePaths{{
        "/assets/models/environment/tree_1_a.glb",
        "/assets/models/environment/tree_1_b.glb",
        "/assets/models/environment/tree_1_c.glb",
        "/assets/models/environment/tree_2_a.glb",
        "/assets/models/environment/tree_2_b.glb",
        "/assets/models/environment/tree_2_c.glb",
        "/assets/models/environment/tree_3_a.glb",
        "/assets/models/environment/tree_3_b.glb",
        "/assets/models/environment/tree_3_c.glb",
    }};
    constexpr std::array<std::size_t, 9> ColliderCounts{{
        5U, 4U, 4U, 4U, 4U, 4U, 3U, 2U, 2U,
    }};
    for (std::size_t index = 0; index < TreePaths.size(); ++index) {
        const GlbCollisionAsset tree = loadGlbCollisionAsset(
            std::string(IAN_SOURCE_DIR) + std::string(TreePaths[index]));
        require(tree.valid() &&
                    tree.colliders.size() == ColliderCounts[index],
                "every tree GLB must expose all authored colliders");
        require(tree.renderMeshIndices.size() == ColliderCounts[index],
                "every tree collider mesh must be hidden");
        require(std::any_of(
                    tree.colliders.begin(), tree.colliders.end(),
                    [](const ModelCollider& collider) {
                        return collider.type == ModelColliderType::Cylinder;
                    }),
                "every tree needs a cylinder collider");
    }

    constexpr std::array<std::string_view, 3> StonePaths{{
        "/assets/models/environment/stone_1.glb",
        "/assets/models/environment/stone_2.glb",
        "/assets/models/environment/stone_3.glb",
    }};
    for (const std::string_view path : StonePaths) {
        const GlbCollisionAsset stone = loadGlbCollisionAsset(
            std::string(IAN_SOURCE_DIR) + std::string(path));
        require(stone.valid() && stone.colliders.size() == 1U,
                "every stone GLB must expose its authored collider");
        require(stone.renderMeshIndices.size() == 1U,
                "stone collision mesh must stay hidden");
        require(stone.colliders.front().type ==
                    ModelColliderType::Sphere,
                "COL_SPHERE stone collider must parse as a sphere");
    }
}
