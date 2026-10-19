#pragma once

#include "TestHarness.hpp"
#include "ui/TargetHealthBar.hpp"
#include "ui/TargetHealthBarAnchor.hpp"
#include "ui/HudBuildingContextCard.hpp"
#include "world/TerrainHeightfield.hpp"
#include <rlgl.h>
#include <filesystem>

inline void checkBuildingHoverUi(ian::GameUi& ui,const char* captureDirectory) {
    using namespace ian;
    SetWindowSize(1280,720);SetTargetFPS(60);
    const auto target=LoadRenderTexture(1280,720);
    auto world=WorldConfig::defaults();world.terrainAmplitude=0;
    world.pondMinimumCount=world.pondMaximumCount=0;
    TerrainHeightfield terrain(world);terrain.generate(23);
    std::array<BuildingInstance,1> buildings{};
    auto& building=buildings[0];building.id={61,1};building.type=BuildingType::Core;
    building.level=4;building.health=250;building.maxHealth=500;
    SimulationSnapshot snapshot{};snapshot.buildings=buildings;snapshot.aimedBuilding=building.id;
    const Camera3D camera{{0,2,7},{0,2,0},{0,1,0},60,CAMERA_PERSPECTIVE};
    const auto render=[&](const Camera3D& view,bool selected,const char* name) {
        TargetHealthBar bars;
        for(int frame=0;frame<12;++frame) {
            BeginDrawing();BeginTextureMode(target);ClearBackground({24,32,40,255});
            BeginMode3D(view);rlDisableDepthTest();
            bars.draw(snapshot,view,terrain,{},building.id,
                [](const BuildingInstance&){return 2.0;},selected?std::optional{building.id}:std::nullopt);
            rlDrawRenderBatchActive();rlEnableDepthTest();EndMode3D();
            bars.drawScreenOverlay();EndTextureMode();EndDrawing();
        }
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
        if(captureDirectory) ExportImage(image,(std::filesystem::path(captureDirectory)/name).string().c_str());
        return image;
    };
    const auto levelBounds=[](Image image) {
        Rectangle bounds{static_cast<float>(image.width),static_cast<float>(image.height),0,0};
        int right=0,bottom=0,count=0;
        auto* pixels=LoadImageColors(image);
        for(int y=0;y<image.height;++y) for(int x=0;x<image.width;++x) {
            const auto c=pixels[y*image.width+x];
            if(c.r<210 || c.g<185 || c.b<100 || c.b>215) continue;
            ++count;bounds.x=std::min(bounds.x,static_cast<float>(x));
            bounds.y=std::min(bounds.y,static_cast<float>(y));right=std::max(right,x);bottom=std::max(bottom,y);
        }
        UnloadImageColors(pixels);
        if(!count) return Rectangle{};
        bounds.width=static_cast<float>(right)-bounds.x+1;bounds.height=static_cast<float>(bottom)-bounds.y+1;
        return bounds;
    };
    Image selected=render(camera,true,"building-selected-world-level.png");
    Image attacked=render(camera,false,"building-damaged-bar-only.png");
    auto far=camera;far.position.z=14;
    Image distant=render(far,true,"building-distant-world-level.png");
    const auto nearText=levelBounds(selected),farText=levelBounds(distant);
    require(nearText.width>30 && nearText.height>5 && levelBounds(attacked).width==0,
        "player selection shows a level but damage-only building bars do not");
    require(farText.height>0 && farText.height<nearText.height*.7F && farText.width<nearText.width*.7F,
        "building level scales in the world with its health bar instead of keeping a HUD minimum size");
    for(Image image:{selected,attacked,distant}) UnloadImage(image);
    snapshot.aimedBuildingStats=BuildingStatComparison{
        .current={.maxHealth=500,.buildingLimit=3,.woodCapacity=120,.stoneCapacity=75,.crystalCapacity=75},
        .next=BuildingStats{.maxHealth=650,.buildingLimit=5,.woodCapacity=240,.stoneCapacity=150,.crystalCapacity=150}};
    HudViewState state{};state.buildingContextCardEntranceSeconds=.5F;
    BeginDrawing();BeginTextureMode(target);ClearBackground({24,32,40,255});
    hud_detail::drawBuildingContextCard(ui,snapshot,building,state,camera,ControlSettings{});
    EndTextureMode();EndDrawing();
    Image card=LoadImageFromTexture(target.texture);ImageFlipVertical(&card);
    auto* pixels=LoadImageColors(card);int translucentBlack=0;
    for(int i=0;i<card.width*card.height;++i) {
        const auto c=pixels[i];
        translucentBlack+=c.r>=14 && c.r<=16 && c.g>=18 && c.g<=21 && c.b>=23 && c.b<=26;
    }
    require(translucentBlack>5000,"building stats share the forty-percent black hint background");
    if(captureDirectory) ExportImage(card,(std::filesystem::path(captureDirectory)/"building-stats-rounded.png").string().c_str());
    UnloadImageColors(pixels);UnloadImage(card);UnloadRenderTexture(target);SetTargetFPS(0);
}
