#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "world/WorldScenery.hpp"
#include "world/WorldSceneryLoot.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <set>

namespace {
double rectangleDistance(double x, double z, const ian::CollisionBox& box) {
    return std::hypot(std::max({box.minX-x, 0.0, x-box.maxX}),
                      std::max({box.minZ-z, 0.0, z-box.maxZ}));
}

bool sameScenery(std::span<const ian::WorldSceneryInstance> a,
                 std::span<const ian::WorldSceneryInstance> b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].model != b[i].model || a[i].site != b[i].site ||
            a[i].yaw != b[i].yaw || a[i].position.x != b[i].position.x ||
            a[i].position.y != b[i].position.y || a[i].position.z != b[i].position.z)
            return false;
    }
    return true;
}

void sceneryLootUsesSeededChance() {
    auto config = ian::WorldConfig::defaults();
    config.terrainResolution = 33;
    config.terrainWorldSize = 200.0;
    config.terrainAmplitude = 0.0;
    config.terrainSurfaceNoiseAmplitude = 0.0;
    config.terrainBoundaryRiseHeight = 0.0;
    config.pondMinimumCount = config.pondMaximumCount = 0;
    ian::TerrainHeightfield terrain{config};
    ian::ResourceSystem props{std::vector<ian::ResourceNodeDefinition>{
        {ian::ResourceType::Barrel,{-60,.62,-60},.68,4,0,75},
        {ian::ResourceType::Crate,{-50,.62,-60},.68,5,0,75},
        {ian::ResourceType::ItemCrate,{-40,.62,-60},.68,5,0,75},
    }};
    const std::array scenery{ian::WorldSceneryInstance{
        .model=ian::WorldSceneryModel::StoneFence,.position={60,0,0},.site=0}};
    ian::LootChestSystem chests;
    int chestMaps = 0, emptyMaps = 0;
    bool hadBarrel = false, hadCrate = false;
    for (unsigned seed = 1; seed <= 128; ++seed) {
        terrain.generate(seed);
        chests.reset(seed,95,terrain,props.nodes(),{0,1.7,0});
        const auto placements = ian::generateWorldSceneryLoot(
            terrain,95,scenery,props.nodes(),chests.chests());
        const auto repeated = ian::generateWorldSceneryLoot(
            terrain,95,scenery,props.nodes(),chests.chests());
        require(placements.size() == repeated.size(), "loot chance is repeatable for a seed");
        std::set<std::pair<ian::SceneryLootKind,std::uint32_t>> ids;
        int chestCount = 0, propCount = 0;
        for (std::size_t i = 0; i < placements.size(); ++i) {
            const auto& placement = placements[i];
            const auto& repeat = repeated[i];
            require(placement.kind == repeat.kind && placement.id == repeat.id &&
                    placement.groundPosition.x == repeat.groundPosition.x &&
                    placement.groundPosition.z == repeat.groundPosition.z && placement.yaw == repeat.yaw,
                    "loot IDs, locations and orientation are seed deterministic");
            require(ids.emplace(placement.kind,placement.id.index).second,
                    "a world loot object belongs to only one scenery slot");
            require(std::hypot(placement.groundPosition.x-60,placement.groundPosition.z) <= 6.0,
                    "scenery loot stays in its location");
            require(rectangleDistance(placement.groundPosition.x,placement.groundPosition.z,
                                     ian::worldSceneryCollisionBox(scenery.front())) > 1.35,
                    "scenery loot does not overlap walls");
            if (placement.kind == ian::SceneryLootKind::Chest) ++chestCount;
            else {
                ++propCount;
                hadBarrel |= props.nodes()[placement.id.index].type == ian::ResourceType::Barrel;
                hadCrate |= props.nodes()[placement.id.index].type == ian::ResourceType::Crate;
            }
        }
        require(chestCount <= 1 && propCount <= 3, "each site has at most one chest and three props");
        chestMaps += chestCount;
        emptyMaps += propCount == 0;
    }
    require(chestMaps >= 35 && chestMaps <= 80, "45 percent chest roll produces both occupied and empty sites");
    require(emptyMaps > 0 && hadBarrel && hadCrate, "prop rolls include empty sites, barrels and crates");

    const auto propId = props.nodes().front().id;
    const double health = props.nodes().front().health;
    require(props.relocateDestructibleProp(propId,{60,2,0},.5), "interactive barrel can be relocated");
    require(props.nodes().front().id == propId && props.nodes().front().health == health,
            "relocation preserves destructible ID and health");
    requireNear(props.nodes().front().position.y-props.nodes().front().groundOffset,2,1e-12,
                "relocated prop retains its ground offset");
    require(props.damage(propId,100)->collected && !props.nodes().front().active,
            "relocated barrel remains destructible");
    require(!props.relocateDestructibleProp(propId,{62,0,0},0), "depleted barrel is never revived by relocation");
    const auto chestId = chests.chests().front().id;
    const auto effect = chests.chests().front().loot.effect;
    require(chests.relocateExplorationChest(chestId,{60,0,4},terrain,.5),
            "exploration chest can be placed in a scenery location");
    require(chests.chests().front().id == chestId && chests.chests().front().loot.effect == effect &&
            chests.chests().size() == 14, "relocation preserves chest loot and world chest budget");
    int coins = 100;
    require(chests.open(chestId,coins) == ian::ChestOpenResult::Opened,
            "relocated chest opens with the existing interaction");
    require(!chests.relocateExplorationChest(chestId,{62,0,0},terrain,0),
            "opening chest cannot be moved or reset by population");
}
} // namespace

