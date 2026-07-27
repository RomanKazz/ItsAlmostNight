#include "TestHarness.hpp"
#include "buildings/BuildingSystem.hpp"

#include <numbers>

void runBuildingSystemTests() {
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

    const auto overlappingWall = buildings.validate(ian::BuildingType::Wall, {1, 0}, 10, 0);
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

    const auto damagedWall = buildings.damage(wall->building.id, 40.0);
    require(damagedWall.has_value() && damagedWall->remainingHealth == 60.0 &&
                damagedWall->gridPosition == ian::GridPosition{4, 0},
            "building damage reduces health");
    const auto destroyedWall = buildings.damage(wall->building.id, 60.0);
    require(destroyedWall.has_value() && destroyedWall->destroyed,
            "lethal building damage destroys instance");
    require(buildings.buildings().size() == 1, "destroyed building leaves active list");

    const auto turretCost = ian::buildingCost(ian::BuildingType::Turret);
    require(turretCost.wood == 25 && turretCost.stone == 15,
            "turret has configured mixed resource cost");
    require(buildings.place(ian::BuildingType::Turret, {4, 1}, 0, 25, 15).has_value(),
            "turret follows normal core-area placement rules");

    const auto poorUpgrade = buildings.validateUpgrade(core->building.id, 0, 0, 49);
    require(poorUpgrade.error == ian::UpgradeError::InsufficientResources,
            "core upgrade requires gold");
    const auto levelTwo = buildings.upgrade(core->building.id, 0, 0, 50);
    require(levelTwo.valid() && levelTwo.building->level == 2,
            "first core upgrade reaches level two");
    require(levelTwo.building->maxHealth == 750.0, "core upgrade increases max health");
    const auto levelThree = buildings.upgrade(core->building.id, 0, 0, 100);
    require(levelThree.valid() && levelThree.building->level == 3,
            "second core upgrade reaches level three");
    require(buildings.validateUpgrade(core->building.id, 0, 0, 100).error ==
                ian::UpgradeError::MaxLevel,
            "core cannot exceed level three");

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
    const auto cannonLevelTwo =
        cannonBuildings.upgrade(cannon->building.id, 20, 15, 23);
    require(cannonLevelTwo.valid() && cannonLevelTwo.building->level == 2 &&
                cannonLevelTwo.building->maxHealth == 270.0,
            "building upgrade raises level and max health");
    require(cannonBuildings.validateUpgrade(cannon->building.id, 40, 30, 50).error ==
                ian::UpgradeError::CoreLevelRequired,
            "building level cannot exceed core level");
    cannonBuildings.upgrade(cannonCore->building.id, 0, 0, 100);
    const auto cannonLevelThree =
        cannonBuildings.upgrade(cannon->building.id, 40, 30, 50);
    require(cannonLevelThree.valid() && cannonLevelThree.building->level == 3 &&
                cannonLevelThree.building->maxHealth == 360.0,
            "core level three unlocks final building upgrade");
    require(cannonBuildings.place(ian::BuildingType::SlowTrap, {4, 2}, 0, 15, 20, 10).has_value(),
            "core level two unlocks slow trap");
    require(!ian::buildingBlocksMovement(ian::BuildingType::SlowTrap),
            "floor trap does not block movement");
    const auto gate =
        cannonBuildings.place(ian::BuildingType::Gate, {4, 4}, 0, 15, 5);
    require(gate.has_value() && ian::buildingBlocksMovement(gate->building),
            "new gate starts closed");
    const auto aimedGate =
        cannonBuildings.raycast({4.0, 1.0, 7.0}, {0.0, 0.0, -1.0}, 4.0);
    require(aimedGate.has_value() && *aimedGate == gate->building.id,
            "building raycast selects gate under crosshair");
    const auto openedGate = cannonBuildings.toggleGate(gate->building.id);
    require(openedGate.has_value() && openedGate->open &&
                !ian::buildingBlocksMovement(*openedGate),
            "open gate stops blocking movement");
    const auto damagedGate = cannonBuildings.damage(gate->building.id, 65.0);
    require(damagedGate.has_value(), "gate can be damaged before repair");
    const auto repairCost = ian::buildingRepairCost(cannonBuildings.buildings().back());
    require(repairCost.wood == 4 && repairCost.stone == 2,
            "repair cost scales with missing health");
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
    require(cannonBuildings.sell(gate->building.id).error ==
                ian::BuildingActionError::NotFound,
            "sold building no longer exists");
    require(cannonBuildings.sell(cannonCore->building.id).error ==
                ian::BuildingActionError::Unsupported,
            "core cannot be sold");

    require(buildings.place(ian::BuildingType::GoldMine, {4, 3}, 0, 20, 10).has_value(),
            "first gold mine placement succeeds");
    require(buildings.place(ian::BuildingType::GoldMine, {4, 5}, 0, 20, 10).has_value(),
            "second gold mine placement succeeds");
    require(buildings.place(ian::BuildingType::GoldMine, {6, 3}, 0, 20, 10).has_value(),
            "third gold mine placement succeeds");
    require(buildings.place(ian::BuildingType::GoldMine, {6, 5}, 0, 20, 10).has_value(),
            "fourth gold mine placement succeeds");
    require(buildings.validate(ian::BuildingType::GoldMine, {8, 3}, 20, 10).error ==
                ian::PlacementError::LimitReached,
            "gold mine limit is four");

    const ian::Vec3 player{0.0, 1.7, 6.0};
    const auto closeAim =
        ian::aimedBuildingGridPosition(player, 0.0, -std::numbers::pi / 4.0);
    require(closeAim == ian::GridPosition{0, 4},
            "looking down places building close to player");
    const auto farAim = ian::aimedBuildingGridPosition(player, 0.0, -0.01);
    require(farAim == ian::GridPosition{0, -4},
            "shallow aim clamps placement to maximum distance");
    const auto rightAim = ian::aimedBuildingGridPosition(
        player, std::numbers::pi / 2.0, -std::numbers::pi / 4.0);
    require(rightAim == ian::GridPosition{2, 6}, "yaw directs placement around player");

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
}
