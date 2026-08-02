#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "enemies/EnemyCollision.hpp"
#include "enemies/EnemySystem.hpp"
#include "navigation/FlowField.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

void runEnemySystemTests() {
    ian::BuildingSystem buildings;
    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "enemy fixture creates core");
    std::optional<ian::PlacedBuilding> frontWall;
    for (int x = -2; x <= 2; ++x) {
        const auto front = buildings.place(ian::BuildingType::Wall, {x, -2}, 0, 10, 0);
        const auto back = buildings.place(ian::BuildingType::Wall, {x, 2}, 0, 10, 0);
        require(front.has_value() && back.has_value(), "enemy fixture creates horizontal walls");
        if (x == 0) {
            frontWall = front;
        }
    }
    for (int z = -1; z <= 1; ++z) {
        require(buildings.place(ian::BuildingType::Wall, {-2, z}, 0, 10, 0).has_value(),
                "enemy fixture creates left wall");
        require(buildings.place(ian::BuildingType::Wall, {2, z}, 0, 10, 0).has_value(),
                "enemy fixture creates right wall");
    }
    require(frontWall.has_value(), "enemy fixture tracks front wall");

    ian::EnemySystem enemies;
    constexpr std::array<ian::Vec3, 5> BasicSpawns{{
        {-4.0, 0.8, -14.0},
        {-2.0, 0.8, -14.0},
        {0.0, 0.8, -14.0},
        {2.0, 0.8, -14.0},
        {4.0, 0.8, -14.0},
    }};
    enemies.spawnWave(BasicSpawns);
    require(enemies.activeCount() == 5, "first wave spawns five enemies");

    ian::EnemySystem groupedEnemies;
    groupedEnemies.spawnWave(
        std::span<const ian::Vec3>{BasicSpawns.data(), static_cast<std::size_t>(1)});
    constexpr std::array<ian::EnemySpawn, 1> Reinforcement{{
        {ian::EnemyType::Fast, {5.0, 0.675, 5.0}},
    }};
    groupedEnemies.spawnGroup(Reinforcement);
    require(groupedEnemies.activeCount() == 2, "spawn group appends reinforcements");
    ian::FlowField flowField;
    flowField.rebuild({0, 0}, buildings.buildings());

    bool wallAttacked = false;
    for (int tick = 0; tick < 600 && !wallAttacked; ++tick) {
        const auto attacks = enemies.tick(1.0 / 60.0, buildings.buildings(), flowField);
        for (const auto& attack : attacks) {
            if (attack.targetId != core->building.id) {
                wallAttacked = true;
            }
        }
    }
    require(wallAttacked, "enemy on blocked route attacks wall");

    ian::EnemySystem foundationEnemies;
    constexpr std::array<ian::Vec3, 1>
        FoundationSpawn{{{0.0, 0.8, -4.5}}};
    foundationEnemies.spawnWave(FoundationSpawn);
    constexpr ian::EntityId FoundationId{12000U, 1U};
    const std::array<ian::EnemyStructureTarget, 1>
        foundationTargets{{
            {
                .id = FoundationId,
                .position = {0.0, 1.0, -3.0},
                .radius = 1.1,
                .buildingType = std::nullopt,
                .modular = true,
                .structuralImpact = 1U,
            },
        }};
    const auto foundationAttacks =
        foundationEnemies.tick(
            1.0 / 60.0, buildings.buildings(),
            flowField, std::nullopt,
            foundationTargets);
    require(
        foundationAttacks.size() == 1 &&
            foundationAttacks.front().targetId ==
                FoundationId,
        "enemy treats PlatformFrame as attackable structure");

    ian::EnemySystem stackedEnemies;
    stackedEnemies.spawnWave(FoundationSpawn);
    constexpr ian::EntityId UpperStructureId{12002U, 1U};
    const std::array<ian::EnemyStructureTarget, 2>
        stackedTargets{{
            {
                .id = FoundationId,
                .position = {0.0, 1.0, -3.0},
                .radius = 1.1,
                .buildingType = std::nullopt,
                .modular = true,
                .structuralImpact = 4U,
            },
            {
                .id = UpperStructureId,
                .position = {0.0, 5.0, -3.0},
                .radius = 1.1,
                .buildingType = std::nullopt,
                .modular = true,
                .structuralImpact = 1U,
            },
        }};
    const auto stackedAttacks = stackedEnemies.tick(
        1.0 / 60.0, buildings.buildings(),
        flowField, std::nullopt, stackedTargets);
    require(
        stackedAttacks.size() == 1 &&
            stackedAttacks.front().targetId ==
                FoundationId,
        "ground enemy attacks reachable lower support instead of upper floor");

    ian::EnemySystem sapperEnemies;
    constexpr std::array<ian::EnemySpawn, 1>
        StructuralSapperSpawn{{
            {
                ian::EnemyType::Sapper,
                {0.0, 0.8, -4.5},
            },
        }};
    sapperEnemies.spawnWave(StructuralSapperSpawn);
    constexpr ian::EntityId ModularWallId{12001U, 1U};
    const std::array<ian::EnemyStructureTarget, 2>
        sapperTargets{{
            {
                .id = ModularWallId,
                .position = {0.0, 1.0, -3.3},
                .radius = 0.55,
                .buildingType = std::nullopt,
                .modular = true,
                .structuralImpact = 0U,
            },
            {
                .id = FoundationId,
                .position = {0.0, 1.0, -2.6},
                .radius = 1.1,
                .buildingType = std::nullopt,
                .modular = true,
                .structuralImpact = 4U,
            },
        }};
    const auto sapperAttacks = sapperEnemies.tick(
        1.0 / 60.0, buildings.buildings(),
        flowField, std::nullopt, sapperTargets);
    require(
        sapperAttacks.size() == 1 &&
            sapperAttacks.front().targetId ==
                FoundationId,
        "sapper prioritizes high-impact structural support");

    const auto centralEnemy = enemies.raycast({0.0, 0.8, -9.0}, {0.0, 0.0, 1.0}, 6.0);
    require(centralEnemy.has_value(), "enemy raycast finds target");
    enemies.damage(*centralEnemy, 4.0);
    const auto killed = enemies.damage(*centralEnemy, 1.0);
    require(killed.has_value() && killed->killed, "lethal damage kills enemy");
    require(enemies.activeCount() == 4, "dead enemy leaves active count");

    ian::EnemySystem overlappingEnemies;
    constexpr std::array<ian::Vec3, 2> SameSpawn{{
        {0.0, 0.8, -6.0},
        {0.0, 0.8, -6.0},
    }};
    overlappingEnemies.spawnWave(SameSpawn);
    overlappingEnemies.tick(1.0 / 60.0, buildings.buildings(), flowField);
    const auto& separated = overlappingEnemies.enemies();
    require(separated[0].position.x < separated[1].position.x,
            "deterministic separation splits overlapping enemies");
    const double separatedDistance = std::hypot(
        separated[1].position.x - separated[0].position.x,
        separated[1].position.z - separated[0].position.z);
    const double requiredDistance =
        ian::enemyCapsule(separated[0].type).radius +
        ian::enemyCapsule(separated[1].type).radius;
    require(
        separatedDistance + 1e-6 >= requiredDistance,
        "enemy capsules cannot overlap after movement");

    ian::EnemySystem typedEnemies;
    constexpr std::array<ian::EnemySpawn, 3> TypedSpawns{{
        {ian::EnemyType::Fast, {-3.0, 0.675, -8.0}},
        {ian::EnemyType::Heavy, {0.0, 1.0, -8.0}},
        {ian::EnemyType::Boss, {3.0, 1.6, -8.0}},
    }};
    typedEnemies.spawnWave(TypedSpawns);
    const auto& typed = typedEnemies.enemies();
    require(typed[0].speed > 3.0 && typed[0].health == 4.0,
            "fast enemy uses fast low-health stats");
    require(typed[1].speed < 2.0 && typed[1].health == 16.0,
            "heavy enemy uses slow high-health stats");
    require(typed[2].type == ian::EnemyType::Boss && typed[2].health == 70.0,
            "boss uses final-wave stats");

    ian::EnemySystem scaledEnemy;
    constexpr std::array<ian::EnemySpawn, 1> ScaledSpawn{{
        {
            .type = ian::EnemyType::Basic,
            .position = {0.0, 0.8, -8.0},
            .healthMultiplier = 2.5,
            .damageMultiplier = 1.75,
        },
    }};
    scaledEnemy.spawnWave(ScaledSpawn);
    require(
        scaledEnemy.enemies().front().health == 12.5 &&
            scaledEnemy.enemies().front().damage == 17.5,
        "wave multipliers scale enemy health and damage");

    ian::EnemySystem rangedEnemy;
    constexpr std::array<ian::EnemySpawn, 1> RangedSpawn{{
        {ian::EnemyType::Ranged, {0.5, 0.85, -6.0}},
    }};
    rangedEnemy.spawnWave(RangedSpawn);
    bool rangedAttackedWall = false;
    for (int tick = 0; tick < 180 && !rangedAttackedWall;
         ++tick) {
        const auto attacks = rangedEnemy.tick(
            1.0 / 60.0, buildings.buildings(), flowField);
        rangedAttackedWall =
            !attacks.empty() &&
            attacks.front().targetId != core->building.id;
    }
    require(rangedAttackedWall &&
                rangedEnemy.enemies().front().position.z <
                    -4.0,
            "ranged enemy attacks blocker from stand-off distance");

    ian::EnemySystem sapperEnemy;
    constexpr std::array<ian::EnemySpawn, 1> SapperSpawn{{
        {ian::EnemyType::Sapper, {0.5, 0.78, -2.7}},
    }};
    sapperEnemy.spawnWave(SapperSpawn);
    bool sapperAmplifiedWallDamage = false;
    for (int tick = 0;
         tick < 120 && !sapperAmplifiedWallDamage; ++tick) {
        const auto attacks = sapperEnemy.tick(
            1.0 / 60.0, buildings.buildings(), flowField);
        sapperAmplifiedWallDamage =
            !attacks.empty() &&
            attacks.front().targetId != core->building.id &&
            attacks.front().damage == 30.0;
    }
    require(sapperAmplifiedWallDamage,
            "sapper deals amplified damage to walls");

    ian::EnemySystem flyingEnemy;
    constexpr std::array<ian::EnemySpawn, 1> FlyingSpawn{{
        {ian::EnemyType::Flying, {0.0, 2.4, -5.0}},
    }};
    flyingEnemy.spawnWave(FlyingSpawn);
    bool flyerAttackedCore = false;
    bool flyerAttackedWall = false;
    for (int tick = 0; tick < 300 && !flyerAttackedCore;
         ++tick) {
        const auto attacks = flyingEnemy.tick(
            1.0 / 60.0, buildings.buildings(), flowField);
        for (const auto& attack : attacks) {
            flyerAttackedCore =
                attack.targetId == core->building.id;
            flyerAttackedWall =
                flyerAttackedWall ||
                attack.targetId != core->building.id;
        }
    }
    require(flyerAttackedCore && !flyerAttackedWall,
            "flying enemy bypasses walls and attacks core");

    ian::BuildingSystem ramBuildings;
    require(ramBuildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0).has_value(),
            "boss ram fixture creates core");
    ian::FlowField ramFlowField;
    ramFlowField.rebuild({0, 0}, ramBuildings.buildings());
    ian::EnemySystem ramBoss;
    constexpr std::array<ian::EnemySpawn, 1> BossSpawn{{
        {ian::EnemyType::Boss, {0.0, 1.6, -3.0}},
    }};
    ramBoss.spawnWave(BossSpawn);
    require(ramBoss.tick(1.0 / 60.0, ramBuildings.buildings(), ramFlowField).empty() &&
                ramBoss.enemies()[0].state == ian::EnemyState::BossRamWindup,
            "boss telegraphs ram before attacking");
    const auto ramAttack =
        ramBoss.tick(1.5, ramBuildings.buildings(), ramFlowField);
    require(ramAttack.size() == 1 && ramAttack[0].ram &&
                ramAttack[0].damage == 150.0,
            "boss ram deals configured amplified damage");
    const auto cooldownAttack =
        ramBoss.tick(1.0, ramBuildings.buildings(), ramFlowField);
    require(cooldownAttack.size() == 1 && !cooldownAttack[0].ram &&
                cooldownAttack[0].damage == 50.0,
            "boss uses regular attack while ram is on cooldown");

    ian::EnemySystem contactEnemies;
    constexpr std::array<ian::Vec3, 1> ContactSpawn{{{4.0, 0.8, 4.0}}};
    contactEnemies.spawnWave(ContactSpawn);
    contactEnemies.tick(1.0 / 60.0, buildings.buildings(), flowField,
                        ian::Vec3{4.0, 1.7, 4.0});
    require(contactEnemies.playerAttacks().size() == 1 &&
                contactEnemies.playerAttacks()[0].damage == 10.0,
            "nearby enemy attacks player");
    require(contactEnemies.enemies()[0].state == ian::EnemyState::AttackPlayer,
            "enemy enters player attack state");
    contactEnemies.tick(0.5, buildings.buildings(), flowField,
                        ian::Vec3{4.0, 1.7, 4.0});
    require(contactEnemies.playerAttacks().empty(),
            "player attack respects enemy cooldown");
    contactEnemies.tick(0.5, buildings.buildings(), flowField,
                        ian::Vec3{4.0, 1.7, 4.0});
    require(contactEnemies.playerAttacks().size() == 1,
            "enemy attacks player again after cooldown");

    ian::EnemySystem chasingEnemies;
    constexpr std::array<ian::Vec3, 1> ChasingSpawn{{
        {4.0, 0.8, 4.0},
    }};
    chasingEnemies.spawnWave(ChasingSpawn);
    chasingEnemies.tick(
        1.0, ramBuildings.buildings(), ramFlowField,
        ian::Vec3{8.0, 1.7, 4.0});
    require(
        chasingEnemies.enemies()[0].state ==
                ian::EnemyState::ChasePlayer &&
            chasingEnemies.enemies()[0].position.x > 4.0,
        "enemy detects and chases player before attack range");

    require(contactEnemies.defeatAll() == 1 && contactEnemies.activeCount() == 0,
            "debug defeat clears all active enemies");

    std::vector<ian::EnemySpawn> stressSpawns;
    stressSpawns.reserve(ian::EnemySystem::MaxEnemies + 64);
    for (std::size_t index = 0;
         index < ian::EnemySystem::MaxEnemies + 64; ++index) {
        stressSpawns.push_back({
            .type = ian::EnemyType::Basic,
            .position = {
                static_cast<double>(
                    static_cast<int>(index % 40) - 20),
                0.8,
                -4.0 -
                    static_cast<double>(index / 40) * 0.7,
            },
        });
    }
    ian::EnemySystem stressEnemies;
    stressEnemies.spawnWave(stressSpawns);
    require(
        stressEnemies.activeCount() ==
            ian::EnemySystem::MaxActiveEnemies,
        "enemy system caps simultaneous active enemies");
    stressEnemies.tick(1.0 / 60.0, buildings.buildings(), flowField);
    require(
        stressEnemies.activeCount() ==
            ian::EnemySystem::MaxActiveEnemies,
        "large enemy stress tick preserves active cap");

    const ian::EntityId recycledId = stressEnemies.enemies().front().id;
    require(stressEnemies.damage(recycledId, 1000.0)->killed,
            "stress fixture frees one pooled enemy");
    constexpr std::array<ian::EnemySpawn, 1> Replacement{{
        {ian::EnemyType::Fast, {12.0, 0.675, -20.0}},
    }};
    stressEnemies.spawnGroup(Replacement);
    const auto recycled =
        std::find_if(stressEnemies.enemies().begin(), stressEnemies.enemies().end(),
                     [recycledId](const ian::EnemyInstance& enemy) {
                         return enemy.id.index == recycledId.index;
                     });
    require(recycled != stressEnemies.enemies().end() && recycled->active &&
                recycled->id.generation == recycledId.generation + 1,
            "enemy pool reuses slot with incremented generation");

    std::vector<ian::EnemySpawn> blastSpawns;
    blastSpawns.reserve(ian::EnemySystem::MaxActiveEnemies);
    for (std::size_t index = 0;
         index < ian::EnemySystem::MaxActiveEnemies; ++index) {
        blastSpawns.push_back({
            .type = ian::EnemyType::Basic,
            .position = {
                (static_cast<double>(index % 16U) - 7.5) * 0.25,
                0.8,
                (static_cast<double>(index / 16U) - 4.5) * 0.25,
            },
        });
    }
    ian::EnemySystem blastEnemies;
    blastEnemies.spawnWave(blastSpawns);
    const ian::EntityId firstBlastId =
        blastEnemies.enemies().front().id;
    const auto blastDamage = blastEnemies.damageInRadius(
        {0.0, 0.8, 0.0}, 4.0, 1000.0, 2.0);
    require(
        blastDamage.size() ==
                ian::EnemySystem::MaxActiveEnemies &&
            blastEnemies.activeCount() == 0 &&
            !blastEnemies.nearestEnemy(
                {0.0, 0.8, 0.0}, 4.0),
        "area damage atomically removes a full active wave from the spatial index");
    blastEnemies.spawnGroup(Replacement);
    require(
        blastEnemies.enemies().front().active &&
            blastEnemies.enemies().front().id.index ==
                firstBlastId.index &&
            blastEnemies.enemies().front().id.generation ==
                firstBlastId.generation + 1,
        "area damage preserves generation-safe pool reuse");
}
