#include "TestHarness.hpp"
#include "economy/CoinPickupSystem.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/WorldConfig.hpp"

#include <cmath>

void runCoinPickupSystemTests() {
    ian::WorldConfig config = ian::WorldConfig::defaults();
    config.terrainResolution = 33;
    config.terrainWorldSize = 32.0;
    config.coreFlatRadius = 8.0;
    ian::TerrainHeightfield terrain{config};
    ian::CoinPickupSystem coins;

    coins.spawn({0.0, 0.0, 0.0}, 3, 42U, terrain);
    require(coins.pickups().size() == 3,
            "enemy reward spawns one visible pickup per coin");

    int collected = 0;
    for (int frame = 0; frame < 60; ++frame) {
        collected += coins.tick(
            1.0 / 60.0, {12.0, 1.7, 0.0}, terrain).value;
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
            1.0 / 60.0, {0.0, 1.7, 0.0}, terrain);
        collected += result.value;
        for (const ian::CoinPickup& coin : coins.pickups()) {
            magnetized = magnetized || coin.magnetized;
        }
    }
    require(magnetized, "nearby coins enter magnetic flight");
    require(collected == 3 && coins.pickups().empty(),
            "magnetic coins reach player and grant their full value");

    coins.reset();
    coins.spawn(
        {0.0, 0.0, 0.0},
        static_cast<int>(ian::CoinPickupSystem::MaximumPickups + 20U),
        7U, terrain);
    require(
        coins.pickups().size() ==
            ian::CoinPickupSystem::MaximumPickups,
        "coin population is capped for large enemy waves");
}
