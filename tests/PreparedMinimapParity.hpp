#pragma once
#include "ui/GameUi.hpp"
#include "ui/HudRenderer.hpp"
#include "game/Simulation.hpp"
#include <memory>
#include <rlgl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace ian::test {
inline bool preparedMinimapPixelParity() {
    GameUi ui; ui.initialize();
    auto simulation = std::make_unique<Simulation>();
    simulation->startRunFromSeed(PlayerClass::Engineer, 12345);
    auto snapshot = simulation->snapshot();
    bool valid = true;
    for (const float expansion : {0.0F, .3F, 1.0F}) {
        snapshot.playerYaw = .73;
        std::array<Image, 2> images{};
        for (int prepared = 0; prepared < 2; ++prepared) {
            // Warm text caches equally; each trial still refreshes the map.
            for (int warm = 0; warm < 4; ++warm) {
                ui.markMinimapTargetRefreshed(-1000);
                if (prepared) drawMinimapHud(ui, snapshot, expansion, MinimapHudPass::Prepare);
                BeginDrawing(); ClearBackground({31, 57, 74, 255});
                drawMinimapHud(ui, snapshot, expansion,
                    prepared ? MinimapHudPass::Composite : MinimapHudPass::Complete);
                rlDrawRenderBatchActive();
                if (warm == 3) images[static_cast<std::size_t>(prepared)] = LoadImageFromScreen();
                EndDrawing();
            }
        }
        Color* a = LoadImageColors(images[0]); Color* b = LoadImageColors(images[1]);
        int changed = 0;
        for (int i = 0; i < images[0].width * images[0].height; ++i)
            if (std::max({std::abs(int(a[i].r)-int(b[i].r)), std::abs(int(a[i].g)-int(b[i].g)),
                std::abs(int(a[i].b)-int(b[i].b))}) > 1) ++changed;
        if (changed) std::cerr << "Prepared minimap pixel mismatch: " << expansion << " = " << changed << '\n';
        valid = valid && changed == 0;
        UnloadImageColors(a); UnloadImageColors(b); for (auto image : images) UnloadImage(image);
    }
    ui.shutdown();
    return valid;
}
} // namespace ian::test
