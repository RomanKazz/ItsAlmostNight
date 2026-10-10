#include "graphics/CrystalMineModel.hpp"
#include "graphics/QuarryModel.hpp"
#include "graphics/CoreCrystalModel.hpp"
#include "graphics/CannonModel.hpp"
#include "graphics/CrossbowModel.hpp"
#include "graphics/CatapultModel.hpp"
#include "graphics/GateModel.hpp"
#include "graphics/SpikeTrapModel.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/AtmosphereParticles.hpp"
#include "app/AppEffectDrawing.hpp"
#include "app/AppProjectileDrawing.hpp"
#include "app/AppBurnDrawing.hpp"
#include "app/AppFreezeDrawing.hpp"
#include "app/AppPoisonDrawing.hpp"
#include "graphics/FireWandEffect.hpp"
#include "graphics/ModularBuildingRenderer.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/EnemyPresentation.hpp"
#include "world/VisualBiomes.hpp"

#include <raylib.h>
#include <rlgl.h>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <vector>

namespace {

bool checkCartoonCoins(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(640, 360);
    const Camera3D camera{{0, .7F, 6}, {0, 0, 0}, {0, 1, 0},

bool checkEnemyPresentation(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(960, 600);
    const Camera3D camera{{0, 3.0F, 10}, {0, 1.7F, 0}, {0, 1, 0},
        5.0F, CAMERA_ORTHOGRAPHIC};
    const auto render = [&](bool night, bool batched, bool flash, bool cover = false,
                            ian::EnemyModelVisual model = ian::EnemyModelVisual::Minion) {
        renderer.beginFrame();
        BeginTextureMode(target);
        const Color background{18, 35, 57, 255};
        ClearBackground(background);
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        if (night) {
            lighting.sunIntensity = .04F;
            lighting.ambientIntensity = .18F;
            lighting.dayNightTint = {.48F, .59F, .8F};
        }
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.bakedAo = .82F;
        renderer.setWorldMaterial(material);
        if (cover) DrawCube({0, 1.7F, 3}, 12, 8, .1F, background);
        std::array<ian::EnemyDrawInstance, 3> enemies{};
        for (std::size_t index = 0; index < enemies.size(); ++index) {
            enemies[index] = {.modelVisual = model,
                .animationVisual = ian::EnemyAnimationVisual::Walk,
                .animationSeconds = .35F + float(index) * .17F,
                .position = {-2.4F + float(index)*2.4F, 0, 0}, .yawRadians = .2F,
                .whiteFlashAmount = flash ? .9F : 0};
        }
        if (batched) static_cast<void>(renderer.drawEnemiesInstanced(enemies));
        else for (const auto& enemy : enemies) {
            material.enemyWhiteFlashAmount = enemy.whiteFlashAmount;
            renderer.setWorldMaterial(material);
            static_cast<void>(renderer.drawEnemy(enemy.modelVisual, enemy.animationVisual,
                enemy.animationSeconds, enemy.position, enemy.yawRadians, WHITE, 1, true, true, true));
        }
        // Drawing enemies must not brighten unrelated geometry afterward.
        material.enemyWhiteFlashAmount = 0;
        renderer.setWorldMaterial(material);
        DrawCube({3.7F, .15F, 0}, .3F, .3F, .3F, {200, 150, 100, 255});
        renderer.endWorldShader(); EndMode3D(); EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    Image day = render(false, false, false), night = render(true, false, false),
          crowd = render(true, true, false), flash = render(true, true, true),
          covered = render(true, true, false, true);
    int pigmentPixels = 0, darkPigmentPixels = 0, brightFlashPixels = 0, difference = 0;
    int coveredPigmentPixels = 0;
    for (int y = 0; y < night.height; ++y) for (int x = 0; x < night.width; ++x) {
        const Color n = GetImageColor(night, x, y);
        const Color c = GetImageColor(crowd, x, y);
        const Color f = GetImageColor(flash, x, y);
        const Color hidden = GetImageColor(covered, x, y);
        if (hidden.r > hidden.g + 16 && hidden.b > hidden.g + 8) ++coveredPigmentPixels;
        if (n.r > n.g + 16 && n.b > n.g + 8) {
            ++pigmentPixels;
            if (std::max(n.r, n.b) < 55) ++darkPigmentPixels;
        }
        if (f.r > 200 && f.g > 180 && f.b > 150) ++brightFlashPixels;
        if (std::max({std::abs(int(n.r)-int(c.r)), std::abs(int(n.g)-int(c.g)),
                      std::abs(int(n.b)-int(c.b))}) > 8) ++difference;
    }
    bool valid = pigmentPixels > 3000 && darkPigmentPixels < pigmentPixels / 5 &&
        brightFlashPixels > 3000 && difference < 400 && coveredPigmentPixels == 0;
    if (!valid) std::cerr << "enemy night pigment/dark/flash/batch difference: "
        << pigmentPixels << '/' << darkPigmentPixels << '/' << brightFlashPixels
        << '/' << difference << '\n';
    if (const char* directory = std::getenv("IAN_CAPTURE_ENEMIES")) {
        std::filesystem::create_directories(directory);
        for (const auto& pair : {std::pair{day, "enemies-day.png"},
                std::pair{night, "enemies-night.png"}, std::pair{flash, "enemies-hit.png"},
                std::pair{covered, "enemies-covered.png"}})
            ExportImage(pair.first, (std::filesystem::path(directory) / pair.second).c_str());
    }
    for (int index = 0; index < 9; ++index) {
        const auto model = static_cast<ian::EnemyModelVisual>(index);
        Image archetype = render(true, true, false, false, model);
        int visiblePixels = 0;
        for (int y = 0; y < archetype.height; ++y) for (int x = 0; x < archetype.width; ++x) {
            const Color c = GetImageColor(archetype, x, y);
            if (std::max({c.r, c.g, c.b}) > 65) ++visiblePixels;
        }
        valid = valid && visiblePixels > 1000;
        if (const char* directory = std::getenv("IAN_CAPTURE_ENEMIES"))
            ExportImage(archetype, (std::filesystem::path(directory) /
                ("enemy-type-" + std::to_string(index) + ".png")).c_str());
        UnloadImage(archetype);
        for (const auto animation : {ian::EnemyAnimationVisual::Walk,
                ian::EnemyAnimationVisual::Run, ian::EnemyAnimationVisual::MeleeAttack}) {
            Matrix previous{};
            for (int frame = 0; frame <= 120; ++frame) {
                const Matrix transform = ian::enemyPresentationTransform(model,
                    animation, float(frame) / 120, {}, 0, 1);
                valid = valid && std::isfinite(MatrixDeterminant(transform)) &&
                    std::abs(MatrixDeterminant(transform) - 1) < .001F &&
                    transform.m13 >= 0 && transform.m13 < .05F;
                if (frame > 0) valid = valid && std::abs(transform.m5 - previous.m5) < .015F;
                previous = transform;
            }
        }
        const Matrix frozen = ian::enemyPresentationTransform(model,
            ian::EnemyAnimationVisual::Idle, 0, {1, 2, 3}, .2F, 1);
        valid = valid && frozen.m12 == 1 && frozen.m13 == 2 && frozen.m14 == 3;
    }
    for (Image image : {day, night, crowd, flash, covered}) UnloadImage(image);
    UnloadRenderTexture(target);
    return valid;
}

bool checkElitePresentation(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(960, 600);
    const Camera3D camera{{0, 3.5F, 10}, {0, 2.1F, 0}, {0, 1, 0}, 5.4F, CAMERA_ORTHOGRAPHIC};
    const Color background{18, 35, 57, 255};
    const auto render = [&](ian::EnemyModelVisual model, bool batched, bool cover,
                            bool elite = true, float time = .35F) {
        renderer.beginFrame(); BeginTextureMode(target); ClearBackground(background); BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        lighting.sunIntensity = .04F; lighting.ambientIntensity = .18F;
        lighting.dayNightTint = {.48F, .59F, .8F};
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        if (cover) DrawCube({0, 2, 3}, 12, 9, .1F, background);
        std::array<ian::EnemyDrawInstance, 3> enemies{};
        for (std::size_t i = 0; i < enemies.size(); ++i) {
            enemies[i] = {.modelVisual = model, .animationVisual = ian::EnemyAnimationVisual::Walk,
                .animationSeconds = time, .position = {-2.4F + float(i) * 2.4F, 0, 0},
                .yawRadians = .25F, .eliteAffixes = static_cast<std::uint8_t>(elite ? 1U << i : 0U)};
        }
        if (batched) static_cast<void>(renderer.drawEnemiesInstanced(enemies));
        else for (const auto& enemy : enemies) static_cast<void>(renderer.drawEnemy(
            enemy.modelVisual, enemy.animationVisual, enemy.animationSeconds, enemy.position,
            enemy.yawRadians, WHITE, 1, true, true, false, enemy.eliteAffixes));
        renderer.endWorldShader(); EndMode3D(); EndTextureMode();
        Image result = LoadImageFromTexture(target.texture); ImageFlipVertical(&result); return result;
    };
    const auto goldPixels = [](Image image) {
        int count = 0;
        for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
            const Color c = GetImageColor(image, x, y);
            if (c.r > 140 && c.g > 95 && c.b < c.g * .7F) ++count;
        }
        return count;
    };
    bool valid = true;
    for (int type = 0; type < 9; ++type) {
        Image image = render(static_cast<ian::EnemyModelVisual>(type), true, false);
        const auto model = static_cast<ian::EnemyModelVisual>(type);
        const auto ordinaryBounds = renderer.enemyWorldBounds(model, {}, .25F, 1);
        const auto eliteBounds = renderer.enemyWorldBounds(model, {}, .25F, 1, true);
        valid = valid && eliteBounds.max.y > ordinaryBounds.max.y;
        const int gold = goldPixels(image);
        valid = valid && gold > 100;
        if (gold <= 100) std::cerr << "elite crown missing for type " << type << ": " << gold << '\n';
        if (const char* directory = std::getenv("IAN_CAPTURE_ENEMIES")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path(directory) /
                ("elite-type-" + std::to_string(type) + ".png")).c_str());
        }
        UnloadImage(image);
    }
    Image single = render(ian::EnemyModelVisual::Minion, false, false);
    Image crowd = render(ian::EnemyModelVisual::Minion, true, false);
    Image cover = render(ian::EnemyModelVisual::Minion, true, true);
    Image normal = render(ian::EnemyModelVisual::Minion, true, false, false);
    Image later = render(ian::EnemyModelVisual::Minion, true, false, true, .69F);
    int differences = 0;
    for (int y = 0; y < single.height; ++y) for (int x = 0; x < single.width; ++x) {
        const Color a = GetImageColor(single, x, y), b = GetImageColor(crowd, x, y);
        if (std::max({std::abs(int(a.r)-int(b.r)), std::abs(int(a.g)-int(b.g)),
                std::abs(int(a.b)-int(b.b))}) > 8) ++differences;
    }
    valid = valid && differences < 500 && goldPixels(cover) == 0 && goldPixels(normal) == 0 && goldPixels(later) > 100;
    if (!valid) std::cerr << "elite batch difference/covered/normal: " << differences << '/' << goldPixels(cover)
        << '/' << goldPixels(normal) << '\n';
    for (Image image : {single, crowd, cover, normal, later}) UnloadImage(image);
    UnloadRenderTexture(target);
    return valid;
}

bool checkCartoonHearts(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(640, 360);
    const Camera3D camera{{0, .35F, 6}, {0, 0, 0}, {0, 1, 0},
                          2.3F, CAMERA_ORTHOGRAPHIC};
    const auto render = [&](float yaw, bool night = false, bool cover = false,
                            bool crowd = false) {
        BeginTextureMode(target);
        ClearBackground(night ? Color{18, 35, 57, 255} : Color{70, 98, 72, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        if (night) { lighting.sunIntensity = .05F; lighting.ambientIntensity = .2F; }
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.unlitAmount = .86F;
        material.screenAoAmount = 0;
        material.inkOutlineEligible = false;
        material.inkOccluder = true;
        renderer.setWorldMaterial(material);
        if (cover) DrawCube({0, 0, 1}, 2.5F, 2.5F, .1F, {80, 65, 110, 255});
        if (crowd) {
            for (int row = 0; row < 3; ++row) for (int col = 0; col < 8; ++col) {
                const Vector3 position{-1.8F + float(col) * .5F,
                    -.5F + float(row) * .5F, 0};
                if (row == 1 && (col == 2 || col == 5))
                    renderer.drawHeart(position, yaw, .9F);
                else if ((row + col) % 3 == 0)
                    renderer.drawCoin(static_cast<ian::CoinType>(col % 3), position,
                                      yaw + float(col) * .47F, .8F);
                else renderer.drawExperienceOctahedron(position, yaw + float(col) * .47F,
                    .8F, col == 6 && row == 1 ? 8 : col == 3 ? 4 : 1);
            }
        } else renderer.drawHeart({}, yaw, 3.0F);
        renderer.endWorldShader(); EndMode3D(); EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    Image front = render(.15F), angled = render(.55F), back = render(PI + .15F),
          night = render(.15F, true), edge = render(PI * .5F), covered = render(.15F, false, true);
    const auto count = [](Image image, auto predicate) {
        int result = 0;
        for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x)
            if (predicate(GetImageColor(image, x, y))) ++result;
        return result;
    };
    const auto red = [](Color c) { return c.r > 145 && c.r > c.g + 55 && c.r > c.b + 40; };
    const auto shine = [](Color c) { return c.r > 225 && c.g > 195 && c.b > 185; };
    // Rendered contour must retain two distinct lobes and a notch. Compare
    // their topmost filled rows after removing the model's fixed roll.
    const auto topAt = [&](int x) {
        for (int y = 0; y < front.height; ++y) {
            const Color c = GetImageColor(front, x, y);
            if (red(c) || shine(c)) return y;
        }
        return front.height;
    };
    const bool valid = count(front, red) > 5000 && count(angled, red) > 5000 &&
        count(back, red) > 5000 && count(night, red) > 5000 &&
        count(front, shine) > 10 && count(back, shine) > 10 &&
        count(edge, red) > count(front, red) * .2 &&
        count(covered, red) == 0 && topAt(320) > std::max(topAt(270), topAt(370)) + 8;
    if (!valid) std::cerr << "hearts red/shine/back/edge/notch " << count(front, red)
        << '/' << count(front, shine) << '/' << count(back, shine) << '/' << count(edge, red)
        << '/' << topAt(270) << '/' << topAt(320) << '/' << topAt(370) << '\n';
    if (const char* directory = std::getenv("IAN_CAPTURE_HEARTS")) {
        std::filesystem::create_directories(directory);
        for (const auto& pair : {std::pair{front, "heart-front.png"}, std::pair{angled, "heart-angled.png"},
             std::pair{night, "heart-night.png"}, std::pair{edge, "heart-side.png"}})
            ExportImage(pair.first, (std::filesystem::path(directory) / pair.second).c_str());
        Image crowd = render(.15F, false, false, true);
        ExportImage(crowd, (std::filesystem::path(directory) / "pickups.png").c_str());
        UnloadImage(crowd);
    }
    for (Image image : {front, angled, back, night, edge, covered}) UnloadImage(image);
    UnloadRenderTexture(target);
    return valid;
}
                          2.3F, CAMERA_ORTHOGRAPHIC};
    const auto render = [&](ian::CoinType type, float yaw, bool cover = false,
                            bool night = false, bool crowd = false) {
        BeginTextureMode(target);
        ClearBackground(night ? Color{18, 35, 57, 255} : Color{70, 98, 72, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        if (night) { lighting.sunIntensity = .05F; lighting.ambientIntensity = .2F; }
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.unlitAmount = .9F;
        material.screenAoAmount = 0;
        material.inkOutlineEligible = false;
        material.inkOccluder = true;
        renderer.setWorldMaterial(material);
        if (cover) DrawCube({0, 0, 1}, 2.5F, 2.5F, .1F, {80, 65, 110, 255});
        if (crowd) {
            for (int row = 0; row < 3; ++row) for (int col = 0; col < 8; ++col) {
                const Vector3 position{-1.8F + float(col) * .5F,
                    -.5F + float(row) * .5F, 0};
                const float angle = yaw + float(row + col) * .47F;
                if ((row + col) % 3 == 0)
                    renderer.drawCoin(static_cast<ian::CoinType>(col % 3), position, angle, .8F);
                else renderer.drawExperienceOctahedron(position, angle, .8F,
                    col == 6 && row == 1 ? 8 : col == 3 ? 4 : 1);
            }
        } else renderer.drawCoin(type, {}, yaw, 3.0F);
        renderer.endWorldShader(); EndMode3D(); EndTextureMode();
        Image result = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&result);
        return result;
    };
    Image common = render(ian::CoinType::Bronze, .35F);
    Image silver = render(ian::CoinType::Silver, .35F);
    Image gold = render(ian::CoinType::HighValue, .35F);
    Image back = render(ian::CoinType::Bronze, .35F + PI);
    Image edge = render(ian::CoinType::Bronze, PI * .5F);
    Image night = render(ian::CoinType::HighValue, .35F, false, true);
    Image covered = render(ian::CoinType::Bronze, .35F, true);
    Image goldCovered = render(ian::CoinType::HighValue, .35F, true);
    const auto count = [](Image image, auto predicate) {
        int total = 0;
        for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x)
            if (predicate(GetImageColor(image, x, y))) ++total;
        return total;
    };
    const auto warm = [](Color c) { return c.r > 170 && c.g > 100 && c.b < 155; };
    const auto cool = [](Color c) { return c.b > 170 && c.r > 100 && c.b > c.r + 10; };
    const auto pale = [](Color c) { return c.r > 220 && c.g > 220 && c.b > 170; };
    const bool valid = count(common, warm) > 1500 && count(back, warm) > 1500 &&
        count(silver, cool) > 1500 && count(gold, warm) > count(common, warm) * 1.2 &&
        count(gold, pale) > 100 && count(night, warm) > 1500 &&
        count(edge, warm) > 100 && count(edge, warm) < count(common, warm) * .35 &&
        count(covered, warm) == 0 && count(goldCovered, warm) == 0;
    if (!valid) std::cerr << "coins warm/back/silver/gold/pale/edge " << count(common, warm)
        << '/' << count(back, warm) << '/' << count(silver, cool) << '/' << count(gold, warm)
        << '/' << count(gold, pale) << '/' << count(edge, warm) << '\n';
    if (const char* directory = std::getenv("IAN_CAPTURE_COINS")) {
        std::filesystem::create_directories(directory);
        for (const auto& pair : {std::pair{common, "coin-1.png"}, std::pair{silver, "coin-5.png"},
             std::pair{gold, "coin-10.png"}, std::pair{night, "coin-night.png"}})
            ExportImage(pair.first, (std::filesystem::path(directory) / pair.second).c_str());
        Image crowd = render(ian::CoinType::Bronze, .3F, false, false, true);
        ExportImage(crowd, (std::filesystem::path(directory) / "coins-and-xp.png").c_str());
        UnloadImage(crowd);
    }
    for (Image image : {common, silver, gold, back, edge, night, covered, goldCovered}) UnloadImage(image);
    UnloadRenderTexture(target);
    return valid;
}

bool checkExperienceCrystals(ian::Renderer& renderer) {
    const auto oldSettings = renderer.settings();
    const int oldWidth = GetScreenWidth(), oldHeight = GetScreenHeight();
    SetWindowSize(640, 360); PollInputEvents();
    BeginDrawing(); ClearBackground(BLACK); EndDrawing();
    renderer.settings().pixelSize = 1;
    renderer.settings().renderWidth = 640;
    renderer.settings().renderHeight = 360;
    renderer.settings().postProcessing = true;
    renderer.settings().ssao = true;
    const auto preview = LoadRenderTexture(640, 360);
    const Camera3D camera{{0, 1.1F, 6}, {0, 0, 0}, {0, 1, 0},
                          2.3F, CAMERA_ORTHOGRAPHIC};
    const auto render = [&](bool ink, bool night, bool covered,
                            float yaw, bool crowd = false, bool worldPass = false,
                            int experienceValue = 1) {
        renderer.settings().inkOutlines = ink;
        const Color background = night ? Color{18, 35, 57, 255}
                                       : Color{70, 98, 72, 255};
        if (worldPass) {
            renderer.beginFrame();
            renderer.beginWorldPass(background, camera);
        } else {
            BeginTextureMode(preview);
            ClearBackground(background);
        }
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        if (night) { lighting.sunIntensity = .05F; lighting.ambientIntensity = .2F; }
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.unlitAmount = .82F;
        material.screenAoAmount = 0;
        material.inkOutlineEligible = false;
        material.inkOccluder = true;
        renderer.setWorldMaterial(material);
        if (covered) DrawCube({0, 0, 1}, 2.5F, 2.5F, .1F, {80, 65, 110, 255});
        if (crowd) {
            for (int row = 0; row < 4; ++row) for (int col = 0; col < 8; ++col)
                renderer.drawExperienceOctahedron(
                    {-1.8F + float(col) * .5F, -.55F + float(row) * .36F,
                     -float(row) * .18F}, yaw + float(col + row) * .73F, .75F,
                    col == 6 && row == 2 ? 8 : col == 3 ? 4 : 1);
        } else renderer.drawExperienceOctahedron({}, yaw, 2.6F, experienceValue);
        renderer.endWorldShader(); EndMode3D();
        if (worldPass) {
            renderer.endWorldPass();
            rlDrawRenderBatchActive();
            Image result = LoadImageFromScreen();
            renderer.endFrame();
            return result;
        }
        EndTextureMode();
        Image result = LoadImageFromTexture(preview.texture);
        ImageFlipVertical(&result);
        return result;
    };
    Image plain = render(false, false, false, .3F, false, true);
    Image worldInk = render(true, false, false, .3F, false, true);
    Image ink = render(true, false, false, .3F);
    Image night = render(true, true, false, .3F);
    Image turned = render(true, false, false, 1.25F);
    Image covered = render(true, false, true, .3F);
    Image coverTurned = render(true, false, true, 1.25F);
    Image blue = render(true, false, false, .3F, false, false, 4);
    Image gold = render(true, false, false, .3F, false, false, 8);
    Image goldNight = render(true, true, false, .3F, false, false, 8);
    Image goldCovered = render(true, false, true, .3F, false, false, 8);
    const auto difference = [](Image a, Image b) {
        std::size_t changed = 0;
        for (int y = 0; y < a.height; ++y) for (int x = 0; x < a.width; ++x) {
            const auto ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y);
            if (std::abs(int(ca.r) - int(cb.r)) + std::abs(int(ca.g) - int(cb.g)) +
                std::abs(int(ca.b) - int(cb.b)) > 15) ++changed;
        }
        return changed;
    };
    int cyan = 0, mint = 0, nightCyan = 0;
    for (int y = 0; y < ink.height; ++y) for (int x = 0; x < ink.width; ++x) {
        const auto c = GetImageColor(ink, x, y), n = GetImageColor(night, x, y);
        if (c.g > 130 && c.b > 135 && c.r + 40 < c.g) ++cyan;
        if (c.r > 170 && c.g > 210 && c.b > 190) ++mint;
        if (n.g > 110 && n.b > 120 && n.r + 40 < n.g) ++nightCyan;
    }
    int bluePixels = 0, goldPixels = 0, nightGoldPixels = 0;
    const auto bodyArea = [](Image image) {
        const Color background = GetImageColor(image, 0, 0);
        int area = 0;
        for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
            const Color c = GetImageColor(image, x, y);
            if (std::abs(int(c.r) - int(background.r)) +
                std::abs(int(c.g) - int(background.g)) +
                std::abs(int(c.b) - int(background.b)) > 35) ++area;
        }
        return area;
    };
    for (int y = 0; y < blue.height; ++y) for (int x = 0; x < blue.width; ++x) {
        const Color b = GetImageColor(blue, x, y), g = GetImageColor(gold, x, y),
                    n = GetImageColor(goldNight, x, y);
        if (b.b > 160 && b.b > b.g + 20 && b.b > b.r + 60) ++bluePixels;
        if (g.r > 180 && g.g > 90 && g.b < 150) ++goldPixels;
        if (n.r > 180 && n.g > 90 && n.b < 150) ++nightGoldPixels;
    }
    const bool tiersValid = bluePixels > 1000 && goldPixels > 1000 &&
        nightGoldPixels > 1000 && bodyArea(blue) > bodyArea(ink) * 1.2 &&
        bodyArea(gold) > bodyArea(blue) * 1.1 &&
        difference(covered, goldCovered) < 20;
    if (!tiersValid) std::cerr << "XP blue/gold/night " << bluePixels << '/'
        << goldPixels << '/' << nightGoldPixels << " areas " << bodyArea(ink)
        << '/' << bodyArea(blue) << '/' << bodyArea(gold) << '\n';
    const bool valid = tiersValid && cyan > 1500 && mint > 100 && nightCyan > 1000 &&
        difference(plain, worldInk) < 20 && difference(ink, turned) > 1000 &&
        difference(covered, coverTurned) < 20;
    if (!valid) std::cerr << "XP cyan/mint/night " << cyan << '/' << mint << '/'
        << nightCyan << " ink " << difference(plain, worldInk) << " turn "
        << difference(ink, turned) << " cover " << difference(covered, coverTurned) << '\n';
    if (const char* directory = std::getenv("IAN_CAPTURE_XP")) {
        std::filesystem::create_directories(directory);
        ExportImage(ink, (std::filesystem::path(directory) / "xp-day.png").c_str());
        ExportImage(night, (std::filesystem::path(directory) / "xp-night.png").c_str());
        ExportImage(blue, (std::filesystem::path(directory) / "xp-blue.png").c_str());
        ExportImage(gold, (std::filesystem::path(directory) / "xp-gold.png").c_str());
        ExportImage(goldNight, (std::filesystem::path(directory) / "xp-gold-night.png").c_str());
        Image field = render(true, false, false, .3F, true);
        ExportImage(field, (std::filesystem::path(directory) / "xp-field.png").c_str());
        UnloadImage(field);
    }
    for (Image image : {plain, worldInk, ink, night, turned, covered, coverTurned,
                        blue, gold, goldNight, goldCovered}) UnloadImage(image);
    UnloadRenderTexture(preview);
    renderer.settings() = oldSettings;
    SetWindowSize(oldWidth, oldHeight); PollInputEvents();
    return valid;
}

