#include "TestHarness.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "game/Simulation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>

namespace {
void unlockAxe(ian::Simulation& simulation) {
    simulation.grantSkillPoints(1, ian::SkillPointSource::Event);
    const auto axe = simulation.skillTree().indexOf("axe");
    require(axe && simulation.purchaseSkill(*axe) == ian::SkillPurchaseError::None,
            "test fixture unlocks axe");
}
void unlockHammer(ian::Simulation& simulation) {
    simulation.grantSkillPoints(1, ian::SkillPointSource::Event);
    const auto hammer = simulation.skillTree().indexOf("hammer");
    require(hammer && simulation.purchaseSkill(*hammer) == ian::SkillPurchaseError::None,
            "test fixture unlocks hammer");
}
}

void runSimulationTests() {
    {
        ian::Simulation defenseProgression;
        defenseProgression.startRun();
        const auto initial = defenseProgression.snapshot();
        require(
            initial.unlockedBuildings[static_cast<std::size_t>(
                ian::BuildingType::GunTurret)] &&
                !initial.unlockedBuildings[static_cast<std::size_t>(
                    ian::BuildingType::Turret)] &&
                !initial.unlockedBuildings[static_cast<std::size_t>(
                    ian::BuildingType::Cannon)],
            "only the starter turret is exposed before defense research");
        defenseProgression.grantSkillPoints(
            3, ian::SkillPointSource::Event);
        const auto hammer =
            defenseProgression.skillTree().indexOf("hammer");
        const auto engineering =
            defenseProgression.skillTree().indexOf("defense_engineering");
        const auto crossbow =
            defenseProgression.skillTree().indexOf("crossbow_unlock");
        require(
            hammer && engineering && crossbow &&
                defenseProgression.purchaseSkill(*hammer) ==
                    ian::SkillPurchaseError::None &&
                defenseProgression.purchaseSkill(*engineering) ==
                    ian::SkillPurchaseError::None &&
                defenseProgression.purchaseSkill(*crossbow) ==
                    ian::SkillPurchaseError::CoreLevelRequired,
            "crossbow research cannot bypass its core level requirement");
    }
    {
        ian::Simulation rotationSimulation;
        rotationSimulation.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        rotationSimulation.tick(1.0 / 60.0, unlimited);

        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::Core,
            .gridPosition = {0, 0},
        };
        rotationSimulation.tick(1.0 / 60.0, placeCore);
        ian::PlayerCommand placeTurret;
        placeTurret.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::Turret,
            .gridPosition = {0, -4},
        };
        rotationSimulation.tick(1.0 / 60.0, placeTurret);
        const auto placed = rotationSimulation.snapshot();
        const auto turret = std::ranges::find(
            placed.buildings, ian::BuildingType::Turret,
            &ian::BuildingInstance::type);
        require(turret != placed.buildings.end(),
                "rotation fixture places directional defense");

        ian::PlayerCommand rotate;
        rotate.rotatePlacedBuilding =
            ian::RotatePlacedBuildingCommand{
                .buildingId = turret->id,
                .steps = 1,
            };
        rotationSimulation.tick(1.0 / 60.0, rotate);
        const auto rotated = rotationSimulation.snapshot();
        require(rotated.towers.size() == 1,
                "rotation fixture keeps tower runtime");
        require(rotated.towers.front().yaw > 0.0 &&
                    rotated.towers.front().yaw <
                        ian::PiRadians * 0.25,
                "daytime placed defense visibly eases after rotation");
        requireNear(rotated.towers.front().baseYaw,
                    rotated.towers.front().yaw, 1e-9,
                    "sector and daytime tower share smoothed rotation");
    }
    {
        ian::Simulation wallDragFoundations;
        wallDragFoundations.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        wallDragFoundations.tick(1.0 / 60.0, unlimited);

        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::Core,
            .gridPosition = {0, 0},
            .rotation = 0,
        };
        wallDragFoundations.tick(1.0 / 60.0, placeCore);
        require(
            wallDragFoundations.snapshot().coreId.has_value(),
            "wall drag foundation fixture creates core");

        constexpr ian::GridPosition FirstWall{0, 4};
        constexpr ian::GridPosition SecondWall{1, 4};
        ian::PlayerCommand placeFirstWall;
        placeFirstWall.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Wall,
                .gridPosition = FirstWall,
                .rotation = 0,
            };
        wallDragFoundations.tick(
            1.0 / 60.0, placeFirstWall);
        require(
            std::ranges::count_if(
                wallDragFoundations.snapshot().buildings,
                [](const ian::BuildingInstance& building) {
                    return building.type ==
                           ian::BuildingType::Wall;
                }) == 1,
            "wall drag foundation fixture places first wall");
        const auto sharedSurface =
            wallDragFoundations.previewPlacementSurface(
                ian::BuildingType::Wall, SecondWall);
        require(
            sharedSurface.storey == 0,
            "adjacent wall cell discovers the newly created foundation");
        ian::PlayerCommand placeSecondWall;
        placeSecondWall.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Wall,
                .gridPosition = SecondWall,
                .rotation = 0,
                .baseHeight = sharedSurface.height,
                .platformStorey = -1,
                .lockHeight = true,
            };
        wallDragFoundations.tick(
            1.0 / 60.0, placeSecondWall);

        const auto built = wallDragFoundations.snapshot();
        const auto wallCount = std::ranges::count_if(
            built.buildings,
            [](const ian::BuildingInstance& building) {
                return building.type ==
                       ian::BuildingType::Wall;
            });
        require(
            wallCount == 2,
            "adjacent dragged walls share their automatic foundation");
        require(
            std::ranges::all_of(
                built.buildings,
                [](const ian::BuildingInstance& building) {
                    return building.type !=
                               ian::BuildingType::Wall ||
                           building.platformStorey == 0;
                }),
            "every dragged wall remains attached to the shared foundation");
    }
    {
        auto sawBalance = ian::GameBalance::defaults();
        sawBalance.gameplay.pickaxeDamage = 10000.0;
        sawBalance.gameplay.pickaxeDamageVariation = 0.0;
        sawBalance.gameplay.pickaxeCriticalChance = 0.0;
        ian::Simulation sawSimulation{sawBalance};
        sawSimulation.startRun();
        sawSimulation.grantLootUpgrade(ian::LootUpgradeEffect::Saw);
        static_cast<void>(sawSimulation.takeEvents());
        const auto before = sawSimulation.snapshot();
        const auto tree = std::ranges::find_if(
            before.resourceNodes, [](const ian::ResourceNode& node) {
                return node.active &&
                    node.type == ian::ResourceType::Wood;
            });
        require(tree != before.resourceNodes.end(),
                "Saw fixture has a tree to destroy");
        ian::PlayerCommand destroyTree;
        destroyTree.overrideAimedResource = true;
        destroyTree.aimedResourceOverride = tree->id;
        destroyTree.usePickaxe = true;
        sawSimulation.tick(1.0 / 60.0, destroyTree);
        const auto launchEvents = sawSimulation.takeEvents();
        const auto splinterLaunch = std::ranges::find_if(
            launchEvents, [](const ian::GameEvent& event) {
                return event.type ==
                           ian::GameEventType::SawSplinterLaunched &&
                    event.entityId.has_value() &&
                    event.targetPosition.has_value();
            });
        require(
            splinterLaunch != launchEvents.end(),
            "destroying a tree with Saw launches visible saw blades");
        const auto splinterTarget = std::ranges::find(
            before.resourceNodes, *splinterLaunch->entityId,
            &ian::ResourceNode::id);
        require(
            splinterTarget != before.resourceNodes.end(),
            "Saw blade targets an existing resource");
        requireNear(
            splinterLaunch->damage,
            splinterTarget->maxHealth * 0.225, 1e-9,
            "first Saw stack deals the reduced saw blade damage");
        sawSimulation.tick(1.05);
        const auto impactEvents = sawSimulation.takeEvents();
        require(
            std::ranges::any_of(
                impactEvents, [&tree](const ian::GameEvent& event) {
                    return (event.type == ian::GameEventType::ResourceHit ||
                            event.type ==
                                ian::GameEventType::ResourceCollected) &&
                        event.sourceId == tree->id;
                }),
            "Saw blades harvest nearby resources after their flight");
    }
    {
        ian::GameBalance objectiveBalance =
            ian::GameBalance::defaults();
        objectiveBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)].wood = 0;
        ian::MapDefinition objectiveMap =
            ian::MapDefinition::defaults();
        objectiveMap.obstacles.clear();
        ian::WorldConfig objectiveWorld =
            ian::WorldConfig::defaults();
        objectiveWorld.terrainAmplitude = 0.0;
        ian::Simulation objectiveSimulation{
            objectiveBalance, objectiveMap, objectiveWorld};
        objectiveSimulation.startRun();
        static_cast<void>(objectiveSimulation.takeEvents());
        const auto naturalCoreSurface =
            objectiveSimulation.previewPlacementSurface(
                ian::BuildingType::Core, {0, 0});
        require(
            !objectiveSimulation.previewPlacement(
                 ian::BuildingType::Core, {0, 0},
                 naturalCoreSurface.height - 0.5).valid(),
            "an exact preview height below terrain is rejected instead of silently lifted");
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        objectiveSimulation.tick(1.0 / 60.0, placeCore);
        const auto objectiveSnapshot =
            objectiveSimulation.snapshot();
        require(
            objectiveSnapshot.challengeColumns.size() == 5U &&
                std::ranges::all_of(
                    objectiveSnapshot.challengeColumns,
                    [&objectiveSimulation](
                        const ian::ChallengeColumnInstance& column) {
                        return objectiveSimulation.terrain().isInside(
                                   column.position.x,
                                   column.position.z) &&
                               column.state ==
                                   ian::ChallengeColumnState::Dormant;
                    }),
            "a new run scatters five dormant skull trials on valid terrain");
        require(
            objectiveSnapshot.worldLandmarks.size() == 2U &&
                objectiveSnapshot.worldLandmarks[0].type !=
                    objectiveSnapshot.worldLandmarks[1].type &&
                std::hypot(
                    objectiveSnapshot.worldLandmarks[0].position.x -
                        objectiveSnapshot.worldLandmarks[1].position.x,
                    objectiveSnapshot.worldLandmarks[0].position.z -
                        objectiveSnapshot.worldLandmarks[1].position.z) >=
                    54.0,
            "mine and lumber mill spawn as separated world landmarks");
        ian::Simulation landmarkProductionSimulation;
        landmarkProductionSimulation.startRun();
        ian::PlayerCommand landmarkGodMode;
        landmarkGodMode.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        landmarkProductionSimulation.tick(
            1.0 / 60.0, landmarkGodMode);
        const auto landmarkSnapshot =
            landmarkProductionSimulation.snapshot();
        require(
            landmarkSnapshot.worldLandmarks.size() == 2U &&
                std::ranges::all_of(
                    landmarkSnapshot.worldLandmarks,
                    [](const ian::WorldLandmarkInstance& landmark) {
                        return !landmark.activated &&
                               landmark.activationCoinCost > 0;
                    }),
            "large resource landmarks begin locked behind a coin cost");
        for (const ian::WorldLandmarkInstance& landmark :
             landmarkSnapshot.worldLandmarks) {
            ian::PlayerCommand restore;
            restore.overrideAimedWorldLandmark = true;
            restore.aimedWorldLandmarkOverride = landmark.id;
            restore.interact = ian::InteractCommand{};
            landmarkProductionSimulation.tick(1.0 / 60.0, restore);
        }
        landmarkProductionSimulation.tick(
            1.0 / 60.0, landmarkGodMode);
        const int landmarkWoodBefore =
            landmarkProductionSimulation.snapshot().wood;
        const int landmarkStoneBefore =
            landmarkProductionSimulation.snapshot().stone;
        landmarkProductionSimulation.tick(10.0);
        require(
            landmarkProductionSimulation.snapshot().wood ==
                    landmarkWoodBefore + 6 &&
                landmarkProductionSimulation.snapshot().stone ==
                    landmarkStoneBefore + 4 &&
                std::ranges::all_of(
                    landmarkProductionSimulation.snapshot().worldLandmarks,
                    [](const ian::WorldLandmarkInstance& landmark) {
                        return landmark.activated;
                    }),
            "restored world landmarks produce wood and stone using passive production timing");
        require(
            objectiveSnapshot.platformFrames.size() == 1U &&
                objectiveSnapshot.platformFrames.front().anchor.x == -1 &&
                objectiveSnapshot.platformFrames.front().anchor.z == -1,
            "every terrain building receives an exactly aligned foundation");
        require(
            std::ranges::all_of(
                objectiveSnapshot.platformFrames.front().supports,
                [&objectiveWorld](
                    const ian::FoundationSupport& support) {
                    return support.length >=
                               objectiveWorld.minimumGroundClearance - 1e-6 &&
                           support.length < 0.20;
                }),
            "flat-ground building foundation leaves only a thin pallet top visible");
        ian::PlayerCommand enableFoundationTestResources;
        enableFoundationTestResources.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        objectiveSimulation.tick(
            1.0 / 60.0, enableFoundationTestResources);
        const auto upperFoundationPreview =
            objectiveSimulation.previewFloorPlatform(
                objectiveSnapshot.platformFrames.front().anchor,
                1,
                objectiveSnapshot.platformFrames.front().floorHeight +
                    ian::modularStoreyHeight(objectiveWorld));
        require(
            upperFoundationPreview.valid(),
            "thin automatic foundation still supports a full upper storey");
        const auto events = objectiveSimulation.takeEvents();
        require(
            std::ranges::any_of(
                events, [](const ian::GameEvent& event) {
                    return event.type ==
                               ian::GameEventType::ObjectiveCompleted &&
                        event.objectiveId == "buildings_1" &&
                        event.intensity == 4.0;
                }),
            "placing a building completes its small Insight objective");
    }
    {
        ian::GameBalance lightningBalance =
            ian::GameBalance::defaults();
        lightningBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)].wood = 0;
        lightningBalance.enemies[static_cast<std::size_t>(
            ian::EnemyType::Heavy)].health = 100.0;
        ian::MapDefinition lightningMap =
            ian::MapDefinition::defaults();
        lightningMap.obstacles.clear();
        ian::WorldConfig lightningWorld =
            ian::WorldConfig::defaults();
        lightningWorld.terrainAmplitude = 0.0;
        ian::Simulation lightningSimulation{
            lightningBalance, lightningMap, lightningWorld};
        lightningSimulation.startRun();
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        lightningSimulation.tick(1.0 / 60.0, placeCore);
        ian::PlayerCommand spawnEnemies;
        spawnEnemies.spawnEnemy = ian::SpawnEnemyCommand{
            ian::EnemyType::Heavy, 4};
        lightningSimulation.tick(1.0 / 60.0, spawnEnemies);
        const auto before = lightningSimulation.snapshot();
        require(before.enemies.size() >= 4,
                "chain lightning fixture spawns enemies");
        const ian::EntityId firstTarget = before.enemies.front().id;
        static_cast<void>(lightningSimulation.takeEvents());

        ian::PlayerCommand castLightning;
        castLightning.castChainLightning =
            ian::CastChainLightningCommand{
                .firstTarget = firstTarget,
                .damage = 20.0,
                .jumpRadius = 50.0,
                .damageFalloff = 0.5,
                .maximumTargets = 3,
            };
        lightningSimulation.tick(1.0 / 60.0, castLightning);
        const auto lightningEvents =
            lightningSimulation.takeEvents();
        std::vector<const ian::GameEvent*> hits;
        for (const ian::GameEvent& event : lightningEvents) {
            if (event.type ==
                ian::GameEventType::ChainLightningHit) {
                hits.push_back(&event);
            }
        }
        require(hits.size() == 3,
                "chain lightning respects its target cap");
        requireNear(hits[0]->damage, 20.0, 1e-9,
                    "first lightning target receives full damage");
        requireNear(hits[1]->damage, 10.0, 1e-9,
                    "lightning damage falls off on first jump");
        requireNear(hits[2]->damage, 5.0, 1e-9,
                    "lightning damage falls off on second jump");
        require(
            hits[0]->entityId != hits[1]->entityId &&
                hits[0]->entityId != hits[2]->entityId &&
                hits[1]->entityId != hits[2]->entityId,
            "chain lightning never strikes one enemy twice");
        require(
            !hits[0]->sourceId && hits[1]->sourceId &&
                hits[2]->sourceId &&
                hits[0]->targetPosition &&
                hits[1]->targetPosition &&
                hits[2]->targetPosition,
            "lightning events preserve every visual segment");
    }
    {
        ian::GameBalance defeatBalance =
            ian::GameBalance::defaults();
        defeatBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)].wood = 0;
        ian::MapDefinition defeatMap =
            ian::MapDefinition::defaults();
        defeatMap.obstacles.clear();
        ian::WorldConfig defeatWorld =
            ian::WorldConfig::defaults();
        defeatWorld.terrainAmplitude = 0.0;
        ian::Simulation defeatSimulation{
            defeatBalance, defeatMap, defeatWorld};
        defeatSimulation.startRun();
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        defeatSimulation.tick(1.0 / 60.0, placeCore);
        defeatSimulation.grantLootUpgrade(
            ian::LootUpgradeEffect::Hourglass,
            ian::LootRarity::Rare);
        ian::PlayerCommand startDefeatWave;
        startDefeatWave.startWaveEarly =
            ian::StartWaveEarlyCommand{};
        defeatSimulation.tick(
            1.0 / 60.0, startDefeatWave);
        require(
            defeatSimulation.snapshot().crystals > 0 &&
                defeatSimulation.snapshot().coins > 0,
            "defeat fixture carries both currencies");
        ian::PlayerCommand spawnEnemy;
        spawnEnemy.spawnEnemy = ian::SpawnEnemyCommand{
            ian::EnemyType::Basic, 5};
        defeatSimulation.tick(1.0 / 60.0, spawnEnemy);
        ian::PlayerCommand defeatEnemies;
        defeatEnemies.defeatAllEnemies =
            ian::DefeatAllEnemiesCommand{};
        defeatSimulation.tick(1.0 / 60.0, defeatEnemies);
        require(
            !defeatSimulation.snapshot().coinPickups.empty(),
            "defeat fixture creates existing physical coin drops");
        ian::PlayerCommand destroyCore;
        destroyCore.damageCore = ian::DamageCoreCommand{100000.0};
        defeatSimulation.tick(1.0 / 60.0, destroyCore);
        const auto defeatEvents = defeatSimulation.takeEvents();
        require(
            defeatSimulation.snapshot().state ==
                    ian::RunState::Defeat &&
                defeatSimulation.snapshot().crystals == 0 &&
                defeatSimulation.snapshot().coins == 0 &&
                defeatSimulation.snapshot().coinPickups.empty() &&
                std::ranges::any_of(
                    defeatEvents,
                    [](const ian::GameEvent& event) {
                        return event.type ==
                            ian::GameEventType::RunEnded;
                    }),
            "core destruction clears coin drops before automatic restart");
        defeatSimulation.restartRun();
        defeatSimulation.tick(1.0 / 60.0);
        require(
            defeatSimulation.snapshot().coins == 0 &&
                defeatSimulation.snapshot().crystals == 0 &&
                defeatSimulation.snapshot().coinPickups.empty(),
            "first restarted tick cannot reward enemies from the lost run");
    }
    {
        ian::GameBalance earlyBalance =
            ian::GameBalance::defaults();
        earlyBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)].wood = 0;
        ian::MapDefinition earlyMap =
            ian::MapDefinition::defaults();
        earlyMap.obstacles.clear();
        ian::WorldConfig earlyWorld =
            ian::WorldConfig::defaults();
        earlyWorld.terrainAmplitude = 0.0;
        ian::Simulation earlySimulation{
            earlyBalance, earlyMap, earlyWorld};
        earlySimulation.startRun();
        requireNear(
            earlySimulation.snapshot().phaseDuration,
            120.0, 1e-9,
            "first wave preparation lasts two minutes");
        earlySimulation.grantSkillPoints(
            7, ian::SkillPointSource::Event);
        constexpr std::array<const char*, 4> LongerDayPath{{
            "nightly_chest", "keymaster", "expanded_storage",
            "longer_days",
        }};
        bool unlockedLongerDays = true;
        for (const char* id : LongerDayPath) {
            const auto skill =
                earlySimulation.skillTree().indexOf(id);
            unlockedLongerDays = unlockedLongerDays && skill &&
                earlySimulation.purchaseSkill(*skill) ==
                    ian::SkillPurchaseError::None;
        }
        requireNear(
            earlySimulation.snapshot().phaseDuration,
            135.0, 1e-9,
            "Longer Days adds exactly fifteen seconds to current daytime");
        require(
            unlockedLongerDays,
            "Longer Days unlocks through its complete prerequisite path");
        ian::PlayerCommand placeFreeCore;
        placeFreeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        earlySimulation.tick(1.0 / 60.0, placeFreeCore);
        const auto twilightResource = std::ranges::find_if(
            earlySimulation.snapshot().resourceNodes,
            [](const ian::ResourceNode& node) {
                return node.active &&
                    ian::isHarvestableResource(node.type);
            });
        require(
            twilightResource !=
                earlySimulation.snapshot().resourceNodes.end(),
            "twilight fixture has a harvestable resource");
        const ian::EntityId twilightResourceId =
            twilightResource->id;
        const double twilightResourceHealth =
            twilightResource->health;
        const double remainingDay =
            earlySimulation.snapshot().phaseTimeRemaining;
        earlySimulation.tick(remainingDay);
        require(
            earlySimulation.snapshot().state ==
                    ian::RunState::Sunset &&
                earlySimulation.snapshot().phaseDuration ==
                    earlyBalance.gameplay.sunsetSeconds,
            "day expiry begins the full twilight construction window");
        ian::PlayerCommand twilightGather;
        twilightGather.overrideAimedResource = true;
        twilightGather.aimedResourceOverride =
            twilightResourceId;
        twilightGather.usePickaxe = true;
        earlySimulation.tick(1.0 / 60.0, twilightGather);
        const auto resourceAfterTwilightGather =
            std::ranges::find(
                earlySimulation.snapshot().resourceNodes,
                twilightResourceId,
                &ian::ResourceNode::id);
        require(
            resourceAfterTwilightGather !=
                    earlySimulation.snapshot().resourceNodes.end() &&
                resourceAfterTwilightGather->health <
                    twilightResourceHealth,
            "manual gathering remains available during twilight");
        ian::PlayerCommand skipTwilight;
        skipTwilight.startWaveEarly =
            ian::StartWaveEarlyCommand{};
        earlySimulation.tick(1.0 / 60.0, skipTwilight);
        require(
            earlySimulation.snapshot().state ==
                ian::RunState::Wave,
            "twilight can be skipped after the player finishes building");
        earlySimulation.restartRun();
        earlySimulation.grantSkillPoints(
            11, ian::SkillPointSource::Event);
        for (const char* id : LongerDayPath) {
            const auto skill =
                earlySimulation.skillTree().indexOf(id);
            if (skill) {
                static_cast<void>(
                    earlySimulation.purchaseSkill(*skill));
            }
        }
        earlySimulation.tick(1.0 / 60.0, placeFreeCore);
        earlySimulation.grantLootUpgrade(
            ian::LootUpgradeEffect::Hourglass,
            ian::LootRarity::Rare);
        const int hourglassBonus =
            earlySimulation.snapshot().earlyWaveBonus;
        const int hourglassCoins =
            earlySimulation.snapshot().earlyWaveCoinBonus;
        const int hourglassInsight =
            earlySimulation.snapshot().earlyWaveInsightBonus;
        require(hourglassBonus > 0,
                "preparation HUD advertises an early-wave bonus");
        require(
            hourglassCoins == hourglassBonus &&
                hourglassInsight == hourglassBonus,
            "Hourglass converts early-wave time into Coins and Insight");

        earlySimulation.grantSkillPoints(
            4, ian::SkillPointSource::Event);
        constexpr std::array<const char*, 2> EarlyPlanningPath{{
            "safe_delivery", "early_planning",
        }};
        bool unlockedEarlyPlanning = true;
        for (const char* id : EarlyPlanningPath) {
            const auto skill = earlySimulation.skillTree().indexOf(id);
            unlockedEarlyPlanning = unlockedEarlyPlanning && skill &&
                earlySimulation.purchaseSkill(*skill) ==
                    ian::SkillPurchaseError::None;
        }
        const int advertisedBonus =
            earlySimulation.snapshot().earlyWaveBonus;
        const int advertisedCoins =
            earlySimulation.snapshot().earlyWaveCoinBonus;
        const int advertisedInsight =
            earlySimulation.snapshot().earlyWaveInsightBonus;
        require(
            unlockedEarlyPlanning &&
                advertisedBonus > hourglassBonus &&
                advertisedCoins > advertisedBonus &&
                advertisedInsight == advertisedCoins,
            "Early Planning adds a data-driven reward source and multiplier");
        const double insightBeforeEarlyStart =
            earlySimulation.snapshot().currentInsight;
        ian::PlayerCommand startEarly;
        startEarly.startWaveEarly =
            ian::StartWaveEarlyCommand{};
        earlySimulation.tick(1.0 / 60.0, startEarly);
        require(
            earlySimulation.snapshot().crystals == advertisedBonus &&
                earlySimulation.snapshot().coins == advertisedCoins,
            "starting early grants advertised crystals and Coins");
        requireNear(
            earlySimulation.snapshot().currentInsight,
            insightBeforeEarlyStart + advertisedInsight + 6.0,
            1e-9,
            "starting early grants Hourglass and small-objective Insight");
        const auto earlyEvents = earlySimulation.takeEvents();
        require(
            std::ranges::any_of(
                earlyEvents,
                [advertisedBonus, advertisedCoins,
                 advertisedInsight](const ian::GameEvent& event) {
                    return event.type ==
                            ian::GameEventType::EarlyWaveBonusGranted &&
                        event.amount == advertisedBonus &&
                        event.coinAmount == advertisedCoins &&
                        event.insightAmount == advertisedInsight;
                }),
            "early-wave bonus emits a presentation event once");
        require(
            std::ranges::any_of(
                earlyEvents,
                [](const ian::GameEvent& event) {
                    return event.type ==
                               ian::GameEventType::ObjectiveCompleted &&
                        event.objectiveId == "early_waves_1" &&
                        event.intensity == 6.0;
                }),
            "early wave start completes its first objective step");
    }
    {
        ian::GameBalance blueprintBalance =
            ian::GameBalance::defaults();
        blueprintBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)].wood = 0;
        blueprintBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Wall)].wood = 0;
        ian::MapDefinition blueprintMap =
            ian::MapDefinition::defaults();
        blueprintMap.obstacles.clear();
        ian::WorldConfig blueprintWorld =
            ian::WorldConfig::defaults();
        blueprintWorld.terrainAmplitude = 0.0;
        ian::Simulation blueprintSimulation{
            blueprintBalance, blueprintMap, blueprintWorld};
        blueprintSimulation.startRun();
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        blueprintSimulation.tick(1.0 / 60.0, placeCore);
        const double beforeBlueprint =
            blueprintSimulation.snapshot().currentInsight;
        blueprintSimulation.grantLootUpgrade(
            ian::LootUpgradeEffect::Blueprint,
            ian::LootRarity::Rare);
        requireNear(
            blueprintSimulation.snapshot().currentInsight,
            beforeBlueprint + blueprintSimulation
                .insightSystem().config().firstBuildingTypeBonus,
            1e-9,
            "Blueprint immediately rewards already-built unique types");
        const double beforeWall =
            blueprintSimulation.snapshot().currentInsight;
        ian::PlayerCommand placeWall;
        placeWall.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Wall, {3, 0}, 0};
        blueprintSimulation.tick(1.0 / 60.0, placeWall);
        require(
            blueprintSimulation.snapshot().currentInsight >
                beforeWall + 2.0,
            "Blueprint rewards a newly built type plus normal building Insight");
    }
    {
        auto fallingSimulation = [](double spawnHeight,
                                    bool rope) {
            ian::MapDefinition map =
                ian::MapDefinition::defaults();
            map.playerSpawn.y = spawnHeight;
            map.obstacles.clear();
            ian::WorldConfig world =
                ian::WorldConfig::defaults();
            world.terrainAmplitude = 0.0;
            ian::Simulation simulation{
                ian::GameBalance::defaults(), map, world};
            simulation.startRun();
            if (rope) {
                simulation.grantLootUpgrade(
                    ian::LootUpgradeEffect::Rope,
                    ian::LootRarity::Rare);
            }
            for (int tick = 0; tick < 180; ++tick) {
                simulation.tick(1.0 / 60.0);
            }
            return simulation;
        };
        ian::Simulation smallFall =
            fallingSimulation(1.0, false);
        requireNear(
            smallFall.snapshot().playerHealth,
            smallFall.snapshot().playerMaxHealth, 1e-9,
            "small falls below safe speed deal no damage");
        ian::Simulation oneStoreyFall =
            fallingSimulation(3.5, false);
        requireNear(
            oneStoreyFall.snapshot().playerHealth,
            oneStoreyFall.snapshot().playerMaxHealth, 1e-9,
            "approximately one storey remains safe from fall damage");
        ian::Simulation fatalFall =
            fallingSimulation(12.0, false);
        require(
            fatalFall.snapshot().playerRespawning,
            "large unprotected falls can kill the player");
        ian::Simulation savedFall =
            fallingSimulation(12.0, true);
        require(
            !savedFall.snapshot().playerRespawning &&
                savedFall.snapshot().playerHealth == 1.0 &&
                savedFall.snapshot().lootStacks[
                    ian::lootUpgradeIndex(
                        ian::LootUpgradeEffect::Rope)] == 0,
            "Rope consumes one stack and leaves one health on fatal fall");
        require(
            std::ranges::any_of(
                savedFall.takeEvents(),
                [](const ian::GameEvent& event) {
                    return event.type ==
                        ian::GameEventType::RopeFallSaved;
                }),
            "Rope rescue emits presentation feedback");
    }
    {
        ian::MapDefinition storageMap =
            ian::MapDefinition::defaults();
        storageMap.obstacles.clear();
        ian::WorldConfig storageWorld =
            ian::WorldConfig::defaults();
        storageWorld.terrainAmplitude = 0.0;
        ian::GameBalance storageBalance =
            ian::GameBalance::defaults();
        storageBalance.economy.sellRefundFraction = 1.0;
        ian::Simulation storageSimulation{
            storageBalance, storageMap,
            storageWorld};
        storageSimulation.startRun();
        require(
            storageSimulation.snapshot().woodCapacity == 60 &&
                storageSimulation.snapshot().stoneCapacity == 30 &&
                storageSimulation.snapshot().crystalCapacity == 10,
            "pre-core inventory has deliberately small resource limits");

        ian::PlayerCommand godMode;
        godMode.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        storageSimulation.tick(1.0 / 60.0, godMode);
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        storageSimulation.tick(1.0 / 60.0, placeCore);
        require(
            storageSimulation.snapshot().woodCapacity == 100 &&
                storageSimulation.snapshot().stoneCapacity == 60 &&
                storageSimulation.snapshot().crystalCapacity == 60,
            "level-one core expands the initial inventory capacity");

        ian::PlayerCommand placeWoodStorage;
        placeWoodStorage.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::WoodStorage, {4, 0}, 0};
        storageSimulation.tick(1.0 / 60.0, placeWoodStorage);
        require(
            std::ranges::none_of(
                storageSimulation.snapshot().buildings,
                [](const ian::BuildingInstance& building) {
                    return building.type ==
                        ian::BuildingType::WoodStorage;
                }),
            "specialized storage buildings can no longer be placed");

        const auto coreBuilding = std::ranges::find(
            storageSimulation.snapshot().buildings,
            ian::BuildingType::Core,
            &ian::BuildingInstance::type);
        require(coreBuilding != storageSimulation.snapshot().buildings.end(),
                "storage progression fixture finds core");
        const ian::EntityId coreId = coreBuilding->id;
        ian::PlayerCommand upgradeCore;
        upgradeCore.upgradeBuilding =
            ian::UpgradeBuildingCommand{coreId};
        storageSimulation.tick(1.0 / 60.0, upgradeCore);
        storageSimulation.tick(1.0 / 60.0, godMode);
        require(
            storageSimulation.snapshot().woodCapacity == 180 &&
                storageSimulation.snapshot().stoneCapacity == 110 &&
                storageSimulation.snapshot().crystalCapacity == 120,
            "upgrading the core expands all resource capacities");
    }
    {
        ian::MapDefinition movementMap =
            ian::MapDefinition::defaults();
        movementMap.resources.clear();
        movementMap.obstacles.clear();
        ian::WorldConfig movementWorld =
            ian::WorldConfig::defaults();
        movementWorld.terrainAmplitude = 0.0;

        ian::Simulation baseline{
            ian::GameBalance::defaults(), movementMap,
            movementWorld};
        ian::Simulation movementSkills{
            ian::GameBalance::defaults(), movementMap,
            movementWorld};
        baseline.startRun();
        movementSkills.startRun();
        movementSkills.grantSkillPoints(
            4, ian::SkillPointSource::Event);
        const auto light =
            movementSkills.skillTree().indexOf(
                "light_footwork");
        const auto sprinter =
            movementSkills.skillTree().indexOf(
                "sprinter");
        const auto dash =
            movementSkills.skillTree().indexOf("dash");
        require(
            light && sprinter && dash &&
                movementSkills.purchaseSkill(*light) ==
                    ian::SkillPurchaseError::None &&
                movementSkills.purchaseSkill(*sprinter) ==
                    ian::SkillPurchaseError::None &&
                movementSkills.purchaseSkill(*dash) ==
                    ian::SkillPurchaseError::None,
            "movement fixture unlocks Light Footwork and Dash");

        ian::PlayerCommand move;
        move.moveForward = 1.0;
        baseline.tick(1.0 / 60.0, move);
        movementSkills.tick(1.0 / 60.0, move);
        require(
            std::hypot(
                movementSkills.snapshot().playerHorizontalVelocity.x,
                movementSkills.snapshot().playerHorizontalVelocity.z) >
                std::hypot(
                    baseline.snapshot().playerHorizontalVelocity.x,
                    baseline.snapshot().playerHorizontalVelocity.z),
            "Light Footwork improves response without changing speed definition");

        static_cast<void>(movementSkills.takeEvents());
        const auto beforeDash =
            movementSkills.snapshot().playerPosition;
        ian::PlayerCommand dashForward;
        dashForward.moveForward = 1.0;
        dashForward.dash = true;
        movementSkills.tick(1.0 / 60.0, dashForward);
        const auto dashSnapshot = movementSkills.snapshot();
        const auto dashEvents = movementSkills.takeEvents();
        require(
            dashSnapshot.dashing &&
                dashSnapshot.dashUnlocked &&
                dashSnapshot.playerPosition.z <
                    beforeDash.z - 0.2 &&
                dashSnapshot.dashCooldownRemaining > 0.0 &&
                std::ranges::any_of(
                    dashEvents,
                    [](const ian::GameEvent& event) {
                        return event.type ==
                            ian::GameEventType::PlayerDashed;
                    }),
            "Dash starts a fast forward burst and exposes cooldown feedback");

        movementSkills.tick(1.0 / 60.0, dashForward);
        require(
            std::ranges::none_of(
                movementSkills.takeEvents(),
                [](const ian::GameEvent& event) {
                    return event.type ==
                        ian::GameEventType::PlayerDashed;
                }),
            "Dash cannot be retriggered before its charge recovers");
    }
    {
        ian::Simulation lootEffects;
        lootEffects.startRun();
        const double baseMaximum =
            lootEffects.snapshot().playerMaxHealth;
        lootEffects.grantLootUpgrade(ian::LootUpgradeEffect::Apple);
        requireNear(
            lootEffects.snapshot().playerMaxHealth,
            baseMaximum, 1e-9,
            "apple no longer acts as a passive maximum-health stat item");
        requireNear(
            lootEffects.snapshot().playerHealth,
            baseMaximum, 1e-9,
            "apple waits for its low-health trigger");
        require(
            lootEffects.snapshot().lootStacks[
                ian::lootUpgradeIndex(ian::LootUpgradeEffect::Apple)] == 1,
            "snapshot exposes collected apple stack");

        ian::PlayerCommand damage;
        damage.damagePlayer = ian::DamagePlayerCommand{70.0};
        lootEffects.tick(0.0, damage);
        const double damagedHealth = lootEffects.snapshot().playerHealth;
        requireNear(
            damagedHealth, 50.0, 1e-9,
            "apple automatically heals once after crossing its health threshold");
        require(
            !lootEffects.snapshot().appleAvailable,
            "apple becomes unavailable until the next night");
        lootEffects.grantLootUpgrade(ian::LootUpgradeEffect::Bread);
        lootEffects.grantLootUpgrade(ian::LootUpgradeEffect::Bread);
        lootEffects.tick(5.9);
        requireNear(
            lootEffects.snapshot().playerHealth,
            damagedHealth, 1e-9,
            "bread does not regenerate before six damage-free seconds");
        lootEffects.tick(0.2);
        requireNear(
            lootEffects.snapshot().playerHealth,
            damagedHealth + 0.13, 1e-9,
            "bread regeneration starts after delay and stacks linearly");
        require(
            lootEffects.snapshot().lootStacks[
                ian::lootUpgradeIndex(ian::LootUpgradeEffect::Bread)] == 2,
            "snapshot exposes stacked bread count");

        lootEffects.grantLootUpgrade(
            ian::LootUpgradeEffect::IronBar);
        requireNear(
            lootEffects.snapshot().playerMaxRecoverableArmor,
            12.0, 1e-9,
            "iron bar grants a separate recoverable armor pool");
        const double healthBeforeArmorHits =
            lootEffects.snapshot().playerHealth;
        ian::PlayerCommand armorHit;
        armorHit.damagePlayer = ian::DamagePlayerCommand{7.0};
        lootEffects.tick(0.0, armorHit);
        requireNear(
            lootEffects.snapshot().playerRecoverableArmor,
            5.0, 1e-9,
            "recoverable armor absorbs incoming damage first");
        requireNear(
            lootEffects.snapshot().playerHealth,
            healthBeforeArmorHits, 1e-9,
            "fully absorbed armor damage does not reduce health");
        armorHit.damagePlayer = ian::DamagePlayerCommand{10.0};
        lootEffects.tick(0.0, armorHit);
        requireNear(
            lootEffects.snapshot().playerHealth,
            healthBeforeArmorHits - 5.0, 1e-9,
            "damage beyond remaining armor reaches health");
        require(
            std::ranges::any_of(
                lootEffects.takeEvents(),
                [](const ian::GameEvent& event) {
                    return event.type ==
                        ian::GameEventType::IronArmorBroken;
                }),
            "breaking Iron Bar armor emits its defensive shockwave");
        lootEffects.tick(4.9);
        requireNear(
            lootEffects.snapshot().playerRecoverableArmor,
            0.0, 1e-9,
            "armor waits five damage-free seconds before recharging");
        lootEffects.tick(0.2);
        requireNear(
            lootEffects.snapshot().playerRecoverableArmor,
            0.5, 1e-9,
            "armor begins recharging smoothly after its delay");

        lootEffects.grantLootUpgrade(
            ian::LootUpgradeEffect::Key);
        require(
            lootEffects.snapshot().freeChestOpeningAvailable &&
                std::abs(
                    lootEffects.snapshot().chestOpeningCostMultiplier -
                    1.0) < 1e-9,
            "first Chest Key grants a free daily opening without a passive discount");
        lootEffects.grantLootUpgrade(
            ian::LootUpgradeEffect::Key);
        require(
            lootEffects.snapshot().freeChestRerollsRemaining == 1 &&
                std::abs(
                    lootEffects.snapshot().chestOpeningCostMultiplier -
                    1.0) < 1e-9,
            "additional Chest Key stacks grant free rerolls without changing chest prices");

        lootEffects.restartRun();
        require(
            lootEffects.snapshot().lootStacks[
                ian::lootUpgradeIndex(ian::LootUpgradeEffect::Apple)] == 0 &&
            lootEffects.snapshot().lootStacks[
                ian::lootUpgradeIndex(ian::LootUpgradeEffect::Bread)] == 0 &&
            lootEffects.snapshot().playerMaxRecoverableArmor == 0.0,
            "run restart clears collected item stacks");
    }
    {
        ian::Simulation breadSimulation;
        breadSimulation.startRun();
        breadSimulation.grantLootUpgrade(
            ian::LootUpgradeEffect::Bread);
        breadSimulation.tick(6.1);
        require(
            breadSimulation.snapshot().breadWellFed,
            "Bread grants Well Fed attack speed at full health");
        ian::PlayerCommand damage;
        damage.damagePlayer = ian::DamagePlayerCommand{1.0};
        breadSimulation.tick(0.0, damage);
        require(
            !breadSimulation.snapshot().breadWellFed,
            "taking a hit removes the Well Fed attack-speed state");
    }
    {
        auto healthAidBalance = ian::GameBalance::defaults();
        healthAidBalance.gameplay.pickaxeDamage = 10000.0;
        healthAidBalance.gameplay.pickaxeDamageVariation = 0.0;
        healthAidBalance.gameplay.pickaxeCriticalChance = 0.0;
        ian::Simulation healthAidSimulation{healthAidBalance};
        healthAidSimulation.startRun();
        healthAidSimulation.grantLootUpgrade(
            ian::LootUpgradeEffect::HealthAid);
        healthAidSimulation.grantLootUpgrade(
            ian::LootUpgradeEffect::HealthAid);
        ian::PlayerCommand damage;
        damage.damagePlayer = ian::DamagePlayerCommand{30.0};
        healthAidSimulation.tick(0.0, damage);
        const double damagedHealth =
            healthAidSimulation.snapshot().playerHealth;
        const auto tree = std::ranges::find_if(
            healthAidSimulation.snapshot().resourceNodes,
            [](const ian::ResourceNode& node) {
                return node.active &&
                    node.type == ian::ResourceType::Wood;
            });
        require(tree != healthAidSimulation.snapshot().resourceNodes.end(),
                "Field Medkit fixture has a tree to destroy");
        ian::PlayerCommand destroyTree;
        destroyTree.overrideAimedResource = true;
        destroyTree.aimedResourceOverride = tree->id;
        destroyTree.usePickaxe = true;
        healthAidSimulation.tick(1.0 / 60.0, destroyTree);
        requireNear(
            healthAidSimulation.snapshot().playerHealth,
            damagedHealth + 8.0, 1e-9,
            "Field Medkit heals once per destroyed tree and stacks linearly");
        healthAidSimulation.tick(1.0);
        const auto stone = std::ranges::find_if(
            healthAidSimulation.snapshot().resourceNodes,
            [](const ian::ResourceNode& node) {
                return node.active &&
                    node.type == ian::ResourceType::Stone;
            });
        require(stone != healthAidSimulation.snapshot().resourceNodes.end(),
                "Field Medkit fixture has a stone to destroy");
        ian::PlayerCommand destroyStone;
        destroyStone.overrideAimedResource = true;
        destroyStone.aimedResourceOverride = stone->id;
        destroyStone.usePickaxe = true;
        healthAidSimulation.tick(1.0 / 60.0, destroyStone);
        requireNear(
            healthAidSimulation.snapshot().playerHealth,
            damagedHealth + 16.0, 1e-9,
            "Field Medkit also heals after destroying stone");
    }
    {
        ian::GameBalance potionBalance =
            ian::GameBalance::defaults();
        potionBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)].wood = 0;
        ian::MapDefinition potionMap =
            ian::MapDefinition::defaults();
        potionMap.obstacles.clear();
        ian::WorldConfig potionWorld =
            ian::WorldConfig::defaults();
        potionWorld.terrainAmplitude = 0.0;
        ian::Simulation potionSimulation{
            potionBalance, potionMap, potionWorld};
        potionSimulation.startRun();
        potionSimulation.grantLootUpgrade(
            ian::LootUpgradeEffect::Potion,
            ian::LootRarity::Rare);
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        potionSimulation.tick(0.0, placeCore);
        ian::PlayerCommand startWave;
        startWave.startWaveEarly =
            ian::StartWaveEarlyCommand{};
        potionSimulation.tick(0.0, startWave);
        require(
            potionSimulation.snapshot().state == ian::RunState::Wave &&
                potionSimulation.snapshot().battlePotionAvailable,
            "Battle Potion arms once at the beginning of a night");
        ian::PlayerCommand criticalDamage;
        criticalDamage.damagePlayer =
            ian::DamagePlayerCommand{66.0};
        potionSimulation.tick(0.0, criticalDamage);
        require(
            !potionSimulation.snapshot().battlePotionAvailable &&
                potionSimulation.snapshot().battlePotionBerserkRemaining >
                    5.9,
            "Battle Potion automatically activates below 35 percent health");
        potionSimulation.tick(6.1);
        require(
            potionSimulation.snapshot().battlePotionBerserkRemaining == 0.0 &&
                potionSimulation.snapshot().playerTemporaryHealth >= 10.0,
            "Berserk ends with temporary health instead of its old wave-start heal");
    }
    {
        ian::Simulation weaponProgression;
        weaponProgression.startRun();
        ian::PlayerCommand cycle;
        cycle.toggleWeapon = ian::ToggleWeaponCommand{};
        weaponProgression.tick(1.0 / 60.0, cycle);
        require(weaponProgression.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::BareHands,
                "weapon cycle skips locked weapons and keeps bare hands selectable");
        const auto initialWeapons =
            weaponProgression.snapshot().unlockedWeapons;
        require(initialWeapons[static_cast<std::size_t>(
                    ian::PlayerWeapon::BareHands)] &&
                    !initialWeapons[static_cast<std::size_t>(
                        ian::PlayerWeapon::Bomb)] &&
                    std::count(initialWeapons.begin(), initialWeapons.end(), true) == 1,
                "weapon hotbar initially exposes only bare hands");
        ian::PlayerCommand selectHandsInitially;
        selectHandsInitially.selectWeapon =
            ian::SelectWeaponCommand{ian::PlayerWeapon::BareHands};
        weaponProgression.tick(1.0 / 60.0, selectHandsInitially);
        ian::PlayerCommand lockedSelection;
        lockedSelection.selectWeapon =
            ian::SelectWeaponCommand{ian::PlayerWeapon::Club};
        weaponProgression.tick(1.0 / 60.0, lockedSelection);
        require(weaponProgression.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::BareHands,
                "direct hotbar selection cannot equip a locked weapon");

        weaponProgression.grantSkillPoints(2, ian::SkillPointSource::Event);
        const auto combatTraining =
            weaponProgression.skillTree().indexOf(
                "combat_training");
        const auto club = weaponProgression.skillTree().indexOf("club");
        require(combatTraining && club &&
                    weaponProgression.purchaseSkill(
                        *combatTraining) ==
                        ian::SkillPurchaseError::None &&
                    weaponProgression.purchaseSkill(*club) ==
                            ian::SkillPurchaseError::None,
                "club can be unlocked before rifle");
        require(weaponProgression.snapshot().unlockedWeapons[
                    static_cast<std::size_t>(ian::PlayerWeapon::Club)],
                "weapon hotbar exposes club immediately after unlock");
        ian::PlayerCommand selectHands;
        selectHands.selectWeapon =
            ian::SelectWeaponCommand{ian::PlayerWeapon::BareHands};
        weaponProgression.tick(1.0 / 60.0, selectHands);
        weaponProgression.tick(1.0 / 60.0, lockedSelection);
        require(weaponProgression.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::Club,
                "direct hotbar selection equips an unlocked weapon");
        weaponProgression.tick(1.0 / 60.0, cycle);
        require(weaponProgression.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::BareHands,
                "weapon cycle includes fists after unlocked club");

        weaponProgression.grantSkillPoints(1, ian::SkillPointSource::Event);
        const auto rifle = weaponProgression.skillTree().indexOf("rifle");
        require(rifle && weaponProgression.purchaseSkill(*rifle) ==
                             ian::SkillPurchaseError::None &&
                    weaponProgression.snapshot().selectedWeapon ==
                        ian::PlayerWeapon::Rifle,
                "rifle unlock applies only after club dependency");
        weaponProgression.tick(1.0 / 60.0, cycle);
        require(weaponProgression.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::BareHands,
                "weapon cycle wraps from rifle to fists while bombs are locked");
    }
    {
        ian::Simulation iceProgression;
        iceProgression.startRun();
        iceProgression.grantSkillPoints(3, ian::SkillPointSource::Event);
        const auto combatTraining =
            iceProgression.skillTree().indexOf(
                "combat_training");
        const auto iceWand = iceProgression.skillTree().indexOf("ice_wand");
        require(combatTraining && iceWand &&
                    iceProgression.purchaseSkill(
                        *combatTraining) ==
                        ian::SkillPurchaseError::None &&
                    iceProgression.purchaseSkill(*iceWand) ==
                    ian::SkillPurchaseError::None &&
                    iceProgression.snapshot().selectedWeapon ==
                        ian::PlayerWeapon::IceWand,
                "ice wand purchase spends two points and equips immediately");
        iceProgression.grantSkillPoints(
            2, ian::SkillPointSource::Event);
        const auto fireWand =
            iceProgression.skillTree().indexOf("fire_wand");
        require(
            fireWand && iceProgression.purchaseSkill(*fireWand) ==
                    ian::SkillPurchaseError::None &&
                iceProgression.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::FireWand,
            "fire wand unlock follows ice wand and equips immediately");
        iceProgression.restartRun();
        iceProgression.grantSkillPoints(1, ian::SkillPointSource::Event);
        ian::PlayerCommand cycle;
        cycle.toggleWeapon = ian::ToggleWeaponCommand{};
        iceProgression.tick(1.0 / 60.0, cycle);
        require(iceProgression.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::BareHands,
                "locked wands and bombs are absent from the weapon cycle");
    }
    {
        auto clubBalance = ian::GameBalance::defaults();
        clubBalance.gameplay.pickaxeDamageVariation = 0.0;
        clubBalance.gameplay.pickaxeCriticalChance = 0.0;
        clubBalance.enemies[0].speed = 10.0;
        clubBalance.weapons.club.maxDamagePerAttack = 2.0;
        clubBalance.waves[0] = {
            2, 2, 0, 0, 0, 0, 0, 0, false, 2, 10.0};

        ian::MapDefinition clubMap =
            ian::MapDefinition::defaults();
        clubMap.playerSpawn = {0.0, 0.0, -5.0};
        clubMap.enemySpawnAnchors = {{0.0, 0.0, -20.0}};
        clubMap.resources.clear();
        clubMap.obstacles.clear();
        ian::WorldConfig clubWorld =
            ian::WorldConfig::defaults();
        clubWorld.terrainAmplitude = 0.0;

        ian::Simulation clubCombat{
            clubBalance, clubMap, clubWorld};
        clubCombat.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        clubCombat.tick(1.0 / 60.0, unlimited);
        const auto clubSkill =
            clubCombat.skillTree().indexOf("club");
        const auto concussiveSkill =
            clubCombat.skillTree().indexOf("concussive_swings");
        const auto combatTraining =
            clubCombat.skillTree().indexOf(
                "combat_training");
        require(combatTraining && clubSkill && concussiveSkill,
                "club combat skills exist");
        require(clubCombat.purchaseSkill(*combatTraining) ==
                    ian::SkillPurchaseError::None,
                "club combat fixture buys combat training");
        require(clubCombat.purchaseSkill(*clubSkill) ==
                    ian::SkillPurchaseError::None,
                "club combat fixture buys club");
        require(clubCombat.purchaseSkill(*concussiveSkill) ==
                    ian::SkillPurchaseError::None &&
                    clubCombat.snapshot().selectedWeapon ==
                        ian::PlayerWeapon::Club,
                "club combat fixture selects club");

        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        clubCombat.tick(1.0 / 60.0, placeCore);
        require(
            clubCombat.snapshot().coreId.has_value(),
            "club combat fixture places core");
        ian::PlayerCommand startWave;
        startWave.startWaveEarly =
            ian::StartWaveEarlyCommand{};
        clubCombat.tick(1.0 / 60.0, startWave);
        std::optional<ian::EnemyInstance> clubAimTarget;
        for (int tick = 0; tick < 180 && !clubAimTarget;
             ++tick) {
            const auto approachSnapshot = clubCombat.snapshot();
            for (const ian::EnemyInstance& enemy :
                 approachSnapshot.enemies) {
                if (enemy.active &&
                    std::hypot(
                        enemy.position.x -
                            approachSnapshot.playerPosition.x,
                        enemy.position.z -
                            approachSnapshot.playerPosition.z) < 3.5) {
                    clubAimTarget = enemy;
                    break;
                }
            }
            if (clubAimTarget) {
                break;
            }
            clubCombat.tick(1.0 / 60.0);
        }
        require(
            clubCombat.snapshot().activeEnemyCount == 2U,
            "club combat fixture spawns two enemies");
        require(
            clubAimTarget.has_value(),
            "club combat fixture brings enemies into melee range");
        const auto clubAimSnapshot = clubCombat.snapshot();
        const double aimDeltaX =
            clubAimTarget->position.x -
            clubAimSnapshot.playerPosition.x;
        const double aimDeltaY =
            clubAimTarget->position.y -
            clubAimSnapshot.playerPosition.y;
        const double aimDeltaZ =
            clubAimTarget->position.z -
            clubAimSnapshot.playerPosition.z;
        const double aimHorizontal =
            std::hypot(aimDeltaX, aimDeltaZ);
        ian::PlayerCommand aimClub;
        aimClub.lookYaw =
            std::atan2(aimDeltaX, -aimDeltaZ) -
            clubAimSnapshot.playerYaw;
        aimClub.lookPitch =
            std::atan2(aimDeltaY, aimHorizontal) -
            clubAimSnapshot.playerPitch;
        clubCombat.tick(0.0, aimClub);
        require(
            clubCombat.snapshot().aimedEnemy.has_value(),
            "club combat fixture places enemies in melee range");

        const auto enemiesBeforeClubHit =
            clubCombat.snapshot().enemies;
        const double firstHealthBefore =
            enemiesBeforeClubHit[0].health;
        const double secondHealthBefore =
            enemiesBeforeClubHit[1].health;
        std::array<double, 2> distancesBeforeClubHit{};
        for (std::size_t index = 0;
             index < distancesBeforeClubHit.size(); ++index) {
            distancesBeforeClubHit[index] = std::hypot(
                enemiesBeforeClubHit[index].position.x -
                    clubAimSnapshot.playerPosition.x,
                enemiesBeforeClubHit[index].position.z -
                    clubAimSnapshot.playerPosition.z);
        }
        ian::PlayerCommand clubAttack;
        clubAttack.usePickaxe = true;
        clubCombat.tick(1.0 / 60.0, clubAttack);
        const auto enemiesAfterClubHit =
            clubCombat.snapshot().enemies;
        require(
            enemiesAfterClubHit[0].health < firstHealthBefore &&
                enemiesAfterClubHit[1].health < secondHealthBefore,
            "club damages aimed and adjacent enemies through simulation tick");
        require(
            firstHealthBefore - enemiesAfterClubHit[0].health +
                    secondHealthBefore - enemiesAfterClubHit[1].health <=
                clubBalance.weapons.club.maxDamagePerAttack + 1e-9,
            "club damage cap applies across all enemies in one swing");
        require(
            std::hypot(
                enemiesAfterClubHit[0].knockbackVelocity.x,
                enemiesAfterClubHit[0].knockbackVelocity.z) > 0.0 &&
                std::hypot(
                    enemiesAfterClubHit[1].knockbackVelocity.x,
                enemiesAfterClubHit[1].knockbackVelocity.z) > 0.0,
            "club gives aimed and adjacent enemies knockback through simulation tick");
        for (std::size_t index = 0;
             index < enemiesAfterClubHit.size(); ++index) {
            const ian::EnemyInstance& enemy =
                enemiesAfterClubHit[index];
            const double awayX =
                enemy.position.x - clubAimSnapshot.playerPosition.x;
            const double awayZ =
                enemy.position.z - clubAimSnapshot.playerPosition.z;
            const double impulseDot =
                enemy.knockbackVelocity.x * awayX +
                enemy.knockbackVelocity.z * awayZ;
            require(
                impulseDot > 0.0,
                "club knockback points away from player");
        }
        for (int tick = 0; tick < 12; ++tick) {
            clubCombat.tick(1.0 / 60.0);
        }
        const auto enemiesAfterKnockbackTravel =
            clubCombat.snapshot().enemies;
        bool allClubTargetsMovedAway = true;
        for (std::size_t index = 0;
             index < enemiesAfterKnockbackTravel.size(); ++index) {
            const double awayX =
                enemiesAfterKnockbackTravel[index].position.x -
                clubAimSnapshot.playerPosition.x;
            const double awayZ =
                enemiesAfterKnockbackTravel[index].position.z -
                clubAimSnapshot.playerPosition.z;
            allClubTargetsMovedAway =
                allClubTargetsMovedAway &&
                std::hypot(awayX, awayZ) >
                    distancesBeforeClubHit[index] + 0.05;
        }
        require(
            allClubTargetsMovedAway,
            "club knockback visibly moves enemies away from player");
    }
    {
        ian::Simulation automaticTools;
        automaticTools.startRun();
        automaticTools.grantSkillPoints(
            2, ian::SkillPointSource::Event);
        const auto axe =
            automaticTools.skillTree().indexOf("axe");
        const auto pickaxe =
            automaticTools.skillTree().indexOf("pickaxe");
        require(
            axe && pickaxe &&
                automaticTools.purchaseSkill(*axe) ==
                    ian::SkillPurchaseError::None &&
                automaticTools.purchaseSkill(*pickaxe) ==
                    ian::SkillPurchaseError::None,
            "automatic tool switch fixture unlocks both tools");
        const auto snapshot = automaticTools.snapshot();
        require(
            snapshot.automaticToolSwitch && snapshot.holdToGather &&
                !automaticTools.skillTree().indexOf("auto_switch_tools") &&
                !automaticTools.skillTree().indexOf("hold_to_gather"),
            "Smart Tools and Hold to Harvest are base mechanics");
        const auto wood = std::find_if(
            snapshot.resourceNodes.begin(),
            snapshot.resourceNodes.end(),
            [](const ian::ResourceNode& node) {
                return node.active &&
                    node.type == ian::ResourceType::Wood;
            });
        const auto stone = std::find_if(
            snapshot.resourceNodes.begin(),
            snapshot.resourceNodes.end(),
            [](const ian::ResourceNode& node) {
                return node.active &&
                    node.type == ian::ResourceType::Stone;
            });
        require(
            wood != snapshot.resourceNodes.end() &&
                stone != snapshot.resourceNodes.end(),
            "automatic tool switch fixture has both resource types");
        ian::PlayerCommand aimWood;
        aimWood.overrideAimedResource = true;
        aimWood.aimedResourceOverride = wood->id;
        automaticTools.tick(1.0 / 60.0, aimWood);
        require(
            automaticTools.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Axe,
            "automatic switch selects axe for wood");
        ian::PlayerCommand aimStone;
        aimStone.overrideAimedResource = true;
        aimStone.aimedResourceOverride = stone->id;
        automaticTools.tick(1.0 / 60.0, aimStone);
        require(
            automaticTools.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Pickaxe,
            "automatic switch selects pickaxe for stone");
        automaticTools.grantSkillPoints(
            1, ian::SkillPointSource::Event);
        const auto hammer =
            automaticTools.skillTree().indexOf("hammer");
        require(
            hammer && automaticTools.purchaseSkill(*hammer) ==
                           ian::SkillPurchaseError::None &&
                automaticTools.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::Hammer,
            "Smart Tools fixture equips the hammer");
        automaticTools.tick(1.0 / 60.0, aimWood);
        require(
            automaticTools.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Axe,
            "Smart Tools switches from hammer to axe for wood");
        ian::PlayerCommand selectHammer;
        selectHammer.selectWeapon = ian::SelectWeaponCommand{
            ian::PlayerWeapon::Hammer};
        selectHammer.overrideAimedResource = true;
        selectHammer.aimedResourceOverride.reset();
        automaticTools.tick(1.0 / 60.0, selectHammer);
        automaticTools.tick(1.0 / 60.0, aimStone);
        require(
            automaticTools.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Pickaxe,
            "Smart Tools switches from hammer to pickaxe for stone");
        ian::PlayerCommand buildWhileAimingWood;
        buildWhileAimingWood.selectBuilding =
            ian::BuildingType::Core;
        buildWhileAimingWood.overrideAimedResource = true;
        buildWhileAimingWood.aimedResourceOverride = wood->id;
        automaticTools.tick(
            1.0 / 60.0, buildWhileAimingWood);
        require(
            automaticTools.snapshot().selectedBuilding ==
                ian::BuildingType::Core &&
            automaticTools.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Pickaxe,
            "automatic tools cannot cancel building mode");
        require(automaticTools.snapshot().holdToGather,
                "building mode does not disable base hold gathering");
    }
    {
        auto powerBalance = ian::GameBalance::defaults();
        powerBalance.gameplay.pickaxeDamageVariation = 0.0;
        powerBalance.gameplay.pickaxeCriticalChance = 0.0;
        ian::MapDefinition powerMap =
            ian::MapDefinition::defaults();
        powerMap.resources = {
            {ian::ResourceType::Wood, {0.0, 0.0, -2.0},
             1.0, 10.0, 10, 20.0},
            {ian::ResourceType::Wood, {1.5, 0.0, -2.0},
             1.0, 10.0, 10, 20.0},
        };
        ian::Simulation powerSwing{
            powerBalance, powerMap};
        powerSwing.startRun();
        powerSwing.grantSkillPoints(
            5, ian::SkillPointSource::Event);
        constexpr std::array<const char*, 4> PowerSkills{{
            "axe", "pickaxe", "efficient_strikes", "power_swing",
        }};
        bool unlockedPowerPath = true;
        for (const char* id : PowerSkills) {
            const auto skill =
                powerSwing.skillTree().indexOf(id);
            unlockedPowerPath = unlockedPowerPath && skill &&
                powerSwing.purchaseSkill(*skill) ==
                    ian::SkillPurchaseError::None;
        }
        require(unlockedPowerPath,
                "Power Swing fixture unlocks gathering path");
        const auto initialResources =
            powerSwing.snapshot().resourceNodes;
        require(initialResources.size() == 2U,
                "Power Swing fixture has adjacent resources");
        const ian::EntityId primary =
            initialResources[0].id;
        const double primaryHealth =
            initialResources[0].health;
        const double nearbyHealth =
            initialResources[1].health;
        for (int hit = 0; hit < 3; ++hit) {
            ian::PlayerCommand gather;
            gather.overrideAimedResource = true;
            gather.aimedResourceOverride = primary;
            gather.usePickaxe = true;
            powerSwing.tick(1.0 / 60.0, gather);
            powerSwing.tick(
                powerBalance.gameplay.pickaxeCooldown);
        }
        const auto afterPowerSwing =
            powerSwing.snapshot().resourceNodes;
        requireNear(
            afterPowerSwing[0].health,
            primaryHealth - 3.75, 1e-12,
            "three gathering hits damage primary resource three times");
        requireNear(
            afterPowerSwing[1].health,
            nearbyHealth - 1.25, 1e-12,
            "third Power Swing damages nearby resource once");
    }
    {
        auto inefficientBalance = ian::GameBalance::defaults();
        inefficientBalance.gameplay.pickaxeDamageVariation = 0.0;
        inefficientBalance.gameplay.pickaxeCriticalChance = 0.0;
        ian::Simulation inefficientTools{inefficientBalance};
        inefficientTools.startRun();
        inefficientTools.grantSkillPoints(
            2, ian::SkillPointSource::Event);
        const auto axe =
            inefficientTools.skillTree().indexOf("axe");
        const auto pickaxe =
            inefficientTools.skillTree().indexOf("pickaxe");
        require(
            axe && pickaxe &&
                inefficientTools.purchaseSkill(*axe) ==
                    ian::SkillPurchaseError::None,
            "inefficient gathering fixture unlocks the axe");
        const auto stone = std::ranges::find_if(
            inefficientTools.snapshot().resourceNodes,
            [](const ian::ResourceNode& node) {
                return node.active &&
                    node.type == ian::ResourceType::Stone;
            });
        require(
            stone != inefficientTools.snapshot().resourceNodes.end(),
            "inefficient gathering fixture has stone");
        ian::PlayerCommand axeStoneHit;
        axeStoneHit.overrideAimedResource = true;
        axeStoneHit.aimedResourceOverride = stone->id;
        axeStoneHit.usePickaxe = true;
        inefficientTools.tick(1.0 / 60.0, axeStoneHit);
        auto hitEvents = inefficientTools.takeEvents();
        const auto axeHit = std::ranges::find_if(
            hitEvents, [](const ian::GameEvent& event) {
                return event.type == ian::GameEventType::ResourceHit &&
                    event.resourceType == ian::ResourceType::Stone;
            });
        require(
            axeHit != hitEvents.end(),
            "axe can damage stone instead of rejecting the target");
        requireNear(
            axeHit->damage,
            inefficientBalance.gameplay.pickaxeDamage * 0.25,
            1e-12,
            "axe mines stone at twenty-five percent efficiency");

        inefficientTools.tick(
            inefficientBalance.gameplay.pickaxeCooldown);
        require(
            inefficientTools.purchaseSkill(*pickaxe) ==
                ian::SkillPurchaseError::None,
            "inefficient gathering fixture unlocks the pickaxe");
        const auto wood = std::ranges::find_if(
            inefficientTools.snapshot().resourceNodes,
            [](const ian::ResourceNode& node) {
                return node.active &&
                    node.type == ian::ResourceType::Wood;
            });
        require(
            wood != inefficientTools.snapshot().resourceNodes.end(),
            "inefficient gathering fixture has wood");
        ian::PlayerCommand pickaxeWoodHit;
        pickaxeWoodHit.overrideAimedResource = true;
        pickaxeWoodHit.aimedResourceOverride = wood->id;
        pickaxeWoodHit.usePickaxe = true;
        inefficientTools.tick(1.0 / 60.0, pickaxeWoodHit);
        hitEvents = inefficientTools.takeEvents();
        const auto pickaxeHit = std::ranges::find_if(
            hitEvents, [](const ian::GameEvent& event) {
                return event.type == ian::GameEventType::ResourceHit &&
                    event.resourceType == ian::ResourceType::Wood;
            });
        require(
            pickaxeHit != hitEvents.end(),
            "Smart Tools routes a wood hit through the unlocked axe");
        requireNear(
            pickaxeHit->damage,
            inefficientBalance.gameplay.pickaxeDamage,
            1e-12,
            "Smart Tools avoids wrong-tool efficiency loss");
        require(
            inefficientTools.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Axe,
            "Smart Tools visibly selects the correct unlocked tool");
    }
    {
        auto bareHandsBalance = ian::GameBalance::defaults();
        bareHandsBalance.gameplay.pickaxeDamageVariation = 0.0;
        bareHandsBalance.gameplay.pickaxeCriticalChance = 0.0;
        ian::Simulation bareHandsTools{bareHandsBalance};
        bareHandsTools.startRun();
        bareHandsTools.grantSkillPoints(
            2, ian::SkillPointSource::Event);
        const auto axe = bareHandsTools.skillTree().indexOf("axe");
        const auto pickaxe = bareHandsTools.skillTree().indexOf("pickaxe");
        require(axe && pickaxe &&
                    bareHandsTools.purchaseSkill(*axe) ==
                        ian::SkillPurchaseError::None &&
                    bareHandsTools.purchaseSkill(*pickaxe) ==
                        ian::SkillPurchaseError::None,
                "bare-hands Smart Tools fixture unlocks gathering tools");
        require(bareHandsTools.snapshot().automaticToolSwitch,
                "Smart Tools remains active in Bare Hands mode");
        ian::PlayerCommand selectBareHands;
        selectBareHands.selectWeapon =
            ian::SelectWeaponCommand{ian::PlayerWeapon::BareHands};
        bareHandsTools.tick(1.0 / 60.0, selectBareHands);
        require(bareHandsTools.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::BareHands,
                "Smart Tools test enters explicit Bare Hands mode");
        const auto bareSnapshot = bareHandsTools.snapshot();
        const auto wood = std::find_if(
            bareSnapshot.resourceNodes.begin(),
            bareSnapshot.resourceNodes.end(),
            [](const ian::ResourceNode& node) {
                return node.active && node.type == ian::ResourceType::Wood;
            });
        require(wood != bareSnapshot.resourceNodes.end(),
                "bare-hands Smart Tools fixture has a tree");
        ian::PlayerCommand gatherWithHands;
        gatherWithHands.overrideAimedResource = true;
        gatherWithHands.aimedResourceOverride = wood->id;
        gatherWithHands.usePickaxe = true;
        bareHandsTools.tick(1.0 / 60.0, gatherWithHands);
        const auto bareEvents = bareHandsTools.takeEvents();
        const auto bareHit = std::find_if(
            bareEvents.begin(), bareEvents.end(),
            [](const ian::GameEvent& event) {
                return (event.type == ian::GameEventType::ResourceHit ||
                        event.type == ian::GameEventType::ResourceCollected) &&
                       event.resourceType == ian::ResourceType::Wood;
            });
        require(bareHandsTools.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::BareHands &&
                    bareHit != bareEvents.end(),
                "Smart Tools keeps Bare Hands and gathers the aimed tree");
        requireNear(
            bareHit->damage,
            bareHandsBalance.gameplay.pickaxeDamage * 0.25,
            1e-12,
            "Bare Hands gathering keeps its reduced damage coefficient");
    }
    {
        auto crystalBalance = ian::GameBalance::defaults();
        crystalBalance.gameplay.pickaxeDamageVariation = 0.0;
        crystalBalance.gameplay.pickaxeCriticalChance = 0.0;
        ian::MapDefinition crystalMap = ian::MapDefinition::defaults();
        crystalMap.resources = {{
            ian::ResourceType::Crystal, {0.0, 0.0, -2.0},
            0.72, 12.0, 8, 28.0,
        }};
        ian::Simulation crystalMining{crystalBalance, crystalMap};
        crystalMining.startRun();
        const ian::EntityId crystal =
            crystalMining.snapshot().resourceNodes.front().id;
        const double initialHealth =
            crystalMining.snapshot().resourceNodes.front().health;
        ian::PlayerCommand handHit;
        handHit.overrideAimedResource = true;
        handHit.aimedResourceOverride = crystal;
        handHit.usePickaxe = true;
        crystalMining.tick(1.0 / 60.0, handHit);
        requireNear(
            crystalMining.snapshot().resourceNodes.front().health,
            initialHealth, 1e-12,
            "bare hands cannot damage a crystal deposit");

        crystalMining.tick(crystalBalance.gameplay.pickaxeCooldown);
        crystalMining.grantSkillPoints(
            2, ian::SkillPointSource::Event);
        const auto axe = crystalMining.skillTree().indexOf("axe");
        const auto pickaxe = crystalMining.skillTree().indexOf("pickaxe");
        require(axe && pickaxe &&
                    crystalMining.purchaseSkill(*axe) ==
                        ian::SkillPurchaseError::None &&
                    crystalMining.purchaseSkill(*pickaxe) ==
                        ian::SkillPurchaseError::None,
                "crystal mining fixture unlocks the pickaxe");
        ian::PlayerCommand selectPickaxe;
        selectPickaxe.selectWeapon =
            ian::SelectWeaponCommand{ian::PlayerWeapon::Pickaxe};
        crystalMining.tick(1.0 / 60.0, selectPickaxe);
        ian::PlayerCommand pickaxeHit;
        pickaxeHit.overrideAimedResource = true;
        pickaxeHit.aimedResourceOverride = crystal;
        pickaxeHit.usePickaxe = true;
        crystalMining.tick(1.0 / 60.0, pickaxeHit);
        require(
            crystalMining.snapshot().resourceNodes.front().health <
                initialHealth,
            "pickaxe damages a crystal deposit");
    }
    {
        ian::Simulation bombUnlock;
        bombUnlock.startRun();
        require(
            bombUnlock.snapshot().bombsRemaining == 0 &&
            !bombUnlock.snapshot().unlockedWeapons[
                static_cast<std::size_t>(ian::PlayerWeapon::Bomb)],
            "bomb weapon starts locked");
        ian::PlayerCommand lockedThrow;
        lockedThrow.useConsumable =
            ian::UseConsumableCommand{};
        bombUnlock.tick(1.0 / 60.0, lockedThrow);
        require(
            bombUnlock.snapshot().bombProjectiles.empty(),
            "locked bomb input cannot fire");
        bombUnlock.grantSkillPoints(
            2, ian::SkillPointSource::Event);
        const auto combatTraining =
            bombUnlock.skillTree().indexOf(
                "combat_training");
        const auto bombs =
            bombUnlock.skillTree().indexOf("bombs");
        require(
            combatTraining && bombs &&
                bombUnlock.purchaseSkill(
                    *combatTraining) ==
                    ian::SkillPurchaseError::None &&
                bombUnlock.purchaseSkill(*bombs) ==
                    ian::SkillPurchaseError::None &&
                bombUnlock.snapshot().unlockedWeapons[
                    static_cast<std::size_t>(ian::PlayerWeapon::Bomb)] &&
                bombUnlock.snapshot().selectedWeapon ==
                    ian::PlayerWeapon::Bomb &&
                bombUnlock.snapshot().bombsRemaining == 0,
            "Bombs node unlocks and equips the weapon; ammunition arrives at night");
    }
    {
        auto transactionBalance =
            ian::GameBalance::defaults();
        transactionBalance.gameplay.pickaxeDamageVariation = 0.0;
        transactionBalance.gameplay.pickaxeCriticalChance = 0.0;
        auto& coreDefinition =
            transactionBalance.buildings[
                static_cast<std::size_t>(
                    ian::BuildingType::Core)];
        coreDefinition.wood = 5;
        coreDefinition.stone = 0;
        coreDefinition.crystals = 0;
        auto& turretDefinition =
            transactionBalance.buildings[
                static_cast<std::size_t>(
                    ian::BuildingType::Wall)];
        turretDefinition.wood = 4;
        turretDefinition.stone = 0;
        turretDefinition.crystals = 0;
        transactionBalance.modularBuildings[0] = {
            .wood = 3,
            .stone = 0,
            .crystals = 0,
        };

        auto transactionMap = ian::MapDefinition::defaults();
        // Transaction semantics do not depend on procedural run layout.
        // Keep the authored tutorial tree in front of the player so this
        // fixture remains focused on atomic resource spending.
        transactionMap.resources.resize(1U);
        ian::Simulation transactions{
            transactionBalance, std::move(transactionMap)};
        transactions.startRun();
        static_cast<void>(transactions.takeEvents());
        unlockAxe(transactions);
        const auto coreSurface =
            transactions.previewPlacementSurface(
                ian::BuildingType::Core, {0, 0});
        const auto raisedCorePreview =
            transactions.previewPlacement(
                ian::BuildingType::Core, {0, 0},
                coreSurface.height + 1.0);
        require(
            raisedCorePreview.cost ==
                ian::ResourceCost{8, 0, 0},
            "low-inventory preview includes automatic foundation cost");
        ian::PlayerCommand rejectedRaisedCore;
        rejectedRaisedCore.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Core,
                .gridPosition = {0, 0},
                .rotation = 0,
                .baseHeight = coreSurface.height + 1.0,
                .platformStorey = -1,
                .lockHeight = true,
            };
        transactions.tick(
            1.0 / 60.0, rejectedRaisedCore);
        require(
            !transactions.snapshot().coreId &&
                transactions.snapshot().wood == 0 &&
                transactions.snapshot().platformFrames.empty(),
            "rejected placement changes neither inventory nor structures");

        ian::PlayerCommand gather;
        gather.usePickaxe = true;
        transactions.tick(1.0 / 60.0, gather);
        transactions.tick(0.5);
        transactions.tick(1.0 / 60.0, gather);
        transactions.tick(0.5);
        transactions.tick(1.0 / 60.0, gather);
        transactions.tick(0.8);
        require(transactions.snapshot().wood == 15,
                "transaction fixture gathers deterministic inventory");

        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        transactions.tick(1.0 / 60.0, placeCore);
        require(
            transactions.snapshot().coreId.has_value() &&
                transactions.snapshot().wood == 10,
            "successful placement spends configured cost exactly once");

        const ian::GridPosition turretPosition{1, 5};
        const auto turretSurface =
            transactions.previewPlacementSurface(
                ian::BuildingType::Wall,
                turretPosition);
        ian::PlayerCommand placeRaisedTurret;
        placeRaisedTurret.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Wall,
                .gridPosition = turretPosition,
                .rotation = 0,
                .baseHeight = turretSurface.height + 1.0,
                .platformStorey = -1,
                .lockHeight = true,
            };
        transactions.tick(
            1.0 / 60.0, placeRaisedTurret);
        const auto turret = std::find_if(
            transactions.snapshot().buildings.begin(),
            transactions.snapshot().buildings.end(),
            [](const ian::BuildingInstance& building) {
                return building.type == ian::BuildingType::Wall;
            });
        require(
            turret != transactions.snapshot().buildings.end() &&
                transactions.snapshot().platformFrames.size() == 2U &&
                transactions.snapshot().wood == 3,
            "raised placement atomically spends building and foundation costs");
        const ian::EntityId turretId = turret->id;
        ian::PlayerCommand sellTurret;
        sellTurret.sellBuilding =
            ian::SellBuildingCommand{turretId};
        transactions.tick(1.0 / 60.0, sellTurret);
        require(
            transactions.snapshot().buildings.size() == 1U &&
                transactions.snapshot().wood == 5,
            "sell removes building and credits configured refund once");
        transactions.tick(1.0 / 60.0, sellTurret);
        require(
            transactions.snapshot().wood == 5,
            "repeated stale sell cannot credit a second refund");
    }
    {
        ian::Simulation restartStress;
        std::uint32_t previousTerrainSeed =
            restartStress.snapshot().terrainSeed;
        restartStress.startRun();
        std::optional<ian::EntityId> previousCoreId;
        std::optional<ian::EntityId> staleCoreId;
        // A restart now regenerates the complete terrain. Sixteen generations
        // still exercise stale EntityIds without making this test spend most
        // of its time rebuilding heightfields.
        for (int restart = 0; restart < 16; ++restart) {
            if (restart > 0) {
                restartStress.restartRun();
            }
            require(
                restartStress.snapshot().terrainSeed !=
                    previousTerrainSeed,
                "every new run receives a different terrain seed");
            previousTerrainSeed =
                restartStress.snapshot().terrainSeed;
            const auto resetEvents = restartStress.takeEvents();
            require(
                resetEvents.size() == 1U &&
                    resetEvents.front().type ==
                        (restart == 0
                             ? ian::GameEventType::RunStarted
                             : ian::GameEventType::RunRestarted),
                "restart stress discards stale events");

            ian::PlayerCommand unlimited;
            unlimited.enableUnlimitedResources =
                ian::EnableUnlimitedResourcesCommand{};
            restartStress.tick(1.0 / 60.0, unlimited);
            ian::PlayerCommand placeCore;
            placeCore.placeBuilding = ian::PlaceBuildingCommand{
                ian::BuildingType::Core, {0, 0}, 0};
            restartStress.tick(1.0 / 60.0, placeCore);
            const auto coreId = restartStress.snapshot().coreId;
            require(
                coreId.has_value() &&
                    (!previousCoreId || *coreId != *previousCoreId),
                "restart never aliases a previous run building ID");

            if (staleCoreId) {
                const auto levelBefore =
                    restartStress.snapshot().buildings.front().level;
                ian::PlayerCommand staleUpgrade;
                staleUpgrade.upgradeBuilding =
                    ian::UpgradeBuildingCommand{*staleCoreId};
                restartStress.tick(
                    1.0 / 60.0, staleUpgrade);
                require(
                    restartStress.snapshot().buildings.front().level ==
                        levelBefore,
                    "stale building ID cannot mutate a restarted run");
            }
            staleCoreId = coreId;
            previousCoreId = coreId;
        }
    }
    {
        ian::Simulation bufferedAttackSimulation;
        bufferedAttackSimulation.startRun();
        ian::PlayerCommand attack;
        attack.usePickaxe = true;
        bufferedAttackSimulation.tick(1.0 / 60.0, attack);
        bufferedAttackSimulation.tick(0.32);
        bufferedAttackSimulation.tick(1.0 / 60.0, attack);
        bufferedAttackSimulation.tick(0.12);
        require(
            bufferedAttackSimulation.snapshot()
                    .pickaxeCooldownRemaining > 0.4,
            "pickaxe input shortly before cooldown end is buffered");
    }
    {
        ian::GameBalance producerUnlockBalance =
            ian::GameBalance::defaults();
        producerUnlockBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)].wood = 0;
        ian::WorldConfig producerUnlockWorld =
            ian::WorldConfig::defaults();
        producerUnlockWorld.terrainAmplitude = 0.0;
        ian::Simulation producerUnlocks{
            producerUnlockBalance,
            ian::MapDefinition::defaults(),
            producerUnlockWorld};
        producerUnlocks.startRun();
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        producerUnlocks.tick(1.0 / 60.0, placeCore);
        require(
            producerUnlocks.previewPlacement(
                ian::BuildingType::LumberMill, {3, 3}).error ==
                    ian::PlacementError::SkillRequired &&
            producerUnlocks.previewPlacement(
                ian::BuildingType::Quarry, {5, 3}).error ==
                    ian::PlacementError::SkillRequired &&
            producerUnlocks.previewPlacement(
                ian::BuildingType::CrystalMine, {7, 3}).error ==
                    ian::PlacementError::SkillRequired,
            "fresh runs lock all three production buildings behind skills");
        producerUnlocks.grantSkillPoints(
            2, ian::SkillPointSource::Event);
        const auto axeSkill =
            producerUnlocks.skillTree().indexOf("axe");
        const auto lumberSkill =
            producerUnlocks.skillTree().indexOf("lumber_mill");
        require(
            axeSkill && lumberSkill &&
                producerUnlocks.purchaseSkill(*axeSkill) ==
                    ian::SkillPurchaseError::None &&
                producerUnlocks.purchaseSkill(*lumberSkill) ==
                    ian::SkillPurchaseError::None &&
                producerUnlocks.previewPlacement(
                    ian::BuildingType::LumberMill, {3, 3}).error !=
                    ian::PlacementError::SkillRequired &&
                producerUnlocks.previewPlacement(
                    ian::BuildingType::Quarry, {5, 3}).error ==
                    ian::PlacementError::SkillRequired,
            "each producer unlock affects only its own building");
    }
    {
        ian::GameBalance productionBalance =
            ian::GameBalance::defaults();
        for (auto& enemy : productionBalance.enemies) {
            enemy.speed = 0.01;
            enemy.damage = 0.01;
        }
        ian::Simulation productionSimulation{
            productionBalance};
        productionSimulation.startRun();
        const auto terrainSurface =
            productionSimulation
                .previewPlacementSurface(
                    ian::BuildingType::Core,
                    {8, 8});
        require(
            std::abs(
                terrainSurface.height -
                std::round(
                    terrainSurface.height)) <
                    1e-9 &&
                std::abs(
                    terrainSurface
                            .foundationBottomHeight -
                    std::round(
                        terrainSurface
                            .foundationBottomHeight)) <
                    1e-9 &&
                terrainSurface
                        .foundationBottomHeight <=
                    terrainSurface.height,
            "terrain buildings use discrete one-cell levels with an automatic foundation");
        const auto preferredSurface =
            productionSimulation
                .previewPlacementSurface(
                    ian::BuildingType::Core,
                    {8, 8},
                    terrainSurface.height + 2.0);
        require(
            std::abs(
                preferredSurface.height -
                terrainSurface.height - 2.0) <
                    1e-9 &&
                preferredSurface.storey == -1 &&
                preferredSurface
                        .foundationBottomHeight ==
                    terrainSurface
                        .foundationBottomHeight,
            "preferred standing level raises terrain building and extends its automatic foundation");
        productionSimulation.grantSkillPoints(
            8, ian::SkillPointSource::Event);
        constexpr std::array<const char*, 5> ProducerSkills{{
            "axe", "pickaxe", "lumber_mill", "quarry", "crystal_mine",
        }};
        for (const char* id : ProducerSkills) {
            const auto skill =
                productionSimulation.skillTree().indexOf(id);
            require(
                skill &&
                    productionSimulation.purchaseSkill(*skill) ==
                        ian::SkillPurchaseError::None,
                "production fixture unlocks each producer separately");
        }
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        productionSimulation.tick(1.0 / 60.0, unlimited);

        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        productionSimulation.tick(1.0 / 60.0, placeCore);
        const auto coreId =
            productionSimulation.snapshot().coreId;
        require(coreId.has_value(),
                "production fixture creates core");

        ian::PlayerCommand upgradeCore;
        upgradeCore.upgradeBuilding =
            ian::UpgradeBuildingCommand{*coreId};
        productionSimulation.tick(
            1.0 / 60.0, upgradeCore);

        require(
            productionSimulation.previewPlacement(
                ian::BuildingType::Wall,
                {0, 2}).error ==
                ian::PlacementError::ResourceBlocked,
            "ordinary buildings cannot replace an active resource");
        const auto firstResource =
            std::find_if(
                productionSimulation.snapshot()
                    .resourceNodes.begin(),
                productionSimulation.snapshot()
                    .resourceNodes.end(),
                [](const ian::ResourceNode& node) {
                    return node.active;
                });
        require(
            firstResource !=
                    productionSimulation.snapshot()
                        .resourceNodes.end() &&
                productionSimulation
                        .previewFoundation(
                            firstResource->position)
                        .error ==
                    ian::ModularPlacementError::
                        ResourceBlocked,
            "platform frames cannot replace an active resource");

        const auto findPlacement =
            [&productionSimulation](
                ian::BuildingType type) {
                for (int z = -8; z <= 8; ++z) {
                    for (int x = -8; x <= 8; ++x) {
                        const ian::GridPosition position{x, z};
                        if (productionSimulation
                                .previewPlacement(
                                    type, position)
                                .valid()) {
                            return position;
                        }
                    }
                }
                return ian::GridPosition{1000, 1000};
            };
        const ian::GridPosition lumberPosition =
            findPlacement(
                ian::BuildingType::LumberMill);
        ian::PlayerCommand placeLumberMill;
        placeLumberMill.placeBuilding =
            ian::PlaceBuildingCommand{
                ian::BuildingType::LumberMill,
                lumberPosition, 0};
        productionSimulation.tick(
            1.0 / 60.0, placeLumberMill);
        const ian::GridPosition quarryPosition =
            findPlacement(
                ian::BuildingType::Quarry);
        ian::PlayerCommand placeQuarry;
        placeQuarry.placeBuilding =
            ian::PlaceBuildingCommand{
                ian::BuildingType::Quarry,
                quarryPosition, 0};
        productionSimulation.tick(
            1.0 / 60.0, placeQuarry);

        productionSimulation.tick(1.0 / 60.0, unlimited);
        require(
            !productionSimulation.snapshot().unlimitedResources,
            "production fixture leaves setup god mode before measuring output");

        const int woodBefore =
            productionSimulation.snapshot().wood;
        const int stoneBefore =
            productionSimulation.snapshot().stone;
        productionSimulation.tick(10.0);
        require(
            productionSimulation.snapshot().wood ==
                    woodBefore + 3 &&
                productionSimulation.snapshot().stone ==
                    stoneBefore + 2,
            "autonomous buildings grant wood and stone");

        ian::PlayerCommand startProductionWave;
        startProductionWave.startWaveEarly =
            ian::StartWaveEarlyCommand{};
        productionSimulation.tick(
            1.0 / 60.0, startProductionWave);
        require(
            productionSimulation.snapshot().state ==
                ian::RunState::Wave,
            "production fixture starts an active night wave");
        const int woodAtNight =
            productionSimulation.snapshot().wood;
        const int stoneAtNight =
            productionSimulation.snapshot().stone;
        productionSimulation.tick(20.0);
        require(
            productionSimulation.snapshot().wood == woodAtNight &&
                productionSimulation.snapshot().stone == stoneAtNight,
            "production buildings stop during waves by default");

        const auto nightShift =
            productionSimulation.skillTree().indexOf("night_shift");
        const bool nightShiftUnlocked = nightShift &&
            productionSimulation.purchaseSkill(*nightShift) ==
                ian::SkillPurchaseError::None;
        productionSimulation.tick(20.0);
        require(
            nightShiftUnlocked &&
                productionSimulation.snapshot().wood ==
                    woodAtNight + 3 &&
                productionSimulation.snapshot().stone ==
                    stoneAtNight + 2,
            "Night Shift restores exactly half-speed wave production");
    }

    {
        ian::Simulation foundationLifecycle;
        foundationLifecycle.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        foundationLifecycle.tick(1.0 / 60.0, unlimited);

        ian::PlayerCommand placeCore;
        placeCore.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Core,
                .gridPosition = {0, 0},
                .rotation = 0,
            };
        foundationLifecycle.tick(
            1.0 / 60.0, placeCore);
        require(
            foundationLifecycle.snapshot()
                .coreId.has_value(),
            "foundation lifecycle fixture creates core");

        const ian::GridPosition turretPosition{1, 5};
        const auto terrainSurface =
            foundationLifecycle
                .previewPlacementSurface(
                    ian::BuildingType::Turret,
                    turretPosition);
        const double playerHeightBefore =
            foundationLifecycle.snapshot()
                .playerPosition.y;
        ian::PlayerCommand placeRaisedTurret;
        placeRaisedTurret.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Turret,
                .gridPosition = turretPosition,
                .rotation = 0,
                .baseHeight =
                    terrainSurface.height + 1.0,
                .platformStorey = -1,
                .lockHeight = true,
            };
        foundationLifecycle.tick(
            1.0 / 60.0, placeRaisedTurret);
        const auto raisedSnapshot =
            foundationLifecycle.snapshot();
        const auto raisedTurret = std::find_if(
            raisedSnapshot.buildings.begin(),
            raisedSnapshot.buildings.end(),
            [turretPosition](
                const ian::BuildingInstance& building) {
                return building.type ==
                           ian::BuildingType::Turret &&
                       building.gridPosition ==
                           turretPosition;
            });
        require(
            raisedTurret !=
                    raisedSnapshot.buildings.end() &&
                raisedSnapshot.platformFrames.size() ==
                    2 &&
                raisedTurret->platformStorey == 0,
            "raised building creates a real ground platform");
        require(
            raisedSnapshot.playerPosition.y >
                playerHeightBefore + 0.5,
            "ground platform placed under player lifts player onto floor");

        const ian::EntityId turretId =
            raisedTurret->id;
        const auto raisedFoundation = std::find_if(
            raisedSnapshot.platformFrames.begin(),
            raisedSnapshot.platformFrames.end(),
            [turretPosition](
                const ian::PlatformFrameInstance& frame) {
                return frame.anchor.x ==
                           turretPosition.x - 1 &&
                       frame.anchor.z ==
                           turretPosition.z - 1;
            });
        require(
            raisedFoundation !=
                raisedSnapshot.platformFrames.end(),
            "raised building foundation matches its footprint");
        const ian::EntityId foundationId =
            raisedFoundation->id;
        ian::PlayerCommand sellTurret;
        sellTurret.sellBuilding =
            ian::SellBuildingCommand{turretId};
        foundationLifecycle.tick(
            1.0 / 60.0, sellTurret);
        require(
            foundationLifecycle.snapshot()
                    .platformFrames.size() == 2 &&
                foundationLifecycle.snapshot()
                    .buildings.size() == 1,
            "selling building leaves its automatic platform");

        const auto retainedSnapshot =
            foundationLifecycle.snapshot();
        const auto retainedPlatform = std::find_if(
            retainedSnapshot.platformFrames.begin(),
            retainedSnapshot.platformFrames.end(),
            [foundationId](
                const ian::PlatformFrameInstance& frame) {
                return frame.id == foundationId;
            });
        require(
            retainedPlatform !=
                retainedSnapshot.platformFrames.end(),
            "selling building retains its matching foundation");
        ian::PlayerCommand replaceTurret;
        replaceTurret.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Turret,
                .gridPosition = turretPosition,
                .rotation = 0,
                .baseHeight = retainedPlatform->floorHeight,
                .platformStorey = retainedPlatform->storey,
                .lockHeight = true,
            };
        foundationLifecycle.tick(
            1.0 / 60.0, replaceTurret);
        require(
            foundationLifecycle.snapshot()
                    .buildings.size() == 2,
            "building can be placed again on retained platform");
        static_cast<void>(
            foundationLifecycle.takeEvents());

        ian::PlayerCommand removeFoundation;
        removeFoundation.removeModularBuilding =
            ian::RemoveModularBuildingCommand{
                foundationId};
        foundationLifecycle.tick(
            1.0 / 60.0, removeFoundation);
        require(
            foundationLifecycle.snapshot()
                    .platformFrames.size() == 1 &&
                foundationLifecycle.snapshot()
                    .buildings.size() == 1,
            "removing platform destroys building supported by it");
        const auto collapseEvents =
            foundationLifecycle.takeEvents();
        const auto destroyedBuilding =
            std::find_if(
                collapseEvents.begin(),
                collapseEvents.end(),
                [](const ian::GameEvent& event) {
                    return event.type ==
                               ian::GameEventType::
                                   BuildingDestroyed &&
                           event.building &&
                           event.building->type ==
                               ian::BuildingType::
                                   Turret;
                });
        require(
            destroyedBuilding !=
                collapseEvents.end(),
            "platform collapse event retains building visual snapshot");
    }

    {
        ian::Simulation protectedCore;
        protectedCore.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        protectedCore.tick(1.0 / 60.0, unlimited);

        const auto coreFoundation =
            protectedCore.placeFoundation(
                {0.2,
                 protectedCore.terrain().getHeight(
                     0.2, 4.2),
                 4.2});
        require(
            coreFoundation.has_value(),
            "protected core fixture creates platform");
        const ian::GridPosition corePosition{1, 5};
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Core,
                .gridPosition = corePosition,
                .rotation = 0,
                .baseHeight =
                    coreFoundation->floorHeight,
                .platformStorey =
                    coreFoundation->storey,
                .lockHeight = true,
            };
        protectedCore.tick(
            1.0 / 60.0, placeCore);
        const auto placed =
            protectedCore.snapshot();
        require(
            placed.coreId &&
                placed.platformFrames.size() == 1,
            "protected core fixture creates automatic platform");

        ian::PlayerCommand removeCoreFoundation;
        removeCoreFoundation.removeModularBuilding =
            ian::RemoveModularBuildingCommand{
                coreFoundation->id};
        protectedCore.tick(
            1.0 / 60.0,
            removeCoreFoundation);
        require(
            protectedCore.snapshot().coreId &&
                protectedCore.snapshot()
                    .platformFrames.size() == 1,
            "platform supporting core cannot be removed");
    }

    {
        ian::Simulation platformAim;
        platformAim.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        platformAim.tick(1.0 / 60.0, unlimited);
        const auto frame = platformAim.placeFoundation(
            {0.2,
             platformAim.terrain().getHeight(
                 0.2, 4.2),
             4.2});
        require(
            frame.has_value(),
            "platform aim fixture creates frame ahead of player");
        ian::PlayerCommand aimAtPlatform;
        aimAtPlatform.lookPitch = -1.0;
        aimAtPlatform.selectBuilding =
            ian::BuildingType::Turret;
        platformAim.tick(
            1.0 / 60.0, aimAtPlatform);
        require(
            platformAim.snapshot().buildingPreview &&
                platformAim.snapshot()
                        .buildingPreview
                        ->gridPosition ==
                    ian::GridPosition{1, 5} &&
                platformAim.snapshot()
                        .buildingPreview
                        ->platformStorey ==
                    frame->storey,
            "ordinary building preview follows aimed platform surface");
        ian::PlayerCommand preciseAimMiss;
        preciseAimMiss.overrideAimedModularBuilding = true;
        preciseAimMiss.aimedModularBuildingOverride =
            std::nullopt;
        platformAim.tick(
            1.0 / 60.0, preciseAimMiss);
        require(
            !platformAim.snapshot().aimedModularBuilding &&
                platformAim.snapshot()
                        .aimedModularBuildingCandidate ==
                    frame->id,
            "render-side precise platform picking can reject a"
            " broad-phase candidate without erasing that candidate");
    }

    {
        ian::MapDefinition map =
            ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world =
            ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        ian::Simulation elevatedBuilding{
            ian::GameBalance::defaults(), map, world};
        elevatedBuilding.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        elevatedBuilding.tick(1.0 / 60.0, unlimited);
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Core,
                .gridPosition = {0, 0},
            };
        elevatedBuilding.tick(1.0 / 60.0, placeCore);
        const auto ground =
            elevatedBuilding.placeFoundation({4.2, 0.0, 4.2});
        require(
            ground.has_value(),
            "elevated building fixture creates ground frame");
        const auto upper = elevatedBuilding.placeFloorPlatform(
            ground->anchor, 1,
            ground->floorHeight +
                ian::modularStoreyHeight(world));
        require(
            upper.has_value(),
            "elevated building fixture creates upper platform");
        const ian::GridPosition turretPosition{
            upper->anchor.x + 1,
            upper->anchor.z + 1};
        require(
            elevatedBuilding
                .previewPlacement(
                    ian::BuildingType::Turret,
                    turretPosition,
                    upper->floorHeight)
                .valid(),
            "ordinary building remains placeable on upper platform");
        ian::PlayerCommand placeTurret;
        placeTurret.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Turret,
                .gridPosition = turretPosition,
                .baseHeight = upper->floorHeight,
                .platformStorey = upper->storey,
                .lockHeight = true,
            };
        elevatedBuilding.tick(1.0 / 60.0, placeTurret);
        require(
            std::ranges::any_of(
                elevatedBuilding.snapshot().buildings,
                [turretPosition](
                    const ian::BuildingInstance& building) {
                    return building.type ==
                               ian::BuildingType::Turret &&
                           building.gridPosition ==
                               turretPosition &&
                           building.platformStorey == 1;
                }),
            "ordinary building is placed on upper platform");
    }

    {
        ian::MapDefinition map =
            ian::MapDefinition::defaults();
        map.resources = {
            {
                .type = ian::ResourceType::Wood,
                .position = {1.0, 5.5, 2.5},
                .radius = 1.0,
                .health = 3.0,
                .yield = 15,
                .respawnSeconds = 12.0,
            },
        };
        ian::Simulation raisedRampResources{
            ian::GameBalance::defaults(), map};
        raisedRampResources.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        raisedRampResources.tick(
            1.0 / 60.0, unlimited);
        const ian::Vec3 supportHit{
            0.2,
            raisedRampResources.terrain().getHeight(
                0.2, 4.2),
            4.2,
        };
        const auto groundFrame =
            raisedRampResources.placeFoundation(
                supportHit);
        require(
            groundFrame.has_value(),
            "raised ramp resource fixture creates ground frame");
        const auto groundRampPreview =
            raisedRampResources.previewRamp(
                supportHit,
                ian::Rotation::Deg180);
        require(
            groundRampPreview.error ==
                ian::ModularPlacementError::
                    ResourceBlocked,
            "ground ramp remains blocked by resource");
        const auto upperFrame =
            raisedRampResources.placeFloorPlatform(
                groundFrame->anchor, 1,
                groundFrame->floorHeight +
                    ian::modularStoreyHeight(
                        raisedRampResources
                            .terrain()
                            .config()));
        require(
            upperFrame.has_value(),
            "raised ramp resource fixture creates upper frame");
        const auto raisedRampPreview =
            raisedRampResources.previewRamp(
                supportHit,
                ian::Rotation::Deg180);
        require(
            raisedRampPreview.valid(),
            "resource below raised ramp does not block placement");
    }

    {
        ian::MapDefinition map =
            ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world =
            ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        ian::Simulation autoJumpSimulation{
            ian::GameBalance::defaults(),
            map, world};
        autoJumpSimulation.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        autoJumpSimulation.tick(
            1.0 / 60.0, unlimited);
        const auto frame =
            autoJumpSimulation.placeFoundation(
                {0.2, 0.0, 2.2});
        require(
            frame && frame->storey == 0 &&
                frame->floorHeight > 0.65,
            "auto-jump fixture creates raised first-level platform");

        ian::PlayerCommand walkForward;
        walkForward.moveForward = 1.0;
        bool becameAirborne = false;
        bool landedOnFrame = false;
        for (int tick = 0; tick < 120; ++tick) {
            autoJumpSimulation.tick(
                1.0 / 60.0, walkForward);
            const auto snapshot =
                autoJumpSimulation.snapshot();
            becameAirborne =
                becameAirborne ||
                !snapshot.playerGrounded;
            if (becameAirborne &&
                snapshot.playerGrounded &&
                std::abs(
                    snapshot.playerPosition.y -
                    frame->floorHeight -
                    1.7) < 1e-6) {
                landedOnFrame = true;
                break;
            }
        }
        require(
            becameAirborne && landedOnFrame,
            "walking into first-level platform automatically jumps onto it");

        ian::WorldConfig edgeBiasWorld = world;
        edgeBiasWorld.verticalGridStep = 1.25;
        ian::Simulation edgeBiasSimulation{
            ian::GameBalance::defaults(), map,
            edgeBiasWorld};
        edgeBiasSimulation.startRun();
        edgeBiasSimulation.tick(
            1.0 / 60.0, unlimited);
        const auto edgeBiasFrame =
            edgeBiasSimulation.placeFoundation(
                {0.2, 0.0, 2.2});
        require(
            edgeBiasFrame &&
                std::abs(
                    edgeBiasFrame->floorHeight - 1.25) <
                    1e-9,
            "edge-bias fixture creates a barely reachable platform");
        bool edgeBiasLanded = false;
        for (int tick = 0; tick < 150; ++tick) {
            edgeBiasSimulation.tick(
                1.0 / 60.0, walkForward);
            const auto edgeBiasSnapshot =
                edgeBiasSimulation.snapshot();
            if (edgeBiasSnapshot.playerGrounded &&
                std::abs(
                    edgeBiasSnapshot.playerPosition.y -
                    edgeBiasFrame->floorHeight - 1.7) <
                    1e-6) {
                edgeBiasLanded = true;
                break;
            }
        }
        require(
            edgeBiasLanded,
            "platform edge bias catches a jump that reaches just below the top");

        bool steppedOffFrame = false;
        for (int tick = 0; tick < 90; ++tick) {
            autoJumpSimulation.tick(
                1.0 / 60.0, walkForward);
            if (!autoJumpSimulation.snapshot().playerGrounded) {
                steppedOffFrame = true;
                break;
            }
        }
        require(steppedOffFrame,
                "coyote-time fixture walks off raised platform");
        ian::PlayerCommand lateJump;
        lateJump.jump = true;
        autoJumpSimulation.tick(1.0 / 60.0, lateJump);
        require(
            autoJumpSimulation.snapshot()
                    .playerVerticalVelocity > 0.0,
            "coyote time accepts jump shortly after leaving platform");

        ian::Simulation tapAutoJumpSimulation{
            ian::GameBalance::defaults(), map, world};
        tapAutoJumpSimulation.startRun();
        tapAutoJumpSimulation.tick(
            1.0 / 60.0, unlimited);
        const auto tapFrame =
            tapAutoJumpSimulation.placeFoundation(
                {0.2, 0.0, 2.2});
        require(
            tapFrame.has_value(),
            "tap auto-jump fixture creates platform");
        bool tapBecameAirborne = false;
        bool tapLandedOnFrame = false;
        for (int tick = 0; tick < 150; ++tick) {
            ian::PlayerCommand command;
            if (!tapBecameAirborne) {
                command.moveForward = 1.0;
            }
            tapAutoJumpSimulation.tick(
                1.0 / 60.0, command);
            const auto tapSnapshot =
                tapAutoJumpSimulation.snapshot();
            tapBecameAirborne =
                tapBecameAirborne ||
                !tapSnapshot.playerGrounded;
            if (tapBecameAirborne &&
                tapSnapshot.playerGrounded &&
                std::abs(
                    tapSnapshot.playerPosition.y -
                    tapFrame->floorHeight - 1.7) <
                    1e-6) {
                tapLandedOnFrame = true;
                break;
            }
        }
        require(
            tapBecameAirborne && tapLandedOnFrame,
            "auto-jump carries a released input safely onto"
            " the platform");
    }

    {
        ian::MapDefinition map =
            ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world =
            ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        world.verticalGridStep = 1.5;
        ian::Simulation shortJumpSimulation{
            ian::GameBalance::defaults(), map, world};
        shortJumpSimulation.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        shortJumpSimulation.tick(
            1.0 / 60.0, unlimited);
        const auto highFrame =
            shortJumpSimulation.placeFoundation(
                {0.2, 0.0, 2.2});
        require(
            highFrame &&
                std::abs(highFrame->floorHeight - 1.5) <
                    1e-9,
            "failed-jump fixture creates an unreachable platform");

        ian::PlayerCommand forward;
        forward.moveForward = 1.0;
        for (int tick = 0; tick < 120; ++tick) {
            shortJumpSimulation.tick(
                1.0 / 60.0, forward);
        }
        ian::PlayerCommand jumpForward = forward;
        jumpForward.jump = true;
        shortJumpSimulation.tick(
            1.0 / 60.0, jumpForward);
        double closestZ =
            shortJumpSimulation.snapshot()
                .playerPosition.z;
        for (int tick = 0; tick < 120; ++tick) {
            shortJumpSimulation.tick(
                1.0 / 60.0, forward);
            closestZ = std::min(
                closestZ,
                shortJumpSimulation.snapshot()
                    .playerPosition.z);
        }
        const double contactZ =
            (highFrame->anchor.z +
             ian::PlatformFrameWidthCells) *
                world.cellSize +
            ian::CollisionWorld::PlayerRadius;
        require(
            closestZ >= contactZ - 1e-4,
            "failed jump cannot enter the side of a platform");

        const double beforeRetreat =
            shortJumpSimulation.snapshot()
                .playerPosition.z;
        ian::PlayerCommand retreat;
        retreat.moveForward = -1.0;
        for (int tick = 0; tick < 30; ++tick) {
            shortJumpSimulation.tick(
                1.0 / 60.0, retreat);
        }
        require(
            shortJumpSimulation.snapshot()
                    .playerPosition.z >
                beforeRetreat + 0.2,
            "player can immediately retreat after a failed platform jump");
    }

    {
        ian::MapDefinition map =
            ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world =
            ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        ian::Simulation bufferedJumpSimulation{
            ian::GameBalance::defaults(), map, world};
        bufferedJumpSimulation.startRun();
        const double groundEyeHeight =
            bufferedJumpSimulation.snapshot()
                .playerPosition.y;
        ian::PlayerCommand initialJump;
        initialJump.jump = true;
        bufferedJumpSimulation.tick(
            1.0 / 60.0, initialJump);
        for (int tick = 0; tick < 180; ++tick) {
            const auto snapshot =
                bufferedJumpSimulation.snapshot();
            if (snapshot.playerVerticalVelocity < 0.0 &&
                snapshot.playerPosition.y - groundEyeHeight <
                    0.12) {
                break;
            }
            bufferedJumpSimulation.tick(1.0 / 60.0);
        }
        ian::PlayerCommand bufferedJump;
        bufferedJump.jump = true;
        bufferedJumpSimulation.tick(
            1.0 / 60.0, bufferedJump);
        bufferedJumpSimulation.tick(1.0 / 60.0);
        require(
            !bufferedJumpSimulation.snapshot()
                 .playerGrounded &&
                bufferedJumpSimulation.snapshot()
                        .playerVerticalVelocity > 0.0,
            "jump buffer launches player when input arrives before landing");
        const auto landingEvents =
            bufferedJumpSimulation.takeEvents();
        require(
            std::any_of(
                landingEvents.begin(), landingEvents.end(),
                [](const ian::GameEvent& event) {
                    return event.type ==
                               ian::GameEventType::PlayerLanded &&
                           event.intensity > 1.0;
                }),
            "landing emits impact event for presentation and audio");
    }

    {
        ian::Simulation daytimeSpawning;
        daytimeSpawning.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        daytimeSpawning.tick(1.0 / 60.0, unlimited);
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding =
            ian::PlaceBuildingCommand{ian::BuildingType::Core, {0, 0}, 0};
        daytimeSpawning.tick(1.0 / 60.0, placeCore);

        ian::PlayerCommand spawn;
        spawn.spawnEnemy =
            ian::SpawnEnemyCommand{ian::EnemyType::Heavy};
        daytimeSpawning.tick(1.0 / 60.0, spawn);
        require(daytimeSpawning.snapshot().activeEnemyCount == 1,
                "debug command spawns enemy during daytime");
        const auto firstPosition =
            daytimeSpawning.snapshot().enemies.front().position;
        const double firstDistance =
            std::sqrt(firstPosition.x * firstPosition.x +
                      firstPosition.z * firstPosition.z);
        require(firstDistance >= 18.0 && firstDistance <= 22.0,
                "daytime debug enemy spawns in ring around core");

        daytimeSpawning.tick(1.0 / 60.0, spawn);
        require(daytimeSpawning.snapshot().activeEnemyCount == 2,
                "daytime debug command supports repeated spawning");
        const auto secondPosition =
            daytimeSpawning.snapshot().enemies.back().position;
        require(firstPosition.x != secondPosition.x ||
                    firstPosition.z != secondPosition.z,
                "daytime debug spawns use different positions");

        ian::PlayerCommand spawnElite;
        spawnElite.spawnEnemy = ian::SpawnEnemyCommand{
            .type = ian::EnemyType::Fast,
            .count = 1,
            .eliteAffixes = ian::eliteAffixMask(
                ian::EliteAffix::Berserker),
        };
        daytimeSpawning.tick(1.0 / 60.0, spawnElite);
        const auto eliteSnapshot = daytimeSpawning.snapshot();
        const auto elite = std::find_if(
            eliteSnapshot.enemies.begin(),
            eliteSnapshot.enemies.end(),
            [](const ian::EnemyInstance& enemy) {
                return enemy.active && ian::hasEliteAffix(
                    enemy.eliteAffixes,
                    ian::EliteAffix::Berserker);
            });
        require(
            elite != eliteSnapshot.enemies.end(),
            "debug command spawns selected elite variant");
    }

    auto simulationBalance = ian::GameBalance::defaults();
    simulationBalance.gameplay.pickaxeDamageVariation = 0.0;
    simulationBalance.gameplay.pickaxeCriticalChance = 0.0;
    simulationBalance.gameplay.playerRespawnSeconds = 1.0;

    ian::Simulation rangerClass{simulationBalance};
    rangerClass.startRun(ian::PlayerClass::Ranger);
    require(
        rangerClass.snapshot().playerClass ==
                ian::PlayerClass::Ranger,
        "ranger class selection is exposed in the snapshot");
    requireNear(
        rangerClass.snapshot().playerMaxHealth, 80.0, 1e-12,
        "ranger class reduces maximum health");
    requireNear(
        rangerClass.snapshot().playerHealth, 80.0, 1e-12,
        "ranger class starts at full adjusted health");
    requireNear(
        rangerClass.snapshot().playerDamageMultiplier, 1.25, 1e-12,
        "ranger class increases damage");
    requireNear(
        rangerClass.snapshot().playerMoveSpeedMultiplier, 1.12, 1e-12,
        "ranger class applies its glass-cannon run modifiers");
    require(
        rangerClass.skillTree().isUnlocked("combat_training") &&
            rangerClass.skillTree().isUnlocked("rifle") &&
            rangerClass.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Rifle &&
            rangerClass.skillTree().points() == 0,
        "ranger starts with its rifle path unlocked for free");
    rangerClass.restartRun();
    require(
        rangerClass.snapshot().playerClass ==
                ian::PlayerClass::Ranger,
        "run restart preserves the selected player class");
    requireNear(
        rangerClass.snapshot().playerMaxHealth, 80.0, 1e-12,
        "restarted class reapplies its modifiers once");

    ian::Simulation vanguardClass{simulationBalance};
    vanguardClass.startRun(ian::PlayerClass::Vanguard);
    requireNear(
        vanguardClass.snapshot().playerMaxHealth, 135.0, 1e-12,
        "vanguard class increases maximum health");
    requireNear(
        vanguardClass.snapshot().playerArmorMultiplier, 1.25, 1e-12,
        "vanguard class adds damage resistance");
    requireNear(
        vanguardClass.snapshot().playerDamageMultiplier, 0.90, 1e-12,
        "vanguard class trades damage for health and resistance");
    require(
        vanguardClass.skillTree().isUnlocked("combat_training") &&
            vanguardClass.skillTree().isUnlocked("club") &&
            vanguardClass.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Club,
        "vanguard starts with combat training and club nodes");

    ian::Simulation engineerClass{simulationBalance};
    engineerClass.startRun(ian::PlayerClass::Engineer);
    require(
        engineerClass.snapshot().playerClass ==
                ian::PlayerClass::Engineer,
        "engineer class selection is exposed in the snapshot");
    requireNear(
        engineerClass.snapshot().playerDamageMultiplier, 0.90, 1e-12,
        "engineer class applies its personal damage tradeoff");
    require(
        engineerClass.skillTree().isUnlocked("hammer") &&
            engineerClass.skillTree().isUnlocked(
                "defense_engineering") &&
            engineerClass.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Hammer,
        "engineer starts with hammer and defense nodes");
    const auto engineerStone = std::ranges::find_if(
        engineerClass.snapshot().resourceNodes,
        [](const ian::ResourceNode& node) {
            return node.active &&
                node.type == ian::ResourceType::Stone;
        });
    require(engineerStone != engineerClass.snapshot().resourceNodes.end(),
            "engineer Smart Tools fixture has stone");
    ian::PlayerCommand engineerAimStone;
    engineerAimStone.overrideAimedResource = true;
    engineerAimStone.aimedResourceOverride = engineerStone->id;
    engineerClass.tick(1.0 / 60.0, engineerAimStone);
    require(
        engineerClass.snapshot().selectedWeapon ==
            ian::PlayerWeapon::BareHands,
        "Smart Tools falls back from hammer to hands when pickaxe is locked");

    ian::Simulation prospectorClass{simulationBalance};
    prospectorClass.startRun(ian::PlayerClass::Prospector);
    require(
        prospectorClass.snapshot().playerClass ==
                ian::PlayerClass::Prospector &&
            prospectorClass.skillTree().isUnlocked("axe") &&
            prospectorClass.skillTree().isUnlocked("pickaxe") &&
            prospectorClass.skillTree().isUnlocked(
                "efficient_strikes") &&
            prospectorClass.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Pickaxe &&
            prospectorClass.skillTree().points() == 0,
        "prospector starts with free resource gathering nodes");
    requireNear(
        prospectorClass.snapshot().playerDamageMultiplier,
        0.85, 1e-12,
        "prospector trades combat damage for economy");

    auto berserkerClass =
        std::make_unique<ian::Simulation>(simulationBalance);
    berserkerClass->startRun(ian::PlayerClass::Berserker);
    require(
        berserkerClass->skillTree().isUnlocked("club") &&
            berserkerClass->skillTree().isUnlocked("dash") &&
            berserkerClass->snapshot().selectedWeapon ==
                ian::PlayerWeapon::Club,
        "berserker starts with club and dash paths");
    ian::PlayerCommand woundBerserker;
    woundBerserker.damagePlayer = ian::DamagePlayerCommand{42.5};
    berserkerClass->tick(0.0, woundBerserker);
    requireNear(
        berserkerClass->snapshot().playerDamageMultiplier,
        1.375, 1e-12,
        "berserker gains damage from missing half of maximum health");

    auto vampireBalance = simulationBalance;
    vampireBalance.buildings[static_cast<std::size_t>(
        ian::BuildingType::Core)].wood = 0;
    auto vampireClass =
        std::make_unique<ian::Simulation>(vampireBalance);
    vampireClass->startRun(ian::PlayerClass::Vampire);
    ian::PlayerCommand woundVampire;
    woundVampire.damagePlayer = ian::DamagePlayerCommand{55.0};
    vampireClass->tick(0.0, woundVampire);
    const double vampireHealthBeforeKill =
        vampireClass->snapshot().playerHealth;
    ian::PlayerCommand placeVampireCore;
    placeVampireCore.placeBuilding = ian::PlaceBuildingCommand{
        ian::BuildingType::Core, {0, 0}, 0};
    vampireClass->tick(0.0, placeVampireCore);
    ian::PlayerCommand spawnVampireTarget;
    spawnVampireTarget.spawnEnemy = ian::SpawnEnemyCommand{
        .type = ian::EnemyType::Basic,
        .count = 1,
    };
    vampireClass->tick(0.0, spawnVampireTarget);
    require(!vampireClass->snapshot().enemies.empty(),
            "vampire fixture spawns a target");
    const ian::EntityId vampireTarget =
        vampireClass->snapshot().enemies.front().id;
    ian::PlayerCommand vampireKill;
    vampireKill.castChainLightning = ian::CastChainLightningCommand{
        .firstTarget = vampireTarget,
        .damage = 10000.0,
        .maximumTargets = 1,
    };
    vampireClass->tick(0.0, vampireKill);
    requireNear(
        vampireClass->snapshot().playerHealth,
        vampireHealthBeforeKill + 2.0, 1e-12,
        "vampire restores health after an enemy kill");

    auto alchemistClass =
        std::make_unique<ian::Simulation>(simulationBalance);
    alchemistClass->startRun(ian::PlayerClass::Alchemist);
    require(
        alchemistClass->snapshot().lootStacks[
            ian::lootUpgradeIndex(ian::LootUpgradeEffect::Apple)] == 1 &&
        alchemistClass->snapshot().lootStacks[
            ian::lootUpgradeIndex(ian::LootUpgradeEffect::Potion)] == 1,
        "alchemist starts with Apple and Potion items");

    auto chronomancerClass =
        std::make_unique<ian::Simulation>(simulationBalance);
    chronomancerClass->startRun(ian::PlayerClass::Chronomancer);
    require(
        chronomancerClass->snapshot().lootStacks[
            ian::lootUpgradeIndex(ian::LootUpgradeEffect::Hourglass)] == 1 &&
        chronomancerClass->snapshot().lootStacks[
            ian::lootUpgradeIndex(ian::LootUpgradeEffect::Rope)] == 1,
        "chronomancer starts with Hourglass and Rope items");

    ian::Simulation simulation{simulationBalance};
    require(simulation.snapshot().state == ian::RunState::MainMenu, "simulation starts in menu");

    simulation.startRun();
    simulation.tick(std::numeric_limits<double>::quiet_NaN());
    simulation.tick(-1.0);
    require(
        simulation.snapshot().tick == 0 &&
            std::isfinite(simulation.snapshot().playerPosition.x),
        "simulation rejects invalid delta times without mutating state");
    simulation.tick(1.0 / 60.0);
    require(simulation.snapshot().buildingCosts[0].wood == 30 &&
                simulation.snapshot().buildingCosts[2].stone == 15,
            "snapshot exposes configured hotbar building costs");
    require(
        simulation.snapshot()
                    .modularBuildingCosts[0]
                    .wood == 20 &&
            simulation.snapshot()
                    .modularBuildingCosts[1]
                    .wood == 20 &&
            simulation.snapshot()
                    .modularBuildingCosts[2]
                    .stone == 10 &&
            simulation.snapshot()
                    .modularBuildingCosts[3]
                    .wood == 16,
        "snapshot exposes modular hotbar costs");
    require(simulation.snapshot().tick == 1, "running simulation advances");
    require(simulation.snapshot().playerHealth == simulation.snapshot().playerMaxHealth,
            "run starts with full player health");
    require(simulation.snapshot().tutorialObjective ==
                ian::TutorialObjective::BareHandsTraining,
            "tutorial starts with bare-hands objective");

    const auto startPosition = simulation.snapshot().playerPosition;
    ian::PlayerCommand movement;
    movement.moveForward = 1.0;
    simulation.tick(1.0, movement);
    require(simulation.snapshot().playerPosition.z < startPosition.z,
            "forward command moves player forward");
    const auto positionBeforeBraking =
        simulation.snapshot().playerPosition;
    simulation.tick(1.0 / 60.0);
    require(
        simulation.snapshot().playerPosition.z <
            positionBeforeBraking.z,
        "player keeps a short horizontal coast after input release");
    simulation.tick(0.5);
    const auto stoppedPosition =
        simulation.snapshot().playerPosition;
    simulation.tick(0.25);
    requireNear(
        simulation.snapshot().playerPosition.z,
        stoppedPosition.z, 1e-9,
        "player braking reaches a stable stop");

    ian::PlayerCommand jump;
    jump.jump = true;
    simulation.tick(1.0 / 60.0, jump);
    require(!simulation.snapshot().playerGrounded, "jump leaves ground");
    require(simulation.snapshot().playerPosition.y > startPosition.y, "jump raises player");

    const auto tickBeforePause = simulation.snapshot().tick;
    simulation.togglePause();
    simulation.tick(1.0 / 60.0);
    require(simulation.snapshot().tick == tickBeforePause, "paused simulation does not advance");

    std::vector<ian::Vec3> resourcePositionsBeforeRestart;
    resourcePositionsBeforeRestart.reserve(
        simulation.snapshot().resourceNodes.size());
    for (const auto& resource : simulation.snapshot().resourceNodes) {
        resourcePositionsBeforeRestart.push_back(resource.position);
    }
    std::vector<std::pair<ian::Vec3, ian::LootUpgradeEffect>>
        chestLayoutBeforeRestart;
    chestLayoutBeforeRestart.reserve(
        simulation.snapshot().lootChests.size());
    for (const auto& chest : simulation.snapshot().lootChests) {
        chestLayoutBeforeRestart.emplace_back(
            chest.position, chest.loot.effect);
    }

    simulation.restartRun();
    require(simulation.snapshot().state == ian::RunState::BuildPhase,
            "restart immediately starts the first preparation timer");
    require(simulation.snapshot().phaseTimeRemaining > 0.0 &&
                simulation.snapshot().phaseTimeRemaining ==
                    simulation.snapshot().phaseDuration,
            "first preparation timer starts before the core is placed");
    require(simulation.snapshot().tick == 0, "restart resets tick counter");
    require(simulation.snapshot().playerHealth == simulation.snapshot().playerMaxHealth,
            "restart restores player health");
    const bool resourceLayoutChanged = std::ranges::any_of(
        simulation.snapshot().resourceNodes,
        [&](const ian::ResourceNode& resource) {
            const std::size_t index = static_cast<std::size_t>(
                &resource - simulation.snapshot().resourceNodes.data());
            if (index >= resourcePositionsBeforeRestart.size()) return true;
            const auto& previous = resourcePositionsBeforeRestart[index];
            return std::abs(resource.position.x - previous.x) > 1e-6 ||
                std::abs(resource.position.z - previous.z) > 1e-6;
        });
    require(resourceLayoutChanged,
            "restart regenerates resource and prop placement");
    const bool chestLayoutOrLootChanged =
        simulation.snapshot().lootChests.size() !=
            chestLayoutBeforeRestart.size() ||
        std::ranges::any_of(
            simulation.snapshot().lootChests,
            [&](const ian::LootChestInstance& chest) {
                const std::size_t index = static_cast<std::size_t>(
                    &chest - simulation.snapshot().lootChests.data());
                if (index >= chestLayoutBeforeRestart.size()) return true;
                const auto& [position, effect] =
                    chestLayoutBeforeRestart[index];
                return std::abs(chest.position.x - position.x) > 1e-6 ||
                    std::abs(chest.position.z - position.z) > 1e-6 ||
                    chest.loot.effect != effect;
            });
    require(chestLayoutOrLootChanged,
            "restart regenerates chest placement or contents");
    const auto restartEvents = simulation.takeEvents();
    require(
        restartEvents.size() == 1U &&
            restartEvents.front().type ==
                ian::GameEventType::RunRestarted,
        "restart discards stale events from previous run");
    unlockAxe(simulation);

    const auto restartTree = std::ranges::find_if(
        simulation.snapshot().resourceNodes,
        [](const ian::ResourceNode& resource) {
            return resource.active &&
                resource.type == ian::ResourceType::Wood;
        });
    require(restartTree != simulation.snapshot().resourceNodes.end(),
            "restarted run contains a tree for gathering tests");
    ian::PlayerCommand attack;
    attack.overrideAimedResource = true;
    attack.aimedResourceOverride = restartTree->id;
    attack.usePickaxe = true;
    simulation.tick(1.0 / 60.0, attack);
    require(simulation.snapshot().wood == 0,
            "resource stays pending during pickup flight");

    simulation.tick(0.5);
    simulation.tick(1.0 / 60.0, attack);
    simulation.tick(0.5);
    simulation.tick(1.0 / 60.0, attack);
    require(simulation.snapshot().wood == 5,
            "first resource grant lands after pickup delay");
    simulation.tick(0.8);
    require(simulation.snapshot().wood == 15,
            "all resource grants land after their pickup delay");

    const auto events = simulation.takeEvents();
    int gatheredWood = 0;
    int grantedWood = 0;
    bool collectedEventFound = false;
    for (const auto& event : events) {
        if ((event.type == ian::GameEventType::ResourceHit ||
             event.type == ian::GameEventType::ResourceCollected) &&
            event.resourceType == ian::ResourceType::Wood) {
            gatheredWood += event.amount;
        }
        if (event.type == ian::GameEventType::ResourceGranted &&
            event.resourceType == ian::ResourceType::Wood) {
            grantedWood += event.amount;
        }
        if (event.type == ian::GameEventType::ResourceCollected &&
            event.resourceType == ian::ResourceType::Wood) {
            collectedEventFound = true;
        }
    }
    require(gatheredWood == 15,
            "resource hit events distribute exact tree capacity");
    require(grantedWood == 15,
            "delayed grant events deliver exact tree capacity");
    require(collectedEventFound, "collection emits ResourceCollected event");
    require(simulation.snapshot().runStatistics.woodAcquired == 15,
            "run statistics record resources after events are drained");

    const auto deathPosition =
        simulation.snapshot().playerPosition;
    ian::PlayerCommand lethalDamage;
    lethalDamage.damagePlayer =
        ian::DamagePlayerCommand{1000.0};
    simulation.tick(1.0 / 60.0, lethalDamage);
    require(
        simulation.snapshot().playerRespawning &&
            simulation.snapshot().playerHealth == 0.0,
        "lethal damage starts delayed respawn");
    require(
        simulation.snapshot().wood == 11 &&
            simulation.snapshot().deathLostWood == 4,
        "death loses rounded-up resource fraction");
    ian::PlayerCommand deadMovement;
    deadMovement.moveForward = 1.0;
    simulation.tick(0.5, deadMovement);
    require(
        simulation.snapshot().playerRespawning &&
            simulation.snapshot().playerPosition.x ==
                deathPosition.x &&
            simulation.snapshot().playerPosition.z ==
                deathPosition.z,
        "respawning player cannot move");
    simulation.tick(0.51);
    require(
        !simulation.snapshot().playerRespawning &&
            simulation.snapshot().playerHealth ==
                simulation.snapshot().playerMaxHealth,
        "respawn restores player after configured delay");
    const auto respawnEvents = simulation.takeEvents();
    require(
        std::any_of(
            respawnEvents.begin(), respawnEvents.end(),
            [](const ian::GameEvent& event) {
                return event.type ==
                       ian::GameEventType::PlayerRespawned;
            }),
        "delayed respawn emits completion event");

    auto criticalBalance = ian::GameBalance::defaults();
    criticalBalance.gameplay.pickaxeDamageVariation = 0.0;
    criticalBalance.gameplay.pickaxeCriticalChance = 1.0;
    ian::Simulation criticalSimulation{criticalBalance};
    criticalSimulation.startRun();
    unlockAxe(criticalSimulation);
    criticalSimulation.tick(1.0 / 60.0, attack);
    const auto criticalEvents = criticalSimulation.takeEvents();
    const auto criticalHit = std::find_if(
        criticalEvents.begin(), criticalEvents.end(),
        [](const ian::GameEvent& event) {
            return event.type == ian::GameEventType::ResourceHit;
        });
    require(criticalHit != criticalEvents.end() &&
                criticalHit->critical &&
                criticalHit->damage ==
                    criticalBalance.gameplay.pickaxeDamage * 2.0,
            "pickaxe critical hit doubles damage and marks event");

    simulation.restartRun();
    ian::PlayerCommand unlimited;
    unlimited.enableUnlimitedResources = ian::EnableUnlimitedResourcesCommand{};
    simulation.tick(1.0 / 60.0, unlimited);
    require(simulation.snapshot().unlimitedResources, "unlimited resource command enables cheat");
    require(simulation.snapshot().playerInvulnerable,
            "god mode makes player invulnerable");
    require(simulation.snapshot().tutorialObjective ==
                ian::TutorialObjective::PlaceCore,
            "unlimited resources skip gathering objective");
    require(simulation.snapshot().bombsRemaining ==
                std::numeric_limits<int>::max(),
            "god mode exposes infinite bomb inventory");
    require(simulation.snapshot().skillPoints ==
                std::numeric_limits<int>::max(),
            "god mode exposes infinite skill points");
    const auto nightlyChestSkill =
        simulation.skillTree().indexOf("nightly_chest");
    require(
        nightlyChestSkill &&
            simulation.purchaseSkill(*nightlyChestSkill) ==
                ian::SkillPurchaseError::None,
        "nightly chest fixture unlocks survived-night reward");
    const auto frequentBountySkill =
        simulation.skillTree().indexOf("frequent_bounty");
    require(
        frequentBountySkill &&
            simulation.purchaseSkill(*frequentBountySkill) ==
                ian::SkillPurchaseError::None,
        "nightly delivery fixture upgrades the reward to every night");
    const std::size_t chestsBeforeFirstNight =
        simulation.snapshot().lootChests.size();
    constexpr std::array<ian::PlayerWeapon, 9> GodModeTools{{
        ian::PlayerWeapon::Axe,
        ian::PlayerWeapon::Pickaxe,
        ian::PlayerWeapon::Club,
        ian::PlayerWeapon::IceWand,
        ian::PlayerWeapon::FireWand,
        ian::PlayerWeapon::Hammer,
        ian::PlayerWeapon::Rifle,
        ian::PlayerWeapon::Bomb,
        ian::PlayerWeapon::BareHands,
    }};
    for (const ian::PlayerWeapon expected : GodModeTools) {
        ian::PlayerCommand cycle;
        cycle.toggleWeapon = ian::ToggleWeaponCommand{};
        cycle.overrideAimedResource = true;
        cycle.aimedResourceOverride.reset();
        simulation.tick(1.0 / 60.0, cycle);
        require(simulation.snapshot().selectedWeapon == expected,
                "god mode weapon cycle includes every tool and weapon");
    }
    ian::PlayerCommand selectGodModeAxe;
    selectGodModeAxe.toggleWeapon =
        ian::ToggleWeaponCommand{};
    simulation.tick(1.0 / 60.0, selectGodModeAxe);
    const auto godModeSnapshot = simulation.snapshot();
    const auto godModeStone = std::find_if(
        godModeSnapshot.resourceNodes.begin(),
        godModeSnapshot.resourceNodes.end(),
        [](const ian::ResourceNode& node) {
            return node.active &&
                node.type == ian::ResourceType::Stone;
        });
    require(
        godModeStone != godModeSnapshot.resourceNodes.end() &&
            godModeSnapshot.selectedWeapon ==
                ian::PlayerWeapon::Axe,
        "god mode tool animation fixture selects axe manually");
    ian::PlayerCommand godModeAimStone;
    godModeAimStone.overrideAimedResource = true;
    godModeAimStone.aimedResourceOverride = godModeStone->id;
    simulation.tick(1.0 / 60.0, godModeAimStone);
    require(
        simulation.snapshot().selectedWeapon ==
            ian::PlayerWeapon::Pickaxe,
        "base Smart Tools also selects the correct tool in god mode");
    const auto godModeAxeSkill =
        simulation.skillTree().indexOf("axe");
    require(
        godModeAxeSkill &&
            simulation.purchaseSkill(*godModeAxeSkill) ==
                ian::SkillPurchaseError::None &&
            simulation.snapshot().skillPoints ==
                std::numeric_limits<int>::max() &&
            simulation.skillTree().points() == 0,
        "god mode purchases skills without consuming stored points");
    ian::PlayerCommand freeBomb;
    freeBomb.selectWeapon =
        ian::SelectWeaponCommand{ian::PlayerWeapon::Bomb};
    freeBomb.useConsumable = ian::UseConsumableCommand{};
    simulation.tick(1.0 / 60.0, freeBomb);
    require(simulation.snapshot().bombsRemaining ==
                std::numeric_limits<int>::max() &&
                !simulation.snapshot().bombProjectiles.empty(),
            "god mode throws bombs without reducing infinite inventory");

    ian::PlayerCommand placeCore;
    placeCore.placeBuilding =
        ian::PlaceBuildingCommand{ian::BuildingType::Core, {0, 0}, 0};
    simulation.tick(1.0 / 60.0, placeCore);
    require(simulation.snapshot().coreMaxHealth > 0.0,
            "unlimited resources allow building without inventory");
    require(!simulation.snapshot().flowDebugVectors.empty(),
            "placing core publishes flow-field debug samples");
    require(simulation.snapshot().wood == 0, "unlimited building does not spend inventory");
    require(simulation.snapshot().tutorialObjective ==
                ian::TutorialObjective::BuildCrystalMine,
            "tutorial requests first crystals mine after core");
    ian::PlayerCommand toggleInvulnerability;
    toggleInvulnerability.toggleInvulnerability =
        ian::ToggleInvulnerabilityCommand{};
    simulation.tick(1.0 / 60.0, toggleInvulnerability);
    require(simulation.snapshot().playerInvulnerable,
            "player invulnerability cannot be disabled while god mode is active");
    const double coreHealthBeforeDebugDamage = simulation.snapshot().coreHealth;
    ian::PlayerCommand damageCore;
    damageCore.damageCore = ian::DamageCoreCommand{25.0};
    simulation.tick(1.0 / 60.0, damageCore);
    requireNear(simulation.snapshot().coreHealth,
                coreHealthBeforeDebugDamage, 1e-12,
                "god mode prevents core damage");

    std::optional<ian::GridPosition> wallGrid;
    for (int z = 2; z <= 6 && !wallGrid; ++z) {
        for (int x = -4; x <= 4; ++x) {
            const ian::GridPosition candidate{x, z};
            if (simulation.previewPlacement(
                    ian::BuildingType::Wall, candidate).valid()) {
                wallGrid = candidate;
                break;
            }
        }
    }
    require(wallGrid.has_value(),
            "debug fixture finds an unoccupied wall cell");
    ian::PlayerCommand placeWall;
    placeWall.placeBuilding =
        ian::PlaceBuildingCommand{
            ian::BuildingType::Wall, *wallGrid, 0};
    simulation.tick(1.0 / 60.0, placeWall);
    const auto wallSnapshot = simulation.snapshot();
    const auto wallBuilding = std::find_if(
        wallSnapshot.buildings.begin(),
        wallSnapshot.buildings.end(),
        [&wallGrid](const ian::BuildingInstance& building) {
            return building.type == ian::BuildingType::Wall &&
                   building.gridPosition == *wallGrid;
        });
    require(
        wallBuilding != wallSnapshot.buildings.end(),
        "placed wall exists in simulation");
    const std::optional<ian::EntityId> wall =
        wallBuilding->id;

    ian::PlayerCommand lockedWeaponUpgrade;
    lockedWeaponUpgrade.selectWeapon =
        ian::SelectWeaponCommand{ian::PlayerWeapon::Rifle};
    lockedWeaponUpgrade.upgradeWeapon = ian::UpgradeWeaponCommand{};
    simulation.tick(1.0 / 60.0, lockedWeaponUpgrade);
    require(simulation.snapshot().rifleLevel == 1,
            "rifle stays locked at core level one");
    auto weaponEvents = simulation.takeEvents();
    bool coreRequirementEventFound = false;
    for (const auto& event : weaponEvents) {
        if (event.type == ian::GameEventType::WeaponUpgradeRejected &&
            event.weaponUpgradeError == ian::WeaponUpgradeError::CoreLevelRequired) {
            coreRequirementEventFound = true;
        }
    }
    require(coreRequirementEventFound,
            "rifle upgrade rejection reports core-level requirement");

    const auto coreId = *simulation.snapshot().coreId;
    ian::PlayerCommand placementUpgradeConflict;
    placementUpgradeConflict.selectBuilding =
        ian::BuildingType::Turret;
    placementUpgradeConflict.upgradeBuilding =
        ian::UpgradeBuildingCommand{coreId};
    simulation.tick(1.0 / 60.0, placementUpgradeConflict);
    require(simulation.snapshot().coreLevel == 1,
            "placement mode blocks stale building upgrade command");
    ian::PlayerCommand cancelPlacement;
    cancelPlacement.cancelBuilding = true;
    simulation.tick(1.0 / 60.0, cancelPlacement);

    ian::PlayerCommand upgradeCore;
    upgradeCore.upgradeBuilding =
        ian::UpgradeBuildingCommand{coreId};
    simulation.tick(1.0 / 60.0, upgradeCore);
    ian::PlayerCommand upgradeWeapon;
    upgradeWeapon.upgradeWeapon = ian::UpgradeWeaponCommand{};
    simulation.tick(1.0 / 60.0, upgradeWeapon);
    require(simulation.snapshot().rifleLevel == 2 &&
                simulation.snapshot().rifleMagazineSize == 10,
            "simulation upgrades rifle after core unlock");
    weaponEvents = simulation.takeEvents();
    bool weaponUpgradeEventFound = false;
    for (const auto& event : weaponEvents) {
        if (event.type == ian::GameEventType::WeaponUpgraded && event.amount == 2) {
            weaponUpgradeEventFound = true;
        }
    }
    require(weaponUpgradeEventFound, "rifle upgrade emits WeaponUpgraded event");

    ian::PlayerCommand upgradeWall;
    upgradeWall.upgradeBuilding = ian::UpgradeBuildingCommand{*wall};
    simulation.tick(1.0 / 60.0, upgradeWall);
    const auto upgradedWall = simulation.snapshot().buildings[1];
    require(upgradedWall.level == 2,
            "simulation upgrades aimed building after core unlock");
    requireNear(upgradedWall.maxHealth, 115.0, 1e-9,
                "simulation uses gradual building health growth");

    unlockHammer(simulation);
    ian::PlayerCommand repairFullWall;
    repairFullWall.repairBuilding = ian::RepairBuildingCommand{*wall};
    simulation.tick(1.0 / 60.0, repairFullWall);
    auto buildingEvents = simulation.takeEvents();
    bool fortified = false;
    for (const auto& event : buildingEvents) {
        if (event.type == ian::GameEventType::BuildingFortified) fortified = true;
    }
    require(fortified, "hammer fortifies full-health building");

    simulation.tick(1.0 / 60.0, repairFullWall);
    buildingEvents = simulation.takeEvents();
    require(
        std::ranges::any_of(
            buildingEvents,
            [](const ian::GameEvent& event) {
                return event.type ==
                           ian::GameEventType::BuildingRepairRejected &&
                    event.buildingActionError ==
                        ian::BuildingActionError::Cooldown &&
                    event.intensity > 0.0;
            }),
        "hammer repair and fortification have a per-target cooldown");
    simulation.tick(
        ian::GameBalance::defaults().economy.repairCooldownSeconds);
    simulation.tick(1.0 / 60.0, repairFullWall);
    buildingEvents = simulation.takeEvents();
    require(
        std::ranges::any_of(
            buildingEvents,
            [](const ian::GameEvent& event) {
                return event.type ==
                    ian::GameEventType::BuildingFortified;
            }),
        "hammer can affect the target after repair cooldown expires");

    ian::PlayerCommand sellWall;
    sellWall.sellBuilding = ian::SellBuildingCommand{*wall};
    simulation.tick(1.0 / 60.0, sellWall);
    require(simulation.snapshot().buildings.size() == 1,
            "sell command removes non-core building");
    buildingEvents = simulation.takeEvents();
    bool soldEventFound = false;
    for (const auto& event : buildingEvents) {
        if (event.type == ian::GameEventType::BuildingSold && event.entityId == wall) {
            soldEventFound = true;
        }
    }
    require(soldEventFound, "sell command emits BuildingSold event");

    ian::PlayerCommand startWaveEarly;
    startWaveEarly.startWaveEarly = ian::StartWaveEarlyCommand{};
    simulation.tick(1.0 / 60.0, startWaveEarly);
    require(simulation.snapshot().state == ian::RunState::Wave,
            "early-wave command immediately starts wave");
    require(simulation.snapshot().bombsRemaining ==
                std::numeric_limits<int>::max(),
            "god mode keeps infinite bombs when night begins");
    require(simulation.snapshot().upcomingAttackDirection ==
                ian::AttackDirection::South,
            "early wave uses least-visible attack direction");
    require(simulation.snapshot().upcomingAttackDirections[
                static_cast<std::size_t>(ian::AttackDirection::South)] &&
                std::count(simulation.snapshot().upcomingAttackDirections.begin(),
                           simulation.snapshot().upcomingAttackDirections.end(),
                           true) == 1,
            "first wave advertises its single attack front");
    require(simulation.snapshot().activeEnemyCount == 5 &&
                simulation.snapshot().pendingEnemyCount == 10,
            "early wave immediately spawns first enemy group");
    auto phaseEvents = simulation.takeEvents();
    bool directionWarningFound = false;
    bool waveEventFound = false;
    for (const auto& event : phaseEvents) {
        if (event.type == ian::GameEventType::AttackDirectionWarned &&
            event.amount == 1) {
            directionWarningFound = true;
        }
        if (event.type == ian::GameEventType::WaveStarted &&
            event.amount == 1) {
            waveEventFound = true;
        }
    }
    require(directionWarningFound,
            "early wave emits attack-direction warning event");
    require(waveEventFound,
            "early wave emits WaveStarted event");
    require(simulation.snapshot().wave == 1 &&
                simulation.snapshot().activeEnemyCount == 5 &&
                simulation.snapshot().pendingEnemyCount == 10,
            "first wave starts with configured enemy group");
    require(simulation.snapshot().enemies.front().position.z > 20.0,
            "first group enters from outside initial player view");
    require(simulation.snapshot().tutorialObjective ==
                ian::TutorialObjective::SurviveFirstWave,
            "tutorial switches to first-night survival");
    simulation.tick(2.0);
    require(simulation.snapshot().activeEnemyCount == 10 &&
                simulation.snapshot().pendingEnemyCount == 5,
            "next enemy group arrives after configured interval");
    ian::PlayerCommand spawnDebugEnemy;
    spawnDebugEnemy.spawnEnemy = ian::SpawnEnemyCommand{ian::EnemyType::Boss};
    simulation.tick(1.0 / 60.0, spawnDebugEnemy);
    require(simulation.snapshot().activeEnemyCount == 11,
            "debug command spawns selected enemy during wave");
    phaseEvents = simulation.takeEvents();

    const int storedPointsBeforeWaveReward =
        simulation.skillTree().points();
    ian::PlayerCommand defeatWave;
    defeatWave.defeatAllEnemies = ian::DefeatAllEnemiesCommand{};
    simulation.tick(1.0 / 60.0, defeatWave);
    require(simulation.snapshot().state == ian::RunState::WaveComplete,
            "cleared non-final wave enters dawn state");
    require(simulation.snapshot().runUpgradeChoicePending &&
                simulation.snapshot().runUpgradeChoiceCount == 3U,
            "survived night offers three run upgrades");
    require(
        !simulation.snapshot().coinPickups.empty() &&
            simulation.snapshot().coins == 0,
        "defeated enemies drop physical coins before collection");
    require(
        simulation.snapshot().lootChests.size() ==
            chestsBeforeFirstNight + 1U,
        "survived night spawns one skill-granted chest");
    const auto rewardChestCount = std::ranges::count_if(
        simulation.snapshot().lootChests,
        [](const ian::LootChestInstance& chest) {
            return chest.purpose ==
                ian::LootChestPurpose::Reward;
        });
    require(
        rewardChestCount == 1,
        "post-wave chests are classified as nearby rewards");
    const double dawnDuration = simulation.snapshot().phaseDuration;
    requireNear(simulation.snapshot().phaseTimeRemaining, dawnDuration, 1e-12,
                "dawn starts with configured duration");
    require(simulation.snapshot().crystals == simulation.snapshot().waveCompletionReward,
            "wave completion grants crystals reward");
    require(
            simulation.snapshot().skillPoints ==
                std::numeric_limits<int>::max() &&
            simulation.skillTree().points() ==
                storedPointsBeforeWaveReward &&
            simulation.snapshot().currentInsight > 0.0,
        "wave completion fills Insight while god mode stays infinite");
    require(!simulation.snapshot().tutorialObjective,
            "tutorial disappears after first night");
    phaseEvents = simulation.takeEvents();
    int waveCompletedEvents = 0;
    bool rewardEventFound = false;
    for (const auto& event : phaseEvents) {
        if (event.type == ian::GameEventType::WaveCompleted) {
            ++waveCompletedEvents;
        }
        if (event.type == ian::GameEventType::WaveRewardGranted &&
            event.amount == simulation.snapshot().waveCompletionReward) {
            rewardEventFound = true;
        }
    }
    require(waveCompletedEvents == 1,
            "wave completion emits exactly one completion event");
    require(rewardEventFound, "wave completion emits reward event");

    simulation.tick(0.5);
    requireNear(simulation.snapshot().phaseTimeRemaining, dawnDuration, 1e-12,
                "post-night choice pauses the dawn timer");
    const ian::RunUpgradeEffect firstRunUpgrade =
        simulation.snapshot().runUpgradeChoices[0];
    do {
        require(simulation.selectRunUpgrade(0U),
                "player can select every earned post-night run upgrade");
    } while (simulation.snapshot().runUpgradeChoicePending);
    require(!simulation.snapshot().runUpgradeChoicePending &&
                simulation.snapshot().runUpgradeStacks[
                    ian::runUpgradeIndex(firstRunUpgrade)] >= 1,
            "selected run upgrades are applied and close the choice");
    simulation.tick(dawnDuration - 1.0);
    require(simulation.snapshot().state == ian::RunState::WaveComplete,
            "dawn remains active before timer expires");
    simulation.tick(1.0);
    require(simulation.snapshot().state == ian::RunState::BuildPhase,
            "dawn transitions to next build phase");
    requireNear(simulation.snapshot().phaseTimeRemaining,
                simulation.snapshot().phaseDuration, 1e-12,
                "new day receives full preparation timer");

    for (int expectedWave = 2; expectedWave < ian::Simulation::StageClearWave;
         ++expectedWave) {
        simulation.tick(1.0 / 60.0, startWaveEarly);
        require(
            simulation.snapshot().state ==
                    ian::RunState::Wave &&
                simulation.snapshot().wave == expectedWave,
            "infinite cycle starts the next numbered wave");
        const int crystalsBeforeReward =
            simulation.snapshot().crystals;
        simulation.tick(1.0 / 60.0, defeatWave);
        require(
            simulation.snapshot().state ==
                ian::RunState::WaveComplete,
            "every cleared wave returns to dawn without victory");
        require(
            simulation.snapshot().crystals ==
                crystalsBeforeReward + 10 + 5 * expectedWave,
            "endless wave reward follows the balanced base plus wave curve");
        if (expectedWave < ian::Simulation::StageClearWave - 1) {
            do {
                require(simulation.selectRunUpgrade(0U),
                        "each survived night accepts every earned run-upgrade choice");
            } while (simulation.snapshot().runUpgradeChoicePending);
            simulation.tick(
                simulation.snapshot().phaseDuration);
            require(
                simulation.snapshot().state ==
                    ian::RunState::BuildPhase,
                "dawn returns to preparation after every wave");
        }
    }
    require(
        simulation.snapshot().wave == ian::Simulation::StageClearWave - 1 &&
            simulation.snapshot().bestWave ==
                ian::Simulation::StageClearWave - 1,
        "snapshot exposes current wave and best reached record");

    do {
        require(simulation.selectRunUpgrade(0U),
                "last pre-clear night accepts earned run upgrades");
    } while (simulation.snapshot().runUpgradeChoicePending);
    simulation.tick(simulation.snapshot().phaseDuration);
    simulation.tick(1.0 / 60.0, startWaveEarly);
    require(
        simulation.snapshot().state == ian::RunState::Wave &&
            simulation.snapshot().wave == ian::Simulation::StageClearWave,
        "stage boss starts on configured clear wave");
    simulation.tick(1.0 / 60.0, defeatWave);
    require(
        simulation.snapshot().state == ian::RunState::StageClear &&
            simulation.snapshot().stageCleared &&
            !simulation.snapshot().finalNight &&
            !simulation.snapshot().runUpgradeChoicePending,
        "stage boss opens bank-or-final-night choice without dawn upgrade");
    require(simulation.enterFinalNight(),
            "cleared stage can continue into final night");
    require(
        simulation.snapshot().state == ian::RunState::Wave &&
            simulation.snapshot().wave == ian::Simulation::StageClearWave + 1 &&
            simulation.snapshot().finalNight,
        "final night immediately starts next numbered wave");
    simulation.tick(1.0 / 60.0, defeatWave);
    require(
        simulation.snapshot().state == ian::RunState::Wave &&
            simulation.snapshot().wave == ian::Simulation::StageClearWave + 2 &&
            !simulation.snapshot().runUpgradeChoicePending,
        "final night chains waves without dawn or upgrade pauses");
    const std::uint32_t completedRunSeed =
        simulation.snapshot().terrainSeed;
    simulation.restartRun();
    require(
        simulation.snapshot().wave == 0 &&
            simulation.snapshot().bestWave ==
                ian::Simulation::StageClearWave + 2,
        "run restart preserves session wave record");
    require(!simulation.snapshot().unlimitedResources,
            "run restart disables unlimited resources");
    require(!simulation.snapshot().playerInvulnerable,
            "run restart disables player invulnerability");
    require(
        simulation.snapshot().coins == 0 &&
            simulation.snapshot().coinPickups.empty(),
        "run restart removes collected crystals and physical coin drops");
    require(
        simulation.snapshot().terrainSeed != completedRunSeed,
        "run restart regenerates the map with a new seed");
}
