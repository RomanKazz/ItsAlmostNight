#include "TestHarness.hpp"
#include "game/Simulation.hpp"
#include <memory>
#include <limits>
void runRunShopTests() {
    using Effect=ian::RunUpgradeEffect;
    using Rarity=ian::RunUpgradeRarity;
    require(ian::runShopPrice(Effect::Damage,Rarity::Common,1)==12 &&
        ian::runShopPrice(Effect::Damage,Rarity::Common,5)==26 &&
        ian::runShopPrice(Effect::Damage,Rarity::Common,10)==49,
        "shop price scales by wave with upward rounding");
    for(const auto effect:{Effect::Damage,Effect::UnlockCannon}) for(int rarity=0;rarity<5;++rarity) {
        int previous=ian::runShopPrice(effect,static_cast<Rarity>(rarity),0);
        for(int wave=2;wave<=100;++wave) {
            const int price=ian::runShopPrice(effect,static_cast<Rarity>(rarity),wave);
            require(price>previous,"every wave raises every rarity and blueprint price");previous=price;
        }
    }
    require(ian::runShopPrice(Effect::Damage,Rarity::Legendary,std::numeric_limits<int>::max())==std::numeric_limits<int>::max(),
        "extreme shop waves saturate without signed overflow");
    for(int wave=2;wave<100;++wave)
        require(ian::runShopRerollPrice({.wave=wave})>ian::runShopRerollPrice({.wave=wave-1}),
            "first reroll becomes dearer on every wave");
    auto map = ian::MapDefinition::defaults(); map.resources.clear(); map.obstacles.clear();
    auto world = ian::WorldConfig::defaults(); world.terrainAmplitude = 0;
    world.pondMinimumCount = world.pondMaximumCount = 0;
    auto balance = ian::GameBalance::defaults();
    balance.buildings[static_cast<std::size_t>(ian::BuildingType::Core)].wood = 0;
    auto run = std::make_unique<ian::Simulation>(balance,map,world);
    run->startRunFromSeed(ian::PlayerClass::Vanguard,12345);
    require(!run->openRunShop(), "shop unavailable before the first completed wave");
    require(!run->openRunShop(true) && !run->snapshot().runShop.pending,
        "temporarily disabled shop cannot open through its shortcut");
    auto suspended = run->saveSuspendedRunState();
    suspended.runShop.pending = true;
    auto resumedWithOldShop = std::make_unique<ian::Simulation>(balance,map,world);
    require(resumedWithOldShop->loadSuspendedRunState(suspended) &&
        !resumedWithOldShop->snapshot().runShop.pending,
        "a suspended run with an old open shop resumes without the overlay");
}

#ifdef IAN_RUN_SHOP_TEST_MAIN
int main() { runRunShopTests(); }
#endif
