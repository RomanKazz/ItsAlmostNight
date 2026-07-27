#include "TestHarness.hpp"
#include "game/Simulation.hpp"

void runSimulationTests() {
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
        require(firstDistance >= 7.0 && firstDistance <= 12.0,
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

    ian::Simulation simulation;
    require(simulation.snapshot().state == ian::RunState::MainMenu, "simulation starts in menu");

    simulation.startRun();
    simulation.tick(1.0 / 60.0);
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

    ian::PlayerCommand attack;
    attack.usePickaxe = true;
    simulation.tick(1.0 / 60.0, attack);
    require(simulation.snapshot().wood == 0, "first tree hit does not collect node");

    simulation.tick(0.5);
    simulation.tick(1.0 / 60.0, attack);
    simulation.tick(0.5);
    simulation.tick(1.0 / 60.0, attack);
    require(simulation.snapshot().wood == 15, "three tree hits collect wood");

    const auto events = simulation.takeEvents();
    bool collectedEventFound = false;
    for (const auto& event : events) {
        if (event.type == ian::GameEventType::ResourceCollected && event.amount == 15) {
            collectedEventFound = true;
        }
    }
    require(collectedEventFound, "collection emits ResourceCollected event");

    simulation.restartRun();
    ian::PlayerCommand unlimited;
    unlimited.enableUnlimitedResources = ian::EnableUnlimitedResourcesCommand{};
    simulation.tick(1.0 / 60.0, unlimited);
    require(simulation.snapshot().unlimitedResources, "unlimited resource command enables cheat");
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
            "debug command toggles player invulnerability");
    const double coreHealthBeforeDebugDamage = simulation.snapshot().coreHealth;
    ian::PlayerCommand damageCore;
    damageCore.damageCore = ian::DamageCoreCommand{25.0};
    simulation.tick(1.0 / 60.0, damageCore);
    requireNear(simulation.snapshot().coreHealth,
                coreHealthBeforeDebugDamage - 25.0, 1e-12,
                "debug command damages core");

    ian::PlayerCommand placeWall;
    placeWall.placeBuilding =
        ian::PlaceBuildingCommand{ian::BuildingType::Wall, {0, 4}, 0};
    simulation.tick(1.0 / 60.0, placeWall);
    const auto wall = simulation.snapshot().aimedBuilding;
    require(wall.has_value(), "placed wall is selected by building raycast");

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

    ian::PlayerCommand upgradeCore;
    upgradeCore.upgradeBuilding =
        ian::UpgradeBuildingCommand{*simulation.snapshot().coreId};
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
    require(upgradedWall.level == 2 && upgradedWall.maxHealth == 150.0,
            "simulation upgrades aimed building after core unlock");

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
    require(simulation.snapshot().state == ian::RunState::Sunset,
            "early-wave command starts sunset warning");
    const double sunsetDuration = simulation.snapshot().phaseDuration;
    requireNear(simulation.snapshot().phaseTimeRemaining, sunsetDuration, 1e-12,
                "sunset starts with configured duration");
    require(simulation.snapshot().upcomingAttackDirection ==
                ian::AttackDirection::East,
            "sunset warns about least-visible attack direction");
    require(simulation.snapshot().activeEnemyCount == 0,
            "enemies do not spawn before sunset ends");
    auto phaseEvents = simulation.takeEvents();
    bool sunsetEventFound = false;
    bool directionWarningFound = false;
    for (const auto& event : phaseEvents) {
        if (event.type == ian::GameEventType::SunsetStarted && event.amount == 1) {
            sunsetEventFound = true;
        }
        if (event.type == ian::GameEventType::AttackDirectionWarned &&
            event.amount == 1) {
            directionWarningFound = true;
        }
    }
    require(sunsetEventFound, "sunset transition emits warning event");
    require(directionWarningFound, "sunset emits attack-direction warning event");

    simulation.tick(sunsetDuration - 1.0);
    require(simulation.snapshot().state == ian::RunState::Sunset,
            "sunset remains active before timer expires");
    simulation.tick(1.0);
    require(simulation.snapshot().state == ian::RunState::Wave,
            "wave starts when sunset timer expires");
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
    bool waveEventFound = false;
    for (const auto& event : phaseEvents) {
        if (event.type == ian::GameEventType::WaveStarted && event.amount == 1) {
            waveEventFound = true;
        }
    }
    require(waveEventFound, "night transition emits WaveStarted event");

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
}
