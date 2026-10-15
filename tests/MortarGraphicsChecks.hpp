#pragma once
#include "graphics/GraphicsResources.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/MortarModel.hpp"
#include "ui/GameUi.hpp"
#include <cstdlib>
#include <filesystem>

inline bool checkMortarGraphics(ian::Renderer& renderer) {
    using namespace ian;
    GraphicsResources resources;resources.initialize({});GameUi ui;ui.initialize();
    auto& resource=resources.mortarModel();
    if(!resource.valid() || resource.get().meshCount!=3 || resource.collisionAsset().sockets.size()!=1 ||
        ui.buildingTexture(BuildingType::Mortar).id==0)return false;
    auto& upgrades=resources.mortarUpgradeModel();
    if(!upgrades.valid() || upgrades.get().meshCount!=21)return false;
    for(int level=1;level<=8;++level) {
        int count=0;
        mortar_model::forEachUpgradePart(level,[&](int mesh,int role) {
            if(mesh<0 || mesh>=21 || role<0 || role>2)count=-100;
            else ++count;
        });
        if(count!=(level==1?0:3))return false;
    }
    for(float yaw:{0.0F,.7F,2.1F})for(float pitch:{float(MortarMinimumPitch),0.0F,float(MortarMaximumPitch)}) {
        const auto muzzle=mortarMuzzleWorldPosition({},yaw,pitch);
        const Vector3 authored{float(MortarAuthoredMuzzle.x),float(MortarAuthoredMuzzle.y),float(MortarAuthoredMuzzle.z)};
        const auto rendered=Vector3Transform(authored,mortar_model::meshTransform(1,{},0,yaw,pitch));
        if(std::abs(muzzle.x-rendered.x)>1e-5 || std::abs(muzzle.y-rendered.y)>1e-5 || std::abs(muzzle.z-rendered.z)>1e-5)return false;
    }
    const auto target=LoadRenderTexture(800,600);
    const Camera3D camera{{3,2.5F,-4},{0,.7F,0},{0,1,0},3.4F,CAMERA_ORTHOGRAPHIC};
    int colored=0,gold[8]{};std::array<std::uint64_t,8> signatures{};
    for(int frame=0;frame<10;++frame) {
        renderer.beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting light{};light.cameraPosition=camera.position;renderer.beginWorldShader(light);
        const int level=frame<8?frame+1:8;
        const float pitch=frame==8?float(MortarMinimumPitch):frame==9?float(MortarMaximumPitch):0;
        if(!renderer.drawMortar({},0,WHITE,1,pitch,.4F,level))return false;
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
        auto* pixels=LoadImageColors(image);
        std::uint64_t signature=1469598103934665603ULL;
        for(int i=0;i<800*600;++i) {
            const auto pixel=pixels[i];
            colored+=pixel.r>120 && pixel.r>pixel.b*1.5F;
            signature=(signature^pixel.r)*1099511628211ULL;
            signature=(signature^pixel.g)*1099511628211ULL;
            if(frame<8)gold[frame]+=pixel.r>100 && pixel.g>pixel.r*.65F && pixel.b<pixel.g*.55F;
        }
        if(frame<8)signatures[static_cast<std::size_t>(frame)]=signature;
        UnloadImageColors(pixels);
        if(const char* output=std::getenv("IAN_MORTAR_CAPTURE")) {
            std::filesystem::create_directories(output);
            const auto path=std::filesystem::path(output)/("mortar-"+std::to_string(frame)+".png");
            ExportImage(image,path.string().c_str());
        }
        UnloadImage(image);
    }
    const BuildingInstance building{.type=BuildingType::Mortar,.gridPosition={0,0}};
    const Ray ray{camera.position,Vector3Normalize(Vector3Subtract(camera.target,camera.position))};
    const bool aim=renderer.buildingRaycastDistance(building,{&building,1},ray,10,.4F,.2F).has_value();
    auto master=building;master.level=8;
    const Ray guardRay{{0,1.16F,3},{0,0,-1}};
    const auto guard=renderer.buildingRaycastDistance(master,{&master,1},guardRay,10,0,0);
    bool distinct=true;
    for(std::size_t a=0;a<8;++a)for(std::size_t b=a+1;b<8;++b)distinct&=signatures[a]!=signatures[b];
    UnloadRenderTexture(target);ui.shutdown();resources.shutdown();
    return colored>20000 && aim && distinct && gold[7]>gold[0]+1000 && guard && *guard<2.7;
}
