#include "TestHarness.hpp"
#include "app/MenuBackgroundScene.hpp"

#include <nlohmann/json.hpp>

#include <limits>

void runMenuBackgroundSceneTests() {
    ian::WorldConfig config;
    config.terrainResolution = 65;
    config.terrainWorldSize = 128.0;
    ian::TerrainHeightfield terrain{config};
    terrain.generate(73129U);
    ian::MenuBackgroundScene scene;
    const auto tower = scene.preview(ian::BuildingType::GunTurret,
        {4.5, 0.0, 4.5}, 7U, terrain, 60.0);
    require(tower.placement.valid() && scene.place(tower),
        "menu construction needs neither resources nor a Core");
    require(scene.buildings().front().rotation == 7U,
        "menu preserves eight-direction defense rotation");
    require(!scene.preview(ian::BuildingType::Wall,
        {4.5, 0.0, 4.5}, 0U, terrain, 60.0).placement.valid(),
        "menu preview rejects overlapping buildings");
    require(!scene.preview(ian::BuildingType::Core,
        {60.0, 0.0, 0.0}, 0U, terrain, 60.0).placement.valid(),
        "entire building footprint must remain inside the world");
    const auto core = scene.preview(ian::BuildingType::Core,
        {0.0, 0.0, 0.0}, 2U, terrain, 60.0);
    require(core.placement.valid() && scene.place(core), "menu can place a Core");
    require(scene.preview(ian::BuildingType::Core,
        {-4.0, 0.0, 0.0}, 0U, terrain, 60.0).placement.error ==
            ian::PlacementError::CoreAlreadyPlaced,
        "menu contains at most one Core");
    const auto saved = scene.toJson();
    ian::MenuBackgroundScene loaded;
    require(loaded.loadJson(nlohmann::json::parse(saved.dump()), 60.0),
        "menu buildings survive a serialized save and reload");
    require(loaded.toJson() == saved,
        "save keeps position, height, foundation, rotation and level");
    auto edit = loaded;
    const auto center = ian::buildingWorldPosition(edit.buildings().front());
    require(edit.remove({center.x, center.y + 12.0, center.z},
            {0.0, -1.0, 0.0}, 20.0),
        "reticle ray removes the aimed building");
    require(loaded.toJson() == saved && edit.buildings().size() == 1U,
        "editing a copied scene leaves the saved/cancel scene intact");
    require(!edit.remove({45.0, 30.0, 45.0}, {0.0, -1.0, 0.0}, 50.0),
        "empty removal rays do not delete another building");
    auto broken = saved;
    broken[0]["type"] = 999;
    require(!loaded.loadJson(broken, 60.0) && loaded.toJson() == saved,
        "invalid building types cannot corrupt the saved scene");
    broken = saved;
    broken[0]["baseHeight"] = std::numeric_limits<double>::infinity();
    require(!loaded.loadJson(broken, 60.0) && loaded.toJson() == saved,
        "non-finite building heights are rejected atomically");
    broken = saved;
    broken[0]["rotation"] = 8;
    require(!loaded.loadJson(broken, 60.0), "invalid rotation is rejected");
    broken = saved;
    broken[0]["x"] = 1000000;
    require(!loaded.loadJson(broken, 60.0), "out-of-world saved buildings are rejected");
    broken = saved;
    broken.push_back(saved[1]);
    require(!loaded.loadJson(broken, 60.0), "duplicate saved Cores are rejected");
    auto oversized = nlohmann::json::array();
    for (std::size_t index = 0; index <= ian::MenuBackgroundScene::MaximumBuildings; ++index) {
        oversized.push_back(saved[0]);
    }
    require(!loaded.loadJson(oversized, 60.0), "menu save size is bounded");
    require(loaded.toJson() == saved, "failed loads never replace the previous scene");
    require(loaded.loadJson(nlohmann::json::array(), 60.0) && loaded.buildings().empty(),
        "empty compositions remain empty after reload");
}