void runWorldSceneryTests() {
    sceneryLootUsesSeededChance();
    const auto map = ian::loadMapDefinition(IAN_SOURCE_DIR "/assets/maps/graybox.json");
    const auto config = ian::loadWorldConfig(IAN_SOURCE_DIR "/assets/data/world.json");
    require(map.valid() && config.valid(), "scenery test uses the shipped map");
    auto simulation = std::make_unique<ian::Simulation>(
        ian::GameBalance::defaults(), map.map, config.config);
    std::vector<ian::WorldSceneryInstance> first;
    for (const unsigned seed : {42U, 123U, 73130U}) {
        simulation->returnToMainMenu();
        simulation->startRunFromSeed(ian::PlayerClass::Vanguard, seed);
        const auto& snapshot = simulation->snapshot();
        std::set<std::size_t> sites;
        std::set<std::uint32_t> ids;
        for (const auto& piece : snapshot.worldScenery) {
            sites.insert(piece.site);
            require(ids.insert(piece.id.index).second, "scenery entity IDs are unique");
            const auto box = ian::worldSceneryCollisionBox(piece);
            require(std::hypot(piece.position.x-map.map.playerSpawn.x,
                               piece.position.z-map.map.playerSpawn.z) > 30.0,
                    "scenery preserves the starting clearing");
            for (const double x : {box.minX, box.maxX}) {
                for (const double z : {box.minZ, box.maxZ}) {
                    require(simulation->terrain().waterSignedDistance(x,z) >= 2.0,
                            "scenery corners stay on dry land");
                    require(std::abs(x) < 144.0 && std::abs(z) < 144.0,
                            "scenery stays inside the playable mountain boundary");
                    require(std::abs(simulation->terrain().getHeight(x,z) -
                                     piece.position.y) < .35,
                            "scenery is grounded without hanging over a cliff");
                }
            }
            for (const auto& node : snapshot.resourceNodes)
                require(rectangleDistance(node.position.x,node.position.z,box) >=
                            std::max(.5,node.radius*node.visualScale)+.64,
                        "scenery preserves existing resource footprints");
            for (const auto& chest : snapshot.lootChests)
                if (!chest.looseLoot)
                    require(rectangleDistance(chest.position.x,chest.position.z,box) > 1.35,
                            "scenery does not block a loot chest");
        }
        require(sites.size() >= 6 && sites.size() <= 12,
                "representative maps contain a bounded number of scenery sites");
        if (first.empty()) first.assign(snapshot.worldScenery.begin(), snapshot.worldScenery.end());
        else require(!sameScenery(first,snapshot.worldScenery), "different seeds vary scenery");
    }
    simulation->returnToMainMenu();
    simulation->startRunFromSeed(ian::PlayerClass::Vanguard,42U);
    require(sameScenery(first,simulation->snapshot().worldScenery),
            "scenery seed is repeatable after visiting other maps");
    const auto saved = simulation->saveSuspendedRunState();
    simulation->returnToMainMenu();
    simulation->startRunFromSeed(ian::PlayerClass::Vanguard,123U);
    simulation->returnToMainMenu();
    require(simulation->loadSuspendedRunState(saved), "scenery supports Continue");
    require(sameScenery(first,simulation->snapshot().worldScenery),
            "Continue preserves scenery placement");
    require(simulation->snapshot().lootChests.size() == saved.lootChests.size() &&
            simulation->snapshot().resourceNodes.size() == saved.resourceNodes.size(),
            "Continue preserves world loot counts");
    for (std::size_t i = 0; i < saved.lootChests.size(); ++i) {
        const auto& chest = simulation->snapshot().lootChests[i];
        require(chest.position.x == saved.lootChests[i].position.x &&
                chest.position.z == saved.lootChests[i].position.z,
                "Continue restores scenery chest positions");
    }
    auto lootedSave = saved;
    lootedSave.lootChests.front().state = ian::LootChestState::Open;
    lootedSave.lootChests.front().openingProgress = 1.0;
    lootedSave.lootChests.front().loot.available = false;
    lootedSave.lootChests.front().loot.collected = true;
    const auto depleted = std::ranges::find_if(lootedSave.resourceNodes, [](const auto& node) {
        return ian::isDestructibleProp(node.type);
    });
    require(depleted != lootedSave.resourceNodes.end(), "world contains interactive scenery props");
    depleted->active = false;
    depleted->health = 0.0;
    depleted->yieldRemaining = 0;
    depleted->respawnRemaining = 30.0;
    const auto depletedIndex = static_cast<std::size_t>(depleted-lootedSave.resourceNodes.begin());
    simulation->returnToMainMenu();
    require(simulation->loadSuspendedRunState(lootedSave), "looted scenery checkpoint restores");
    require(simulation->snapshot().lootChests.front().state == ian::LootChestState::Open &&
            simulation->snapshot().lootChests.front().loot.collected &&
            !simulation->snapshot().resourceNodes[depletedIndex].active,
            "Continue never replenishes opened chests or destroyed props");
    auto olderSave = saved;
    olderSave.playerPosition = first.front().position;
    olderSave.playerPosition.y += 1.7;
    simulation->returnToMainMenu();
    require(simulation->loadSuspendedRunState(olderSave), "older saves remain loadable");
    for (const auto& piece : simulation->snapshot().worldScenery)
        require(rectangleDistance(olderSave.playerPosition.x,olderSave.playerPosition.z,
                                 ian::worldSceneryCollisionBox(piece)) >= 1.5,
                "Continue never puts the saved player inside new scenery");

    ian::CollisionWorld collision{144.0,{}};
    const ian::WorldSceneryInstance fence{
        .model=ian::WorldSceneryModel::StoneFence,.position={60.0,0.0,0.0}};
    const auto fenceBox = ian::worldSceneryCollisionBox(fence);
    collision.syncWorldScenery(std::array{fenceBox});
    require(collision.moveCircle({57.0,1.7,0.0},{6.0,0.0,0.0},.35).x < fenceBox.minX,
            "player cannot walk through scenery walls");
    require(collision.moveCircleAgainstRaisedSurfaces(
                {57.0,1.7,0.0},{6.0,0.0,0.0},.35,0.0).x < fenceBox.minX,
            "ground enemies cannot walk through scenery walls");
    require(collision.moveCircle({57.0,3.2,0.0},{6.0,0.0,0.0},.35).x > fenceBox.maxX,
            "player can jump over low scenery walls");
    const auto direction = collision.scenerySteeringDirection({58.0,1.7,0.0},{1,0,0},.35);
    require(std::abs(direction.z) > .5, "enemies steer around scenery beyond the core flow field");
    ian::Vec3 approaching{57.0,1.7,0.0};
    for (int step = 0; step < 600; ++step) {
        const ian::Vec3 desired{64.0-approaching.x,0.0,-approaching.z};
        if (std::hypot(desired.x,desired.z) < .5) break;
        const auto heading=collision.scenerySteeringDirection(approaching,desired,.35);
        approaching=collision.moveCircleAgainstRaisedSurfaces(
            approaching,{heading.x*.1,0,heading.z*.1},.35,0);
    }
    require(std::hypot(64.0-approaching.x,approaching.z) < .5,
            "local steering gets a ground enemy around a wall to its target");
    ian::EnemySystem enemies;
    enemies.spawnGroup(std::array{ian::EnemySpawn{
        ian::EnemyType::Basic,{57.0,.8,0.0}}});
    ian::FlowField noCoreFlow;
    const ian::EnemyNavigationView navigation{.collisionWorld=&collision};
    for (int tick = 0; tick < 1800; ++tick)
        static_cast<void>(enemies.tick(1.0/60.0,{},noCoreFlow,
            ian::Vec3{64.0,1.7,0.0},{},nullptr,navigation,true));
    require(enemies.enemies().front().position.x > 61.0,
            "enemy AI pursues the player around new scenery outside navigation bounds");
    const ian::WorldSceneryInstance canopy{
        .model=ian::WorldSceneryModel::Tent,.position={60.0,0.0,0.0}};
    collision.syncWorldScenery(ian::worldScenerySolidBoxes(canopy));
    requireNear(collision.moveCircle({57.0,1.7,0.0},{6.0,0.0,0.0},.35).x,63.0,1e-9,
                "open canopy permits walking between posts");
    collision.reset();
    requireNear(collision.moveCircle({57.0,1.7,0.0},{6.0,0.0,0.0},.35).x,63.0,1e-9,
                "collision reset removes old scenery");
}