bool checkCartoonBurn(ian::Renderer& renderer) {
    using namespace ian::app_burn_drawing;
    const auto target = LoadRenderTexture(384, 256);
    const Camera3D camera{{0, 1.3F, 7}, {0, 1.3F, 0}, {0, 1, 0},
                          3.4F, CAMERA_ORTHOGRAPHIC};
    const auto render = [&](double time, float amount, bool detailed,
                            bool cover, float scale = 1.0F) {
        BeginTextureMode(target);
        ClearBackground({24, 49, 65, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.unlitAmount = 1;
        material.screenAoAmount = 0;
        material.inkOutlineEligible = false;
        material.inkOccluder = false;
        renderer.setWorldMaterial(material);
        if (cover) DrawCube({0, 1.3F, 1}, 4, 4, .1F, GREEN);
        for (const auto pass : {Pass::Glow, Pass::Smoke, Pass::Flame}) {
            beginBatch(pass);
            emit({0, 0, 0}, {1, 0, 0}, {0, 0, 1}, scale, 42,
                 time, amount, detailed, pass);
            endBatch();
        }
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    Image base = render(0.31, 0, true, false);
    Image fire = render(0.31, 1, true, false);
    Image next = render(0.63, 1, true, false);
    Image faded = render(0.31, .1F, true, false);
    Image cheap = render(0.31, 1, false, false);
    Image coveredBase = render(0.31, 0, true, true);
    Image covered = render(0.31, 1, true, true);
    Image small = render(0.31, 1, true, false, .6F);
    Image large = render(0.31, 1, true, false, 1.65F);
    const auto difference = [](const Image& a, const Image& b) {
        Color* ca = LoadImageColors(a);
        Color* cb = LoadImageColors(b);
        std::size_t total = 0;
        for (int i = 0; i < a.width * a.height; ++i) {
            total += static_cast<std::size_t>(std::abs(int(ca[i].r) - int(cb[i].r)) +
                std::abs(int(ca[i].g) - int(cb[i].g)) +
                std::abs(int(ca[i].b) - int(cb[i].b)));
        }
        UnloadImageColors(ca); UnloadImageColors(cb);
        return total;
    };
    int orange = 0, yellow = 0, hot = 0;
    for (int y = 0; y < fire.height; ++y) {
        for (int x = 0; x < fire.width; ++x) {
            const auto c = GetImageColor(fire, x, y);
            if (c.r > 200 && c.g > 50 && c.g < 170 && c.b < 60) ++orange;
            if (c.r > 210 && c.g > 170 && c.b < 145) ++yellow;
            if (c.r > 230 && c.g > 220 && c.b > 145) ++hot;
        }
    }
    const bool valid = orange > 80 && yellow > 80 && hot > 10 &&
        difference(fire, next) > 10000 &&
        difference(base, faded) < difference(base, fire) / 2 &&
        difference(base, cheap) > 50000 && difference(coveredBase, covered) < 100 &&
        difference(base, large) > difference(base, small) * 2;
    if (!valid) std::cerr << "burn palette " << orange << '/' << yellow << '/' << hot
        << " occlusion " << difference(coveredBase, covered) << '\n';
    if (const char* directory = std::getenv("IAN_CAPTURE_BURN")) {
        std::filesystem::create_directories(directory);
        ExportImage(fire, (std::filesystem::path(directory) / "burn-detail.png").c_str());
        ExportImage(cheap, (std::filesystem::path(directory) / "burn-low-detail.png").c_str());
        const auto preview = LoadRenderTexture(960, 540);
        const Camera3D view{{4.0F, 3.1F, 8.0F}, {0, .9F, 0}, {0, 1, 0},
                            40, CAMERA_PERSPECTIVE};
        Vector3 right = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(view.target, view.position), {0, 1, 0}));
        for (const bool night : {false, true}) {
            for (int frame = 0; frame < 4; ++frame) {
                const double time = .25 + frame * .16;
                renderer.beginFrame();
                BeginTextureMode(preview);
                ClearBackground(night ? Color{13, 29, 48, 255} : Color{111, 170, 187, 255});
                BeginMode3D(view);
                ian::WorldLighting lighting{};
                lighting.cameraPosition = view.position;
                lighting.sunDirection = {-0.4F, -0.8F, -0.3F};
                lighting.sunColor = night ? Vector3{.2F, .3F, .55F} : Vector3{1, .86F, .65F};
                lighting.skyAmbientColor = night ? Vector3{.12F, .18F, .3F} : Vector3{.4F, .5F, .5F};
                renderer.beginWorldShader(lighting);
                ian::WorldMaterialState material{};
                renderer.setWorldMaterial(material);
                DrawPlane({0, -.03F, 0}, {20, 20}, night ? Color{28, 57, 63, 255} : Color{68, 113, 83, 255});
                constexpr std::array<Vector3, 3> positions{{{-1.9F, 0, 0}, {0, 0, 0}, {1.9F, 0, 0}}};
                constexpr std::array<ian::EnemyModelVisual, 3> models{{
                    ian::EnemyModelVisual::Minion, ian::EnemyModelVisual::Warrior,
                    ian::EnemyModelVisual::Rogue}};
                for (std::size_t index = 0; index < positions.size(); ++index) {
                    static_cast<void>(renderer.drawEnemy(models[index],
                        ian::EnemyAnimationVisual::Idle, 0, positions[index], 0,
                        {255, 198, 132, 255}, index == 1 ? .67F : index == 2 ? .55F : .94F));
                }
                material.unlitAmount = 1;
                material.screenAoAmount = 0;
                material.inkOutlineEligible = false;
                renderer.setWorldMaterial(material);
                for (const auto pass : {Pass::Glow, Pass::Smoke, Pass::Flame}) {
                    beginBatch(pass);
                    for (std::size_t index = 0; index < positions.size(); ++index) {
                        Vector3 front = Vector3Subtract(view.position, positions[index]);
                        front.y = 0;
                        front = Vector3Normalize(front);
                        const float scale = index == 1 ? 1.25F : index == 2 ? .82F : 1.0F;
                        const auto bounds = renderer.enemyWorldBounds(models[index], positions[index],
                            0, index == 1 ? .67F : index == 2 ? .55F : .94F);
                        const Vector3 extent = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), .5F);
                        const float radius = std::clamp(std::abs(front.x) * extent.x +
                            std::abs(front.z) * extent.z, scale * .35F, scale * .95F);
                        emit(positions[index], right, front, scale,
                            static_cast<std::uint32_t>(index * 19 + 3), time, 1, true, pass, radius);
                    }
                    endBatch();
                }
                renderer.endWorldShader();
                EndMode3D();
                EndTextureMode();
                Image capture = LoadImageFromTexture(preview.texture);
                ImageFlipVertical(&capture);
                const std::string filename = std::string(night ? "night-" : "day-") +
                                             std::to_string(frame) + ".png";
                ExportImage(capture, (std::filesystem::path(directory) / filename).c_str());
                UnloadImage(capture);
                renderer.endFrame();
            }
        }
        UnloadRenderTexture(preview);
    }
    for (Image image : {base, fire, next, faded, cheap, coveredBase, covered, small, large})
        UnloadImage(image);
    UnloadRenderTexture(target);
    return valid;
}

