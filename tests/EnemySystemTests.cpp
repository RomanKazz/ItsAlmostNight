#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "enemies/EnemySystem.hpp"
#include "navigation/FlowField.hpp"

#include <algorithm>
#include <array>
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
            if (attack.targetId == frontWall->building.id) {
                wallAttacked = true;
            }
        }
    }
    require(wallAttacked, "enemy on blocked route attacks wall");

    const auto centralEnemy = enemies.raycast({0.0, 0.8, -9.0}, {0.0, 0.0, 1.0}, 6.0);
    require(centralEnemy.has_value(), "enemy raycast finds target");
    enemies.damage(*centralEnemy, 2.0);
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

    ian::EnemySystem typedEnemies;
    constexpr std::array<ian::EnemySpawn, 3> TypedSpawns{{
        {ian::EnemyType::Fast, {-3.0, 0.675, -8.0}},
        {ian::EnemyType::Heavy, {0.0, 1.0, -8.0}},
        {ian::EnemyType::Boss, {3.0, 1.6, -8.0}},
    }};
    typedEnemies.spawnWave(TypedSpawns);
    const auto& typed = typedEnemies.enemies();
    require(typed[0].speed > 3.0 && typed[0].health == 2.0,
            "fast enemy uses fast low-health stats");
    require(typed[1].speed < 2.0 && typed[1].health == 10.0,
            "heavy enemy uses slow high-health stats");
    require(typed[2].type == ian::EnemyType::Boss && typed[2].health == 40.0,
            "boss uses final-wave stats");

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
    require(contactEnemies.defeatAll() == 1 && contactEnemies.activeCount() == 0,
            "debug defeat clears all active enemies");

    std::vector<ian::EnemySpawn> stressSpawns;
    stressSpawns.reserve(220);
    for (int index = 0; index < 220; ++index) {
        stressSpawns.push_back({
            .type = ian::EnemyType::Basic,
            .position = {
                static_cast<double>((index % 20) - 10),
                0.8,
                -20.0 - static_cast<double>(index / 20),
            },
        });
    }
    ian::EnemySystem stressEnemies;
    stressEnemies.spawnWave(stressSpawns);
    require(stressEnemies.activeCount() == ian::EnemySystem::MaxEnemies,
            "enemy pool caps wave at 200 entries");
    stressEnemies.tick(1.0 / 60.0, buildings.buildings(), flowField);
    require(stressEnemies.activeCount() == ian::EnemySystem::MaxEnemies,
            "200-enemy stress tick preserves active pool");

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
}
