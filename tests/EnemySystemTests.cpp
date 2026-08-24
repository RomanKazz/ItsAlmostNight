#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"
#include "enemies/EnemyCollision.hpp"
#include "enemies/EnemySystem.hpp"
#include "navigation/FlowField.hpp"
#include "world/TerrainHeightfield.hpp"
#include "world/CollisionWorld.hpp"
#include "world/WorldConfig.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

void runEnemySystemTests() {
    require(
        ian::enemyUsesForwardSurfaceProbe(
            ian::EnemyState::MoveToCore) &&
            ian::enemyUsesForwardSurfaceProbe(
                ian::EnemyState::ChasePlayer) &&
            !ian::enemyUsesForwardSurfaceProbe(
                ian::EnemyState::AttackPlayer) &&
            !ian::enemyUsesForwardSurfaceProbe(
                ian::EnemyState::AttackBuilding) &&
            !ian::enemyUsesForwardSurfaceProbe(
                ian::EnemyState::AttackCore),
        "only moving enemies probe forward for a higher surface");
    {
        ian::EnemySystem resetIds;
        constexpr std::array<ian::Vec3, 1> Spawn{{
            {0.0, 0.8, -14.0},
        }};
        resetIds.spawnWave(Spawn);
        const ian::EntityId beforeReset =
            resetIds.enemies().front().id;
        resetIds.reset();
        resetIds.spawnWave(Spawn);
        const ian::EntityId afterReset =
            resetIds.enemies().front().id;
        require(
            afterReset != beforeReset &&
                resetIds.enemy(afterReset).has_value() &&
                resetIds.damage(afterReset, 1.0).has_value(),
            "enemy reset keeps new IDs unique and addressable");
    }
    {
        ian::EnemySystem trialEnemies;
        constexpr std::array<ian::EnemySpawn, 1> TrialSpawn{{
            {ian::EnemyType::Basic, {0.0, 0.8, -10.0}},
        }};
        trialEnemies.spawnGroup(TrialSpawn);
        ian::FlowField noCoreFlow;
        const double initialZ = trialEnemies.enemies().front().position.z;
        static_cast<void>(trialEnemies.tick(
            0.5, {}, noCoreFlow, ian::Vec3{0.0, 1.7, 0.0},
            {}, nullptr, {}, true));
        require(
            trialEnemies.enemies().front().position.z > initialZ &&
                trialEnemies.enemies().front().state ==
                    ian::EnemyState::ChasePlayer,
            "trial enemy pursues player even before a core exists");
    }
    {
        constexpr std::array<ian::EnemySpawn, 3> EliteSpawns{{
            {
                .type = ian::EnemyType::Basic,
                .position = {0.0, 0.8, 0.0},
                .eliteAffixes = ian::eliteAffixMask(
                    ian::EliteAffix::Warden),
            },
            {
                .type = ian::EnemyType::Basic,
                .position = {2.0, 0.8, 0.0},
            },
            {
                .type = ian::EnemyType::Basic,
                .position = {10.0, 0.8, 0.0},
                .eliteAffixes = ian::eliteAffixMask(
                    ian::EliteAffix::Volatile),
            },
        }};
        ian::EnemySystem eliteEnemies;
        eliteEnemies.spawnWave(EliteSpawns);
        const auto spawnEvents =
            eliteEnemies.takeEliteSpawnEvents();
        require(
            spawnEvents.size() == 2U &&
                eliteEnemies.enemies()[0].maxHealth >
                    eliteEnemies.enemies()[1].maxHealth,
            "elite spawns carry affixes and increased health");
        const auto protectedHit = eliteEnemies.damage(
            eliteEnemies.enemies()[1].id, 1.0);
        const auto wardenHit = eliteEnemies.damage(
            eliteEnemies.enemies()[0].id, 1.0);
        require(
            protectedHit && wardenHit &&
                std::abs(protectedHit->damage - 0.75) < 1e-9 &&
                std::abs(wardenHit->damage - 1.0) < 1e-9,
            "warden protects nearby allies but not itself");
        const ian::EntityId volatileId =
            eliteEnemies.enemies()[2].id;
        const auto volatileDeath =
            eliteEnemies.damage(volatileId, 1000.0);
        require(
            volatileDeath && volatileDeath->killed &&
                volatileDeath->type == ian::EnemyType::Basic &&
                ian::hasEliteAffix(
                    volatileDeath->eliteAffixes,
                    ian::EliteAffix::Volatile),
            "death result preserves type and elite reward metadata");
        const auto eliteDeaths =
            eliteEnemies.takeEliteDeathEvents();
        require(
            eliteDeaths.size() == 1U &&
                eliteDeaths.front().id == volatileId &&
                ian::hasEliteAffix(
                    eliteDeaths.front().affixes,
                    ian::EliteAffix::Volatile),
            "volatile death is preserved for delayed explosion");
    }
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

    ian::EnemySystem roleEnemies;
    std::array<ian::EnemySpawn, 10> roleSpawns{};
    for (ian::EnemySpawn& spawn : roleSpawns) {
        spawn = {ian::EnemyType::Basic, {0.0, 0.8, -8.0}};
    }
    roleEnemies.spawnWave(roleSpawns);
    const auto countRole = [&roleEnemies](ian::EnemyApproachRole role) {
        return std::ranges::count_if(
            roleEnemies.enemies(),
            [role](const ian::EnemyInstance& enemy) {
                return enemy.active && enemy.approachRole == role;
            });
    };
    require(
        countRole(ian::EnemyApproachRole::Direct) == 6 &&
            countRole(ian::EnemyApproachRole::FlankLeft) == 2 &&
            countRole(ian::EnemyApproachRole::FlankRight) == 2,
        "basic melee roles use a deterministic 60/20/20 distribution");
    ian::FlowField flowField;
    flowField.rebuild({0, 0}, buildings.buildings());

    ian::EnemySystem farEnemies;
    constexpr std::array<ian::EnemySpawn, 1> FarSpawn{{
        {ian::EnemyType::Basic, {0.0, 0.8, -60.0}},
    }};
    farEnemies.spawnWave(FarSpawn);
    farEnemies.tick(
        1.0 / 60.0, buildings.buildings(), flowField);
    const double firstFarPosition =
        farEnemies.enemies().front().position.z;
    require(
        farEnemies.performanceStats().structureGridRebuilds == 1U,
        "enemy structure grid builds on its first tick");
    farEnemies.tick(
        1.0 / 60.0, buildings.buildings(), flowField);
    require(
        farEnemies.performanceStats().structureGridRebuilds == 0U,
        "unchanged structures reuse the cached enemy grid");
    const double secondFarPosition =
        farEnemies.enemies().front().position.z;
    farEnemies.tick(
        1.0 / 60.0, buildings.buildings(), flowField);
    require(
        farEnemies.performanceStats().fullAiUpdates == 0U &&
            farEnemies.performanceStats().throttledAiMoves == 1U &&
            farEnemies.enemies().front().position.z >
                secondFarPosition &&
            secondFarPosition > firstFarPosition,
        "far enemy AI is throttled while movement stays continuous");

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

    ian::EnemySystem traversingEnemies;
    traversingEnemies.spawnWave(FoundationSpawn);
    auto walkableFoundation = foundationTargets;
    walkableFoundation.front().traversable = true;
    const auto traversalAttacks = traversingEnemies.tick(
        1.0 / 60.0, buildings.buildings(),
        flowField, std::nullopt, walkableFoundation);
    require(
        traversalAttacks.empty(),
        "enemy never attacks a walkable platform surface");

    auto supportCollisionEnemy =
        traversingEnemies.enemies().front();
    supportCollisionEnemy.position = {0.0, 0.8, 0.0};
    supportCollisionEnemy.worldSurfaceHeight = 0.0;
    const std::array<ian::EnemyStructureTarget, 1>
        platformSupport{{{
            .id = {12001U, 1U},
            .position = {0.0, 0.0, 0.0},
            .radius = 0.14,
            .modular = true,
            .maximumEnemySurfaceHeight = 1.9,
            .attackable = false,
        }}};
    std::array<ian::EnemyInstance, 1> belowPlatform{{
        supportCollisionEnemy,
    }};
    ian::resolveEnemyCapsuleCollisions(
        belowPlatform, platformSupport);
    require(
        std::hypot(
            belowPlatform.front().position.x,
            belowPlatform.front().position.z) >= 0.73,
        "enemy capsule cannot overlap platform support below floor");
    std::array<ian::EnemyInstance, 1> abovePlatform{{
        supportCollisionEnemy,
    }};
    abovePlatform.front().worldSurfaceHeight = 2.0;
    ian::resolveEnemyCapsuleCollisions(
        abovePlatform, platformSupport);
    require(
        std::hypot(
            abovePlatform.front().position.x,
            abovePlatform.front().position.z) < 1e-9,
        "platform support does not repel enemy standing above it");

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

    {
        ian::TerrainHeightfield traversalTerrain;
        traversalTerrain.generate(90210U);
        const double lowerFloor =
            traversalTerrain.getHeight(1.0, 1.0) + 0.10;
        const std::array<ian::PlatformFrameInstance, 4>
            traversalFrames{{
                {
                    .id = {12100U, 1U},
                    .anchor = {0, 0, 0},
                    .floorHeight = lowerFloor,
                    .storey = 0,
                },
                {
                    .id = {12101U, 1U},
                    .anchor = {0, 0, 6},
                    .floorHeight = lowerFloor + 4.0,
                    .storey = 1,
                },
                {
                    .id = {12103U, 1U},
                    .anchor = {0, 0, 8},
                    .floorHeight = lowerFloor + 4.0,
                    .storey = 1,
                },
                {
                    .id = {12104U, 1U},
                    .anchor = {0, 0, 10},
                    .floorHeight = lowerFloor + 4.0,
                    .storey = 1,
                },
            }};
        const std::array<ian::RampInstance, 1>
            traversalRamps{{
                {
                    .id = {12102U, 1U},
                    .anchor = {0, 0, 2},
                    .rotation = ian::Rotation::Deg0,
                    .bottomHeight = lowerFloor,
                    .topHeight = lowerFloor + 4.0,
                    .targetStorey = 1,
                },
            }};
        ian::CollisionWorld traversalCollision;
        traversalCollision.syncModularBuildings({
            traversalFrames,
            {},
            traversalRamps,
            1.0,
        });
        ian::BuildingSystem elevatedBuildings;
        const auto elevatedCore = elevatedBuildings.place(
            ian::BuildingType::Core, {1, 11}, 0,
            1000, 1000, 1000,
            lowerFloor + 4.0, 1, lowerFloor + 4.0);
        require(
            elevatedCore.has_value(),
            "multi-level enemy fixture creates elevated core");
        ian::FlowField elevatedFlow;
        elevatedFlow.rebuild(
            {1, 11}, elevatedBuildings.buildings());

        ian::EnemySystem climbingEnemies;
        const std::array<ian::Vec3, 1> climbingSpawn{{
            {1.0, 0.8, 1.0},
        }};
        climbingEnemies.spawnWave(climbingSpawn);
        bool attackedElevatedCore = false;
        bool reactedToPlayerUpstairs = false;
        bool testedPlayerUpstairs = false;
        double maximumSurfaceOffset = 0.0;
        for (int tick = 0;
             tick < 1200 && !attackedElevatedCore; ++tick) {
            const ian::EnemyInstance& beforeTick =
                climbingEnemies.enemies().front();
            const bool probePlayer =
                !testedPlayerUpstairs &&
                beforeTick.surfaceHeightOffset > 3.0 &&
                beforeTick.position.z < 8.5;
            const std::optional<ian::Vec3> nearbyPlayer = probePlayer
                ? std::optional<ian::Vec3>{{
                      beforeTick.position.x,
                      lowerFloor + 5.7,
                      beforeTick.position.z,
                  }}
                : std::nullopt;
            const auto attacks = climbingEnemies.tick(
                1.0 / 60.0,
                elevatedBuildings.buildings(), elevatedFlow,
                nearbyPlayer, {}, &traversalTerrain,
                {
                    traversalFrames,
                    traversalRamps,
                    1.0,
                    &traversalCollision,
                });
            if (probePlayer) {
                testedPlayerUpstairs = true;
                reactedToPlayerUpstairs =
                    climbingEnemies.enemies().front().state ==
                        ian::EnemyState::AttackPlayer ||
                    climbingEnemies.enemies().front().state ==
                        ian::EnemyState::ChasePlayer;
            }
            maximumSurfaceOffset = std::max(
                maximumSurfaceOffset,
                climbingEnemies.enemies().front()
                    .surfaceHeightOffset);
            attackedElevatedCore = std::ranges::any_of(
                attacks,
                [&](const ian::EnemyAttack& attack) {
                    return attack.targetId ==
                        elevatedCore->building.id;
                });
        }
        require(
            maximumSurfaceOffset > 3.0,
            "ground enemy climbs modular ramp to upper storey");
        require(
            testedPlayerUpstairs && reactedToPlayerUpstairs,
            "enemy reacts to a nearby player on the same upper storey");
        require(
            attackedElevatedCore,
            "enemy reaches and attacks core through multi-level route");
        double minimumAttackSurface =
            std::numeric_limits<double>::infinity();
        double maximumAttackSurface =
            -std::numeric_limits<double>::infinity();
        int sustainedCoreAttacks = 0;
        for (int tick = 0; tick < 180; ++tick) {
            const auto attacks = climbingEnemies.tick(
                1.0 / 60.0,
                elevatedBuildings.buildings(), elevatedFlow,
                std::nullopt, {}, &traversalTerrain,
                {
                    traversalFrames,
                    traversalRamps,
                    1.0,
                    &traversalCollision,
                });
            const ian::EnemyInstance& attacker =
                climbingEnemies.enemies().front();
            minimumAttackSurface = std::min(
                minimumAttackSurface,
                attacker.worldSurfaceHeight);
            maximumAttackSurface = std::max(
                maximumAttackSurface,
                attacker.worldSurfaceHeight);
            sustainedCoreAttacks += static_cast<int>(
                std::ranges::count_if(
                    attacks,
                    [&](const ian::EnemyAttack& attack) {
                        return attack.targetId ==
                            elevatedCore->building.id;
                    }));
        }
        requireNear(
            minimumAttackSurface,
            elevatedCore->building.baseHeight, 1e-9,
            "core attacker stays on its platform working plane");
        requireNear(
            maximumAttackSurface,
            elevatedCore->building.baseHeight, 1e-9,
            "core attack cannot restart a platform hop");
        require(
            sustainedCoreAttacks > 0,
            "elevated core attack continues while height is locked");
    }

    std::vector<ian::EnemyInstance> modularOverlap{
        stackedEnemies.enemies().front()};
    modularOverlap.front().position = {0.0, 0.8, -3.0};
    const std::array<ian::EnemyStructureTarget, 1>
        lowerCollisionTarget{{stackedTargets.front()}};
    ian::resolveEnemyCapsuleCollisions(
        modularOverlap, lowerCollisionTarget);
    const double modularSeparation = std::hypot(
        modularOverlap.front().position.x,
        modularOverlap.front().position.z + 3.0);
    require(
        modularSeparation + 1e-9 >= 1.70,
        "modular foundations physically repel overlapping enemies");

    modularOverlap.front().position = {0.0, 0.8, -3.0};
    const std::array<ian::EnemyStructureTarget, 1>
        upperCollisionTarget{{stackedTargets.back()}};
    ian::resolveEnemyCapsuleCollisions(
        modularOverlap, upperCollisionTarget);
    require(
        std::hypot(
            modularOverlap.front().position.x,
            modularOverlap.front().position.z + 3.0) < 1e-9,
        "upper-storey structures do not repel ground enemies");

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

    ian::EnemySystem capsuleAimEnemies;
    constexpr std::array<ian::Vec3, 1> CapsuleAimSpawn{{
        {0.0, 0.8, 0.0},
    }};
    capsuleAimEnemies.spawnWave(CapsuleAimSpawn);
    require(
        capsuleAimEnemies.raycast(
            {0.0, 1.9, -4.0}, {0.0, 0.0, 1.0}, 6.0)
            .has_value(),
        "enemy raycast covers the full vertical capsule");

    ian::WorldConfig aimTerrainConfig =
        ian::WorldConfig::defaults();
    aimTerrainConfig.terrainResolution = 33;
    aimTerrainConfig.terrainWorldSize = 32.0;
    aimTerrainConfig.coreFlatRadius = 0.0;
    aimTerrainConfig.terrainBuildPlateauRadius = 0.0;
    aimTerrainConfig.terrainFeatureSize = 12.0;
    aimTerrainConfig.terrainAmplitude = 10.0;
    aimTerrainConfig.terrainBoundaryRiseWidth = 8.0;
    aimTerrainConfig.terrainBoundaryRiseHeight = 14.0;
    ian::TerrainHeightfield aimTerrain{aimTerrainConfig};
    ian::Vec3 elevatedSpawn{};
    double largestHeightMagnitude = 0.0;
    for (int z = -14; z <= 14; ++z) {
        for (int x = -14; x <= 14; ++x) {
            const double height = aimTerrain.getHeight(x, z);
            if (std::abs(height) > largestHeightMagnitude) {
                largestHeightMagnitude = std::abs(height);
                elevatedSpawn = {
                    static_cast<double>(x), 0.8,
                    static_cast<double>(z)};
            }
        }
    }
    require(
        largestHeightMagnitude > 1.5,
        "enemy aim terrain fixture contains a meaningful elevation");
    ian::EnemySystem terrainAimEnemies;
    terrainAimEnemies.spawnWave(
        std::span<const ian::Vec3>{&elevatedSpawn, 1});
    const double elevatedWorldY =
        aimTerrain.getHeight(elevatedSpawn.x, elevatedSpawn.z) +
        elevatedSpawn.y;
    const ian::Vec3 elevatedRayOrigin{
        elevatedSpawn.x, elevatedWorldY,
        elevatedSpawn.z - 4.0};
    require(
        !terrainAimEnemies.raycast(
             elevatedRayOrigin, {0.0, 0.0, 1.0}, 6.0)
             .has_value() &&
            terrainAimEnemies.raycast(
                elevatedRayOrigin, {0.0, 0.0, 1.0}, 6.0,
                &aimTerrain)
                .has_value(),
        "enemy raycast follows rendered terrain elevation");

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
    // Pink Blobs retain compact physical crowd spacing even though their
    // combat hit capsule is intentionally much more forgiving.
    constexpr double requiredDistance = 0.60 + 0.60;
    require(
        separatedDistance + 1e-6 >= requiredDistance,
        "enemy capsules cannot overlap after movement");
    const ian::EnemyCapsule basicCapsule =
        ian::enemyCapsule(ian::EnemyType::Basic);
    require(
        std::abs(basicCapsule.radius - 0.8625) < 1e-9 &&
            std::abs(basicCapsule.segmentHalfHeight - 0.506) < 1e-9,
        "basic Pink Blob uses its fifteen-percent larger gameplay collider");
    const ian::EnemyCapsule fastCapsule =
        ian::enemyCapsule(ian::EnemyType::Fast);
    require(
        std::abs(fastCapsule.radius - 0.408) < 1e-9 &&
            std::abs(fastCapsule.segmentHalfHeight - 0.312) < 1e-9,
        "fast Ninja uses its twenty-percent larger gameplay collider");
    const ian::EnemyCapsule heavyCapsule =
        ian::enemyCapsule(ian::EnemyType::Heavy);
    require(
        std::abs(heavyCapsule.radius - 0.84) < 1e-9 &&
            std::abs(heavyCapsule.segmentHalfHeight - 0.63) < 1e-9,
        "heavy Mushnub uses its fifty-percent larger gameplay collider");

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

    ian::EnemySystem splitterEnemies;
    constexpr std::array<ian::EnemySpawn, 1> SplitterSpawn{{{
        .type = ian::EnemyType::Splitter,
        .position = {0.0, 1.05, -5.0},
        .healthMultiplier = 1.5,
        .damageMultiplier = 1.25,
    }}};
    splitterEnemies.spawnWave(SplitterSpawn);
    const ian::EntityId splitterId =
        splitterEnemies.enemies().front().id;
    const auto splitterDeath =
        splitterEnemies.damage(splitterId, 1000.0);
    require(
        splitterDeath && splitterDeath->killed &&
            splitterEnemies.activeCount() == 0 &&
            splitterEnemies.enemies().front().splitAnimationRemaining > 0.0,
        "splitter death starts a readable split windup");
    require(
        splitterEnemies.takeSplitEvents().empty(),
        "split event waits until the windup finishes");
    splitterEnemies.tick(
        0.4, buildings.buildings(), flowField);
    const auto splitEvents = splitterEnemies.takeSplitEvents();
    require(
        splitEvents.size() == 1U &&
            splitEvents.front().parentId == splitterId &&
            splitEvents.front().childCount == 3,
        "splitter emits one confirmed split event");
    std::optional<ian::EntityId> firstSplitling;
    for (const ian::EnemyInstance& enemy : splitterEnemies.enemies()) {
        if (!enemy.active) {
            continue;
        }
        require(
            enemy.type == ian::EnemyType::Splitling &&
                std::abs(enemy.maxHealth - 4.5) < 1e-9 &&
                std::abs(enemy.damage - 8.75) < 1e-9 &&
                std::hypot(enemy.knockbackVelocity.x,
                           enemy.knockbackVelocity.z) > 2.7,
            "split children inherit scaling and launch outwards");
        firstSplitling = enemy.id;
    }
    require(firstSplitling.has_value(), "split creates targetable children");
    require(
        splitterEnemies.damage(*firstSplitling, 1000.0)->killed &&
            splitterEnemies.takeSplitEvents().empty(),
        "split children cannot recursively split");

    ian::EnemySystem crowdedSplitters;
    std::vector<ian::EnemySpawn> crowdedSpawns(
        ian::EnemySystem::MaxActiveEnemies,
        {ian::EnemyType::Basic, {0.0, 0.8, -8.0}});
    crowdedSpawns.front().type = ian::EnemyType::Splitter;
    crowdedSplitters.spawnWave(crowdedSpawns);
    const ian::EntityId crowdedSplitterId =
        crowdedSplitters.enemies().front().id;
    require(
        crowdedSplitters.damage(crowdedSplitterId, 1000.0)->killed,
        "crowded splitter enters its split windup");
    crowdedSplitters.tick(
        0.4, buildings.buildings(), flowField);
    require(
            crowdedSplitters.activeCount() ==
                ian::EnemySystem::MaxActiveEnemies + 2U,
        "splitter always creates all three children at the active cap");

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
    bool rangedLaunchedAtWall = false;
    bool rangedAttackedWall = false;
    for (int tick = 0; tick < 240 && !rangedAttackedWall;
         ++tick) {
        const auto attacks = rangedEnemy.tick(
            1.0 / 60.0, buildings.buildings(), flowField);
        if (!rangedLaunchedAtWall &&
            !rangedEnemy.projectiles().empty()) {
            rangedLaunchedAtWall = true;
            require(attacks.empty(),
                    "ranged structure shot deals no instant damage");
        }
        rangedAttackedWall = std::ranges::any_of(
            attacks, [&core](const ian::EnemyAttack& attack) {
                return attack.targetId != core->building.id;
            });
    }
    require(rangedLaunchedAtWall && rangedAttackedWall &&
                rangedEnemy.enemies().front().position.z <
                    -4.0,
            "ranged enemy projectile reaches blocker from stand-off distance");

    ian::BuildingSystem sideTargetBuildings;
    const auto sideCore = sideTargetBuildings.place(
        ian::BuildingType::Core, {6, -6}, 0, 30, 0);
    require(sideCore.has_value(),
            "ranged radial targeting fixture creates side target");
    ian::FlowField sideTargetFlow;
    sideTargetFlow.rebuild(
        {6, -6}, sideTargetBuildings.buildings());
    ian::EnemySystem sideTargetRanged;
    constexpr std::array<ian::EnemySpawn, 1> SideTargetSpawn{{{
        ian::EnemyType::Ranged, {0.0, 0.85, -6.0}}}};
    sideTargetRanged.spawnWave(SideTargetSpawn);
    sideTargetRanged.tick(
        1.0 / 60.0, sideTargetBuildings.buildings(),
        sideTargetFlow);
    require(
        sideTargetRanged.projectiles().size() == 1U &&
            sideTargetRanged.projectiles().front().targetId ==
                sideCore->building.id &&
            sideTargetRanged.enemies().front().position.x < 0.05,
        "ranged enemy acquires an off-axis structure at full firing range");

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

    const auto phaseTwoDamage = ramBoss.damage(
        ramBoss.enemies()[0].id, 25.0);
    require(phaseTwoDamage && phaseTwoDamage->remainingHealth == 45.0,
            "boss phase fixture crosses phase two threshold");
    ramBoss.tick(
        1.0 / 60.0, ramBuildings.buildings(), ramFlowField,
        ian::Vec3{0.0, 1.7, -3.0});
    auto bossActions = ramBoss.takeBossActionEvents();
    require(
        bossActions.size() == 1 &&
            bossActions[0].type == ian::BossActionType::PhaseChanged &&
            bossActions[0].phase == 2 &&
            ramBoss.enemies()[0].state ==
                ian::EnemyState::BossPhaseTransition,
        "boss visibly enters phase two at two thirds health");
    ramBoss.tick(
        1.05, ramBuildings.buildings(), ramFlowField,
        ian::Vec3{0.0, 1.7, -3.0});
    ramBoss.tick(
        1.0 / 60.0, ramBuildings.buildings(), ramFlowField,
        ian::Vec3{0.0, 1.7, -3.0});
    require(
        ramBoss.enemies()[0].state ==
            ian::EnemyState::BossSlamWindup,
        "phase two boss telegraphs an anti-player ground slam");
    ramBoss.tick(
        1.25, ramBuildings.buildings(), ramFlowField,
        ian::Vec3{0.0, 1.7, -3.0});
    bossActions = ramBoss.takeBossActionEvents();
    require(
        bossActions.size() == 1 &&
            bossActions[0].type == ian::BossActionType::GroundSlam &&
            bossActions[0].radius == 5.25 &&
            bossActions[0].damage == 28.0,
        "phase two slam reports its telegraphed radius and damage");

    const auto phaseThreeDamage = ramBoss.damage(
        ramBoss.enemies()[0].id, 25.0);
    require(phaseThreeDamage && phaseThreeDamage->remainingHealth == 20.0,
            "boss phase fixture crosses phase three threshold");
    ramBoss.tick(
        1.0 / 60.0, ramBuildings.buildings(), ramFlowField,
        ian::Vec3{0.0, 1.7, -3.0});
    bossActions = ramBoss.takeBossActionEvents();
    require(
        bossActions.size() == 1 &&
            bossActions[0].type == ian::BossActionType::PhaseChanged &&
            bossActions[0].phase == 3,
        "boss visibly enters phase three at one third health");
    ramBoss.tick(
        1.05, ramBuildings.buildings(), ramFlowField,
        ian::Vec3{0.0, 1.7, -3.0});
    require(
        ramBoss.enemies()[0].state ==
            ian::EnemyState::BossWarCryWindup,
        "phase three transition chains into a war cry telegraph");
    ramBoss.tick(
        1.35, ramBuildings.buildings(), ramFlowField,
        ian::Vec3{0.0, 1.7, -3.0});
    bossActions = ramBoss.takeBossActionEvents();
    require(
        bossActions.size() == 1 &&
            bossActions[0].type == ian::BossActionType::WarCry,
        "phase three boss calls reinforcements after the telegraph");

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

    ian::EnemySystem differentFloorEnemies;
    differentFloorEnemies.spawnWave(ContactSpawn);
    differentFloorEnemies.tick(
        1.0 / 60.0, buildings.buildings(), flowField,
        ian::Vec3{4.0, 5.7, 4.0});
    require(
        differentFloorEnemies.playerAttacks().empty() &&
            differentFloorEnemies.enemies()[0].state !=
                ian::EnemyState::AttackPlayer,
        "enemy cannot attack or aggro a player on another storey");

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

    ian::BuildingSystem projectileBuildings;
    require(projectileBuildings.place(
                ian::BuildingType::Core, {20, 20}, 0, 30, 0).has_value(),
            "ranged projectile fixture creates distant core");
    ian::FlowField projectileFlow;
    projectileFlow.rebuild({20, 20}, projectileBuildings.buildings());
    ian::EnemySystem projectileEnemy;
    constexpr std::array<ian::EnemySpawn, 1> ProjectileSpawn{{{
        ian::EnemyType::Ranged, {0.0, 0.85, -6.0}}}};
    projectileEnemy.spawnWave(ProjectileSpawn);
    projectileEnemy.tick(
        1.0 / 60.0, projectileBuildings.buildings(), projectileFlow,
        ian::Vec3{0.0, 1.7, 0.0});
    require(
        projectileEnemy.playerAttacks().empty() &&
            projectileEnemy.projectiles().size() == 1U,
        "ranged enemy launches a projectile instead of instant damage");
    bool projectileHit = false;
    for (int tick = 0; tick < 120 && !projectileHit; ++tick) {
        projectileEnemy.tick(
            1.0 / 60.0, projectileBuildings.buildings(), projectileFlow,
            ian::Vec3{0.0, 1.7, 0.0});
        projectileHit = !projectileEnemy.playerAttacks().empty();
    }
    require(projectileHit,
            "ranged projectile damages player only after travel time");

    ian::EnemySystem predictiveRanged;
    predictiveRanged.spawnWave(ProjectileSpawn);
    predictiveRanged.tick(
        0.01, projectileBuildings.buildings(), projectileFlow,
        ian::Vec3{0.0, 1.7, 0.0});
    predictiveRanged.clearProjectiles();
    const double initialRangedX =
        predictiveRanged.enemies().front().position.x;
    predictiveRanged.tick(
        1.5, projectileBuildings.buildings(), projectileFlow,
        ian::Vec3{3.0, 1.7, 0.0});
    require(
        !predictiveRanged.projectiles().empty() &&
            predictiveRanged.projectiles().back().targetPosition.x > 3.5,
        "ranged enemy leads a moving player instead of firing at old position");
    require(
        std::abs(
            predictiveRanged.enemies().front().position.x -
            initialRangedX) > 0.05,
        "ranged enemy repositions while maintaining firing distance");

    projectileEnemy.tick(
        1.5, projectileBuildings.buildings(), projectileFlow,
        ian::Vec3{0.0, 1.7, 0.0});
    projectileEnemy.clearProjectiles();
    require(projectileEnemy.projectiles().empty(),
            "enemy projectiles can be cleared at a phase boundary");

    ian::EnemySystem replacementWaveEnemy;
    replacementWaveEnemy.spawnWave(ProjectileSpawn);
    replacementWaveEnemy.tick(
        1.0 / 60.0, projectileBuildings.buildings(), projectileFlow,
        ian::Vec3{0.0, 1.7, 0.0});
    require(!replacementWaveEnemy.projectiles().empty(),
            "replacement-wave fixture launches a projectile");
    replacementWaveEnemy.spawnWave(ProjectileSpawn);
    require(replacementWaveEnemy.projectiles().empty(),
            "starting a new wave clears old enemy projectiles");

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
    const std::size_t blastColumns = static_cast<std::size_t>(
        std::ceil(std::sqrt(static_cast<double>(
            ian::EnemySystem::MaxActiveEnemies))));
    const double blastCenter =
        static_cast<double>(blastColumns - 1U) * 0.1;
    for (std::size_t index = 0;
         index < ian::EnemySystem::MaxActiveEnemies; ++index) {
        blastSpawns.push_back({
            .type = ian::EnemyType::Basic,
            .position = {
                static_cast<double>(index % blastColumns) * 0.2 -
                    blastCenter,
                0.8,
                static_cast<double>(index / blastColumns) * 0.2 -
                    blastCenter,
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

    constexpr std::array<ian::EnemySpawn, 2> CloseBlastSpawns{{
        {ian::EnemyType::Basic, {0.0, 0.8, 0.0}},
        {ian::EnemyType::Heavy, {1.35, 1.0, 0.0}},
    }};
    ian::EnemySystem closeBlastEnemies;
    closeBlastEnemies.spawnWave(CloseBlastSpawns);
    const auto closeBlastDamage = closeBlastEnemies.damageInRadius(
        {0.0, 0.8, 0.0}, 1.25, 1.0, 3.0);
    require(
        closeBlastDamage.size() == 2U &&
            closeBlastEnemies.enemies()[0].health <
                closeBlastEnemies.enemies()[0].maxHealth &&
            closeBlastEnemies.enemies()[1].health <
                closeBlastEnemies.enemies()[1].maxHealth,
        "area damage reaches touching enemy capsules");
    require(
        std::abs(closeBlastEnemies.enemies()[0].knockbackVelocity.x) +
                std::abs(closeBlastEnemies.enemies()[0].knockbackVelocity.z) >
            2.9,
        "area damage keeps full impulse at impact center");
    require(
        std::abs(closeBlastEnemies.enemies()[1].knockbackVelocity.x) +
                std::abs(closeBlastEnemies.enemies()[1].knockbackVelocity.z) >
            0.1,
        "area damage pushes a capsule touching the radius edge");

    constexpr std::array<ian::EnemySpawn, 3> CappedBlastSpawns{{
        {ian::EnemyType::Basic, {-0.8, 0.8, 0.0}},
        {ian::EnemyType::Basic, {0.0, 0.8, 0.0}},
        {ian::EnemyType::Basic, {0.8, 0.8, 0.0}},
    }};
    ian::EnemySystem cappedBlastEnemies;
    cappedBlastEnemies.spawnWave(CappedBlastSpawns);
    const auto cappedBlastDamage = cappedBlastEnemies.damageInRadius(
        {0.0, 0.8, 0.0}, 1.25, 4.0, 0.0, std::nullopt, 5.0);
    double totalCappedDamage = 0.0;
    for (const auto& result : cappedBlastDamage) {
        totalCappedDamage += result.damage;
    }
    require(
        cappedBlastDamage.size() == 3U &&
            totalCappedDamage <= 5.0 + 1e-9,
        "area damage respects a total damage cap");

    blastEnemies.spawnGroup(Replacement);
    require(
        blastEnemies.enemies().front().active &&
            blastEnemies.enemies().front().id.index ==
                firstBlastId.index &&
            blastEnemies.enemies().front().id.generation ==
                firstBlastId.generation + 1,
        "area damage preserves generation-safe pool reuse");
}
