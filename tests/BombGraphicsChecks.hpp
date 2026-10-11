#pragma once

#include "graphics/BombDrawing.hpp"
#include "graphics/Renderer.hpp"

#include <filesystem>
#include <iostream>
#include <cstdlib>

inline bool checkCartoonBombs(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(800, 450);
    const Camera3D camera{{0, .65F, 2.4F}, {0, .16F, 0}, {0, 1, 0},
        1.1F, CAMERA_ORTHOGRAPHIC};
    constexpr Color Background{21, 34, 46, 255};
    const auto render = [&](float time, float urgency, bool night,
                            bool cover, bool draw, bool particles = true) {
        BeginTextureMode(target);
        ClearBackground(Background);
        BeginMode3D(camera);
        ian::WorldLighting light{};
        light.cameraPosition = camera.position;
        if (night) {
            light.sunIntensity = .04F;
            light.ambientIntensity = .18F;
            light.dayNightTint = {.48F, .59F, .8F};
        }
        renderer.beginWorldShader(light);
        if (cover) DrawCube({0, .2F, 1}, 2, 2, .1F, Background);
        if (draw) {
            ian::WorldMaterialState material{};
            material.unlitAmount = .30F;
            material.inkOccluder = true;
            renderer.setWorldMaterial(material);
            const auto tip = ian::bomb_drawing::drawBody(time, urgency);
            material.unlitAmount = 1;
            material.screenAoAmount = 0;
            material.inkOutlineEligible = false;
            renderer.setWorldMaterial(material);
            renderer.beginColorOnlyWorldEffects();
            ian::bomb_drawing::drawFuse(tip, camera.position, camera.up,
                                      time, urgency, particles);
            renderer.endColorOnlyWorldEffects();
        }
        renderer.setWorldMaterial({});
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    const auto difference = [](Image a, Image b) {
        int changed = 0;
        Color* first = LoadImageColors(a);
        Color* second = LoadImageColors(b);
        for (int i = 0; i < a.width * a.height; ++i) {
            if (std::abs(int(first[i].r) - int(second[i].r)) +
                std::abs(int(first[i].g) - int(second[i].g)) +
                std::abs(int(first[i].b) - int(second[i].b)) > 8) ++changed;
        }
        UnloadImageColors(first);
        UnloadImageColors(second);
        return changed;
    };
    Image day = render(.2F, 0, false, false, true);
    Image moving = render(.47F, 0, false, false, true);
    Image night = render(.2F, 0, true, false, true);
    Image hot = render(.2F, .9F, false, false, true);
    Image cheap = render(.2F, 0, false, false, true, false);
    Image covered = render(.2F, 0, false, true, true);
    Image base = render(.2F, 0, false, true, false);
    int blue = 0, gold = 0, gloss = 0;
    Color* pixels = LoadImageColors(night);
    for (int i = 0; i < night.width * night.height; ++i) {
        const Color p = pixels[i];
        if (p.b > p.r + 15 && p.g > p.r + 8) ++blue;
        if (p.r > 160 && p.g > 100 && p.b < 100) ++gold;
        if (p.r > 110 && p.g > 150 && p.b > 150) ++gloss;
    }
    UnloadImageColors(pixels);
    bool valid = blue > 1000 && gold > 100 && gloss > 100 &&
        difference(day, moving) > 50 && difference(day, hot) > 500 &&
        difference(day, cheap) > 20 && difference(covered, base) == 0;
    if (!valid) std::cerr << "bomb presentation blue/gold/gloss " << blue << '/' << gold
        << '/' << gloss << " motion " << difference(day, moving)
        << " urgency " << difference(day, hot) << " cover " << difference(covered, base) << '\n';
    if (const char* directory = std::getenv("IAN_CAPTURE_BOMBS")) {
        std::filesystem::create_directories(directory);
        for (const auto& pair : {std::pair{day, "bomb-day.png"},
            std::pair{moving, "bomb-next.png"}, std::pair{night, "bomb-night.png"},
            std::pair{hot, "bomb-fuse-ending.png"}})
            ExportImage(pair.first, (std::filesystem::path(directory) / pair.second).c_str());
        const auto visual = ian::FirstPersonToolVisual::Bomb;
        const auto tuning = ian::defaultFirstPersonToolTuning(visual);
        if (renderer.beginFirstPersonToolPass()) renderer.endFirstPersonToolPass(tuning);
        BeginTextureMode(target);
        ClearBackground(Background);
        BeginMode3D(ian::firstPersonToolCamera(visual));
        valid = renderer.drawFirstPersonTool(visual, 0, 0, 0, tuning) && valid;
        EndMode3D(); EndTextureMode();
        Image held = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&held);
        ExportImage(held, (std::filesystem::path(directory) / "bomb-held.png").c_str());
        UnloadImage(held);
    }
    for (Image image : {day, moving, night, hot, cheap, covered, base}) UnloadImage(image);
    UnloadRenderTexture(target);
    return valid;
}
