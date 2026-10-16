#pragma once

#include "app/RepairStationDrawing.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/Renderer.hpp"
#include "ui/GameUi.hpp"
#include <cstdlib>
#include <filesystem>

inline bool checkRepairStationGraphics(ian::Renderer& renderer) {
    using namespace ian;
    GraphicsResources resources; resources.initialize({});
    GameUi ui; ui.initialize();
    auto& model = resources.repairStationModel();
    if (!model.valid() || model.get().meshCount != 2 ||
        model.collisionAsset().colliders.size() != 1 ||
        !IsTextureValid(ui.buildingTexture(BuildingType::RepairStation))) return false;
    const auto canvas = LoadRenderTexture(800,600);
    const Camera3D camera{{3,2.5F,-4},{0,.8F,0},{0,1,0},3.8F,CAMERA_ORTHOGRAPHIC};
    const std::array links{RepairStationLink{{1001,1},{1002,1},{0,1.205,0},{1.4,.7,0}}};
    auto config = WorldConfig::defaults();
    config.terrainResolution=33;config.terrainAmplitude=config.terrainSurfaceNoiseAmplitude=0;
    config.pondMinimumCount=config.pondMaximumCount=0;
    TerrainHeightfield terrain(config);
    const std::array stations{BuildingInstance{.id={1001,1},.type=BuildingType::RepairStation,.gridPosition={0,0}}};
    SimulationSnapshot snapshot; snapshot.repairStationLinks=links;snapshot.buildings=stations;
    std::array<std::uint64_t,2> signatures{};
    int orange=0,cyan=0;
    for (int frame=0;frame<2;++frame) {
        renderer.beginFrame();BeginTextureMode(canvas);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting light{};light.cameraPosition=camera.position;renderer.beginWorldShader(light);
        if (!renderer.drawRepairStation({},0)) return false;
        renderer.endWorldShader();
        snapshot.elapsedSeconds=.27+frame*.25;
        snapshot.repairStationPulseAge=snapshot.elapsedSeconds;
        repair_station_drawing::draw(snapshot,camera,false,terrain);
        EndMode3D();EndTextureMode();
        auto image=LoadImageFromTexture(canvas.texture);ImageFlipVertical(&image);
        auto* pixels=LoadImageColors(image);
        std::uint64_t hash=1469598103934665603ULL;
        for(int index=0;index<800*600;++index) {
            const auto pixel=pixels[index];
            orange+=pixel.r>120 && pixel.r>pixel.b*1.5;
            cyan+=pixel.g>100 && pixel.b>100 && pixel.r<pixel.g*.8;
            hash=(hash^pixel.r)*1099511628211ULL;
            hash=(hash^pixel.g)*1099511628211ULL;
        }
        signatures[static_cast<std::size_t>(frame)]=hash;
        UnloadImageColors(pixels);
        if(const char* directory=std::getenv("IAN_REPAIR_STATION_CAPTURE")) {
            std::filesystem::create_directories(directory);
            const auto path=std::filesystem::path(directory)/("repair-station-"+std::to_string(frame)+".png");
            ExportImage(image,path.string().c_str());
        }
        UnloadImage(image);
    }
    bool levelsValid = &resources.repairStationModel(0)==&resources.repairStationModel(1) &&
        &resources.repairStationModel(99)==&resources.repairStationModel(8);
    std::uint64_t previousHash=0;
    for(int level=1;level<=8;++level) {
        auto& resource=resources.repairStationModel(level);
        levelsValid=levelsValid && resource.valid() && resource.get().meshCount==2 &&
            resource.collisionAsset().colliders.size()==1 && resource.collisionAsset().sockets.empty() &&
            std::abs(renderer.buildingVisualHeight(BuildingType::RepairStation,level)-(resource.visualBounds().max.y+.005F))<1e-4F;
        renderer.beginFrame();BeginTextureMode(canvas);ClearBackground({29,47,57,255});BeginMode3D(camera);
        WorldLighting lighting{};lighting.cameraPosition=camera.position;renderer.beginWorldShader(lighting);
        levelsValid=renderer.drawRepairStation({},0,WHITE,1,level) && levelsValid;
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        auto image=LoadImageFromTexture(canvas.texture);ImageFlipVertical(&image);
        auto* pixels=LoadImageColors(image);std::uint64_t hash=1469598103934665603ULL;
        for(int i=0;i<image.width*image.height;++i) {
            hash=(hash^pixels[i].r)*1099511628211ULL;
            hash=(hash^pixels[i].g)*1099511628211ULL;
        }
        levelsValid=levelsValid && (level==1 || hash!=previousHash);previousHash=hash;
        if(const char* directory=std::getenv("IAN_REPAIR_STATION_CAPTURE")) {
            std::filesystem::create_directories(directory);
            const auto path=std::filesystem::path(directory)/("repair-level-"+std::to_string(level)+".png");
            ExportImage(image,path.string().c_str());
        }
        UnloadImageColors(pixels);UnloadImage(image);
        BuildingInstance building{.type=BuildingType::RepairStation,.gridPosition={0,0},.level=static_cast<std::uint8_t>(level)};
        const Ray aimedRay{camera.position,Vector3Normalize(Vector3Subtract(camera.target,camera.position))};
        levelsValid=levelsValid && renderer.buildingRaycastDistance(building,{&building,1},aimedRay,10).has_value();
        const Ray expandedColliderRay{{.645F,.250F,-3},{0,0,1}};
        levelsValid=levelsValid && (renderer.buildingRaycastDistance(building,{&building,1},expandedColliderRay,10).has_value()==(level>=2));
    }
    const BuildingInstance station{.type=BuildingType::RepairStation,.gridPosition={0,0}};
    const Ray ray{camera.position,Vector3Normalize(Vector3Subtract(camera.target,camera.position))};
    const bool aim=renderer.buildingRaycastDistance(station,{&station,1},ray,10).has_value();
    UnloadRenderTexture(canvas);ui.shutdown();resources.shutdown();
    return levelsValid && aim && orange>10000 && cyan>1000 && signatures[0]!=signatures[1];
}
