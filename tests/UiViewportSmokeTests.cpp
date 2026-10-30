#include "TestHarness.hpp"
#include "app/UserSettings.hpp"
#include "ui/UiViewport.hpp"
#include "ui/UiRenderLayer.hpp"
#include "ui/UiText.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudRenderer.hpp"
#include "game/Simulation.hpp"
#include "localization/Localization.hpp"
#include "world/VisualBiomes.hpp"

#include <filesystem>
#include <memory>
#include <algorithm>
#include <array>
#include <cstdlib>

namespace {
void verifyHudFade(ian::GameUi& ui, const ian::SimulationSnapshot& snapshot) {
    using namespace ian;
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    auto output = LoadRenderTexture(width, height);
    auto layer = LoadRenderTexture(width, height);
    auto cache = LoadRenderTexture(32, 32);
    require(IsRenderTextureValid(output) && IsRenderTextureValid(layer) &&
                IsRenderTextureValid(cache), "HUD fade render targets load");
    for (int frame = 0; frame < 3; ++frame) {
        ScopedUiRenderLayer capture(output);
        ClearBackground({0, 0, 200, 255});
        const ScopedUiCanvas canvas;
        suspendUiCanvas();
        {
            ScopedUiRenderLayer hud(layer);
            resumeUiCanvas();
            DrawRectangle(0, 0, uiScreenWidth(), uiScreenHeight(), {200, 0, 0, 255});
            // Exercise real minimap and SDF caches while an outer target owns HUD.
            drawMinimapHud(ui, snapshot, 0.0F);
            drawUiText("FADING HUD", {40, 40}, 24, WHITE);
            suspendUiCanvas();
            BeginTextureMode(cache);
            ClearBackground(GREEN);
            EndTextureMode();
            restoreUiRenderLayer();
            resumeUiCanvas();
            DrawRectangle(0, uiScreenHeight() / 2,
                          uiScreenWidth(), uiScreenHeight() / 2, {0, 200, 0, 255});
            rlDrawRenderBatchActive();
            require(rlGetActiveFramebuffer() == layer.id,
                    "nested UI caches restore the HUD layer rather than the window");
            suspendUiCanvas();
        }
        resumeUiCanvas();
        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        DrawTexturePro(layer.texture, {0, 0, static_cast<float>(width), -static_cast<float>(height)},
            {0, 0, static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())},
            {}, 0, {128, 128, 128, 128});
        EndBlendMode();
    }
    Image image = LoadImageFromTexture(output.texture);
    ImageFlipVertical(&image);
    const Color top = GetImageColor(image, width / 2, height / 4);
    const Color bottom = GetImageColor(image, width / 2, height * 3 / 4);
    require(top.r > 85 && top.r < 115 && top.b > 85 && top.b < 115,
            "HUD geometry fades evenly over the world at half opacity");
    require(bottom.g > 85 && bottom.g < 115 && bottom.b > 85 && bottom.b < 115,
            "HUD content after nested caches stays aligned and fades as one layer");
    require(ui_render_layer_detail::current == nullptr,
            "HUD fade leaves no stale render target after closing");
    UnloadImage(image);
    UnloadRenderTexture(cache);
    UnloadRenderTexture(layer);
    UnloadRenderTexture(output);
}

