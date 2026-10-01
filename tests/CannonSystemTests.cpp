#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "combat/CannonSystem.hpp"
#include "enemies/EnemySystem.hpp"

#include <array>

void runCannonSystemTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "cannon fixture creates core");
    buildings.upgrade(core->building.id, 0, 0, 50);
    buildings.upgrade(core->building.id, 0, 0, 100);
    buildings.upgrade(core->building.id, 0, 0, 150);
    const auto cannon = buildings.place(ian::BuildingType::Cannon, {0, -4}, 0, 40, 30, 25);
    require(cannon.has_value(), "cannon fixture creates cannon");
    const ian::ResourceCost cannonBlueprintCost =
        buildings.blueprintUpgradeCost(ian::BuildingType::Cannon);
    require(buildings.upgradeBlueprint(
                ian::BuildingType::Cannon,
                cannonBlueprintCost.wood,
                cannonBlueprintCost.stone,
                cannonBlueprintCost.crystals).valid(),
            "cannon fixture upgrades cannon");

    ian::EnemySystem enemies;
    constexpr std::array<ian::Vec3, 4> Cluster{{
        {0.0, 0.8, -6.0},
        {-0.6, 0.8, -10.0},
        {0.0, 0.8, -10.0},
        {0.6, 0.8, -10.0},
    }};
    enemies.spawnWave(Cluster);

    ian::CannonSystem cannons;
    cannons.setTypeBonuses(ian::BuildingType::Cannon, 1.3, 1.5);
    cannons.syncBuildings(buildings.buildings());
    cannons.tick(1.0 / 60.0, buildings.buildings(), enemies);
    requireNear(cannons.projectiles().front().explosionDamage, 3.9, 1e-12,
        "starting cannon perk scales actual projectile damage");
    requireNear(cannons.cannons().front().fireCooldownRemaining,
        ian::CannonSystem::fireInterval(ian::BuildingType::Cannon, 2) / 1.5, 1e-12,
        "type-specific fire rate scales cannon cooldown");
    const ian::EntityId firstRunProjectile =
        cannons.projectiles().front().id;
    cannons.reset();
    cannons.syncBuildings(buildings.buildings());
    cannons.tick(1.0 / 60.0, buildings.buildings(), enemies);
    require(!cannons.projectiles().empty() &&
                cannons.projectiles().front().id !=
                    firstRunProjectile &&
                cannons.projectiles().front().explosionDamage == 2.85 &&
                cannons.projectiles().front().explosionRadius == 2.62,
            "cannon reset preserves ID safety and upgraded shell stats");
    bool exploded = false;
    for (int tick = 0; tick < 179 && !exploded; ++tick) {
        const auto explosions = cannons.tick(1.0 / 60.0, buildings.buildings(), enemies);
        if (!explosions.empty()) {
            exploded = true;
            require(explosions.front().hitCount == 3, "explosion damages clustered enemies");
            require(explosions.front().killedCount == 0,
                    "weaker cannon does not erase healthy enemy cluster");
        }
    }
    require(exploded, "physical cannon projectile reaches target");
    require(enemies.activeCount() == 4,
            "cannon targets dense group instead of closer isolated enemy");

    ian::EnemySystem clusterPayloadEnemies;
    clusterPayloadEnemies.spawnWave(Cluster);
    ian::CannonSystem clusterPayloadCannons;
    clusterPayloadCannons.setModifiers(
        1.0, 1.0, 1.0, 1.0, 2);
    clusterPayloadCannons.syncBuildings(buildings.buildings());
    std::span<const ian::CannonExplosion> clusterExplosions;
    for (int tick = 0;
         tick < 240 && clusterExplosions.empty(); ++tick) {
        clusterExplosions = clusterPayloadCannons.tick(
            1.0 / 60.0, buildings.buildings(),
            clusterPayloadEnemies);
    }
    require(
        clusterExplosions.size() == 3U &&
            clusterExplosions[1].radius <
                clusterExplosions[0].radius,
        "Cluster Payload adds two smaller secondary explosions");

    // Actual launch payloads, cooldowns and impacts for each specialization.
    for (auto style : {ian::CannonAttackStyle::Heavy, ian::CannonAttackStyle::Scatter,
                       ian::CannonAttackStyle::Cluster}) {
        ian::EnemySystem targets;
        targets.spawnWave(Cluster);
        ian::CannonSystem specialized;
        specialized.setAttackStyle(style);
        specialized.setTypeBonuses(ian::BuildingType::Cannon, 1.3, 1.5);
        specialized.setModifiers(1.2, 1.1, 1.4, 1.0, 2);
        specialized.syncBuildings(buildings.buildings());
        specialized.tick(1.0 / 60.0, buildings.buildings(), targets);
        const auto profile = ian::cannonAttackProfile(style);
        require(specialized.projectiles().size() == static_cast<std::size_t>(profile.projectileCount),
            "specialization emits the expected shell volley");
        require(specialized.shots().size() == 1, "scatter volley emits one fire sound and shot event");
        requireNear(specialized.cannons().front().fireCooldownRemaining,
            ian::CannonSystem::fireInterval(2) * profile.interval / (1.5 * 1.4), 1e-12,
            "specialization cooldown combines with class and global fire rate");
        for (const auto& shell : specialized.projectiles()) {
            requireNear(shell.explosionDamage, 3.0 * 1.3 * 1.2 * profile.damage, 1e-12,
                "specialization damage combines with class and global damage");
            requireNear(shell.explosionRadius, 2.6 * 1.1 * profile.radius, 1e-12,
                "scatter shells have smaller actual explosion radii");
            require(shell.clusterExplosionCount == 2 + profile.clusterCount &&
                    shell.visualScale == profile.visualScale,
                "shell captures its cluster payload and visible size");
        }
        if (style == ian::CannonAttackStyle::Scatter) {
            require(specialized.projectiles()[0].targetPosition.x != specialized.projectiles()[1].targetPosition.x ||
                specialized.projectiles()[0].targetPosition.z != specialized.projectiles()[1].targetPosition.z,
                "scatter pellets use different ballistic trajectories");
        }
        // Changing runtime modifiers cannot rewrite shells already in flight.
        specialized.setAttackStyle(ian::CannonAttackStyle::Standard);
        specialized.setModifiers(1.0, 1.0, 1.0, 1.0, 0);
        int totalExplosions = 0;
        for (int tick = 0; tick < 90; ++tick) {
            totalExplosions += static_cast<int>(specialized.tick(
                1.0 / 60.0, buildings.buildings(), targets).size());
        }
        require(totalExplosions == profile.projectileCount * (1 + 2 + profile.clusterCount),
            "every shell produces its captured main and secondary explosions");
        specialized.reset();
        specialized.syncBuildings(buildings.buildings());
        targets.reset();
        targets.spawnWave(Cluster);
        specialized.tick(1.0 / 60.0, buildings.buildings(), targets);
        require(specialized.projectiles().size() == 1 && specialized.projectiles()[0].visualScale == 1.0,
            "new run resets specialization to a standard single shell");
    }

    ian::EnemySystem angledEnemies;
    constexpr std::array<ian::Vec3, 3> AngledCluster{{
        {5.0, 0.8, -8.0},
        {5.4, 0.8, -8.0},
        {5.0, 0.8, -8.4},
    }};
    angledEnemies.spawnWave(AngledCluster);
    ian::CannonSystem aimingCannons;
    aimingCannons.syncBuildings(buildings.buildings());
    aimingCannons.tick(1.0 / 60.0, buildings.buildings(), angledEnemies);
    aimingCannons.tick(1.0 / 60.0, buildings.buildings(), angledEnemies);
    require(aimingCannons.cannons().front().yaw < 0.0,
            "cannon turns toward positive X using raylib Y-axis convention");
    require(aimingCannons.cannons().front().pitch > 0.0,
            "cannon barrel raises for ballistic trajectory");

    const std::vector<ian::BuildingInstance> catapultBuildings{{
        .id = {900, 1},
        .type = ian::BuildingType::Catapult,
        .gridPosition = {0, 0},
        .health = 210.0,
        .maxHealth = 210.0,
    }};
    ian::EnemySystem catapultEnemies;
    constexpr std::array<ian::Vec3, 4> CatapultTargets{{
        {0.0, 0.8, -2.5},
        {-0.5, 0.8, -8.0},
        {0.0, 0.8, -8.0},
        {0.5, 0.8, -8.0},
    }};
    catapultEnemies.spawnWave(CatapultTargets);
    ian::CannonSystem catapults;
    catapults.setAttackStyle(ian::CannonAttackStyle::Scatter);
    catapults.syncBuildings(catapultBuildings);
    catapults.tick(1.0 / 60.0, catapultBuildings, catapultEnemies);
    require(catapults.projectiles().empty() &&
                catapults.cannons().front().loaded,
            "catapult keeps its ball in the cup during windup");
    for (int tick = 0; tick < 12; ++tick) {
        catapults.tick(1.0 / 60.0, catapultBuildings,
                       catapultEnemies);
    }
    require(catapults.cannons().front().pitch < -0.1 &&
                catapults.projectiles().empty(),
            "catapult arm pitches before releasing its ball");
    for (int tick = 0; tick < 20 &&
         catapults.projectiles().empty(); ++tick) {
        catapults.tick(1.0 / 60.0, catapultBuildings,
                       catapultEnemies);
    }
    require(catapults.projectiles().size() == 1 &&
                catapults.projectiles().front().explosionDamage == 4.0 &&
                catapults.projectiles().front().visualScale == 1.0 &&
                catapults.projectiles().front().type ==
                    ian::BuildingType::Catapult &&
                !catapults.cannons().front().loaded &&
                catapults.projectiles().front().targetPosition.z < -4.0,
            "catapult releases a ballistic ball beyond its dead zone");
}