bool checkCartoonPoison(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(384, 256);
    const Camera3D camera{{0, 1.3F, 7}, {0, 1.3F, 0}, {0, 1, 0}, 3.4F, CAMERA_ORTHOGRAPHIC};
    const auto render = [&](double seconds, float amount, bool detailed, bool covered) {
        BeginTextureMode(target);
        ClearBackground({24, 49, 65, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.unlitAmount = 1;
        material.screenAoAmount = 0;
        material.inkOutlineEligible = false;
        renderer.setWorldMaterial(material);
        if (covered) DrawCube({0, 1.3F, 2}, 5, 5, .1F, BLUE);
        ian::app_freeze_drawing::beginBatch();
        ian::app_poison_drawing::emitPoison({}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1},
            .8F, 2.2F, 42, seconds, amount, detailed);
        ian::app_freeze_drawing::endBatch();
        renderer.endWorldShader();
        EndMode3D(); EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    const auto difference = [](Image a, Image b) {
        std::size_t sum = 0;
        const auto* ca = static_cast<Color*>(a.data);
        const auto* cb = static_cast<Color*>(b.data);
        for (int i = 0; i < a.width * a.height; ++i)
            sum += static_cast<std::size_t>(std::abs(int(ca[i].r) - int(cb[i].r)) +
                std::abs(int(ca[i].g) - int(cb[i].g)) + std::abs(int(ca[i].b) - int(cb[i].b)));
        return sum;
    };
    Image base = render(.31, 0, true, false), poison = render(.31, 1, true, false);
    Image next = render(.73, 1, true, false), faded = render(.31, .1F, true, false);
    Image cheap = render(.31, 1, false, false), coverBase = render(.31, 0, true, true);
    Image covered = render(.31, 1, true, true);
    int green = 0, highlights = 0;
    for (int y = 0; y < poison.height; ++y) for (int x = 0; x < poison.width; ++x) {
        const auto c = GetImageColor(poison, x, y);
        if (c.g > 120 && c.g > c.r + 25 && c.g > c.b + 30) ++green;
        if (c.r > 170 && c.g > 190 && c.b > 110 && c.b < c.g - 10) ++highlights;
    }
    const bool result = green > 100 && highlights > 3 && difference(base, poison) > 10000 &&
        difference(poison, next) > 3000 && difference(base, faded) < difference(base, poison) &&
        difference(base, cheap) > 1000 && difference(coverBase, covered) == 0;
    if (const char* path = std::getenv("IAN_CAPTURE_POISON")) ExportImage(poison, path);
    for (Image image : {base, poison, next, faded, cheap, coverBase, covered}) UnloadImage(image);
    UnloadRenderTexture(target);
    if (!result) std::cerr << "poison visual: green=" << green << " highlights=" << highlights << '\n';
    return result;
}


bool checkCartoonFreeze(ian::Renderer& renderer) {
    using namespace ian::app_freeze_drawing;
    const auto target = LoadRenderTexture(384, 256);
    const Camera3D camera{{0, 1.3F, 7}, {0, 1.3F, 0}, {0, 1, 0},
                          3.4F, CAMERA_ORTHOGRAPHIC};
    const auto render = [&](double time, float amount, bool detailed, bool cover,
                            int impact = 0, float progress = .12F) {
        BeginTextureMode(target);
        ClearBackground({24, 49, 65, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.unlitAmount = 1;
        material.screenAoAmount = 0;
        material.inkOutlineEligible = false;
        material.inkOccluder = false;
        renderer.setWorldMaterial(material);
        if (cover) DrawCube({0, 1.3F, 2}, 5, 5, .1F, GREEN);
        for (const auto pass : {Pass::Ice, Pass::Atmosphere}) {
            beginBatch();
            if (impact != 0) {
                emitImpact({0, 1, 0}, {1, 0, 0}, {0, 1, 0}, progress,
                    amount, 42, impact == 2, detailed, pass);
            } else {
                emitFrozen({}, {1, 0, 0}, {0, 0, 1}, .8F, 2.2F, 42,
                    time, amount, amount < 1, detailed, pass);
            }
            endBatch();
        }
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    Image base = render(.31, 0, true, false);
    Image frozen = render(.31, 1, true, false);
    Image next = render(.73, 1, true, false);
    Image faded = render(.31, .1F, true, false);
    Image cheap = render(.31, 1, false, false);
    Image coveredBase = render(.31, 0, true, true);
    Image covered = render(.31, 1, true, true);
    Image impact = render(.31, 1, true, false, 1);
    Image crack = render(.31, 1, true, false, 2);
    Image finished = render(.31, 1, true, false, 1, 1);
    Image seamBefore = render(99.999, 1, true, false);
    Image seamAfter = render(100.001, 1, true, false);
    const auto difference = [](const Image& a, const Image& b) {
        Color* ca = LoadImageColors(a);
        Color* cb = LoadImageColors(b);
        std::size_t total = 0;
        for (int i = 0; i < a.width * a.height; ++i) {
            total += static_cast<std::size_t>(std::abs(int(ca[i].r) - int(cb[i].r)) +
                std::abs(int(ca[i].g) - int(cb[i].g)) +
                std::abs(int(ca[i].b) - int(cb[i].b)));
        }
        UnloadImageColors(ca); UnloadImageColors(cb);
        return total;
    };
    int cyan = 0, white = 0;
    for (int y = 0; y < frozen.height; ++y) {
        for (int x = 0; x < frozen.width; ++x) {
            const auto c = GetImageColor(frozen, x, y);
            if (c.b > 180 && c.g > 125 && c.b > c.r + 30) ++cyan;
            if (c.r > 180 && c.g > 210 && c.b > 220) ++white;
        }
    }
    const bool valid = cyan > 80 && white > 10 &&
        difference(frozen, next) > 1000 &&
        difference(base, faded) < difference(base, frozen) / 2 &&
        difference(base, cheap) > 50000 && difference(coveredBase, covered) < 100 &&
        difference(base, impact) > 10000 && difference(impact, crack) > 1000 &&
        difference(base, finished) == 0 && difference(seamBefore, seamAfter) < difference(frozen, next) / 10;
    if (!valid) std::cerr << "freeze palette " << cyan << '/' << white
        << " occlusion " << difference(coveredBase, covered)
        << " motion " << difference(frozen, next)
        << " seam " << difference(seamBefore, seamAfter) << '\n';
    if (const char* directory = std::getenv("IAN_CAPTURE_FREEZE")) {
        std::filesystem::create_directories(directory);
        for (const auto& pair : {std::pair{frozen, "freeze-detail.png"},
                                 std::pair{cheap, "freeze-low-detail.png"},
                                 std::pair{impact, "ice-impact.png"},
                                 std::pair{faded, "thaw.png"}})
            ExportImage(pair.first, (std::filesystem::path(directory) / pair.second).c_str());
        const auto preview = LoadRenderTexture(960, 540);
        const Camera3D view{{4, 3.1F, 8}, {0, .9F, 0}, {0, 1, 0}, 40, CAMERA_PERSPECTIVE};
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(
            Vector3Subtract(view.target, view.position), {0, 1, 0}));
        constexpr std::array<Vector3, 3> positions{{{-1.9F, 0, 0}, {0, 0, 0}, {1.9F, 0, 0}}};
        constexpr std::array<ian::EnemyModelVisual, 3> models{{
            ian::EnemyModelVisual::Minion, ian::EnemyModelVisual::Warrior, ian::EnemyModelVisual::Rogue}};
        constexpr std::array<float, 3> modelScales{{.94F, .67F, .55F}};
        constexpr std::array<float, 3> effectScales{{1, 1.25F, .82F}};
        for (const bool night : {false, true}) {
            for (int frame = 0; frame < 4; ++frame) {
                renderer.beginFrame();
                BeginTextureMode(preview);
                ClearBackground(night ? Color{13, 29, 48, 255} : Color{111, 170, 187, 255});
                BeginMode3D(view);
                ian::WorldLighting lighting{};
                lighting.cameraPosition = view.position;
                lighting.sunDirection = {-.4F, -.8F, -.3F};
                lighting.sunColor = night ? Vector3{.2F, .3F, .55F} : Vector3{1, .86F, .65F};
                lighting.skyAmbientColor = night ? Vector3{.12F, .18F, .3F} : Vector3{.4F, .5F, .5F};
                renderer.beginWorldShader(lighting);
                ian::WorldMaterialState material{};
                renderer.setWorldMaterial(material);
                DrawPlane({0, -.03F, 0}, {20, 20}, night ? Color{28, 57, 63, 255} : Color{68, 113, 83, 255});
                const float amount = frame == 3 ? .45F : 1;
                for (std::size_t index = 0; index < positions.size(); ++index) {
                    static_cast<void>(renderer.drawEnemy(models[index], ian::EnemyAnimationVisual::Idle,
                        0, positions[index], 0, {151, 224, 255, 255}, modelScales[index]));
                }
                material.unlitAmount = 1;
                material.screenAoAmount = 0;
                material.inkOutlineEligible = false;
                material.inkOccluder = false;
                renderer.setWorldMaterial(material);
                for (const auto pass : {Pass::Ice, Pass::Atmosphere}) {
                    beginBatch();
                    for (std::size_t reverse = positions.size(); reverse > 0; --reverse) {
                        const std::size_t index = reverse - 1;
                        Vector3 front = Vector3Subtract(view.position, positions[index]);
                        front.y = 0; front = Vector3Normalize(front);
                        const auto bounds = renderer.enemyWorldBounds(models[index], positions[index],
                            0, modelScales[index]);
                        const Vector3 extent = Vector3Scale(Vector3Subtract(bounds.max, bounds.min), .5F);
                        const float radius = std::clamp(std::max(extent.x, extent.z),
                            effectScales[index] * .35F, effectScales[index] * .95F);
                        const float height = std::clamp(bounds.max.y - bounds.min.y,
                            effectScales[index] * .8F, effectScales[index] * 2.5F);
                        emitFrozen(positions[index], right, front, radius, height,
                            static_cast<std::uint32_t>(index * 19 + 3), .25 + frame * .16,
                            amount, frame == 3, true, pass);
                    }
                    endBatch();
                }
                renderer.endWorldShader();
                EndMode3D(); EndTextureMode();
                Image capture = LoadImageFromTexture(preview.texture);
                ImageFlipVertical(&capture);
                const std::string name = std::string(night ? "night-" : "day-") + std::to_string(frame) + ".png";
                ExportImage(capture, (std::filesystem::path(directory) / name).c_str());
                UnloadImage(capture);
                renderer.endFrame();
            }
        }
        UnloadRenderTexture(preview);
    }
    for (Image image : {base, frozen, next, faded, cheap, coveredBase, covered,
                        impact, crack, finished, seamBefore, seamAfter}) UnloadImage(image);
    UnloadRenderTexture(target);
    return valid;
}

bool checkFireWandAttack(ian::Renderer& renderer) {
    using namespace ian::fire_wand_effect;
    const auto target = LoadRenderTexture(480, 320);
    const Camera3D camera{{0, 1, 8}, {0, 1, 0}, {0, 1, 0}, 4, CAMERA_ORTHOGRAPHIC};
    ian::IceWandProjectile projectile{};
    projectile.active = true;
    projectile.element = ian::WandElement::Fire;
    projectile.radius = .3;
    projectile.id = {6101, 4};
    projectile.position = {1.3, 1.5, 0};
    projectile.previousPosition = {1.1, 1.5, 0};
    projectile.velocity = {18, 2, 0};
    projectile.trailCount = projectile.trail.size();
    for (std::size_t i = 0; i < projectile.trail.size(); ++i) {
        const double t = static_cast<double>(i) * .13;
        projectile.trail[i] = {1.3 - t, 1.5 - t * t * .18, 0};
    }
    const auto render = [&](int mode, double time, bool cover, bool detailed,
                            float amount = 1, bool behindAfter = false) {
        BeginTextureMode(target);
        ClearBackground({24, 49, 65, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.unlitAmount = 1;
        material.screenAoAmount = 0;
        material.inkOutlineEligible = false;
        renderer.setWorldMaterial(material);
        if (cover) DrawCube({0, 1, 2}, 7, 6, .1F, GREEN);
        if (mode == 1 || mode == 5 || mode == 6) {
            auto shot = projectile;
            shot.active = mode != 5;
            if (mode == 6) shot.trailCount = 0;
            beginBatch(true);
            emitProjectile(shot, camera.position, time, .5F);
            endBatch();
        } else if (mode == 3 || mode == 4) {
            rlPushMatrix(); rlScalef(6, 6, 6);
            beginBatch();
            emitStaff({0, .17F, 0}, time, amount, mode == 4 ? .9F : 0, detailed);
            endBatch();
            rlPopMatrix();
        }
        if (behindAfter) DrawCube({1.2F, 1.5F, -.5F}, .3F, .3F, .1F, MAGENTA);
        renderer.endWorldShader(); EndMode3D(); EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image); return image;
    };
    Image base = render(0, .1, false, true);
    Image fire = render(1, .1, false, true);
    Image next = render(1, .35, false, true);
    Image cheap = render(1, .1, false, false);
    Image coverBase = render(0, .1, true, true);
    Image covered = render(1, .1, true, true);
    Image idle = render(3, .1, false, true, 0);
    Image charged = render(3, .1, false, true, 1);
    Image flash = render(4, .1, false, true, 0);
    Image inactive = render(5, .1, false, true);
    Image withRear = render(1, .1, false, true, 1, true);
    Image noTrail = render(6, .1, false, true);
    const auto difference = [](const Image& a, const Image& b) {
        Color* ca = LoadImageColors(a); Color* cb = LoadImageColors(b);
        std::size_t total = 0;
        for (int i = 0; i < a.width * a.height; ++i)
            total += static_cast<std::size_t>(std::abs(int(ca[i].r) - int(cb[i].r)) +
                std::abs(int(ca[i].g) - int(cb[i].g)) + std::abs(int(ca[i].b) - int(cb[i].b)));
        UnloadImageColors(ca); UnloadImageColors(cb); return total;
    };
    int orange = 0, gold = 0, hot = 0, outside = 0;
    for (int y = 0; y < fire.height; ++y) for (int x = 0; x < fire.width; ++x) {
        const auto c = GetImageColor(fire, x, y);
        if (c.r > 200 && c.g > 50 && c.g < 150 && c.b < 70) ++orange;
        if (c.r > 220 && c.g > 160 && c.b < 145) ++gold;
        if (c.r > 230 && c.g > 220 && c.b > 145) ++hot;
        const float dx = static_cast<float>(x) - 336, dy = static_cast<float>(y) - 120;
        if (dx * dx + dy * dy > 32 * 32 && c.r > 90) ++outside;
    }
    const bool valid = orange > 80 && gold > 80 && hot > 10 && outside == 0 &&
        difference(fire, noTrail) == 0 &&
        difference(fire, next) > 10000 && difference(base, cheap) > 50000 &&
        difference(coverBase, covered) == 0 && difference(base, inactive) == 0 &&
        difference(base, charged) > difference(base, idle) * 2 &&
        difference(idle, flash) > 10000 && difference(fire, withRear) < 100;
    if (!valid) std::cerr << "fire wand palette " << orange << '/' << gold << '/' << hot
        << " outside " << outside << " trail " << difference(fire, noTrail)
        << " cover " << difference(coverBase, covered)
        << " rear " << difference(fire, withRear)
        << " charge " << difference(base, charged) << '/' << difference(base, idle) << '\n';
    if (const char* directory = std::getenv("IAN_CAPTURE_FIRE_WAND")) {
        std::filesystem::create_directories(directory);
        for (const auto& pair : {std::pair{fire, "fire-flight.png"},
            std::pair{next, "fire-flight-next.png"},
            std::pair{charged, "fire-charge.png"}, std::pair{flash, "fire-release.png"}})
            ExportImage(pair.first, (std::filesystem::path(directory) / pair.second).c_str());
        const auto preview = LoadRenderTexture(960, 540);
        const auto visual = ian::FirstPersonToolVisual::FireWand;
        const auto tuning = ian::defaultFirstPersonToolTuning(visual);
        const auto view = ian::firstPersonToolCamera(visual);
        if (renderer.beginFirstPersonToolPass()) renderer.endFirstPersonToolPass(tuning);
        for (int phase = 0; phase < 3; ++phase) {
            BeginTextureMode(preview);
            ClearBackground({33, 67, 86, 255});
            BeginMode3D(view);
            static_cast<void>(renderer.drawFirstPersonTool(visual, 0, 0, 0,
                tuning, phase == 1 ? 1.0F : 0.0F, phase == 2 ? .9F : 0.0F));
            EndMode3D(); EndTextureMode();
            Image capture = LoadImageFromTexture(preview.texture);
            ImageFlipVertical(&capture);
            const std::string name = "fire-staff-" + std::to_string(phase) + ".png";
            ExportImage(capture, (std::filesystem::path(directory) / name).c_str());
            UnloadImage(capture);
        }
        UnloadRenderTexture(preview);
    }
    for (Image image : {base, fire, next, cheap, coverBase, covered,
                        idle, charged, flash, inactive, withRear, noTrail}) UnloadImage(image);
    UnloadRenderTexture(target);
    return valid;
}

bool checkFireSelection(ian::Renderer& renderer) {
    using namespace ian::fire_wand_effect;
    const int oldWidth = GetScreenWidth(), oldHeight = GetScreenHeight();
    const auto oldSettings = renderer.settings();
    SetWindowSize(480, 320);
    PollInputEvents();
    BeginDrawing(); ClearBackground(BLACK); EndDrawing();
    renderer.settings().pixelSize = 1;
    renderer.settings().renderWidth = 480;
    renderer.settings().renderHeight = 320;
    renderer.settings().ssao = true;
    renderer.settings().postProcessing = true;
    renderer.settings().inkOutlines = false;
    const Camera3D camera{{0, 0, 8}, {0, 0, 0}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE};
    const auto render = [&](int mode, bool outline, bool sky = false) {
        renderer.beginFrame();
        if (renderer.beginSelectionMaskPass(camera)) {
            renderer.setSelectionMaskColor(WHITE);
            DrawCube({0, 0, 0}, 1, 1, 1, WHITE);
            renderer.endSelectionMaskPass();
        }
        renderer.beginWorldPass({25, 49, 65, 255}, camera);
        if (sky) renderer.drawSky(ian::SkyState{});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.unlitAmount = 1;
        material.screenAoAmount = 0;
        material.inkOutlineEligible = false;
        renderer.setWorldMaterial(material);
        DrawCube({0, 0, 0}, 1, 1, 1, {66, 52, 81, 255});
        if (mode != 0) {
            if (mode == 4) {
                material.inkOccluder = true;
                renderer.setWorldMaterial(material);
            }
            renderer.beginColorOnlyWorldEffects();
            if (mode == 3) {
                for (auto pass : {ian::app_burn_drawing::Pass::Glow,
                    ian::app_burn_drawing::Pass::Smoke, ian::app_burn_drawing::Pass::Flame}) {
                    ian::app_burn_drawing::beginBatch(pass);
                    ian::app_burn_drawing::emit({0, -.5F, .6F}, {1, 0, 0}, {0, 0, 1},
                        1, 42, .3, 1, true, pass, .5F);
                    ian::app_burn_drawing::endBatch();
                }
            } else {
                beginBatch();
                // A foreground/behind translucent effect with a solid core,
                // using the same world fragment output as explosion clouds.
                disc({0, 0, mode != 2 ? 1.0F : -2.0F}, {1, 0, 0}, {0, 1, 0},
                    1.3F, {255, 97, 18, 255}, {249, 76, 16, 255});
                endBatch();
            }
            renderer.endColorOnlyWorldEffects();
        }
        renderer.endWorldShader(); EndMode3D(); renderer.endWorldPass();
        if (outline) renderer.drawSelectionOutline();
        rlDrawRenderBatchActive();
        Image result = LoadImageFromScreen();
        renderer.endFrame();
        return result;
    };
    const auto contourPixels = [](Image a, Image b) {
        if (a.width != b.width || a.height != b.height) return -1;
        int count = 0;
        for (int y = 0; y < a.height; ++y) for (int x = 0; x < a.width; ++x) {
            const Color ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y);
            if (std::abs(int(ca.r) - int(cb.r)) + std::abs(int(ca.g) - int(cb.g)) +
                std::abs(int(ca.b) - int(cb.b)) > 60) ++count;
        }
        return count;
    };
    Image plain = render(0, false), selected = render(0, true);
    Image front = render(1, false), frontSelected = render(1, true);
    Image back = render(2, false), backSelected = render(2, true);
    Image burn = render(3, false), burnSelected = render(3, true);
    Image cloud = render(4, false), cloudSelected = render(4, true);
    Image sky = render(0, false, true), skySelected = render(0, true, true);
    // Coverage must be cleared between frames, including after deselection.
    renderer.clearSelectionOutline();
    Image cleared = render(0, true);
    renderer.settings().pixelSize = 2;
    Image resizedFront = render(1, false), resizedSelected = render(1, true);
    renderer.settings().pixelSize = 1;
    renderer.settings().ssao = false;
    Image noAoFront = render(1, false), noAoSelected = render(1, true);
    renderer.settings().ssao = true;
    const int baseline = contourPixels(selected, plain);
    const int foreground = contourPixels(frontSelected, front);
    const int background = contourPixels(backSelected, back);
    const int againstSky = contourPixels(skySelected, sky);
    const int burning = contourPixels(burnSelected, burn);
    const int afterClear = contourPixels(cleared, plain);
    bool valid = baseline > 100 && foreground < baseline / 3 &&
        background > baseline * .8 && againstSky > baseline * .8 &&
        afterClear > baseline * .8 && burning < baseline * .8 &&
        contourPixels(resizedSelected, resizedFront) < baseline / 3 &&
        contourPixels(noAoSelected, noAoFront) < baseline / 3 &&
        contourPixels(cloudSelected, cloud) < baseline / 3;
    if (!valid) std::cerr << "fire selection contours baseline/front/back/sky/reset "
        << baseline << '/' << foreground << '/' << background << '/'
        << againstSky << '/' << afterClear << " burn " << burning << '\n';

    if (const char* directory = std::getenv("IAN_CAPTURE_FIRE_WAND")) {
        std::filesystem::create_directories(directory);
        for (const auto& pair : {std::pair{frontSelected, "fire-selection-front.png"},
            std::pair{burnSelected, "fire-selection-burning.png"},
            std::pair{backSelected, "fire-selection-behind.png"},
            std::pair{skySelected, "fire-selection-sky.png"},
            std::pair{sky, "fire-selection-sky-plain.png"},
            std::pair{selected, "fire-selection-plain.png"},
            std::pair{plain, "fire-selection-none.png"}})
            ExportImage(pair.first, (std::filesystem::path(directory) / pair.second).c_str());
    }
    for (Image image : {plain, selected, front, frontSelected, back, backSelected,
        sky, skySelected, burn, burnSelected, cloud, cloudSelected, cleared, resizedFront, resizedSelected,
        noAoFront, noAoSelected}) UnloadImage(image);
    renderer.clearSelectionOutline();
    renderer.settings() = oldSettings;
    SetWindowSize(oldWidth, oldHeight);
    PollInputEvents();
    return valid;
}

std::size_t changedPixels(const Image& first, const Image& second) {
    Color* a = LoadImageColors(first);
    Color* b = LoadImageColors(second);
    std::size_t changed = 0;
    for (int pixel = 0; pixel < first.width * first.height; ++pixel) {
        if (std::abs(static_cast<int>(a[pixel].r) - b[pixel].r) +
            std::abs(static_cast<int>(a[pixel].g) - b[pixel].g) +
            std::abs(static_cast<int>(a[pixel].b) - b[pixel].b) > 12) ++changed;
    }
    UnloadImageColors(a);
    UnloadImageColors(b);
    return changed;
}

bool checkAdditiveEffectPass() {
    ian::ShaderResource resource;
    if (!resource.load(nullptr, "assets/shaders/vfx_impact.fs")) return false;
    const int textureLocation = GetShaderLocation(resource.get(), "useTexture");
    Image source = GenImageColor(8, 8, {60, 200, 80, 128});
    const Texture2D texture = LoadTextureFromImage(source);
    UnloadImage(source);
    const auto target = LoadRenderTexture(64, 32);
    bool valid = true;
    for (const bool tagged : {true, false}) {
        const Shader shader = tagged ? resource.get() : Shader{};
        BeginTextureMode(target);
        ClearBackground({20, 30, 40, 255});
        // Submit background geometry before switching the blend/shader state.
        DrawRectangle(0, 0, 64, 32, {20, 30, 40, 255});
        ian::app_effect_drawing::beginAdditiveEffectPass(shader, textureLocation);
        DrawRectangle(2, 2, 12, 12, {240, 80, 40, 128});
        ian::app_effect_drawing::endAdditiveEffectPass(shader);
        ian::app_effect_drawing::beginAdditiveEffectPass(shader, textureLocation,
            ian::app_effect_drawing::EffectSurface::Texture);
        DrawTexture(texture, 20, 2, WHITE);
        ian::app_effect_drawing::endAdditiveEffectPass(shader);
        DrawRectangle(40, 2, 12, 12, {80, 120, 160, 255});
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        const auto nearColor = [](Color actual, Color expected) {
            return std::abs(int(actual.r) - int(expected.r)) <= 2 &&
                   std::abs(int(actual.g) - int(expected.g)) <= 2 &&
                   std::abs(int(actual.b) - int(expected.b)) <= 2;
        };
        const Color geometry = GetImageColor(image, 6, 6);
        const Color flipbook = GetImageColor(image, 23, 5);
        valid = valid && nearColor(geometry, {140, 70, 60, 255}) &&
            nearColor(flipbook, {50, 130, 80, 255}) &&
            nearColor(GetImageColor(image, 44, 6), {80, 120, 160, 255});
        if (tagged) {
            valid = valid && std::abs(int(geometry.a) - 96) <= 1 &&
                            std::abs(int(flipbook.a) - 96) <= 1;
        }
        UnloadImage(image);
    }
    UnloadRenderTexture(target);
    UnloadTexture(texture);
    return valid;
}

bool checkProjectileDepth(ian::Renderer& renderer) {
    using namespace ian::app_projectile_drawing;
    const auto target = LoadRenderTexture(256, 128);
    const Camera3D camera{{0, 1, 8}, {0, 1, 0}, {0, 1, 0}, 4, CAMERA_ORTHOGRAPHIC};
    ian::WorldLighting lighting{};
    lighting.cameraPosition = camera.position;
    ian::WorldMaterialState material{};
    material.unlitAmount = 1;
    material.screenAoAmount = 0;
    material.inkOutlineEligible = false;
    material.inkOccluder = true;
    const auto begin = [&] {
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial(material);
    };
    const auto end = [&] {
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    const auto obstacle = [](float z) {
        // The same depth-writing surface used by grass, trees, enemies and
        // buildings, drawn late to catch missing depth writes on solid heads.
        DrawCube({0, 1, z}, 7, 3, .1F, GREEN);
        rlDrawRenderBatchActive();
    };
    const auto shot = [&](int kind, bool tail) {
        beginProjectileBatch(tail);
        if (kind < 2 || tail) {
            emitDefenseShotGeometry({.5F, 1, 0}, {1, 0, 0}, camera.position,
                                    3, 1, 1, {255, 80, 30, 255}, tail, kind == 1);
        } else {
            emitCannonballGeometry({0, 1, 0}, {1, 0, 0}, camera.position,
                                   .24F, {255, 80, 30, 255});
        }
        endProjectileBatch();
    };
    bool valid = true;
    for (int kind = 0; kind < 4; ++kind) {
        begin(); shot(kind, false); Image head = end();
        begin(); shot(kind, false); obstacle(-1); Image behind = end();
        int headPixels = 0;
        int overwritten = 0;
        for (int y = 0; y < head.height; ++y) {
            for (int x = 0; x < head.width; ++x) {
                const Color expected = GetImageColor(head, x, y);
                if (expected.r < 30) continue;
                ++headPixels;
                const Color actual = GetImageColor(behind, x, y);
                if (std::abs(int(expected.r) - int(actual.r)) +
                    std::abs(int(expected.g) - int(actual.g)) +
                    std::abs(int(expected.b) - int(actual.b)) > 6) ++overwritten;
            }
        }
        begin(); obstacle(1); Image cover = end();
        begin(); obstacle(1); shot(kind, false); Image hidden = end();
        begin(); shot(kind, false); obstacle(1); Image coverAfter = end();
        const bool solidValid = headPixels > 20 && overwritten == 0 &&
            changedPixels(cover, hidden) == 0 && changedPixels(cover, coverAfter) == 0;
        if (!solidValid) std::cerr << "projectile " << kind << " depth: "
            << headPixels << " body pixels, " << overwritten << " overwritten\n";
        valid = valid && solidValid;
        if (const char* directory = std::getenv("IAN_CAPTURE_PROJECTILE_DEPTH")) {
            std::filesystem::create_directories(directory);
            ExportImage(behind, (std::filesystem::path{directory} /
                ("head-" + std::to_string(kind) + "-foreground.png")).string().c_str());
        }
        UnloadImage(head); UnloadImage(behind); UnloadImage(cover);
        UnloadImage(hidden); UnloadImage(coverAfter);
    }
    // Ribbons respect opaque foreground depth, blend over the background,
    // and cannot leave invisible depth that cuts out later transparent effects.
    for (int kind = 0; kind < 2; ++kind) {
        begin(); obstacle(1); Image cover = end();
        begin(); obstacle(1); shot(kind, true); Image hidden = end();
        begin(); obstacle(-1); Image backdrop = end();
        begin(); obstacle(-1); shot(kind, true); Image visible = end();
        begin(); shot(kind, true); obstacle(-1); Image noDepth = end();
        const bool trailValid = changedPixels(cover, hidden) == 0 &&
            changedPixels(backdrop, visible) > 20 && changedPixels(backdrop, noDepth) == 0;
        if (!trailValid) std::cerr << "projectile ribbon depth failed: " << kind << '\n';
        valid = valid && trailValid;
        UnloadImage(cover); UnloadImage(hidden); UnloadImage(backdrop);
        UnloadImage(visible); UnloadImage(noDepth);
    }
    const bool particles = renderer.settings().particles;
    renderer.settings().particles = false;
    for (const auto element : {ian::WandElement::Ice, ian::WandElement::Fire}) {
        ian::IceWandProjectile projectile{};
        projectile.active = true;
        projectile.position = projectile.previousPosition = {2, 1, 0};
        projectile.radius = .25;
        projectile.element = element;
        projectile.trailCount = 3;
        projectile.trail[0] = projectile.position;
        projectile.trail[1] = {0, 1, 0};
        projectile.trail[2] = {-2, 1, 0};
        begin(); obstacle(-1); Image backdrop = end();
        begin();
        renderer.drawIceWandProjectile(projectile, camera.position, 0, 1);
        // Restore the world shader for the opaque object after the magic glow.
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial(material);
        obstacle(-1);
        Image noDepth = end();
        // x=128 samples the ribbon; x=203 the glow outside the solid sphere.
        for (const int x : {128, 203}) {
            const Color expected = GetImageColor(backdrop, x, 64);
            const Color actual = GetImageColor(noDepth, x, 64);
            const bool matches = std::abs(int(expected.r) - int(actual.r)) +
                std::abs(int(expected.g) - int(actual.g)) +
                std::abs(int(expected.b) - int(actual.b)) <= 6;
            if (!matches) std::cerr << "wand effect reserved depth at " << x << '\n';
            valid = valid && matches;
        }
        UnloadImage(backdrop); UnloadImage(noDepth);
    }
    renderer.settings().particles = particles;
    UnloadRenderTexture(target);
    return valid;
}

bool checkTacticalSectorGradient(ian::Renderer& renderer) {
    constexpr int Size = 384;
    constexpr Color Background{24, 61, 73, 255};
    const auto target = LoadRenderTexture(Size, Size);
    bool valid = true;
    for (const float radius : {3.0F, 10.0F, 24.0F}) {
        for (const float arc : {90.0F, 160.0F, 360.0F}) {
            const float inner = radius == 24.0F ? 3.0F : 0.0F;
            const Camera3D camera{{0, 20, 0}, {0, 0, 0}, {0, 0, -1},
                2.0F * (radius + 1), CAMERA_ORTHOGRAPHIC};
            BeginTextureMode(target); ClearBackground(Background); BeginMode3D(camera);
            renderer.drawTacticalGroundSector({0, .085F, 0}, radius, 0, arc,
                {255, 252, 244, 190}, inner);
            EndMode3D(); EndTextureMode();
            Image image = LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
            const auto sample = [&](float x, float z) {
                const Vector2 point = GetWorldToScreenEx({x, .085F, z}, camera, Size, Size);
                return GetImageColor(image, int(point.x), int(point.y));
            };
            // The middle stays clear: coarse vertex interpolation used to
            // smear white triangles/radial bands far beyond the 0.9 m rim.
            for (float distance = inner + 1.2F; distance < radius - 1.2F; distance += .4F) {
                for (const float angle : {-.15F, 0.0F, .15F}) {
                    const float sideDistance = distance * std::sin(arc * DEG2RAD * .5F - std::abs(angle));
                    if (arc < 360 && sideDistance < 1.1F) continue;
                    const Color middle = sample(distance * std::sin(angle), -distance * std::cos(angle));
                    valid = valid && std::abs(int(middle.r) - Background.r) <= 6;
                }
            }
            const Color rim = sample(0, -(radius - .15F));
            const Color interior = sample(0, -(radius - 1.2F));
            const Color outside = sample(0, -(radius + .5F));
            valid = valid && rim.r > interior.r + 35 && outside.r == Background.r;
            if (inner > 0) valid = valid && sample(0, -1).r == Background.r;
            if (arc < 360) valid = valid && sample(0, radius * .5F).r == Background.r;
            if (const char* directory = std::getenv("IAN_CAPTURE_ATTACK_VISUALS")) {
                std::filesystem::create_directories(directory);
                ExportImage(image, (std::filesystem::path{directory} /
                    ("sector-" + std::to_string(int(radius)) + "-" + std::to_string(int(arc)) + ".png")).string().c_str());
            }
            UnloadImage(image);
        }
    }
    UnloadRenderTexture(target);
    return valid;
}

bool checkProjectileGrassDepth(ian::Renderer& renderer) {
    using namespace ian::app_projectile_drawing;
    auto config = ian::WorldConfig::defaults();
    config.terrainWorldSize = 64;
    config.terrainResolution = 33;
    config.terrainAmplitude = 0;
    config.terrainBoundaryRiseHeight = 0;
    config.terrainSurfaceNoiseAmplitude = 0;
    config.pondMinimumCount = config.pondMaximumCount = 0;
    ian::TerrainHeightfield terrain{config};
    terrain.generate(42);
    renderer.rebuildTerrain(terrain);
    renderer.setWorldReveal({0, 0}, 1000);
    const auto target = LoadRenderTexture(256, 128);
    const Camera3D camera{{0, .3F, 20}, {0, .3F, 0}, {0, 1, 0}, 1.5F, CAMERA_ORTHOGRAPHIC};
    ian::WorldLighting lighting{};
    lighting.cameraPosition = camera.position;
    lighting.fogStart = 500;
    lighting.fogEnd = 700;
    const std::array<ian::GrassClearArea, 1> clear{{{{0, 20}, 3, 1}}};
    const auto capture = [&](bool grass, bool heads, float z) {
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        if (heads) {
            renderer.beginWorldShader(lighting);
            ian::WorldMaterialState material{};
            material.unlitAmount = 1;
            material.inkOutlineEligible = false;
            material.inkOccluder = true;
            renderer.setWorldMaterial(material);
            beginProjectileBatch(false);
            for (const float x : {-.9F, 0.F, .9F}) {
                emitDefenseShotGeometry({x + .4F, .3F, z}, {1, 0, 0}, camera.position,
                    3, 1, 1, {255, 80, 30, 255}, false);
            }
            endProjectileBatch();
            renderer.endWorldShader();
        }
        if (grass) renderer.drawGrassInstances(camera.position, 32, 0, lighting, clear);
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    Image heads = capture(false, true, 19);
    Image grass = capture(true, false, 19);
    Image front = capture(true, true, 19);
    Image behind = capture(true, true, -40);
    int overlap = 0;
    int foregroundSamples = 0;
    int failures = 0;
    const auto difference = [](Color a, Color b) {
        return std::abs(int(a.r) - int(b.r)) + std::abs(int(a.g) - int(b.g)) +
            std::abs(int(a.b) - int(b.b));
    };
    for (int y = 0; y < heads.height; ++y) {
        for (int x = 0; x < heads.width; ++x) {
            const Color head = GetImageColor(heads, x, y);
            const Color blade = GetImageColor(grass, x, y);
            if (head.r < 30 || blade.r + blade.g + blade.b < 30) continue;
            ++overlap;
            if (difference(head, GetImageColor(front, x, y)) > 6) ++failures;
            // Wind advances between captures. Test stable blade interiors for
            // projectile leakage, allowing their lighting/normal to animate.
            bool interior = x >= 2 && y >= 2 && x + 2 < grass.width && y + 2 < grass.height;
            for (int dy = -2; interior && dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (difference(blade, GetImageColor(grass, x + dx, y + dy)) > 12) {
                        interior = false;
                        break;
                    }
                }
            }
            if (interior && difference(blade, head) > 60) {
                ++foregroundSamples;
                if (difference(head, GetImageColor(behind, x, y)) <= 6) ++failures;
            }
        }
    }
    if (const char* directory = std::getenv("IAN_CAPTURE_PROJECTILE_DEPTH")) {
        ExportImage(front, (std::filesystem::path{directory} / "grass-behind-bullets.png").string().c_str());
        ExportImage(behind, (std::filesystem::path{directory} / "grass-in-front-of-bullets.png").string().c_str());
    }
    UnloadImage(heads); UnloadImage(grass); UnloadImage(front); UnloadImage(behind);
    UnloadRenderTexture(target);
    // Detach the borrowed terrain before leaving the fixture.
    renderer.shutdown();
    renderer.initialize();
    if (overlap <= 20 || foregroundSamples <= 20 || failures != 0) std::cerr << "grass/projectile depth: "
        << overlap << " overlapping pixels, " << failures << " wrong pixels\n";
    return overlap > 20 && foregroundSamples > 20 && failures == 0;
}

bool checkAtmosphereRendering() {
    ian::atmosphere::Field field;
    field.configure(842, 384);
    const auto biomes = ian::visual_biomes::generate(842, 384);
    const auto target = LoadRenderTexture(640, 320);
    const Camera3D camera{{0, 0, 4}, {0, 0, 0}, {0, 1, 0}, 2.4F, CAMERA_ORTHOGRAPHIC};
    BeginTextureMode(target);
    ClearBackground({20, 28, 30, 255});
    BeginMode3D(camera);
    rlBegin(RL_TRIANGLES);
    for (std::size_t biome = 0; biome < 4; ++biome) {
        ian::atmosphere::Particle p;
        p.position = {biomes.centers[biome][0], 0, biomes.centers[biome][1]};
        p.opacity = .85F; p.size = .95F;
        const Color color = field.leafColor(p);
        for (int face = 0; face < 2; ++face) {
            p.position = {-1.8F + static_cast<float>(biome) * 1.2F, .6F - static_cast<float>(face) * 1.2F, 0};
            p.yaw = static_cast<float>(face) * PI; p.tilt = .17F;
            for (const auto& vertex : ian::atmosphere::leafVertices(p, color)) {
                rlColor4ub(vertex.color.r, vertex.color.g, vertex.color.b, vertex.color.a);
                rlVertex3f(vertex.position.x, vertex.position.y, vertex.position.z);
            }
        }
    }
    rlEnd();
    EndMode3D();
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    Color* pixels = LoadImageColors(image);
    bool visible = true;
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 4; ++column) {
            int painted = 0;
            for (int y = row * 160; y < (row + 1) * 160; ++y) {
                for (int x = column * 160; x < (column + 1) * 160; ++x) {
                    const Color pixel = pixels[y * 640 + x];
                    if (pixel.r + pixel.g + pixel.b > 110) ++painted;
                }
            }
            visible = visible && painted > 600;
        }
    }
    if (const char* directory = std::getenv("IAN_CAPTURE_ATMOSPHERE")) {
        std::filesystem::create_directories(directory);
        ExportImage(image, (std::filesystem::path{directory} / "leaves.png").string().c_str());
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    UnloadRenderTexture(target);
    return visible;
}

bool checkSpikeTrapLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.2F, 1.25F, -1.6F}, {0, .10F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float seconds = -1.0F, Color tint = WHITE) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawSpikeTrap({0, 0, 0}, 0, seconds, tint, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_SPIKE_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("spike-level-" + std::to_string(level) + "-time-" + std::to_string(seconds) +
                 (tint.a == 255 ? "" : "-tinted") + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 1; level <= 8; ++level) {
        Image idle = capture(level);
        if (level > 1) {
            const auto difference = changedPixels(previous, idle);
            if (difference <= 80) std::cerr << "spike level " << level << " differs by only " << difference << " pixels\n";
            valid = valid && difference > 80;
        }
        Image raised = capture(level, .5F);
        Image halfway = capture(level, .1F);
        Image retracted = capture(level, 1.2F);
        valid = valid && changedPixels(idle, raised) > 80 &&
            changedPixels(idle, halfway) > 80 && changedPixels(raised, halfway) > 80 &&
            changedPixels(idle, retracted) == 0;
        UnloadImage(raised);
        UnloadImage(halfway);
        UnloadImage(retracted);
        UnloadImage(previous);
        previous = idle;
    }
    Image tinted = capture(8, -1, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restoredUpgrade = capture(8);
    valid = valid && changedPixels(previous, restoredUpgrade) == 0;
    UnloadImage(previous);
    UnloadImage(restoredUpgrade);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    // The trap occupies a full cell, including empty corners and gaps.
    // Aiming must stay stable across upgrades, rotations and spike animation.
    for (const std::uint8_t rotation : {std::uint8_t{0}, std::uint8_t{1},
                                      std::uint8_t{2}, std::uint8_t{3}}) {
        ian::BuildingInstance trap{.type = ian::BuildingType::SpikeTrap,
            .gridPosition = {4, -3}, .rotation = rotation, .baseHeight = 1.25};
        const auto center = ian::buildingWorldPosition(trap);
        const Vector3 position{static_cast<float>(center.x),
            static_cast<float>(center.y), static_cast<float>(center.z)};
        for (int level = 1; level <= 8; ++level) {
            trap.level = static_cast<std::uint8_t>(level);
            for (const float seconds : {-1.0F, .1F, .5F, 1.2F}) {
                for (const float x : {-.49F, 0.0F, .49F}) {
                    const Ray ray{Vector3Add(position, {x, .4F, -2}), {0, 0, 1}};
                    const auto hit = renderer.buildingRaycastDistance(
                        trap, {}, ray, 4, 0, 0, 1, seconds);
                    valid = valid && hit && std::abs(*hit - 1.5) < .0001 &&
                        !renderer.buildingRaycastDistance(trap, {}, ray, 1.49, 0, 0, 1, seconds);
                }
                for (const Vector3 offset : {Vector3{.51F, .4F, -2},
                                             Vector3{0, .51F, -2}, Vector3{0, -.01F, -2}}) {
                    valid = valid && !renderer.buildingRaycastDistance(
                        trap, {}, {Vector3Add(position, offset), {0, 0, 1}},
                        4, 0, 0, 1, seconds);
                }
                const auto top = renderer.buildingRaycastDistance(trap, {},
                    {Vector3Add(position, {.49F, 2, .49F}), {0, -1, 0}},
                    4, 0, 0, 1, seconds);
                valid = valid && top && std::abs(*top - 1.5) < .0001;
            }
        }
    }
    return valid;
}

bool checkCrystalMineLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.7F, 1.5F, 2.2F}, {0, .43F, 0}, {0, 1, 0},
                          36, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawResourceProducer(
            ian::BuildingType::CrystalMine, {0, 0, 0}, 0, WHITE, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_MINE_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("mine-level-" + std::to_string(level) + ".png")).string().c_str());
        }
        return image;
    };
    Image previous = capture(1);
    bool distinct = true;
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        distinct = distinct && changedPixels(previous, current) > 35;
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    UnloadRenderTexture(target);
    const float lowHeight = renderer.buildingVisualHeight(ian::BuildingType::CrystalMine, 1);
    const float highHeight = renderer.buildingVisualHeight(ian::BuildingType::CrystalMine, 8);
    bool correctAiming = highHeight > lowHeight + .25F;
    for (std::uint8_t rotation = 0; rotation < 4; ++rotation) {
        bool hitRaisedCrystals = false;
        for (int x = -20; x <= 20; ++x) {
            ian::BuildingInstance mine{.type = ian::BuildingType::CrystalMine,
                                      .rotation = rotation, .level = 1};
            const auto center = ian::buildingWorldPosition(mine);
            const Ray ray{{static_cast<float>(center.x) + static_cast<float>(x) * .02F,
                           .80F, static_cast<float>(center.z) + 2}, {0, 0, -1}};
            correctAiming = correctAiming && !renderer.buildingRaycastDistance(mine, {}, ray, 4);
            mine.level = 8;
            hitRaisedCrystals |= renderer.buildingRaycastDistance(mine, {}, ray, 4).has_value();
        }
        correctAiming = correctAiming && hitRaisedCrystals;
    }
    return distinct && correctAiming;
}

