#pragma once
#include "ui/InteractionPrompt.hpp"
#include "ui/GameUi.hpp"
#include <filesystem>

inline void checkShrineResourcePrompt(ian::GameUi& ui,const char* captureDirectory) {
    SetWindowSize(1024,512);SetTargetFPS(60);
    const auto target=LoadRenderTexture(1024,512);
    const Camera3D camera{{0,0,8},{0,0,0},{0,1,0},4,CAMERA_ORTHOGRAPHIC};
    ian::InteractionPrompt prompt{
        .targetKind=ian::InteractionPromptTargetKind::Building,.targetId={1,1},
        .worldAnchor={0,0,0},.objectName="Player Shrine",.actionText="Receive blessing",
        .state=ian::InteractionState::Warning,.cost=ian::ResourceCost{20,12,4},
        .availableResources={0,99,99},
    };
    const auto render=[&](const ian::InteractionPrompt& current) {
        ian::InteractionPromptRenderer painter;
        for(int frame=0;frame<30;++frame) {
            BeginDrawing();BeginTextureMode(target);ClearBackground({24,32,40,255});
            painter.draw(current,camera,ui,{});EndTextureMode();EndDrawing();
        }
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
    };
    Image reference=render(prompt);
    prompt.targetKind=ian::InteractionPromptTargetKind::Landmark;prompt.showResourceCost=true;
    Image shrine=render(prompt);
    const Color* a=LoadImageColors(reference);const Color* b=LoadImageColors(shrine);
    int changed=0,red=0,white=0;
    // The top cost row must use the exact building-upgrade renderer, including
    // all three icons and per-resource affordability colors. E is below it.
    for(int y=225;y<287;++y) for(int x=280;x<744;++x) {
        const auto i=y*reference.width+x;
        changed+=std::abs(static_cast<int>(a[i].r)-b[i].r)>3 ||
            std::abs(static_cast<int>(a[i].g)-b[i].g)>3 || std::abs(static_cast<int>(a[i].b)-b[i].b)>3;
        red+=b[i].r>220 && b[i].g>65 && b[i].g<130 && b[i].b<120;
        white+=b[i].r>225 && b[i].g>225 && b[i].b>200;
    }
    if(captureDirectory) {
        std::filesystem::create_directories(captureDirectory);
        require(ExportImage(shrine,(std::filesystem::path(captureDirectory)/"shrine-resource-price.png").string().c_str()),"shrine resource price capture exports");
    }
    if(changed>=150 || red<=30 || white<=30) {
        std::cerr<<"shrine prompt pixels: changed="<<changed<<" red="<<red<<" white="<<white<<'\n';
    }
    require(changed<150 && red>30 && white>30,
        "shrine displays the same three-resource cost row as upgrades, with red missing and white available amounts");
    UnloadImageColors(const_cast<Color*>(a));UnloadImageColors(const_cast<Color*>(b));
    UnloadImage(reference);UnloadImage(shrine);UnloadRenderTexture(target);SetTargetFPS(0);
}
