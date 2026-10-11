#pragma once
#include "app/PowerupDrawing.hpp"
#include "ui/TargetHealthBar.hpp"
#include <filesystem>
#include <iostream>

inline bool checkWorldUiOcclusion() {
    using namespace ian;
    ShaderResource shader;
    if (!shader.load(nullptr,"assets/shaders/world_ui_occlusion.fs")) return false;
    RenderTextureResource scene, overlay;
    if (!scene.loadScreenSpace(160,100) || !overlay.load(320,200)) return false;
    const Camera3D camera{{0,1,8},{0,1,0},{0,1,0},4,CAMERA_ORTHOGRAPHIC};
    const auto render = [&](float z, bool blocker) {
        BeginTextureMode(scene.get()); ClearBackground(BLACK); BeginMode3D(camera);
        if (blocker) DrawCube({0,1,z},2,2,.3F,GREEN);
        EndMode3D(); EndTextureMode();
        BeginTextureMode(overlay.get()); ClearBackground(BLACK); BeginMode3D(camera);
        rlDisableDepthTest(); BeginShaderMode(shader.get());
        const Vector2 viewport{320,200};
        SetShaderValueTexture(shader.get(),GetShaderLocation(shader.get(),"sceneDepth"),scene.get().depth);
        SetShaderValue(shader.get(),GetShaderLocation(shader.get(),"viewportSize"),&viewport,SHADER_UNIFORM_VEC2);
        drawWorldBillboardRoundedRectangle({0,1,0},1.2F,.25F,.04F,RED,{1,0,0},{0,1,0});
        EndShaderMode(); rlEnableDepthTest(); EndMode3D(); EndTextureMode();
        Image image=LoadImageFromTexture(overlay.get().texture);
        int red=0;
        for (int y=0;y<image.height;++y) for (int x=0;x<image.width;++x) {
            const auto c=GetImageColor(image,x,y); if(c.r>180 && c.g<80) ++red;
        }
        UnloadImage(image); return red;
    };
    const int clear=render(2,false), hidden=render(2,true), inFront=render(-2,true);
    if (clear<100 || hidden>4 || inFront<clear*.95) {
        std::cerr << "World health bar occlusion: " << clear << ' ' << hidden << ' ' << inFront << '\n';
        return false;
    }
    // SSAO disabled still needs a texture-backed depth attachment.
    if (!scene.loadWithDepthTexture(160,100) || render(2,true)>4 || render(-2,true)<100) return false;
    return true;
}

inline bool checkPowerupPresentation(ian::Renderer& renderer) {
    using namespace ian;
    RenderTextureResource target; if (!target.load(960,600)) return false;
    initializeUiText();
    const Camera3D camera{{0,3,9},{0,1.4F,0},{0,1,0},45,CAMERA_PERSPECTIVE};
    SimulationSnapshot snapshot{};
    snapshot.elapsedSeconds=2.4;
    snapshot.powerups.remaining={4,12,18};
    for (std::size_t i=0;i<3;++i) snapshot.powerups.pickups[i]={{(double(i)-1)*2.4,0,1},static_cast<PowerupType>(i),1.3,true};
    std::array<BuildingInstance,2> buildings{};
    for (std::size_t i=0;i<2;++i) {
        buildings[i].id={static_cast<std::uint32_t>(i+1),1}; buildings[i].type=BuildingType::GunTurret;
        buildings[i].gridPosition={int(i)*3-2,-2}; buildings[i].level=1; buildings[i].health=100;
    }
    snapshot.buildings=buildings;
    BeginTextureMode(target.get()); ClearBackground({23,45,65,255}); BeginMode3D(camera);
    DrawPlane({0,-.05F,0},{20,20},{38,78,58,255});
    for(const auto& b:buildings) { const auto p=buildingWorldPosition(b); DrawCube({float(p.x),float(p.y)+.4F,float(p.z)},1,.8F,1,{200,107,53,255}); }
    powerup_drawing::drawWorld(snapshot,camera,renderer,false);
    EndMode3D();
    const ScopedSdfUiTextCache noCache(false);
    powerup_drawing::drawHud(snapshot.powerups,2.4F,960,600,false);
    EndTextureMode();
    Image image=LoadImageFromTexture(target.get().texture); ImageFlipVertical(&image);
    int cyan=0,purple=0,gold=0;
    for(int y=0;y<image.height;++y) for(int x=0;x<image.width;++x) {
        const auto c=GetImageColor(image,x,y);
        if(c.g>150 && c.b>180 && c.r<120) ++cyan;
        if(c.r>120 && c.b>180 && c.g<140) ++purple;
        if(c.r>200 && c.g>140 && c.b<90) ++gold;
    }
    if(std::getenv("IAN_CAPTURE_POWERUPS")) {
        std::filesystem::create_directories("/tmp/ian-powerups-hud-2026-10-11");
        ExportImage(image,"/tmp/ian-powerups-hud-2026-10-11/powerups.png");
    }
    UnloadImage(image); shutdownUiText();
    return cyan>100 && purple>100 && gold>100;
}
