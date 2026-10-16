#pragma once

#include "graphics/Renderer.hpp"
#include "graphics/WandPresentation.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

inline bool checkWandPresentation(ian::Renderer& renderer) {
    using namespace ian::wand_presentation;
    // Measure the held silhouette, not wall-clock charge motes which may
    // briefly touch an edge depending on how long earlier GPU tests took.
    const bool particles = renderer.settings().particles;
    renderer.settings().particles = false;
    bool valid = chargeProgress(0, .4) == 0 && chargeProgress(.4, .4) == 0 &&
        std::abs(chargeProgress(.1, .4) - .75F) < .00001F;
    for (const bool fire : {false, true}) {
        const auto ready = pose(fire, 1, 0);
        const auto release = pose(fire, 0, 1);
        const auto rest = pose(fire, 0, .00001F);
        valid &= Vector3Distance(ready.offset, release.offset) < .00001F &&
            Vector3Distance(ready.rotation, release.rotation) < .00001F &&
            Vector3Length(rest.offset) < .00001F && Vector3Length(rest.rotation) < .0001F;
        // Sweep both sides of each keyframe: a fast cast must not snap.
        auto previous = pose(fire, 0, 1);
        for (int i = 1; i <= 1000; ++i) {
            const auto current = pose(fire, 0, 1 - static_cast<float>(i) / 1000);
            valid &= Vector3Distance(previous.offset, current.offset) < .002F &&
                Vector3Distance(previous.rotation, current.rotation) < .6F;
            previous = current;
        }
    }
    const int oldWidth = GetScreenWidth(), oldHeight = GetScreenHeight();
    SetWindowSize(960, 600);
    const auto target = LoadRenderTexture(960, 600);
    constexpr Color background{27, 44, 67, 255};
    const char* directory = std::getenv("IAN_CAPTURE_WANDS");
    if (directory) std::filesystem::create_directories(directory);
    std::array<Image, 2> idle{};
    for (int element = 0; element < 2; ++element) {
        const auto visual = element == 0 ? ian::FirstPersonToolVisual::IceWand : ian::FirstPersonToolVisual::FireWand;
        const auto tuning = ian::defaultFirstPersonToolTuning(visual);
        if (renderer.beginFirstPersonToolPass()) renderer.endFirstPersonToolPass(tuning);
        else valid = false;
        for (int phase = 0; phase < 10; ++phase) {
            const float charge = phase == 1 ? .5F : phase == 2 ? 1.0F : 0.0F;
            const float recoil = phase >= 3 ? 1.0F - static_cast<float>(phase - 3) / 7 : 0;
            BeginTextureMode(target); ClearBackground(background);
            BeginMode3D(ian::firstPersonToolCamera(visual));
            valid &= renderer.drawFirstPersonTool(visual, 0, 0, 0, tuning, charge, recoil);
            EndMode3D(); EndTextureMode();
            Image image = LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
            Color* pixels = LoadImageColors(image);
            int ink = 0, border = 0, colored = 0, changed = 0;
            Color* original = phase > 0 ? LoadImageColors(idle[static_cast<std::size_t>(element)]) : nullptr;
            for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
                const auto p = pixels[y * image.width + x];
                const bool drawn = std::abs(int(p.r)-background.r) + std::abs(int(p.g)-background.g) +
                    std::abs(int(p.b)-background.b) > 25;
                ink += drawn;
                if (y < 3 || x < 3 || x > image.width - 4) border += drawn;
                colored += element == 0 ? p.b > 110 && p.b > int(p.r) + 35 : p.r > 150 && p.r > int(p.b) + 70;
                if (original) {
                    const auto q = original[y * image.width + x];
                    changed += std::abs(int(p.r)-q.r) + std::abs(int(p.g)-q.g) + std::abs(int(p.b)-q.b) > 35;
                }
            }
            const bool frame = ink > 1500 && border == 0 && colored > 120 && (phase == 0 || changed > 100);
            if (!frame) std::cerr << "wand frame " << element << '/' << phase << " ink/border/color/change "
                << ink << '/' << border << '/' << colored << '/' << changed << '\n';
            valid &= frame;
            if (directory) ExportImage(image, (std::filesystem::path(directory) /
                ((element == 0 ? "ice-" : "fire-") + std::to_string(phase) + ".png")).string().c_str());
            if (phase == 0) idle[static_cast<std::size_t>(element)] = ImageCopy(image);
            if (original) UnloadImageColors(original);
            UnloadImageColors(pixels); UnloadImage(image);
        }
    }
    for (const auto image : idle) UnloadImage(image);
    UnloadRenderTexture(target); SetWindowSize(oldWidth, oldHeight);
    renderer.settings().particles = particles;
    return valid;
}
