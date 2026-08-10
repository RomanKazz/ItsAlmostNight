#include "TestHarness.hpp"
#include "assets/GlbCollision.hpp"

#include <string_view>

using namespace ian;

void runGlbCollisionTests() {
    constexpr std::string_view Document = R"json({
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [
            {"name": "Root", "translation": [0, 2, 0], "children": [1, 2, 3]},
            {"name": "COL_BOX_WALK_00", "mesh": 2, "translation": [1, 0, 0], "scale": [2, 1, 1]},
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
            "COL_BOX_WALK convention must set box and walkable");
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

    const GlbCollisionAsset tree =
        loadGlbCollisionAsset(
            IAN_SOURCE_DIR "/assets/models/environment/tree.glb");
    require(tree.valid() && tree.colliders.size() == 4,
            "tree GLB must expose three boxes and one cylinder");
    require(tree.renderMeshIndices.size() == 4,
            "all tree collision meshes must be hidden");
    require(tree.colliders.back().type ==
                ModelColliderType::Cylinder,
            "COL_CYL alias must create cylinder collider");

    const GlbCollisionAsset treeB =
        loadGlbCollisionAsset(
            IAN_SOURCE_DIR "/assets/models/environment/tree_b.glb");
    require(treeB.valid() && treeB.colliders.size() == 4,
            "tree B GLB must expose three boxes and one cylinder");
    const GlbCollisionAsset treeC =
        loadGlbCollisionAsset(
            IAN_SOURCE_DIR "/assets/models/environment/tree_c.glb");
    require(treeC.valid() && treeC.colliders.size() == 5,
            "tree C GLB must expose four boxes and one cylinder");
}
