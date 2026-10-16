#pragma once
#include "app/WindBlowerDrawing.hpp"
#include "graphics/WindBlowerModel.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/Renderer.hpp"
#include "ui/GameUi.hpp"
#include <cstdlib>
#include <filesystem>

inline bool checkWindBlowerGraphics(ian::Renderer& renderer) {
    using namespace ian;
    GraphicsResources resources;resources.initialize({});GameUi ui;ui.initialize();
    auto& resource=resources.windBlowerModel();
    if(!resource.valid() || resource.get().meshCount!=3 || resource.collisionAsset().colliders.size()!=1 ||
        resource.collisionAsset().sockets.size()!=1 || !IsTextureValid(ui.buildingTexture(BuildingType::WindBlower)))return false;
    // The rotor spins about its real shaft, independently of the head and base.
    const Vector3 pivot{0,.960F,-.300F};
    for(float angle:{0.0F,.4F,1.8F}) {
        const auto transform=wind_blower_model::meshTransform(1,{},0,0,angle);
        if(Vector3Distance(Vector3Transform(pivot,transform),pivot)>.00001F)return false;
    }
    const auto canvas=LoadRenderTexture(800,600);
    const Camera3D camera{{5,3.5F,-7},{0,.6F,-1.8F},{0,1,0},8.5F,CAMERA_ORTHOGRAPHIC};
    const std::array buildings{BuildingInstance{.id={1001,1},.type=BuildingType::WindBlower,.gridPosition={0,0}}};
    std::array towers{TowerRuntime{.buildingId={1001,1},.type=BuildingType::WindBlower}};
    towers[0].attack.stats=resolveAttackStats(TowerSystem::baseStats(BuildingType::WindBlower,1));
    towers[0].windOrigin=windBlowerMuzzle({},0);
    SimulationSnapshot snapshot;snapshot.towers=towers;snapshot.buildings=buildings;
    std::array<std::uint64_t,3> hashes{},rotorHashes{};
    int orange=0,wind=0;
    for(int mode=0;mode<2;++mode)for(int frame=0;frame<3;++frame) {
        const float rotor=mode==0 ? float(frame)*.35F : 0;
        towers[0].windAge=.14+frame*.17;
        renderer.beginFrame();BeginTextureMode(canvas);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting light{};light.cameraPosition=camera.position;renderer.beginWorldShader(light);
        if(!renderer.drawWindBlower({},0,WHITE,1,0,rotor))return false;
        renderer.endWorldShader();
        if(mode==1) {
            renderer.beginColorOnlyWorldEffects();wind_blower_drawing::draw(snapshot,camera,false);renderer.endColorOnlyWorldEffects();
        }
        EndMode3D();EndTextureMode();
        auto image=LoadImageFromTexture(canvas.texture);ImageFlipVertical(&image);
        auto* pixels=LoadImageColors(image);std::uint64_t hash=1469598103934665603ULL;
        for(int index=0;index<800*600;++index) {
            const auto c=pixels[index];orange+=c.r>140 && c.r>c.g*1.2 && c.g>50;
            wind+=c.b>80 && c.g>80 && c.b>c.r*1.1 && c.g>c.r*1.1;
            hash=(hash^c.r)*1099511628211ULL;hash=(hash^c.g)*1099511628211ULL;
        }
        (mode==0?rotorHashes:hashes)[static_cast<std::size_t>(frame)]=hash;UnloadImageColors(pixels);
        if(mode==1)if(const char* directory=std::getenv("IAN_WIND_CAPTURE")) {
            std::filesystem::create_directories(directory);
            const auto path=std::filesystem::path(directory)/("wind-blower-"+std::to_string(frame)+".png");ExportImage(image,path.string().c_str());
        }
        UnloadImage(image);
    }
    const Ray ray{{0,.9F,-3},{0,0,1}};
    const bool aimed=renderer.buildingRaycastDistance(buildings[0],buildings,ray,10,0,0,1,-1,0,.4F).has_value();
    UnloadRenderTexture(canvas);ui.shutdown();resources.shutdown();
    return aimed && orange>1000 && wind>100 && hashes[0]!=hashes[1] && hashes[1]!=hashes[2] &&
        rotorHashes[0]!=rotorHashes[1] && rotorHashes[1]!=rotorHashes[2];
}
