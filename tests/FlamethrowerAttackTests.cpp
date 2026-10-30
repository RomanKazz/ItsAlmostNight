#include "TestHarness.hpp"
#include "combat/FlamethrowerAttack.hpp"
#include "combat/TowerSystem.hpp"
#include "combat/AttackStatCompatibility.hpp"

#include <chrono>

void checkFlamethrowerAttack() {
    using namespace ian;
    const BuildingInstance flame{.id = {830, 1}, .type = BuildingType::Flamethrower,
        .gridPosition = {0, 0}, .level = 1};
    const std::vector<BuildingInstance> buildings{flame};
    auto definitions = GameBalance::defaults().enemies;
    for (auto& definition : definitions) definition.health = 100000;
    const auto enemiesAt = [&](std::initializer_list<Vec3> positions) {
        EnemySystem enemies(definitions);
        std::vector<EnemySpawn> spawns;
        for (const auto position : positions) spawns.push_back({.type = EnemyType::Basic, .position = position});
        enemies.spawnWave(spawns);
        return enemies;
    };
    const auto base = TowerSystem::baseStats(flame.type, flame.level);
    auto previousLevel = base;
    for (int level = 2; level <= 8; ++level) {
        const auto current = TowerSystem::baseStats(flame.type, static_cast<std::uint8_t>(level));
        require(current.damage > previousLevel.damage && current.range > previousLevel.range &&
            current.attackInterval < previousLevel.attackInterval,
            "every flamethrower level improves damage, reach and heat frequency");
        previousLevel = current;
    }
    requireNear(base.range, 4.0, 1e-12, "flamethrower has short four-cell reach");
    requireNear(base.damage / base.attackInterval, 2.5, 1e-12, "base heat stream has 2.5 raw DPS per target");
    const Vec3 muzzle = flamethrowerMuzzleWorldPosition({1, 0, 1}, 0, 0);
    requireNear(muzzle.y, 0.924, 1e-12, "flame jet starts at authored pitch socket");
    requireNear(muzzle.z, 0.41, 1e-12, "flame jet starts ahead of barrel");
    auto geometryEnemies = enemiesAt({{1, 0, -1}, {1.4, 0, -2}, {3.5, 0, -1}, {1, 0, 2}, {1, 0, -5}});
    const std::array expected{true, true, false, false, false};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        require(flamethrowerTouchesEnemy(geometryEnemies.enemies()[index], muzzle, {0, 0, -1},
            flamethrowerStreamLength(base)) == expected[index],
            "finite flame cone accepts front crowd and rejects side, rear and distant enemies");
    }
    const auto pulseCount = [&](double speed) {
        auto enemies = enemiesAt({{1, 0, -1}});
        TowerSystem towers;
        AttackModifiers modifiers;
        modifiers[AttackStat::AttackSpeed].multiplier = speed;
        towers.setAttackModifiers(modifiers);
        towers.syncBuildings(buildings);
        int events = 0;
        for (int frame = 0; frame < 360; ++frame) {
            towers.tick(1.0 / 120.0, buildings, enemies);
            events += static_cast<int>(towers.attacks().size());
            require(towers.projectiles().empty() && towers.towers()[0].sequenceRemaining == 0,
                "flame stream has no fake projectiles or overlapping burst sequences");
        }
        require(towers.activeFlameBurns() == 1, "repeated heat pulses refresh one burn per enemy");
        return events;
    };
    const int normal = pulseCount(1);
    const int fast = pulseCount(2);
    require(normal >= 13 && fast >= normal * 2 - 1, "attack speed increases real heat pulse throughput");

    auto enemies = enemiesAt({{1, 0, -1}, {1.3, 0, -1.6}, {4, 0, -1}});
    TowerSystem towers;
    towers.syncBuildings(buildings);
    bool firstHit = false, secondHit = false, outsideHit = false;
    for (int frame = 0; frame < 120; ++frame) {
        for (const auto& hit : towers.tick(1.0 / 120.0, buildings, enemies)) {
            if (hit.periodicBurn) continue;
            firstHit |= hit.targetId == enemies.enemies()[0].id;
            secondHit |= hit.targetId == enemies.enemies()[1].id;
            outsideHit |= hit.targetId == enemies.enemies()[2].id;
        }
    }
    require(firstHit && secondHit && !outsideHit, "one heat event hits multiple actual cone contacts");
    require(towers.towers()[0].flameAge > .3 && towers.towers()[0].flameTailRemaining > 0,
        "continuous heat pulses preserve ignition age and live visual tail");
    auto lostTarget = enemiesAt({{1, 0, 5}});
    towers.tick(1.0 / 120.0, buildings, lostTarget);
    require(towers.towers()[0].flameRemaining == 0 && towers.towers()[0].flameTailRemaining > 0,
        "losing the target stops emission but retains travelling flames");
    for (int frame = 0; frame < 120; ++frame) towers.tick(1.0 / 120.0, buildings, lostTarget);
    require(towers.towers()[0].flameTailRemaining == 0, "visual tail expires without continuing attacks");
    const double beforeBurn = enemies.enemies()[0].health;
    int burnHits = 0;
    towers.syncBuildings({});
    for (int frame = 0; frame < 300; ++frame) {
        for (const auto& hit : towers.tick(1.0 / 120.0, {}, enemies)) {
            require(hit.periodicBurn && hit.type == BuildingType::Flamethrower && hit.towerId == flame.id,
                "afterburn retains original defense attribution after source removal");
            ++burnHits;
        }
    }
    require(burnHits > 0 && enemies.enemies()[0].health < beforeBurn && towers.activeFlameBurns() == 0,
        "afterburn continues briefly, expires and releases runtime state");

    AttackModifiers modifiers;
    modifiers[AttackStat::Damage].multiplier = 2;
    modifiers[AttackStat::Size].multiplier = 1.5;
    modifiers[AttackStat::Duration].multiplier = 2;
    modifiers[AttackStat::Quantity].flat = 30;
    modifiers[AttackStat::Bounces].flat = 5;
    modifiers[AttackStat::ProjectileSpeed].multiplier = 5;
    modifiers[AttackStat::CritChance].flat = 1;
    modifiers[AttackStat::CritDamage].multiplier = 1.5;
    towers.setAttackModifiers(modifiers);
    const auto enhanced = towers.finalStats(flame);
    requireNear(enhanced.damage, base.damage * 2, 1e-12, "flame damage uses shared modifiers");
    requireNear(flamethrowerRange(enhanced), 6, 1e-12, "size extends real flame cone and displayed reach");
    requireNear(enhanced.duration, 4, 1e-12, "duration extends afterburn");
    const auto distant = enemiesAt({{1, 0, -4.8}}).enemies()[0];
    require(!flamethrowerTouchesEnemy(distant, muzzle, {0, 0, -1}, flamethrowerStreamLength(base)) &&
        flamethrowerTouchesEnemy(distant, muzzle, {0, 0, -1}, flamethrowerStreamLength(enhanced)),
        "size upgrade reaches an enemy beyond the base cone");
    require(enhanced.quantity == 1 && enhanced.bounces == 0 && enhanced.projectileSpeed == 0,
        "unsupported projectile statistics have no artificial flame interpretation");
    auto criticalEnemies = enemiesAt({{1, 0, -1}});
    towers.syncBuildings(buildings);
    bool critical = false;
    for (int frame = 0; frame < 120 && !critical; ++frame) {
        for (const auto& hit : towers.tick(1.0 / 120.0, buildings, criticalEnemies)) {
            if (hit.periodicBurn) continue;
            critical = hit.critical;
            requireNear(hit.damage, enhanced.damage * enhanced.critDamage, 1e-8, "heat hits use shared critical damage");
        }
    }
    require(critical, "flame cone resolves guaranteed crit through shared hit flow");
    towers.clearProjectiles();
    require(towers.activeFlameBurns() == 0 && towers.towers()[0].flameRemaining == 0,
        "wave cleanup clears flames and afterburn");
    towers.reset();
    require(towers.towers().empty() && towers.activeFlameBurns() == 0, "new run resets flame state");

    // Crowded overlap is the expensive case: many jets share one dose per
    // enemy. Measure it, but do not impose a machine-specific time assertion.
    EnemySystem crowd(definitions);
    std::vector<EnemySpawn> spawns;
    for (int index = 0; index < 1000; ++index) {
        spawns.push_back({.type = EnemyType::Basic,
            .position = {0.75 + (index % 10) * 0.05, 0, -0.6 - (index % 17) * 0.1}});
    }
    crowd.spawnWave(spawns);
    std::vector<BuildingInstance> battery;
    for (std::uint32_t index = 0; index < 32; ++index) {
        auto copy = flame; copy.id = {900 + index, 1}; battery.push_back(copy);
    }
    TowerSystem many;
    many.syncBuildings(battery);
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < 120; ++frame) many.tick(1.0 / 60.0, battery, crowd);
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    require(many.activeFlameBurns() <= 1000 && many.projectiles().empty(),
        "32 overlapping jets do not grow burns per pulse or allocate projectiles");
    std::cout << "Flamethrower stress: 32 jets, 1000 enemies, " << milliseconds / 120.0 << " ms/tick\n";
}

int main() { checkFlamethrowerAttack(); }
