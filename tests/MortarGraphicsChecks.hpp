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
    for(float yaw:{0.0F,.7F,2.1F})for(float pitch:{float(MortarMinimumPitch),0.0F,float(MortarMaximumPitch)}) {
        const auto muzzle=mortarMuzzleWorldPosition({},yaw,pitch);
        const Vector3 authored{float(MortarAuthoredMuzzle.x),float(MortarAuthoredMuzzle.y),float(MortarAuthoredMuzzle.z)};
        const auto rendered=Vector3Transform(authored,mortar_model::meshTransform(1,{},0,yaw,pitch));
        if(std::abs(muzzle.x-rendered.x)>1e-5 || std::abs(muzzle.y-rendered.y)>1e-5 || std::abs(muzzle.z-rendered.z)>1e-5)return false;
    }
    const auto target=LoadRenderTexture(800,600);
    const Camera3D camera{{3,2.5F,-4},{0,.7F,0},{0,1,0},3.4F,CAMERA_ORTHOGRAPHIC};
    int colored=0;
    for(int frame=0;frame<3;++frame) {
        renderer.beginFrame();BeginTextureMode(target);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting light{};light.cameraPosition=camera.position;renderer.beginWorldShader(light);
        const float pitch=frame==0?float(MortarMinimumPitch):frame==1?0:float(MortarMaximumPitch);
        if(!renderer.drawMortar({},0,WHITE,1,pitch,.4F))return false;
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
        auto* pixels=LoadImageColors(image);
        for(int i=0;i<800*600;++i)colored+=pixels[i].r>120 && pixels[i].r>pixels[i].b*1.5F;
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
    UnloadRenderTexture(target);ui.shutdown();resources.shutdown();
    return colored>20000 && aim;
}
