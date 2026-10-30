#pragma once

#include "assets/GlbCollision.hpp"
#include "app/AppRenderSupport.hpp"
#include "graphics/GraphicsResources.hpp"
#include "graphics/Renderer.hpp"
#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudHotbar.hpp"
#include "ui/UiText.hpp"
#include "ui/UiViewport.hpp"
#include "graphics/FlamethrowerEffect.hpp"
#include "graphics/FlamethrowerModel.hpp"

#include <cstdlib>
#include <filesystem>

inline bool checkFlamethrowerHoverRange(ian::Renderer& renderer) {
    using namespace ian;
    const auto target = LoadRenderTexture(960, 600);
    const Camera3D camera{{0, 10, 4}, {0, 0, -2}, {0, 1, 0}, 9, CAMERA_ORTHOGRAPHIC};
    const BuildingInstance building{.id = {42, 1}, .type = BuildingType::Flamethrower,
        .gridPosition = {-1, -1}};
    const auto render = [&](bool hovered, double size) {
        SimulationSnapshot snapshot;
        snapshot.buildings = {&building, 1};
        if (hovered) snapshot.aimedBuilding = building.id;
        FinalAttackStats stats;
        stats.range = 4; stats.size = size;
        snapshot.aimedFinalAttackStats = stats;
        renderer.beginFrame();
        BeginTextureMode(target); ClearBackground({36, 66, 72, 255}); BeginMode3D(camera);
        app_detail::drawBuildingTacticalOverlay(renderer, snapshot);
        EndMode3D(); EndTextureMode();
        Image image = LoadImageFromTexture(target.texture);
        auto* pixels = LoadImageColors(image);
        int white = 0;
        for (int index = 0; index < image.width * image.height; ++index) {
            const auto color = pixels[index];
            if (color.r > 90 && std::abs(int(color.r)-int(color.g)) < 20 &&
                std::abs(int(color.g)-int(color.b)) < 20) ++white;
        }
        if (hovered && size == 1 && std::getenv("IAN_FLAME_ATTACK_CAPTURE")) {
            ImageFlipVertical(&image);
            const auto path = std::filesystem::path{std::getenv("IAN_FLAME_ATTACK_CAPTURE")} / "hover-range.png";
            std::filesystem::create_directories(path.parent_path()); ExportImage(image, path.string().c_str());
        }
        UnloadImageColors(pixels); UnloadImage(image);
        return white;
    };
    const int absent = render(false, 1);
    const int ordinary = render(true, 1);
    const int enhanced = render(true, 1.5);
    UnloadRenderTexture(target);
    return absent == 0 && ordinary > 300 && enhanced > ordinary * 1.25;
}

