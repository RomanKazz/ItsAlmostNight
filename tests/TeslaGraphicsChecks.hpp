#pragma once
#include "assets/GlbCollision.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/ChainLightningEffect.hpp"
#include "app/AppRenderSupport.hpp"
#include "ui/GameUi.hpp"
#include <cstdlib>
#include <filesystem>
inline bool checkTeslaGraphics(ian::Renderer& renderer) {
    using namespace ian;
    const auto collision=loadGlbCollisionAsset("assets/models/buildings/tesla_tower.glb");
    if(!collision.valid() || collision.colliders.size()!=1 || collision.sockets.size()!=1) return false;
    if(std::abs(collision.sockets[0].position.y-1.26)>1e-5) return false;
    GraphicsResources resources;resources.initialize({});GameUi ui;ui.initialize();
    if(!resources.teslaTowerModel().valid() || resources.teslaTowerModel().get().meshCount!=2 ||
        ui.buildingTexture(BuildingType::TeslaTower).id==0) return false;
    const RenderTexture2D texture=LoadRenderTexture(800,600);
    const Camera3D camera{{3,2.8F,-5},{0,.9F,0},{0,1,0},4.5F,CAMERA_ORTHOGRAPHIC};
    Image images[3]{};
    for(int frame=0;frame<3;++frame) {
        renderer.beginFrame();BeginTextureMode(texture);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting lighting{};lighting.cameraPosition=camera.position;renderer.beginWorldShader(lighting);
        if(!renderer.drawTeslaTower({},0)) return false;
        WorldMaterialState material{};material.unlitAmount=1;material.inkOutlineEligible=false;renderer.setWorldMaterial(material);
        if(frame==2) DrawCube({0,1.2F,-2.8F},10,7,.2F,{29,47,57,255});
        renderer.beginColorOnlyWorldEffects();fire_wand_effect::beginBatch();
        chain_lightning_effect::emit({0,1.26F,-.2F},{-1.6F,.65F,-1},camera.position,.08F+static_cast<float>(frame%2)*.065F,.42F,918);
        chain_lightning_effect::emit({-1.6F,.65F,-1},{1.4F,.65F,-1.6F},camera.position,.08F+static_cast<float>(frame%2)*.065F,.42F,919);
        fire_wand_effect::endBatch();renderer.endColorOnlyWorldEffects();renderer.endWorldShader();EndMode3D();EndTextureMode();
        images[frame]=LoadImageFromTexture(texture.texture);ImageFlipVertical(&images[frame]);
        if(const char* output=std::getenv("IAN_TESLA_CAPTURE")) {
            std::filesystem::create_directories(output);const auto path=std::filesystem::path(output)/("tesla-"+std::to_string(frame)+".png");
            ExportImage(images[frame],path.string().c_str());
        }
    }
    auto* first=LoadImageColors(images[0]);auto* second=LoadImageColors(images[1]);auto* covered=LoadImageColors(images[2]);
    int orange=0,blue=0,changed=0,hidden=0;
    for(int i=0;i<800*600;++i) {
        orange+=first[i].r>100 && first[i].r>first[i].b*1.3;
        blue+=first[i].b>150 && first[i].g>100 && first[i].b>first[i].r*1.2;
        changed+=std::abs(int(first[i].r)-int(second[i].r))>15 || std::abs(int(first[i].g)-int(second[i].g))>15;
        hidden+=covered[i].b>150 && covered[i].g>100;
    }
    UnloadImageColors(first);UnloadImageColors(second);UnloadImageColors(covered);
    for(auto image:images)UnloadImage(image);
    const BuildingInstance building{.type=BuildingType::TeslaTower,.gridPosition={0,0}};
    const Ray ray{camera.position,Vector3Normalize(Vector3Subtract(camera.target,camera.position))};
    const bool cast=renderer.buildingRaycastDistance(building,{&building,1},ray,10,0,0).has_value();
    // Hover uses the same 360-degree range; no rear sector blind zone.
    const auto rangeRender=[&](bool hovered) {
        renderer.beginFrame();BeginTextureMode(texture);ClearBackground({29,47,57,255});
        const Camera3D top{{0,20,0},{0,0,0},{0,0,-1},20,CAMERA_ORTHOGRAPHIC};BeginMode3D(top);
        SimulationSnapshot snapshot;snapshot.buildings={&building,1};
        if(hovered)snapshot.aimedBuilding=building.id;
        FinalAttackStats stats;stats.range=8;snapshot.aimedFinalAttackStats=stats;
        app_detail::drawBuildingTacticalOverlay(renderer,snapshot);EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(texture.texture);auto* pixels=LoadImageColors(image);int count=0;
        for(int i=0;i<800*600;++i)count+=pixels[i].r>120 && std::abs(int(pixels[i].r)-int(pixels[i].g))<20;
        UnloadImageColors(pixels);UnloadImage(image);return count;
    };
    const bool circle=rangeRender(true)>rangeRender(false)+1000;
    UnloadRenderTexture(texture);ui.shutdown();resources.shutdown();
    return orange>1000 && blue>100 && changed>100 && hidden==0 && cast && circle;
}
