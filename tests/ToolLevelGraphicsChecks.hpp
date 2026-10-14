#pragma once

#include "graphics/Renderer.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>

inline bool checkToolLevelRendering(ian::Renderer& renderer) {
    const int previousWidth = GetScreenWidth();
    const int previousHeight = GetScreenHeight();
    SetWindowSize(960, 600);
    const auto target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    if (!IsRenderTextureValid(target)) {
        SetWindowSize(previousWidth, previousHeight);
        return false;
    }
    constexpr Color Background{27, 44, 67, 255};
    bool valid = true;
    const char* directory = std::getenv("IAN_CAPTURE_TOOL_LEVELS");
    if (directory) std::filesystem::create_directories(directory);
    const auto capture = [&](ian::FirstPersonToolVisual visual, int level,
                             float progress, bool alternate) {
        const auto tuning = ian::defaultFirstPersonToolTuning(visual);
        if (renderer.beginFirstPersonToolPass()) {
            renderer.endFirstPersonToolPass(tuning);
        } else valid = false;
        BeginTextureMode(target);
        ClearBackground(Background);
        BeginMode3D(ian::firstPersonToolCamera(visual));
        valid = renderer.drawFirstPersonTool(visual, progress, 0, 0, tuning,
                                             0, 0, alternate, level) && valid;
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (directory) {
            ExportImage(image, (std::filesystem::path{directory} /
                (std::to_string(static_cast<int>(visual)) + "_L" + std::to_string(level) +
                 "_" + std::to_string(progress) + (alternate ? "_alternate" : "") + ".png")).string().c_str());
        }
        return image;
    };
    const auto difference = [](Image first, Image second) {
        Color* a = LoadImageColors(first);
        Color* b = LoadImageColors(second);
        int count = 0;
        for (int i = 0; i < first.width * first.height; ++i) {
            if (std::abs(int(a[i].r) - b[i].r) +
                std::abs(int(a[i].g) - b[i].g) +
                std::abs(int(a[i].b) - b[i].b) > 12) ++count;
        }
        UnloadImageColors(a); UnloadImageColors(b);
        return count;
    };
    for (const auto visual : {ian::FirstPersonToolVisual::Axe,
                             ian::FirstPersonToolVisual::Pickaxe}) {
        Image original = capture(visual, 1, 0, false);
        Image previous = ImageCopy(original);
        for (int level = 2; level <= 8; ++level) {
            Image current = capture(visual, level, 0, false);
            const int changed = difference(previous, current);
            if (changed <= 30) std::cerr << "tool " << int(visual)
                << " level " << level << " differs by only " << changed << " pixels\n";
            valid = valid && changed > 30;
            // Both animation clips must move the entire selected upgrade.
            for (const bool alternate : {false, true}) {
                Image swing = capture(visual, level, alternate ? .54F : .36F, alternate);
                valid = valid && difference(current, swing) > 100;
                UnloadImage(swing);
            }
            UnloadImage(previous); previous = current;
        }
        Image clamped = capture(visual, 99, 0, false);
        valid = valid && difference(previous, clamped) == 0;
        UnloadImage(clamped); UnloadImage(previous);
        Image restored = capture(visual, 1, 0, false);
        valid = valid && difference(original, restored) == 0;
        UnloadImage(original); UnloadImage(restored);
    }
    UnloadRenderTexture(target);
    SetWindowSize(previousWidth, previousHeight);
    return valid;
}