void verifyMapMarkers(ian::SimulationSnapshot snapshot) {
    using namespace ian;
    snapshot.unlimitedResources = false;
    snapshot.lootStacks.fill(0);
    snapshot.worldLandmarks = {};
    snapshot.worldScenery = {};
    snapshot.challengeColumns = {};
    std::array resources{
        ResourceNode{.type = ResourceType::Wood, .position = {1, 0, 1}, .active = true},
        ResourceNode{.type = ResourceType::Wood, .position = {2, 0, 2}, .active = true},
        ResourceNode{.type = ResourceType::Stone, .position = {-1, 0, -1}, .active = true},
        ResourceNode{.type = ResourceType::Crystal, .position = {50, 0, 50}, .active = false},
        ResourceNode{.type = ResourceType::Barrel, .position = {140, 0, 140}, .active = true}};
    std::array chests{
        LootChestInstance{.position = {20, 0, 0}, .revealed = false},
        LootChestInstance{.position = {30, 0, 0}, .revealed = true}};
    snapshot.resourceNodes = resources;
    snapshot.lootChests = chests;
    auto markers = collectMinimapMarkers(snapshot);
    const auto count = [&](MapIcon icon) {
        return std::ranges::count_if(markers, [icon](const MinimapMarker& m) { return m.icon == icon; });
    };
    require(count(MapIcon::Chest) == 1, "map icons respect chest discovery");
    require(count(MapIcon::Wood) == 1 && count(MapIcon::Stone) == 1,
        "nearby resources cluster by type and handle negative world coordinates");
    require(count(MapIcon::Crystal) == 0 && count(MapIcon::Supplies) == 0,
        "depleted resources and distant undiscovered supplies stay off the map");
    const auto wood = std::ranges::find_if(markers, [](const MinimapMarker& m) { return m.icon == MapIcon::Wood; });
    require(wood->count == 2 && (wood->position.x == 1 || wood->position.x == 2),
        "cluster marker anchors on a real resource rather than the water between nodes");
    snapshot.lootStacks[lootUpgradeIndex(LootUpgradeEffect::Map)] = 1;
    markers = collectMinimapMarkers(snapshot);
    require(count(MapIcon::Chest) == 2 && count(MapIcon::Supplies) == 1,
        "map item reveals chest and supply icons");
    chests[1].state = LootChestState::Open;
    chests[1].loot.available = true;
    markers = collectMinimapMarkers(snapshot);
    require(count(MapIcon::Chest) == 2, "uncollected chest loot remains marked");
    chests[1].loot.collected = true;
    resources[0].active = resources[1].active = false;
    markers = collectMinimapMarkers(snapshot);
    require(count(MapIcon::Chest) == 1 && count(MapIcon::Wood) == 0,
        "collected chest loot and depleted clusters clear from the map");
}

