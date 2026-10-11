#pragma once

#include "graphics/IceWandEffect.hpp"
#include "graphics/Renderer.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

inline bool checkCartoonIceProjectile(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 256);
    const Camera3D camera{{0, 1, 8}, {0, 1, 0}, {0, 1, 0}, 3.2F, CAMERA_ORTHOGRAPHIC};
    ian::IceWandProjectile projectile{};
    projectile.active = true;
    projectile.element = ian::WandElement::Ice;
    projectile.radius = .3;
    projectile.id = {7101, 1};
    projectile.previousPosition = {-.4, 1, 0};
    projectile.position = {0, 1, 0};
    projectile.trailCount = projectile.trail.size();
    for (std::size_t i = 0; i < projectile.trail.size(); ++i)
        projectile.trail[i] = {-float(i) * .2F, 1, 0};
    const auto render = [&](int mode, float time, bool night = false) {
        BeginTextureMode(target);
        ClearBackground({18, 33, 54, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        if (night) {
            lighting.sunIntensity = .03F;
            lighting.ambientIntensity = .18F;
            lighting.dayNightTint = {.48F, .59F, .8F};
        }
        renderer.beginWorldShader(lighting);
        if (mode == 3 || mode == 4) DrawCube({0, 1, 2}, 8, 6, .1F, GREEN);
        if (mode != 0 && mode != 3) {
            auto shot = projectile;
            if (mode == 6) shot.trailCount = 0;
            if (mode == 7) shot.active = false;
            if (mode == 2) {
                ian::WorldMaterialState material{};
                material.unlitAmount = 1;
                material.screenAoAmount = 0;
                material.inkOutlineEligible = false;
                material.inkOccluder = true;
                renderer.setWorldMaterial(material);
                ian::fire_wand_effect::beginBatch(true);
                ian::ice_wand_effect::emitProjectile(shot, camera.position, time, .5F);
                ian::fire_wand_effect::endBatch();
                renderer.setWorldMaterial({});
            } else renderer.drawIceWandProjectile(shot, camera.position, time, .5F);
        }
        if (mode == 5) DrawCube({-.2F, 1, -.5F}, .2F, .2F, .1F, MAGENTA);
        renderer.endWorldShader(); EndMode3D(); EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    const auto difference = [](Image a, Image b) {
        int count = 0;
        Color* first = LoadImageColors(a);
        Color* second = LoadImageColors(b);
        for (int i = 0; i < a.width * a.height; ++i)
            if (std::abs(int(first[i].r) - int(second[i].r)) +
                std::abs(int(first[i].g) - int(second[i].g)) +
                std::abs(int(first[i].b) - int(second[i].b)) > 6) ++count;
        UnloadImageColors(first); UnloadImageColors(second);
        return count;
    };
    Image empty = render(0, .1F), ice = render(1, .1F), batch = render(2, .1F),
        next = render(1, .39F), night = render(1, .1F, true),
        cover = render(3, .1F), hidden = render(4, .1F), rear = render(5, .1F),
        noTrail = render(6, .1F), inactive = render(7, .1F);
    int blue = 0, cyan = 0, white = 0;
    Color* pixels = LoadImageColors(ice);
    for (int i = 0; i < ice.width * ice.height; ++i) {
        const Color p = pixels[i];
        if (p.b > 140 && p.b > p.g + 20 && p.g > p.r + 20) ++blue;
        if (p.g > 180 && p.b > 200 && p.r < 170) ++cyan;
        if (p.r > 220 && p.g > 235 && p.b > 235) ++white;
    }
    UnloadImageColors(pixels);
    const bool valid = blue > 80 && cyan > 80 && white > 20 &&
        difference(ice, batch) == 0 && difference(ice, next) > 20 &&
        difference(ice, night) == 0 && difference(cover, hidden) == 0 &&
        difference(ice, rear) == 0 && difference(ice, noTrail) == 0 &&
        difference(empty, inactive) == 0;
    if (!valid) std::cerr << "ice projectile blue/cyan/white " << blue << '/' << cyan << '/' << white
        << " batch " << difference(ice, batch) << " motion " << difference(ice, next)
        << " cover " << difference(cover, hidden) << " rear " << difference(ice, rear) << '\n';
    if (const char* directory = std::getenv("IAN_CAPTURE_ICE_PROJECTILE")) {
        std::filesystem::create_directories(directory);
        for (const auto& pair : {std::pair{ice, "ice-core.png"},
            std::pair{next, "ice-next.png"}, std::pair{night, "ice-night.png"}})
            ExportImage(pair.first, (std::filesystem::path(directory) / pair.second).c_str());
    }
    for (Image image : {empty, ice, batch, next, night, cover, hidden, rear, noTrail, inactive})
        UnloadImage(image);
    UnloadRenderTexture(target);
    return valid;
}
