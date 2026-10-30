#include "TestHarness.hpp"
#include "DefenseIdleAimChecks.hpp"
#include "buildings/BuildingSystem.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "combat/TowerSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "navigation/FlowField.hpp"

#include <array>
#include <algorithm>
#include <cmath>

void runTowerSystemTests() {
    defense_idle_aim_checks::towers();
    requireNear(
        ian::TowerSystem::fireInterval(
            ian::BuildingType::Turret, 1),
        1.0, 1e-9,
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
    const auto placedCore = buildings.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(placedCore.has_value(),
            "tower fixture creates core");
    require(buildings.upgrade(
                placedCore->building.id, 0, 0, 50).valid() &&
                buildings.upgrade(
                    placedCore->building.id, 0, 0, 100).valid(),
            "tower fixture unlocks crossbow");
    const auto turret = buildings.place(ian::BuildingType::Turret, {0, -4}, 0, 25, 15, 5);
    require(turret.has_value(), "tower fixture creates turret");
    const ian::ResourceCost turretUpgradeCost =
        buildings.upgradeCost(turret->building);
    require(buildings.upgrade(
                turret->building.id,
                turretUpgradeCost.wood,
                turretUpgradeCost.stone,
                turretUpgradeCost.crystals).valid(),
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

    {
        ian::EnemySystem perkEnemies;
        perkEnemies.spawnWave(Spawn);
        ian::TowerSystem perkTowers;
        perkTowers.setTypeBonuses(ian::BuildingType::Turret, 1.25, 1.5);
        perkTowers.syncBuildings(buildings.buildings());
        std::span<const ian::TowerShot> perkShots;
        for (int tick = 0; tick < 120 && perkShots.empty(); ++tick) {
            perkShots = perkTowers.tick(1.0 / 60.0, buildings.buildings(), perkEnemies);
        }
        require(!perkShots.empty(), "character tower fires a real shot");
        requireNear(perkShots.front().payloadDamage,
            ian::TowerSystem::attackDamage(ian::BuildingType::Turret, 2) * 1.25, 1e-12,
            "tower character perk changes payload damage");
        requireNear(perkTowers.finalStats(buildings.buildings().back()).attackInterval,
            ian::TowerSystem::fireInterval(ian::BuildingType::Turret, 2) / 1.5, 1e-12,
            "tower character perk changes cooldown");
        perkTowers.reset();
        perkEnemies.reset();
        perkEnemies.spawnWave(Spawn);
        perkTowers.setTypeBonuses(ian::BuildingType::GunTurret, 2.0, 2.0);
        perkTowers.syncBuildings(buildings.buildings());
        perkShots = {};
        for (int tick = 0; tick < 120 && perkShots.empty(); ++tick) {
            perkShots = perkTowers.tick(1.0 / 60.0, buildings.buildings(), perkEnemies);
        }
        require(!perkShots.empty(), "non-starter tower fires");
        requireNear(perkShots.front().payloadDamage,
            ian::TowerSystem::attackDamage(ian::BuildingType::Turret, 2), 1e-12,
            "starter perk does not affect another tower type");
    }

    int shotCount = 0;
    bool killed = false;
    for (int tick = 0; tick < 600 && !killed; ++tick) {
        const auto shots = towers.tick(1.0 / 60.0, buildings.buildings(), enemies);
        shotCount += static_cast<int>(shots.size());
        for (const auto& shot : shots) {
            killed = killed || shot.killed;
        }
    }
    require(shotCount >= 2, "level-two turret deals meaningful upgraded damage");
    require(killed, "turret can kill target");
    require(enemies.activeCount() == 0, "tower damage updates enemy system");

    ian::BuildingSystem piercingBuildings;
    const auto piercingCore = piercingBuildings.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(piercingCore.has_value(),
            "piercing fixture creates core");
    require(piercingBuildings.upgrade(
                piercingCore->building.id, 0, 0, 50).valid() &&
                piercingBuildings.upgrade(
                    piercingCore->building.id, 0, 0, 100).valid(),
            "piercing fixture unlocks crossbow");
    require(piercingBuildings.place(
                ian::BuildingType::Turret, {0, -4}, 0,
                25, 15, 5).has_value(),
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
    std::vector<ian::TowerShot> piercingShots;
    for (int tick = 0; tick < 120 && piercingShots.size() < 3; ++tick) {
        const auto hits = piercingTowers.tick(1.0 / 60.0, piercingBuildings.buildings(), piercingEnemies);
        piercingShots.insert(piercingShots.end(), hits.begin(), hits.end());
    }
    require(piercingShots.size() == 3 && piercingShots[0].targetId != piercingShots[1].targetId &&
        piercingShots[1].targetId != piercingShots[2].targetId,
        "crossbow bolt physically reaches its target and two enemies behind it");
    require(!piercingShots[0].secondaryImpact && piercingShots[1].secondaryImpact && piercingShots[2].secondaryImpact,
        "physical piercing marks subsequent contacts on the same bolt");

    constexpr std::array<ian::EnemySpawn, 1> FlyingSpawn{{{
        .type = ian::EnemyType::Flying,
        .position = {0.0, 0.0, -8.0},
    }}};
    enemies.spawnWave(FlyingSpawn);
    for (int tick = 0; tick < 90 && towers.towers().front().pitch <= 0.05; ++tick) {
        towers.tick(1.0 / 60.0, buildings.buildings(), enemies);
    }
    require(towers.towers().front().pitch > 0.05,
            "crossbow pitches upward to track flying enemies");

    for (const auto type : {ian::BuildingType::Turret, ian::BuildingType::GunTurret}) {
        std::vector<ian::BuildingInstance> registry{{.id = {901, 1}, .type = type}};
        ian::EnemySystem targets;
        constexpr std::array<ian::Vec3, 1> SideSpawn{{{4.0, 0.8, -7.0}}};
        targets.spawnWave(SideSpawn);
        ian::TowerSystem smoothTowers;
        smoothTowers.setModifiers(0.05, 1.0, 1.0, 1.0);
        smoothTowers.syncBuildings(registry);
        double firstTurn = 0.0;
        double peakTurn = 0.0;
        double lastTurn = 0.0;
        bool fired = false;
        for (int tick = 0; tick < 90; ++tick) {
            const double before = smoothTowers.towers().front().yaw;
            fired |= !smoothTowers.tick(1.0 / 60.0, registry, targets).empty();
            const auto& motion = smoothTowers.towers().front();
            lastTurn = std::abs(ian::wrapBuildingAngle(motion.yaw - before));
            if (tick == 0) firstTurn = lastTurn;
            peakTurn = std::max(peakTurn, lastTurn);
            requireNear(motion.previousYaw, before, 1e-12,
                        "physical tower rendering retains its previous fixed-step pose");
            require(lastTurn < 0.045,
                    "physical acquisition cannot produce a sudden yaw step");
        }
        require(firstTurn > 0.0 && firstTurn < 0.02 &&
                    peakTurn > firstTurn * 3.0 && lastTurn < firstTurn && fired,
                "both physical towers ease into aim, settle and still fire");
        targets.reset();
        const double aimedYaw = smoothTowers.towers().front().yaw;
        const double aimedPitch = smoothTowers.towers().front().pitch;
        smoothTowers.tick(1.0 / 60.0, registry, targets);
        require(std::abs(ian::wrapBuildingAngle(smoothTowers.towers().front().yaw - aimedYaw)) < 0.02,
                "target loss cannot abruptly spin a physical tower back to rest");
        for (int tick = 0; tick < 120; ++tick) {
            smoothTowers.tick(1.0 / 60.0, registry, targets);
        }
        requireNear(smoothTowers.towers().front().yaw, aimedYaw, 1e-12,
                    "idle physical tower retains its last yaw indefinitely");
        requireNear(smoothTowers.towers().front().pitch, aimedPitch, 1e-12,
                    "idle physical tower retains its last elevation indefinitely");
    }

    {
        std::vector<ian::BuildingInstance> registry{{
            .id = {902, 1}, .type = ian::BuildingType::Turret, .rotation = 1}};
        ian::TowerSystem crossbow;
        crossbow.setModifiers(0.05, 1.0, 1.0, 1.0);
        crossbow.syncBuildings(registry);
        requireNear(crossbow.towers().front().previousYaw,
                    crossbow.towers().front().yaw, 1e-12,
                    "new rotated crossbow starts with a matching interpolation pose");
        registry.front().rotation = 0;
        crossbow.syncBuildings(registry);
        ian::EnemySystem targets;
        constexpr std::array<ian::EnemySpawn, 1> HighTarget{{{
            .type = ian::EnemyType::Flying, .position = {-3.0, 4.0, -6.0}}}};
        targets.spawnWave(HighTarget);
        for (int frame = 0; frame < 180; ++frame) {
            const auto before = crossbow.towers().front();
            crossbow.tick(1.0 / 60.0, registry, targets);
            const auto moving = crossbow.towers().front();
            require(std::abs(moving.pitch - before.pitch) <= 2.8 / 60.0 + 1e-9,
                    "crossbow elevation respects its speed limit during acquisition");
            if (frame == 90) {
                crossbow.syncBuildings(registry);
                requireNear(crossbow.towers().front().previousPitch,
                            moving.previousPitch, 1e-12,
                            "building refresh preserves crossbow interpolation history");
                requireNear(crossbow.towers().front().pitchVelocity,
                            moving.pitchVelocity, 1e-12,
                            "building refresh preserves crossbow aiming velocity");
            }
        }
        require(crossbow.towers().front().pitch > 0.05,
                "crossbow settles toward elevated targets");
        targets.reset();
        constexpr std::array<ian::Vec3, 1> LowTarget{{{3.0, 0.0, -6.0}}};
        targets.spawnWave(LowTarget);
        for (int frame = 0; frame < 180; ++frame) {
            const auto before = crossbow.towers().front();
            crossbow.tick(1.0 / 60.0, registry, targets);
            const auto moving = crossbow.towers().front();
            require(std::abs(moving.pitch - before.pitch) <= 2.8 / 60.0 + 1e-9 &&
                    std::abs(ian::wrapBuildingAngle(moving.yaw - before.yaw)) <= 4.0 / 60.0 + 1e-9,
                    "crossbow changes yaw and elevation smoothly between targets");
        }
        targets.reset();
        const double heldPitch = crossbow.towers().front().pitch;
        for (int frame = 0; frame < 180; ++frame)
            crossbow.tick(1.0 / 60.0, registry, targets);
        requireNear(crossbow.towers().front().pitch, heldPitch, 1e-12,
                    "crossbow retains its last elevation after target loss");
    }

    ian::BuildingSystem gunBuildings;
    const auto gunCore = gunBuildings.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(gunCore.has_value(),
            "gun turret fixture creates core");
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

    ian::EnemySystem pierceEnemies;
    pierceEnemies.spawnWave(PiercingSpawn);
    ian::TowerSystem pierceTowers;
    // Isolate one projectile's pierce path from the new baseline burst.
    ian::AttackModifiers pierceModifiers;
    pierceModifiers[ian::AttackStat::Quantity].flat = -2;
    pierceModifiers[ian::AttackStat::Pierce].flat = 2;
    pierceTowers.setAttackModifiers(pierceModifiers);
    pierceTowers.syncBuildings(gunBuildings.buildings());
    std::vector<ian::TowerShot> pierceShots;
    for (int tick = 0; tick < 120 && pierceShots.size() < 3; ++tick) {
        const auto hits = pierceTowers.tick(1.0 / 60.0, gunBuildings.buildings(), pierceEnemies);
        pierceShots.insert(pierceShots.end(), hits.begin(), hits.end());
    }
    require(pierceShots.size() == 3 && pierceShots[0].targetId != pierceShots[1].targetId &&
        pierceShots[1].targetId != pierceShots[2].targetId && !pierceShots[0].secondaryImpact &&
        pierceShots[1].secondaryImpact && pierceShots[2].secondaryImpact,
        "gun piercing physically passes through three lined-up enemies");

    ian::EnemySystem twinBatteryEnemies;
    constexpr std::array<ian::EnemySpawn, 4> TwinBatterySpawn{{
        {.type = ian::EnemyType::Basic,
         .position = {0.0, 0.0, -8.0}},
        {.type = ian::EnemyType::Basic,
         .position = {-1.8, 0.0, -8.0}},
        {.type = ian::EnemyType::Basic,
         .position = {1.8, 0.0, -8.0}},
        {.type = ian::EnemyType::Basic,
         .position = {0.0, 0.0, -2.0}},
    }};
    twinBatteryEnemies.spawnWave(TwinBatterySpawn);
    ian::TowerSystem twinBatteryTowers;
    twinBatteryTowers.setModifiers(
        1.0, 1.0, 1.0, 1.0, 1);
    twinBatteryTowers.syncBuildings(gunBuildings.buildings());
    std::vector<ian::TowerShot> twinShots;
    for (int tick = 0; tick < 120 && twinShots.size() < 2; ++tick) {
        const auto hits = twinBatteryTowers.tick(1.0 / 60.0, gunBuildings.buildings(), twinBatteryEnemies);
        twinShots.insert(twinShots.end(), hits.begin(), hits.end());
    }
    require(twinShots.size() == 2 && twinShots[0].targetId == twinShots[1].targetId &&
        !twinShots[0].secondaryImpact && !twinShots[1].secondaryImpact,
        "Quantity adds a second full-damage burst bullet toward the same target");

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
    require(!gunTowers.towers().front().targetId,
        "rotation invalidates the old gun target");
    requireNear(gunTowers.towers().front().targetSearchCooldownRemaining,
        0.25, 1e-9, "rotated physical defenses retain their search interval");
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