bool checkQuarryLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.5F, 1.5F, 2.4F}, {0, .47F, 0}, {0, 1, 0},
                          36, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawResourceProducer(
            ian::BuildingType::Quarry, {0, 0, 0}, 0, WHITE, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_QUARRY_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("quarry-level-" + std::to_string(level) + ".png")).string().c_str());
        }
        return image;
    };
    Image previous = capture(1);
    bool distinct = true;
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        distinct = distinct && changedPixels(previous, current) > 35;
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    UnloadRenderTexture(target);
    const float lowHeight = renderer.buildingVisualHeight(ian::BuildingType::Quarry, 1);
    const float highHeight = renderer.buildingVisualHeight(ian::BuildingType::Quarry, 8);
    bool correctAiming = highHeight > lowHeight + .20F;
    for (std::uint8_t rotation = 0; rotation < 4; ++rotation) {
        bool hitRaisedCrystals = false;
        for (int x = -20; x <= 20; ++x) {
            ian::BuildingInstance mine{.type = ian::BuildingType::Quarry,
                                      .rotation = rotation, .level = 1};
            const auto center = ian::buildingWorldPosition(mine);
            const Ray ray{{static_cast<float>(center.x) + static_cast<float>(x) * .02F,
                           .75F, static_cast<float>(center.z) + 2}, {0, 0, -1}};
            correctAiming = correctAiming && !renderer.buildingRaycastDistance(mine, {}, ray, 4);
            mine.level = 8;
            hitRaisedCrystals |= renderer.buildingRaycastDistance(mine, {}, ray, 4).has_value();
        }
        correctAiming = correctAiming && hitRaisedCrystals;
    }
    return distinct && correctAiming;
}

