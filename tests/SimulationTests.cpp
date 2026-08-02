#include "TestHarness.hpp"
#include "game/Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

void runSimulationTests() {
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
        coreDefinition.gold = 0;
        auto& wallDefinition =
            transactionBalance.buildings[
                static_cast<std::size_t>(
                    ian::BuildingType::Wall)];
        wallDefinition.wood = 4;
        wallDefinition.stone = 0;
        wallDefinition.gold = 0;

        ian::Simulation transactions{transactionBalance};
        transactions.startRun();
        static_cast<void>(transactions.takeEvents());
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        const auto coreSurface =
            transactions.previewPlacementSurface(
                ian::BuildingType::Core, {0, 0});
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

        transactions.tick(1.0 / 60.0, placeCore);
        require(
            transactions.snapshot().coreId.has_value() &&
                transactions.snapshot().wood == 10,
            "successful placement spends configured cost exactly once");
        ian::PlayerCommand placeWall;
        placeWall.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Wall, {0, 4}, 0};
        transactions.tick(1.0 / 60.0, placeWall);
        const auto wall = std::find_if(
            transactions.snapshot().buildings.begin(),
            transactions.snapshot().buildings.end(),
            [](const ian::BuildingInstance& building) {
                return building.type == ian::BuildingType::Wall;
            });
        require(
            wall != transactions.snapshot().buildings.end() &&
                transactions.snapshot().wood == 6,
            "second placement performs one atomic deduction");
        const ian::EntityId wallId = wall->id;
        ian::PlayerCommand sellWall;
        sellWall.sellBuilding =
            ian::SellBuildingCommand{wallId};
        transactions.tick(1.0 / 60.0, sellWall);
        require(
            transactions.snapshot().buildings.size() == 1U &&
                transactions.snapshot().wood == 8,
            "sell removes building and credits configured refund once");
        transactions.tick(1.0 / 60.0, sellWall);
        require(
            transactions.snapshot().wood == 8,
            "repeated stale sell cannot credit a second refund");
    }
    {
        ian::Simulation restartStress;
        restartStress.startRun();
        std::optional<ian::EntityId> previousCoreId;
        std::optional<ian::EntityId> staleCoreId;
        for (int restart = 0; restart < 128; ++restart) {
            if (restart > 0) {
                restartStress.restartRun();
            }
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
        ian::Simulation productionSimulation;
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
                    1 &&
                raisedTurret->platformStorey == 0,
            "raised building creates a real ground platform");
        require(
            raisedSnapshot.playerPosition.y >
                playerHeightBefore + 0.5,
            "ground platform placed under player lifts player onto floor");

        const ian::EntityId turretId =
            raisedTurret->id;
        const ian::EntityId foundationId =
            raisedSnapshot.platformFrames.front().id;
        ian::PlayerCommand sellTurret;
        sellTurret.sellBuilding =
            ian::SellBuildingCommand{turretId};
        foundationLifecycle.tick(
            1.0 / 60.0, sellTurret);
        require(
            foundationLifecycle.snapshot()
                    .platformFrames.size() == 1 &&
                foundationLifecycle.snapshot()
                    .buildings.size() == 1,
            "selling building leaves its automatic platform");

        const auto platform =
            foundationLifecycle.snapshot()
                .platformFrames.front();
        ian::PlayerCommand replaceTurret;
        replaceTurret.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Turret,
                .gridPosition = turretPosition,
                .rotation = 0,
                .baseHeight = platform.floorHeight,
                .platformStorey = platform.storey,
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
                    .platformFrames.empty() &&
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
                .position = {1.0, 1.0, 1.5},
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
        require(
            raisedRampResources
                    .previewRamp(
                        supportHit,
                        ian::Rotation::Deg180)
                    .error ==
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
        require(
            raisedRampResources
                .previewRamp(
                    supportHit,
                    ian::Rotation::Deg180)
                .valid(),
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
    }

    auto simulationBalance = ian::GameBalance::defaults();
    simulationBalance.gameplay.pickaxeDamageVariation = 0.0;
    simulationBalance.gameplay.pickaxeCriticalChance = 0.0;
    simulationBalance.gameplay.playerRespawnSeconds = 1.0;
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
                ian::TutorialObjective::MineWood,
            "tutorial starts with wood objective");

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

    simulation.restartRun();
    require(simulation.snapshot().state == ian::RunState::Gathering, "restart enters gathering");
    require(simulation.snapshot().tick == 0, "restart resets tick counter");
    require(simulation.snapshot().playerHealth == simulation.snapshot().playerMaxHealth,
            "restart restores player health");
    const auto restartEvents = simulation.takeEvents();
    require(
        restartEvents.size() == 1U &&
            restartEvents.front().type ==
                ian::GameEventType::RunRestarted,
        "restart discards stale events from previous run");

    ian::PlayerCommand attack;
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
            event.amount == 5) {
            collectedEventFound = true;
        }
    }
    require(gatheredWood == 15,
            "resource hit events distribute exact tree capacity");
    require(grantedWood == 15,
            "delayed grant events deliver exact tree capacity");
    require(collectedEventFound, "collection emits ResourceCollected event");

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
                ian::TutorialObjective::BuildGoldMine,
            "tutorial requests first gold mine after core");
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

    ian::PlayerCommand placeWall;
    placeWall.placeBuilding =
        ian::PlaceBuildingCommand{ian::BuildingType::Wall, {0, 4}, 0};
    simulation.tick(1.0 / 60.0, placeWall);
    const auto wallSnapshot = simulation.snapshot();
    const auto wallBuilding = std::find_if(
        wallSnapshot.buildings.begin(),
        wallSnapshot.buildings.end(),
        [](const ian::BuildingInstance& building) {
            return building.type == ian::BuildingType::Wall &&
                   building.gridPosition ==
                       ian::GridPosition{0, 4};
        });
    require(
        wallBuilding != wallSnapshot.buildings.end(),
        "placed wall exists in simulation");
    const std::optional<ian::EntityId> wall =
        wallBuilding->id;

    ian::PlayerCommand lockedWeaponUpgrade;
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

    ian::PlayerCommand repairFullWall;
    repairFullWall.repairBuilding = ian::RepairBuildingCommand{*wall};
    simulation.tick(1.0 / 60.0, repairFullWall);
    auto buildingEvents = simulation.takeEvents();
    bool fullRepairRejected = false;
    for (const auto& event : buildingEvents) {
        if (event.type == ian::GameEventType::BuildingRepairRejected &&
            event.buildingActionError == ian::BuildingActionError::FullHealth) {
            fullRepairRejected = true;
        }
    }
    require(fullRepairRejected, "repair command rejects full-health building");

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
    require(simulation.snapshot().upcomingAttackDirection ==
                ian::AttackDirection::East,
            "early wave uses least-visible attack direction");
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
    require(simulation.snapshot().enemies.front().position.x > 15.0,
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

    ian::PlayerCommand defeatWave;
    defeatWave.defeatAllEnemies = ian::DefeatAllEnemiesCommand{};
    simulation.tick(1.0 / 60.0, defeatWave);
    require(simulation.snapshot().state == ian::RunState::WaveComplete,
            "cleared non-final wave enters dawn state");
    const double dawnDuration = simulation.snapshot().phaseDuration;
    requireNear(simulation.snapshot().phaseTimeRemaining, dawnDuration, 1e-12,
                "dawn starts with configured duration");
    require(simulation.snapshot().gold == simulation.snapshot().waveCompletionReward,
            "wave completion grants gold reward");
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

    simulation.tick(dawnDuration - 1.0);
    require(simulation.snapshot().state == ian::RunState::WaveComplete,
            "dawn remains active before timer expires");
    simulation.tick(1.0);
    require(simulation.snapshot().state == ian::RunState::BuildPhase,
            "dawn transitions to next build phase");
    requireNear(simulation.snapshot().phaseTimeRemaining,
                simulation.snapshot().phaseDuration, 1e-12,
                "new day receives full preparation timer");

    for (int expectedWave = 2; expectedWave <= 7;
         ++expectedWave) {
        simulation.tick(1.0 / 60.0, startWaveEarly);
        require(
            simulation.snapshot().state ==
                    ian::RunState::Wave &&
                simulation.snapshot().wave == expectedWave,
            "infinite cycle starts the next numbered wave");
        const int goldBeforeReward =
            simulation.snapshot().gold;
        simulation.tick(1.0 / 60.0, defeatWave);
        require(
            simulation.snapshot().state ==
                ian::RunState::WaveComplete,
            "every cleared wave returns to dawn without victory");
        require(
            simulation.snapshot().gold ==
                goldBeforeReward + 15 * expectedWave,
            "endless wave reward remains fifteen times wave number");
        if (expectedWave < 7) {
            simulation.tick(
                simulation.snapshot().phaseDuration);
            require(
                simulation.snapshot().state ==
                    ian::RunState::BuildPhase,
                "dawn returns to preparation after every wave");
        }
    }
    require(
        simulation.snapshot().wave == 7 &&
            simulation.snapshot().bestWave == 7,
        "snapshot exposes current wave and best reached record");
    simulation.restartRun();
    require(
        simulation.snapshot().wave == 0 &&
            simulation.snapshot().bestWave == 7,
        "run restart preserves session wave record");
    require(!simulation.snapshot().unlimitedResources,
            "run restart disables unlimited resources");
    require(!simulation.snapshot().playerInvulnerable,
            "run restart disables player invulnerability");
}
