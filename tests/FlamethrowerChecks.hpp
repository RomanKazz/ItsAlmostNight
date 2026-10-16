#pragma once

#include "TestHarness.hpp"
#include "buildings/BuildingHotbarLayout.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "combat/AttackStatCompatibility.hpp"
#include "game/Simulation.hpp"
#include "world/CollisionWorld.hpp"

#include <memory>

inline void checkFlamethrowerBuilding() {
    using namespace ian;
    static_assert(static_cast<int>(BuildingType::Catapult) == 14);
    static_assert(static_cast<int>(BuildingType::Flamethrower) == 15);
    static_assert(runUpgradeIndex(RunUpgradeEffect::UpgradePickaxe) == 58);
    static_assert(runUpgradeIndex(RunUpgradeEffect::UnlockFlamethrower) == 59);
    require(runDefenseCard(RunUpgradeEffect::UnlockFlamethrower), "flamethrower uses shared defense cards");
    require(buildingLimitCategory(BuildingType::Flamethrower) == BuildingLimitCategory::Defense,
        "flamethrower consumes the core defense limit");
    require(isDirectionalDefense(BuildingType::Flamethrower) && buildingRotationStepCount(BuildingType::Flamethrower)==8,
        "flamethrower supports placement and manual eight-way rotation");
    for (std::size_t i=0; i<static_cast<std::size_t>(AttackStat::Count); ++i)
        require(!supportsAttackStat(BuildingType::Flamethrower, static_cast<AttackStat>(i)),
            "unimplemented flamethrower attack does not advertise working attack statistics");
    const auto box = buildingCollisionBox(BuildingType::Flamethrower, {0,0}, 3.0);
    requireNear(box.maxX-box.minX, 2, 1e-12, "flamethrower occupies two cells");
    requireNear(box.minimumBlockingEyeY, 3, 1e-12, "building collision follows platform height");

    auto map=MapDefinition::defaults(); map.obstacles.clear();
    auto world=WorldConfig::defaults(); world.terrainAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    auto simulation=std::make_unique<Simulation>(GameBalance::defaults(), map, world);
    simulation->startRunFromSeed(PlayerClass::Vanguard, 702314);
    PlayerCommand god; god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};
    simulation->tick(0,god);
    PlayerCommand place; place.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};
    simulation->tick(0,place);
    const auto core=simulation->snapshot().coreId;
    require(core.has_value(), "flamethrower fixture places core");
    PlayerCommand upgrade; upgrade.upgradeBuilding=UpgradeBuildingCommand{*core,false};
    simulation->tick(0,upgrade); simulation->tick(0,upgrade);
    require(simulation->snapshot().coreLevel==3, "fixture raises core to the defense level limit");
    require(simulation->grantDebugRunUpgrade(RunUpgradeEffect::UnlockFlamethrower), "flamethrower acquisition card works");
    const auto slot=static_cast<std::size_t>(BuildingType::Flamethrower);
    require(simulation->snapshot().unlockedBuildings[slot], "flamethrower appears after acquisition");
    const auto layout=makeBuildingHotbarLayout(simulation->snapshot().unlockedBuildings,
        BuildingHotbarCategory::Defense,true,1,true);
    require(layout.indexOf(BuildingType::Flamethrower).has_value(), "acquired flamethrower has its own hotbar slot");
    auto allUnlocked = simulation->snapshot().unlockedBuildings;
    allUnlocked.fill(true);
    const auto complete = makeBuildingHotbarLayout(allUnlocked, BuildingHotbarCategory::Defense, true, 8, true);
    require(complete.count == 13 && complete.types[4] == BuildingType::Flamethrower &&
        complete.types[5] == BuildingType::MissileLauncher && complete.types[6] == BuildingType::TeslaTower && complete.types[7] == BuildingType::LaserTower && complete.types[9] == BuildingType::RepairStation && complete.types[10] == BuildingType::Wall && complete.types[11] == BuildingType::SpikeTrap && complete.types[12] == BuildingType::SlowTrap,
        "defense hotbar places weapons first and traps in the final slots");
    place.placeBuilding=PlaceBuildingCommand{BuildingType::Flamethrower,{3,0},3};
    simulation->tick(0,place);
    auto findFlame=[&]() {
        const auto& buildings=simulation->snapshot().buildings;
        return std::ranges::find(buildings,BuildingType::Flamethrower,&BuildingInstance::type);
    };
    require(findFlame()!=simulation->snapshot().buildings.end(), "flamethrower places as a real building");
    const auto id=findFlame()->id;
    require(findFlame()->rotation==3, "placed flamethrower preserves direction");
    PlayerCommand rotate; rotate.rotatePlacedBuilding=RotatePlacedBuildingCommand{id,1};
    simulation->tick(0,rotate);
    require(findFlame()->rotation==4, "placed flamethrower turns through the shared rotation command");
    simulation->tick(0,god);
    require(!simulation->snapshot().unlimitedResources, "card upgrade test uses normal progression");
    require(simulation->grantDebugRunUpgradeBatch(RunUpgradeEffect::UnlockFlamethrower,2), "flamethrower upgrades through cards");
    require(findFlame()->level==3 && findFlame()->maxHealth>160, "card level upgrades placed flamethrower health");
    require(std::ranges::any_of(simulation->snapshot().defenseCards,[&](const auto& card){return card.buildingId==id;}),
        "flamethrower supplies management statistics");
    PlayerCommand start; start.startWaveEarly=StartWaveEarlyCommand{};
    const auto checkpoint=simulation->saveSuspendedRunState();
    simulation->tick(0,start);
    simulation->tick(3);
    require(simulation->snapshot().defenseProjectiles.empty(), "flamethrower does not accidentally fire another weapon's projectiles");
    simulation->returnToMainMenu();
    require(simulation->loadSuspendedRunState(checkpoint), "flamethrower checkpoint loads");
    require(findFlame()!=simulation->snapshot().buildings.end() && findFlame()->level==3 && findFlame()->rotation==4,
        "flamethrower saves identity, blueprint level and direction");
    PlayerCommand sell; sell.sellBuilding=SellBuildingCommand{id};
    simulation->tick(0,sell);
    require(findFlame()==simulation->snapshot().buildings.end(), "flamethrower sells through normal building management");
}
