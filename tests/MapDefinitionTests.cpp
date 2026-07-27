#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "game/Simulation.hpp"
#include "resources/ResourceSystem.hpp"
#include "waves/WaveDirector.hpp"
#include "world/CollisionWorld.hpp"
#include "world/MapDefinition.hpp"

void runMapDefinitionTests() {
    const auto asset = ian::loadMapDefinition("assets/maps/graybox.json");
    require(asset.valid() && asset.map.resources.size() == 7 &&
                asset.map.enemySpawnAnchors.size() == 3,
            "copied graybox map loads");

    constexpr std::string_view CustomMap = R"json({
        "playerSpawn": {"x": 2, "y": 0, "z": 3},
        "worldLimit": 20,
        "coreBuildRadius": 5,
        "enemySpawnAnchors": [{"x": 0, "y": 0, "z": -15}],
        "resources": [{
            "type": "stone",
            "position": {"x": 2, "y": 1, "z": 1},
            "radius": 0.5,
            "health": 2,
            "yield": 9,
            "respawnSeconds": 4
        }],
        "obstacles": [
            {"minX": 5, "maxX": 7, "minZ": 5, "maxZ": 8, "height": 3}
        ]
    })json";
    const auto loaded = ian::parseMapDefinition(CustomMap);
    require(loaded.valid() && loaded.map.playerSpawn.x == 2.0 &&
                loaded.map.coreBuildRadius == 5,
            "valid custom map parses");
    ian::Simulation simulation{ian::GameBalance::defaults(), loaded.map};
    simulation.startRun();
    require(simulation.snapshot().playerPosition.x == 2.0 &&
                simulation.snapshot().playerPosition.z == 3.0 &&
                simulation.snapshot().resourceNodes.size() == 1 &&
                simulation.snapshot().mapObstacles.size() == 1,
            "simulation consumes loaded map");

    ian::ResourceSystem resources{loaded.map.resources};
    require(resources.nodes().size() == 1 &&
                resources.nodes()[0].type == ian::ResourceType::Stone &&
                resources.nodes()[0].yield == 9,
            "resource system consumes map nodes");

    ian::CollisionWorld collision{
        loaded.map.worldLimit, {loaded.map.obstacles[0].collision}};
    require(collision.overlapsBox({5.5, 6.5, 6.0, 7.0}),
            "collision world consumes map obstacle");
    const auto clamped = collision.moveCircle(
        {19.0, 1.7, 0.0}, {5.0, 0.0, 0.0}, ian::CollisionWorld::PlayerRadius);
    requireNear(clamped.x, 20.0 - ian::CollisionWorld::PlayerRadius, 1e-12,
                "collision world consumes map boundary");

    ian::BuildingSystem buildings{ian::GameBalance::defaults().buildings,
                                  ian::GameBalance::defaults().economy,
                                  loaded.map.coreBuildRadius};
    require(buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0).has_value(),
            "custom-radius fixture creates core");
    require(buildings.validate(ian::BuildingType::Wall, {6, 0}, 10, 0).error ==
                ian::PlacementError::OutsideCoreArea,
            "building system consumes map build radius");

    ian::WaveDirector waves{ian::GameBalance::defaults().waves,
                            loaded.map.enemySpawnAnchors};
    const auto wave = waves.buildWave(1, {0, 0});
    require(!wave.spawns.empty() && wave.spawns.front().position.z <= -15.0,
            "wave director consumes map spawn anchor");

    const auto invalid = ian::parseMapDefinition(R"json({
        "playerSpawn": {"x": 99, "y": 0, "z": 0},
        "worldLimit": 10,
        "coreBuildRadius": 5,
        "enemySpawnAnchors": [],
        "resources": [],
        "obstacles": []
    })json");
    require(!invalid.valid() &&
                invalid.map.worldLimit == ian::MapDefinition::defaults().worldLimit,
            "invalid map preserves safe fallback");
}