void verifyMapWater(ian::SimulationSnapshot snapshot) {
    using namespace ian;
    std::array<float, 4> flatHeights{};
    std::array ponds{PondDefinition{.radiusX = 12, .radiusZ = 14, .depth = 3,
        .islandRadius = 3}};
    snapshot.resourceNodes = {};
    snapshot.terrainPaths = {};
    snapshot.terrainSamples = flatHeights;
    snapshot.terrainResolution = 2;
    snapshot.terrainWorldSize = 384;
    snapshot.playerPosition = {};
    snapshot.ponds = ponds;
    auto target = LoadRenderTexture(400, 400);
    MinimapLandscape landscape;
    const auto draw = [&] {
        BeginTextureMode(target);
        landscape.draw(snapshot, {0, 0, 400, 400}, 40);
        EndTextureMode();
        Image result = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&result);
        return result;
    };
    Image capture = draw();
    const Color island = GetImageColor(capture, 200, 200);
    const Color deep = GetImageColor(capture, 230, 200);
    const Color shallow = GetImageColor(capture, 255, 200);
    // Compare with the same terrain without water rather than duplicating its palette.
    auto landSnapshot = snapshot;
    landSnapshot.ponds = {};
    MinimapLandscape landOnly;
    BeginTextureMode(target);
    landOnly.draw(landSnapshot, {0, 0, 400, 400}, 40);
    EndTextureMode();
    Image landCapture = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&landCapture);
    const Color expectedIsland = GetImageColor(landCapture, 200, 200);
    require(std::abs(island.r-expectedIsland.r) < 3 && std::abs(island.g-expectedIsland.g) < 3 &&
        std::abs(island.b-expectedIsland.b) < 3, "actual islands retain the same land color as dry terrain");
    UnloadImage(landCapture);
    landOnly.shutdown();
    require(deep.b > deep.r + 60 && shallow.g > deep.g,
        "water has distinct shallow and deep colors");
    // Existing roads must remain on dry land, including islands, without
    // drawing fictitious bridges over the world's actual pond shape.
    auto roadSnapshot = snapshot;
    std::array roads{TerrainPathDefinition{.fromX = -18, .toX = 18, .halfWidth = 1.8}};
    roadSnapshot.terrainPaths = roads;
    MinimapLandscape withRoad;
    BeginTextureMode(target);
    withRoad.draw(roadSnapshot, {0, 0, 400, 400}, 40);
    EndTextureMode();
    Image roadCapture = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&roadCapture);
    const Color trail = GetImageColor(roadCapture, 120, 200);
    const Color dryGround = GetImageColor(capture, 120, 200);
    const Color waterUnderTrail = GetImageColor(roadCapture, 230, 200);
    require(trail.r > dryGround.r + 15 && trail.b > dryGround.b + 10,
        "actual road curves receive readable sandy trail markings");
    require(std::abs(waterUnderTrail.r-deep.r) < 3 && std::abs(waterUnderTrail.g-deep.g) < 3 &&
        std::abs(waterUnderTrail.b-deep.b) < 3, "road markings do not create imaginary bridges across water");
    UnloadImage(roadCapture);
    withRoad.shutdown();
    UnloadImage(capture);
    ponds[0].x = 30;
    ++snapshot.terrainSeed;
    capture = draw();
    const Color replaced = GetImageColor(capture, 230, 200);
    require(replaced.r > deep.r + 25, "changing terrain seed invalidates the landscape cache and removes old water");
    UnloadImage(capture);
    snapshot.ponds = {};
    snapshot.terrainSeed = 42;
    const auto biomes = visual_biomes::generate(snapshot.terrainSeed, snapshot.terrainWorldSize);
    BeginTextureMode(target);
    landscape.draw(snapshot, {0, 0, 400, 400}, 192);
    EndTextureMode();
    capture = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&capture);
    std::array<Color, 4> biomeColors{};
    for (std::size_t i = 0; i < biomeColors.size(); ++i) {
        biomeColors[i] = GetImageColor(capture,
            static_cast<int>((biomes.centers[i][0] + 192) / 384 * 400),
            static_cast<int>((biomes.centers[i][1] + 192) / 384 * 400));
    }
    require(biomeColors[0].g > biomeColors[0].r && biomeColors[1].r > biomeColors[1].g &&
        biomeColors[2].r > biomeColors[2].b && biomeColors[3].b > biomeColors[3].r,
        "the full tactical map displays the same four regions as the world renderer");
    if (const char* directory = std::getenv("IAN_CAPTURE_BIOMES")) {
        std::filesystem::create_directories(directory);
        ExportImage(capture, (std::filesystem::path{directory} / "map.png").string().c_str());
    }
    UnloadImage(capture);
    landscape.shutdown();
    UnloadRenderTexture(target);
}
} // namespace

