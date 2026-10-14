#pragma once
#include "TestHarness.hpp"
#include "graphics/Renderer.hpp"
#include <filesystem>

inline void checkChestItemModels(ian::Renderer& renderer) {
    auto canvas = LoadRenderTexture(224,224);
    const Camera3D camera{{0,0.55F,1.6F},{0,0,0},{0,1,0},35,CAMERA_PERSPECTIVE};
    for(std::size_t index=19; index<ian::LootUpgradeEffectCount; ++index) {
        BeginTextureMode(canvas);ClearBackground({18,27,32,255});
        BeginMode3D(camera);
        renderer.drawLootItem({},static_cast<ian::LootUpgradeEffect>(index),ian::LootRarity::Rare,.3F);
        EndMode3D();EndTextureMode();
        auto image=LoadImageFromTexture(canvas.texture);ImageFlipVertical(&image);
        const auto* pixels=static_cast<const Color*>(image.data);
        int changed=0;
        for(int pixel=0;pixel<image.width*image.height;++pixel)
            if(pixels[pixel].r!=18 || pixels[pixel].g!=27 || pixels[pixel].b!=32) ++changed;
        require(changed>300,"every new chest item has visible solid world geometry");
        if(const char* path=std::getenv("IAN_CAPTURE_CHEST_MODELS")) {
            std::filesystem::create_directories(path);
            ExportImage(image,(std::filesystem::path(path)/(std::to_string(index)+".png")).c_str());
        }
        UnloadImage(image);
    }
    UnloadRenderTexture(canvas);
}
