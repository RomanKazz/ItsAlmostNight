#include "TestHarness.hpp"
#include "economy/CoinPickupSystem.hpp"
#include "world/CollisionWorld.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/WorldConfig.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>

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

    coins.reset();
    coins.spawnValue({0.0, 0.0, 0.0}, 1, 43U, terrain, 0.0);
    int longRangeCollected = 0;
    for (int frame = 0; frame < 240 && !coins.pickups().empty(); ++frame) {
        longRangeCollected += coins.tick(
            1.0 / 60.0, {10.0, 1.7, 0.0}, terrain,
            collision, 0.0, 11.0).value;
    }
    require(longRangeCollected == 1 && coins.pickups().empty(),
            "upgraded attraction radius pulls distant pickups");

    coins.reset();
    coins.spawn({0.0, 0.0, 0.0}, 3, 42U, terrain);
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
    coins.spawnValue({0.0, 0.0, 0.0}, 1, 81U, terrain, 0.0);
    bool recoiledBeforePull = false;
    for (int frame = 0; frame < 90 && !coins.pickups().empty(); ++frame) {
        static_cast<void>(coins.tick(
            1.0 / 60.0, {1.5, 1.7, 0.0}, terrain, collision));
        if (!coins.pickups().empty() &&
            coins.pickups().front().magnetized &&
            coins.pickups().front().magnetTime <
                ian::CoinPickupSystem::MagnetAnticipationDuration) {
            const ian::CoinPickup& coin = coins.pickups().front();
            const ian::Vec3 toPlayer{
                1.5 - coin.position.x,
                1.7 - 0.72 - coin.position.y,
                -coin.position.z,
            };
            recoiledBeforePull =
                coin.velocity.x * toPlayer.x +
                    coin.velocity.z * toPlayer.z < 0.0;
            break;
        }
    }
    require(
        recoiledBeforePull,
        "magnetized coins recoil away before flying to the player");

    coins.reset();
    coins.spawnValue({0.0, 0.0, 0.0}, 16, 123U, terrain);
    require(
        coins.pickups().size() == 3 &&
            coins.pickups()[0].type == ian::CoinType::HighValue &&
            coins.pickups()[0].value == 10 &&
            coins.pickups()[1].type == ian::CoinType::Silver &&
            coins.pickups()[1].value == 5 &&
            coins.pickups()[2].type == ian::CoinType::Bronze &&
            coins.pickups()[2].value == 1,
        "coin rewards decompose into crystals, silver and bronze values");

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
    require(coins.spawnExperience(
        {0.0, 0.0, 0.0}, 10, 4, 616U, terrain, 0.0) == 10,
        "experience reward is accepted by the pickup system");
    require(coins.pickups().size() == 4 &&
            std::ranges::all_of(coins.pickups(),
                [](const ian::CoinPickup& pickup) {
                    return pickup.kind == ian::PickupKind::Experience;
                }),
        "experience reward bursts into several separate orbs");
    int experienceCollected = 0;
    int orbCount = 0;
    for (int frame = 0; frame < 240 && !coins.pickups().empty(); ++frame) {
        const ian::CoinCollection result = coins.tick(
            1.0 / 60.0, {0.0, 1.7, 0.0}, terrain, collision);
        experienceCollected += result.experience;
        orbCount += result.experienceCount;
        require(result.value == 0,
            "experience collection does not award coins");
    }
    require(experienceCollected == 10 && orbCount == 4 &&
            coins.pickups().empty(),
        "magnet collects the full split experience reward");

    coins.reset();
    require(coins.spawnExperience(
        {0.0, 0.0, 2.0}, 8, 8, 617U, terrain) == 8,
        "experience burst spawns every crystal");
    require(std::ranges::all_of(coins.pickups(),
            [](const ian::CoinPickup& pickup) {
                return pickup.position.x == 0.0 &&
                    pickup.position.z == 2.0;
            }),
        "experience crystals spawn at the reward source");
    for (int frame = 0; frame < 30; ++frame) {
        static_cast<void>(coins.tick(
            1.0 / 60.0, {20.0, 1.7, 20.0}, terrain, collision));
    }
    require(std::ranges::any_of(coins.pickups(),
                [](const ian::CoinPickup& pickup) {
                    return pickup.position.x < -0.3;
                }) &&
            std::ranges::any_of(coins.pickups(),
                [](const ian::CoinPickup& pickup) {
                    return pickup.position.x > 0.3;
                }),
        "experience crystals fan out to both sides of the source");
    require(std::ranges::any_of(coins.pickups(),
                [](const ian::CoinPickup& pickup) {
                    return pickup.position.z < 1.7;
                }) &&
            std::ranges::any_of(coins.pickups(),
                [](const ian::CoinPickup& pickup) {
                    return pickup.position.z > 2.3;
                }),
        "experience burst spreads independently of the player's view");
    require(std::ranges::all_of(coins.pickups(),
            [&](const ian::CoinPickup& pickup) {
                const double ground = terrain.getHeight(
                    pickup.position.x, pickup.position.z) + 0.30;
                return pickup.position.y <= ground + 0.12;
            }),
        "experience burst lands within half a second");

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

    coins.reset();
    coins.spawnHeart({0.0, 0.0, 0.0}, 177U, terrain);
    double healing = 0.0;
    for (int frame = 0; frame < 180; ++frame) {
        healing += coins.tick(
            1.0 / 60.0, {0.0, 1.7, 0.0}, terrain,
            collision, 0.0).healing;
    }
    require(
        healing == 0.0 && coins.pickups().size() == 1 &&
            coins.pickups().front().kind == ian::PickupKind::Heart &&
            !coins.pickups().front().magnetized,
        "heart stays on the ground and does not attract at full health");
    for (int frame = 0; frame < 240 && !coins.pickups().empty(); ++frame) {
        const ian::CoinCollection result = coins.tick(
            1.0 / 60.0, {0.0, 1.7, 0.0}, terrain,
            collision, 12.0 - healing);
        healing += result.healing;
    }
    requireNear(
        healing, 12.0, 1e-12,
        "heart attracts when health is missing and cannot overheal");
    require(coins.pickups().empty(),
            "collected heart leaves the pickup world");

    coins.spawnHeart({0.0, 0.0, 0.0}, 188U, terrain);
    for (int frame = 0; frame < 510; ++frame) {
        static_cast<void>(coins.tick(
            0.1, {0.0, 1.7, 0.0}, terrain,
            collision, 0.0));
    }
    require(coins.pickups().empty(),
            "unneeded heart expires after the coin pickup lifetime");
    require(coins.spawnMetaSilver({0,0,0},199,terrain),"gold crate can spawn a dedicated meta silver pickup");
    auto savedSilver=coins.metaSilverState();
    require(savedSilver.size()==1 && savedSilver.front().value==1 &&
            savedSilver.front().kind==ian::PickupKind::MetaSilver,
            "one silver is distinct from five-value run coins");
    ian::CoinPickupSystem resumedSilver;
    require(resumedSilver.loadMetaSilverState(savedSilver),"uncollected silver restores after save/load");
    for(int frame=0;frame<510;++frame) static_cast<void>(resumedSilver.tick(.1,{15,1.7,15},terrain,collision));
    require(resumedSilver.pickups().size()==1,"meta silver does not expire like ordinary loot");
    int silver=0,gold=0;
    for(int frame=0;frame<240 && silver==0;++frame) {
        const auto reward=resumedSilver.tick(1.0/60,{0,1.7,0},terrain,collision);
        silver+=reward.silver;gold+=reward.value;
    }
    require(silver==1 && gold==0 && resumedSilver.pickups().empty(),"silver collection never credits the run coin balance");
    savedSilver.push_back(savedSilver.front());
    require(!resumedSilver.loadMetaSilverState(savedSilver),"duplicate silver pickup IDs are rejected");
}
