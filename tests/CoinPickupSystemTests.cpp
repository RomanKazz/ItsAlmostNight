#include "TestHarness.hpp"
#include "economy/CoinPickupSystem.hpp"
#include "world/CollisionWorld.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/WorldConfig.hpp"

#include <cmath>

void runCoinPickupSystemTests() {
    ian::WorldConfig config = ian::WorldConfig::defaults();
    config.terrainResolution = 33;
    config.terrainWorldSize = 32.0;
    config.coreFlatRadius = 8.0;
    ian::TerrainHeightfield terrain{config};
    ian::CollisionWorld collision;
    ian::CoinPickupSystem coins;

    coins.spawn({0.0, 0.0, 0.0}, 3, 42U, terrain);
    require(coins.pickups().size() == 3,
            "enemy reward spawns one visible pickup per coin");

    int collected = 0;
    for (int frame = 0; frame < 60; ++frame) {
        collected += coins.tick(
            1.0 / 60.0, {12.0, 1.7, 0.0}, terrain,
            collision).value;
    }
    require(collected == 0 && coins.pickups().size() == 3,
            "coins wait in the world outside attraction radius");
    for (const ian::CoinPickup& coin : coins.pickups()) {
        require(
            std::isfinite(coin.position.x) &&
                std::isfinite(coin.position.y) &&
                std::isfinite(coin.position.z),
            "coin bounce remains finite");
    }

    bool magnetized = false;
    for (int frame = 0; frame < 180 && !coins.pickups().empty(); ++frame) {
        const ian::CoinCollection result = coins.tick(
            1.0 / 60.0, {0.0, 1.7, 0.0}, terrain,
            collision);
        collected += result.value;
        for (const ian::CoinPickup& coin : coins.pickups()) {
            magnetized = magnetized || coin.magnetized;
        }
    }
    require(magnetized, "nearby coins enter magnetic flight");
    require(collected == 3 && coins.pickups().empty(),
            "magnetic coins reach player and grant their full value");

    coins.reset();
    coins.spawnValue({0.0, 0.0, 0.0}, 16, 123U, terrain);
    require(
        coins.pickups().size() == 3 &&
            coins.pickups()[0].type == ian::CoinType::Gold &&
            coins.pickups()[0].value == 10 &&
            coins.pickups()[1].type == ian::CoinType::Silver &&
            coins.pickups()[1].value == 5 &&
            coins.pickups()[2].type == ian::CoinType::Bronze &&
            coins.pickups()[2].value == 1,
        "coin rewards decompose into gold, silver and bronze values");

    coins.reset();
    coins.spawnValue({0.0, 0.0, 0.0}, 10, 314U, terrain);
    for (int frame = 0; frame < 30; ++frame) {
        static_cast<void>(coins.tick(
            1.0 / 60.0, {0.0, 1.7, 0.0}, terrain, collision));
    }
    const ian::CoinPickup& burstingCoin = coins.pickups().front();
    require(
        !burstingCoin.magnetized &&
            std::hypot(
                burstingCoin.position.x,
                burstingCoin.position.z) > 0.5,
        "new coins burst outward before player magnet attraction begins");

    coins.reset();
    coins.spawnValue({0.0, 0.0, 0.0}, 10, 515U, terrain, 1.0);
    const double fullSpread = std::hypot(
        coins.pickups().front().velocity.x,
        coins.pickups().front().velocity.z);
    coins.reset();
    coins.spawnValue({0.0, 0.0, 0.0}, 10, 515U, terrain, 0.5);
    const double propSpread = std::hypot(
        coins.pickups().front().velocity.x,
        coins.pickups().front().velocity.z);
    requireNear(
        propSpread, fullSpread * 0.5, 1e-12,
        "destructible prop coins use half lateral burst distance");

    coins.reset();
    coins.spawn(
        {0.0, 0.0, 0.0},
        static_cast<int>(ian::CoinPickupSystem::MaximumPickups + 20U),
        7U, terrain);
    require(
        coins.pickups().size() ==
            ian::CoinPickupSystem::MaximumPickups,
        "coin population is capped for large enemy waves");

    std::vector<ian::PlatformFrameInstance> elevatedFrames;
    std::uint32_t frameId = 1;
    for (int z = -4; z <= 2; z += 2) {
        for (int x = -4; x <= 2; x += 2) {
            elevatedFrames.push_back({
                .id = {frameId++, 1U},
                .anchor = {x, 0, z},
                .floorHeight = 3.0,
                .storey = 0,
            });
        }
    }
    collision.syncModularBuildings({
        elevatedFrames, {}, {}, 1.0,
    });
    coins.reset();
    coins.spawn({0.0, 3.0, 0.0}, 1, 91U, terrain);
    for (int frame = 0; frame < 180; ++frame) {
        static_cast<void>(coins.tick(
            1.0 / 60.0, {20.0, 1.7, 20.0}, terrain,
            collision));
    }
    require(
        coins.pickups().size() == 1,
        "elevated platform coin remains in the world");
    require(
        coins.pickups().front().position.y >= 3.29,
        "falling coins land on elevated modular platforms");
}
