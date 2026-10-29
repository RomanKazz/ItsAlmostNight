#pragma once

#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include <memory>

inline void checkWallRarity() {
    using namespace ian;
    constexpr auto effect=RunUpgradeEffect::UnlockWall;
    constexpr auto index=runUpgradeIndex(effect);
    // Common progression keeps the previous HP curve, including levels beyond 8.
    require(wallMaxHealthAtPower(100,1)==100 && wallMaxHealthAtPower(100,2)==130 &&
        wallMaxHealthAtPower(100,8)==300 && wallMaxHealthAtPower(100,9)==320,
        "common fence progression keeps the existing health curve");
    double previous=0;
    auto map=MapDefinition::defaults();map.resources.clear();map.obstacles.clear();
    auto world=WorldConfig::defaults();world.terrainResolution=33;world.terrainAmplitude=world.terrainSurfaceNoiseAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    auto run=std::make_unique<Simulation>(GameBalance::defaults(),map,world);
    run->startRunFromSeed(PlayerClass::Miner,71);
    auto saved=run->saveSuspendedRunState();
    saved.buildings={
        {.id={1000,1},.type=BuildingType::Core,.gridPosition={0,0},.health=500,.maxHealth=500},
        {.id={1001,1},.type=BuildingType::Wall,.gridPosition={2,0},.health=80,.maxHealth=100},
        {.id={1002,1},.type=BuildingType::Gate,.gridPosition={4,0},.health=100,.maxHealth=100}};
    for (std::size_t rarity=0;rarity<RunUpgradeRarityCount;++rarity) {
        run->returnToMainMenu();require(run->loadSuspendedRunState(saved),"wall rarity fixture restores");
        const auto before=run->snapshot();
        const double damage=before.buildings[1].maxHealth-before.buildings[1].health;
        require(run->grantDebugRunUpgrade(effect,static_cast<RunUpgradeRarity>(rarity)),"fence card applies");
        const auto after=run->snapshot();
        require(after.runUpgradeStacks[index]==2,"rarity changes HP without skipping the displayed fence level");
        const double expectedPower=1+runUpgradeRarityMultiplier(static_cast<RunUpgradeRarity>(rarity));
        requireNear(after.runUpgradePower[index],expectedPower,1e-9,"fence cards accumulate rarity power");
        const double expected=wallMaxHealthAtPower(after.wallBaseMaxHealth,expectedPower)*after.upgradeStats.buildingHealth;
        require(expected>previous,"every higher fence rarity grants more HP");previous=expected;
        requireNear(after.buildings[1].maxHealth,expected,1e-9,"fence HP matches the preview formula");
        requireNear(after.buildings[2].maxHealth,expected,1e-9,"doors inherit the same fence bonus");
        requireNear(after.buildings[1].maxHealth-after.buildings[1].health,damage,1e-9,"fence upgrade preserves damage taken");
        require(run->grantDebugRunUpgrade(RunUpgradeEffect::BuildingHealth),"global building health can apply after fence rarity");
        const auto buffed=run->snapshot();
        requireNear(buffed.buildings[1].maxHealth,wallMaxHealthAtPower(buffed.wallBaseMaxHealth,expectedPower)*buffed.upgradeStats.buildingHealth,
            1e-9,"global health does not erase fence rarity");
        const auto persisted=run->saveSuspendedRunState();
        run->returnToMainMenu();require(run->loadSuspendedRunState(persisted),"weighted fence power survives save validation");
        requireNear(run->snapshot().runUpgradePower[index],expectedPower,1e-9,"load preserves fractional fence power");
        requireNear(run->snapshot().buildings[1].maxHealth,buffed.buildings[1].maxHealth,1e-9,"load does not change fence HP");
    }
    BuildingSystem buildings;buildings.setCombatCardProgressionEnabled(true);
    require(buildings.place(BuildingType::Core,{0,0},0,99999,99999,99999).has_value(),"placement core fixture");
    static_cast<void>(buildings.setCombatBlueprintLevel(BuildingType::Wall,2,2.6));
    const auto wall=buildings.place(BuildingType::Wall,{2,0},0,99999,99999,99999);
    const auto gate=buildings.place(BuildingType::Gate,{4,0},0,99999,99999,99999);
    const double expected=wallMaxHealthAtPower(buildings.baseMaxHealth(BuildingType::Wall),2.6);
    require(wall&&gate&&wall->building.maxHealth==expected&&gate->building.maxHealth==expected,
        "new fences and doors inherit accumulated rarity bonuses");
}