bool checkGateLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.6F, 1.0F, -1.9F}, {0, .26F, 0}, {0, 1, 0}, 28, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float open = 0.0F, Color tint = WHITE, bool neighbors = false) {
        const Camera3D view = neighbors
            ? Camera3D{{2, 1.6F, -3.4F}, {0, .26F, 0}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE}
            : camera;
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(view);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = view.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawGate({0, 0, 0}, 0, open, tint, 1, level));
        if (neighbors) {
            static_cast<void>(renderer.drawWall({-1, 0, 0}, 0, 0, WHITE, 1, level));
            static_cast<void>(renderer.drawWall({1, 0, 0}, 0, 0, WHITE, 1, level));
        }
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_GATE_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("gate-level-" + std::to_string(level) + "-open-" + std::to_string(open) +
                 (neighbors ? "-neighbors" : "") + (tint.a == 255 ? "" : "-tinted") + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "gate level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    for (const float open : {-1.0F, .5F, 1.0F}) {
        Image opened = capture(8, open);
        valid = valid && changedPixels(previous, opened) > 80;
        UnloadImage(opened);
    }
    Image connected = capture(8, 0, WHITE, true);
    UnloadImage(connected);
    Image tinted = capture(8, 0, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restoredUpgrade = capture(8);
    valid = valid && changedPixels(previous, restoredUpgrade) == 0;
    UnloadImage(previous);
    UnloadImage(restoredUpgrade);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    for (const std::uint8_t rotation : {std::uint8_t{0}, std::uint8_t{1}}) {
        ian::BuildingInstance gate{.type = ian::BuildingType::Gate, .rotation = rotation, .level = 1, .open = true};
        const auto center = ian::buildingWorldPosition(gate);
        const Matrix yaw = MatrixRotateY(static_cast<float>(rotation)*PI*.5F);
        for (const float open : {-1.0F, 0.0F, 1.0F}) {
            const Matrix pose = MatrixMultiply(ian::gate_model_detail::doorHingeTransform(open), yaw);
            Vector3 origin = Vector3Transform({-.39F, .535F, .4F}, pose);
            origin = Vector3Add(origin, {static_cast<float>(center.x), static_cast<float>(center.y) + .005F, static_cast<float>(center.z)});
            const Ray ray{origin, Vector3Transform({0, 0, -1}, MatrixMultiply(MatrixRotateY(ian::gate_model_detail::OpenAngle*open), yaw))};
            gate.level = 1;
            const auto plain = renderer.buildingRaycastDistance(gate, {}, ray, 1, 0, 0, open);
            gate.level = 7;
            const auto upgraded = renderer.buildingRaycastDistance(gate, {}, ray, 1, 0, 0, open);
            const bool hingeAligned = !plain && upgraded && std::abs(*upgraded - .315) < .002;
            if (!hingeAligned) std::cerr << "gate cap aiming, rotation " << static_cast<int>(rotation) << ", open " << open << ": " << plain.value_or(-1) << " -> " << upgraded.value_or(-1) << '\n';
            valid = valid && hingeAligned;
        }
    }
    return valid;
}

