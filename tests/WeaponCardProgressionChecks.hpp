#pragma once

#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include "progression/RunUpgradeOfferSelection.hpp"

#include <memory>

inline void checkWeaponCardProgression() {
    using namespace ian;
    require(runUpgradeIndex(RunUpgradeEffect::UpgradeAxe)==57 &&
        runUpgradeIndex(RunUpgradeEffect::UpgradePickaxe)==58 &&
        runUpgradeIndex(RunUpgradeEffect::ResourceCapacity)==27,
        "new tool IDs append and capacity preserves the old tome ID");
    std::array<int,RunUpgradeEffectCount> levels{};
    levels[runUpgradeIndex(RunUpgradeEffect::UpgradeAxe)]=1;
    const std::array pool{RunUpgradeEffect::UpgradeAxe,RunUpgradeEffect::UnlockRifle,
        RunUpgradeEffect::UnlockClub,RunUpgradeEffect::UnlockIceWand,RunUpgradeEffect::UnlockFireWand};
    std::uint64_t random=578421;int owned=0;
    for(int i=0;i<40000;++i) owned+=chooseRunUpgradeOffer(pool,levels,random)==0;
    require(owned>19400 && owned<20600,"weapon cards split owned upgrades and new acquisitions fifty/fifty");
    auto map=MapDefinition::defaults();map.resources={{ResourceType::Wood,{10,0,0},.8,1000,10,75},
        {ResourceType::Stone,{20,0,0},.8,1000,10,75}};map.obstacles.clear();
    auto world=WorldConfig::defaults();world.terrainAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    auto run=std::make_unique<Simulation>(GameBalance::defaults(),map,world);
    run->startRunFromSeed(PlayerClass::Vanguard,574123);
    require(!run->grantDebugRunUpgrade(RunUpgradeEffect::UpgradeAxe) &&
        !run->grantDebugRunUpgrade(RunUpgradeEffect::UpgradePickaxe),"tools are level one before the core");
    for(const auto effect:{RunUpgradeEffect::UnlockClub,RunUpgradeEffect::UnlockRifle,
            RunUpgradeEffect::UnlockIceWand,RunUpgradeEffect::UnlockFireWand})
        require(run->grantDebugRunUpgrade(effect),"weapon acquisition is still possible without the core");
    PlayerCommand god;god.enableUnlimitedResources=EnableUnlimitedResourcesCommand{};
    run->tick(0,god);
    PlayerCommand place;place.placeBuilding=PlaceBuildingCommand{BuildingType::Core,{0,0},0};run->tick(0,place);
    require(run->snapshot().coreId.has_value(),"weapon fixture places the core");
    PlayerCommand upgrade;upgrade.upgradeBuilding=UpgradeBuildingCommand{*run->snapshot().coreId};run->tick(0,upgrade);
    run->tick(0,god);
    require(run->snapshot().coreLevel==2,"weapon fixture has level two core");
    for(const auto weapon:{PlayerWeapon::Axe,PlayerWeapon::Pickaxe,PlayerWeapon::Club,
            PlayerWeapon::Rifle,PlayerWeapon::IceWand,PlayerWeapon::FireWand}) {
        PlayerCommand select;select.selectWeapon=SelectWeaponCommand{weapon};run->tick(0,select);
        const double before=run->snapshot().selectedWeaponDamage;
        const auto effect=*weaponRunUpgrade(weapon);
        require(run->grantDebugRunUpgrade(effect,RunUpgradeRarity::Legendary),"owned weapon upgrades through its card");
        require(run->snapshot().weaponLevels[static_cast<std::size_t>(weapon)]==2 &&
            run->snapshot().selectedWeaponDamage>before,"a card increases power by exactly one level at every rarity");
        require(!run->grantDebugRunUpgrade(effect),"weapon cards cannot exceed the current core level");
    }
    require(run->snapshot().tomeCount==0,"weapon and tool upgrades do not consume tome slots");
    const int capacity=run->snapshot().woodCapacity;
    const double yield=run->snapshot().upgradeStats.resourceYield;
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::ResourceCapacity,RunUpgradeRarity::Rare),"capacity tome can be acquired");
    require(run->snapshot().woodCapacity==static_cast<int>(std::ceil(capacity*1.4)) &&
        run->snapshot().upgradeStats.resourceYield==yield,"capacity scales storage while preserving resource yield");
    require(run->openRunShop(true),"level-capped weapons can be inspected in the shop pool");
    for(const auto& offer:run->snapshot().runShop.offers)
        require(!offer.present || !runWeaponLevelCard(offer.effect),"capped weapons are filtered out of shop offers");
    require(run->closeRunShop(),"weapon fixture leaves the shop");
    auto saved=run->saveSuspendedRunState();
    run->returnToMainMenu();require(run->loadSuspendedRunState(saved),"weapon levels resume from save");
    require(run->snapshot().weaponLevels[static_cast<std::size_t>(PlayerWeapon::FireWand)]==2 &&
        run->snapshot().woodCapacity==static_cast<int>(std::ceil(capacity*1.4)),"load rebuilds level stats and capacity once");
    auto legacy=saved;legacy.playerWeaponArsenalVersion=1;legacy.rifleLevel=3;
    for(const auto& definition:RunUpgradeDefinitions) if(runWeaponLevelCard(definition.effect)) {
        const auto index=runUpgradeIndex(definition.effect);
        const bool tool=isPlayerTool(*runUpgradeWeapon(definition.effect));
        legacy.runUpgradeStacks[index]=tool?0:1;legacy.runUpgradePower[index]=tool?0:1;
    }
    for(auto& node:legacy.resourceNodes) {
        node.yield=static_cast<int>(std::lround(node.yield*1.3));node.yieldRemaining=node.yield;
    }
    run->returnToMainMenu();require(run->loadSuspendedRunState(legacy),"legacy ownership-only weapons migrate");
    require(run->snapshot().rifleLevel==3 && run->snapshot().weaponLevels[static_cast<std::size_t>(PlayerWeapon::Axe)]==1,
        "migration preserves the old rifle level and supplies baseline tools");
    for(std::size_t i=0;i<saved.resourceNodes.size();++i)
        require(run->snapshot().resourceNodes[i].yield==saved.resourceNodes[i].yield,
            "legacy yield-tome bonus is removed from saved nodes once when it becomes capacity");
    run->restartRun();require(run->snapshot().weaponLevels[static_cast<std::size_t>(PlayerWeapon::Axe)]==1 &&
        run->snapshot().upgradeStats.resourceCapacity==1,"restart clears tool upgrades and capacity tome");
}
