#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "combat/TowerSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "navigation/FlowField.hpp"

#include <array>
#include <cmath>

void runTowerSystemTests() {
    requireNear(
        ian::TowerSystem::fireInterval(
            ian::BuildingType::Turret, 1),
        1.25, 1e-9,
        "crossbow trades fire rate for piercing damage");
    require(
        ian::TowerSystem::piercingCount(
            ian::BuildingType::Turret, 1) == 2 &&
            ian::TowerSystem::piercingCount(
                ian::BuildingType::Turret, 3) == 3 &&
            ian::TowerSystem::piercingCount(
                ian::BuildingType::Turret, 8) == 5 &&
            ian::TowerSystem::piercingCount(
                ian::BuildingType::GunTurret, 8) == 0,
        "crossbow gains one piercing target every two levels");
    ian::BuildingSystem buildings;
    require(buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0).has_value(),
            "tower fixture creates core");
    const auto turret = buildings.place(ian::BuildingType::Turret, {0, -4}, 0, 25, 15);
    require(turret.has_value(), "tower fixture creates turret");
    const auto core = buildings.core();
    require(core.has_value(), "tower fixture finds core");
    require(buildings.upgrade(core->id, 0, 0, 50).valid(),
            "tower fixture upgrades core");
    const ian::ResourceCost turretBlueprintCost =
        buildings.blueprintUpgradeCost(ian::BuildingType::Turret);
    require(buildings.upgradeBlueprint(
                ian::BuildingType::Turret,
                turretBlueprintCost.wood,
                turretBlueprintCost.stone,
                turretBlueprintCost.crystals).valid(),
            "tower fixture upgrades turret");

    ian::FlowField flow;
    flow.rebuild({0, 0}, buildings.buildings());
    ian::EnemySystem enemies;
    constexpr std::array<ian::Vec3, 1> Spawn{{{0.0, 0.8, -8.0}}};
    enemies.spawnWave(Spawn);
    enemies.tick(1.0 / 60.0, buildings.buildings(), flow);

    ian::TowerSystem towers;
    towers.syncBuildings(buildings.buildings());
    require(towers.towers().size() == 1, "tower runtime follows turret building");

    int shotCount = 0;
    bool killed = false;
    for (int tick = 0; tick < 600 && !killed; ++tick) {
        const auto shots = towers.tick(1.0 / 60.0, buildings.buildings(), enemies);
        shotCount += static_cast<int>(shots.size());
        for (const auto& shot : shots) {
            killed = killed || shot.killed;
        }
    }
    require(shotCount >= 4, "level-two turret deals gradual upgraded damage");
    require(killed, "turret can kill target");
    require(enemies.activeCount() == 0, "tower damage updates enemy system");

    ian::BuildingSystem piercingBuildings;
    require(piercingBuildings.place(
                ian::BuildingType::Core, {0, 0}, 0,
                30, 0).has_value(),
            "piercing fixture creates core");
    require(piercingBuildings.place(
                ian::BuildingType::Turret, {0, -4}, 0,
                25, 15).has_value(),
            "piercing fixture creates crossbow");
    constexpr std::array<ian::EnemySpawn, 3> PiercingSpawn{{
        {.type = ian::EnemyType::Basic,
         .position = {0.0, 0.0, -6.0}},
        {.type = ian::EnemyType::Basic,
         .position = {0.0, 0.0, -7.0}},
        {.type = ian::EnemyType::Basic,
         .position = {0.0, 0.0, -8.0}},
    }};
    ian::EnemySystem piercingEnemies;
    piercingEnemies.spawnWave(PiercingSpawn);
    ian::TowerSystem piercingTowers;
    piercingTowers.syncBuildings(piercingBuildings.buildings());
    std::span<const ian::TowerShot> piercingShots;
    for (int tick = 0; tick < 120 && piercingShots.empty(); ++tick) {
        piercingShots = piercingTowers.tick(
            1.0 / 60.0, piercingBuildings.buildings(),
            piercingEnemies);
    }
    require(
        piercingShots.size() == 3 &&
            piercingShots[0].targetId != piercingShots[1].targetId &&
            piercingShots[1].targetId != piercingShots[2].targetId,
        "level-one crossbow arrow damages its target and two enemies behind it");
    require(
        piercingShots[0].secondaryImpact &&
            piercingShots[1].secondaryImpact &&
            !piercingShots[2].secondaryImpact,
        "one visual arrow travels through all piercing impacts to the final target");

    constexpr std::array<ian::EnemySpawn, 1> FlyingSpawn{{{
        .type = ian::EnemyType::Flying,
        .position = {0.0, 0.0, -8.0},
    }}};
    enemies.spawnWave(FlyingSpawn);
    towers.tick(1.0 / 60.0, buildings.buildings(), enemies);
    towers.tick(1.0 / 60.0, buildings.buildings(), enemies);
    require(towers.towers().front().pitch > 0.05,
            "crossbow pitches upward to track flying enemies");

    ian::BuildingSystem gunBuildings;
    const auto gunCore = gunBuildings.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(gunCore.has_value(),
            "gun turret fixture creates core");
    require(gunBuildings.upgrade(
                gunCore->building.id, 0, 0, 50).valid(),
            "gun turret fixture reaches required core level");
    const auto gunTurret = gunBuildings.place(
        ian::BuildingType::GunTurret, {0, -4}, 0, 35, 25, 10);
    require(gunTurret.has_value(), "2x2 gun turret can be placed at authored scale");
    requireNear(ian::buildingFootprintHalfExtent(
                    ian::BuildingType::GunTurret),
                1.0, 1e-9,
                "gun turret reserves a 2x2 placement footprint");
    ian::FlowField gunFlow;
    gunFlow.rebuild({0, 0}, gunBuildings.buildings());
    ian::EnemySystem gunEnemies;
    gunEnemies.spawnWave(Spawn);
    gunEnemies.tick(1.0 / 60.0, gunBuildings.buildings(), gunFlow);
    ian::TowerSystem gunTowers;
    gunTowers.syncBuildings(gunBuildings.buildings());
    require(gunTowers.towers().size() == 1 &&
                std::abs(gunTowers.towers().front().restYaw) < 1e-9,
            "placement rotation becomes the yaw-sector origin");
    requireNear(
        ian::buildingRotationYaw(
            ian::BuildingType::GunTurret, 3),
        ian::PiRadians * 0.75, 1e-9,
        "directional defenses rotate in 45-degree steps");
    requireNear(
        ian::defenseAttackArcDegrees(1), 90.0, 1e-9,
        "level-one defenses begin with a 90-degree arc");
    requireNear(
        ian::defenseAttackArcDegrees(8), 160.0, 1e-9,
        "defense upgrades widen the arc to 160 degrees");
    require(
        ian::directionInsideDefenseArc(
            {}, {0.0, 0.0, -4.0}, 0.0, 1) &&
            !ian::directionInsideDefenseArc(
                {}, {0.0, 0.0, 4.0}, 0.0, 8),
        "directional defenses cannot attack through their rear blind spot");
    std::array<int, 2> muzzles{-1, -1};
    int gunShots = 0;
    for (int tick = 0; tick < 180 && gunShots < 2; ++tick) {
        for (const auto& shot : gunTowers.tick(
                 1.0 / 60.0, gunBuildings.buildings(), gunEnemies)) {
            muzzles[static_cast<std::size_t>(gunShots++)] =
                static_cast<int>(shot.muzzleIndex);
            if (gunShots == 2) break;
        }
    }
    require(muzzles[0] == 0 && muzzles[1] == 1,
            "gun turret must alternate authored muzzle sockets");

    ian::EnemySystem twinBatteryEnemies;
    constexpr std::array<ian::EnemySpawn, 3> TwinBatterySpawn{{
        {.type = ian::EnemyType::Basic,
         .position = {0.0, 0.0, -8.0}},
        {.type = ian::EnemyType::Basic,
         .position = {-1.8, 0.0, -8.0}},
        {.type = ian::EnemyType::Basic,
         .position = {1.8, 0.0, -8.0}},
    }};
    twinBatteryEnemies.spawnWave(TwinBatterySpawn);
    ian::TowerSystem twinBatteryTowers;
    twinBatteryTowers.setSkillModifiers(
        1.0, 1.0, 1.0, 1.0, 1);
    twinBatteryTowers.syncBuildings(gunBuildings.buildings());
    std::span<const ian::TowerShot> twinShots;
    for (int tick = 0; tick < 120 && twinShots.empty(); ++tick) {
        twinShots = twinBatteryTowers.tick(
            1.0 / 60.0, gunBuildings.buildings(),
            twinBatteryEnemies);
    }
    require(
        twinShots.size() == 2U &&
            twinShots[0].targetId != twinShots[1].targetId &&
            !twinShots[1].secondaryImpact,
        "Twin Batteries creates a visible shot toward another target");

    ian::EnemySystem flyingGunEnemies;
    flyingGunEnemies.spawnWave(FlyingSpawn);
    ian::TowerSystem antiAirRestrictedGun;
    antiAirRestrictedGun.syncBuildings(gunBuildings.buildings());
    int flyingShots = 0;
    for (int tick = 0; tick < 120; ++tick) {
        flyingShots += static_cast<int>(antiAirRestrictedGun.tick(
            1.0 / 60.0, gunBuildings.buildings(),
            flyingGunEnemies).size());
    }
    require(flyingShots == 0 &&
                !antiAirRestrictedGun.towers().front().targetId,
            "gun turret must ignore flying enemies");

    ian::BuildingSystem elevatedGunBuildings;
    const auto elevatedGunCore = elevatedGunBuildings.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(elevatedGunCore.has_value(),
            "elevated gun turret fixture creates core");
    require(elevatedGunBuildings.upgrade(
                elevatedGunCore->building.id, 0, 0, 50).valid(),
            "elevated gun turret fixture reaches required core level");
    require(elevatedGunBuildings.place(
                ian::BuildingType::GunTurret, {0, -4}, 0,
                35, 25, 10, 4.0, 1).has_value(),
            "gun turret can be mounted on an upper storey");
    ian::EnemySystem lowGroundEnemies;
    lowGroundEnemies.spawnWave(Spawn);
    ian::TowerSystem elevatedGunTowers;
    elevatedGunTowers.syncBuildings(
        elevatedGunBuildings.buildings());
    int downwardShots = 0;
    for (int tick = 0; tick < 120; ++tick) {
        downwardShots += static_cast<int>(
            elevatedGunTowers.tick(
                1.0 / 60.0,
                elevatedGunBuildings.buildings(),
                lowGroundEnemies).size());
    }
    require(downwardShots == 0 &&
                !elevatedGunTowers.towers().front().targetId,
            "gun turret cannot target enemies on a lower storey");

    const double yawBeforeRotation =
        gunTowers.towers().front().yaw;
    const double baseYawBeforeRotation =
        gunTowers.towers().front().baseYaw;
    require(
        gunBuildings.rotateDirectionalDefense(
            gunTurret->building.id, 1).has_value(),
        "placed gun turret can rotate");
    gunTowers.syncBuildings(gunBuildings.buildings());
    requireNear(
        gunTowers.towers().front().yaw, yawBeforeRotation,
        1e-9,
        "post-placement rotation does not snap visual yaw");
    requireNear(
        gunTowers.towers().front().baseYaw,
        baseYawBeforeRotation,
        1e-9,
        "post-placement rotation does not snap gun turret base");
    gunTowers.tick(
        1.0 / 60.0, gunBuildings.buildings(), gunEnemies);
    const double smoothedYaw =
        gunTowers.towers().front().yaw;
    require(
        smoothedYaw > yawBeforeRotation &&
            smoothedYaw < ian::PiRadians * 0.25,
        "post-placement rotation eases toward its new yaw");
    const double expectedBaseYaw = ian::smoothBuildingAngle(
        baseYawBeforeRotation, ian::PiRadians * 0.25,
        1.0 / 60.0);
    requireNear(
        gunTowers.towers().front().baseYaw,
        expectedBaseYaw, 1e-9,
        "gun turret base reuses the shared rotation easing");
}
