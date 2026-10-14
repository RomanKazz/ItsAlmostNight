#pragma once
#include "assets/GlbCollision.hpp"
#include "graphics/MissileLauncherModel.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/GraphicsResources.hpp"
#include "app/AppProjectileDrawing.hpp"
#include "buildings/TowerRig.hpp"
#include "ui/GameUi.hpp"
#include <filesystem>
#include <cstdlib>
inline bool checkMissileLauncherGraphics(ian::Renderer& renderer) {
    using namespace ian;
    const auto asset = loadGlbCollisionAsset("assets/models/buildings/missile_launcher.glb");
    if (asset.colliders.size()!=1 || std::ranges::count_if(asset.sockets, [](const auto& socket){return socket.name.starts_with("MUZZLE_");})!=4) return false;
    GraphicsResources resources; resources.initialize({});
    if (!resources.missileLauncherModel().valid() || resources.missileLauncherModel().get().meshCount!=3) return false;
    GameUi ui;ui.initialize();
    if (ui.buildingTexture(BuildingType::MissileLauncher).id==0) return false;
    const RenderTexture2D target=LoadRenderTexture(800,600);
    const Camera3D camera{{3,2.4F,-4},{0,.8F,0},{0,1,0},3.0F,CAMERA_ORTHOGRAPHIC};
    int counts[2]{};
    for(int pose=0;pose<2;++pose) {
        renderer.beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting lighting{};lighting.cameraPosition=camera.position;renderer.beginWorldShader(lighting);
        const float yaw=pose==0?0:.6F, pitch=pose==0?0:.4F;
        if(!renderer.drawMissileLauncher({},0,WHITE,1,pitch,yaw)) return false;
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
        auto* pixels=LoadImageColors(image);
        for(int i=0;i<image.width*image.height;++i) if(pixels[i].r>100 && pixels[i].r>pixels[i].b*1.3) ++counts[pose];
        if(const char* output=std::getenv("IAN_MISSILE_CAPTURE")) {
            std::filesystem::create_directories(output);
            const auto path=std::filesystem::path(output)/(pose==0?"launcher-neutral.png":"launcher-aimed.png");
            ExportImage(image,path.string().c_str());
        }
        UnloadImageColors(pixels);UnloadImage(image);
        // Verify render pose and authored muzzle pose agree for every tube.
        for(std::size_t tube=0;tube<4;++tube) {
            const auto neutral=missileLauncherMuzzleWorldPosition({},0,0,tube);
            const auto world=missileLauncherMuzzleWorldPosition({},yaw,pitch,tube);
            const auto posed=Vector3Transform({static_cast<float>(neutral.x),static_cast<float>(neutral.y),static_cast<float>(neutral.z)},
                missile_launcher_model::meshTransform(1,{},0,yaw,pitch));
            if(std::abs(posed.x-world.x)>1e-5 || std::abs(posed.y-world.y)>1e-5 || std::abs(posed.z-world.z)>1e-5) return false;
        }
        BuildingInstance building{.type=BuildingType::MissileLauncher,.gridPosition={0,0}};
        const Ray ray{camera.position,Vector3Normalize(Vector3Subtract(camera.target,camera.position))};
        if(!renderer.buildingRaycastDistance(building,{&building,1},ray,10,yaw,pitch,0,-1,0)) return false;
    }
    int colored=0, changed=0;
    Image rocketImages[2]{};
    for(int frame=0;frame<2;++frame) {
        renderer.beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting lighting{};lighting.cameraPosition=camera.position;renderer.beginWorldShader(lighting);
        WorldMaterialState material{};material.unlitAmount=1;material.inkOutlineEligible=false;renderer.setWorldMaterial(material);
        AttackProjectile rocket;rocket.type=BuildingType::MissileLauncher;rocket.active=true;
        rocket.position=rocket.previousPosition={0,.8,0};rocket.runtime.velocity={0,5,-10};
        rocket.configuration.stats.size=1;rocket.runtime.elapsedLifetime=1.12+frame*.04;
        rocket.runtime.trailHead=7;rocket.runtime.trailCount=8;
        for(std::size_t i=0;i<8;++i) rocket.runtime.trailPositions[i]={.05*static_cast<double>(i),.3+.07*static_cast<double>(i),.7-.1*static_cast<double>(i)};
        app_projectile_drawing::beginProjectileBatch(false);
        app_projectile_drawing::emitDefenseProjectileGeometry(rocket,camera.position,1,false);
        app_projectile_drawing::endProjectileBatch();
        app_projectile_drawing::beginProjectileBatch(true);
        app_projectile_drawing::emitDefenseProjectileGeometry(rocket,camera.position,1,true);
        app_projectile_drawing::endProjectileBatch();
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        rocketImages[frame]=LoadImageFromTexture(target.texture);ImageFlipVertical(&rocketImages[frame]);
        if(frame==0) if(const char* output=std::getenv("IAN_MISSILE_CAPTURE")) {
            const auto path=std::filesystem::path(output)/"rocket-flight.png";ExportImage(rocketImages[frame],path.string().c_str());
        }
    }
    auto* first=LoadImageColors(rocketImages[0]);auto* second=LoadImageColors(rocketImages[1]);
    for(int i=0;i<800*600;++i) {
        if(first[i].r>140 && first[i].r>first[i].b*1.3) ++colored;
        if(std::abs(int(first[i].r)-int(second[i].r))>8) ++changed;
    }
    UnloadImageColors(first);UnloadImageColors(second);
    for(auto image:rocketImages) UnloadImage(image);
    UnloadRenderTexture(target);ui.shutdown();resources.shutdown();
    return counts[0]>10000 && counts[1]>10000 && colored>100 && changed>10;
}