bool checkCatapultLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(448, 384);
    const Camera3D camera{{2.4F, 1.8F, 3.4F}, {0, .53F, .20F}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float pitch = 0.0F, bool loaded = false, Color tint = WHITE) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawCatapult({0, 0, 0}, 0, pitch, loaded, tint, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_CATAPULT_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("catapult-level-" + std::to_string(level) + (pitch == 0 ? "" : "-pitched") +
                 (loaded ? "-loaded" : "") + (tint.a == 255 ? "" : "-tinted") + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "catapult level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    Image pitched = capture(8, -.35F);
    valid = valid && changedPixels(previous, pitched) > 80;
    UnloadImage(pitched);
    Image loaded = capture(8, 0, true);
    valid = valid && changedPixels(previous, loaded) > 80;
    UnloadImage(loaded);
    Image tinted = capture(8, 0, false, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restoredUpgrade = capture(8);
    valid = valid && changedPixels(previous, restoredUpgrade) == 0;
    UnloadImage(previous);
    UnloadImage(restoredUpgrade);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    ian::BuildingInstance catapult{.type = ian::BuildingType::Catapult, .level = 1};
    const auto center = ian::buildingWorldPosition(catapult);
    for (const float pitch : {0.0F, -.35F}) {
        constexpr float Yaw = PI * .5F;
        const Matrix pose = MatrixMultiply(ian::catapult_model_detail::pitchTransform(pitch), MatrixRotateY(Yaw));
        Vector3 origin = Vector3Transform({0, .925F, 3}, pose);
        origin = Vector3Add(origin, {static_cast<float>(center.x), static_cast<float>(center.y), static_cast<float>(center.z)});
        const Ray ray{origin, Vector3Transform({0, 0, -1}, MatrixMultiply(MatrixRotateX(pitch), MatrixRotateY(Yaw)))};
        catapult.level = 1;
        const auto plain = renderer.buildingRaycastDistance(catapult, {}, ray, 8, Yaw, pitch);
        catapult.level = 7;
        const auto upgraded = renderer.buildingRaycastDistance(catapult, {}, ray, 8, Yaw, pitch);
        const bool bucketAligned = upgraded && (!plain || *upgraded < *plain - .02);
        if (!bucketAligned) std::cerr << "catapult raised rim aiming at pitch " << pitch << ": " << plain.value_or(-1) << " -> " << upgraded.value_or(-1) << '\n';
        valid = valid && bucketAligned;
    }
    return valid;
}

bool checkCrossbowLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(448, 384);
    const Camera3D camera{{2.4F, 1.8F, -3.2F}, {0, .52F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float pitch = 0.0F, Color tint = WHITE, bool rear = false) {
        const Camera3D view = rear
            ? Camera3D{{2.4F, 2.2F, 3.2F}, {0, .52F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE}
            : camera;
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(view);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = view.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawCrossbow({0, 0, 0}, 0, tint, 1, pitch, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_CROSSBOW_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("crossbow-level-" + std::to_string(level) + (pitch == 0 ? "" : "-pitched") +
                 (tint.a == 255 ? "" : "-tinted") + (rear ? "-rear" : "") + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "crossbow level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    Image pitched = capture(8, .30F);
    valid = valid && changedPixels(previous, pitched) > 80;
    UnloadImage(pitched);
    Image rear = capture(8, 0, WHITE, true);
    for (const float pitch : {-.3F, .3F}) {
        Image rearPitched = capture(8, pitch, WHITE, true);
        valid = valid && changedPixels(rear, rearPitched) > 80;
        UnloadImage(rearPitched);
    }
    UnloadImage(rear);
    Image tinted = capture(8, 0, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restoredUpgrade = capture(8);
    valid = valid && changedPixels(previous, restoredUpgrade) == 0;
    UnloadImage(previous);
    UnloadImage(restoredUpgrade);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    ian::BuildingInstance crossbow{.type = ian::BuildingType::Turret, .level = 1};
    const auto center = ian::buildingWorldPosition(crossbow);
    for (const float pitch : {0.0F, .30F}) {
        constexpr float Yaw = PI * .5F;
        const Matrix pose = MatrixMultiply(ian::crossbow_model_detail::pitchTransform(pitch), MatrixRotateY(Yaw));
        Vector3 origin = Vector3Transform({0, 1.030F, -3}, pose);
        origin = Vector3Add(origin, {static_cast<float>(center.x), static_cast<float>(center.y), static_cast<float>(center.z)});
        const Ray ray{origin, Vector3Transform({0, 0, 1}, MatrixMultiply(MatrixRotateX(pitch), MatrixRotateY(Yaw)))};
        crossbow.level = 1;
        const auto plain = renderer.buildingRaycastDistance(crossbow, {}, ray, 8, Yaw, pitch);
        crossbow.level = 7;
        const auto upgraded = renderer.buildingRaycastDistance(crossbow, {}, ray, 8, Yaw, pitch);
        const bool bowAligned = upgraded && (!plain || *upgraded < *plain - .02);
        if (!bowAligned) std::cerr << "crossbow raised bow aiming at pitch " << pitch << ": " << plain.value_or(-1) << " -> " << upgraded.value_or(-1) << '\n';
        valid = valid && bowAligned;
    }
    return valid;
}

bool checkCannonLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(448, 384);
    const Camera3D camera{{2.4F, 1.8F, -3.2F}, {0, .52F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, float pitch = 0.0F, Color tint = WHITE) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawCannon({0, 0, 0}, 0, pitch, tint, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_CANNON_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("cannon-level-" + std::to_string(level) + (pitch == 0 ? "" : "-pitched") +
                 (tint.a == 255 ? "" : "-tinted") +
                 ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "cannon level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    Image pitched = capture(8, .30F);
    valid = valid && changedPixels(previous, pitched) > 80;
    UnloadImage(pitched);
    UnloadImage(previous);
    Image tinted = capture(8, 0, {140, 190, 220, 180});
    UnloadImage(tinted);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    UnloadRenderTexture(target);

    // A raised, yawed muzzle collar must follow the pitched barrel for aiming.
    ian::BuildingInstance cannon{.type = ian::BuildingType::Cannon, .level = 1};
    const auto center = ian::buildingWorldPosition(cannon);
    constexpr float Pitch = .30F;
    constexpr float Yaw = PI * .5F;
    const Matrix pose = MatrixMultiply(ian::cannon_model_detail::pitchTransform(Pitch), MatrixRotateY(Yaw));
    Vector3 origin = Vector3Transform({0, .980118F, -3}, pose);
    origin = Vector3Add(origin, {static_cast<float>(center.x), static_cast<float>(center.y), static_cast<float>(center.z)});
    const Ray ray{origin, Vector3Transform({0, 0, 1}, MatrixMultiply(MatrixRotateX(Pitch), MatrixRotateY(Yaw)))};
    const auto plain = renderer.buildingRaycastDistance(cannon, {}, ray, 8, Yaw, Pitch);
    cannon.level = 6;
    const auto upgraded = renderer.buildingRaycastDistance(cannon, {}, ray, 8, Yaw, Pitch);
    const bool collarAligned = upgraded && (!plain || *upgraded < *plain - .02);
    if (!collarAligned) std::cerr << "cannon pitched collar aiming: " << plain.value_or(-1) << " -> " << upgraded.value_or(-1) << '\n';
    return valid && collarAligned;
}

bool checkWallLevelRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(512, 384);
    const Camera3D camera{{1.6F, 1.0F, -1.9F}, {0, .26F, 0}, {0, 1, 0}, 28, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level, std::uint8_t connections = 0U) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawWall({0, 0, 0}, connections, 0, WHITE, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_WALL_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("wall-level-" + std::to_string(level) + "-mask-" +
                 std::to_string(connections) + ".png")).string().c_str());
        }
        return image;
    };
    bool valid = true;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "wall level " << level << " differs by only " << difference << " pixels\n";
        valid = valid && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    Image restored = capture(1);
    valid = valid && changedPixels(original, restored) == 0;
    UnloadImage(original);
    UnloadImage(restored);
    // All junctions and their rotations must use the matching upgrade mesh.
    for (std::uint8_t mask = 1; mask < 16; ++mask) {
        Image plain = capture(1, mask);
        Image upgraded = capture(8, mask);
        valid = valid && changedPixels(plain, upgraded) > 80;
        UnloadImage(plain);
        UnloadImage(upgraded);
    }
    UnloadRenderTexture(target);
    ian::BuildingInstance wall{.type = ian::BuildingType::Wall, .level = 1};
    const ian::Vec3 center = ian::buildingWorldPosition(wall);
    const Ray capRay{{static_cast<float>(center.x) + .436F, .545F,
        static_cast<float>(center.z) - 2}, {0, 0, 1}};
    const auto plainCap = renderer.buildingRaycastDistance(wall, {}, capRay, 4);
    wall.level = 7;
    const auto upgradedCap = renderer.buildingRaycastDistance(wall, {}, capRay, 4);
    if (plainCap || !upgradedCap)
        std::cerr << "wall cap aiming: " << plainCap.value_or(-1) << " -> " << upgradedCap.value_or(-1) << '\n';
    return valid && !plainCap && upgradedCap;
}

bool checkGunTurretLevelRendering(ian::Renderer& renderer) {
    const bool correctHeights =
        renderer.buildingVisualHeight(ian::BuildingType::GunTurret, 1) < 1.10F &&
        renderer.buildingVisualHeight(ian::BuildingType::GunTurret, 7) > 1.15F &&
        renderer.buildingVisualHeight(ian::BuildingType::Core, 8) >
            renderer.buildingVisualHeight(ian::BuildingType::Core, 1) + 0.30F;
    const auto target = LoadRenderTexture(384, 384);
    const Camera3D camera{{2.4F, 1.9F, -3.2F}, {0, 0.62F, 0}, {0, 1, 0}, 30, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawGunTurret({0, 0, 0}, 0, 0, WHITE, 1, {}, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        if (const char* directory = std::getenv("IAN_CAPTURE_GUN_LEVELS")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("turret-level-" + std::to_string(level) + ".png")).string().c_str());
        }
        return image;
    };
    bool distinct = correctHeights;
    Image original = capture(1);
    Image previous = ImageCopy(original);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        const auto difference = changedPixels(previous, current);
        if (difference <= 80) std::cerr << "turret level " << level << " differs by only " << difference << " pixels\n";
        distinct = distinct && difference > 80;
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    Image recovered = capture(1);
    distinct = distinct && changedPixels(original, recovered) == 0;
    UnloadImage(original);
    UnloadImage(recovered);
    UnloadRenderTexture(target);

    ian::BuildingInstance turret{.type = ian::BuildingType::GunTurret, .level = 1};
    const ian::Vec3 center = ian::buildingWorldPosition(turret);
    const Ray topRay{{static_cast<float>(center.x), 1.16F,
        static_cast<float>(center.z) - 4}, {0, 0, 1}};
    const auto oldTop = renderer.buildingRaycastDistance(turret, {}, topRay, 8);
    turret.level = 7;
    const auto armoredTop = renderer.buildingRaycastDistance(turret, {}, topRay, 8);
    const Ray collarRay{{static_cast<float>(center.x) - 3, 0.805445F,
        static_cast<float>(center.z) + 0.140388F}, {1, 0, 0}};
    turret.level = 1;
    const auto oldCollar = renderer.buildingRaycastDistance(turret, {}, collarRay, 8, PI * 0.5F);
    turret.level = 6;
    const auto rotatedCollar = renderer.buildingRaycastDistance(turret, {}, collarRay, 8, PI * 0.5F);
    Ray hollowRay = collarRay;
    hollowRay.position.y = 0.685445F;
    turret.level = 1;
    const auto oldHollow = renderer.buildingRaycastDistance(turret, {}, hollowRay, 8, PI * 0.5F);
    turret.level = 6;
    const auto hollow = renderer.buildingRaycastDistance(turret, {}, hollowRay, 8, PI * 0.5F);
    // A hollow muzzle must not be selected as a solid box in front of the housing.
    const bool hollowPreserved = oldHollow && hollow && std::abs(*oldHollow - *hollow) < 0.001;
    if (oldTop || !armoredTop || !oldCollar || !rotatedCollar ||
        *rotatedCollar >= *oldCollar - 0.02 || !hollowPreserved)
        std::cerr << "turret aiming: top " << oldTop.value_or(-1) << " -> " << armoredTop.value_or(-1)
                  << ", collar " << oldCollar.value_or(-1) << " -> " << rotatedCollar.value_or(-1) << '\n';
    return distinct && !oldTop && armoredTop && oldCollar && rotatedCollar &&
        *rotatedCollar < *oldCollar - 0.02 && hollowPreserved;
}

bool checkCoreOrbitRendering(ian::Renderer& renderer) {
    const auto target = LoadRenderTexture(384, 384);
    const Camera3D camera{{0, 1.5F, 5}, {0, 1.0F, 0}, {0, 1, 0}, 36, CAMERA_PERSPECTIVE};
    const auto capture = [&](int level = 1) {
        BeginTextureMode(target);
        ClearBackground({26, 30, 40, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawCore({0, 0, 0}, 0, WHITE, 1, level));
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    Image before = capture();
    WaitTime(.35);
    Image after = capture();
    bool changed = changedPixels(before, after) > 200;
    if (const char* directory = std::getenv("IAN_CAPTURE_CORE_ORBIT")) {
        std::filesystem::create_directories(directory);
        ExportImage(before, (std::filesystem::path{directory} / "core-before.png").string().c_str());
        ExportImage(after, (std::filesystem::path{directory} / "core-after.png").string().c_str());
    }
    UnloadImage(before);
    UnloadImage(after);
    Image previous = capture(1);
    for (int level = 2; level <= 8; ++level) {
        Image current = capture(level);
        changed = changed && changedPixels(previous, current) > 120;
        if (const char* directory = std::getenv("IAN_CAPTURE_CORE_LEVELS")) {
            std::filesystem::create_directories(directory);
            if (level == 2) ExportImage(previous,
                (std::filesystem::path{directory} / "core-level-1.png").string().c_str());
            ExportImage(current, (std::filesystem::path{directory} /
                ("core-level-" + std::to_string(level) + ".png")).string().c_str());
        }
        UnloadImage(previous);
        previous = current;
    }
    UnloadImage(previous);
    UnloadRenderTexture(target);
    return changed;
}

bool checkSequentialMountainRendering(ian::Renderer& renderer) {
    ian::TerrainHeightfield terrain;
    terrain.generate(42);
    renderer.rebuildTerrain(terrain);
    renderer.setWorldReveal({0, 0}, 1000);
    const auto target = LoadRenderTexture(480, 270);
    const Camera3D camera{{0, static_cast<float>(terrain.getHeight(0, 0)) + 2, 0},
                          {0, 8, -100}, {0, 1, 0}, 65, CAMERA_PERSPECTIVE};
    const auto capture = [&](float lower, float upper) {
        BeginTextureMode(target);
        ClearBackground({95, 160, 215, 255});
        BeginMode3D(camera);
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.terrainAmount = 1;
        material.distantFadeAmount = 1;
        renderer.setWorldMaterial(material);
        renderer.drawTerrain({66, 112, 67, 255}, camera.position, false, upper, lower);
        material.terrainAmount = 0;
        material.treeAmount = 1;
        material.coniferAmount = 1;
        renderer.setWorldMaterial(material);
        renderer.drawBoundaryForest(camera);
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    Image flat = capture(0, 0);
    Image lower = capture(1, 0);
    Image both = capture(1, 1);
    const bool valid = changedPixels(flat, lower) > 1000 && changedPixels(lower, both) > 1000;
    if (const char* directory = std::getenv("IAN_CAPTURE_CORE_ORBIT")) {
        ExportImage(flat, (std::filesystem::path{directory} / "mountains-start.png").string().c_str());
        ExportImage(lower, (std::filesystem::path{directory} / "mountains-lower.png").string().c_str());
        ExportImage(both, (std::filesystem::path{directory} / "mountains-both.png").string().c_str());
    }
    UnloadImage(flat);
    UnloadImage(lower);
    UnloadImage(both);
    UnloadRenderTexture(target);
    // Renderer borrows this terrain; detach it before the local field expires.
    renderer.shutdown();
    renderer.initialize();
    return valid;
}

bool checkVisualBiomeRendering(ian::Renderer& renderer) {
    auto config = ian::WorldConfig::defaults();
    config.terrainResolution = 129;
    config.terrainAmplitude = 0;
    config.terrainBoundaryRiseHeight = 0;
    config.terrainSurfaceNoiseAmplitude = 0;
    config.pondMinimumCount = config.pondMaximumCount = 0;
    ian::TerrainHeightfield terrain{config};
    terrain.generate(42);
    renderer.rebuildTerrain(terrain);
    renderer.setWorldReveal({0, 0}, 1000);
    const auto biomes = ian::visual_biomes::generate(terrain.seed(), config.terrainWorldSize);
    const auto target = LoadRenderTexture(384, 288);
    std::array<Vector3, 4> groundColors{};
    std::array<Color, 4> propColors{};
    for (std::size_t region = 0; region < 4; ++region) {
        const float x = biomes.centers[region][0], z = biomes.centers[region][1];
        const Camera3D camera{{x, 4.5F, z + 12}, {x, 1, z - 1}, {0, 1, 0}, 55, CAMERA_PERSPECTIVE};
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        lighting.sunColor = {1, 1, 1};
        lighting.cloudShadowStrength = 0;
        lighting.fogStart = 500;
        lighting.fogEnd = 700;
        BeginTextureMode(target);
        ClearBackground({100, 170, 220, 255});
        BeginMode3D(camera);
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.terrainAmount = 1;
        renderer.setWorldMaterial(material);
        renderer.drawTerrain(WHITE, camera.position);
        material.terrainAmount = 0;
        material.treeAmount = material.vegetationAmount = 1;
        renderer.setWorldMaterial(material);
        static_cast<void>(renderer.drawTree({x - 2.8F, 0, z - 1}, WHITE, 1, 0));
        static_cast<void>(renderer.drawTree({x + 2.8F, 0, z - 2}, WHITE, 1, 3));
        renderer.setWorldMaterial({});
        static_cast<void>(renderer.drawRock({x, 0, z - 4}, WHITE, .8F));
        renderer.drawDecorativeRocks(camera, 192);
        renderer.drawGrassInstances(camera.position, 192, 0, lighting);
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image scene = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&scene);
        Vector3 average{};
        for (int py = 212; py < 274; ++py) {
            for (int px = 155; px < 229; ++px) {
                const auto color = GetImageColor(scene, px, py);
                average.x += color.r; average.y += color.g; average.z += color.b;
            }
        }
        groundColors[region] = Vector3Scale(average, 1.0F / (62 * 74));
        if (const char* directory = std::getenv("IAN_CAPTURE_BIOMES")) {
            std::filesystem::create_directories(directory);
            constexpr std::array names{"forest", "pink-grove", "autumn", "cold"};
            ExportImage(scene, (std::filesystem::path{directory} / (std::string{names[region]} + ".png")).string().c_str());
        }
        UnloadImage(scene);
        // The same ordinary prop must retain its authored color in every zone.
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        renderer.beginWorldShader(lighting);
        material = {};
        material.unlitAmount = 1;
        renderer.setWorldMaterial(material);
        DrawCube({x, 1, z - 1}, 3, 3, 3, {100, 130, 190, 255});
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image prop = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&prop);
        propColors[region] = GetImageColor(prop, 192, 144);
        UnloadImage(prop);
    }
    // A large crown straddling a transition must match the same tree wholly
    // inside the region selected by its trunk, in both rendering paths.
    Vector2 border{};
    std::size_t borderRegion = 0;
    bool foundBorder = false;
    for (int z = -150; z < 150 && !foundBorder; ++z) {
        for (int x = -150; x < 150 && !foundBorder; ++x) {
            const auto blend = ian::visual_biomes::weights(biomes, float(x), float(z));
            const auto region = static_cast<std::size_t>(
                std::max_element(blend.begin(), blend.end()) - blend.begin());
            if (blend[region] > .55F && blend[region] < .80F) {
                border = {float(x), float(z)};
                borderRegion = region;
                foundBorder = true;
            }
        }
    }
    const auto captureTree = [&](Vector2 position, bool instanced) {
        const Camera3D camera{{position.x, 8, position.y + 28},
            {position.x, 5, position.y}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE};
        ian::WorldLighting lighting{};
        lighting.cameraPosition = camera.position;
        lighting.sunColor = {1, 1, 1};
        lighting.cloudShadowStrength = 0;
        lighting.fogStart = 500;
        lighting.fogEnd = 700;
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        renderer.beginWorldShader(lighting);
        ian::WorldMaterialState material{};
        material.treeAmount = material.vegetationAmount = 1;
        renderer.setWorldMaterial(material);
        if (instanced) {
            const std::array instances{ian::TreeDrawInstance{
                .position = {position.x, 0, position.y}, .scale = 3}};
            static_cast<void>(renderer.drawTreesInstanced(instances));
        } else {
            static_cast<void>(renderer.drawTree({position.x, 0, position.y}, WHITE, 3, 0));
        }
        renderer.endWorldShader();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        return image;
    };
    bool discreteTrees = foundBorder;
    for (const bool instanced : {false, true}) {
        Image edgeTree = captureTree(border, instanced);
        Image centerTree = captureTree(
            {biomes.centers[borderRegion][0], biomes.centers[borderRegion][1]}, instanced);
        const auto changes = changedPixels(edgeTree, centerTree);
        discreteTrees = discreteTrees && changes < 50;
        if (changes >= 50) std::cerr << "boundary crown changes " << changes << " instanced " << instanced << '\n';
        if (const char* directory = std::getenv("IAN_CAPTURE_BIOMES"))
            ExportImage(edgeTree, (std::filesystem::path{directory} /
                (instanced ? "boundary-tree-instanced.png" : "boundary-tree.png")).string().c_str());
        UnloadImage(edgeTree);
        UnloadImage(centerTree);
    }
    // Probe the actual grass shader on a bright blade under daytime exposure:
    // blue vegetation must keep color in its highlights rather than turn white.
    ian::ShaderResource grassShader;
    bool blueGrass = grassShader.load("assets/shaders/grass_instanced.vs", "assets/shaders/grass_instanced.fs");
    if (blueGrass) {
        Shader& shader = grassShader.get();
        const auto uniform = [&](const char* name, const void* value, int type, int count = 1) {
            SetShaderValueV(shader, GetShaderLocation(shader, name), value, type, count);
        };
        uniform("worldBiomeCenters[0]", biomes.centers.data(), SHADER_UNIFORM_VEC2, 4);
        uniform("worldBiomeGround[0]", biomes.ground.data(), SHADER_UNIFORM_VEC3, 4);
        uniform("worldBiomeShape", biomes.shape.data(), SHADER_UNIFORM_VEC4);
        const Vector4 white{1, 1, 1, 1};
        const Vector3 daylight{1, 1, 1}, sun{-1, -1, -1};
        const float exposure = 1.01F, saturation = 1.03F, fogStart = 500, fogEnd = 700;
        uniform("grassTint", &white, SHADER_UNIFORM_VEC4);
        uniform("dayNightTint", &daylight, SHADER_UNIFORM_VEC3);
        uniform("sunDirection", &sun, SHADER_UNIFORM_VEC3);
        uniform("exposure", &exposure, SHADER_UNIFORM_FLOAT);
        uniform("saturation", &saturation, SHADER_UNIFORM_FLOAT);
        uniform("fogStart", &fogStart, SHADER_UNIFORM_FLOAT);
        uniform("fogEnd", &fogEnd, SHADER_UNIFORM_FLOAT);
        const auto cold = biomes.centers[3];
        const Camera3D camera{{cold[0], 1, cold[1] + 4}, {cold[0], 0, cold[1]},
            {0, 1, 0}, 40, CAMERA_PERSPECTIVE};
        uniform("cameraPosition", &camera.position, SHADER_UNIFORM_VEC3);
        Mesh blade = GenMeshCube(1, 1, .08F);
        Material material = LoadMaterialDefault();
        material.shader = shader;
        const Matrix transform = MatrixTranslate(cold[0], 0, cold[1]);
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        // The shader's alpha is an ink tag; the game writes RGB opaquely.
        rlDisableColorBlend();
        DrawMeshInstanced(blade, material, &transform, 1);
        rlEnableColorBlend();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        int coloredPixels = 0;
        for (int y = 0; y < image.height; ++y) {
            for (int x = 0; x < image.width; ++x) {
                const Color color = GetImageColor(image, x, y);
                if (color.b < 60) continue;
                ++coloredPixels;
                blueGrass = blueGrass && color.b > color.r + 50 && color.r < 150 && color.b < 235;
            }
        }
        blueGrass = blueGrass && coloredPixels > 500;
        if (const char* directory = std::getenv("IAN_CAPTURE_BIOMES"))
            ExportImage(image, (std::filesystem::path{directory} / "cold-grass.png").string().c_str());
        UnloadImage(image);
        UnloadMesh(blade);
        material.shader = {rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
        UnloadMaterial(material);
        grassShader.unload();
    }
    const bool distinct = groundColors[0].y > groundColors[0].x &&
        groundColors[1].x > groundColors[1].y + 12 &&
        groundColors[2].x > groundColors[2].z + 12 &&
        groundColors[3].z > groundColors[3].x + 8;
    bool unchangedProps = true;
    for (std::size_t i = 1; i < 4; ++i)
        unchangedProps = unchangedProps && propColors[i].r == propColors[0].r &&
            propColors[i].g == propColors[0].g && propColors[i].b == propColors[0].b;
    if (!distinct) {
        for (const auto color : groundColors) std::cerr << "biome RGB " << color.x << ' ' << color.y << ' ' << color.z << '\n';
    }
    UnloadRenderTexture(target);
    renderer.shutdown();
    renderer.initialize();
    return distinct && unchangedProps && discreteTrees && blueGrass;
}

bool checkCoreCrystalOrbit(ian::ModelResource& resource) {
    if (!resource.valid() || resource.get().meshCount != 7) return false;
    const Model& model = resource.get();
    for (int part = 0; part < model.meshCount; ++part) {
        const Mesh& mesh = model.meshes[part];
        const Matrix pose = ian::core_crystal_model_detail::meshTransform(part, 2.0);
        const bool satellite = part >= 1 && part <= 4;
        const BoundingBox bounds = GetMeshBoundingBox(mesh);
        const Vector3 center{(bounds.min.x + bounds.max.x) * .5F,
                             (bounds.min.y + bounds.max.y) * .5F,
                             (bounds.min.z + bounds.max.z) * .5F};
        if (satellite) {
            // Verify the loader baked the original node offsets: rotating an
            // unbaked mesh would only spin it about itself.
            const Vector3 orbitCenter = Vector3Transform(center, pose);
            if (std::abs(std::hypot(center.x, center.z) - .25F) > .001F ||
                std::abs(std::hypot(orbitCenter.x, orbitCenter.z) - .29F) > .001F ||
                Vector3Distance(center, orbitCenter) < .15F ||
                orbitCenter.y - center.y < .0349F || orbitCenter.y - center.y > .0651F) return false;
        }
        for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
            const Vector3 point{mesh.vertices[vertex * 3], mesh.vertices[vertex * 3 + 1],
                                mesh.vertices[vertex * 3 + 2]};
            const Vector3 animated = Vector3Transform(point, pose);
            if (!satellite && Vector3Distance(point, animated) > .00001F) return false;
            const Vector3 spread = Vector3Transform(point,
                ian::core_crystal_model_detail::meshTransform(part, 0));
            const float lift = Vector3Transform(center, pose).y - center.y;
            if (std::abs(animated.y - point.y - lift) > .00001F ||
                std::abs(std::hypot(spread.x, spread.z) - std::hypot(animated.x, animated.z)) > .00001F ||
                std::abs(Vector3Distance(point, center) -
                    Vector3Distance(animated, Vector3Transform(center, pose))) > .00001F)
                return false;
        }
    }
    const Vector3 first = Vector3Transform({}, ian::core_crystal_model_detail::meshTransform(1, 0));
    const Vector3 second = Vector3Transform({}, ian::core_crystal_model_detail::meshTransform(2, 0));
    if (std::abs(first.y - second.y) < .01F) return false;
    for (int part = 1; part <= 4; ++part) {
        const Vector3 start = Vector3Transform({}, ian::core_crystal_model_detail::meshTransform(part, 0));
        const Vector3 loop = Vector3Transform({}, ian::core_crystal_model_detail::meshTransform(part, 6));
        if (std::abs(start.y - loop.y) > .00001F) return false;
        for (int step = 0; step < 120; ++step) {
            const auto before = ian::core_crystal_model_detail::meshTransform(part, step / 20.0);
            const auto after = ian::core_crystal_model_detail::meshTransform(part, (step + 1) / 20.0);
            const float y = Vector3Transform({}, before).y;
            if (y < .0349F || y > .0651F ||
                std::abs(y - Vector3Transform({}, after).y) > .0009F) return false;
        }
    }
    // Bounds used by aiming and feedback must contain every upgraded part,
    // including rotating satellite rings, without changing the build footprint.
    for (int level = 1; level <= 8; ++level) {
        const BoundingBox bounds = ian::core_crystal_model_detail::levelBounds(resource.visualBounds(), level);
        for (int frame = 0; frame < 24; ++frame) {
            bool contained = true;
            ian::core_crystal_model_detail::forEachLevelPart(level, frame * 0.25,
                [&](int meshIndex, Matrix pose, Color, bool) {
                    const Mesh& mesh = model.meshes[meshIndex];
                    for (int vertex = 0; vertex < mesh.vertexCount; ++vertex) {
                        const Vector3 point = Vector3Transform({mesh.vertices[vertex * 3],
                            mesh.vertices[vertex * 3 + 1], mesh.vertices[vertex * 3 + 2]}, pose);
                        constexpr float tolerance = 0.0001F;
                        contained = contained && point.x >= bounds.min.x - tolerance &&
                            point.x <= bounds.max.x + tolerance && point.y >= bounds.min.y - tolerance &&
                            point.y <= bounds.max.y + tolerance && point.z >= bounds.min.z - tolerance &&
                            point.z <= bounds.max.z + tolerance;
                    }
                });
            if (!contained) return false;
        }
    }
    return true;
}

bool checkCrystalMineStaticShards(ian::ModelResource& resource) {
    if (!resource.valid()) return false;
    Model& model = resource.get();
    if (model.meshCount != ian::crystal_mine_model_detail::MeshCount ||
        model.meshes[0].triangleCount != 232 ||
        model.meshes[1].triangleCount != 204) return false;
    for (int level = 1; level <= 8; ++level) {
        int emitted = 0;
        bool expected = true;
        ian::crystal_mine_model_detail::forEachLevelPart(level,
            [&](int meshIndex, Matrix pose) {
                expected = expected && meshIndex < model.meshCount &&
                    model.meshes[meshIndex].triangleCount > 0 &&
                    (meshIndex == 1 || meshIndex == 10 || meshIndex == level + 1);
                const Vector3 posed = Vector3Transform({.23F, .45F, .12F}, pose);
                expected = expected && (meshIndex == 1
                    ? posed.y > .45F && posed.y < 1.0F
                    : meshIndex == 10 || Vector3Distance(posed, {.23F, .45F, .12F}) < .00001F);
                ++emitted;
            });
        if (!expected || emitted != 2 + (level >= 3) + (level >= 5) + (level >= 7)) return false;
    }
    return true;
}

bool checkMissingResources() {
    ian::ShaderResource shader;
    ian::TextureResource texture;
    ian::ModelResource model;
    ian::ModelAnimationsResource animations;
    ian::RenderTextureResource target;

    const bool rejected =
        !shader.load(nullptr, "assets/missing/not_found.fs") &&
        !texture.load("assets/missing/not_found.png") &&
        !model.load("assets/missing/not_found.glb") &&
        !animations.load("assets/missing/not_found.glb") &&
        !target.load(0, 64);
    shader.unload();
    shader.unload();
    texture.unload();
    texture.unload();
    model.unload();
    model.unload();
    animations.unload();
    animations.unload();
    target.unload();
    target.unload();
    return rejected;
}

// A world surface must replace the normal attachment even with alpha blending
// enabled. Encoding object IDs in its alpha previously leaked background ink.
bool checkOpaqueNormalAttachment(ian::GraphicsResources& resources) {
    Shader& shader = resources.worldShader().get();
    const float eligible = 1.0F;
    SetShaderValue(shader, GetShaderLocation(shader, "inkOutlineEligible"),
                   &eligible, SHADER_UNIFORM_FLOAT);
    const Camera3D camera{{0.0F, 0.0F, 4.0F}, {0.0F, 0.0F, 0.0F},
                          {0.0F, 1.0F, 0.0F}, 60.0F, CAMERA_PERSPECTIVE};
    std::array<Color, 2> samples{};
    const std::array<Color, 2> backgrounds{RED, BLUE};
    for (std::size_t i = 0; i < backgrounds.size(); ++i) {
        BeginTextureMode(resources.sceneTarget());
        rlActiveDrawBuffers(2);
        ClearBackground(backgrounds[i]);
        BeginMode3D(camera);
        BeginShaderMode(shader);
        DrawCube({0.0F, 0.0F, 0.0F}, 2.0F, 2.0F, 2.0F, WHITE);
        EndShaderMode();
        EndMode3D();
        EndTextureMode();
        Image image = LoadImageFromTexture(resources.sceneNormalTexture());
        samples[i] = GetImageColor(image, image.width/2, image.height/2);
        UnloadImage(image);
    }
    return samples[0].a == 255 && samples[1].a == 255 &&
        samples[0].r == samples[1].r && samples[0].g == samples[1].g &&
        samples[0].b == samples[1].b && samples[0].b >= 127;
}

bool checkPlatformVisibilityMasks() {
    const std::array<ian::PlatformFrameInstance, 2> frames{{
        {.id = ian::EntityId{1U, 0U},
         .anchor = {0, 0, 0}},
        {.id = ian::EntityId{2U, 0U},
         .anchor = {2, 0, 0}},
    }};
    const auto edges = ian::platformEdgeVisibilityMasks(frames);
    if (edges.size() != frames.size() ||
        !edges[0][0] || edges[0][1] ||
        edges[1][0] || !edges[1][1] ||
        !edges[0][2] || !edges[0][3] ||
        !edges[1][2] || !edges[1][3]) {
        return false;
    }

    auto sharedFrames = frames;
    sharedFrames[0].supportIds[1] = 7U;
    sharedFrames[1].supportIds[0] = 7U;
    sharedFrames[0].supports[1] = {
        .top = {2.0, 3.0, 0.0},
        .bottom = {2.0, 0.0, 0.0},
        .length = 3.0,
    };
    sharedFrames[1].supports[0] = {
        .top = {2.0, 4.0, 0.0},
        .bottom = {2.0, 1.0, 0.0},
        .length = 3.0,
    };
    const std::array<ian::SharedSupport, 1> supports{{
        {.id = 7U,
         .referenceCount = 2U,
         .active = true},
    }};
    const auto posts = ian::platformSupportVisibilityMasks(
        sharedFrames, supports);
    const auto lengths = ian::platformSupportVisualLengths(
        sharedFrames, supports);
    return posts.size() == sharedFrames.size() &&
        lengths.size() == sharedFrames.size() &&
        posts[0][1] && posts[1][0] &&
        std::abs(lengths[0][1] - 3.0) < 1e-9 &&
        std::abs(lengths[1][0] - 4.0) < 1e-9;
}

bool hasTexturedRenderableMesh(
    const ian::ModelResource& resource) {
    const Model& model = resource.get();
    if (model.materials == nullptr || model.meshMaterial == nullptr) {
        return false;
    }
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        if (!resource.meshValid(
                static_cast<std::size_t>(meshIndex))) {
            continue;
        }
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 ||
            materialIndex >= model.materialCount ||
            model.materials[materialIndex].maps == nullptr) {
            continue;
        }
        if (IsTextureValid(model.materials[materialIndex]
                               .maps[MATERIAL_MAP_DIFFUSE]
                               .texture)) {
            return true;
        }
    }
    return false;
}

bool animationMovesSkeleton(
    ian::ModelResource& resource,
    const ian::ModelAnimationsResource& animations,
    const char* clipName) {
    Model& model = resource.get();
    const ModelAnimation* animation = animations.find(clipName);
    if (animation == nullptr || animation->keyframeCount < 2 ||
        model.skeleton.boneCount <= 0 ||
        model.skeleton.boneCount > ian::MaximumGpuSkinningBones ||
        model.boneMatrices == nullptr ||
        !resource.gpuSkinningCompatible()) {
        return false;
    }

    UpdateModelAnimation(model, *animation, 0);
    const std::size_t valueCount =
        static_cast<std::size_t>(model.skeleton.boneCount) * 16U;
    std::vector<float> initial(valueCount);
    const float* values = &model.boneMatrices[0].m0;
    std::copy(values, values + valueCount, initial.begin());

    for (int frame = 1; frame < animation->keyframeCount; ++frame) {
        UpdateModelAnimation(
            model, *animation, static_cast<float>(frame));
        values = &model.boneMatrices[0].m0;
        for (std::size_t index = 0; index < valueCount; ++index) {
            if (std::abs(values[index] - initial[index]) > 0.0001F) {
                UpdateModelAnimation(model, *animation, 0);
                return true;
            }
        }
    }
    return false;
}

bool pickaxePivotAnimationMoves(
    const ian::GlbNodeAnimation& animation) {
    if (!animation.valid()) return false;
    const Transform& base = animation.baseTransform();
    const Transform middle = animation.sample(0.5F);
    const float translationDistance = std::sqrt(
        (base.translation.x - middle.translation.x) *
            (base.translation.x - middle.translation.x) +
        (base.translation.y - middle.translation.y) *
            (base.translation.y - middle.translation.y) +
        (base.translation.z - middle.translation.z) *
            (base.translation.z - middle.translation.z));
    const float rotationDot =
        base.rotation.x * middle.rotation.x +
        base.rotation.y * middle.rotation.y +
        base.rotation.z * middle.rotation.z +
        base.rotation.w * middle.rotation.w;
    return translationDistance > 0.01F ||
        std::abs(rotationDot) < 0.999F;
}

} // namespace

int main() {
    std::cerr << "graphics smoke: create context\n";
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    const bool captureOutlines = std::getenv("IAN_CAPTURE_OUTLINES") != nullptr;
    InitWindow(captureOutlines ? 960 : 64,
               captureOutlines ? 640 : 64,
               "IAN graphics resource smoke");
    if (!IsWindowReady()) {
        std::cerr << "graphics context unavailable; skipping\n";
        return 77;
    }

    int result = 0;
    {
        if (!checkPlatformVisibilityMasks()) {
            std::cerr << "platform visibility masks failed\n";
            result = 1;
        }
        std::cerr << "graphics smoke: missing resources\n";
        if (!checkMissingResources()) {
            std::cerr << "missing resource fallback failed\n";
            result = 1;
        }

        ian::GraphicsSettings settings;
        settings.shadowMapSize = 64;
        settings.pixelSize = 2;
        ian::GraphicsResources resources;
        std::cerr << "graphics smoke: initialize 1\n";
        resources.initialize(settings);
        if (!checkCoreCrystalOrbit(resources.coreModel())) {
            std::cerr << "core satellites fail to orbit the fixed central crystal/pedestal\n";
            result = 1;
        }
        if (!checkCrystalMineStaticShards(resources.mineModel())) {
            std::cerr << "crystal mine static shards or level meshes failed\n";
            result = 1;
        }
        if (!resources.sceneTargetValid() ||
            !resources.sceneScreenSpaceBuffers() ||
            !IsTextureValid(resources.sceneTarget().depth) ||
            !IsTextureValid(resources.sceneNormalTexture()) ||
            !resources.ssaoTargetValid() ||
            !resources.ssaoShader().valid() ||
            !resources.selectionMaskValid() ||
            !resources.viewModelTargetValid() ||
            !resources.viewModelCompositeShader().valid() ||
            !resources.enemyMinionModel().valid() ||
            !resources.enemyRogueModel().valid() ||
            !resources.enemyWarriorModel().valid() ||
            !resources.enemySplitterModel().valid() ||
            !resources.enemySplitlingModel().valid() ||
            !resources.spikeTrapModel().valid() ||
            !resources.grassModelB().valid() ||
            !resources.grassModelC().valid() ||
            !resources.grassModelD().valid() ||
            !resources.gateModel().valid() ||
            resources.enemyPinkBlobAnimations().find("Idle") == nullptr ||
            resources.enemyPinkBlobAnimations().find("Walk") == nullptr ||
            resources.enemyPinkBlobAnimations().find("Death") == nullptr ||
            resources.enemyNinjaAnimations().find("Idle") == nullptr ||
            resources.enemyNinjaAnimations().find("Walk") == nullptr ||
            resources.enemyNinjaAnimations().find("Bite_Front") == nullptr ||
            resources.enemyNinjaAnimations().find("HitRecieve") == nullptr ||
            resources.enemyNinjaAnimations().find("Death") == nullptr ||
            resources.enemyMushroomKingAnimations().find("Idle") == nullptr ||
            resources.enemyMushroomKingAnimations().find("Walk") == nullptr ||
            resources.enemyMushroomKingAnimations().find("Bite_Front") == nullptr ||
            resources.enemyMushroomKingAnimations().find("HitRecieve") == nullptr ||
            resources.enemyMushroomKingAnimations().find("Death") == nullptr ||
            resources.enemySplitterAnimations().find("Idle") == nullptr ||
            resources.enemySplitterAnimations().find("Walk") == nullptr ||
            resources.enemySplitterAnimations().find("Bite_Front") == nullptr ||
            resources.enemySplitterAnimations().find("Death") == nullptr ||
            resources.enemySplitlingAnimations().find("Idle") == nullptr ||
            resources.enemySplitlingAnimations().find("Walk") == nullptr ||
            resources.enemySplitlingAnimations().find("Bite_Front") == nullptr ||
            resources.enemySplitlingAnimations().find("Death") == nullptr ||
            !animationMovesSkeleton(
                resources.enemyRogueModel(),
                resources.enemyNinjaAnimations(), "Walk") ||
            !animationMovesSkeleton(
                resources.enemyWarriorModel(),
                resources.enemyMushroomKingAnimations(), "Walk") ||
            !animationMovesSkeleton(
                resources.enemySplitterModel(),
                resources.enemySplitterAnimations(), "Walk") ||
            !animationMovesSkeleton(
                resources.enemySplitlingModel(),
                resources.enemySplitlingAnimations(), "Walk") ||
            !resources.pickaxeModel().valid() ||
            !pickaxePivotAnimationMoves(resources.pickaxeAnimation()) ||
            !pickaxePivotAnimationMoves(
                resources.pickaxeAlternateAnimation()) ||
            !resources.clubModel().valid() ||
            !resources.hammerModel().valid() ||
            !resources.iceWandModel().valid() ||
            !resources.iceMagicShader().valid() ||
            !resources.coinOutlineShader().valid() ||
            !resources.heartOutlineShader().valid() ||
            !resources.portalMatterTexture().valid() ||
            !resources.portalMatterShader().valid() ||
            !resources.enemyPortalModel().valid() ||
            !resources.skyboxTexture(0).valid() ||
            !resources.skyboxTexture(1).valid() ||
            !resources.skyboxTexture(2).valid() ||
            !resources.woodenChestModel().valid() ||
            !resources.stoneChestModel().valid() ||
            !resources.coinModel(0).valid() ||
            !resources.coinModel(1).valid() ||
            !resources.coinModel(2).valid() ||
            !resources.destructiblePropModel(0).valid() ||
            !resources.destructiblePropModel(1).valid() ||
            !resources.destructiblePropModel(2).valid() ||
            !resources.decorativeBushModel(6).valid() ||
            !resources.decorativeBushModel(7).valid() ||
            !resources.decorativeBushModel(8).valid() ||
            !resources.appleLootModel().valid() ||
            !resources.breadLootModel().valid() ||
            !resources.ironBarLootModel().valid() ||
            !resources.fuelJerrycanLootModel().valid() ||
            !resources.compassLootModel().valid() ||
            !resources.nailLootModel().valid() ||
            !resources.keyLootModel().valid() ||
            !resources.mapLootModel().valid() ||
            !resources.anvilLootModel().valid() ||
            !resources.sawLootModel().valid() ||
            !resources.potionLootModel().valid() ||
            !resources.blueprintLootModel().valid() ||
            !resources.hourglassLootModel().valid() ||
            !resources.ropeLootModel().valid() ||
            !resources.heartLootModel().valid()) {
            std::cerr << "required graphics resource failed to load\n";
            result = 1;
        }
        if (!hasTexturedRenderableMesh(resources.appleLootModel()) ||
            !hasTexturedRenderableMesh(resources.enemyPortalModel()) ||
            !hasTexturedRenderableMesh(resources.breadLootModel()) ||
            !hasTexturedRenderableMesh(resources.potionLootModel()) ||
            !hasTexturedRenderableMesh(resources.blueprintLootModel()) ||
            !hasTexturedRenderableMesh(resources.hourglassLootModel()) ||
            !hasTexturedRenderableMesh(resources.ropeLootModel()) ||
            !hasTexturedRenderableMesh(resources.heartLootModel()) ||
            !hasTexturedRenderableMesh(resources.coinModel(0)) ||
            !hasTexturedRenderableMesh(resources.coinModel(1)) ||
            !hasTexturedRenderableMesh(resources.coinModel(2)) ||
            !hasTexturedRenderableMesh(
                resources.destructiblePropModel(0)) ||
            !hasTexturedRenderableMesh(
                resources.destructiblePropModel(1)) ||
            !hasTexturedRenderableMesh(
                resources.destructiblePropModel(2)) ||
            !hasTexturedRenderableMesh(
                resources.decorativeBushModel(6)) ||
            !hasTexturedRenderableMesh(
                resources.decorativeBushModel(7)) ||
            !hasTexturedRenderableMesh(
                resources.decorativeBushModel(8)) ||
            !hasTexturedRenderableMesh(resources.gateModel()) ||
            !hasTexturedRenderableMesh(resources.enemyRogueModel()) ||
            !hasTexturedRenderableMesh(resources.enemyWarriorModel())) {
            std::cerr << "item model has no valid albedo texture\n";
            result = 1;
        }
        settings.ssao = false;
        resources.updateFramebuffer(settings);
        if (!resources.sceneTargetValid() ||
            resources.sceneScreenSpaceBuffers() ||
            resources.ssaoTargetValid()) {
            std::cerr << "SSAO disable transition failed\n";
            result = 1;
        }
        settings.postProcessing = false;
        resources.updateFramebuffer(settings);
        if (resources.sceneTargetValid() ||
            resources.ssaoTargetValid()) {
            std::cerr << "post-process disable transition failed\n";
            result = 1;
        }
        settings.pixelSize = 1;
        settings.renderWidth = 1280;
        settings.renderHeight = 720;
        resources.updateFramebuffer(settings);
        const int expectedScaledWidth = static_cast<int>(std::lround(
            static_cast<float>(GetScreenWidth()) * (2.0F / 3.0F)));
        const int expectedScaledHeight = static_cast<int>(std::lround(
            static_cast<float>(GetScreenHeight()) * (2.0F / 3.0F)));
        if (!resources.sceneTargetValid() ||
            resources.sceneWidth() != expectedScaledWidth ||
            resources.sceneHeight() != expectedScaledHeight) {
            std::cerr << "render-resolution scale transition failed\n";
            result = 1;
        }
        settings.renderWidth = 1920;
        settings.renderHeight = 1080;
        resources.updateFramebuffer(settings);
        if (resources.sceneTargetValid()) {
            std::cerr << "native render-resolution transition failed\n";
            result = 1;
        }
        settings.postProcessing = true;
        settings.ssao = true;
        resources.updateFramebuffer(settings);
        if (!resources.sceneTargetValid() ||
            !resources.sceneScreenSpaceBuffers() ||
            !IsTextureValid(resources.sceneTarget().depth) ||
            !IsTextureValid(resources.sceneNormalTexture()) ||
            !resources.ssaoTargetValid()) {
            std::cerr << "SSAO enable transition failed\n";
            result = 1;
        }
        if (!checkOpaqueNormalAttachment(resources)) {
            std::cerr << "normal attachment leaks background through opaque geometry\n";
            result = 1;
        }
        resources.shutdown();
        resources.shutdown();
        std::cerr << "graphics smoke: initialize 2\n";
        resources.initialize(settings);
        resources.shutdown();

        ian::Renderer renderer;
        renderer.initialize();
        if (!checkCartoonCoins(renderer)) {
            std::cerr << "cartoon coin palette, relief, rotation or depth failed\n";
            return 1;
        }
        if (!checkExperienceCrystals(renderer)) {
            std::cerr << "experience crystal palette, ink or depth checks failed\n";
            return 1;
        }
        if (!checkFireSelection(renderer)) {
            std::cerr << "fire foreground selection checks failed\n";
            return 1;
        }
        if (!checkFireWandAttack(renderer)) {
            std::cerr << "cartoon fire wand checks failed\n";
            return 1;
        }
        if (!checkCartoonPoison(renderer)) {
            std::cerr << "cartoon poison presentation failed\n";
            result = 1;
        }
        if (!checkCartoonFreeze(renderer)) {
            std::cerr << "cartoon freeze checks failed\n";
            return 1;
        }
        if (!checkCartoonBurn(renderer)) {
            std::cerr << "cartoon burn palette, animation, fade, LOD or occlusion failed\n";
            result = 1;
        }
        if (!checkTacticalSectorGradient(renderer)) {
            std::cerr << "tactical sector rim/interior gradient is uneven\n";
            result = 1;
        }
        if (!checkProjectileDepth(renderer)) {
            std::cerr << "projectile heads, ribbons or glow depth occlusion failed\n";
            result = 1;
        }
        if (!checkProjectileGrassDepth(renderer)) {
            std::cerr << "instanced grass/projectile depth occlusion failed\n";
            result = 1;
        }
        if (std::getenv("IAN_PROFILE_FIRST_SHOT")) {
            const auto target = LoadRenderTexture(64, 64);
            const Camera3D camera{{0, 0, 5}, {0, 0, 0}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE};
            for (int frame = 0; frame < 8; ++frame) {
                const double start = GetTime();
                BeginTextureMode(target);
                ClearBackground(BLACK);
                BeginMode3D(camera);
                renderer.beginWorldShader({});
                ian::WorldMaterialState material{};
                material.unlitAmount = 1;
                material.screenAoAmount = 0;
                material.inkOutlineEligible = false;
                renderer.setWorldMaterial(material);
                rlDisableDepthMask();
                rlDisableBackfaceCulling();
                BeginBlendMode(BLEND_ALPHA);
                DrawTriangle3D({-1, -1, 0}, {1, -1, 0}, {0, 1, 0}, {255, 180, 30, 180});
                rlDrawRenderBatchActive();
                EndBlendMode();
                BeginBlendMode(BLEND_ADDITIVE);
                DrawTriangle3D({-1, -1, 0}, {1, -1, 0}, {0, 1, 0}, {255, 180, 30, 180});
                rlDrawRenderBatchActive();
                EndBlendMode();
                rlEnableDepthMask();
                rlEnableBackfaceCulling();
                renderer.endWorldShader();
                EndMode3D();
                EndTextureMode();
                Image image = LoadImageFromTexture(target.texture);
                UnloadImage(image);
                std::cerr << "first-shot render " << frame << " ms " << (GetTime() - start) * 1000 << '\n';
            }
            UnloadRenderTexture(target);
        }
        if (!checkAdditiveEffectPass()) {
            std::cerr << "additive effects lose coverage, foreground tags or render-state restoration\n";
            result = 1;
        }
        if (!checkAtmosphereRendering()) {
            std::cerr << "biome leaves disappear when viewed from either side\n";
            result = 1;
        }
        if (!checkCoreOrbitRendering(renderer)) {
            std::cerr << "core orbit or distinct level appearances failed\n";
            result = 1;
        }
        if (!checkWallLevelRendering(renderer)) {
            std::cerr << "wall levels, junctions, cap aiming or material restoration failed\n";
            result = 1;
        }
        if (!checkGunTurretLevelRendering(renderer)) {
            std::cerr << "gun turret levels, material restoration or rotated armor aiming failed\n";
            result = 1;
        }
        if (!checkCannonLevelRendering(renderer)) {
            std::cerr << "cannon levels, pitched collar aiming or material restoration failed\n";
            result = 1;
        }
        if (!checkCrossbowLevelRendering(renderer)) {
            std::cerr << "crossbow levels, pitched bow aiming or material restoration failed\n";
            result = 1;
        }
        if (!checkCatapultLevelRendering(renderer)) {
            std::cerr << "catapult level rendering check failed\n";
            result = 1;
        }
        if (!checkSpikeTrapLevelRendering(renderer)) {
            std::cerr << "spike trap level rendering failed\n";
            result = 1;
        }
        if (!checkCrystalMineLevelRendering(renderer)) {
            std::cerr << "crystal mine levels fail to render distinctly\n";
            result = 1;
        }
        if (!checkQuarryLevelRendering(renderer)) {
            std::cerr << "quarry levels, visual height or raised stone aiming failed\n";
            result = 1;
        }
        if (!checkGateLevelRendering(renderer)) {
            std::cerr << "gate levels, moving reinforcement aiming or tint restoration failed\n";
            result = 1;
        }
        if (!checkSequentialMountainRendering(renderer)) {
            std::cerr << "lower/upper mountain rendering stages fail\n";
            result = 1;
        }
        if (!checkVisualBiomeRendering(renderer)) {
            std::cerr << "biome palettes fail to recolor the ground or leak onto ordinary props\n";
            result = 1;
        }
        const ian::BuildingInstance core{
            .id = {1U, 0U},
            .type = ian::BuildingType::Core,
            .gridPosition = {0, 0},
        };
        const std::array coreBuildings{core};
        const auto coreCenterHit = renderer.buildingRaycastDistance(
            core, coreBuildings,
            {{0.0F, 1.0F, 3.0F}, {0.0F, 0.0F, -1.0F}},
            5.0);
        const auto coreEmptySpaceHit = renderer.buildingRaycastDistance(
            core, coreBuildings,
            {{0.0F, 2.0F, 3.0F}, {0.0F, 0.0F, -1.0F}},
            5.0);
        if (!coreCenterHit || coreEmptySpaceHit) {
            std::cerr << "core aim extends beyond visible model\n";
            result = 1;
        }
        const bool rampSurfaceRaycast =
            renderer.rampRaycastDistance(
                {0.0F, 0.0F, 0.0F}, 0.0F, 1.0F,
                {{0.0F, 5.0F, 0.0F}, {0.0F, -1.0F, 0.0F}},
                10.0).has_value();
        const bool rampEmptyVolumeRejected =
            !renderer.rampRaycastDistance(
                {0.0F, 0.0F, 0.0F}, 0.0F, 1.0F,
                {{-3.0F, 0.5F, 0.0F}, {1.0F, 0.0F, 0.0F}},
                6.0).has_value();
        const Camera3D smokeCamera{
            .position = {0.0F, 1.5F, 4.0F},
            .target = {0.0F, 1.0F, 0.0F},
            .up = {0.0F, 1.0F, 0.0F},
            .fovy = 60.0F,
            .projection = CAMERA_PERSPECTIVE,
        };
        renderer.beginFrame();
        renderer.beginWorldPass(BLACK, smokeCamera);
        renderer.drawSky(ian::SkyState{});
        BeginMode3D(smokeCamera);
        ian::WorldLighting smokeLighting{};
        smokeLighting.cameraPosition = smokeCamera.position;
        renderer.beginWorldShader(smokeLighting);
        ian::WorldMaterialState treeMaterial{};
        treeMaterial.treeAmount = 1.0F;
        treeMaterial.vegetationAmount = 1.0F;
        renderer.setWorldMaterial(treeMaterial);
        const std::array<ian::TreeDrawInstance, 2> smokeTrees{{
            {.position = {-0.8F, 0.0F, 0.0F},
             .yawRadians = 0.2F,
             .scale = 0.6F,
             .visualVariant = 0U},
            {.position = {0.8F, 0.0F, 0.0F},
             .yawRadians = -0.2F,
             .scale = 0.6F,
             .visualVariant = 0U},
        }};
        const bool instancedTreesDrawn =
            renderer.drawTreesInstanced(smokeTrees);
        renderer.endWorldShader();
        if (captureOutlines) {
            // Reproduce a transparent world particle crossing outlined
            // geometry. It changes scene color/alpha but intentionally does
            // not replace the normal attachment behind it.
            rlDrawRenderBatchActive();
            rlSetBlendFactorsSeparate(
                RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
                RL_ZERO, RL_ZERO,
                RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            rlDisableDepthMask();
            DrawSphereEx(
                {0.32F, 2.22F, 1.05F}, 0.22F, 6, 6,
                {255, 182, 55, 150});
            rlDrawRenderBatchActive();
            rlEnableDepthMask();
            EndBlendMode();
        }
        EndMode3D();
        renderer.endWorldPass();
        if (captureOutlines) {
            rlDrawRenderBatchActive();
            Image frame = LoadImageFromScreen();
            ExportImage(frame, "/tmp/ian-outline-regression.png");
            UnloadImage(frame);
        }
        EndDrawing();
        BeginDrawing();
        ClearBackground(BLACK);
        const Camera3D camera{
            .position = {0.0F, 2.0F, 4.0F},
            .target = {0.0F, 0.5F, 0.0F},
            .up = {0.0F, 1.0F, 0.0F},
            .fovy = 60.0F,
            .projection = CAMERA_PERSPECTIVE,
        };
        BeginMode3D(camera);
        const bool woodenDrawn = renderer.drawLootChest(
            ian::LootChestType::Wooden, {-1.0F, 0.0F, 0.0F},
            0.0F, 0.72F);
        const bool stoneDrawn = renderer.drawLootChest(
            ian::LootChestType::Stone, {1.0F, 0.0F, 0.0F},
            0.0F, 0.72F);
        renderer.drawLootItem(
            {-0.45F, 1.2F, 0.0F},
            ian::LootUpgradeEffect::Apple,
            ian::LootRarity::Common, 0.5F);
        renderer.drawLootItem(
            {0.45F, 1.2F, 0.0F},
            ian::LootUpgradeEffect::Bread,
            ian::LootRarity::Common, -0.5F);
        renderer.drawLootItem(
            {-0.45F, 1.7F, 0.0F},
            ian::LootUpgradeEffect::Magnet,
            ian::LootRarity::Common, 0.2F);
        renderer.drawLootItem(
            {0.45F, 1.7F, 0.0F},
            ian::LootUpgradeEffect::HealthAid,
            ian::LootRarity::Common, -0.2F);
        renderer.drawCoin(
            ian::CoinType::HighValue, {0.0F, 0.55F, 0.0F}, 0.7F, 1.0F);
        renderer.drawHeart({0.55F, 0.55F, 0.0F}, 0.7F, 1.0F);
        const bool barrelDrawn = renderer.drawDestructibleProp(
            ian::ResourceType::Barrel, {-1.3F, 0.0F, -1.0F}, 0.2F);
        const bool crateDrawn = renderer.drawDestructibleProp(
            ian::ResourceType::Crate, {0.0F, 0.0F, -1.0F}, -0.2F);
        const bool itemCrateDrawn = renderer.drawDestructibleProp(
            ian::ResourceType::ItemCrate, {1.3F, 0.0F, -1.0F}, 0.0F);
        const bool portalDrawn = renderer.drawEnemyPortal(
            {0.0F, 0.0F, -2.0F}, 0.0F, 0.4F,
            smokeLighting, 0.35F);
        EndMode3D();
        EndDrawing();
        renderer.shutdown();
        if (!rampSurfaceRaycast || !rampEmptyVolumeRejected ||
            !instancedTreesDrawn || !woodenDrawn || !stoneDrawn ||
            !barrelDrawn ||
            !crateDrawn || !itemCrateDrawn || !portalDrawn) {
            std::cerr << "world prop renderer failed\n";
            result = 1;
        }
    }
    std::cerr << "graphics smoke: close context\n";
    CloseWindow();
    return result;
}
        if (!checkEnemyPresentation(renderer)) {
            std::cerr << "cartoon enemy presentation failed\n";
            result = 1;
        }
        if (!checkElitePresentation(renderer)) {
            std::cerr << "elite geometry, animation, batch parity or depth failed\n";
            result = 1;
        }
        if (!checkCartoonHearts(renderer)) {
            std::cerr << "cartoon heart silhouette, highlights or depth failed\n";
            return 1;
        }
