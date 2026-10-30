#pragma once
#include "graphics/Renderer.hpp"
#include "graphics/WorldTransforms.hpp"
#include "world/TerrainHeightfield.hpp"
#include <filesystem>
#include <cstdlib>
#include <iostream>

inline bool checkShrineRendering(ian::Renderer& renderer,const ian::TerrainHeightfield& terrain) {
    const auto target=LoadRenderTexture(512,384);
    const Camera3D camera{{0,2.3F,8},{0,1.3F,0},{0,1,0},3.8F,CAMERA_ORTHOGRAPHIC};
    const auto render=[&](Color tint) {
        BeginTextureMode(target);ClearBackground({24,32,40,255});BeginMode3D(camera);
        ian::WorldLighting light{};light.cameraPosition=camera.position;renderer.beginWorldShader(light);
        ian::WorldMaterialState material{};material.unlitAmount=1;material.screenAoAmount=0;renderer.setWorldMaterial(material);
        bool loaded=true;
        for(std::size_t kind=2;kind<4;++kind) {
            const Vector3 position{kind==2 ? -1.25F : 1.25F,0,0};
            loaded=renderer.drawWorldLandmark(kind,position,0,tint) && loaded;
        }
        renderer.endWorldShader();EndMode3D();EndTextureMode();
        Image image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
        return std::pair{image,loaded};
    };
    auto [active,loaded]=render(WHITE);auto [spent,spentLoaded]=render({115,115,125,255});
    int purple=0,blue=0,gold=0,red=0;long activeBrightness=0,spentBrightness=0;
    for(int y=0;y<active.height;++y) for(int x=0;x<active.width;++x) {
        const Color c=GetImageColor(active,x,y),d=GetImageColor(spent,x,y);
        if(c.r>c.g+20 && c.b>c.g+20) ++purple;
        if(c.b>c.r+20 && c.b>c.g+15) ++blue;
        if(c.r>170 && c.g>110 && c.b<110) ++gold;
        if(c.r>c.g+60 && c.r>c.b+60) ++red;
        activeBrightness+=c.r+c.g+c.b;spentBrightness+=d.r+d.g+d.b;
    }
    bool valid=loaded && spentLoaded && purple>300 && blue>300 && gold>300 && red>300 && spentBrightness<activeBrightness;
    for(std::size_t kind=2;kind<4;++kind) {
        const auto bounds=renderer.worldLandmarkWorldBounds(kind,{0,0,0},0);
        valid=valid && std::abs(bounds.min.y)<.001F && std::abs(bounds.max.y-2.8F)<.001F;
        const auto hit=renderer.worldLandmarkRaycastDistance(kind,{0,0,0},0,Ray{{0,1,8},{0,0,-1}},10);
        valid=valid && hit.has_value() && *hit>6 && *hit<9;
    }
    // The same tilted pose must be used by rendering, the outline bounds and
    // precise picking; a world-up offset is wrong on a slope.
    std::array<BoundingBox,2> flatBounds{};
    std::array<std::optional<double>,2> flatHits{};
    for(std::size_t kind=2;kind<4;++kind) {
        flatBounds[kind-2]=renderer.worldLandmarkWorldBounds(kind,{0,0,0},0);
        flatHits[kind-2]=renderer.worldLandmarkRaycastDistance(kind,{0,0,0},0,Ray{{0,1,8},{0,0,-1}},10);
    }
    renderer.rebuildTerrain(terrain);
    Vector3 point{},normal{0,1,0};
    for(int z=-25;z<=25;++z) for(int x=-25;x<=25;++x) {
        const auto n=terrain.getNormal(x,z);
        if(n.y<normal.y) {
            normal={static_cast<float>(n.x),static_cast<float>(n.y),static_cast<float>(n.z)};
            point={static_cast<float>(x),static_cast<float>(terrain.getHeight(x,z)),static_cast<float>(z)};
        }
    }
    valid=valid && normal.y<.98F;
    if(normal.y>=.98F) std::cerr<<"shrine slope fixture too flat: "<<normal.y<<'\n';
    constexpr float yaw=.63F;
    const auto rotation=ian::world_transforms::surfaceRotation(normal,yaw);
    const auto transform=MatrixMultiply(rotation,MatrixTranslate(point.x,point.y,point.z));
    const Ray slopeRay{Vector3Transform({0,1,8},transform),Vector3Transform({0,0,-1},rotation)};
    for(std::size_t kind=2;kind<4;++kind) {
        const auto expected=ian::world_transforms::transformBounds(flatBounds[kind-2],transform);
        const auto actual=renderer.worldLandmarkWorldBounds(kind,point,yaw);
        valid=valid && Vector3Distance(expected.min,actual.min)<.002F &&
            Vector3Distance(expected.max,actual.max)<.002F;
        const auto hit=renderer.worldLandmarkRaycastDistance(kind,point,yaw,slopeRay,10);
        const bool poseValid=Vector3Distance(expected.min,actual.min)<.002F &&
            Vector3Distance(expected.max,actual.max)<.002F && hit && flatHits[kind-2] &&
            std::abs(*hit-*flatHits[kind-2])<.003;
        if(!poseValid) std::cerr<<"shrine slope pose "<<kind<<" bounds errors "
            <<Vector3Distance(expected.min,actual.min)<<'/'<<Vector3Distance(expected.max,actual.max)
            <<" hits "<<hit.value_or(-1)<<'/'<<flatHits[kind-2].value_or(-1)<<'\n';
        valid=valid && poseValid;
    }
    if(const char* directory=std::getenv("IAN_CAPTURE_SHRINES")) {
        std::filesystem::create_directories(directory);
        ExportImage(active,(std::filesystem::path(directory)/"shrines-active.png").string().c_str());
        ExportImage(spent,(std::filesystem::path(directory)/"shrines-spent.png").string().c_str());
    }
    if(!valid) std::cerr<<"shrine load/render/bounds/raycast failed; colors "<<purple<<'/'<<blue<<'/'<<gold<<'/'<<red<<'\n';
    UnloadImage(active);UnloadImage(spent);UnloadRenderTexture(target);
    return valid;
}
