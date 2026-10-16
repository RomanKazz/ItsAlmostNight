#pragma once
#include "graphics/Renderer.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/LaserModel.hpp"
#include "graphics/LaserBeamEffect.hpp"
#include "ui/GameUi.hpp"
#include <filesystem>
#include <cstdlib>
inline bool checkLaserGraphics(ian::Renderer& renderer) {
    using namespace ian;
    GraphicsResources resources;resources.initialize({});GameUi ui;ui.initialize();
    auto& model=resources.laserTowerModel();
    if(!model.valid() || model.get().meshCount!=4 || model.collisionAsset().sockets.size()!=1 ||
        ui.buildingTexture(BuildingType::LaserTower).id==0) return false;
    const auto neutral=laserMuzzleWorldPosition({},0,0);
    for(float yaw:{0.0F,.65F,2.1F})for(float pitch:{-.2F,0.0F,.34F}) {
        const auto world=laserMuzzleWorldPosition({},yaw,pitch);
        const auto render=Vector3Transform({float(neutral.x),float(neutral.y),float(neutral.z)},
            laser_model::meshTransform(2,{},0,yaw,pitch));
        if(std::abs(render.x-world.x)>1e-5 || std::abs(render.y-world.y)>1e-5 || std::abs(render.z-world.z)>1e-5) return false;
    }
    const auto target=LoadRenderTexture(800,600);
    const Camera3D camera{{3,2.6F,-5},{0,.8F,0},{0,1,0},4.5F,CAMERA_ORTHOGRAPHIC};
    int colors[4]{};Image frames[4]{};
    for(int frame=0;frame<4;++frame) {
        renderer.beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting lighting{};lighting.cameraPosition=camera.position;renderer.beginWorldShader(lighting);
        if(!renderer.drawLaserTower({},0,WHITE,1,.15F,.25F)) return false;
        WorldMaterialState material{};material.unlitAmount=1;material.inkOutlineEligible=false;material.inkOccluder=true;
        renderer.setWorldMaterial(material);
        if(frame==2) DrawCube({0,1,-2.8F},10,7,.2F,{29,47,57,255});
        renderer.beginColorOnlyWorldEffects();fire_wand_effect::beginBatch();
        const auto muzzle=laserMuzzleWorldPosition({},.25,.15);
        for(int ray=0;ray<(frame==3?3:1);++ray) {
            auto origin=chain_lightning_effect::point(muzzle);
            Vector3 end{-1.2F,.7F,-2};
            const float lane=frame==3?float(ray)/2-.5F:0;
            origin=laser_beam_effect::parallelPoint(origin,camera.position,lane,.16F);
            end=laser_beam_effect::parallelPoint(end,camera.position,lane,.36F);
            laser_beam_effect::emit(origin,end,camera.position,
                frame==0?0.0F:1.0F,.3F+float(frame)*.15F);
            if(frame==3)laser_beam_effect::emit(end,{.7F,.7F,-2.4F+lane*.36F},camera.position,1,.7F);
        }
        fire_wand_effect::endBatch();renderer.endColorOnlyWorldEffects();renderer.endWorldShader();EndMode3D();EndTextureMode();
        frames[frame]=LoadImageFromTexture(target.texture);ImageFlipVertical(&frames[frame]);auto* pixels=LoadImageColors(frames[frame]);
        for(int i=0;i<800*600;++i)colors[frame]+=pixels[i].b>170 && pixels[i].g>130;
        UnloadImageColors(pixels);
        if(const char* output=std::getenv("IAN_LASER_CAPTURE")) {
            std::filesystem::create_directories(output);const auto p=std::filesystem::path(output)/("laser-"+std::to_string(frame)+".png");
            ExportImage(frames[frame],p.string().c_str());
        }
    }
    const BuildingInstance building{.type=BuildingType::LaserTower,.gridPosition={0,0}};
    const Ray ray{camera.position,Vector3Normalize(Vector3Subtract(camera.target,camera.position))};
    const bool aim=renderer.buildingRaycastDistance(building,{&building,1},ray,10,.25F,.15F).has_value();
    bool levelsValid=&resources.laserTowerModel(0)==&resources.laserTowerModel(1) &&
        &resources.laserTowerModel(99)==&resources.laserTowerModel(8);
    std::uint64_t previousHash=0;
    for(int level=1;level<=8;++level) {
        auto& variant=resources.laserTowerModel(level);
        levelsValid=levelsValid && variant.valid() && variant.get().meshCount==4 &&
            variant.collisionAsset().colliders.size()==1 && variant.collisionAsset().sockets.size()==1 &&
            renderer.buildingVisualHeight(BuildingType::LaserTower,level)==(level==1?1.4F:1.5F);
        if(!levelsValid) break;
        const auto& muzzle=variant.collisionAsset().sockets.front();
        levelsValid=std::abs(muzzle.position.x-neutral.x)<1e-5 &&
            std::abs(muzzle.position.y-neutral.y)<1e-5 && std::abs(muzzle.position.z-neutral.z)<1e-5;
        renderer.beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting lighting{};lighting.cameraPosition=camera.position;renderer.beginWorldShader(lighting);
        levelsValid=renderer.drawLaserTower({},0,WHITE,1,.15F,.25F,level) && levelsValid;
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
        auto* pixels=LoadImageColors(image);std::uint64_t hash=1469598103934665603ULL;
        for(int i=0;i<image.width*image.height;++i) {
            hash=(hash^pixels[i].r)*1099511628211ULL;
            hash=(hash^pixels[i].g)*1099511628211ULL;
        }
        levelsValid=levelsValid && (level==1 || hash!=previousHash);previousHash=hash;
        if(const char* output=std::getenv("IAN_LASER_CAPTURE")) {
            const auto path=std::filesystem::path(output)/("laser-level-"+std::to_string(level)+".png");
            ExportImage(image,path.string().c_str());
        }
        UnloadImageColors(pixels);UnloadImage(image);
        BuildingInstance upgraded{.type=BuildingType::LaserTower,.gridPosition={0,0},
            .level=static_cast<std::uint8_t>(level)};
        levelsValid=levelsValid && renderer.buildingRaycastDistance(upgraded,{&upgraded,1},ray,10,.25F,.15F).has_value();
    }
    for(auto image:frames)UnloadImage(image);
    UnloadRenderTexture(target);ui.shutdown();resources.shutdown();
    return levelsValid && colors[0]>200 && colors[1]>colors[0] && colors[2]==0 && colors[3]>colors[1]*1.3 && aim;
}
