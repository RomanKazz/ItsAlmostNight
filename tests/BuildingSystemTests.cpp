#include "TestHarness.hpp"
#include "buildings/BuildingHotbarLayout.hpp"
#include "buildings/BuildingSystem.hpp"

#include <numbers>
#include <ranges>

void runBuildingSystemTests() {
    std::array<bool, ian::GameBalance::BuildingTypeCount> unlocked{};
    unlocked[static_cast<std::size_t>(ian::BuildingType::Core)] = true;
    unlocked[static_cast<std::size_t>(ian::BuildingType::Wall)] = true;
    unlocked[static_cast<std::size_t>(ian::BuildingType::GunTurret)] = true;
    auto hotbar = ian::makeBuildingHotbarLayout(unlocked);
    require(
        hotbar.count == ian::GameBalance::BuildingTypeCount &&
            hotbar.types[0] == ian::BuildingType::Core &&
            hotbar.types[1] == ian::BuildingType::Wall &&
            hotbar.types[2] == ian::BuildingType::GunTurret &&
            hotbar.types[3] == ian::BuildingType::Turret,
        "locked buildings keep permanent visible slots");
    unlocked[static_cast<std::size_t>(ian::BuildingType::Turret)] = true;
    hotbar = ian::makeBuildingHotbarLayout(unlocked);
    require(
        hotbar.count == ian::GameBalance::BuildingTypeCount &&
            hotbar.types[3] == ian::BuildingType::Turret,
        "unlocking a building does not move hotbar slots");

    ian::BuildingSystem radiusBuildings;
    radiusBuildings.setCoreBuildRadius(5);
    require(radiusBuildings.coreBuildRadius() == 5,
            "core build radius can be upgraded at runtime");

    ian::BuildingSystem starterDefense;
    const auto starterCore = starterDefense.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(
        starterCore.has_value() &&
            starterDefense.validate(
                ian::BuildingType::GunTurret, {4, 0},
                35, 25, 10).valid(),
        "starter turret is buildable with a level-one core");

    ian::BuildingSystem buildings;

    const auto poorCore = buildings.validate(ian::BuildingType::Core, {0, 0}, 29, 0);
    require(poorCore.error == ian::PlacementError::InsufficientResources,
            "core requires configured resources");

    const auto wallBeforeCore = buildings.validate(ian::BuildingType::Wall, {4, 0}, 10, 0);
    require(wallBeforeCore.error == ian::PlacementError::CoreRequired,
            "wall requires placed core");

    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "valid core placement succeeds");
    require(core->cost.wood == 30, "core reports spent resources");

    const auto secondCore = buildings.validate(ian::BuildingType::Core, {6, 0}, 30, 0);
    require(secondCore.error == ian::PlacementError::CoreAlreadyPlaced,
            "only one core can be placed");

    const auto overlappingWall =
        buildings.validate(ian::BuildingType::Wall, {0, 0}, 10, 0);
    require(overlappingWall.error == ian::PlacementError::Occupied,
            "wall cannot overlap core footprint");

    const auto distantWall = buildings.validate(ian::BuildingType::Wall, {13, 0}, 10, 0);
    require(distantWall.error == ian::PlacementError::OutsideCoreArea,
            "wall stays inside core build radius");

    const auto wall = buildings.place(ian::BuildingType::Wall, {4, 0}, 3, 10, 0);
    require(wall.has_value(), "valid wall placement succeeds");
    require(wall->building.rotation == 3, "building stores grid rotation");

    const auto occupiedWall = buildings.validate(ian::BuildingType::Wall, {4, 0}, 10, 0);
    require(occupiedWall.error == ian::PlacementError::Occupied,
            "buildings cannot share occupied cell");
    const auto poorOccupiedWall =
        buildings.validate(
            ian::BuildingType::Wall, {4, 0}, 0, 0);
    require(
        poorOccupiedWall.error ==
            ian::PlacementError::Occupied,
        "occupied preview takes priority over resource shortage");

    const auto damagedWall = buildings.damage(wall->building.id, 40.0);
    require(damagedWall.has_value() && damagedWall->remainingHealth == 60.0 &&
                damagedWall->gridPosition == ian::GridPosition{4, 0},
            "building damage reduces health");
    requireNear(
        buildings.restoreHealthFraction(0.15), 15.0, 1e-12,
        "field repair restores fifteen percent of max health");
    const auto restoredWall = std::ranges::find(
        buildings.buildings(), wall->building.id,
        &ian::BuildingInstance::id);
    require(
        restoredWall != buildings.buildings().end() &&
            restoredWall->health == 75.0,
        "field repair updates damaged building health");
    const auto destroyedWall = buildings.damage(wall->building.id, 75.0);
    require(destroyedWall.has_value() && destroyedWall->destroyed,
            "lethal building damage destroys instance");
    require(buildings.buildings().size() == 1, "destroyed building leaves active list");

    const auto turretCost = ian::buildingCost(ian::BuildingType::Turret);
    require(turretCost.wood == 25 && turretCost.stone == 15,
            "turret has configured mixed resource cost");
    const auto turret = buildings.place(
        ian::BuildingType::Turret, {4, 1}, 0, 25, 15);
    require(turret.has_value(),
            "turret follows normal core-area placement rules");
    const auto rotatedTurret =
        buildings.rotateDirectionalDefense(
            turret->building.id, -1);
    require(
        rotatedTurret.has_value() &&
            rotatedTurret->rotation == 7,
        "directional defenses rotate and wrap across eight steps");
    require(
        !buildings.rotateDirectionalDefense(
            core->building.id, 1),
        "non-directional buildings reject post-placement rotation");

    const auto poorUpgrade = buildings.validateUpgrade(core->building.id, 0, 0, 49);
    require(poorUpgrade.error == ian::UpgradeError::InsufficientResources,
            "core upgrade requires crystals");
    require(
        ian::coreResourceCapacity(
            ian::BuildingType::CrystalStorage,
            core->building.level) >=
            buildings.upgradeCost(core->building).crystals,
        "level-one core can store its next upgrade cost");
    const auto levelTwo = buildings.upgrade(core->building.id, 0, 0, 50);
    require(levelTwo.valid() && levelTwo.building->level == 2,
            "first core upgrade reaches level two");
    require(levelTwo.building->maxHealth == 575.0, "core upgrade increases max health");
    require(
        ian::coreResourceCapacity(
            ian::BuildingType::CrystalStorage,
            levelTwo.building->level) >=
            buildings.upgradeCost(*levelTwo.building).crystals,
        "level-two core can store its next upgrade cost");
    const auto levelThree = buildings.upgrade(core->building.id, 0, 0, 100);
    require(levelThree.valid() && levelThree.building->level == 3,
            "second core upgrade reaches level three");
    require(buildings.validateUpgrade(core->building.id, 0, 0, 149).error ==
                ian::UpgradeError::InsufficientResources,
            "higher core levels use progressive upgrade costs");
    for (int expectedLevel = 4;
         expectedLevel <= ian::MaxBuildingLevel;
         ++expectedLevel) {
        const auto currentCore = buildings.core();
        require(currentCore.has_value(),
                "core remains available through progression");
        const auto cost = buildings.upgradeCost(*currentCore);
        require(
            ian::coreResourceCapacity(
                ian::BuildingType::WoodStorage,
                currentCore->level) >= cost.wood &&
                ian::coreResourceCapacity(
                    ian::BuildingType::StoneStorage,
                    currentCore->level) >= cost.stone &&
                ian::coreResourceCapacity(
                    ian::BuildingType::CrystalStorage,
                    currentCore->level) >= cost.crystals,
            "every core level can store its next upgrade cost");
        const auto upgraded = buildings.upgrade(
            currentCore->id, cost.wood, cost.stone, cost.crystals);
        require(
            upgraded.valid() &&
                upgraded.building->level == expectedLevel,
            "core progresses through extended building levels");
    }
    require(
        buildings.validateUpgrade(
            core->building.id, 10000, 10000, 10000)
                .error == ian::UpgradeError::MaxLevel,
        "core stops at extended maximum level");

    ian::BuildingSystem limitedDefense;
    const auto limitedCore = limitedDefense.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(limitedCore.has_value(),
            "defense limit fixture creates core");
    require(
        limitedDefense.place(
            ian::BuildingType::Turret, {-4, -4}, 0,
            25, 15).has_value() &&
            limitedDefense.place(
                ian::BuildingType::Turret, {-4, 0}, 0,
                25, 15).has_value() &&
            limitedDefense.place(
                ian::BuildingType::Turret, {-4, 4}, 0,
                25, 15).has_value(),
        "core level one allows three defenses");
    require(
        limitedDefense.validate(
            ian::BuildingType::Turret, {0, -4}, 25, 15)
                .error == ian::PlacementError::LimitReached,
        "turrets share the core defense limit");
    require(
        limitedDefense.upgrade(
            limitedCore->building.id, 0, 0, 50).valid() &&
            limitedDefense.place(
                ian::BuildingType::Cannon, {0, -4}, 0,
                40, 30, 25).has_value(),
        "upgrading the core expands the shared defense limit");
    require(
        limitedDefense.placementCount(
            ian::BuildingType::SlowTrap) == 4 &&
            limitedDefense.placementLimit(
                ian::BuildingType::SlowTrap) == 5,
        "all active defense types report the same shared capacity");

    ian::BuildingSystem cannonBuildings;
    const auto cannonCore =
        cannonBuildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(cannonCore.has_value(), "cannon fixture creates core");
    require(cannonBuildings.validate(ian::BuildingType::Cannon, {4, 0}, 40, 30, 25).error ==
                ian::PlacementError::CoreLevelRequired,
            "cannon requires core level two");
    cannonBuildings.upgrade(cannonCore->building.id, 0, 0, 50);
    const auto cannon =
        cannonBuildings.place(ian::BuildingType::Cannon, {4, 0}, 0, 40, 30, 25);
    require(cannon.has_value(), "core level two unlocks cannon");
    require(
        cannonBuildings.upgradeCost(cannon->building) ==
            ian::buildingUpgradeCost(cannon->building),
        "default upgrade helpers share one pricing formula");
    require(
        cannonBuildings.validateUpgrade(
            cannon->building.id, 1000, 1000, 1000).error ==
            ian::UpgradeError::Unsupported,
        "directional defenses cannot be upgraded individually");
    const ian::ResourceCost levelTwoCost =
        cannonBuildings.blueprintUpgradeCost(
            ian::BuildingType::Cannon);
    const auto cannonLevelTwo = cannonBuildings.upgradeBlueprint(
        ian::BuildingType::Cannon,
        levelTwoCost.wood, levelTwoCost.stone,
        levelTwoCost.crystals);
    require(cannonLevelTwo.valid() && cannonLevelTwo.level == 2 &&
                cannonLevelTwo.upgradedBuildingCount == 1,
            "blueprint upgrade raises all existing defenses");
    const auto upgradedCannon = std::find_if(
        cannonBuildings.buildings().begin(),
        cannonBuildings.buildings().end(),
        [&cannon](const ian::BuildingInstance& building) {
            return building.id == cannon->building.id;
        });
    require(upgradedCannon != cannonBuildings.buildings().end(),
            "upgraded cannon remains in the world");
    requireNear(upgradedCannon->maxHealth, 207.0, 1e-9,
                "blueprint upgrade gives gradual health increase");
    require(cannonBuildings.validateBlueprintUpgrade(
                ian::BuildingType::Cannon, 1000, 1000, 1000).error ==
                ian::UpgradeError::CoreLevelRequired,
            "blueprint level cannot exceed core level");
    cannonBuildings.upgrade(cannonCore->building.id, 0, 0, 100);
    const ian::ResourceCost levelThreeCost =
        cannonBuildings.blueprintUpgradeCost(
            ian::BuildingType::Cannon);
    const auto cannonLevelThree = cannonBuildings.upgradeBlueprint(
        ian::BuildingType::Cannon,
        levelThreeCost.wood, levelThreeCost.stone,
        levelThreeCost.crystals);
    require(cannonLevelThree.valid() && cannonLevelThree.level == 3,
            "core level three unlocks next building upgrade");
    const auto levelThreeCannon = std::find_if(
        cannonBuildings.buildings().begin(),
        cannonBuildings.buildings().end(),
        [&cannon](const ian::BuildingInstance& building) {
            return building.id == cannon->building.id;
        });
    requireNear(levelThreeCannon->maxHealth, 234.0, 1e-9,
                "next building level keeps gradual health curve");
    require(
        cannonBuildings.configuredCost(ian::BuildingType::Cannon).wood >
            ian::buildingCost(ian::BuildingType::Cannon).wood,
        "higher blueprint levels increase future construction cost");
    const auto secondCannon = cannonBuildings.place(
        ian::BuildingType::Cannon, {4, -2}, 0, 1000, 1000, 1000);
    require(secondCannon && secondCannon->building.level == 3,
            "new defenses inherit the current blueprint level");
    require(cannonBuildings.place(ian::BuildingType::SlowTrap, {4, 2}, 0, 15, 20, 10).has_value(),
            "core level two unlocks slow trap");
    require(!ian::buildingBlocksMovement(ian::BuildingType::SlowTrap),
            "floor trap does not block movement");
    const auto gate =
        cannonBuildings.place(ian::BuildingType::Gate, {4, 4}, 0, 15, 5);
    require(gate.has_value() && ian::buildingBlocksMovement(gate->building),
            "new gate starts closed");
    const auto aimedGate =
        cannonBuildings.raycast(
            {4.5, 1.0, 7.0},
            {0.0, 0.0, -1.0}, 4.0);
    require(aimedGate.has_value() && *aimedGate == gate->building.id,
            "building raycast selects gate under crosshair");

    ian::BuildingSystem selectionBuildings;
    require(
        selectionBuildings
            .place(ian::BuildingType::Core, {0, 0}, 0,
                   30, 0)
            .has_value(),
        "selection fixture creates core");
    const auto selectedWall = selectionBuildings.place(
        ian::BuildingType::Wall, {4, 4}, 0, 10, 0);
    const auto diagonalWall = selectionBuildings.place(
        ian::BuildingType::Wall, {5, 5}, 0, 10, 0);
    require(
        selectedWall.has_value() &&
            diagonalWall.has_value(),
        "selection fixture creates neighboring walls");
    const auto preciseWallAim = selectionBuildings.raycast(
        {4.5, 1.0, 7.5}, {0.0, 0.0, -1.0}, 4.0);
    require(
        preciseWallAim.has_value() &&
            *preciseWallAim == selectedWall->building.id,
        "neighboring building does not steal precise aim");

    ian::BuildingSystem elevatedBuildings;
    require(
        elevatedBuildings
            .place(ian::BuildingType::Core, {0, 0}, 0,
                   30, 0)
            .has_value(),
        "elevated fixture creates ground core");
    const auto elevatedWall = elevatedBuildings.place(
        ian::BuildingType::Wall, {0, 0}, 0,
        10, 0, 0, 4.0, 0, 4.0);
    require(
        elevatedWall.has_value() &&
            elevatedWall->building.baseHeight == 4.0 &&
            elevatedWall->building.platformStorey == 0,
        "building can occupy the same grid cell on a platform level");
    const auto elevatedAim = elevatedBuildings.raycast(
        {0.5, 5.0, 3.0}, {0.0, 0.0, -1.0},
        4.0);
    require(
        elevatedAim == elevatedWall->building.id,
        "raycast uses elevated building base height");

    const auto openedGate = cannonBuildings.toggleGate(gate->building.id);
    require(openedGate.has_value() && openedGate->open &&
                !ian::buildingBlocksMovement(*openedGate),
            "open gate stops blocking movement");
    const auto damagedGate = cannonBuildings.damage(gate->building.id, 65.0);
    require(damagedGate.has_value(), "gate can be damaged before repair");
    const auto repairCost = ian::buildingRepairCost(cannonBuildings.buildings().back());
    require(repairCost.wood == 4 && repairCost.stone == 2,
            "repair cost scales with missing health");
    const auto repairValidation =
        cannonBuildings.validateRepair(
            gate->building.id, 4, 2, 0);
    require(
        repairValidation.cost == repairCost,
        "default repair helpers share one pricing formula");
    require(cannonBuildings.validateRepair(gate->building.id, 3, 2, 0).error ==
                ian::BuildingActionError::InsufficientResources,
            "repair validates available resources");
    const auto repairedGate = cannonBuildings.repair(gate->building.id, 4, 2, 0);
    require(repairedGate.valid() && repairedGate.building->health == 130.0 &&
                repairedGate.repairedHealth == 65.0,
            "repair restores full building health");
    require(cannonBuildings.validateRepair(gate->building.id, 4, 2, 0).error ==
                ian::BuildingActionError::FullHealth,
            "full-health building cannot consume repair resources");
    const auto soldGate = cannonBuildings.sell(gate->building.id);
    require(soldGate.valid() && soldGate.refund.wood == 7 && soldGate.refund.stone == 2,
            "selling returns half base cost");
    require(
        soldGate.refund ==
            ian::buildingSellRefund(*soldGate.building),
        "default sell helpers share one pricing formula");
    require(cannonBuildings.sell(gate->building.id).error ==
                ian::BuildingActionError::NotFound,
            "sold building no longer exists");
    require(cannonBuildings.sell(cannonCore->building.id).error ==
                ian::BuildingActionError::Unsupported,
            "core cannot be sold");

    require(buildings.place(ian::BuildingType::CrystalMine, {4, 3}, 0, 20, 10).has_value(),
            "first crystals mine placement succeeds");
    require(buildings.place(ian::BuildingType::CrystalMine, {4, 5}, 0, 20, 10).has_value(),
            "second crystals mine placement succeeds");
    require(buildings.place(ian::BuildingType::CrystalMine, {6, 3}, 0, 20, 10).has_value(),
            "third crystals mine placement succeeds");
    require(buildings.place(ian::BuildingType::CrystalMine, {6, 5}, 0, 20, 10).has_value(),
            "fourth crystals mine placement succeeds");
    require(buildings.place(ian::BuildingType::CrystalMine, {8, 3}, 0, 20, 10).has_value() &&
                buildings.place(ian::BuildingType::CrystalMine, {8, 5}, 0, 20, 10).has_value() &&
                buildings.place(ian::BuildingType::CrystalMine, {10, 3}, 0, 20, 10).has_value() &&
                buildings.place(ian::BuildingType::CrystalMine, {10, 5}, 0, 20, 10).has_value(),
            "maximum core level allows eight mines of each type");
    require(buildings.validate(ian::BuildingType::CrystalMine, {8, 7}, 20, 10).error ==
                ian::PlacementError::LimitReached,
            "producer limit follows maximum core level");

    const ian::Vec3 player{0.0, 1.7, 6.0};
    const auto closeAim =
        ian::aimedBuildingGridPosition(player, 0.0, -std::numbers::pi / 4.0);
    require(closeAim == ian::GridPosition{1, 5},
            "two-cell building snaps to PlatformFrame center");
    const auto farAim = ian::aimedBuildingGridPosition(player, 0.0, -0.01);
    require(farAim == ian::GridPosition{1, -3},
            "shallow aim clamps placement to maximum distance");
    const auto rightAim = ian::aimedBuildingGridPosition(
        player, std::numbers::pi / 2.0, -std::numbers::pi / 4.0);
    require(
        rightAim == ian::GridPosition{1, 7},
        "yaw keeps two-cell placement on PlatformFrame lattice");
    const auto wallAim = ian::aimedBuildingGridPosition(
        player, std::numbers::pi / 2.0,
        -std::numbers::pi / 4.0,
        ian::MinimumPlacementDistance,
        ian::MaximumPlacementDistance,
        ian::BuildingType::Wall);
    require(wallAim == ian::GridPosition{1, 6},
        "one-cell building uses containing grid cell");

    const auto elevatedPlaneAim =
        ian::aimedBuildingGridPosition(
            {0.0, 1.7, 0.0}, 0.0,
            std::atan2(2.3, 6.0),
            1.0, 10.0,
            ian::BuildingType::Wall, 4.0);
    require(
        elevatedPlaneAim == ian::GridPosition{0, -6},
        "building drag ray intersects an elevated plane while aiming upward");
    const ian::Vec3 wallCenter = ian::buildingWorldPosition(
        ian::BuildingType::Wall, wallAim);
    require(
        wallCenter.x == 1.5 && wallCenter.z == 6.5,
        "one-cell building is centered inside grid cell");
    const ian::Vec3 coreCenter = ian::buildingWorldPosition(
        ian::BuildingType::Core, rightAim);
    require(
        coreCenter.x == 1.0 && coreCenter.z == 7.0 &&
            (rightAim.x - 1) % 2 == 0 &&
            (rightAim.z - 1) % 2 == 0,
        "two-cell core center matches an even PlatformFrame anchor");
    require(
        ian::buildingFootprintHalfExtent(
            ian::BuildingType::CrystalMine) == 0.5 &&
            ian::buildingFootprintHalfExtent(
                ian::BuildingType::LumberMill) == 0.5 &&
            ian::buildingFootprintHalfExtent(
                ian::BuildingType::Quarry) == 0.5 &&
            ian::buildingFootprintHalfExtent(
                ian::BuildingType::Turret) == 1.0 &&
            ian::buildingFootprintHalfExtent(
                ian::BuildingType::Cannon) == 1.0 &&
            ian::buildingFootprintHalfExtent(
                ian::BuildingType::WoodStorage) == 1.0 &&
            ian::buildingFootprintHalfExtent(
                ian::BuildingType::StoneStorage) == 1.0 &&
            ian::buildingFootprintHalfExtent(
                ian::BuildingType::CrystalStorage) == 1.0,
        "producers use one-cell footprints and storages use two-by-two footprints");
    const ian::Vec3 turretCenter =
        ian::buildingWorldPosition(
            ian::BuildingType::Turret, {2, 6});
    require(
        turretCenter.x == 2.0 && turretCenter.z == 6.0,
        "two-cell combat building remains centered on grid intersection");

    std::vector<ian::BuildingInstance> wallModules{
        {.id = {5000, 1}, .type = ian::BuildingType::Wall, .gridPosition = {0, 0}},
        {.id = {5001, 1}, .type = ian::BuildingType::Wall, .gridPosition = {0, -1}},
        {.id = {5002, 1},
         .type = ian::BuildingType::Gate,
         .gridPosition = {1, 0},
         .rotation = 0},
        {.id = {5003, 1},
         .type = ian::BuildingType::Gate,
         .gridPosition = {0, 1},
         .rotation = 0},
        {.id = {5004, 1}, .type = ian::BuildingType::Wall, .gridPosition = {-1, 0}},
        {.id = {5005, 1}, .type = ian::BuildingType::Wall, .gridPosition = {0, 2}},
    };
    const auto firstMask = ian::wallConnectionMask(wallModules, {0, 0});
    const auto expectedFirstMask = static_cast<std::uint8_t>(
        ian::WallConnectionNorth | ian::WallConnectionEast | ian::WallConnectionWest);
    require(firstMask == expectedFirstMask,
            "wall mask includes adjacent walls and aligned gate");
    wallModules[3].rotation = 1;
    const auto crossMask = ian::wallConnectionMask(wallModules, {0, 0});
    require(crossMask == static_cast<std::uint8_t>(
                             ian::WallConnectionNorth | ian::WallConnectionEast |
                             ian::WallConnectionSouth | ian::WallConnectionWest),
            "rotated gate completes four-way wall connection");

    const std::vector<ian::BuildingInstance> brokenHorizontalLine{
        {.id = {5100, 1},
         .type = ian::BuildingType::Wall,
         .gridPosition = {0, 0},
         .rotation = 1},
        {.id = {5101, 1},
         .type = ian::BuildingType::Wall,
         .gridPosition = {-3, 0}},
        {.id = {5102, 1},
         .type = ian::BuildingType::Wall,
         .gridPosition = {4, 0}},
    };
    require(
        ian::wallFallbackRotation(
            brokenHorizontalLine,
            brokenHorizontalLine.front()) == 0,
        "isolated wall keeps horizontal line orientation across gaps");

    const std::vector<ian::BuildingInstance> brokenVerticalLine{
        {.id = {5200, 1},
         .type = ian::BuildingType::Wall,
         .gridPosition = {0, 0},
         .rotation = 0},
        {.id = {5201, 1},
         .type = ian::BuildingType::Wall,
         .gridPosition = {0, -4}},
    };
    require(
        ian::wallFallbackRotation(
            brokenVerticalLine,
            brokenVerticalLine.front()) == 1,
        "isolated wall keeps vertical line orientation across gaps");
}
