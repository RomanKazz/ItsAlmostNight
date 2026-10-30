#include "TestHarness.hpp"
#include <limits>
#include "buildings/BuildingHotbarLayout.hpp"
#include "buildings/BuildingSystem.hpp"
#include "game/BuildingDestructionVisual.hpp"

#include <numbers>
#include <ranges>

void runBuildingSystemTests() {
    {
        std::array<bool, ian::GameBalance::BuildingTypeCount> allUnlocked;
        allUnlocked.fill(true);
        const auto creativeDefense = ian::makeBuildingHotbarLayout(
            allUnlocked, ian::BuildingHotbarCategory::Defense,
            true, 1, true);
        require(
            creativeDefense.count == 14U,
            "creative catalog exposes every defense structure");
        const auto creativeEconomy = ian::makeBuildingHotbarLayout(
            allUnlocked, ian::BuildingHotbarCategory::Economy,
            true, 1, true);
        const auto creativeConstruction =
            ian::makeConstructionHotbarLayout(
                allUnlocked, true, 1, true);
        const auto levelOneConstruction =
            ian::makeConstructionHotbarLayout(
                allUnlocked, true, 1, false);
        const auto levelTwoConstruction =
            ian::makeConstructionHotbarLayout(
                allUnlocked, true, 2, false);
        const auto levelThreeConstruction =
            ian::makeConstructionHotbarLayout(
                allUnlocked, true, 3, false);
        require(
            creativeEconomy.count == 3U &&
                creativeConstruction.count == ian::MaximumBuildingHotbarSlots - 1U &&
                creativeConstruction.items[0].buildingType == ian::BuildingType::GunTurret &&
                !creativeConstruction.indexOf(ian::BuildingType::Gate) &&
                creativeConstruction.indexOf(ian::BuildingType::LumberMill) &&
                creativeConstruction.items[14].buildingType == ian::BuildingType::CrystalMine &&
                creativeConstruction.items[14].gapBefore &&
                creativeConstruction.items[17].buildingType == ian::BuildingType::Wall &&
                creativeConstruction.items[17].gapBefore &&
                creativeConstruction.items[18].modularPiece == ian::ModularBuildPiece::Foundation &&
                !creativeConstruction.items[18].gapBefore &&
                creativeConstruction.items[19].modularPiece == ian::ModularBuildPiece::Ramp,
            "one construction catalog combines defenses, production, platforms and ramps");
        require(levelOneConstruction.count == 20U && levelTwoConstruction.count == 20U &&
                levelThreeConstruction.count == 20U,
            "unified catalog preserves every unlocked structure at Core level one");
        require(creativeConstruction.indexOf(ian::ModularBuildPiece::FloorPlatform) ==
                creativeConstruction.indexOf(ian::ModularBuildPiece::Foundation) &&
                ian::modularBuildHotbarIndex(ian::ModularBuildPiece::FloorPlatform) ==
                ian::modularBuildHotbarIndex(ian::ModularBuildPiece::Foundation),
            "foundation and floor share one visible platform slot");
        for (std::size_t i = 0; i < creativeConstruction.count; ++i) {
            const auto& item = creativeConstruction.items[i];
            require(item.modularPiece.has_value() != item.buildingType.has_value(),
                "every unified slot has exactly one placement type");
            for (std::size_t j = 0; j < i; ++j) {
                const auto& other = creativeConstruction.items[j];
                require(!(item.buildingType && item.buildingType == other.buildingType) &&
                        !(item.modularPiece && item.modularPiece == other.modularPiece),
                    "unified catalog never duplicates a structure or tool");
            }
        }
        const auto beforeCore = ian::makeConstructionHotbarLayout(allUnlocked, false, 0, true);
        require(beforeCore.count == 1U && beforeCore.items[0].buildingType == ian::BuildingType::Core,
            "unified catalog shows only Core before its placement");
    }
    std::array<bool, ian::GameBalance::BuildingTypeCount> unlocked{};
    unlocked[static_cast<std::size_t>(ian::BuildingType::Core)] = true;
    unlocked[static_cast<std::size_t>(ian::BuildingType::Wall)] = true;
    unlocked[static_cast<std::size_t>(ian::BuildingType::GunTurret)] = true;
    auto hotbar = ian::makeBuildingHotbarLayout(
        unlocked, ian::BuildingHotbarCategory::Base,
        true, 1);
    require(
        hotbar.count == 0U,
        "base category is empty after the core is placed");
    hotbar = ian::makeBuildingHotbarLayout(
        unlocked, ian::BuildingHotbarCategory::Modular,
        true, 1);
    require(hotbar.count == 1U && hotbar.types[0] == ian::BuildingType::Wall,
            "walls share the construction group with platforms and ramps");
    hotbar = ian::makeBuildingHotbarLayout(
        unlocked, ian::BuildingHotbarCategory::Defense,
        true, 1);
    require(
        hotbar.count == 1U &&
            hotbar.types[0] == ian::BuildingType::GunTurret,
        "defense category is compact and independent from base buildings");
    unlocked[static_cast<std::size_t>(ian::BuildingType::Turret)] = true;
    hotbar = ian::makeBuildingHotbarLayout(
        unlocked, ian::BuildingHotbarCategory::Defense,
        true, 3);
    require(
        hotbar.count == 2U &&
            hotbar.types[0] == ian::BuildingType::GunTurret &&
            hotbar.types[1] == ian::BuildingType::Turret,
        "new defense unlock appears at the end of the compact category");
    hotbar = ian::makeBuildingHotbarLayout(
        unlocked, ian::BuildingHotbarCategory::Economy,
        false, 0);
    require(
        hotbar.count == 1U &&
            hotbar.types[0] == ian::BuildingType::Core,
        "only the core is visible before the core is placed");
    unlocked[static_cast<std::size_t>(
        ian::BuildingType::WoodStorage)] = true;
    unlocked[static_cast<std::size_t>(
        ian::BuildingType::StoneStorage)] = true;
    unlocked[static_cast<std::size_t>(
        ian::BuildingType::CrystalStorage)] = true;
    hotbar = ian::makeBuildingHotbarLayout(
        unlocked, ian::BuildingHotbarCategory::Economy,
        true, 8);
    require(
        hotbar.count == 0U,
        "legacy storage buildings never return to the build catalog");
    hotbar = ian::makeBuildingHotbarLayout(
        unlocked, ian::BuildingHotbarCategory::Economy,
        true, 1, true);
    require(
        hotbar.count == 0U,
        "sandbox catalog also excludes removed storage buildings");

    ian::BuildingSystem removedStorageBuildings;
    removedStorageBuildings.setSandboxMode(true);
    require(
        !removedStorageBuildings.place(
            ian::BuildingType::WoodStorage, {0, 0}, 0,
            1000, 1000, 1000),
        "removed storage cannot be placed through the building system");
    {
        ian::BuildingSystem repeatedBuildings;
        repeatedBuildings.setSandboxMode(true);
        require(repeatedBuildings.place(
                    ian::BuildingType::Core, {0, 0}, 0,
                    1000, 1000, 1000).has_value(),
                "repeat cost fixture creates core");
        const auto firstCost = repeatedBuildings.configuredCost(
            ian::BuildingType::GunTurret);
        const auto otherTypeCost = repeatedBuildings.configuredCost(
            ian::BuildingType::Turret);
        const auto first = repeatedBuildings.place(
            ian::BuildingType::GunTurret, {-4, -4}, 0,
            1000, 1000, 1000);
        const auto secondCost = repeatedBuildings.configuredCost(
            ian::BuildingType::GunTurret);
        require(repeatedBuildings.validate(
                    ian::BuildingType::GunTurret, {-4, 0},
                    firstCost.wood, firstCost.stone,
                    firstCost.crystals).error ==
                    ian::PlacementError::InsufficientResources,
                "placement validates the increased price");
        const auto second = repeatedBuildings.place(
            ian::BuildingType::GunTurret, {-4, 0}, 0,
            1000, 1000, 1000);
        const auto thirdCost = repeatedBuildings.configuredCost(
            ian::BuildingType::GunTurret);
        require(first && second && first->cost == firstCost &&
                    second->cost == secondCost &&
                    firstCost == ian::ResourceCost{35, 25, 10} &&
                    secondCost == ian::ResourceCost{40, 30, 10} &&
                    thirdCost == ian::ResourceCost{45, 35, 15} &&
                    repeatedBuildings.configuredCost(
                        ian::BuildingType::Turret) == otherTypeCost,
                "each same-type defense scales every resource by 1.15 and rounds to five");
        for (int existing = 2; existing < 12; ++existing) {
            if (existing >= 8)
                require(repeatedBuildings.configuredCost(ian::BuildingType::GunTurret) ==
                            ian::ResourceCost{105, 75, 30},
                    "ninth and later same-type defenses stay at three times base cost");
            const auto placed = repeatedBuildings.place(ian::BuildingType::GunTurret,
                {4 + existing * 3, -4}, 0, 1000, 1000, 1000);
            require(placed.has_value(), "price ceiling fixture places successive defenses");
            if (existing >= 8)
                require(placed->cost == ian::ResourceCost{105, 75, 30},
                    "placement charges the capped defense price");
        }
        for (int removed = 0; removed < 5; ++removed) {
            const auto last = repeatedBuildings.buildings().back().id;
            require(repeatedBuildings.remove(last).has_value(), "price fixture removes a defense");
        }
        require(repeatedBuildings.configuredCost(ian::BuildingType::GunTurret) ==
                    ian::ResourceCost{95, 65, 25},
            "removing defenses below the ceiling lowers the next copy price");
    }
    {
        ian::BuildingSystem perimeter;
        perimeter.setSandboxMode(true);
        require(perimeter.place(ian::BuildingType::Core, {0, 0}, 0, 1000, 1000, 1000).has_value(),
                "wall pricing fixture creates core");
        std::optional<ian::PlacedBuilding> lastWall;
        for (int existing = 0; existing < 40; ++existing) {
            const ian::ResourceCost expected{10 + existing / 10, 0, 0};
            const ian::GridPosition site{4 + existing * 2, 0};
            require(perimeter.configuredCost(ian::BuildingType::Wall) == expected,
                    "long perimeter accumulates one wood per ten existing walls");
            require(perimeter.validate(ian::BuildingType::Wall, site, expected.wood - 1, 0, 0).error ==
                        ian::PlacementError::InsufficientResources,
                    "wall preview rejects resources below its current price");
            lastWall = perimeter.place(ian::BuildingType::Wall, site, 0, expected.wood, 0, 0);
            require(lastWall && lastWall->cost == expected,
                    "wall placement charges exactly its preview price");
        }
        require(perimeter.configuredCost(ian::BuildingType::Wall).wood == 14 &&
                    perimeter.configuredCost(ian::BuildingType::GunTurret) ==
                        ian::buildingCost(ian::BuildingType::GunTurret),
                "forty walls cost fourteen wood without affecting other building types");
        require(perimeter.damage(lastWall->building.id, lastWall->building.maxHealth)->destroyed &&
                    perimeter.configuredCost(ian::BuildingType::Wall).wood == 13,
                "destroyed walls no longer count toward perimeter inflation");
    }
    std::array<std::uint8_t, ian::GameBalance::BuildingTypeCount>
        legacyBlueprintLevels{};
    legacyBlueprintLevels.fill(1U);
    const std::array legacyBuildings{
        ian::BuildingInstance{
            .id = {1000U, 1U},
            .type = ian::BuildingType::Core,
            .gridPosition = {0, 0},
            .health = 500.0,
            .maxHealth = 500.0,
        },
        ian::BuildingInstance{
            .id = {1001U, 1U},
            .type = ian::BuildingType::StoneStorage,
            .gridPosition = {4, 0},
            .health = 200.0,
            .maxHealth = 200.0,
        },
    };
    require(
        removedStorageBuildings.restoreBuildings(
            legacyBuildings, legacyBlueprintLevels, 10) &&
            removedStorageBuildings.buildings().size() == 1U &&
            removedStorageBuildings.buildings().front().type ==
                ian::BuildingType::Core,
        "loading an old run silently removes legacy storage entities");

    hotbar = ian::makeBuildingHotbarLayout(
        unlocked, ian::BuildingHotbarCategory::Modular,
        true, 8);
    require(
        hotbar.count == 1U && hotbar.types[0] == ian::BuildingType::Wall &&
            ian::BuildingHotbarCategoryCount == 4U &&
            ian::buildingHotbarCategoryName(
                ian::BuildingHotbarCategory::Modular) == "MODULAR",
        "combined construction remains a dedicated building category");
    const auto combinedHotbar =
        ian::makeConstructionHotbarLayout(
            unlocked, true, 8);
    require(
        combinedHotbar.count == 5U &&
            combinedHotbar.items[0].buildingType == ian::BuildingType::GunTurret &&
            combinedHotbar.items[1].buildingType == ian::BuildingType::Turret &&
        combinedHotbar.items[2].buildingType == ian::BuildingType::Wall &&
        combinedHotbar.items[2].gapBefore &&
            combinedHotbar.items[3].modularPiece == ian::ModularBuildPiece::Foundation &&
        !combinedHotbar.items[3].gapBefore &&
            combinedHotbar.items[4].modularPiece == ian::ModularBuildPiece::Ramp,
        "normal catalog filters locked buildings and separates construction tools");

    {
        ian::BuildingSystem expandingRadius;
        expandingRadius.setCoreBuildRadius(4);
        const auto core = expandingRadius.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
        require(core.has_value(), "core range fixture places its core");
        expandingRadius.setCombatCardProgressionEnabled(true);
        require(!expandingRadius.upgrade(core->building.id, 0, 0, 0).valid() &&
            expandingRadius.coreBuildRadius() == 4, "card-gated upgrade never expands range");
        expandingRadius.setCombatCardProgressionEnabled(false);
        require(!expandingRadius.validate(ian::BuildingType::Wall, {5, 0}, 100, 100, 100).valid(),
            "construction outside level-one range is rejected");
        for (int level = 2; level <= ian::MaxBuildingLevel; ++level) {
            require(expandingRadius.upgrade(core->building.id, 100000, 100000, 100000).valid() &&
                expandingRadius.coreBuildRadius() == 4 + (level - 1),
                "each core level adds exactly one meter of build range");
        }
        require(expandingRadius.validate(ian::BuildingType::Wall, {10, 0}, 100, 100, 100).valid(),
            "core expansion permits placement in the new range");
        require(expandingRadius.upgrade(core->building.id, 100000, 100000, 100000).valid() &&
            expandingRadius.coreBuildRadius() == 12, "Core can progress beyond eight authored tiers");
    }

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
                35, 25, 10).valid() &&
            starterDefense.validate(
                ian::BuildingType::Turret, {4, 0},
                25, 15, 0).valid(),
        "low-level placement does not enforce run arsenal ownership");
    require(starterDefense.validate(ian::BuildingType::Turret, {4, 0}, 25, 14, 0).error ==
                ian::PlacementError::InsufficientResources &&
            !starterDefense.place(ian::BuildingType::Turret, {4, 0}, 0, 25, 14, 0) &&
            starterDefense.buildings().size() == 1,
            "crossbow still requires its full stone cost");
    const auto crossbow = starterDefense.place(ian::BuildingType::Turret, {4, 0}, 0, 25, 15, 0);
    require(crossbow && crossbow->cost == ian::ResourceCost{25, 15, 0},
            "crossbow can be built without crystals");

    auto buildingFixtureBalance = ian::GameBalance::defaults();
    buildingFixtureBalance.buildings[static_cast<std::size_t>(
        ian::BuildingType::Wall)].unlockCoreLevel = 1;
    ian::BuildingSystem buildings{
        buildingFixtureBalance.buildings,
        buildingFixtureBalance.economy};

    const auto poorCore = buildings.validate(ian::BuildingType::Core, {0, 0}, 19, 0);
    require(poorCore.error == ian::PlacementError::InsufficientResources,
            "core requires configured resources");

    const auto wallBeforeCore = buildings.validate(ian::BuildingType::Wall, {4, 0}, 10, 0);
    require(wallBeforeCore.error == ian::PlacementError::CoreRequired,
            "wall requires placed core");

    const auto core = buildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(core.has_value(), "valid core placement succeeds");
    require(core->cost.wood == 20, "core reports spent resources");

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
    require(!ian::buildingBeforeLethalHit(buildings,wall->building.id,74.0),
            "nonlethal hits do not capture destruction ghosts");
    const auto wallVisual=ian::buildingBeforeLethalHit(buildings,wall->building.id,75.0);
    require(wallVisual && wallVisual->gridPosition==wall->building.gridPosition &&
                wallVisual->level==wall->building.level && wallVisual->rotation==wall->building.rotation,
            "lethal hits retain the exact building model before erasing it");
    const auto destroyedWall = buildings.damage(wall->building.id, 75.0);
    require(destroyedWall.has_value() && destroyedWall->destroyed,
            "lethal building damage destroys instance");
    require(buildings.buildings().size() == 1, "destroyed building leaves active list");

    const auto turretCost = ian::buildingCost(ian::BuildingType::GunTurret);
    require(turretCost.wood == 35 && turretCost.stone == 25 &&
                turretCost.crystals == 10,
            "starter turret has configured mixed resource cost");
    const auto turret = buildings.place(
        ian::BuildingType::GunTurret, {4, 1}, 0, 35, 25, 10);
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

    buildings.setCombatCardProgressionEnabled(true);
    require(buildings.validateUpgrade(core->building.id, 0, 0, 29).error ==
                ian::UpgradeError::CardRequired,
            "normal Core progression requires cards");
    buildings.setCombatCardProgressionEnabled(false);
    const auto levelTwo = buildings.upgrade(core->building.id, 0, 0, 0);
    require(levelTwo.valid() && levelTwo.building->level == 2 &&
                levelTwo.cost == ian::ResourceCost{},
            "sandbox Core upgrade reaches level two without crystals");
    require(levelTwo.building->maxHealth == 650.0, "core upgrade increases max health");
    const auto levelThree = buildings.upgrade(core->building.id, 0, 0, 0);
    require(levelThree.valid() && levelThree.building->level == 3 &&
                levelThree.cost == ian::ResourceCost{},
            "second sandbox Core upgrade remains free");
    for (int expectedLevel = 4;
         expectedLevel <= ian::MaxBuildingLevel;
         ++expectedLevel) {
        const auto currentCore = buildings.core();
        require(currentCore.has_value(),
                "core remains available through progression");
        const auto upgraded = buildings.upgrade(
            currentCore->id, 0, 0, 0);
        require(
            upgraded.valid() &&
                upgraded.building->level == expectedLevel,
            "core progresses through extended building levels");
    }
    ian::BuildingSystem uncappedCore;
    const auto uncappedPlacement = uncappedCore.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(uncappedPlacement.has_value(), "uncapped Core fixture places Core");
    for (int expectedLevel = 2;
         expectedLevel <= ian::MaxStructureLevel; ++expectedLevel) {
        const auto upgraded = uncappedCore.upgrade(uncappedPlacement->building.id, 0, 0, 0);
        require(upgraded.valid() && upgraded.building->level == expectedLevel,
                "Core continues beyond authored visual tiers");
    }
    require(
        uncappedCore.validateUpgrade(
            uncappedPlacement->building.id, 10000, 10000, 10000)
                .error == ian::UpgradeError::MaxLevel,
        "core stops at extended maximum level");

    ian::BuildingSystem limitedDefense;
    const auto limitedCore = limitedDefense.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(limitedCore.has_value(),
            "defense limit fixture creates core");
    require(
        limitedDefense.place(
            ian::BuildingType::GunTurret, {-4, -4}, 0,
            1000, 1000, 1000).has_value() &&
            limitedDefense.place(
                ian::BuildingType::GunTurret, {-4, 0}, 0,
                1000, 1000, 1000).has_value() &&
            limitedDefense.place(
                ian::BuildingType::GunTurret, {-4, 4}, 0,
                1000, 1000, 1000).has_value() &&
            limitedDefense.place(
                ian::BuildingType::GunTurret, {0, 4}, 0,
                1000, 1000, 1000).has_value() &&
            limitedDefense.place(
                ian::BuildingType::GunTurret, {4, 4}, 0,
                1000, 1000, 1000).has_value(),
        "core level one allows five defenses");
    require(limitedDefense.validate(ian::BuildingType::GunTurret, {0, -4},
                1000, 1000, 1000).valid() &&
            limitedDefense.placementLimit(ian::BuildingType::GunTurret) == std::numeric_limits<int>::max(),
        "defenses can be built beyond the former Core capacity");
    require(
        limitedDefense.upgrade(
            limitedCore->building.id, 0, 0, 50).valid() &&
            limitedDefense.place(
                ian::BuildingType::GunTurret, {0, -4}, 0,
                1000, 1000, 1000).has_value(),
        "upgrading the core does not prevent additional defenses");
    require(
        limitedDefense.placementCount(
            ian::BuildingType::SlowTrap) == 6 &&
            limitedDefense.placementLimit(
                ian::BuildingType::SlowTrap) == std::numeric_limits<int>::max(),
        "all active defense types have unlimited placement capacity");

    // Producers never consume defense places or have a per-type count cap.
    ian::BuildingSystem sharedCapacity;
    auto capacityCore = sharedCapacity.place(ian::BuildingType::Core, {0,0}, 0, 10000,10000,10000);
    require(capacityCore.has_value(), "shared capacity core exists");
    auto millA = sharedCapacity.place(ian::BuildingType::LumberMill, {-4,-4}, 0,10000,10000,10000);
    auto millB = sharedCapacity.place(ian::BuildingType::LumberMill, {-4,0}, 0,10000,10000,10000);
    auto capacityTurret = sharedCapacity.place(ian::BuildingType::GunTurret, {-4,4}, 0,10000,10000,10000);
    auto capacityTrap = sharedCapacity.place(ian::BuildingType::SpikeTrap, {0,4}, 0,10000,10000,10000);
    auto capacitySupport = sharedCapacity.place(ian::BuildingType::RepairStation, {4,4}, 0,10000,10000,10000);
    require(millA && millB && capacityTurret && capacityTrap && capacitySupport &&
            sharedCapacity.placedBuildingCount() == 3 &&
            sharedCapacity.placementLimit(ian::BuildingType::LumberMill) == std::numeric_limits<int>::max() &&
            sharedCapacity.configuredCost(ian::BuildingType::LumberMill) == ian::ResourceCost{40,15,0},
        "producers use no defense place and their 1.15x costs round to five");
    auto capacityCannon = sharedCapacity.place(ian::BuildingType::Cannon, {0,-4}, 0,10000,10000,10000);
    auto capacityLaser = sharedCapacity.place(ian::BuildingType::LaserTower, {4,-4}, 0,10000,10000,10000);
    require(capacityCannon && capacityLaser && sharedCapacity.placedBuildingCount() == 5,
        "defense count remains tracked despite unlimited placement");
    for (auto type : {ian::BuildingType::GunTurret,
            ian::BuildingType::SpikeTrap, ian::BuildingType::RepairStation, ian::BuildingType::DecoyBeacon}) {
        require(sharedCapacity.placementCount(type) == 5 &&
            sharedCapacity.placementLimit(type) == std::numeric_limits<int>::max() &&
            sharedCapacity.validate(type, {2,-3}, 10000,10000,10000).valid(),
            "defenses, support buildings and traps remain available after five placements");
    }
    require(sharedCapacity.configuredCost(ian::BuildingType::CrystalMine) == ian::ResourceCost{10,20,5} &&
        sharedCapacity.configuredCost(ian::BuildingType::Quarry) == ian::ResourceCost{10,30,0} &&
        sharedCapacity.place(ian::BuildingType::Quarry, {2,-3},0,10000,10000,10000).has_value() &&
        sharedCapacity.configuredCost(ian::BuildingType::Quarry) == ian::ResourceCost{10,35,0} &&
        sharedCapacity.placedBuildingCount() == 5,
        "producers remain buildable after the defense budget fills and use requested prices");
    require(sharedCapacity.place(ian::BuildingType::Wall, {3,0},0,10000,10000,10000).has_value() &&
        sharedCapacity.place(ian::BuildingType::Gate, {5,0},0,10000,10000,10000).has_value() &&
        sharedCapacity.placedBuildingCount() == 5,
        "Core, walls and gates never consume shared places");
    require(sharedCapacity.sell(millA->building.id).valid() && sharedCapacity.placedBuildingCount() == 5,
        "selling a producer does not change defense capacity");
    require(sharedCapacity.damage(capacityTurret->building.id, 100000).has_value() &&
        sharedCapacity.placedBuildingCount() == 4,
        "destroying a defense immediately frees shared capacity");
    const auto capacityLegacyBuildings = sharedCapacity.buildings();
    sharedCapacity.setBuildingLimitBonus(0);
    ian::BuildingSystem legacyCapacity;
    auto overBudget = capacityLegacyBuildings;
    for (int i = 0; i < 8; ++i) {
        auto copy = millB->building;
        copy.id = {static_cast<std::uint32_t>(9000+i),1};
        copy.gridPosition = {20+i*3, 0};
        overBudget.push_back(copy);
    }
    require(legacyCapacity.restoreBuildings(overBudget, sharedCapacity.blueprintLevels(), 100) &&
        legacyCapacity.placedBuildingCount() == 4 && legacyCapacity.totalBuildingLimit() == 5 &&
        legacyCapacity.placementCount(ian::BuildingType::LumberMill) == 9 &&
        legacyCapacity.placementLimit(ian::BuildingType::LumberMill) == std::numeric_limits<int>::max(),
        "old bases retain unlimited producer copies without consuming defense capacity");
    sharedCapacity.setBuildingLimitBonus(4);
    for (int level = 1; level <= ian::MaxBuildingLevel; ++level) {
        require(sharedCapacity.totalBuildingLimit() == 5 + 2*(level-1) + 4 &&
            sharedCapacity.placementLimit(ian::BuildingType::GunTurret) == std::numeric_limits<int>::max() &&
            sharedCapacity.placementLimit(ian::BuildingType::LumberMill) == std::numeric_limits<int>::max(),
            "legacy capacity values persist without restricting placement");
        if (level < ian::MaxBuildingLevel)
            require(sharedCapacity.upgrade(capacityCore->building.id,10000,10000,10000).valid(),
                "shared capacity fixture upgrades core");
    }
    sharedCapacity.setBuildingLimitBonus(0);
    require(sharedCapacity.totalBuildingLimit() == 19, "core level eight has nineteen base places");
    sharedCapacity.setSandboxMode(true);
    require(sharedCapacity.placementLimit(ian::BuildingType::Quarry) == std::numeric_limits<int>::max(),
        "godmode retains unlimited construction");
    sharedCapacity.setSandboxMode(false);
    sharedCapacity.reset();
    require(sharedCapacity.totalBuildingLimit() == 5 && sharedCapacity.placedBuildingCount() == 0,
        "reset clears shared buildings and capacity bonuses");

    ian::BuildingSystem cannonBuildings;
    const auto cannonCore =
        cannonBuildings.place(ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(cannonCore.has_value(), "cannon fixture creates core");
    require(cannonBuildings.validate(ian::BuildingType::Cannon, {4, 0}, 40, 30, 25).valid(),
            "acquired cannon can be placed at Core level one");
    cannonBuildings.upgrade(cannonCore->building.id, 0, 0, 50);
    cannonBuildings.upgrade(cannonCore->building.id, 0, 0, 100);
    require(cannonBuildings.validate(
                ian::BuildingType::Cannon, {4, 0},
                40, 30, 25).valid(),
            "Core tiers do not gate cannon placement");
    cannonBuildings.upgrade(cannonCore->building.id, 0, 0, 150);
    const auto cannon =
        cannonBuildings.place(ian::BuildingType::Cannon, {4, 0}, 0, 40, 30, 25);
    require(cannon.has_value(), "core level four unlocks cannon");
    require(
        cannonBuildings.upgradeCost(cannon->building) ==
            ian::buildingUpgradeCost(cannon->building),
        "default upgrade helpers share one pricing formula");
    require(
        cannonBuildings.validateUpgrade(
            cannon->building.id, 1000, 1000, 1000).error ==
            ian::UpgradeError::None,
        "directional defenses can be upgraded individually");
    const auto secondCannon = cannonBuildings.place(
        ian::BuildingType::Cannon, {4, -2}, 0, 1000, 1000, 1000);
    require(secondCannon && secondCannon->building.level == 1,
            "another defense starts at level one");
    const ian::ResourceCost levelTwoCost =
        cannonBuildings.upgradeCost(cannon->building);
    const auto cannonLevelTwo = cannonBuildings.upgrade(
        cannon->building.id,
        levelTwoCost.wood, levelTwoCost.stone,
        levelTwoCost.crystals);
    require(cannonLevelTwo.valid() &&
                cannonLevelTwo.building->level == 2,
            "upgrade raises only the selected defense");
    const auto upgradedCannon = std::find_if(
        cannonBuildings.buildings().begin(),
        cannonBuildings.buildings().end(),
        [&cannon](const ian::BuildingInstance& building) {
            return building.id == cannon->building.id;
        });
    require(upgradedCannon != cannonBuildings.buildings().end(),
            "upgraded cannon remains in the world");
    requireNear(upgradedCannon->maxHealth, 235.0, 1e-9,
                "individual upgrade gives meaningful health increase");
    const auto untouchedCannon = std::ranges::find(
        cannonBuildings.buildings(), secondCannon->building.id,
        &ian::BuildingInstance::id);
    require(untouchedCannon != cannonBuildings.buildings().end() &&
                untouchedCannon->level == 1,
            "upgrading one cannon leaves another cannon unchanged");
    const ian::ResourceCost levelThreeCost =
        cannonBuildings.upgradeCost(*upgradedCannon);
    const auto cannonLevelThree = cannonBuildings.upgrade(
        cannon->building.id,
        levelThreeCost.wood, levelThreeCost.stone,
        levelThreeCost.crystals);
    require(cannonLevelThree.valid() && cannonLevelThree.building->level == 3,
            "core level unlocks next individual building upgrade");
    const auto levelThreeCannon = std::find_if(
        cannonBuildings.buildings().begin(),
        cannonBuildings.buildings().end(),
        [&cannon](const ian::BuildingInstance& building) {
            return building.id == cannon->building.id;
        });
    requireNear(levelThreeCannon->maxHealth, 290.0, 1e-9,
                "next building level follows rounded health curve");
    const auto thirdCannon = cannonBuildings.place(
        ian::BuildingType::Cannon, {4, -4}, 0, 1000, 1000, 1000);
    require(thirdCannon && thirdCannon->building.level == 1,
            "new defenses do not inherit another building's upgrades");
    const ian::ResourceCost levelFourCost =
        cannonBuildings.upgradeCost(*cannonLevelThree.building);
    const auto cannonLevelFour = cannonBuildings.upgrade(
        cannon->building.id,
        levelFourCost.wood, levelFourCost.stone,
        levelFourCost.crystals);
    require(
        cannonLevelFour.valid() && cannonLevelFour.building->level == 4 &&
            cannonBuildings.validateUpgrade(
                cannon->building.id,
                1000, 1000, 1000).valid(),
        "individual defense levels can exceed core level");
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
    selectionBuildings.setSandboxMode(true);
    const auto selectionCore = selectionBuildings.place(
        ian::BuildingType::Core, {0, 0}, 0, 30, 0);
    require(
        selectionCore.has_value(),
        "selection fixture creates core");
    require(
        selectionBuildings.raycast(
            {0.0, 1.0, 3.0}, {0.0, 0.0, -1.0}, 4.0) ==
            selectionCore->building.id &&
        !selectionBuildings.raycast(
            {0.0, 2.0, 3.0}, {0.0, 0.0, -1.0}, 4.0),
        "core selection does not extend far above its model");
    const auto selectedWall = selectionBuildings.place(
        ian::BuildingType::Wall, {4, 4}, 0, 1000, 1000);
    const auto diagonalWall = selectionBuildings.place(
        ian::BuildingType::Wall, {5, 5}, 0, 1000, 1000);
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
    const auto door = selectionBuildings.convertWallToGate(selectedWall->building.id);
    require(door && door->type == ian::BuildingType::Gate &&
                door->id == selectedWall->building.id &&
                door->health == selectedWall->building.health &&
                door->maxHealth == selectedWall->building.maxHealth,
        "turning a wall into a door preserves its identity and wall health");
    require(selectionBuildings.setCombatBlueprintLevel(ian::BuildingType::Wall, 2),
        "wall card upgrades its converted doors too");
    const auto upgradedDoor = std::ranges::find(selectionBuildings.buildings(),
        door->id, &ian::BuildingInstance::id);
    require(upgradedDoor != selectionBuildings.buildings().end() &&
                upgradedDoor->level == 2 &&
                upgradedDoor->maxHealth ==
                    ian::buildingMaxHealthAtLevel(100.0, 2),
        "door level and health follow the wall blueprint");

    ian::BuildingSystem elevatedBuildings;
    elevatedBuildings.setSandboxMode(true);
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
    require(repairCost.wood == 5 && repairCost.stone == 2,
            "repair cost scales with missing health");
    const auto repairValidation =
        cannonBuildings.validateRepair(
        gate->building.id, 5, 2, 0);
    require(
        repairValidation.cost == repairCost,
        "default repair helpers share one pricing formula");
    require(cannonBuildings.validateRepair(gate->building.id, 4, 2, 0).error ==
                ian::BuildingActionError::InsufficientResources,
            "repair validates available resources");
    const auto repairedGate = cannonBuildings.repair(gate->building.id, 5, 2, 0);
    require(repairedGate.valid() && repairedGate.building->health == 100.0 &&
                repairedGate.repairedHealth == 65.0,
            "repair restores full building health");
    require(cannonBuildings.validateRepair(gate->building.id, 5, 2, 0).error ==
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

    require(buildings.place(ian::BuildingType::CrystalMine, {4, 3}, 0, 1000, 1000, 1000).has_value(),
            "first crystals mine placement succeeds");
    require(buildings.place(ian::BuildingType::CrystalMine, {4, 5}, 0, 1000, 1000, 1000).has_value(),
            "second crystals mine placement succeeds");
    require(buildings.place(ian::BuildingType::CrystalMine, {6, 3}, 0, 1000, 1000, 1000).has_value(),
            "third crystals mine placement succeeds");
    require(buildings.place(ian::BuildingType::CrystalMine, {6, 5}, 0, 1000, 1000, 1000).has_value(),
            "fourth crystals mine placement succeeds");
    require(buildings.place(ian::BuildingType::CrystalMine, {8, 3}, 0, 1000, 1000, 1000).has_value() &&
                buildings.place(ian::BuildingType::CrystalMine, {8, 5}, 0, 1000, 1000, 1000).has_value() &&
                buildings.place(ian::BuildingType::CrystalMine, {10, 3}, 0, 1000, 1000, 1000).has_value() &&
                buildings.place(ian::BuildingType::CrystalMine, {10, 5}, 0, 1000, 1000, 1000).has_value(),
            "eight crystal mines can be built without consuming defense places");
    const int defensesBeforeNinthMine = buildings.placedBuildingCount();
    require(buildings.place(ian::BuildingType::CrystalMine, {8, 7}, 0, 1000, 1000, 1000).has_value() &&
        buildings.placementLimit(ian::BuildingType::CrystalMine) == std::numeric_limits<int>::max() &&
        buildings.placementCount(ian::BuildingType::CrystalMine) == 9 &&
        buildings.placedBuildingCount() == defensesBeforeNinthMine,
        "a ninth producer is allowed and still uses no defense place");

    const ian::Vec3 player{0.0, 1.7, 6.0};
    const auto closeAim =
        ian::aimedBuildingGridPosition(player, 0.0, -std::numbers::pi / 4.0);
    require(closeAim == ian::GridPosition{0, 4},
            "large building snaps to the nearest one-cell intersection");
    const auto farAim = ian::aimedBuildingGridPosition(player, 0.0, -0.01);
    require(farAim == ian::GridPosition{0, -4},
            "shallow aim clamps placement to maximum distance");
    const auto rightAim = ian::aimedBuildingGridPosition(
        player, std::numbers::pi / 2.0, -std::numbers::pi / 4.0);
    require(
        rightAim == ian::GridPosition{2, 6},
        "yaw places large buildings on the one-cell lattice");
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
        coreCenter.x == 2.0 && coreCenter.z == 6.0,
        "large building centers may use either parity without changing footprint");
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
                ian::BuildingType::Cannon) == 1.0,
        "producers use one-cell footprints and combat buildings keep theirs");
    const ian::Vec3 turretCenter =
        ian::buildingWorldPosition(
            ian::BuildingType::Turret, {2, 6});
    require(
        turretCenter.x == 2.0 && turretCenter.z == 6.0,
        "two-cell combat building remains centered on grid intersection");

    for (std::size_t index = 0; index < ian::GameBalance::BuildingTypeCount; ++index) {
        const auto type = static_cast<ian::BuildingType>(index);
        const auto first = ian::aimedBuildingGridPosition(
            {0.2, 1.7, 0.2}, 0.0, 0.0, 1.0, 1.0, type);
        const auto adjacent = ian::aimedBuildingGridPosition(
            {1.2, 1.7, 0.2}, 0.0, 0.0, 1.0, 1.0, type);
        const auto negative = ian::aimedBuildingGridPosition(
            {-0.8, 1.7, 0.2}, 0.0, 0.0, 1.0, 1.0, type);
        require(adjacent.x == first.x + 1 && negative.x == first.x - 1,
                "every building advances exactly one cell including negative coordinates");
        if (ian::buildingFootprintHalfExtent(type) == 1.0) {
            const auto snapAt = [type](double x) {
                return ian::aimedBuildingGridPosition(
                    {x, 1.7, 0.0}, 0.0, 0.0, 1.0, 1.0, type).x;
            };
            require(snapAt(0.49) == 0 && snapAt(0.51) == 1 &&
                        snapAt(-0.49) == 0 && snapAt(-0.51) == -1,
                    "large building snapping changes at cell midpoints in both directions");
        }
    }

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
