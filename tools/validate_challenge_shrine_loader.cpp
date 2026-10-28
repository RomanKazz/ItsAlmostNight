// Focused native asset/render probe. Link with the game's existing graphics libs.
#include "graphics/Renderer.hpp"
#include <raymath.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
Rectangle visibleBounds(const Image& image) {
    int left=image.width, top=image.height, right=0, bottom=0;
    for (int y=0;y<image.height;++y) for (int x=0;x<image.width;++x) {
        const Color c=GetImageColor(image,x,y);
        if (c.r+c.g+c.b<5) continue;
        left=std::min(left,x);right=std::max(right,x);
        top=std::min(top,y);bottom=std::max(bottom,y);
    }
    return {static_cast<float>(left),static_cast<float>(top),
        static_cast<float>(right-left+1),static_cast<float>(bottom-top+1)};
}
}
int main() {
    SetConfigFlags(FLAG_WINDOW_HIDDEN);InitWindow(1000,1000,"Challenge shrine check");
    SetTraceLogLevel(LOG_WARNING);
    try {
        ian::ModelResource model;
        require(model.load("assets/models/challenges/skull_column.glb")&&model.valid(),"model load failed");
        const auto bounds=model.visualBounds();
        require(std::abs(bounds.min.y)<1e-5F&&std::abs(bounds.max.y-3.25F)<1e-5F,"floor / height");
        require(bounds.min.x>=-.7501F&&bounds.max.x<=.7501F&&bounds.min.z>=-.7501F&&bounds.max.z<=.7501F,"selection bounds");
        require(model.get().meshCount==2,"mesh count");
        for (int i=0;i<model.get().materialCount;++i)
            require(IsTextureValid(model.get().materials[i].maps[MATERIAL_MAP_ALBEDO].texture),"embedded texture");
        model.unload();
        ian::Renderer renderer;renderer.initialize();
        const auto target=LoadRenderTexture(800,900);
        const Camera3D camera{{0,1.6F,8},{0,1.6F,0},{0,1,0},4,CAMERA_ORTHOGRAPHIC};
        const auto render=[&](Color tint,float scale,float yaw) {
            BeginTextureMode(target);ClearBackground(BLACK);BeginMode3D(camera);
            ian::WorldLighting lighting{};lighting.cameraPosition=camera.position;
            renderer.beginWorldShader(lighting);
            ian::WorldMaterialState material{};material.unlitAmount=1;material.screenAoAmount=0;
            renderer.setWorldMaterial(material);
            require(renderer.drawChallengeColumn({0,0,0},yaw,tint,scale),"draw failed");
            renderer.endWorldShader();EndMode3D();EndTextureMode();
            Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);return image;
        };
        Image normal=render(WHITE,1,0), back=render(WHITE,1,PI), active=render({205,235,255,255},1,0),
              shrinking=render(WHITE,.5F,0);
        const auto full=visibleBounds(normal), half=visibleBounds(shrinking), rear=visibleBounds(back);
        require(full.height>600&&std::abs(full.height-rear.height)<2&&std::abs(full.width-rear.width)<2,"front/back projection");
        require(half.height>full.height*.48F&&half.height<full.height*.52F,"completion shrink");
        int changed=0;
        for (int y=0;y<normal.height;++y) for (int x=0;x<normal.width;++x) {
            const Color a=GetImageColor(normal,x,y),b=GetImageColor(active,x,y);
            changed+=std::abs(static_cast<int>(a.r)-b.r)>5;
        }
        require(changed>1000,"active tint");
        ExportImage(normal,"art/models/challenge_shrine/runtime-front.png");
        ExportImage(back,"art/models/challenge_shrine/runtime-back.png");
        ExportImage(active,"art/models/challenge_shrine/runtime-active.png");
        ExportImage(shrinking,"art/models/challenge_shrine/runtime-shrinking.png");
        for (Image image:{normal,back,active,shrinking}) UnloadImage(image);
        const Camera3D litCamera{{3.3F,3.3F,7},{0,1.6F,0},{0,1,0},40,CAMERA_PERSPECTIVE};
        BeginTextureMode(target);ClearBackground({106,163,199,255});BeginMode3D(litCamera);
        ian::WorldLighting lighting{};lighting.cameraPosition=litCamera.position;
        renderer.beginWorldShader(lighting);ian::WorldMaterialState material{};material.screenAoAmount=0;
        renderer.setWorldMaterial(material);DrawPlane({0,-.01F,0},{200,200},{85,133,47,255});
        require(renderer.drawChallengeColumn({0,0,0},0,WHITE),"lit draw failed");
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        Image lit=LoadImageFromTexture(target.texture);ImageFlipVertical(&lit);
        ExportImage(lit,"art/models/challenge_shrine/game-preview.png");UnloadImage(lit);
        UnloadRenderTexture(target);renderer.shutdown();
        nlohmann::json report{{"platform","macOS arm64"},{"model_resource_load",true},
            {"embedded_texture",true},{"floor_and_height",true},{"selection_bounds",true},
            {"front_back_render",true},{"active_tint",true},{"completion_shrink",true},
            {"world_shader_render",true},{"gameplay_code_changed",false},{"manual_gameplay_playtest",false}};
        std::ofstream("art/models/challenge_shrine/runtime-validation.json")<<report.dump(2)<<'\n';
        CloseWindow();std::cout<<"Challenge shrine loader and render checks passed\n";return 0;
    } catch (const std::exception& error) {
        CloseWindow();std::cerr<<error.what()<<'\n';return 1;
    }
}
