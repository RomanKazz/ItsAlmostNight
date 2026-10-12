#pragma once

#include "graphics/PropBreakEffect.hpp"
#include "graphics/Renderer.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

inline bool checkPropBreakGraphics(ian::Renderer& renderer) {
    using namespace ian;
    const auto target=LoadRenderTexture(640,480);
    const Camera3D camera{{0,2,7},{0,.8F,0},{0,1,0},4.5F,CAMERA_ORTHOGRAPHIC};
    const auto capture=[&](float progress,bool detail=true,bool cover=false,bool night=false,bool barrel=false) {
        BeginTextureMode(target);ClearBackground({30,45,59,255});BeginMode3D(camera);
        if(cover) DrawCube({0,1,3},10,10,.2F,GREEN);
        WorldLighting light{};light.cameraPosition=camera.position;
        if(night) {light.sunIntensity=.03F;light.ambientIntensity=.15F;}
        renderer.beginWorldShader(light);
        WorldMaterialState material{};material.unlitAmount=1;material.screenAoAmount=0;
        material.inkOutlineEligible=false;material.inkOccluder=true;
        renderer.setWorldMaterial(material);renderer.beginColorOnlyWorldEffects();
        fire_wand_effect::beginBatch();
        prop_break_effect::burst({0,.65F,0},camera.position,progress,1,43,detail,barrel);
        fire_wand_effect::endBatch();renderer.endColorOnlyWorldEffects();renderer.endWorldShader();
        EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
    };
    const auto difference=[](Image a,Image b) {
        Color* first=LoadImageColors(a);Color* second=LoadImageColors(b);int count=0;
        for(int i=0;i<a.width*a.height;++i) if(std::abs(int(first[i].r)-int(second[i].r))+
            std::abs(int(first[i].g)-int(second[i].g))+std::abs(int(first[i].b)-int(second[i].b))>6) ++count;
        UnloadImageColors(first);UnloadImageColors(second);return count;
    };
    Image empty=capture(0),burst=capture(.22F),late=capture(.65F),expired=capture(1),
        low=capture(.22F,false),night=capture(.22F,true,false,true),
        cover=capture(0,true,true),hidden=capture(.22F,true,true),barrel=capture(.22F,true,false,false,true);
    const int visible=difference(empty,burst),motion=difference(burst,late),blocked=difference(cover,hidden);
    const bool valid=visible>2000 && motion>1000 && blocked==0 && difference(empty,expired)==0 &&
        difference(empty,low)>1000 && difference(burst,night)==0 && difference(burst,barrel)>50;
    if(!valid) std::cerr<<"Prop break visible/motion/depth: "<<visible<<' '<<motion<<' '<<blocked<<'\n';
    if(const char* directory=std::getenv("IAN_CAPTURE_PROP_BREAK")) {
        std::filesystem::create_directories(directory);
        for(auto view:{std::pair{"crate",burst},std::pair{"barrel",barrel},std::pair{"late",late}})
            ExportImage(view.second,(std::filesystem::path(directory)/(std::string(view.first)+".png")).string().c_str());
    }
    for(Image image:{empty,burst,late,expired,low,night,cover,hidden,barrel}) UnloadImage(image);
    UnloadRenderTexture(target);return valid;
}
