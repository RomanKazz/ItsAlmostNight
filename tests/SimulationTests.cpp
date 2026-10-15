#include "OvertimeChecks.hpp"
#include "TestHarness.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/CoreProgression.hpp"
#include "buildings/PlacementValidator.hpp"
#include "game/Simulation.hpp"
#include "progression/RunUpgrade.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <ranges>

namespace {
void resolveLevelUps(ian::Simulation& simulation) {
    while (simulation.snapshot().runUpgradeChoicePending) {
        std::size_t choice = 0U;
        const auto& offer = simulation.snapshot();
        for (std::size_t index = 0; index < offer.runUpgradeChoiceCount; ++index) {
            if (!ian::runUpgradeBuilding(offer.runUpgradeChoices[index])) { choice = index; break; }
        }
        require(simulation.selectRunUpgrade(choice),
                "test fixture resolves queued level-up cards");
    }
    if (simulation.snapshot().runShop.pending)
        require(simulation.closeRunShop(), "wave fixture skips optional purchases before continuing");
}
}

void runSimulationTests() {
    checkOvertime();
    {
        auto map=ian::MapDefinition::defaults();map.obstacles.clear();
        map.playerSpawn={0,0,0};
        map.resources={
            {ian::ResourceType::Barrel,{10,0,0},.68,1000,0,75},
            {ian::ResourceType::Crate,{20,0,0},.68,1000,0,75},
            {ian::ResourceType::ItemCrate,{30,0,0},.68,1000,0,75},
            {ian::ResourceType::Wood,{40,0,0},.68,1000,10,75}};
        auto world=ian::WorldConfig::defaults();world.terrainAmplitude=0;
        world.pondMinimumCount=world.pondMaximumCount=0;
        auto run=std::make_unique<ian::Simulation>(ian::GameBalance::defaults(),map,world);
        run->startRun(ian::PlayerClass::Vanguard);
        require(run->grantDebugRunUpgrade(ian::RunUpgradeEffect::UnlockRifle),"container fixture unlocks rifle");
        for(std::size_t i=0;i<4;++i) {
            auto saved=run->saveSuspendedRunState();
            const auto node=saved.resourceNodes[i];
            saved.playerPosition={node.position.x,node.position.y+1.7,node.position.z+1.8};
            run->returnToMainMenu();
            require(run->loadSuspendedRunState(saved),"container interaction fixture restores nearby player");
            ian::PlayerCommand open;open.interact=ian::InteractCommand{};open.overrideAimedResource=true;open.aimedResourceOverride=node.id;
            open.selectWeapon=ian::SelectWeaponCommand{ian::PlayerWeapon::Rifle};
            const auto countOpened=[](const auto& events){return std::ranges::count_if(events,[](const auto& event){
                return event.type==ian::GameEventType::ResourceCollected;
            });};
            run->takeEvents(); run->tick(0,open);
            const auto events=run->takeEvents();
            require(countOpened(events)==(i<3?1:0) && run->snapshot().resourceNodes[i].active==(i==3),
                "E opens each container while holding a rifle but never opens trees");
            if(i<3) {
                const auto loot=run->snapshot().lootChests.size();
                const auto pickups=run->snapshot().coinPickups.size();
                require((loot>saved.lootChests.size() || pickups>0) &&
                    run->snapshot().selectedWeapon==ian::PlayerWeapon::Rifle,
                    "rifle-equipped interaction creates a fresh container reward");
                run->tick(0,open);
                const auto repeated=run->takeEvents();
                require(countOpened(repeated)==0 && run->snapshot().lootChests.size()==loot && run->snapshot().coinPickups.size()==pickups,
                    "repeated E does not duplicate rewards");
                const auto opened=run->saveSuspendedRunState();
                run->returnToMainMenu();
                require(run->loadSuspendedRunState(opened) && !run->snapshot().resourceNodes[i].active,
                    "opened state survives save/load");
            }
        }
        run->restartRun();
        ian::PlayerCommand remote;remote.interact=ian::InteractCommand{};remote.overrideAimedResource=true;
        remote.aimedResourceOverride=run->snapshot().resourceNodes[0].id;
        run->tick(0,remote);
        require(run->snapshot().resourceNodes[0].active,"a remote ID override cannot open a container");
    }
    {
        auto map = ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        auto world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0;
        world.pondMinimumCount = world.pondMaximumCount = 0;
        world.buildPreviewDistance = 100;
        auto balance = ian::GameBalance::defaults();
        balance.gameplay.maximumPlacementDistance = 100;
        auto run = std::make_unique<ian::Simulation>(balance, map, world);
        run->startSandboxRun();
        ian::PlayerCommand place;
        place.placeBuilding = ian::PlaceBuildingCommand{ian::BuildingType::Core, {0, 0}, 0};
        run->tick(0, place);
        resolveLevelUps(*run);
        ian::PlayerCommand upgrade;
        upgrade.upgradeBuilding = ian::UpgradeBuildingCommand{*run->snapshot().coreId};
        run->tick(0, upgrade);
        run->tick(0, upgrade);
        resolveLevelUps(*run);
        ian::PlayerCommand start;
        start.startWaveEarly = ian::StartWaveEarlyCommand{};
        run->tick(0, start);
        ian::PlayerCommand spawn;
        spawn.spawnEnemy = ian::SpawnEnemyCommand{.type = ian::EnemyType::Basic, .count = 24};
        run->tick(0, spawn);
        const auto target = std::ranges::find_if(run->snapshot().enemies, [](const auto& enemy) {
            return enemy.active && std::hypot(
                enemy.position.x - std::floor(enemy.position.x) - .5,
                enemy.position.z - std::floor(enemy.position.z) - .5) < .35;
        });
        require(target != run->snapshot().enemies.end(), "dawn trap fixture finds an enemy near a cell center");
        const ian::GridPosition cell{static_cast<int>(std::floor(target->position.x)),
                                     static_cast<int>(std::floor(target->position.z))};
        place.placeBuilding = ian::PlaceBuildingCommand{ian::BuildingType::SpikeTrap, cell, 0};
        run->tick(0, place);
        require(run->snapshot().traps.size() == 1 &&
                run->snapshot().traps.front().activationRemaining > 1.1,
            "dawn trap fixture starts an active spike cycle");
        resolveLevelUps(*run);
        ian::PlayerCommand clear;
        clear.defeatAllEnemies = ian::DefeatAllEnemiesCommand{};
        run->tick(0, clear);
        resolveLevelUps(*run);
        require(run->snapshot().state == ian::RunState::WaveComplete &&
                run->snapshot().activeEnemyCount == 0,
            "dawn trap fixture finishes combat during the active spike cycle");
        const double remaining = run->snapshot().traps.front().activationRemaining;
        run->tick(.4);
        requireNear(run->snapshot().traps.front().activationRemaining,
            remaining - .4, 1e-9, "spike animation continues after the last enemy and dawn");
        run->tick(1.2);
        requireNear(run->snapshot().traps.front().activationRemaining, 0, 1e-9,
            "spikes finish retracting during daytime without another activation");
        requireNear(run->snapshot().traps.front().cooldownRemaining, 0, 1e-9,
            "trap cooldown also finishes outside combat");
    }
    {
        auto map = ian::MapDefinition::defaults();
        map.resources.clear(); map.obstacles.clear();
        auto world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0;
        world.pondMinimumCount = world.pondMaximumCount = 0;
        ian::Simulation simulation{ian::GameBalance::defaults(), map, world};
        simulation.startSandboxRun();
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::Core, .gridPosition = {0, 0},
        };
        simulation.tick(1.0/60.0, placeCore);
        resolveLevelUps(simulation);
        const auto existing = simulation.placeFoundation({.2, simulation.terrain().getHeight(.2, 4.2), 4.2});
        require(existing.has_value(), "trap foundation fixture creates a neighbouring platform");
        const ian::GridPosition trapCell{existing->anchor.x - 1, existing->anchor.z};
        const auto ghost = simulation.previewBuildingFoundation(ian::BuildingType::SpikeTrap,
            trapCell, existing->floorHeight);
        require(ghost && ghost->valid(), "a compact trap can select an alternative 2x2 foundation at a platform edge");
        require(ghost->anchor.x == existing->anchor.x - 2 && ghost->anchor.z == existing->anchor.z,
            "trap support chooses a nonoverlapping anchor instead of covering the neighbouring platform");
        requireNear(ghost->supports[3].top.x - ghost->supports[0].top.x, 2, 1e-9,
            "actual foundation preview stays 2x2 for a 1x1 trap");
        ian::PlayerCommand place;
        place.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::SpikeTrap, .gridPosition = trapCell, .rotation = 0,
            .baseHeight = ghost->floorHeight, .platformStorey = -1, .lockHeight = true,
        };
        simulation.tick(1.0/60.0, place);
        const auto built = std::ranges::find_if(simulation.snapshot().platformFrames,
            [&](const auto& frame) { return frame.anchor == ghost->anchor; });
        require(built != simulation.snapshot().platformFrames.end(),
            "placement creates the same 2x2 platform shown in the trap ghost");
        for (std::size_t i = 0; i < ghost->supports.size(); ++i) {
            requireNear(built->supports[i].top.x, ghost->supports[i].top.x, 1e-9, "placed support matches preview X");
            requireNear(built->supports[i].top.z, ghost->supports[i].top.z, 1e-9, "placed support matches preview Z");
        }
        require(!simulation.previewBuildingFoundation(ian::BuildingType::SpikeTrap, trapCell),
            "a real platform is reused rather than previewing a second foundation");
    }
    {
        auto map = ian::MapDefinition::defaults();
        map.resources.clear(); map.obstacles.clear();
        auto world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0;
        world.pondMinimumCount = world.pondMaximumCount = 0;
        const auto balance = ian::GameBalance::defaults();
        auto simulation = std::make_unique<ian::Simulation>(balance, map, world);
        simulation->startSandboxRun(ian::PlayerClass::Vanguard);
        ian::PlayerCommand core;
        core.placeBuilding = ian::PlaceBuildingCommand{.type = ian::BuildingType::Core, .gridPosition = {0, 0}};
        simulation->tick(1.0/60.0, core);
        resolveLevelUps(*simulation);
        const auto platform = simulation->placeFoundation({.2, 0, 4.2});
        require(platform.has_value(), "drag fixture creates its starting platform");
        auto saved = simulation->saveSuspendedRunState();
        const std::array cells{ian::GridPosition{1, 4}, ian::GridPosition{2, 4},
            ian::GridPosition{3, 4}, ian::GridPosition{4, 4}};
        const auto previews = simulation->previewBuildingLine(ian::BuildingType::SpikeTrap, cells, 0, platform->floorHeight);
        require(std::ranges::all_of(previews, [](const auto& p) { return p.placement.valid(); }),
            "dragging off an existing platform keeps supported cells and new foundations valid");
        require(previews[0].platformStorey == platform->storey && previews[1].platformStorey < 0 &&
                previews[2].platformStorey == platform->storey && !previews[2].foundationPreview,
            "drag assigns each cell its own support and reuses the preceding 2x2 platform");
        require(simulation->snapshot().platformFrames.size() == saved.platformFrames.size() &&
                simulation->snapshot().buildings.size() == saved.buildings.size(),
            "drag planning never modifies live construction state");
        const auto commands = [](const std::vector<ian::BuildingPreview>& line) {
            ian::PlayerCommand command;
            for (const auto& p : line) if (p.placement.valid()) command.placeBuildingBatch.push_back({
                .type = p.type, .gridPosition = p.gridPosition, .rotation = p.rotation,
                .baseHeight = p.baseHeight, .platformStorey = p.platformStorey, .lockHeight = true,
                .foundationAnchor = p.foundationPreview ? std::optional<ian::GridCoord>{p.foundationPreview->anchor} : std::nullopt});
            return command;
        };
        simulation->tick(1.0/60.0, commands(previews));
        require(std::ranges::count(simulation->snapshot().buildings, ian::BuildingType::SpikeTrap,
                    &ian::BuildingInstance::type) == 4,
            "all green trap cells commit in one tick across an existing platform edge");
        require(simulation->snapshot().platformFrames.size() == saved.platformFrames.size() + 2,
            "four trap cells need only the two new platforms shown in the staged preview");
        for (const auto& event : simulation->takeEvents()) require(event.type != ian::GameEventType::BuildingRejected,
            "green drag cells do not emit a failed placement event");

        saved.runUpgradeStacks[ian::runUpgradeIndex(*ian::buildingRunUpgrade(ian::BuildingType::SpikeTrap))] = 1;
        saved.runUpgradePower[ian::runUpgradeIndex(*ian::buildingRunUpgrade(ian::BuildingType::SpikeTrap))] = 1;
        saved.wood = saved.stone = saved.crystals = 200;
        auto limited = std::make_unique<ian::Simulation>(balance, map, world);
        require(limited->loadSuspendedRunState(saved), "drag limit fixture restores a regular run");
        const auto limitLine = limited->previewBuildingLine(ian::BuildingType::SpikeTrap, cells, 0, platform->floorHeight);
        require(limitLine[0].placement.valid() && limitLine[1].placement.valid() && limitLine[2].placement.valid() &&
                limitLine[3].placement.error == ian::PlacementError::LimitReached,
            "drag preview reserves defense slots and marks the cell exceeding the Core limit");
        limited->tick(1.0/60.0, commands(limitLine));
        require(std::ranges::count(limited->snapshot().buildings, ian::BuildingType::SpikeTrap,
                    &ian::BuildingInstance::type) == 3,
            "the affordable supported prefix commits up to its previewed defense limit");

        const std::array raisedCells{ian::GridPosition{2, 4}, ian::GridPosition{3, 4}, ian::GridPosition{4, 4}};
        const double raisedHeight = platform->floorHeight + 1;
        // Derive the exact combined budget from an empty, unlimited staging fixture.
        auto reference = std::make_unique<ian::Simulation>(balance, map, world);
        require(reference->loadSuspendedRunState(saved), "drag budget fixture restores construction");
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources = ian::EnableUnlimitedResourcesCommand{};
        reference->tick(1.0/60.0, unlimited);
        const auto costLine = reference->previewBuildingLine(ian::BuildingType::SpikeTrap, raisedCells, 0, raisedHeight);
        require(costLine[0].placement.valid() && costLine[1].placement.valid() && costLine[0].foundationPreview &&
                !costLine[1].foundationPreview && costLine[0].placement.cost.wood > costLine[1].placement.cost.wood,
            "an elevated drag charges a new platform once and reuses it for the next trap");
        const std::array duplicateCells{ian::GridPosition{1, 4}, ian::GridPosition{1, 4}, ian::GridPosition{2, 4}};
        const auto duplicateLine = reference->previewBuildingLine(ian::BuildingType::SpikeTrap,
            duplicateCells, 0, platform->floorHeight);
        require(duplicateLine[0].placement.valid() && duplicateLine[1].placement.error == ian::PlacementError::Occupied &&
                duplicateLine[2].placement.valid(),
            "rejected cells reserve neither a building slot nor resources for the rest of the drag");
        const std::array turretCells{ian::GridPosition{1, 5}, ian::GridPosition{3, 5}, ian::GridPosition{5, 5}};
        const auto turretLine = reference->previewBuildingLine(ian::BuildingType::GunTurret,
            turretCells, 0, platform->floorHeight);
        require(std::ranges::all_of(turretLine, [](const auto& p) { return p.placement.valid(); }),
            "2x2 defense drag validates existing and newly created supports");
        reference->tick(1.0/60.0, commands(turretLine));
        require(std::ranges::count(reference->snapshot().buildings, ian::BuildingType::GunTurret,
                    &ian::BuildingInstance::type) == 3,
            "all green 2x2 defenses commit on their individual platforms");
        auto blockedSaved = saved;
        blockedSaved.coreBuildRadius = map.coreBuildRadius + 8;
        blockedSaved.resourceNodes = {{
            .id = {0, 1}, .type = ian::ResourceType::Stone, .position = {1.75, 0, 6.75},
            .radius = .2, .groundOffset = 0, .health = 20, .maxHealth = 20,
            .yield = 10, .yieldRemaining = 10, .respawnSeconds = 30, .respawnRemaining = 0,
            .active = true,
        }};
        auto blockedMap = map;
        blockedMap.resources = {{.type = ian::ResourceType::Stone, .position = {1.75, 0, 6.75},
            .radius = .2, .health = 20, .yield = 10, .respawnSeconds = 30}};
        auto aligned = std::make_unique<ian::Simulation>(balance, blockedMap, world);
        require(aligned->loadSuspendedRunState(blockedSaved), "aligned wall drag fixture restores a stone near one side");
        aligned->tick(1.0/60.0, unlimited);
        const std::array wallCells{ian::GridPosition{0, 5}, ian::GridPosition{0, 6},
            ian::GridPosition{0, 7}, ian::GridPosition{0, 8}, ian::GridPosition{0, 9}};
        const auto isolatedSupport = aligned->previewBuildingFoundation(ian::BuildingType::Wall,
            wallCells[1], platform->floorHeight);
        require(isolatedSupport && isolatedSupport->valid() && isolatedSupport->anchor.x == -1,
            "a single compact building can choose a valid alternative support containing its actual cell");
        const auto wallLine = aligned->previewBuildingLine(ian::BuildingType::Wall, wallCells, 0, platform->floorHeight);
        require(std::ranges::all_of(wallLine, [](const auto& p) { return p.placement.valid(); }),
            "wall drag shifts its support row away from a resource blocking the original lattice");
        require(wallLine[1].foundationPreview && wallLine[1].foundationPreview->anchor.x == -1 &&
                wallLine[3].foundationPreview && wallLine[3].foundationPreview->anchor.x == -1 &&
                !wallLine[2].foundationPreview && !wallLine[4].foundationPreview,
            "the alternative wall plan stays straight and shares its 2x2 foundations");
        aligned->tick(1.0/60.0, commands(wallLine));
        require(std::ranges::count(aligned->snapshot().buildings, ian::BuildingType::Wall,
                    &ian::BuildingInstance::type) == 5,
            "the complete shifted wall drag commits with the previewed support anchors");
        for (const bool alongX : {false, true}) {
            for (const bool reverse : {false, true}) {
                auto gap = std::make_unique<ian::Simulation>(balance, map, world);
                gap->startSandboxRun(ian::PlayerClass::Vanguard);
                gap->tick(1.0/60.0, core);
                resolveLevelUps(*gap);
                const int start = reverse ? 6 : 4;
                const int neighbor = reverse ? 4 : 6;
                const auto support = gap->placeFoundation(alongX
                    ? ian::Vec3{start + .2, 0, .2} : ian::Vec3{.2, 0, start + .2});
                require(support.has_value(), "gap fixture places the starting platform");
                const auto adjacent = gap->placeFoundationAtHeight(alongX
                    ? ian::Vec3{neighbor + .2, 0, 1.2} : ian::Vec3{1.2, 0, neighbor + .2},
                    support->floorHeight);
                require(adjacent.has_value(), "gap fixture places an offset neighboring platform");
                const int first = reverse ? 6 : 5;
                const int second = reverse ? 5 : 6;
                const std::array gapCells{alongX ? ian::GridPosition{first, 0} : ian::GridPosition{0, first},
                                         alongX ? ian::GridPosition{second, 0} : ian::GridPosition{0, second}};
                const auto line = gap->previewBuildingLine(ian::BuildingType::Wall, gapCells, 0, support->floorHeight);
                require(line.size() == 2 && line[0].placement.valid() && line[1].placement.valid(),
                    "two fences fit against offset pallets in both axes and drag directions");
                require(line[1].foundationPreview &&
                        (alongX ? line[1].foundationPreview->anchor.z : line[1].foundationPreview->anchor.x) == -1,
                    "the new platform moves one cell sideways to avoid an unfillable gap");
                require(gap->snapshot().platformFrames.size() == 3,
                    "alternative support planning never installs preview platforms in the live world");
                gap->tick(1.0/60.0, commands(line));
                require(std::ranges::count(gap->snapshot().buildings, ian::BuildingType::Wall,
                            &ian::BuildingInstance::type) == 2 && gap->snapshot().platformFrames.size() == 4,
                    "both previewed fences commit on exactly one new shifted platform");
                for (const auto& event : gap->takeEvents()) require(event.type != ian::GameEventType::BuildingRejected,
                    "shifted fence previews do not fail when committed");
            }
        }
        saved.wood = costLine[0].placement.cost.wood + costLine[1].placement.cost.wood;
        saved.stone = costLine[0].placement.cost.stone + costLine[1].placement.cost.stone;
        saved.crystals = costLine[0].placement.cost.crystals + costLine[1].placement.cost.crystals;
        auto budgeted = std::make_unique<ian::Simulation>(balance, map, world);
        require(budgeted->loadSuspendedRunState(saved), "drag budget fixture restores exact resources");
        const auto budgetLine = budgeted->previewBuildingLine(ian::BuildingType::SpikeTrap, raisedCells, 0, raisedHeight);
        require(budgetLine[0].placement.valid() && budgetLine[1].placement.valid() &&
                budgetLine[2].placement.error == ian::PlacementError::InsufficientResources,
            "drag marks unaffordable cells before release including the cost of their foundations");
        budgeted->tick(1.0/60.0, commands(budgetLine));
        require(std::ranges::count(budgeted->snapshot().buildings, ian::BuildingType::SpikeTrap,
                    &ian::BuildingInstance::type) == 2 && budgeted->snapshot().wood == 0 &&
                budgeted->snapshot().stone == 0 && budgeted->snapshot().crystals == 0,
            "committing a supported drag uses exactly the resources promised by its preview");
    }

    {
        auto map = ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        auto world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0;
        world.pondMinimumCount = world.pondMaximumCount = 0;
        auto balance = ian::GameBalance::defaults();
        balance.economy.coreUpgradeCrystals = {2, 3, 4, 5, 6, 7, 8};
        balance.buildings[static_cast<std::size_t>(ian::BuildingType::Wall)].stone = 10;
        auto fixture = std::make_unique<ian::Simulation>(balance, map, world);
        fixture->startSandboxRun(ian::PlayerClass::Vanguard);
        ian::PlayerCommand place;
        place.placeBuilding = ian::PlaceBuildingCommand{ian::BuildingType::Core, {0, 0}, 0};
        fixture->tick(0, place);
        resolveLevelUps(*fixture);
        place.placeBuilding = ian::PlaceBuildingCommand{ian::BuildingType::Wall, {3, 0}, 0};
        fixture->tick(0, place);
        auto saved = fixture->saveSuspendedRunState();
        require(saved.buildings.size() == 2, "bulk upgrade fixture places core and wall");
        saved.wood = 100;
        saved.stone = 60;
        saved.crystals = 9;
        const auto coreId = *fixture->snapshot().coreId;
        const auto wallId = std::ranges::find(saved.buildings, ian::BuildingType::Wall,
            &ian::BuildingInstance::type)->id;
        auto run = std::make_unique<ian::Simulation>(balance, map, world);
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(saved), "bulk upgrade fixture resumes with finite resources");
        ian::PlayerCommand upgrade;
        upgrade.upgradeBuilding = ian::UpgradeBuildingCommand{coreId};
        run->tick(0, upgrade);
        require(run->snapshot().coreLevel == 2 && run->snapshot().crystals == 7,
            "ordinary upgrade still purchases exactly one level");
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(saved), "bulk upgrade resets the core fixture");
        run->takeEvents();
        upgrade.upgradeBuilding = ian::UpgradeBuildingCommand{coreId, true};
        run->tick(0, upgrade);
        require(run->snapshot().coreLevel == 4 && run->snapshot().crystals == 0 &&
                run->snapshot().wood == 100 && run->snapshot().stone == 60,
            "bulk core upgrade pays each level and stops at available crystals");
        const auto events = run->takeEvents();
        require(std::ranges::count(events, ian::GameEventType::BuildingUpgraded,
                    &ian::GameEvent::type) == 1 &&
                std::ranges::count(events, ian::GameEventType::BuildingUpgradeRejected,
                    &ian::GameEvent::type) == 0,
            "successful bulk upgrade emits one effect without a final rejection");
        auto coreFour = run->saveSuspendedRunState();
        coreFour.stone = 15;
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(coreFour), "bulk wall fixture limits stone");
        upgrade.upgradeBuilding = ian::UpgradeBuildingCommand{wallId, true};
        run->tick(0, upgrade);
        const auto limitedWall = std::ranges::find(run->snapshot().buildings, wallId,
            &ian::BuildingInstance::id);
        require(limitedWall->level == 3 && run->snapshot().wood == 85 && run->snapshot().stone == 0,
            "bulk wall upgrade stops at insufficient stone without overspending wood");
        coreFour.stone = 100;
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(coreFour), "bulk wall fixture restores resources");
        run->tick(0, upgrade);
        const auto cappedWall = std::ranges::find(run->snapshot().buildings, wallId,
            &ian::BuildingInstance::id);
        require(cappedWall->level == 4 && run->snapshot().wood == 65 && run->snapshot().stone == 65,
            "bulk wall upgrade respects the core level despite remaining resources");
        run->takeEvents();
        run->tick(0, upgrade);
        const auto blockedEvents = run->takeEvents();
        require(std::ranges::any_of(blockedEvents, [](const auto& event) {
            return event.type == ian::GameEventType::BuildingUpgradeRejected &&
                event.upgradeError == ian::UpgradeError::CoreLevelRequired;
        }), "bulk upgrade reports a restriction when no level can be purchased");
        saved.crystals = 35;
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(saved), "bulk maximum-level fixture restores the core");
        upgrade.upgradeBuilding = ian::UpgradeBuildingCommand{coreId, true};
        run->tick(0, upgrade);
        require(run->snapshot().coreLevel == ian::MaxBuildingLevel && run->snapshot().crystals == 0,
            "bulk upgrade reaches level eight with the exact total cost");
        run->returnToMainMenu();
        require(run->loadSuspendedRunState(saved), "bulk unlimited fixture resets the core");
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources = ian::EnableUnlimitedResourcesCommand{};
        run->tick(0, unlimited);
        run->tick(0, upgrade);
        require(run->snapshot().coreLevel == ian::MaxBuildingLevel,
            "unlimited resources bulk upgrade terminates at maximum level");
    }
    {
        auto map = ian::MapDefinition::defaults();
        map.resources.clear(); map.obstacles.clear();
        auto world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        world.pondMinimumCount = world.pondMaximumCount = 0;
        const auto balance = ian::GameBalance::defaults();
        auto walking = std::make_unique<ian::Simulation>(balance, map, world);
        auto shiftHeld = std::make_unique<ian::Simulation>(balance, map, world);
        walking->startRunFromSeed(ian::PlayerClass::Vanguard, 12345);
        shiftHeld->startRunFromSeed(ian::PlayerClass::Vanguard, 12345);
        ian::PlayerCommand forward; forward.moveForward = 1;
        auto withShift = forward; withShift.sprint = true;
        for (int frame = 0; frame < 60; ++frame) {
            walking->tick(1.0 / 60.0, forward);
            shiftHeld->tick(1.0 / 60.0, withShift);
        }
        requireNear(std::hypot(walking->snapshot().playerHorizontalVelocity.x,
                walking->snapshot().playerHorizontalVelocity.z), 6.5, 1e-9,
            "ordinary movement reaches the new six and a half meter per second base speed");
        requireNear(walking->snapshot().playerPosition.z, shiftHeld->snapshot().playerPosition.z, 1e-12,
            "holding former sprint input cannot increase movement speed");
        require(walking->grantDebugRunUpgrade(ian::RunUpgradeEffect::MoveSpeed),
            "movement tome remains available without sprint");
        for (int frame = 0; frame < 30; ++frame) walking->tick(1.0 / 60.0, forward);
        requireNear(std::hypot(walking->snapshot().playerHorizontalVelocity.x,
                walking->snapshot().playerHorizontalVelocity.z), 6.5 * 1.07, 1e-9,
            "movement tome scales the faster base speed");
        ian::PlayerCommand dash = forward; dash.dash = true;
        walking->tick(1.0 / 60.0, dash);
        require(walking->snapshot().dashing &&
            std::hypot(walking->snapshot().playerHorizontalVelocity.x,
                walking->snapshot().playerHorizontalVelocity.z) > 15.0,
            "dash retains its separate burst speed");
        ian::PlayerCommand diagonal; diagonal.moveForward = diagonal.moveRight = 1;
        for (int frame = 0; frame < 60; ++frame) shiftHeld->tick(1.0 / 60.0, diagonal);
        requireNear(std::hypot(shiftHeld->snapshot().playerHorizontalVelocity.x,
                shiftHeld->snapshot().playerHorizontalVelocity.z), 6.5, 1e-9,
            "diagonal input cannot exceed the constant movement speed");
    }
    {
        ian::MapDefinition map = ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        ian::Simulation sandbox{
            ian::GameBalance::defaults(), map, world};
        sandbox.startSandboxRun();
        ian::PlayerCommand tap;
        tap.jump = true;
        tap.jumpHeld = true;
        sandbox.tick(1.0 / 60.0, tap);
        sandbox.tick(1.0 / 60.0);
        sandbox.tick(1.0 / 60.0, tap);
        const double flightStartHeight =
            sandbox.snapshot().playerPosition.y;
        ian::PlayerCommand ascend;
        ascend.jumpHeld = true;
        for (int frame = 0; frame < 60; ++frame) {
            sandbox.tick(1.0 / 60.0, ascend);
        }
        require(
            sandbox.snapshot().playerPosition.y >
                flightStartHeight + 6.0,
            "double jump enables sustained sandbox flight");
        const double raisedHeight =
            sandbox.snapshot().playerPosition.y;
        for (int frame = 0; frame < 30; ++frame) {
            sandbox.tick(1.0 / 60.0);
        }
        requireNear(
            sandbox.snapshot().playerPosition.y,
            raisedHeight, 1e-6,
            "sandbox flight hovers without vertical input");
        ian::PlayerCommand descend;
        descend.sprint = true;
        for (int frame = 0; frame < 15; ++frame) {
            sandbox.tick(1.0 / 60.0, descend);
        }
        require(
            sandbox.snapshot().playerPosition.y <
                raisedHeight - 1.0,
            "legacy Shift binding still descends during sandbox flight");
        sandbox.tick(1.0 / 60.0, tap);
        sandbox.tick(1.0 / 60.0);
        sandbox.tick(1.0 / 60.0, tap);
        const double fallStartHeight =
            sandbox.snapshot().playerPosition.y;
        for (int frame = 0; frame < 30; ++frame) {
            sandbox.tick(1.0 / 60.0);
        }
        require(
            sandbox.snapshot().playerPosition.y <
                fallStartHeight - 1.0,
            "another double jump exits sandbox flight");

        ian::Simulation regular{
            ian::GameBalance::defaults(), map, world};
        regular.startRun();
        regular.tick(1.0 / 60.0, tap);
        regular.tick(1.0 / 60.0);
        regular.tick(1.0 / 60.0, tap);
        for (int frame = 0; frame < 120; ++frame) {
            regular.tick(1.0 / 60.0, ascend);
        }
        require(
            regular.snapshot().playerGrounded,
            "double jump cannot enable flight outside sandbox");
    }
    {
        constexpr std::uint32_t SavedSeed = 0x4f52544bU;
        auto original = std::make_unique<ian::Simulation>();
        auto restored = std::make_unique<ian::Simulation>();
        original->startRunFromSeed(
            ian::PlayerClass::Prospector, SavedSeed);
        restored->startRunFromSeed(
            ian::PlayerClass::Prospector, SavedSeed);
        ian::PlayerCommand movement;
        movement.moveForward = 1.0;
        movement.lookYaw = 0.16;
        for (int tick = 0; tick < 90; ++tick) {
            original->tick(1.0 / 60.0, movement);
            restored->tick(1.0 / 60.0, movement);
            movement.lookYaw = 0.0;
        }
        require(
            original->snapshot().terrainSeed == SavedSeed &&
                restored->snapshot().terrainSeed == SavedSeed &&
                original->snapshot().tick == restored->snapshot().tick &&
                std::abs(original->snapshot().playerPosition.x -
                    restored->snapshot().playerPosition.x) < 1e-12 &&
                std::abs(original->snapshot().playerPosition.y -
                    restored->snapshot().playerPosition.y) < 1e-12 &&
                std::abs(original->snapshot().playerPosition.z -
                    restored->snapshot().playerPosition.z) < 1e-12,
            "saved seed and input replay restore deterministic run state");
    }
    {
        ian::Simulation simulation;
        simulation.startRun();

        ian::PlayerCommand grantInsight;
        grantInsight.grantInsight = ian::GrantInsightCommand{25.0};
        simulation.tick(1.0 / 60.0, grantInsight);
        requireNear(
            simulation.snapshot().currentInsight, 0.0, 1e-12,
            "debug Insight shortcut is ignored outside god mode");

        ian::PlayerCommand godMode;
        godMode.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        simulation.tick(1.0 / 60.0, godMode);
        simulation.takeEvents();
        simulation.tick(1.0 / 60.0, grantInsight);
        requireNear(
            simulation.snapshot().currentInsight, 25.0, 1e-12,
            "god mode can grant debug Insight");
        const auto events = simulation.takeEvents();
        require(
            std::ranges::any_of(
                events,
                [](const ian::GameEvent& event) {
                    return event.type ==
                               ian::GameEventType::InsightGranted &&
                           event.amount == 25;
                }),
            "debug Insight grant uses the normal progression event");
    }
    {
        ian::GameBalance balance = ian::GameBalance::defaults();
        auto& coreBalance = balance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)];
        coreBalance.wood = 0;
        coreBalance.stone = 0;
        coreBalance.crystals = 0;
        ian::MapDefinition map = ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        auto interrupted = std::make_unique<ian::Simulation>(
            balance, map, world);
        interrupted->startRunFromSeed(
            ian::PlayerClass::Vanguard, 0x57415645U);
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        interrupted->tick(1.0 / 60.0, placeCore);
        require(interrupted->snapshot().coreMaxHealth > 0.0,
                "interrupted-wave fixture places a core");
        resolveLevelUps(*interrupted);
        ian::PlayerCommand beginWave;
        beginWave.startWaveEarly = ian::StartWaveEarlyCommand{};
        interrupted->tick(1.0 / 60.0, beginWave);
        require(interrupted->snapshot().state == ian::RunState::Wave &&
                    interrupted->snapshot().wave == 1,
                "interrupted-wave fixture starts its first night");
        interrupted->grantLootUpgrade(
            ian::LootUpgradeEffect::Map,
            ian::LootRarity::Common);
        require(
            interrupted->snapshot().lootStacks[ian::lootUpgradeIndex(
                ian::LootUpgradeEffect::Map)] == 1,
            "interrupted-wave fixture earns transient wave progress");

        const ian::SuspendedRunState replay =
            interrupted->saveSuspendedRunState();
        require(replay.resumeState == ian::RunState::BuildPhase &&
                    replay.wave == 0 &&
                    replay.lootStacks[ian::lootUpgradeIndex(
                        ian::LootUpgradeEffect::Map)] == 0,
                "active night checkpoint replays without farming wave rewards");
        auto resumed = std::make_unique<ian::Simulation>(
            balance, map, world);
        require(resumed->loadSuspendedRunState(replay) &&
                    resumed->snapshot().state == ian::RunState::BuildPhase &&
                    resumed->snapshot().wave == 0 &&
                    resumed->snapshot().enemies.empty(),
                "active night resumes at its preceding safe build phase");
    }
    {
        ian::GameBalance timedBalance = ian::GameBalance::defaults();
        timedBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)].wood = 0;
        timedBalance.gameplay.nightDurationSeconds = 12.0;
        ian::WorldConfig timedWorld = ian::WorldConfig::defaults();
        timedWorld.terrainAmplitude = 0.0;
        ian::Simulation timedNight{
            timedBalance, ian::MapDefinition::defaults(), timedWorld};
        timedNight.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        timedNight.tick(1.0 / 60.0, unlimited);
        const auto& unlocked = timedNight.snapshot().unlockedBuildings;
        require(
            !unlocked[static_cast<std::size_t>(
                ian::BuildingType::WoodStorage)] &&
                !unlocked[static_cast<std::size_t>(
                    ian::BuildingType::StoneStorage)] &&
                !unlocked[static_cast<std::size_t>(
                    ian::BuildingType::CrystalStorage)],
            "godmode does not restore removed storage buildings");
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        timedNight.tick(1.0 / 60.0, placeCore);
        resolveLevelUps(timedNight);
        ian::PlayerCommand startNight;
        startNight.startWaveEarly = ian::StartWaveEarlyCommand{};
        timedNight.tick(1.0 / 60.0, startNight);
        require(
            timedNight.snapshot().state == ian::RunState::Wave &&
                timedNight.snapshot().phaseDuration == 12.0,
            "regular nights expose their fixed dawn timer");
        timedNight.tick(8.0);
        require(
            timedNight.snapshot().state == ian::RunState::Wave &&
                timedNight.snapshot().activeEnemyCount >= 8U &&
                timedNight.snapshot().activeEnemyCount <= 16U,
            "first timed night builds continuous pressure without overcrowding");
        timedNight.tick(4.0);
        require(
            timedNight.snapshot().state == ian::RunState::WaveComplete &&
                timedNight.snapshot().activeEnemyCount == 0U,
            "dawn ends the fixed night and banishes remaining enemies");
    }
    {
        ian::GameBalance balance = ian::GameBalance::defaults();
        balance.gameplay.nightDurationSeconds = 20.0;
        ian::WorldConfig world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        ian::Simulation fastClearNight{
            balance, ian::MapDefinition::defaults(), world};
        fastClearNight.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        fastClearNight.tick(0.0, unlimited);
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        fastClearNight.tick(0.0, placeCore);
        resolveLevelUps(fastClearNight);
        ian::PlayerCommand startNight;
        startNight.startWaveEarly = ian::StartWaveEarlyCommand{};
        fastClearNight.tick(0.0, startNight);
        require(fastClearNight.snapshot().activeEnemyCount == 3U &&
                    fastClearNight.snapshot().pendingEnemyCount == 12U,
            "fast-clear night starts with the configured group");

        const auto killEnemies = [&](int count) {
            const auto enemies = fastClearNight.snapshot().enemies;
            const auto target = std::ranges::find_if(
                enemies, [](const ian::EnemyInstance& enemy) {
                    return enemy.active;
                });
            require(target != enemies.end(),
                    "fast-clear fixture has a live target");
            ian::PlayerCommand lightning;
            lightning.castChainLightning =
                ian::CastChainLightningCommand{
                    .firstTarget = target->id,
                    .damage = 1'000'000.0,
                    .jumpRadius = 50.0,
                    .damageFalloff = 1.0,
                    .maximumTargets = count,
                };
            fastClearNight.tick(0.0, lightning);
        };
        killEnemies(3);
        require(fastClearNight.snapshot().activeEnemyCount == 3U &&
                    fastClearNight.snapshot().pendingEnemyCount == 9U,
                "clearing the opening group immediately admits one replacement group");
        fastClearNight.tick(1.61);
        require(fastClearNight.snapshot().activeEnemyCount >= 3U &&
                    fastClearNight.snapshot().pendingEnemyCount < 12U,
                "clearing a group does not leave a long empty night");
        fastClearNight.takeEvents();
        fastClearNight.tick(5.61);
        const auto firstWarning = fastClearNight.takeEvents();
        require(
            std::ranges::any_of(
                firstWarning, [](const ian::GameEvent& event) {
                    return event.type ==
                               ian::GameEventType::AttackDirectionWarned &&
                           event.critical;
                }),
            "first horde peak warns the player before it starts");
        fastClearNight.tick(8.01);
        const auto secondWarning = fastClearNight.takeEvents();
        require(
            std::ranges::any_of(
                secondWarning, [](const ian::GameEvent& event) {
                    return event.type ==
                               ian::GameEventType::AttackDirectionWarned &&
                           event.critical;
                }),
            "final horde peak has its own warning");
    }
    {
        ian::GameBalance balance = ian::GameBalance::defaults();
        auto& core = balance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)];
        core.wood = 0;
        core.stone = 0;
        core.crystals = 0;
        auto& mine = balance.buildings[static_cast<std::size_t>(
            ian::BuildingType::CrystalMine)];
        mine.wood = 0;
        mine.stone = 0;
        mine.crystals = 0;
        balance.economy.coreUpgradeCrystals = {0, 0};
        ian::MapDefinition map = ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        ian::Simulation earlyStart{balance, map, world};
        earlyStart.startRun();

        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        earlyStart.tick(0.0, placeCore);
        resolveLevelUps(earlyStart);
        const auto coreId = earlyStart.snapshot().coreId;
        require(coreId.has_value(),
                "early-start economy fixture places its Core");
        ian::PlayerCommand upgradeCore;
        upgradeCore.upgradeBuilding =
            ian::UpgradeBuildingCommand{*coreId};
        earlyStart.tick(0.0, upgradeCore);
        require(earlyStart.grantDebugRunUpgrade(ian::RunUpgradeEffect::UnlockCrystalMine),
            "early-start fixture acquires its producer blueprint");
        ian::PlayerCommand placeMine;
        placeMine.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::CrystalMine, {7, 3}, 0};
        earlyStart.tick(0.0, placeMine);
        require(
            std::ranges::any_of(
                earlyStart.snapshot().buildings,
                [](const ian::BuildingInstance& building) {
                    return building.type ==
                        ian::BuildingType::CrystalMine;
                }),
            "early-start economy fixture places a crystal mine");

        ian::PlayerCommand startEarly;
        startEarly.startWaveEarly = ian::StartWaveEarlyCommand{};
        earlyStart.tick(0.0, startEarly);
        require(
            earlyStart.snapshot().crystals == 40,
            "starting night early resolves skipped mine production and bonus");
    }
    {
        ian::GameBalance balance = ian::GameBalance::defaults();
        balance.gameplay.nightDurationSeconds = 1.0;
        ian::WorldConfig world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        ian::Simulation bossNight{
            balance, ian::MapDefinition::defaults(), world};
        bossNight.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        bossNight.tick(0.0, unlimited);
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        bossNight.tick(0.0, placeCore);
        ian::PlayerCommand startNight;
        startNight.startWaveEarly = ian::StartWaveEarlyCommand{};
        ian::PlayerCommand clearNight;
        clearNight.defeatAllEnemies = ian::DefeatAllEnemiesCommand{};
        for (int wave = 1; wave < 6; ++wave) {
            resolveLevelUps(bossNight);
            bossNight.tick(0.0, startNight);
            bossNight.tick(0.0, clearNight);
            require(bossNight.snapshot().state == ian::RunState::WaveComplete,
                    "boss timing fixture advances through early nights");
            resolveLevelUps(bossNight);
            bossNight.tick(bossNight.snapshot().phaseDuration);
        }
        bossNight.tick(0.0, startNight);
        require(std::ranges::all_of(bossNight.snapshot().enemies,[](const ian::EnemyInstance& enemy){
                    return !enemy.active || (enemy.arrivalPortal && enemy.portalArrivalRemaining>0);}),
                "wave groups carry actual portal birth metadata");
        const auto hasActiveBoss = [&bossNight] {
            return std::ranges::any_of(
                bossNight.snapshot().enemies,
                [](const ian::EnemyInstance& enemy) {
                    return enemy.active && enemy.type == ian::EnemyType::Boss;
                });
        };
        require(!hasActiveBoss(),
                "boss waits for the final pressure peak");
        bossNight.tick(0.79);
        require(!hasActiveBoss(),
                "boss stays hidden before final pressure peak");
        bossNight.tick(0.02);
        require(hasActiveBoss(),
                "boss joins the final pressure peak");
        bossNight.tick(.5);
        require(hasActiveBoss() && bossNight.snapshot().state==ian::RunState::Wave,
                "dawn timer cannot banish the living royal boss");
    }
    {
        ian::Simulation defenseProgression;
        defenseProgression.startRun();
        const auto initial = defenseProgression.snapshot();
        require(
            initial.unlockedBuildings[static_cast<std::size_t>(
                ian::BuildingType::Core)] &&
                !initial.unlockedBuildings[static_cast<std::size_t>(
                    ian::BuildingType::GunTurret)] &&
                !initial.unlockedBuildings[static_cast<std::size_t>(
                    ian::BuildingType::Turret)] &&
                !initial.unlockedBuildings[static_cast<std::size_t>(
                    ian::BuildingType::Cannon)],
            "only the Core is exposed before it is placed");
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
        resolveLevelUps(rotationSimulation);
        ian::PlayerCommand placeTurret;
        placeTurret.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::GunTurret,
            .gridPosition = {0, -4},
        };
        rotationSimulation.tick(1.0 / 60.0, placeTurret);
        const auto placed = rotationSimulation.snapshot();
        const auto turret = std::ranges::find(
            placed.buildings, ian::BuildingType::GunTurret,
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
        resolveLevelUps(wallDragFoundations);
        require(
            wallDragFoundations.snapshot().coreId.has_value(),
            "wall drag foundation fixture creates core");
        ian::PlayerCommand unlockWalls;
        unlockWalls.upgradeBuilding = ian::UpgradeBuildingCommand{
            *wallDragFoundations.snapshot().coreId};
        wallDragFoundations.tick(1.0 / 60.0, unlockWalls);

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
        ian::MapDefinition map = ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 8.0;
        world.terrainFeatureSize = 12.0;
        world.terrainSlopeWidth = 2.0;
        world.terrainBuildPlateauRadius = 2.0;
        world.coreFlatRadius = 0.0;
        world.buildPreviewDistance = 100.0;
        world.pondMinimumCount = 0;
        world.pondMaximumCount = 0;
        world.pondMaximumAreaFraction = 0.0;
        ian::GameBalance balance = ian::GameBalance::defaults();
        balance.gameplay.maximumPlacementDistance = 100.0;
        constexpr std::uint32_t TerrainSeed = 0x53544142U;
        const auto makeSimulation = [&]() {
            auto simulation = std::make_unique<ian::Simulation>(
                balance, map, world);
            simulation->startSandboxRun();
            simulation->regenerateTerrain(TerrainSeed);
            ian::PlayerCommand placeCore;
            placeCore.placeBuilding = ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Core,
                .gridPosition = {0, 0},
            };
            simulation->tick(1.0 / 60.0, placeCore);
            return simulation;
        };
        auto heightProbe = makeSimulation();
        ian::BuildGrid heightGrid{world};
        ian::PlacementValidator heightValidator{
            heightProbe->terrain(), heightGrid};

        std::optional<ian::GridCoord> targetAnchor;
        std::optional<ian::GridCoord> neighborAnchor;
        double targetHeight = 0.0;
        double neighborHeight = 0.0;
        for (int z = -40; z <= 40 && !targetAnchor; z += 2) {
            for (int x = -40; x <= 38; x += 2) {
                const ian::Vec3 targetHit{
                    static_cast<double>(x) + 1.0, 0.0,
                    static_cast<double>(z) + 1.0};
                const ian::Vec3 neighborHit{
                    static_cast<double>(x) + 3.0, 0.0,
                    static_cast<double>(z) + 1.0};
                const auto target = heightProbe->previewFoundation(targetHit);
                const auto neighbor = heightProbe->previewFoundation(neighborHit);
                const double stableTargetHeight =
                    heightValidator.stableGroundPlatformHeight(
                        target.anchor,
                        ian::GroundPlatformHeightStep);
                const double stableNeighborHeight =
                    heightValidator.stableGroundPlatformHeight(
                        neighbor.anchor,
                        ian::GroundPlatformHeightStep);
                if (target.valid() && neighbor.valid() &&
                    std::abs(stableTargetHeight -
                             stableNeighborHeight) > 0.251) {
                    targetAnchor = target.anchor;
                    neighborAnchor = neighbor.anchor;
                    targetHeight = stableTargetHeight;
                    neighborHeight = stableNeighborHeight;
                    break;
                }
            }
        }
        require(
            targetAnchor.has_value() && neighborAnchor.has_value(),
            "terrain fixture finds adjacent valid cells on different construction planes");

        std::optional<ian::Vec3> flatSourceHit;
        std::optional<ian::Vec3> flatTargetHit;
        for (int z = -40; z <= 40 && !flatSourceHit; z += 2) {
            for (int x = -40; x <= 38; x += 2) {
                const std::array hits{
                    ian::Vec3{
                        static_cast<double>(x) + 1.0, 0.0,
                        static_cast<double>(z) + 1.0},
                    ian::Vec3{
                        static_cast<double>(x) + 3.0, 0.0,
                        static_cast<double>(z) + 1.0},
                };
                const std::array previews{
                    heightProbe->previewFoundation(hits[0]),
                    heightProbe->previewFoundation(hits[1]),
                };
                const std::array fineHeights{
                    heightValidator.stableGroundPlatformHeight(
                        previews[0].anchor, 0.05),
                    heightValidator.stableGroundPlatformHeight(
                        previews[1].anchor, 0.05),
                };
                const double difference = std::abs(
                    fineHeights[0] - fineHeights[1]);
                if (!previews[0].valid() ||
                    !previews[1].valid() ||
                    difference < 1e-6 || difference > 0.21) {
                    continue;
                }
                const std::size_t sourceIndex =
                    fineHeights[0] >= fineHeights[1]
                        ? 0U
                        : 1U;
                const std::size_t targetIndex = 1U - sourceIndex;
                if (!heightProbe->previewFoundationAtHeight(
                         hits[targetIndex],
                         fineHeights[sourceIndex])
                         .valid()) {
                    continue;
                }
                flatSourceHit = hits[sourceIndex];
                flatTargetHit = hits[targetIndex];
                break;
            }
        }
        require(
            flatSourceHit.has_value() && flatTargetHit.has_value(),
            "terrain fixture finds adjacent cells separated only by surface noise");

        auto flatFoundationPlacement = makeSimulation();
        const auto placedFlatSource =
            flatFoundationPlacement->placeFoundation(
                *flatSourceHit);
        const auto alignedFlatTarget =
            flatFoundationPlacement->previewFoundation(
                *flatTargetHit);
        require(
            placedFlatSource && alignedFlatTarget.valid() &&
                std::abs(
                    alignedFlatTarget.floorHeight -
                    placedFlatSource->floorHeight) < 1e-9,
            "adjacent foundations ignore small terrain height noise");

        auto flatBuildingPlacement = makeSimulation();
        const auto sourceGrid =
            flatBuildingPlacement->previewFoundation(
                *flatSourceHit).anchor;
        const auto targetGrid =
            flatBuildingPlacement->previewFoundation(
                *flatTargetHit).anchor;
        ian::PlayerCommand placeFlatSourceBuilding;
        placeFlatSourceBuilding.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Wall,
                .gridPosition = {
                    sourceGrid.x, sourceGrid.z},
            };
        flatBuildingPlacement->tick(
            1.0 / 60.0, placeFlatSourceBuilding);
        ian::PlayerCommand placeFlatTargetBuilding;
        placeFlatTargetBuilding.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Wall,
                .gridPosition = {
                    targetGrid.x, targetGrid.z},
            };
        flatBuildingPlacement->tick(
            1.0 / 60.0, placeFlatTargetBuilding);
        const auto flatBuildings =
            flatBuildingPlacement->snapshot().buildings;
        const auto sourceBuilding = std::ranges::find_if(
            flatBuildings,
            [&sourceGrid](const ian::BuildingInstance& building) {
                return building.type == ian::BuildingType::Wall &&
                    building.gridPosition.x == sourceGrid.x &&
                    building.gridPosition.z == sourceGrid.z;
            });
        const auto targetBuilding = std::ranges::find_if(
            flatBuildings,
            [&targetGrid](const ian::BuildingInstance& building) {
                return building.type == ian::BuildingType::Wall &&
                    building.gridPosition.x == targetGrid.x &&
                    building.gridPosition.z == targetGrid.z;
            });
        require(
            sourceBuilding != flatBuildings.end() &&
                targetBuilding != flatBuildings.end() &&
                std::abs(
                    sourceBuilding->baseHeight -
                    targetBuilding->baseHeight) < 1e-9,
            "adjacent automatic building foundations share a flat construction plane");

        auto directPlacement = makeSimulation();
        ian::PlayerCommand placeTarget;
        placeTarget.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::Wall,
            .gridPosition = {
                targetAnchor->x, targetAnchor->z},
        };
        directPlacement->tick(1.0 / 60.0, placeTarget);
        const auto directTargetWall = std::ranges::find_if(
            directPlacement->snapshot().buildings,
            [&targetAnchor](const ian::BuildingInstance& building) {
                return building.type == ian::BuildingType::Wall &&
                    building.gridPosition.x == targetAnchor->x &&
                    building.gridPosition.z == targetAnchor->z;
            });
        require(
            directTargetWall != directPlacement->snapshot().buildings.end(),
            "stable-height fixture can place the target directly");
        const double directTargetHeight = directTargetWall->baseHeight;

        auto placementAfterNeighbor = makeSimulation();
        ian::PlayerCommand placeNeighbor;
        placeNeighbor.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::Wall,
            .gridPosition = {
                neighborAnchor->x, neighborAnchor->z},
        };
        placementAfterNeighbor->tick(1.0 / 60.0, placeNeighbor);
        placementAfterNeighbor->tick(1.0 / 60.0, placeTarget);
        const auto targetWall = std::ranges::find_if(
            placementAfterNeighbor->snapshot().buildings,
            [&targetAnchor](const ian::BuildingInstance& building) {
                return building.type == ian::BuildingType::Wall &&
                    building.gridPosition.x == targetAnchor->x &&
                    building.gridPosition.z == targetAnchor->z;
            });
        require(
            targetWall !=
                    placementAfterNeighbor->snapshot().buildings.end() &&
                std::abs(directTargetHeight - targetHeight) < 1e-9 &&
                std::abs(targetWall->baseHeight - directTargetHeight) < 1e-9 &&
                std::abs(targetWall->baseHeight - neighborHeight) > 0.051,
            "placing a neighbour does not change a ground cell's construction height");
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
        resolveLevelUps(lightningSimulation);
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
        resolveLevelUps(defeatSimulation);
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
            ian::EnemyType::Boss, 1};
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
        ian::GameBalance blueprintBalance =
            ian::GameBalance::defaults();
        blueprintBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Core)].wood = 0;
        blueprintBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Wall)].wood = 0;
        blueprintBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Wall)].stone = 0;
        blueprintBalance.buildings[static_cast<std::size_t>(
            ian::BuildingType::Wall)].crystals = 0;
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
        resolveLevelUps(blueprintSimulation);
        const double beforeBlueprint =
            blueprintSimulation.snapshot().totalInsightEarned;
        blueprintSimulation.grantLootUpgrade(
            ian::LootUpgradeEffect::Blueprint,
            ian::LootRarity::Rare);
        require(
            blueprintSimulation.snapshot().unlockedBuildings[
                static_cast<std::size_t>(ian::BuildingType::Wall)],
            "basic walls are available without combat blueprint acquisition");
        requireNear(
            blueprintSimulation.snapshot().totalInsightEarned,
            beforeBlueprint + blueprintSimulation
                .insightSystem().config().firstBuildingTypeBonus,
            1e-9,
            "Blueprint immediately rewards already-built unique types");
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
        resolveLevelUps(storageSimulation);
        require(
            storageSimulation.snapshot().woodCapacity == 120 &&
                storageSimulation.snapshot().stoneCapacity == 75 &&
                storageSimulation.snapshot().crystalCapacity == 75,
            "level-one core expands the initial inventory capacity");

        // This assertion covers the normal catalog. Creative modes expose
        // these legacy structures intentionally for testing.
        storageSimulation.tick(1.0 / 60.0, godMode);
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

        storageSimulation.tick(1.0 / 60.0, godMode);

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
            storageSimulation.snapshot().woodCapacity == 240 &&
                storageSimulation.snapshot().stoneCapacity == 150 &&
                storageSimulation.snapshot().crystalCapacity == 150,
            "upgrading the core expands all resource capacities");
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
        resolveLevelUps(potionSimulation);
        ian::PlayerCommand startWave;
        startWave.startWaveEarly =
            ian::StartWaveEarlyCommand{};
        potionSimulation.tick(0.0, startWave);
        require(
            potionSimulation.snapshot().state == ian::RunState::Wave &&
                potionSimulation.snapshot().battlePotionAvailable,
            "Battle Potion arms once at the beginning of a night");
        const double potionTriggerDamage =
            potionSimulation.snapshot().playerHealth * 0.8;
        ian::PlayerCommand criticalDamage;
        criticalDamage.damagePlayer =
            ian::DamagePlayerCommand{potionTriggerDamage};
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
        auto transactionBalance =
            ian::GameBalance::defaults();
        transactionBalance.gameplay.resourceToolDamageMultiplier = 1.0;
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
        turretDefinition.unlockCoreLevel = 1;
        transactionBalance.economy.coreUpgradeCrystals = {0, 0};
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
        resolveLevelUps(transactions);
        require(
            transactions.snapshot().coreId.has_value() &&
                transactions.snapshot().wood == 10,
            "successful placement spends configured cost exactly once");
        ian::PlayerCommand unlockTransactionWalls;
        unlockTransactionWalls.upgradeBuilding =
            ian::UpgradeBuildingCommand{
                *transactions.snapshot().coreId};
        transactions.tick(1.0 / 60.0, unlockTransactionWalls);

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
            resolveLevelUps(restartStress);
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
        producerUnlockBalance.economy.coreUpgradeCrystals = {0, 0};
        ian::WorldConfig producerUnlockWorld =
            ian::WorldConfig::defaults();
        producerUnlockWorld.terrainAmplitude = 0.0;
        auto producerFixture = std::make_unique<ian::Simulation>(
            producerUnlockBalance, ian::MapDefinition::defaults(), producerUnlockWorld);
        auto& producerUnlocks = *producerFixture;
        producerUnlocks.startRun();
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        producerUnlocks.tick(1.0 / 60.0, placeCore);
        resolveLevelUps(producerUnlocks);
        const auto initial = producerUnlocks.snapshot();
        for (auto type : {ian::BuildingType::Wall, ian::BuildingType::Gate}) {
            require(initial.unlockedBuildings[static_cast<std::size_t>(type)],
                "perimeter remains available at Core level one");
        }
        for (auto type : ian::RunEconomyBuildingTypes) {
            require(!initial.unlockedBuildings[static_cast<std::size_t>(type)],
                "producer construction requires its level-up blueprint");
        }
        require(ian::ModularBuildingRequiredCoreLevel == 1 && initial.buildingArsenalCount == 1,
            "foundation access does not consume a combat slot");
        const auto coreId = initial.coreId;
        require(coreId.has_value(), "progression fixture places the Core");
        ian::PlayerCommand upgradeCore;
        upgradeCore.upgradeBuilding = ian::UpgradeBuildingCommand{*coreId};
        ian::PlayerCommand toggleUnlimited;
        toggleUnlimited.enableUnlimitedResources = ian::EnableUnlimitedResourcesCommand{};
        for (int level = 2; level <= 5; ++level) {
            producerUnlocks.tick(1.0 / 60.0, toggleUnlimited);
            producerUnlocks.tick(1.0 / 60.0, upgradeCore);
            producerUnlocks.tick(1.0 / 60.0, toggleUnlimited);
            const auto upgraded = producerUnlocks.snapshot();
            require(upgraded.coreLevel == level && upgraded.buildingArsenalCount == 1,
                "Core upgrades expand capacity without acquiring combat types");
            for (auto type : ian::RunBuildingTypes) {
                require(upgraded.unlockedBuildings[static_cast<std::size_t>(type)] ==
                    (type == ian::BuildingType::GunTurret), "only the starter remains in the arsenal");
            }
            for (const auto weapon : {ian::PlayerWeapon::Club, ian::PlayerWeapon::Rifle,
                    ian::PlayerWeapon::Bomb, ian::PlayerWeapon::IceWand, ian::PlayerWeapon::FireWand}) {
                require(!upgraded.unlockedWeapons[static_cast<std::size_t>(weapon)],
                    "Core upgrades never acquire player weapons");
            }
        }

    }

    {
        ian::Simulation foundationLifecycle;
        foundationLifecycle.startRun(ian::PlayerClass::Engineer);
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
        resolveLevelUps(foundationLifecycle);
        require(
            foundationLifecycle.snapshot()
                .coreId.has_value(),
            "foundation lifecycle fixture creates core");

        const ian::GridPosition turretPosition{1, 5};
        const auto terrainSurface =
            foundationLifecycle
                .previewPlacementSurface(
                    ian::BuildingType::GunTurret,
                    turretPosition);
        const double playerHeightBefore =
            foundationLifecycle.snapshot()
                .playerPosition.y;
        ian::PlayerCommand placeRaisedTurret;
        placeRaisedTurret.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::GunTurret,
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
                           ian::BuildingType::GunTurret &&
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
        const ian::ResourceCost standardRefund =
            ian::buildingSellRefund(*raisedTurret);
        auto legacySalvage = foundationLifecycle.saveSuspendedRunState();
        legacySalvage.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::Salvager)] = 1;
        legacySalvage.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::Salvager)] = 1.0;
        foundationLifecycle.returnToMainMenu();
        require(foundationLifecycle.loadSuspendedRunState(legacySalvage),
                "legacy salvage bonus resumes before a sale");
        foundationLifecycle.tick(1.0 / 60.0);
        const auto resourcesBeforeSale =
            foundationLifecycle.snapshot();
        const ian::ResourceCost saleCost =
            ian::buildingCost(ian::BuildingType::GunTurret);
        const auto expectedSaleAmount = [](int cost, int refund) {
            return refund + std::min(
                cost - refund,
                static_cast<int>(std::lround(
                    static_cast<double>(cost) * 0.30)));
        };
        ian::PlayerCommand sellTurret;
        sellTurret.sellBuilding =
            ian::SellBuildingCommand{turretId};
        foundationLifecycle.tick(
            1.0 / 60.0, sellTurret);
        const auto resourcesAfterSale =
            foundationLifecycle.snapshot();
        require(
            resourcesAfterSale.wood - resourcesBeforeSale.wood ==
                expectedSaleAmount(
                    saleCost.wood, standardRefund.wood) &&
            resourcesAfterSale.stone - resourcesBeforeSale.stone ==
                expectedSaleAmount(
                    saleCost.stone, standardRefund.stone) &&
            resourcesAfterSale.crystals -
                    resourcesBeforeSale.crystals ==
                expectedSaleAmount(
                    saleCost.crystals, standardRefund.crystals),
            "salvage adds 30 percent of cost to ordinary sale refund");
        foundationLifecycle.tick(1.0 / 60.0, unlimited);
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
                .type = ian::BuildingType::GunTurret,
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
                                   GunTurret;
                });
        require(
            destroyedBuilding !=
                    collapseEvents.end() &&
                destroyedBuilding->structuralCollapse,
            "platform collapse event retains building visual snapshot");
    }

    {
        ian::Simulation modularSalvage;
        modularSalvage.startRun(ian::PlayerClass::Engineer);
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        modularSalvage.tick(1.0 / 60.0, unlimited);
        const auto frame = modularSalvage.placeFoundation(
            {0.2, modularSalvage.terrain().getHeight(0.2, 4.2),
             4.2});
        require(frame.has_value(),
                "modular salvage fixture creates a foundation");
        auto legacySalvage = modularSalvage.saveSuspendedRunState();
        legacySalvage.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::Salvager)] = 1;
        legacySalvage.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::Salvager)] = 1.0;
        modularSalvage.returnToMainMenu();
        require(modularSalvage.loadSuspendedRunState(legacySalvage),
                "modular salvage fixture retains an old acquired bonus");
        modularSalvage.tick(1.0 / 60.0);
        const auto beforeRemoval = modularSalvage.snapshot();
        ian::PlayerCommand removeFoundation;
        removeFoundation.removeModularBuilding =
            ian::RemoveModularBuildingCommand{frame->id};
        modularSalvage.tick(1.0 / 60.0, removeFoundation);
        const auto afterRemoval = modularSalvage.snapshot();
        const auto foundationCost =
            ian::GameBalance::defaults().modularBuildings[0];
        const auto salvageAmount = [](int cost) {
            return static_cast<int>(std::lround(
                static_cast<double>(cost) * 0.30));
        };
        require(
            afterRemoval.platformFrames.empty() &&
                afterRemoval.wood - beforeRemoval.wood ==
                    salvageAmount(foundationCost.wood) &&
                afterRemoval.stone - beforeRemoval.stone ==
                    salvageAmount(foundationCost.stone) &&
                afterRemoval.crystals - beforeRemoval.crystals ==
                    salvageAmount(foundationCost.crystals),
            "salvage refunds 30 percent when a modular foundation is sold");
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
        resolveLevelUps(protectedCore);
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
        elevatedBuilding.startSandboxRun();
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Core,
                .gridPosition = {0, 0},
            };
        elevatedBuilding.tick(1.0 / 60.0, placeCore);
        resolveLevelUps(elevatedBuilding);
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
        const auto lowerSurface =
            elevatedBuilding.previewPlacementSurface(
                ian::BuildingType::Cannon,
                turretPosition,
                ground->floorHeight);
        require(
            lowerSurface.storey == ground->storey &&
                std::abs(
                    lowerSurface.height -
                    ground->floorHeight) < 1e-9,
            "ordinary building can target a lower foundation beneath a floor");
        const auto lowerCannonPreview =
            elevatedBuilding.previewPlacement(
                ian::BuildingType::Cannon,
                turretPosition,
                ground->floorHeight);
        require(
            lowerCannonPreview.valid(),
            "ordinary building fits between a foundation and floor");
        ian::PlayerCommand placeLowerCannon;
        placeLowerCannon.placeBuilding =
            ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Cannon,
                .gridPosition = turretPosition,
                .baseHeight = ground->floorHeight,
                .platformStorey = ground->storey,
                .lockHeight = true,
            };
        elevatedBuilding.tick(
            1.0 / 60.0, placeLowerCannon);
        require(
            std::ranges::any_of(
                elevatedBuilding.snapshot().buildings,
                [turretPosition](
                    const ian::BuildingInstance& building) {
                    return building.type ==
                               ian::BuildingType::Cannon &&
                           building.gridPosition ==
                               turretPosition &&
                           building.platformStorey == 0;
                }),
            "ordinary building is placed below an existing floor");
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
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world =
            ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;

        ian::Simulation automaticFloor{
            ian::GameBalance::defaults(), map, world};
        automaticFloor.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        automaticFloor.tick(1.0 / 60.0, unlimited);
        const ian::Vec3 floorHit{0.2, 0.0, 4.2};
        const auto floorPreview =
            automaticFloor
                .previewFloorPlatformWithFoundation(
                    floorHit);
        require(
            floorPreview.valid() &&
                floorPreview.foundation.storey == 0 &&
                floorPreview.floor.storey == 1 &&
                floorPreview.floor.floorHeight >
                    floorPreview.foundation.floorHeight,
            "floor preview includes a valid automatic foundation");
        const auto floorBuild =
            automaticFloor.placeFloorPlatformWithFoundation(
                floorHit);
        require(
            floorBuild &&
                automaticFloor.snapshot()
                        .platformFrames.size() == 2,
            "floor and automatic foundation are placed together");

        ian::Simulation automaticRamp{
            ian::GameBalance::defaults(), map, world};
        automaticRamp.startRun();
        automaticRamp.tick(1.0 / 60.0, unlimited);
        const ian::Vec3 rampHit{0.2, 0.0, 4.2};
        const auto rampPreview =
            automaticRamp.previewRampWithFoundation(
                rampHit, ian::Rotation::Deg180);
        require(
            rampPreview.valid() &&
                rampPreview.foundations[0].storey == 0 &&
                rampPreview.foundations[1].storey == 0 &&
                rampPreview.ramp.targetStorey == 1,
            "ramp preview includes two valid automatic foundations");
        const auto rampBuild =
            automaticRamp.placeRampWithFoundation(
                rampHit, ian::Rotation::Deg180);
        require(
            rampBuild &&
                automaticRamp.snapshot()
                        .platformFrames.size() == 2 &&
                automaticRamp.snapshot().ramps.size() == 1,
            "ramp and both automatic foundations are placed together");
    }

    {
        ian::MapDefinition map = ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world = ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        world.pondMinimumCount = 0;
        world.pondMaximumCount = 0;
        world.pondMaximumAreaFraction = 0.0;
        ian::Simulation sharedGroundLevel{
            ian::GameBalance::defaults(), map, world};
        sharedGroundLevel.startSandboxRun();
        const ian::Vec3 foundationHit{4.2, 0.0, 0.2};
        const auto beforeCore =
            sharedGroundLevel.previewFoundation(foundationHit);

        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::Core,
            .gridPosition = {0, 0},
        };
        sharedGroundLevel.tick(1.0 / 60.0, placeCore);
        const auto afterCore =
            sharedGroundLevel.previewFoundation(foundationHit);
        const auto core = std::ranges::find(
            sharedGroundLevel.snapshot().buildings,
            ian::BuildingType::Core,
            &ian::BuildingInstance::type);
        require(
            beforeCore.valid() && afterCore.valid() &&
                core != sharedGroundLevel.snapshot().buildings.end() &&
                std::abs(beforeCore.floorHeight - core->baseHeight) < 1e-9 &&
                std::abs(afterCore.floorHeight - beforeCore.floorHeight) < 1e-9,
            "ground foundation stays level with Core before and after Core placement");
    }

    {
        ian::MapDefinition map =
            ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world =
            ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        world.buildPreviewDistance = 64.0;
        ian::Simulation foundationRadius{
            ian::GameBalance::defaults(), map, world};
        foundationRadius.startRun();
        ian::PlayerCommand unlimited;
        unlimited.enableUnlimitedResources =
            ian::EnableUnlimitedResourcesCommand{};
        foundationRadius.tick(1.0 / 60.0, unlimited);
        ian::PlayerCommand placeCore;
        placeCore.placeBuilding = ian::PlaceBuildingCommand{
            ian::BuildingType::Core, {0, 0}, 0};
        foundationRadius.tick(1.0 / 60.0, placeCore);
        require(
            foundationRadius.snapshot().coreMaxHealth > 0.0,
            "foundation radius fixture places Core");

        const double outsideX = static_cast<double>(
            foundationRadius.snapshot().coreBuildRadius + 3);
        const ian::Vec3 outsideHit{
            outsideX,
            foundationRadius.terrain().getHeight(outsideX, 0.2),
            0.2,
        };
        const auto outsidePreview =
            foundationRadius.previewFoundation(outsideHit);
        require(
            outsidePreview.error ==
                ian::ModularPlacementError::OutsideCoreArea &&
                !foundationRadius.placeFoundation(outsideHit),
            "foundation cannot be placed outside Core build radius");

        const ian::Vec3 insideHit{
            4.2,
            foundationRadius.terrain().getHeight(4.2, 0.2),
            0.2,
        };
        require(
            foundationRadius.previewFoundation(insideHit).valid(),
            "foundation remains placeable inside Core build radius");
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
        const auto groundRampPreview = raisedRampResources
            .previewRampWithFoundation(
                supportHit, ian::Rotation::Deg180);
        require(
            !groundRampPreview.valid() &&
                groundRampPreview.ramp.error ==
                ian::ModularPlacementError::
                    ResourceBlocked,
            "automatic ramp foundations remain blocked by resources");
        require(
            !raisedRampResources
                 .placeRampWithFoundation(
                     supportHit,
                     ian::Rotation::Deg180)
                 .has_value(),
            "automatic ramp and foundations are placed atomically");
    }

    {
        ian::MapDefinition map =
            ian::MapDefinition::defaults();
        map.resources.clear();
        map.obstacles.clear();
        ian::WorldConfig world =
            ian::WorldConfig::defaults();
        world.terrainAmplitude = 0.0;
        {
            auto fenceMap = map;
            fenceMap.playerSpawn = {3.5, 0.0, 1.7};
            auto fenceWorld = world;
            fenceWorld.terrainSurfaceNoiseAmplitude = 0.0;
            fenceWorld.pondMinimumCount = 0;
            fenceWorld.pondMaximumCount = 0;
            auto fenceJump = std::make_unique<ian::Simulation>(
                ian::GameBalance::defaults(), fenceMap, fenceWorld);
            fenceJump->startRun();
            ian::PlayerCommand setup;
            setup.enableUnlimitedResources = ian::EnableUnlimitedResourcesCommand{};
            setup.placeBuilding = ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Core, .gridPosition = {0, 0}};
            fenceJump->tick(1.0 / 60.0, setup);
            setup = {};
            setup.placeBuilding = ian::PlaceBuildingCommand{
                .type = ian::BuildingType::Wall, .gridPosition = {3, 0}};
            fenceJump->tick(1.0 / 60.0, setup);
            resolveLevelUps(*fenceJump);
            const auto beforeJump = fenceJump->snapshot();
            const auto fence = std::ranges::find(beforeJump.buildings,
                ian::BuildingType::Wall, &ian::BuildingInstance::type);
            require(fence != beforeJump.buildings.end(), "jump fixture places a fence");
            const double fenceTop = fence->baseHeight + 0.5;
            bool landed = false;
            for (int tick = 0; tick < 100; ++tick) {
                ian::PlayerCommand movement;
                movement.jump = tick == 0;
                movement.moveForward = fenceJump->snapshot().playerPosition.z > 0.6 ? 0.4 : 0.0;
                fenceJump->tick(1.0 / 60.0, movement);
                const auto after = fenceJump->snapshot();
                landed = landed || (tick > 5 && after.playerGrounded &&
                    std::abs(after.playerPosition.y - 1.7 - fenceTop) < 1e-6);
            }
            require(landed, "normal jump lands on the fence through full player movement");
            requireNear(fenceJump->snapshot().playerPosition.y, fenceTop + 1.7, 1e-6,
                        "player remains standing on fence after the jump");
        }
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
            autoJumpSimulation.placeFoundationAtHeight(
                {0.2, 0.0, 2.2},
                world.verticalGridStep);
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
            edgeBiasSimulation.placeFoundationAtHeight(
                {0.2, 0.0, 2.2},
                edgeBiasWorld.verticalGridStep);
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
            tapAutoJumpSimulation.placeFoundationAtHeight(
                {0.2, 0.0, 2.2},
                world.verticalGridStep);
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
            shortJumpSimulation.placeFoundationAtHeight(
                {0.2, 0.0, 2.2},
                world.verticalGridStep);
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
        resolveLevelUps(daytimeSpawning);

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
    simulationBalance.gameplay.resourceToolDamageMultiplier = 1.0;
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
    ian::PlayerCommand rangerMove;
    rangerMove.moveForward = 1.0;
    rangerClass.tick(0.5, rangerMove);
    requireNear(
        rangerClass.snapshot().playerDamageMultiplier, 1.50, 1e-12,
        "ranger gains momentum damage while moving");
    require(
        rangerClass.snapshot().selectedWeapon ==
            ian::PlayerWeapon::Axe,
        "ranger respects core weapon progression");
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
    requireNear(
        vanguardClass.snapshot().playerRecoverableArmor, 25.0, 1e-12,
        "vanguard begins with a recharging armor layer");
    require(
        vanguardClass.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Axe,
        "vanguard respects core weapon progression");

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
        engineerClass.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Axe,
        "engineer keeps a gathering tool equipped");
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
            ian::PlayerWeapon::Axe,
        "Smart Tools does not switch from resource hover alone");
    engineerAimStone.usePickaxe = true;
    engineerClass.tick(1.0 / 60.0, engineerAimStone);
    require(
        engineerClass.snapshot().selectedWeapon ==
            ian::PlayerWeapon::Pickaxe,
        "Smart Tools switches to the matching tool when attacking a resource");

    ian::Simulation prospectorClass{simulationBalance};
    prospectorClass.startRun(ian::PlayerClass::Prospector);
    require(
        prospectorClass.snapshot().playerClass ==
                ian::PlayerClass::Prospector &&
            prospectorClass.snapshot().selectedWeapon ==
                ian::PlayerWeapon::Pickaxe,
        "prospector starts with its gathering tools");
    requireNear(
        prospectorClass.snapshot().playerDamageMultiplier,
        0.85, 1e-12,
        "prospector trades combat damage for economy");
    require(
        prospectorClass.snapshot().woodCapacity == 75 &&
            prospectorClass.snapshot().stoneCapacity == 38 &&
            prospectorClass.snapshot().crystalCapacity == 13,
        "prospector can carry more resources before placing a core");

    auto berserkerClass =
        std::make_unique<ian::Simulation>(simulationBalance);
    berserkerClass->startRun(ian::PlayerClass::Berserker);
    require(
        berserkerClass->snapshot().dashUnlocked &&
            berserkerClass->snapshot().selectedWeapon ==
                ian::PlayerWeapon::Axe,
        "berserker starts with baseline dash and respects core weapon progression");
    ian::PlayerCommand woundBerserker;
    woundBerserker.damagePlayer = ian::DamagePlayerCommand{42.5};
    berserkerClass->tick(0.0, woundBerserker);
    requireNear(
        berserkerClass->snapshot().playerDamageMultiplier,
        1.375, 1e-12,
        "berserker gains damage from missing half of maximum health");
    ian::PlayerCommand berserkerSwing;
    berserkerSwing.usePickaxe = true;
    berserkerClass->tick(0.0, berserkerSwing);
    require(
        berserkerClass->snapshot().pickaxeCooldownRemaining <
            simulationBalance.gameplay.pickaxeCooldown,
        "wounded berserker swings melee weapons faster");

    auto vampireBalance = simulationBalance;
    vampireBalance.buildings[static_cast<std::size_t>(
        ian::BuildingType::Core)].wood = 0;
    auto vampireClass =
        std::make_unique<ian::Simulation>(vampireBalance);
    vampireClass->startRun(ian::PlayerClass::Vampire);
    ian::PlayerCommand woundVampire;
    woundVampire.damagePlayer = ian::DamagePlayerCommand{55.0};
    vampireClass->tick(0.0, woundVampire);
    ian::PlayerCommand placeVampireCore;
    placeVampireCore.placeBuilding = ian::PlaceBuildingCommand{
        ian::BuildingType::Core, {0, 0}, 0};
    vampireClass->tick(0.0, placeVampireCore);
    resolveLevelUps(*vampireClass);
    const double vampireHealthBeforeKill =
        vampireClass->snapshot().playerHealth;
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
                ian::lootUpgradeIndex(ian::LootUpgradeEffect::Potion)] == 1 &&
            alchemistClass->snapshot().selectedWeapon ==
                ian::PlayerWeapon::FireWand,
        "alchemist starts with Fire Wand, Apple and Potion");
    ian::PlayerCommand woundAlchemist;
    woundAlchemist.damagePlayer = ian::DamagePlayerCommand{70.0};
    alchemistClass->tick(0.0, woundAlchemist);
    requireNear(
        alchemistClass->snapshot().playerHealth, 60.0, 1e-12,
        "alchemist's starting apple heals 50 percent more");

    auto chronomancerClass =
        std::make_unique<ian::Simulation>(simulationBalance);
    chronomancerClass->startRun(ian::PlayerClass::Chronomancer);
    require(
        chronomancerClass->snapshot().lootStacks[
            ian::lootUpgradeIndex(ian::LootUpgradeEffect::Hourglass)] == 1 &&
            chronomancerClass->snapshot().lootStacks[
            ian::lootUpgradeIndex(ian::LootUpgradeEffect::Rope)] == 1 &&
            chronomancerClass->snapshot().selectedWeapon ==
                ian::PlayerWeapon::Axe,
        "chronomancer starts with Hourglass and Rope and respects core weapon progression");

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
    require(
        simulation.snapshot().tutorialObjective ==
                ian::TutorialObjective::MineWood &&
            simulation.snapshot().unlockedWeapons[
                static_cast<std::size_t>(ian::PlayerWeapon::Axe)] &&
            simulation.snapshot().unlockedWeapons[
                static_cast<std::size_t>(ian::PlayerWeapon::Pickaxe)],
        "tutorial starts with wood gathering and both smart tools available");

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
    requireNear(simulation.snapshot().phaseDuration, 75.0, 1e-12,
                "first day gives seventy-five seconds to explore and build");
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
                    criticalBalance.gameplay.pickaxeDamage *
                        criticalBalance.gameplay
                            .resourceToolDamageMultiplier *
                        2.0,
            "pickaxe critical hit doubles damage and marks event");

    simulation.restartRun();
    ian::PlayerCommand unlimited;
    unlimited.enableUnlimitedResources = ian::EnableUnlimitedResourcesCommand{};
    // Smart Tools must not let a randomly aligned resource change the
    // deterministic starting point of the weapon-cycle assertion below.
    unlimited.overrideAimedResource = true;
    unlimited.aimedResourceOverride.reset();
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
    const std::size_t chestsBeforeFirstNight =
        simulation.snapshot().lootChests.size();
    constexpr std::array<ian::PlayerWeapon, 7> GodModeTools{{
        ian::PlayerWeapon::Pickaxe,
        ian::PlayerWeapon::Club,
        ian::PlayerWeapon::Rifle,
        ian::PlayerWeapon::Bomb,
        ian::PlayerWeapon::IceWand,
        ian::PlayerWeapon::FireWand,
        ian::PlayerWeapon::Axe,
    }};
    for (const ian::PlayerWeapon expected : GodModeTools) {
        ian::PlayerCommand cycle;
        cycle.toggleWeapon = ian::ToggleWeaponCommand{};
        cycle.overrideAimedResource = true;
        cycle.aimedResourceOverride.reset();
        simulation.tick(1.0 / 60.0, cycle);
        require(simulation.snapshot().selectedWeapon == expected,
                "god mode weapon cycle excludes contextual tools");
    }
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
    godModeAimStone.usePickaxe = true;
    simulation.tick(1.0 / 60.0, godModeAimStone);
    require(
        simulation.snapshot().selectedWeapon ==
            ian::PlayerWeapon::Pickaxe,
        "base Smart Tools also selects the correct tool in god mode");
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
    resolveLevelUps(simulation);
    require(simulation.snapshot().coreMaxHealth > 0.0,
            "unlimited resources allow building without inventory");
    require(!simulation.snapshot().flowDebugVectors.empty(),
            "placing core publishes flow-field debug samples");
    require(simulation.snapshot().wood == 0, "unlimited building does not spend inventory");
    require(simulation.snapshot().tutorialObjective ==
                ian::TutorialObjective::PrepareForNight,
            "tutorial moves directly from the core to first-night preparation");
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
    require(std::ranges::none_of(weaponEvents,[](const auto& event) {
        return event.type==ian::GameEventType::WeaponUpgradeRejected || event.type==ian::GameEventType::WeaponUpgraded;
    }),"legacy V command is silent and cannot buy weapon levels");

    const auto coreId = *simulation.snapshot().coreId;
    ian::PlayerCommand placementUpgradeConflict;
    placementUpgradeConflict.selectBuilding =
        ian::BuildingType::GunTurret;
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
    require(simulation.snapshot().rifleLevel==1,"V no longer upgrades weapons");
    require(simulation.grantDebugRunUpgradeBatch(ian::RunUpgradeEffect::UnlockRifle,2),"rifle card reaches the godmode fixture level");
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
    bool fortified = false;
    for (const auto& event : buildingEvents) {
        if (event.type == ian::GameEventType::BuildingFortified) fortified = true;
    }
    require(fortified, "fortification kit fortifies full-health building");

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
        "repair and fortification have a per-target cooldown");
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
    require(
        simulation.snapshot().attackPortalPositions[
            static_cast<std::size_t>(ian::AttackDirection::South)]
                .has_value() &&
            std::count_if(
                simulation.snapshot().attackPortalPositions.begin(),
                simulation.snapshot().attackPortalPositions.end(),
                [](const auto& portal) { return portal.has_value(); }) == 1,
        "first night opens one portal on its attack front");
    require(
        simulation.snapshot().attackPortalPositions[
            static_cast<std::size_t>(ian::AttackDirection::South)]->z > 0.0,
        "south attack portal surrounds the enemy spawn lane");
    require(simulation.snapshot().activeEnemyCount == 3 &&
                simulation.snapshot().pendingEnemyCount == 12,
            "early wave immediately spawns first enemy group");
    const ian::Vec3 southPortal =
        *simulation.snapshot().attackPortalPositions[
            static_cast<std::size_t>(ian::AttackDirection::South)];
    require(
        std::ranges::all_of(
            simulation.snapshot().enemies,
            [southPortal](const ian::EnemyInstance& enemy) {
                return !enemy.active ||
                    (std::abs(enemy.position.x-southPortal.x)<=1.25 &&
                     enemy.position.z<=southPortal.z && enemy.position.z>=southPortal.z-2.0);
            }),
        "opening enemy group retains aperture width and emerges on the forward side");
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
                simulation.snapshot().activeEnemyCount == 3 &&
                simulation.snapshot().pendingEnemyCount == 12,
            "first wave starts with configured enemy group");
    requireNear(simulation.snapshot().phaseDuration, 60.0, 1e-12,
                "first night leaves room for early base expansion");
    require(simulation.snapshot().enemies.front().position.z > 20.0,
            "first group enters from outside initial player view");
    require(simulation.snapshot().tutorialObjective ==
                ian::TutorialObjective::SurviveFirstWave,
            "tutorial switches to first-night survival");
    simulation.tick(8.0);
    require(simulation.snapshot().activeEnemyCount >= 9,
            "night builds pressure through regular reinforcements");
    const std::size_t activeBeforeDebugSpawn =
        simulation.snapshot().activeEnemyCount;
    ian::PlayerCommand spawnDebugEnemy;
    spawnDebugEnemy.spawnEnemy = ian::SpawnEnemyCommand{ian::EnemyType::Boss};
    simulation.tick(1.0 / 60.0, spawnDebugEnemy);
    require(simulation.snapshot().activeEnemyCount ==
                activeBeforeDebugSpawn + 1U,
            "debug command spawns selected enemy during wave");
    phaseEvents = simulation.takeEvents();

    ian::PlayerCommand defeatWave;
    defeatWave.defeatAllEnemies = ian::DefeatAllEnemiesCommand{};
    simulation.tick(1.0 / 60.0, defeatWave);
    require(simulation.snapshot().state == ian::RunState::WaveComplete,
            "cleared non-final wave enters dawn state");
    require(!simulation.snapshot().runUpgradeChoicePending &&
                simulation.snapshot().playerLevel == 1,
            "partial XP progress does not interrupt play with cards");
    require(
        !simulation.snapshot().coinPickups.empty() &&
            simulation.snapshot().coins == 0,
        "defeated enemies drop physical coins before collection");
    require(
        simulation.snapshot().lootChests.size() ==
            chestsBeforeFirstNight,
        "survived night does not create obsolete progression drops");
    const auto rewardChestCount = std::ranges::count_if(
        simulation.snapshot().lootChests,
        [](const ian::LootChestInstance& chest) {
            return chest.purpose ==
                ian::LootChestPurpose::Reward;
        });
    require(
        rewardChestCount == 0,
        "no obsolete post-wave reward chest is created");
    const double dawnDuration = simulation.snapshot().phaseDuration;
    requireNear(simulation.snapshot().phaseTimeRemaining, dawnDuration, 1e-12,
                "dawn starts with configured duration");
    require(simulation.snapshot().crystals == simulation.snapshot().waveCompletionReward,
            "wave completion grants crystals reward");
    require(simulation.snapshot().currentInsight > 0.0,
        "wave completion fills Insight");
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
    requireNear(
        simulation.snapshot().phaseTimeRemaining,
        dawnDuration - 0.5, 1e-12,
        "dawn timer continues when the XP bar is not full");
    simulation.tick(dawnDuration - 1.0);
    require(simulation.snapshot().state == ian::RunState::WaveComplete,
            "dawn remains active before timer expires");
    simulation.tick(1.0);
    require(simulation.snapshot().state == ian::RunState::BuildPhase,
            "dawn transitions to next build phase");
    requireNear(simulation.snapshot().phaseTimeRemaining,
                simulation.snapshot().phaseDuration, 1e-12,
                "new day receives full preparation timer");
    requireNear(simulation.snapshot().phaseDuration, 75.0, 1e-12,
                "later days give seventy-five seconds to explore and build");

    for (int expectedWave = 2; expectedWave < ian::Simulation::StageClearWave;
         ++expectedWave) {
        simulation.tick(1.0 / 60.0, startWaveEarly);
        require(
            simulation.snapshot().state ==
                    ian::RunState::Wave &&
                simulation.snapshot().wave == expectedWave,
            "infinite cycle starts the next numbered wave");
        requireNear(
            simulation.snapshot().phaseDuration,
            60.0,
            1e-12,
            "every regular night lasts sixty seconds");
        const auto& portals =
            simulation.snapshot().attackPortalPositions;
        for (std::size_t left = 0; left < portals.size(); ++left) {
            if (!portals[left]) continue;
            for (std::size_t right = left + 1U;
                 right < portals.size(); ++right) {
                if (!portals[right]) continue;
                require(
                    std::hypot(
                        portals[right]->x - portals[left]->x,
                        portals[right]->z - portals[left]->z) >= 5.19,
                    "simultaneous attack portals do not overlap");
            }
        }
        const int crystalsBeforeReward =
            simulation.snapshot().crystals;
        simulation.tick(1.0 / 60.0, defeatWave);
        require(
            simulation.snapshot().state ==
                ian::RunState::WaveComplete,
            "every cleared wave returns to dawn without victory");
        require(
            simulation.snapshot().crystals ==
                crystalsBeforeReward + 15 + 6 * expectedWave,
            "endless wave reward follows the balanced base plus wave curve");
        if (expectedWave < ian::Simulation::StageClearWave - 1) {
            while (simulation.snapshot().runUpgradeChoicePending) {
                require(simulation.selectRunUpgrade(0U),
                        "each earned level accepts every queued card choice");
            }
            require(simulation.closeRunShop(), "wave fixture leaves the optional shop");
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

    while (simulation.snapshot().runUpgradeChoicePending) {
        require(simulation.selectRunUpgrade(0U),
                "last pre-clear level accepts earned upgrades");
    }
    require(simulation.closeRunShop(), "pre-clear fixture leaves the shop");
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
            !simulation.snapshot().finalNight,
        "stage boss opens the bank-or-final-night choice");
    while (simulation.snapshot().runUpgradeChoicePending) {
        require(simulation.selectRunUpgrade(0U),
                "stage-clear XP upgrades remain selectable");
    }
    require(simulation.closeRunShop(), "stage-clear fixture leaves the shop");
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
            simulation.snapshot().wave == ian::Simulation::StageClearWave + 2,
        "final night chains waves while preserving earned level-ups");
    while (simulation.snapshot().runUpgradeChoicePending) {
        require(simulation.selectRunUpgrade(0U),
                "final-night XP upgrades remain selectable");
    }
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

    {
        ian::Simulation debugUpgradeRun;
        debugUpgradeRun.startRun();
        require(
            debugUpgradeRun.grantDebugRunUpgrade(
                ian::RunUpgradeEffect::GunPiercing) &&
            debugUpgradeRun.snapshot().runUpgradeStacks[
                ian::runUpgradeIndex(
                    ian::RunUpgradeEffect::GunPiercing)] == 1,
            "debug menu can grant run upgrades outside sandbox mode");
    }

    {
        ian::Simulation longerDay;
        longerDay.startRun();
        const auto before = longerDay.snapshot();
        require(longerDay.grantDebugRunUpgrade(
                    ian::RunUpgradeEffect::LongerDay),
                "long-day card can be granted");
        const auto after = longerDay.snapshot();
        requireNear(after.phaseTimeRemaining,
                    before.phaseTimeRemaining + 5.0, 1e-9,
                    "long-day card extends the active build phase");
        requireNear(after.phaseDuration,
                    before.phaseDuration + 5.0, 1e-9,
                    "long-day card extends the displayed day timer");
    }

    {
        ian::Simulation baseGathering;
        ian::Simulation hastyGathering;
        baseGathering.startRun();
        hastyGathering.startRun();
        require(hastyGathering.grantDebugRunUpgrade(ian::RunUpgradeEffect::AttackSpeed),
                "combat haste coexists without multiplying tool speed");
        require(hastyGathering.grantDebugRunUpgrade(
                    ian::RunUpgradeEffect::GatheringSpeed),
                "gathering tome can be granted");
        ian::PlayerCommand selectAxe;
        selectAxe.selectWeapon =
            ian::SelectWeaponCommand{ian::PlayerWeapon::Axe};
        selectAxe.overrideAimedResource = true;
        baseGathering.tick(0.0, selectAxe);
        hastyGathering.tick(0.0, selectAxe);
        ian::PlayerCommand baseSwing;
        baseSwing.overrideAimedResource = true;
        baseSwing.usePickaxe = true;
        baseGathering.tick(0.0, baseSwing);
        hastyGathering.tick(0.0, baseSwing);
        requireNear(
            hastyGathering.snapshot().pickaxeCooldownRemaining * 1.15,
            baseGathering.snapshot().pickaxeCooldownRemaining,
            1e-9,
            "gathering tome accelerates the axe itself by 15 percent");
    }

    {
        ian::Simulation shardRun;
        shardRun.startSandboxRun();
        ian::PlayerCommand shardPlaceCore;
        shardPlaceCore.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::Core,
            .gridPosition = {0, 0},
        };
        shardRun.tick(1.0 / 60.0, shardPlaceCore);
        ian::PlayerCommand shardPlaceTurret;
        shardPlaceTurret.placeBuilding = ian::PlaceBuildingCommand{
            .type = ian::BuildingType::GunTurret,
            .gridPosition = {0, -4},
        };
        shardRun.tick(1.0 / 60.0, shardPlaceTurret);
        require(
            std::ranges::any_of(
                shardRun.snapshot().buildings,
                [](const ian::BuildingInstance& building) {
                    return building.type ==
                        ian::BuildingType::GunTurret;
                }),
            "shrapnel fixture places a gun turret");
        require(shardRun.grantDebugRunUpgrade(
                    ian::RunUpgradeEffect::GunShrapnel),
                "shrapnel card activates in a sandbox run");
        ian::PlayerCommand spawnCluster;
        spawnCluster.spawnEnemy = ian::SpawnEnemyCommand{
            .type = ian::EnemyType::Basic,
            .count = 80,
            .eliteAffixes = ian::eliteAffixMask(
                ian::EliteAffix::Berserker),
        };
        shardRun.tick(1.0 / 60.0, spawnCluster);
        static_cast<void>(shardRun.takeEvents());
        bool sawShard = false;
        bool sawAutomaticTurretCoins = false;
        for (int tick = 0;
             tick < 900 && (!sawShard || !sawAutomaticTurretCoins);
             ++tick) {
            resolveLevelUps(shardRun);
            const double shotDamage =
                ian::TowerSystem::attackDamage(ian::BuildingType::GunTurret, 1) *
                shardRun.snapshot().upgradeStats.defenseDamage;
            shardRun.tick(1.0 / 30.0, {});
            for (const ian::GameEvent& event :
                 shardRun.takeEvents()) {
                if (event.type == ian::GameEventType::ProjectileHit &&
                    event.buildingType == ian::BuildingType::GunTurret) {
                    requireNear(event.projectileDamage,
                        shotDamage * (event.amount < 0 ? 0.45 : 1.0), 1e-9,
                        "main shots and shrapnel carry their outgoing damage for presentation");
                }
                if (event.type == ian::GameEventType::EnemyKilled &&
                    event.buildingType == ian::BuildingType::GunTurret) {
                    require(event.entityId && event.sourceId &&
                                event.enemyType == ian::EnemyType::Basic &&
                                (event.enemyEliteAffixes & ian::eliteAffixMask(
                                    ian::EliteAffix::Berserker)) != 0,
                        "direct and shrapnel kills retain enemy and defense attribution");
                }
                sawShard = sawShard ||
                    (event.type ==
                         ian::GameEventType::ProjectileHit &&
                     event.buildingType ==
                         ian::BuildingType::GunTurret &&
                     event.secondaryImpact &&
                     event.amount < 0 &&
                     event.targetPosition.has_value());
                sawAutomaticTurretCoins =
                    sawAutomaticTurretCoins ||
                    (event.type ==
                         ian::GameEventType::CoinCollected &&
                     event.amount > 0);
            }
        }
        require(sawShard,
                "gun turret kills launch visible damaging shards");
        require(sawAutomaticTurretCoins,
                "turret kills collect their coin drops automatically");
    }
}
