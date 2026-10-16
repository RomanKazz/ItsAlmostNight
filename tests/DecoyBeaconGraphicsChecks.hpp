#pragma once
#include "app/DecoyBeaconDrawing.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/Renderer.hpp"
#include "ui/GameUi.hpp"
#include <cstdlib>
#include <filesystem>

inline bool checkDecoyBeaconGraphics(ian::Renderer& renderer) {
    using namespace ian;
    GraphicsResources resources;resources.initialize({});
    GameUi ui;ui.initialize();
    auto& model=resources.decoyBeaconModel();
    if(!model.valid() || model.get().meshCount!=2 || model.collisionAsset().colliders.size()!=1 ||
        !IsTextureValid(ui.buildingTexture(BuildingType::DecoyBeacon)))return false;
    const auto canvas=LoadRenderTexture(800,600);
    const Camera3D camera{{4,3.2F,-5},{0,.6F,0},{0,1,0},7.5F,CAMERA_ORTHOGRAPHIC};
    auto config=WorldConfig::defaults();config.terrainResolution=33;
    config.terrainAmplitude=config.terrainSurfaceNoiseAmplitude=0;config.pondMinimumCount=config.pondMaximumCount=0;
    TerrainHeightfield terrain(config);
    const std::array buildings{BuildingInstance{.id={1001,1},.type=BuildingType::DecoyBeacon,.gridPosition={0,0}}};
    std::array beacons{DecoyBeaconRuntime{{1001,1},{},1,7.8,.2}};
    SimulationSnapshot snapshot;snapshot.decoyBeacons=beacons;snapshot.buildings=buildings;
    std::array<std::uint64_t,3> hashes{};
    int orange=0,yellow=0;
    for(int frame=0;frame<3;++frame) {
        beacons[0].pulseAge=.2+frame*.35;beacons[0].pulseRemaining=8-beacons[0].pulseAge;
        renderer.beginFrame();BeginTextureMode(canvas);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting light{};light.cameraPosition=camera.position;renderer.beginWorldShader(light);
        if(!renderer.drawDecoyBeacon({},.35F,WHITE,1,static_cast<float>(decoyBeaconLightStrength(beacons[0]))))return false;
        renderer.endWorldShader();renderer.beginColorOnlyWorldEffects();
        decoy_beacon_drawing::draw(snapshot,camera,false,terrain);
        renderer.endColorOnlyWorldEffects();EndMode3D();EndTextureMode();
        auto image=LoadImageFromTexture(canvas.texture);ImageFlipVertical(&image);
        auto* pixels=LoadImageColors(image);std::uint64_t hash=1469598103934665603ULL;
        for(int index=0;index<800*600;++index) {
            const auto c=pixels[index];orange+=c.r>140 && c.r>c.g*1.2 && c.g>50;
            yellow+=c.r>140 && c.g>120 && c.b<c.g*.7;
            hash=(hash^c.r)*1099511628211ULL;hash=(hash^c.g)*1099511628211ULL;
        }
        hashes[static_cast<std::size_t>(frame)]=hash;UnloadImageColors(pixels);
        if(const char* directory=std::getenv("IAN_DECOY_CAPTURE")) {
            std::filesystem::create_directories(directory);
            const auto path=std::filesystem::path(directory)/("decoy-beacon-"+std::to_string(frame)+".png");ExportImage(image,path.string().c_str());
        }
        UnloadImage(image);
    }
    const auto& beacon=buildings[0];
    const Ray ray{camera.position,Vector3Normalize(Vector3Subtract(camera.target,camera.position))};
    const bool aimed=renderer.buildingRaycastDistance(beacon,buildings,ray,10).has_value();
    UnloadRenderTexture(canvas);ui.shutdown();resources.shutdown();
    return aimed && orange>1000 && yellow>1000 && hashes[0]!=hashes[1] && hashes[1]!=hashes[2];
}