int main(int argc, char** argv) {
    using namespace ian;
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_HIGHDPI);
    InitWindow(640, 360, "Responsive UI smoke");
    if (!IsWindowReady()) return 77;
    initializeLocalization();
    initializeUiText();
    GameUi ui;
    ui.initialize();
    auto simulation = std::make_unique<Simulation>();
    simulation->startRunFromSeed(PlayerClass::Engineer, 12345);
    auto snapshot = simulation->snapshot();
    verifyMapMarkers(snapshot);
    verifyMapWater(snapshot);
    verifyHudFade(ui, snapshot);
    snapshot.coreMaxHealth = 500;
    snapshot.coreLevel = 1;
    snapshot.coreHealth = 350;
    snapshot.playerMaxRecoverableArmor = 25;
    snapshot.playerRecoverableArmor = 20;
    snapshot.unlockedBuildings.fill(true);
    snapshot.unlimitedResources = true;
    const HudViewState view{.actionMode = ActionMode::Buildings, .hotbarModeBlend = 1.0F,
        .foundationBuildMode = true,
        .activeModularBuildPiece = ModularBuildPiece::FloorPlatform,
        .foundationHotbarSelectionPosition = 19.0F,
        .foundationHotbarSelectionAlpha = 1.0F};
    const Camera3D camera{{0, 4, 10}, {0, 0, 0}, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
    for (Vector2 resolution : {Vector2{640, 360}, Vector2{1024, 768},
            Vector2{1280, 800}, Vector2{1280, 1024}, Vector2{800, 1200},
            Vector2{2560, 720}, Vector2{1920, 1080}}) {
        SetWindowSize(static_cast<int>(resolution.x), static_cast<int>(resolution.y));
        PollInputEvents();
        const auto viewport = UiViewport::fromScreen(resolution);
        require(viewport.size.x >= 1599.9F && viewport.size.y >= 899.9F,
            "fixed-size panels and full hotbar have room at every aspect ratio");
        requireNear(viewport.size.x / viewport.size.y, resolution.x / resolution.y,
            1e-5, "UI canvas preserves window aspect ratio");
        Rectangle button;
        for (int frame = 0; frame < 5; ++frame) {
            BeginDrawing();
            ClearBackground({40, 60, 50, 255});
            {
                const ScopedUiCanvas canvas;
                require(uiScreenWidth() >= 1600 && uiScreenHeight() >= 900,
                    "drawing and layout use the same logical canvas");
                const Vector2 projected = uiWorldToScreen({0, 0, 0}, camera);
                const Vector2 physical = GetWorldToScreen({0, 0, 0}, camera);
                requireNear(projected.x * viewport.scale, physical.x, 0.01,
                    "world-attached UI retains the real camera projection");
                requireNear(projected.y * viewport.scale, physical.y, 0.01,
                    "world-attached UI retains vertical projection");
                // Nested scopes must neither double-scale nor reset the canvas.
                { const ScopedUiCanvas nested; require(!nested.owns(), "nested UI layers reuse the canvas"); }
                beginUiScissor(20, 120, 120, 60);
                DrawRectangle(0, 110, 300, 90, RED);
                EndScissorMode();
                drawHud(ui, snapshot, view, camera, {});
                ui.markMinimapTargetRefreshed(-1000);
                drawMinimapHud(ui, snapshot, frame == 0 ? 1.0F : frame == 1 ? .5F : 0.0F);
                button = {static_cast<float>(uiScreenWidth()) * 0.5F - 100, static_cast<float>(uiScreenHeight()) * 0.5F, 200, 64};
                // Repeated labels promote the cache and exercise texture-mode
                // restoration before the colored marker/button is submitted.
                beginSdfUiTextBatch();
                for (int line = 0; line < 10; ++line)
                    drawSdfUiText("CANVAS CACHE", {40, 230.0F + static_cast<float>(line) * 24.0F}, 14, WHITE);
                endSdfUiTextBatch();
                DrawRectangleRec(button, BLUE);
            }
            require(uiScreenWidth() == GetScreenWidth() && uiScreenHeight() == GetScreenHeight(),
                "leaving UI restores physical coordinates for game rendering");
            EndDrawing();
        }
        Image capture = LoadImageFromScreen();
        const auto sample = [&](float x, float y) {
            return GetImageColor(capture, static_cast<int>(x * static_cast<float>(capture.width) / resolution.x),
                static_cast<int>(y * static_cast<float>(capture.height) / resolution.y));
        };
        const Color formerWeaponRow=sample(25.0F*viewport.scale,210.0F*viewport.scale);
        require(std::abs(int(formerWeaponRow.r)-40)<=2 &&
            std::abs(int(formerWeaponRow.g)-60)<=2 && std::abs(int(formerWeaponRow.b)-50)<=2,
            "game HUD does not draw the removed upper-left equipment row at any resolution");
        const Rectangle physicalButton = viewport.toScreen(button);
        const Color center = sample(physicalButton.x + physicalButton.width * 0.5F,
            physicalButton.y + physicalButton.height * 0.5F);
        if (!(center.b > 200 && center.r < 30)) {
            TraceLog(LOG_ERROR, "Viewport %gx%g capture %dx%d marker %g,%g color %d,%d,%d",
                resolution.x, resolution.y, capture.width, capture.height, physicalButton.x, physicalButton.y, center.r, center.g, center.b);
            ExportImage(capture, "/tmp/ian-responsive-failure.png");
        }
        require(center.b > 200 && center.r < 30, "cached text/minimap do not reset the UI transform");
        const Color clipped = sample(30 * viewport.scale, 140 * viewport.scale);
        const Color outside = sample(160 * viewport.scale, 140 * viewport.scale);
        require(clipped.r > 200 && outside.r < 100,
            "scroll/list scissor clips in physical screen coordinates");
        SetMousePosition(static_cast<int>(physicalButton.x + physicalButton.width * 0.5F),
            static_cast<int>(physicalButton.y + physicalButton.height * 0.5F));
        {
            const ScopedUiCoordinates input;
            require(CheckCollisionPointRec(uiMousePosition(), button),
                "mouse hitboxes match scaled rendering");
        }
        for (const unsigned eventType : {6U, 5U}) {
            AutomationEvent event{};
            event.type = eventType;
            event.params[0] = MOUSE_BUTTON_LEFT;
            PlayAutomationEvent(event);
            BeginDrawing();
            {
                const ScopedUiCanvas canvas;
                const bool clicked = ui.drawButton(button, "VIEWPORT BUTTON");
                require(clicked == (eventType == 5U),
                    "raygui buttons activate on release at their scaled screen position");
            }
            EndDrawing();
        }
        if (argc > 1) {
            auto expandedSnapshot = snapshot;
            expandedSnapshot.playerPosition = {63, 0, -45};
            expandedSnapshot.playerYaw = .73;
            for (int frame = 0; frame < 3; ++frame) {
                ui.markMinimapTargetRefreshed(-100.0);
                BeginDrawing();
                ClearBackground({40, 60, 50, 255});
                drawMinimapHud(ui, expandedSnapshot, 1.0F);
                EndDrawing();
            }
            Image mapCapture = LoadImageFromScreen();
            const auto mapPath = std::filesystem::path(argv[1]) / ("map_" +
                std::to_string(static_cast<int>(resolution.x)) + "x" +
                std::to_string(static_cast<int>(resolution.y)) + ".png");
            require(ExportImage(mapCapture, mapPath.string().c_str()), "expanded map screenshot exported");
            UnloadImage(mapCapture);
            const auto path = std::filesystem::path(argv[1]) / ("hud_" +
                std::to_string(static_cast<int>(resolution.x)) + "x" +
                std::to_string(static_cast<int>(resolution.y)) + ".png");
            require(ExportImage(capture, path.string().c_str()), "HUD screenshot exported");
        }
        UnloadImage(capture);
    }
    if (argc > 1) {
        snapshot.unlimitedResources = false;
        snapshot.unlockedBuildings.fill(false);
        for (const auto type : {BuildingType::Core, BuildingType::GunTurret,
                               BuildingType::Cannon, BuildingType::LumberMill,
                               BuildingType::Gate}) {
            snapshot.unlockedBuildings[static_cast<std::size_t>(type)] = true;
        }
        snapshot.selectedBuilding = BuildingType::GunTurret;
        snapshot.placedBuildingCount = 1;
        snapshot.totalBuildingLimit = 3;
        for (const bool corePlaced : {true, false}) {
            snapshot.coreMaxHealth = corePlaced ? 500 : 0;
            if (!corePlaced) snapshot.selectedBuilding = BuildingType::Core;
            auto compactView = view;
            compactView.foundationBuildMode = false;
            compactView.foundationHotbarSelectionPosition = 0;
            for (int frame = 0; frame < 3; ++frame) {
                BeginDrawing();
                ClearBackground({40, 60, 50, 255});
                const ScopedUiCanvas canvas;
                drawHud(ui, snapshot, compactView, camera, {});
                EndDrawing();
            }
            Image compact = LoadImageFromScreen();
            const auto logical = UiViewport::fromScreen(
                {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())});
            const int rowY = compact.height - static_cast<int>(50.0F * logical.scale *
                static_cast<float>(compact.height) / static_cast<float>(GetScreenHeight()));
            int visibleSlots = 0;
            for (int x = compact.width / 4; x < compact.width * 3 / 4; ++x) {
                const Color pixel = GetImageColor(compact, x, rowY);
                visibleSlots += pixel.r > 110 && pixel.g > 80 && pixel.r > pixel.b + 25;
            }
            require(visibleSlots > 40, "normal-run and Core-only construction hotbars remain visible");
            require(ExportImage(compact, (std::filesystem::path(argv[1]) /
                (corePlaced ? "hotbar-normal.png" : "hotbar-before-core.png")).string().c_str()),
                "normal-run and Core-only hotbar screenshots exported");
            UnloadImage(compact);
        }
    }
    ui.shutdown();
    shutdownUiText();
    CloseWindow();
}
