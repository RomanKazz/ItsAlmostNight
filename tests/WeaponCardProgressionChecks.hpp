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
    require(owned>17500 && owned<18500,"owned weapon uses the fixed existing-category chance");
    ResourceSystem nodes({{ResourceType::Wood,{0,0,0},.8,100,100,1},
        {ResourceType::Stone,{10,0,0},.8,100,100,1},
        {ResourceType::Crystal,{20,0,0},.8,100,100,1},
        {ResourceType::Crate,{30,0,0},.8,1,0,1}});
    const auto initial=nodes.nodes();
    require(nodes.damage(initial[0].id,20).has_value(),"fixture partially mines a node");
    const auto partial=nodes.nodes()[0];
    nodes.setNodeCapacityMultiplier(1.4);
    const auto expanded=nodes.nodes();
    requireNear(expanded[0].maxHealth-expanded[0].health,partial.maxHealth-partial.health,1e-9,
        "capacity preserves work already spent on a partially mined node");
    require(expanded[0].yield-expanded[0].yieldRemaining==partial.yield-partial.yieldRemaining,
        "capacity preserves resources already extracted");
    nodes.setNodeCapacityMultiplier(1.4);
    require(nodes.nodes()[0].yieldRemaining==expanded[0].yieldRemaining,"repeated refresh cannot refill a node");
    require(nodes.nodes()[3].maxHealth==initial[3].maxHealth && nodes.nodes()[3].yield==initial[3].yield,
        "capacity does not affect loot containers");
    require(nodes.damage(initial[1].id,10000)->collected,"fixture depletes a stone node");
    nodes.setNodeCapacityMultiplier(1.6);
    require(!nodes.nodes()[1].active && nodes.nodes()[1].health==0,"upgrade cannot revive a depleted node");
    nodes.tick(1.1,{},100);
    require(nodes.nodes()[1].active,"depleted node respawns normally");
    ResourceSystem reference({{ResourceType::Wood,{0,0,0},.8,100,100,1},
        {ResourceType::Stone,{10,0,0},.8,100,100,1},
        {ResourceType::Crystal,{20,0,0},.8,100,100,1},
        {ResourceType::Crate,{30,0,0},.8,1,0,1}});
    require(reference.restoreNodes(expanded,1,1,1.4),"partial node checkpoint restores its capacity modifier");
    reference.setNodeCapacityMultiplier(1.4);
    requireNear(reference.nodes()[0].health,expanded[0].health,1e-9,"restored partial mining does not reset health");
    require(reference.nodes()[0].yieldRemaining==expanded[0].yieldRemaining,
        "restored partial mining does not grant duplicate stock");
    reference.reset();
    require(reference.damage(reference.nodes()[1].id,10000)->collected,"reference node depletes");
    reference.tick(1.1,{},100);
    requireNear(nodes.nodes()[1].maxHealth,reference.nodes()[1].maxHealth*1.6,1e-9,
        "respawn retains the current world capacity multiplier");
    require(nodes.nodes()[1].yield==static_cast<int>(std::lround(reference.nodes()[1].yield*1.6)),
        "respawn retains the increased world resource stock");
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
        require(run->grantDebugRunUpgradeBatch(effect,1),"explicit bulk debug access acquires the complete weapon test loadout without the core");
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
    const auto nodeView=run->snapshot().resourceNodes;
    const std::vector<ResourceNode> baseNodes(nodeView.begin(),nodeView.end());
    const double yield=run->snapshot().upgradeStats.resourceYield;
    require(run->grantDebugRunUpgrade(RunUpgradeEffect::ResourceCapacity,RunUpgradeRarity::Rare),"capacity tome can be acquired");
    require(run->snapshot().woodCapacity==capacity &&
        run->snapshot().upgradeStats.resourceYield==yield,"world capacity preserves player storage and yield bonuses");
    for (std::size_t i=0;i<baseNodes.size();++i) {
        const auto& grown=run->snapshot().resourceNodes[i];
        requireNear(grown.maxHealth,baseNodes[i].maxHealth*1.28,1e-9,"capacity raises resource durability");
        require(grown.yield==static_cast<int>(std::lround(baseNodes[i].yield*1.28)),
            "capacity increases the resource stock in the world");
    }
    require(!run->openRunShop(true) && !run->snapshot().runShop.pending,
        "temporarily disabled shop stays closed even for capped weapons");
    auto saved=run->saveSuspendedRunState();
    run->returnToMainMenu();require(run->loadSuspendedRunState(saved),"weapon levels resume from save");
    require(run->snapshot().weaponLevels[static_cast<std::size_t>(PlayerWeapon::FireWand)]==2 &&
        run->snapshot().woodCapacity==capacity,"load retains levels without changing carrying capacity");
    for(std::size_t i=0;i<saved.resourceNodes.size();++i)
        requireNear(run->snapshot().resourceNodes[i].maxHealth,saved.resourceNodes[i].maxHealth,1e-9,
            "load does not apply world node capacity twice");
    auto oldStorage=saved;oldStorage.resourceNodeCapacityVersion=0;oldStorage.resourceNodes=baseNodes;
    run->returnToMainMenu();require(run->loadSuspendedRunState(oldStorage),"old storage-tome saves migrate to world node capacity");
    for(std::size_t i=0;i<saved.resourceNodes.size();++i)
        requireNear(run->snapshot().resourceNodes[i].maxHealth,saved.resourceNodes[i].maxHealth,1e-9,
            "migration applies world capacity exactly once");
    auto legacy=saved;legacy.playerWeaponArsenalVersion=1;legacy.rifleLevel=3;
    legacy.resourceNodeCapacityVersion=0;legacy.resourceNodes=baseNodes;
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
            "legacy yield bonus migrates to world stock and durability once");
    auto invalid=saved;invalid.resourceNodeCapacityVersion=2;
    run->returnToMainMenu();require(!run->loadSuspendedRunState(invalid),"unknown node-capacity save version is rejected");
    run->restartRun();require(run->snapshot().weaponLevels[static_cast<std::size_t>(PlayerWeapon::Axe)]==1 &&
        run->snapshot().upgradeStats.resourceCapacity==1,"restart clears tool upgrades and capacity tome");
}
