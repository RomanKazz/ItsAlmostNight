#pragma once
#include "ui/RewardBackdrop.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
#include "TestHarness.hpp"

inline void checkRewardBackdrops(ian::GameUi& ui,const ian::SimulationSnapshot& shop,const char* output) {
    using namespace ian;
    const auto target=LoadRenderTexture(1920,1080);
    const auto capture=[&](int mode,double seconds,bool reduced=false) {
        BeginTextureMode(target);ClearBackground({40,70,65,255});
        if(mode==0) app_detail::drawRunShopOverlay(ui,shop,std::nullopt,Vector2{-100,-100},seconds,reduced);
        else reward_backdrop::draw(ui,mode==1?reward_backdrop::Kind::Coins:reward_backdrop::Kind::Experience,
            seconds,{1920,1080},mode==3?0:1,reduced);
        EndTextureMode();Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
    };
    const auto difference=[](Image a,Image b) {
        Color* first=LoadImageColors(a);Color* second=LoadImageColors(b);int count=0;
        for(int i=0;i<a.width*a.height;++i) if(std::abs(int(first[i].r)-int(second[i].r))+
            std::abs(int(first[i].g)-int(second[i].g))+std::abs(int(first[i].b)-int(second[i].b))>9) ++count;
        UnloadImageColors(first);UnloadImageColors(second);return count;
    };
    Image hidden=capture(0,0),early=capture(0,.07),ready=capture(0,1),coins=capture(1,1),
        coinsNext=capture(1,4),xp=capture(2,1),xpNext=capture(2,4),quiet=capture(2,1,true),
        quietNext=capture(2,4,true),transparent=capture(3,4),quietCoins=capture(1,1,true),
        quietCoinsNext=capture(1,4,true),quietShop=capture(0,1,true),quietShopNext=capture(0,4,true);
    require(difference(hidden,early)>10000 && difference(early,ready)>10000,
        "shop entrance visibly fades and grows before settling");
    require(difference(hidden,coins)>500 && difference(coins,coinsNext)>500 &&
        difference(hidden,xp)>500 && difference(xp,xpNext)>500,
        "coin and XP backgrounds remain visible and animated");
    require(difference(quiet,quietNext)>500 && difference(quietCoins,quietCoinsNext)>500 &&
        difference(quietShop,quietShopNext)>500,
        "reduce flashes retains continuous XP rain, coin rain and settled shop animation");
    require(difference(xp,quiet)>500 && difference(hidden,transparent)==0,
        "reduce flashes dims decorations and zero opacity draws nothing");
    if(output) {
        for(auto view:{std::pair{"shop-entering",early},std::pair{"shop-settled",ready},
                       std::pair{"xp-background",xp},std::pair{"coin-background",coins},
                       std::pair{"shop-reduced-flashes",quietShop},std::pair{"shop-reduced-flashes-next",quietShopNext}})
            require(ExportImage(view.second,(std::string(output)+"/"+view.first+".png").c_str()),"reward overlay capture exported");
    }
    for(Image image:{hidden,early,ready,coins,coinsNext,xp,xpNext,quiet,quietNext,transparent,
                    quietCoins,quietCoinsNext,quietShop,quietShopNext}) UnloadImage(image);
    UnloadRenderTexture(target);
}
