#pragma once

#include "assets/GlbCollision.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/Renderer.hpp"
#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudHotbar.hpp"
#include "ui/UiText.hpp"
#include "ui/UiViewport.hpp"

#include <cstdlib>

inline bool checkFlamethrowerGraphics(ian::Renderer& renderer) {
    using namespace ian;
    const auto collision=loadGlbCollisionAsset("assets/models/buildings/flamethrower.glb");
    ModelResource model;
    if (!collision.valid() || collision.colliders.size()!=1 || collision.sockets.size()!=1 ||
        collision.sockets[0].name!="MUZZLE_01" || !model.load("assets/models/buildings/flamethrower.glb") ||
        model.get().meshCount!=3 || std::abs(renderer.buildingVisualHeight(BuildingType::Flamethrower)-1.14F)>.001F)
        return false;
    GameUi ui; ui.initialize();
    if (!IsTextureValid(ui.buildingTexture(BuildingType::Flamethrower))) return false;
    const auto target=LoadRenderTexture(1280,720);
    const Camera3D camera{{2.8F,2.2F,-4.8F},{0,.6F,0},{0,1,0},3.3F,CAMERA_ORTHOGRAPHIC};
    const Color background{55,88,89,255};
    renderer.beginFrame();
    BeginTextureMode(target);
    ClearBackground(background);
    BeginMode3D(camera);
    WorldLighting lighting{}; lighting.cameraPosition=camera.position;
    renderer.beginWorldShader(lighting);
    DrawPlane({0,-.02F,0},{8,8},{104,137,86,255});
    const bool visible=renderer.drawFlamethrower({-.85F,0,0},0);
    static_cast<void>(renderer.drawCannon({.85F,0,0},0,0));
    renderer.endWorldShader();
    EndMode3D();
    SimulationSnapshot snapshot;
    snapshot.coreId=EntityId{1,1}; snapshot.coreMaxHealth=500;
    snapshot.coreLevel=1; snapshot.unlimitedResources=true;
    snapshot.unlockedBuildings.fill(true);
    snapshot.selectedBuilding=BuildingType::Flamethrower;
    snapshot.buildingCosts.fill({35,25,10});
    HudViewState view; view.buildingHotbarCategory=BuildingHotbarCategory::Defense;
    view.buildHotbarSelectionPosition=6; view.buildHotbarSelectionAlpha=1;
    const UiViewport viewport{{1280,720},1};
    const auto* previousViewport=ui_viewport_detail::current;
    ui_viewport_detail::current=&viewport;
    beginSdfUiTextBatch();
    hud_detail::drawBuildHotbar(ui,snapshot,view);
    endSdfUiTextBatch();
    ui_viewport_detail::current=previousViewport;
    EndTextureMode();
    Image capture=LoadImageFromTexture(target.texture); ImageFlipVertical(&capture);
    if (const auto* path=std::getenv("IAN_FLAMETHROWER_CAPTURE")) ExportImage(capture,path);
    UnloadImage(capture); UnloadRenderTexture(target);
    return visible;
}
