#include "TestHarness.hpp"
#include "DefenseIdleAimChecks.hpp"
#include "ArtilleryAirTargetChecks.hpp"
#include "buildings/BuildingSystem.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/CatapultRig.hpp"
#include "combat/CannonSystem.hpp"
#include "enemies/EnemySystem.hpp"

#include <array>
#include <algorithm>
#include <cmath>

namespace {
void tickUntilShot(ian::CannonSystem& cannons,
                   const std::vector<ian::BuildingInstance>& buildings,
                   ian::EnemySystem& enemies) {
    for (int tick = 0; tick < 120; ++tick) {
        cannons.tick(1.0 / 60.0, buildings, enemies);
        if (!cannons.shots().empty()) return;
    }
    require(false, "ballistic defense acquires and smoothly aims before firing");
}
} // namespace

void runCannonSystemTests() {
    defense_idle_aim_checks::ballistics();
    artillery_air_target_checks::run();
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "cannon fixture creates core");
    buildings.upgrade(core->building.id, 0, 0, 50);
    buildings.upgrade(core->building.id, 0, 0, 100);
    buildings.upgrade(core->building.id, 0, 0, 150);
    const auto cannon = buildings.place(ian::BuildingType::Cannon, {0, -4}, 0, 40, 30, 25);
    require(cannon.has_value(), "cannon fixture creates cannon");
    const ian::ResourceCost cannonUpgradeCost =
        buildings.upgradeCost(cannon->building);
    require(buildings.upgrade(
                cannon->building.id,
                cannonUpgradeCost.wood,
                cannonUpgradeCost.stone,
                cannonUpgradeCost.crystals).valid(),
            "cannon fixture upgrades cannon");

    ian::EnemySystem enemies;
    constexpr std::array<ian::Vec3, 4> Cluster{{
        {4.0, 0.8, -6.0},
        {-0.6, 0.8, -10.0},
        {0.0, 0.8, -10.0},
        {0.6, 0.8, -10.0},
    }};
    enemies.spawnWave(Cluster);

    ian::CannonSystem cannons;
    cannons.setTypeBonuses(ian::BuildingType::Cannon, 1.3, 1.5);
    cannons.syncBuildings(buildings.buildings());
    tickUntilShot(cannons, buildings.buildings(), enemies);
    requireNear(cannons.projectiles().front().explosionDamage, 3.9, 1e-12,
        "starting cannon perk scales actual projectile damage");
    requireNear(cannons.cannons().front().fireCooldownRemaining,
        ian::CannonSystem::fireInterval(ian::BuildingType::Cannon, 2) / 1.5, 1e-12,
        "type-specific fire rate scales cannon cooldown");
    const ian::EntityId firstRunProjectile =
        cannons.projectiles().front().id;
    cannons.reset();
    cannons.syncBuildings(buildings.buildings());
    tickUntilShot(cannons, buildings.buildings(), enemies);
    require(!cannons.projectiles().empty() &&
                cannons.projectiles().front().id !=
                    firstRunProjectile &&
                cannons.projectiles().front().explosionDamage == 3.0 &&
                cannons.projectiles().front().explosionRadius == 2.6,
            "cannon reset preserves ID safety and upgraded shell stats");
    require(cannons.projectiles().front().trailPointCount == 2U,
            "new cannon shell starts its trail at the muzzle with no stale history");
    bool observedCurvedTrail = false;
    bool observedWrappedTrail = false;
    bool exploded = false;
    for (int tick = 0; tick < 179 && !exploded; ++tick) {
        const auto explosions = cannons.tick(1.0 / 60.0, buildings.buildings(), enemies);
        const auto& shell = cannons.projectiles().front();
        if (shell.active) {
            require(shell.trailPointCount <= ian::CannonProjectile::TrailCapacity,
                    "cannon trail stays bounded during flight");
            const auto point = [&shell](std::size_t age) {
                return shell.trailPositions[
                    (shell.trailHead + ian::CannonProjectile::TrailCapacity - age) %
                    ian::CannonProjectile::TrailCapacity];
            };
            requireNear(point(0).y, shell.position.y, 1e-12,
                        "cannon trail head follows the physical shell");
            if (shell.trailPointCount >= 3U) {
                const double curvature = point(0).y - 2.0 * point(1).y + point(2).y;
                requireNear(curvature, -9.8 / (60.0 * 60.0), 1e-12,
                            "trail records the gravity-driven arc rather than a straight line");
                observedCurvedTrail = true;
            }
            observedWrappedTrail |= shell.trailPointCount == ian::CannonProjectile::TrailCapacity;
        }
        if (!explosions.empty()) {
            exploded = true;
            require(explosions.front().hitCount == 3, "explosion damages clustered enemies");
            require(explosions.front().killedCount == 0,
                    "weaker cannon does not erase healthy enemy cluster");
        }
    }
    require(exploded, "physical cannon projectile reaches target");
    require(observedCurvedTrail && observedWrappedTrail,
            "cannon flight keeps a curved trail after its history wraps");
    bool reusedShell = false;
    for (int tick = 0; tick < 360 && !reusedShell; ++tick) {
        cannons.tick(1.0 / 60.0, buildings.buildings(), enemies);
        if (!cannons.shots().empty()) {
            reusedShell = true;
            require(cannons.projectiles().front().trailPointCount == 2U,
                    "reused cannon shell clears the previous shot's trail");
        }
    }
    require(reusedShell, "cannon trail fixture exercises projectile reuse");
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
        tickUntilShot(specialized, buildings.buildings(), targets);
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
                    shell.visualScale == profile.visualScale * 1.1,
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
        tickUntilShot(specialized, buildings.buildings(), targets);
        require(std::ranges::count_if(specialized.projectiles(), [](const auto& item) { return item.active; }) == 1 &&
            std::ranges::find_if(specialized.projectiles(), [](const auto& item) { return item.active; })->visualScale == 1.0,
            "new run resets specialization while retaining reusable pool storage");
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

    // Large elevation changes previously moved the barrel by up to 7.6
    // degrees immediately. Acquisition, tracking, loss and rotation all retain
    // motion instead of snapping the barrel to rest.
    std::vector<ian::BuildingInstance> smoothRegistry{{
        .id = {902, 1}, .type = ian::BuildingType::Cannon,
    }};
    ian::EnemySystem elevatedEnemies;
    constexpr std::array<ian::Vec3, 1> ElevatedTarget{{{0.5, 6.0, -8.0}}};
    elevatedEnemies.spawnWave(ElevatedTarget);
    ian::CannonSystem smoothCannons;
    smoothCannons.syncBuildings(smoothRegistry);
    double firstRise = 0.0;
    double peakRise = 0.0;
    double lastRise = 0.0;
    for (int tick = 0; tick < 90; ++tick) {
        const double before = smoothCannons.cannons().front().pitch;
        smoothCannons.tick(1.0 / 60.0, smoothRegistry, elevatedEnemies);
        const auto& motion = smoothCannons.cannons().front();
        lastRise = motion.pitch - before;
        if (tick == 0) firstRise = lastRise;
        peakRise = std::max(peakRise, lastRise);
        requireNear(motion.previousPitch, before, 1e-12,
                    "barrel rendering retains its previous fixed-step pose");
        require(std::abs(lastRise) < 0.06,
                "elevated target cannot produce a sudden barrel step");
    }
    require(firstRise > 0.0 && firstRise < 0.02 &&
                peakRise > firstRise * 3.0 && std::abs(lastRise) < firstRise,
            "barrel aim accelerates from rest and decelerates near its target");
    require(smoothCannons.cannons().front().pitch > 0.4,
            "smooth barrel still reaches a strongly elevated target");
    elevatedEnemies.reset();
    const double raisedPitch = smoothCannons.cannons().front().pitch;
    smoothCannons.tick(1.0 / 60.0, smoothRegistry, elevatedEnemies);
    require(std::abs(smoothCannons.cannons().front().pitch - raisedPitch) < 0.02,
            "losing a target does not snap the raised barrel down");
    for (int tick = 0; tick < 120; ++tick) {
        smoothCannons.tick(1.0 / 60.0, smoothRegistry, elevatedEnemies);
    }
    requireNear(smoothCannons.cannons().front().pitch, raisedPitch, 1e-12,
                "idle barrel retains its last elevation indefinitely");
    smoothRegistry.front().rotation = 3;
    smoothCannons.syncBuildings(smoothRegistry);
    for (int tick = 0; tick < 120; ++tick) {
        smoothCannons.tick(1.0 / 60.0, smoothRegistry, elevatedEnemies);
    }
    smoothRegistry.front().rotation = 5;
    const double oldYaw = smoothCannons.cannons().front().yaw;
    smoothCannons.syncBuildings(smoothRegistry);
    requireNear(smoothCannons.cannons().front().yaw, oldYaw, 1e-12,
                "manual rotation preserves the cannon's current pose");
    double turnedDistance = 0.0;
    const double turnBlend = 1.0 - std::exp(-18.0 / 60.0);
    for (int tick = 0; tick < 120; ++tick) {
        const double before = smoothCannons.cannons().front().yaw;
        smoothCannons.tick(1.0 / 60.0, smoothRegistry, elevatedEnemies);
        const auto& motion = smoothCannons.cannons().front();
        const double turn = ian::wrapBuildingAngle(motion.yaw - before);
        require(turn >= -1e-8 && turn < 0.5,
                "rest rotation takes the short route across the angle seam");
        requireNear(turn,
                    ian::wrapBuildingAngle(motion.restYaw - before) * turnBlend,
                    1e-12, "manual cannon rotation keeps its original exponential easing");
        turnedDistance += turn;
    }
    requireNear(turnedDistance, ian::PiRadians * 0.5, 1e-5,
                "angle wrap cannot make a cannon turn the long way around");

    const std::vector<ian::BuildingInstance> catapultBuildings{{
        .id = {900, 1},
        .type = ian::BuildingType::Catapult,
        .gridPosition = {0, 0},
        .health = 210.0,
        .maxHealth = 210.0,
    }};
    ian::EnemySystem catapultEnemies;
    constexpr std::array<ian::Vec3, 4> CatapultTargets{{
        {3.0, 0.8, -2.5},
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
    catapults.tick(1.0 / 60.0, catapultBuildings, catapultEnemies);
    require(catapults.cannons().front().pitch > 0.0 &&
                catapults.cannons().front().pitch < 0.01,
            "catapult starts with a small smooth backward windup");
    for (int tick = 0; tick < 16; ++tick) {
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
    const auto& releasedBall = catapults.projectiles().front();
    require(releasedBall.trailPointCount == 2U,
            "catapult ball starts a clean trail at release");
    const auto& releasePose = catapults.cannons().front();
    const auto cup = ian::catapultMuzzleWorldPosition(
        ian::buildingWorldPosition(catapultBuildings.front()),
        releasePose.yaw, releasePose.pitch);
    const auto& launchPoint = releasedBall.trailPositions[0];
    require(std::hypot(cup.y - launchPoint.y, cup.z - launchPoint.z) < 0.08,
            "catapult ball separates from the moving cup without jumping");
    bool catapultCurvedTrail = false;
    bool catapultWrappedTrail = false;
    bool followedThrough = false;
    bool recovered = false;
    for (int tick = 0; tick < 120; ++tick) {
        const double before = catapults.cannons().front().pitch;
        catapults.tick(1.0 / 60.0, catapultBuildings, catapultEnemies);
        const auto& motion = catapults.cannons().front();
        requireNear(motion.previousPitch, before, 1e-12,
                    "catapult render interpolation retains the previous arm pose");
        require(std::abs(motion.pitch - before) < 0.15,
                "catapult throw and recovery cannot snap the arm");
        followedThrough |= motion.pitch < -1.15;
        recovered |= motion.loaded && motion.firingAnimationRemaining == 0.0 &&
                     std::abs(motion.pitch) < 1e-8 && !motion.pendingTargetPosition;
        const auto& ball = catapults.projectiles().front();
        if (!ball.active || ball.trailPointCount < 3U) continue;
        const auto point = [&ball](std::size_t age) {
            return ball.trailPositions[
                (ball.trailHead + ian::CannonProjectile::TrailCapacity - age) %
                ian::CannonProjectile::TrailCapacity];
        };
        requireNear(point(0).y, ball.position.y, 1e-12,
                    "catapult trail head follows the physical ball");
        requireNear(point(0).y - 2.0 * point(1).y + point(2).y,
                    -9.8 / (60.0 * 60.0), 1e-12,
                    "catapult trail follows gravity throughout the arc");
        catapultCurvedTrail = true;
        catapultWrappedTrail |= ball.trailPointCount == ian::CannonProjectile::TrailCapacity;
    }
    require(catapultCurvedTrail && catapultWrappedTrail && followedThrough && recovered,
            "catapult follows through, settles and keeps a bounded curved trail");
    require(std::ranges::any_of(catapultEnemies.enemies(),[](const auto& enemy) {
            return enemy.movementMultiplier < 1.0 && enemy.slowRemaining > 0.0;
        }), "catapult impact applies a timed ground stagger");
    bool reusedCatapultBall = false;
    for (int tick = 0; tick < 240 && !reusedCatapultBall; ++tick) {
        catapults.tick(1.0 / 60.0, catapultBuildings, catapultEnemies);
        reusedCatapultBall = !catapults.shots().empty();
    }
    require(reusedCatapultBall && catapults.projectiles().front().trailPointCount == 2U,
            "reused catapult ball clears its old trajectory");

    ian::EnemySystem sideTargets;
    constexpr std::array<ian::Vec3, 1> SideTarget{{{5.0, 0.8, -9.0}}};
    sideTargets.spawnWave(SideTarget);
    ian::CannonSystem turningCatapult;
    turningCatapult.syncBuildings(catapultBuildings);
    double firstTurn = 0.0;
    double peakTurn = 0.0;
    bool releasedWhileAiming = false;
    for (int tick = 0; tick < 150; ++tick) {
        const double before = turningCatapult.cannons().front().yaw;
        turningCatapult.tick(1.0 / 60.0, catapultBuildings, sideTargets);
        const auto& motion = turningCatapult.cannons().front();
        const double turn = std::abs(ian::wrapBuildingAngle(motion.yaw - before));
        if (tick == 0) firstTurn = turn;
        peakTurn = std::max(peakTurn, turn);
        require(turn < 0.04,
                "catapult acquisition and throw keep yaw continuous");
        if (!turningCatapult.shots().empty()) {
            const double aimedYaw = std::atan2(-5.0, 9.0);
            require(std::abs(ian::wrapBuildingAngle(motion.yaw - aimedYaw)) < 0.01,
                    "catapult finishes aiming during windup instead of freezing early");
            releasedWhileAiming = true;
        }
    }
    require(firstTurn > 0.0 && peakTurn > firstTurn * 3.0 && releasedWhileAiming,
            "catapult accelerates into aim and releases toward its target");

    for (const auto type : {ian::BuildingType::Cannon, ian::BuildingType::Catapult}) {
        std::vector<ian::BuildingInstance> registry{{
            .id = {91, 1}, .type = type, .rotation = 0,
        }};
        ian::CannonSystem defense;
        defense.syncBuildings(registry);
        defense.syncBuildings(registry);
        require(defense.cannons().size() == 1,
            "repeated defense synchronization does not duplicate runtimes");
        registry.front().rotation = 1;
        defense.syncBuildings(registry);
        requireNear(defense.cannons().front().yaw, 0, 1e-9,
            "rotating ballistic defenses preserves their current visual yaw");
        requireNear(defense.cannons().front().targetSearchCooldownRemaining,
            0.5, 1e-9, "ballistic defenses retain their own search interval");
        registry.front().type = ian::BuildingType::Wall;
        defense.syncBuildings(registry);
        require(defense.cannons().empty(),
            "unsupported buildings cannot retain ballistic defense runtimes");
    }
}