inline bool checkFlamethrowerAnimation(ian::Renderer& renderer) {
    using namespace ian;
    const auto envelope = flamethrower_effect::emissionEnvelope;
    if (envelope(0, 0, -1) != 0 || envelope(.08, 0, -1) <= 0 || envelope(.08, .2, -1) != 0 ||
        envelope(1, 0, .2) != 0 || envelope(1, .3, .2) < .99F || envelope(1, .3, .5) != 0)
        return false;
    const auto target = LoadRenderTexture(960, 600);
    const Camera3D camera{{4, 3, 6}, {0, 0.9F, -1.7F}, {0, 1, 0}, 5.6F, CAMERA_ORTHOGRAPHIC};
    const auto render = [&](double seconds, bool cover) {
        renderer.beginFrame();
        BeginTextureMode(target);
        ClearBackground({36, 66, 72, 255});
        BeginMode3D(camera);
        WorldLighting lighting{}; lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        static_cast<void>(renderer.drawFlamethrower({0, 0, 0}, 0));
        DrawPlane({0, -.02F, -1.5F}, {10, 10}, {74, 122, 70, 255});
        if (cover) DrawCube({0, 1, 2}, 16, 8, .1F, {42, 61, 70, 255});
        WorldMaterialState material{};
        material.unlitAmount = 1; material.screenAoAmount = 0;
        material.inkOutlineEligible = false; material.inkOccluder = true;
        renderer.setWorldMaterial(material);
        fire_wand_effect::beginBatch();
        flamethrower_effect::emitStream({0, .924F, -.59F}, {0, 0, -1}, camera.position,
            3.41F, 1, 1, seconds, 7, 18, true);
        fire_wand_effect::endBatch();
        renderer.endWorldShader();
        EndMode3D(); EndTextureMode();
        Image image = LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
        return image;
    };
    Image first = render(1.25, false);
    Image second = render(1.39, false);
    Image covered = render(1.39, true);
    auto* firstPixels = LoadImageColors(first);
    auto* secondPixels = LoadImageColors(second);
    auto* coveredPixels = LoadImageColors(covered);
    int changed = 0, visibleFire = 0, coveredFire = 0;
    const auto firePixel = [](Color color) { return color.r > 210 && color.g > 110 && color.b < 100; };
    for (int index = 0; index < first.width * first.height; ++index) {
        if (std::abs(int(firstPixels[index].r) - int(secondPixels[index].r)) > 25 ||
            std::abs(int(firstPixels[index].g) - int(secondPixels[index].g)) > 25) ++changed;
        if (firePixel(secondPixels[index])) ++visibleFire;
        if (firePixel(coveredPixels[index])) ++coveredFire;
    }
    if (const auto* directory = std::getenv("IAN_FLAME_ATTACK_CAPTURE")) {
        std::filesystem::create_directories(directory);
        ExportImage(first, (std::filesystem::path{directory} / "stream-1.png").string().c_str());
        ExportImage(second, (std::filesystem::path{directory} / "stream-2.png").string().c_str());
        ExportImage(covered, (std::filesystem::path{directory} / "occluded.png").string().c_str());
    }
    UnloadImageColors(firstPixels); UnloadImageColors(secondPixels); UnloadImageColors(coveredPixels);
    UnloadImage(first); UnloadImage(second); UnloadImage(covered); UnloadRenderTexture(target);
    // The pitch pose must move the imported socket identically to simulation.
    const float yaw = .6F, pitch = .3F;
    const auto transformed = Vector3Transform({0, .924F, -.59F},
        flamethrower_model::meshTransform(1, {1, 2, 3}, 0, yaw, pitch));
    const auto socket = flamethrowerMuzzleWorldPosition({1, 2, 3}, yaw, pitch);
    const bool rigMatches = Vector3Distance(transformed, flamethrower_effect::point(socket)) < .00001F;
    return changed > 250 && visibleFire > 300 && coveredFire < 10 && rigMatches;
}

inline bool checkFlamethrowerUpgradeModels(ian::Renderer& renderer) {
    using namespace ian;
    ModelResource upgrades;
    if (!upgrades.load("assets/models/buildings/flamethrower_upgrades.glb") ||
        upgrades.get().meshCount != 21) return false;
    const auto target = LoadRenderTexture(640, 480);
    const Camera3D camera{{2.5F, 2.1F, -3.4F}, {0, .7F, 0}, {0, 1, 0}, 2.1F, CAMERA_ORTHOGRAPHIC};
    Color* previous = nullptr;
    bool different = true;
    for (int level = 1; level <= 8; ++level) {
        renderer.beginFrame();
        BeginTextureMode(target); ClearBackground({36, 66, 72, 255}); BeginMode3D(camera);
        WorldLighting lighting{}; lighting.cameraPosition = camera.position;
        renderer.beginWorldShader(lighting);
        static_cast<void>(renderer.drawFlamethrower({0, 0, 0}, 0, WHITE, 1, 0, {}, level));
        renderer.endWorldShader(); EndMode3D(); EndTextureMode();
        Image image = LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
        Color* pixels = LoadImageColors(image);
        if (previous) {
            int changed = 0;
            for (int index = 0; index < image.width * image.height; ++index)
                if (std::abs(int(previous[index].r) - int(pixels[index].r)) > 15 ||
                    std::abs(int(previous[index].g) - int(pixels[index].g)) > 15) ++changed;
            different &= changed > 50;
            UnloadImageColors(previous);
        }
        previous = pixels;
        if (const auto* directory = std::getenv("IAN_FLAME_ATTACK_CAPTURE")) {
            std::filesystem::create_directories(directory);
            ExportImage(image, (std::filesystem::path{directory} /
                ("level-" + std::to_string(level) + ".png")).string().c_str());
        }
        UnloadImage(image);
    }
    UnloadImageColors(previous); UnloadRenderTexture(target);
    BuildingInstance building{.type = BuildingType::Flamethrower, .gridPosition = {0, 0}, .level = 1};
    const Ray gauge{{.04F, 1.30F, -3}, {0, 0, 1}};
    const auto baseHit = renderer.buildingRaycastDistance(building, {}, gauge, 10);
    building.level = 8;
    const auto upgradeHit = renderer.buildingRaycastDistance(building, {}, gauge, 10);
    if (!different || baseHit || !upgradeHit) {
        std::cerr << "flame upgrade checks: distinct=" << different << " baseGauge=" << bool(baseHit)
                  << " level8Gauge=" << bool(upgradeHit) << '\n';
    }
    return different && !baseHit && upgradeHit &&
        renderer.buildingVisualHeight(BuildingType::Flamethrower, 8) > 1.39F;
}

