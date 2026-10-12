#pragma once

#include "graphics/Renderer.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>

inline bool checkPowerupPickupModels(ian::Renderer& renderer) {
    using namespace ian;
    RenderTextureResource target;
    if (!target.load(960,600)) return false;
    const Camera3D camera{{0,2.5F,7},{0,.8F,0},{0,1,0},42,CAMERA_PERSPECTIVE};
    bool loaded = true;
    const auto capture = [&](float angle, bool models, bool blocked) {
        BeginTextureMode(target.get());
        ClearBackground({22,37,57,255});
        BeginMode3D(camera);
        DrawPlane({0,-.02F,0},{20,20},{35,70,50,255});
        if (blocked) DrawCube({0,1,3},8,5,.3F,{22,37,57,255});
        if (models) for (std::size_t i=0;i<3;++i)
            loaded &= renderer.drawPowerupPickup(i,{(float(i)-1)*1.6F,.8F,0},angle);
        EndMode3D();
        EndTextureMode();
        Image image=LoadImageFromTexture(target.get().texture);
        ImageFlipVertical(&image);
        return image;
    };
    const auto difference = [](Image a, Image b) {
        int changed=0;
        for(int y=0;y<a.height;++y) for(int x=0;x<a.width;++x) {
            const Color first=GetImageColor(a,x,y),second=GetImageColor(b,x,y);
            if (std::abs(int(first.r)-int(second.r))+std::abs(int(first.g)-int(second.g))+
                std::abs(int(first.b)-int(second.b))>15) ++changed;
        }
        return changed;
    };
    Image empty=capture(0,false,false),front=capture(0,true,false),
        turned=capture(.8F,true,false),back=capture(PI,true,false),
        blocker=capture(0,false,true),hidden=capture(0,true,true);
    const int visible=difference(empty,front),rotation=difference(front,turned),
        rear=difference(empty,back),occluded=difference(blocker,hidden);
    const bool valid=loaded && visible>2500 && rotation>800 && rear>1500 && occluded<5;
    if(!valid) std::cerr << "Powerup model load/visibility/rotation/rear/depth: "
        << loaded << ' ' << visible << ' ' << rotation << ' ' << rear << ' ' << occluded << '\n';
    if(const char* directory=std::getenv("IAN_CAPTURE_POWERUP_MODELS")) {
        std::filesystem::create_directories(directory);
        for(auto view : {std::pair{"front",front},std::pair{"turned",turned},std::pair{"back",back}})
            ExportImage(view.second,(std::filesystem::path(directory)/(std::string(view.first)+".png")).string().c_str());
    }
    for(Image image:{empty,front,turned,back,blocker,hidden}) UnloadImage(image);
    return valid;
}
