#pragma once

#include "graphics/PortalArrivalEffect.hpp"
#include "graphics/Renderer.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

inline bool checkPortalArrivalGraphics(ian::Renderer& renderer) {
    using namespace ian;
    const auto target=LoadRenderTexture(960,640);
    const Camera3D camera{{0,3,9},{0,1.4F,0},{0,1,0},5.5F,CAMERA_ORTHOGRAPHIC};
    bool loaded=true;
    const auto capture=[&](int mode,float progress,bool night=false) {
        BeginTextureMode(target);ClearBackground({22,35,51,255});BeginMode3D(camera);
        WorldLighting light{};light.cameraPosition=camera.position;
        if(night) {light.sunIntensity=.03F;light.ambientIntensity=.18F;}
        if(mode==1) {
            DrawPlane({0,-.05F,0},{20,20},{48,76,59,255});
            loaded &= renderer.drawEnemyPortal({0,0,0},-PI/2,progress*.8F,light,.72F,progress,
                std::clamp(progress*1.6F,0.0F,1.0F));
        }
        renderer.beginWorldShader(light);
        if(mode==1) {
            const auto pose=portal_arrival::pose(progress);
            loaded &= renderer.drawEnemy(EnemyModelVisual::Minion,EnemyAnimationVisual::Walk,progress,
                {0,pose.hop+.02F,.9F-.65F*(1-progress)*(1-progress)},0,WHITE,.94F,
                true,true,false,0,pose.stretch,pose.roll);
        }
        if(mode==3 || mode==4) DrawCube({0,2,3},15,10,.2F,GREEN);
        if(mode==1 || mode==2 || mode==4 || mode==5) {
            WorldMaterialState material{};material.unlitAmount=1;material.screenAoAmount=0;
            material.inkOutlineEligible=false;material.inkOccluder=false;
            renderer.setWorldMaterial(material);renderer.beginColorOnlyWorldEffects();
            fire_wand_effect::beginBatch(false);
            if(mode!=1) portal_arrival::gate({0,1.25F,0},{0,0,1},camera.position,progress,mode!=5);
            portal_arrival::exit({0,0,.9F},camera.position,progress,.85F,mode!=5,17);
            fire_wand_effect::endBatch();renderer.endColorOnlyWorldEffects();
        }
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
    };
    const auto difference=[](Image a,Image b) {
        Color* first=LoadImageColors(a);Color* second=LoadImageColors(b);int count=0;
        for(int i=0;i<a.width*a.height;++i) if(std::abs(int(first[i].r)-int(second[i].r))+
            std::abs(int(first[i].g)-int(second[i].g))+std::abs(int(first[i].b)-int(second[i].b))>6) ++count;
        UnloadImageColors(first);UnloadImageColors(second);return count;
    };
    Image empty=capture(0,.3F),early=capture(1,.08F),burst=capture(1,.30F),late=capture(1,.65F),
        idle=capture(1,1),energy=capture(2,.3F),next=capture(2,.6F),night=capture(2,.3F,true),
        cover=capture(3,.3F),hidden=capture(4,.3F),low=capture(5,.3F),expired=capture(2,1);
    const int motion=difference(energy,next),visible=difference(empty,energy),blocked=difference(cover,hidden);
    const auto settled=portal_arrival::pose(1);
    const bool valid=loaded && visible>3000 && motion>1000 && blocked==0 && difference(energy,night)==0 &&
        difference(early,burst)>2000 && difference(burst,late)>1000 && difference(late,idle)>1000 &&
        difference(empty,low)>1000 && difference(empty,expired)==0 && settled.stretch.x==1 && settled.stretch.y==1;
    if(!valid) std::cerr<<"Portal arrival visible/motion/depth: "<<visible<<' '<<motion<<' '<<blocked<<'\n';
    if(const char* directory=std::getenv("IAN_CAPTURE_PORTAL_ARRIVAL")) {
        std::filesystem::create_directories(directory);
        for(auto view:{std::pair{"early",early},std::pair{"burst",burst},std::pair{"late",late},
                       std::pair{"settled",idle},std::pair{"energy",energy}})
            ExportImage(view.second,(std::filesystem::path(directory)/(std::string(view.first)+".png")).string().c_str());
    }
    for(Image image:{empty,early,burst,late,idle,energy,next,night,cover,hidden,low,expired}) UnloadImage(image);
    UnloadRenderTexture(target);return valid;
}