inline bool checkFlamethrowerGraphics(ian::Renderer& renderer) {
    using namespace ian;
    const auto collision=loadGlbCollisionAsset("assets/models/buildings/flamethrower.glb");
    ModelResource model;
    if (!collision.valid() || collision.colliders.size()!=1 || collision.sockets.size()!=1 ||
        collision.sockets[0].name!="MUZZLE_01" || !model.load("assets/models/buildings/flamethrower.glb") ||
        model.get().meshCount!=3 || std::abs(renderer.buildingVisualHeight(BuildingType::Flamethrower)-1.14F)>.001F)
        return false;
    GameUi ui; ui.initialize();
    if (!IsTextureValid(ui.buildingTexture(BuildingType::Flamethrower))) return false;
    const auto target=LoadRenderTexture(1280,720);
    const Camera3D camera{{2.8F,2.2F,-4.8F},{0,.6F,0},{0,1,0},3.3F,CAMERA_ORTHOGRAPHIC};
    const Color background{55,88,89,255};
    renderer.beginFrame();
    BeginTextureMode(target);
    ClearBackground(background);
    BeginMode3D(camera);
    WorldLighting lighting{}; lighting.cameraPosition=camera.position;
    renderer.beginWorldShader(lighting);
    DrawPlane({0,-.02F,0},{8,8},{104,137,86,255});
    const bool visible=renderer.drawFlamethrower({-.85F,0,0},0);
    static_cast<void>(renderer.drawCannon({.85F,0,0},0,0));
    renderer.endWorldShader();
    EndMode3D();
    SimulationSnapshot snapshot;
    snapshot.coreId=EntityId{1,1}; snapshot.coreMaxHealth=500;
    snapshot.coreLevel=1; snapshot.unlimitedResources=true;
    snapshot.unlockedBuildings.fill(true);
    snapshot.selectedBuilding=BuildingType::Flamethrower;
    snapshot.buildingCosts.fill({35,25,10});
    HudViewState view; view.buildingHotbarCategory=BuildingHotbarCategory::Defense;
    view.buildHotbarSelectionPosition=4; view.buildHotbarSelectionAlpha=1;
    const UiViewport viewport{{1280,720},1};
    const auto* previousViewport=ui_viewport_detail::current;
    ui_viewport_detail::current=&viewport;
    beginSdfUiTextBatch();
    hud_detail::drawBuildHotbar(ui,snapshot,view);
    endSdfUiTextBatch();
    ui_viewport_detail::current=previousViewport;
    EndTextureMode();
    Image capture=LoadImageFromTexture(target.texture); ImageFlipVertical(&capture);
    if (const auto* path=std::getenv("IAN_FLAMETHROWER_CAPTURE")) ExportImage(capture,path);
    UnloadImage(capture); UnloadRenderTexture(target);
    return visible && checkFlamethrowerAnimation(renderer) && checkFlamethrowerHoverRange(renderer) &&
        checkFlamethrowerUpgradeModels(renderer);
}
