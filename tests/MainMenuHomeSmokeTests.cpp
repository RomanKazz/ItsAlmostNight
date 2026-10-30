#include "TestHarness.hpp"
#include "localization/Localization.hpp"
#include "ui/MainMenuHome.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <cmath>

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "Main menu home smoke");
    if (!IsWindowReady()) return 77;
    ian::initializeLocalization();
    ian::GameUi ui;
    ui.initialize();
    require(IsTextureValid(ui.mainMenuLogoTexture()), "original menu logo loads");
    for (int index = 0; index < static_cast<int>(ian::MainMenuTexture::Count); ++index) {
        const auto texture = ui.mainMenuTexture(static_cast<ian::MainMenuTexture>(index));
        require(IsTextureValid(texture), "every current menu icon creates a texture");
        Image image = LoadImageFromTexture(texture);
        Color* pixels = LoadImageColors(image);
        require(pixels && std::any_of(pixels, pixels + image.width * image.height,
            [](Color pixel) { return pixel.a != 0; }), "Figma asset renders non-empty artwork");
        UnloadImageColors(pixels);
        UnloadImage(image);
    }
    // These are live QuestStatus objects, not labels/progress baked into art.
    const std::array definitions{
        ian::QuestDefinition{"spikes", "Defeat 1000 blobs", "Defeat 1000 blobs",
            ian::QuestCategory::Buildings,
            {{ian::QuestMetric::EnemiesDefeated, ian::QuestScope::Lifetime, 1000,
              ian::QuestComparison::AtLeast, "spike_trap", ""}}, {}, {2, "", 0}},
        ian::QuestDefinition{"build", "Place 100 buildings", "Place 100 buildings",
            ian::QuestCategory::Buildings, {{ian::QuestMetric::BuildingsPlaced,
                ian::QuestScope::Lifetime, 100, ian::QuestComparison::AtLeast, "", ""}}, {}, {2, "", 0}},
        ian::QuestDefinition{"repair", "Repair buildings 100 times", "Repair buildings 100 times",
            ian::QuestCategory::Buildings, {{ian::QuestMetric::StructuresRepaired,
                ian::QuestScope::Lifetime, 100, ian::QuestComparison::AtLeast, "", ""}}, {}, {2, "", 0}},
    };
    const std::array featured{
        ian::QuestStatus{.definition = &definitions[0], .progress = {744}, .fraction = 0.744},
        ian::QuestStatus{.definition = &definitions[1], .progress = {4}, .fraction = 0.04},
        ian::QuestStatus{.definition = &definitions[2], .progress = {47}, .fraction = 0.47},
    };
    ian::MetaProgression profile;
    profile.silver = 72;
    profile.bestWave = 265;
    profile.runsPlayed = 100;
    profile.stageClears = 14;
    profile.enemiesDefeated = 17123;
    profile.resourcesGathered = 5240;
    profile.lootCollected = 1028;
    for (const auto language : {ian::Language::English, ian::Language::Russian}) {
        ian::setLanguage(language);
        for (const Vector2 viewport : {Vector2{1920, 1080}, Vector2{1280, 720},
                Vector2{1280, 800}, Vector2{1920, 1200}, Vector2{2560, 1080}, Vector2{1024, 768}, Vector2{1280, 1024}, Vector2{800, 1200}, Vector2{640, 360}, Vector2{2560, 720}}) {
            const float scale = std::min(viewport.x / 1920, viewport.y / 1080);
            const Vector2 origin{(viewport.x - 1920 * scale) * 0.5F,
                                 (viewport.y - 1080 * scale) * 0.5F};
            const auto point = [&](float x, float y) {
                return Vector2{origin.x + x * scale, origin.y + y * scale};
            };
            const auto topLeft = [&](float x, float y) {
                return Vector2{x * scale, y * scale};
            };
            const auto bottomLeft = [&](float x, float y) {
                return Vector2{x * scale, viewport.y - (1080 - y) * scale};
            };
            const auto bottomRight = [&](float x, float y) {
                return Vector2{viewport.x - (1920 - x) * scale,
                    viewport.y - (1080 - y) * scale};
            };
            const auto bottomCenter = [&](float x, float y) {
                return Vector2{origin.x + x * scale, viewport.y - (1080 - y) * scale};
            };
            const auto target = LoadRenderTexture(static_cast<int>(viewport.x), static_cast<int>(viewport.y));
            ian::MainMenuHomeState state;
            state.completedQuestCount = 16;
            state.totalQuestCount = 30;
            state.unclaimedQuestRewards = 3;
            BeginTextureMode(target);
            ClearBackground(WHITE);
            auto result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport);
            require(result.action == ian::MainMenuHomeAction::None,
                "drawing home does not activate gameplay or navigation");
            EndTextureMode();
            Image anchorCapture = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&anchorCapture);
            const auto hasArtwork = [&](Vector2 at) {
                const Color pixel = GetImageColor(anchorCapture,
                    static_cast<int>(std::round(at.x)), static_cast<int>(std::round(at.y)));
                return pixel.r != 255 || pixel.g != 255 || pixel.b != 255;
            };
            require(hasArtwork(topLeft(100, 80)), "quest panel stays at the top left edge");
            require(hasArtwork({viewport.x - 125 * scale, 50 * scale}),
                "currency stays at the top right edge");
            require(hasArtwork(bottomLeft(64, 929)), "footer stays at the bottom left edge");
            require(hasArtwork(bottomRight(1500, 780)), "records stay at the bottom right edge");
            require(!hasArtwork(bottomCenter(960, 1030)),
                "background editor button is absent from home");
            require(hasArtwork(point(960, 566)) && hasArtwork(point(682, 687)),
                "primary and secondary buttons render at their new positions");
            UnloadImage(anchorCapture);
            if (argc > 1) {
                Image capture = LoadImageFromTexture(target.texture);
                ImageFlipVertical(&capture);
                const auto name = std::string("main_menu_") + std::to_string(static_cast<int>(viewport.x)) + "x" +
                    std::to_string(static_cast<int>(viewport.y)) + (language == ian::Language::Russian ? "_ru" : "_en") + ".png";
                require(ExportImage(capture, (std::filesystem::path(argv[1]) / name).string().c_str()),
                    "main menu comparison screenshot exports");
                UnloadImage(capture);
            }
            constexpr std::array expected{
                ian::MainMenuHomeAction::Play, ian::MainMenuHomeAction::Unlocks,
                ian::MainMenuHomeAction::Quests, ian::MainMenuHomeAction::Shop,
                ian::MainMenuHomeAction::Settings,
                ian::MainMenuHomeAction::Exit};
            constexpr std::array<Vector2, 6> centers{{
                {960, 566}, {682, 687}, {960, 687}, {1238, 687},
                {177, 920}, {167, 1004}}};
            BeginTextureMode(target);
            const auto activate = [&](Vector2 pointer, bool canContinue) {
                const auto waiting = ian::drawMainMenuHome(ui, profile, featured, canContinue,
                    state, viewport, {.pointer = pointer, .click = true});
                require(waiting.action == ian::MainMenuHomeAction::None,
                    "screen transition waits for the selection animation");
                const auto activated = ian::drawMainMenuHome(ui, profile, featured, canContinue,
                    state, viewport, {.deltaTime = 0.25F});
                require(CheckCollisionPointRec(pointer,
                    {origin.x + state.focusBounds.x * scale, origin.y + state.focusBounds.y * scale,
                     state.focusBounds.width * scale, state.focusBounds.height * scale}),
                    "animated focus follows the anchored button hitbox");
                return activated;
            };
            for (std::size_t index = 0; index < centers.size(); ++index) {
                result = activate(index < 4 ? point(centers[index].x, centers[index].y)
                    : bottomLeft(centers[index].x, centers[index].y), false);
                require(result.action == expected[index], "all Figma buttons route to the correct game screen");
            }
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = point(960, 566), .pointerDown = true});
            require(result.action == ian::MainMenuHomeAction::None, "holding a button does not start another run");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = bottomCenter(960, 1024), .click = true});
            require(result.action == ian::MainMenuHomeAction::None,
                "the removed home background button has no active hitbox");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = topLeft(200, 170), .click = true});
            require(result.action == ian::MainMenuHomeAction::Quests && result.questId == "spikes",
                "quest card opens its real catalog ID");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = topLeft(200, 274), .click = true});
            require(result.action == ian::MainMenuHomeAction::Quests && result.questId == "build",
                "second quest card opens its own catalog ID");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = topLeft(370, 200), .click = true});
            require(result.action == ian::MainMenuHomeAction::Quests && result.questId == "spikes",
                "first quest card remains clickable where View All used to overlap");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = topLeft(200, 90), .click = true});
            require(result.action == ian::MainMenuHomeAction::Quests && result.questId.empty(),
                "quest panel header opens the full quest list");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = topLeft(200, 370), .click = true});
            require(result.action == ian::MainMenuHomeAction::None,
                "only two featured quests are displayed and clickable");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = point(960, 448), .click = true});
            require(result.action == ian::MainMenuHomeAction::None, "Continue is absent without a suspended run");
            result = activate(point(960, 448), true);
            require(result.action == ian::MainMenuHomeAction::Continue, "Continue remains available with a suspended run");
            const Rectangle continueBounds = state.focusBounds;
            result = activate(point(960, 566), true);
            require(result.action == ian::MainMenuHomeAction::Play &&
                std::abs(state.focusBounds.height - 120.0F) < 0.01F &&
                std::abs(continueBounds.height - 92.0F) < 0.01F &&
                continueBounds.y + continueBounds.height < state.focusBounds.y,
                "Continue is shorter while Play retains its original height");
            const Rectangle playBounds = state.focusBounds;
            result = activate(point(1238, 687), true);
            require(result.action == ian::MainMenuHomeAction::Shop,
                "secondary actions remain in a horizontal row below Play");
            require(continueBounds.y + continueBounds.height < playBounds.y &&
                playBounds.y + playBounds.height < state.focusBounds.y,
                "Continue, Play and secondary actions have distinct vertical tiers");
            const Rectangle shopBounds = state.focusBounds;
            static_cast<void>(activate(point(960, 687), true));
            const Rectangle questsBounds = state.focusBounds;
            static_cast<void>(activate(point(682, 687), true));
            const Rectangle unlocksBounds = state.focusBounds;
            require(std::abs((questsBounds.x - unlocksBounds.x - unlocksBounds.width) -
                (shopBounds.x - questsBounds.x - questsBounds.width)) < 0.01F,
                "secondary actions have even horizontal spacing");
            static_cast<void>(ian::drawMainMenuHome(ui, profile, featured, true, state, viewport,
                {.pointer = bottomRight(1700, 565), .click = true}));
            require(state.friendsLeaderboardVisible, "Friends tab selects the friends leaderboard preview");
            static_cast<void>(ian::drawMainMenuHome(ui, profile, featured, true, state, viewport,
                {.pointer = bottomRight(1580, 565), .click = true}));
            require(!state.friendsLeaderboardVisible, "Global tab selects the global leaderboard preview");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = topLeft(520, 170), .click = true});
            require(result.action == ian::MainMenuHomeAction::None,
                "space outside the compact quest widget is not clickable");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = bottomLeft(147, 982), .click = true});
            require(result.action == ian::MainMenuHomeAction::None,
                "the gap between footer actions does not activate a button");
            result = ian::drawMainMenuHome(ui, profile, featured, false, state, viewport,
                {.pointer = bottomLeft(540, 1017), .click = true});
            require(result.action == ian::MainMenuHomeAction::None,
                "footer ends after Settings and Exit with no third action");
            ian::MainMenuHomeState animated;
            static_cast<void>(ian::drawMainMenuHome(ui, profile, featured, false,
                animated, viewport, {.pointer = point(1238, 687), .click = true}));
            result = ian::drawMainMenuHome(ui, profile, featured, false,
                animated, viewport, {.deltaTime = ian::MainMenuSelectionSeconds * 0.5F});
            require(result.action == ian::MainMenuHomeAction::None &&
                animated.focusBounds.y > 506 && animated.focusBounds.y < 638,
                "focus corners move between buttons before navigation");
            require(animated.selectionAmounts[0] > 0 && animated.selectionAmounts[0] < 1 &&
                animated.selectionAmounts[3] > 0 && animated.selectionAmounts[3] < 1,
                "old button fades to grey as the clicked button fades to yellow");
            static_cast<void>(ian::drawMainMenuHome(ui, profile, featured, false,
                animated, viewport, {.pointer = point(960, 687), .click = true}));
            result = ian::drawMainMenuHome(ui, profile, featured, false,
                animated, viewport, {.deltaTime = ian::MainMenuSelectionSeconds});
            require(result.action == ian::MainMenuHomeAction::Quests &&
                animated.focusBounds.y == 638 && animated.selectionAmounts[2] == 1,
                "a second click replaces the pending action and focus destination");
            result = ian::drawMainMenuHome(ui, profile, featured, false,
                animated, viewport, {.deltaTime = ian::MainMenuSelectionSeconds});
            require(result.action == ian::MainMenuHomeAction::None,
                "animation completion activates a button only once");
            static_cast<void>(ian::drawMainMenuHome(ui, profile, {}, false, state, viewport));
            EndTextureMode();
            if (argc > 1 && viewport.x == 1920 && viewport.y == 1200) {
                ian::MainMenuHomeState preview;
                preview.completedQuestCount = 16;
                preview.totalQuestCount = 30;
                BeginTextureMode(target);
                ClearBackground(WHITE);
                static_cast<void>(ian::drawMainMenuHome(ui, profile, featured, true, preview, viewport));
                EndTextureMode();
                Image capture = LoadImageFromTexture(target.texture);
                ImageFlipVertical(&capture);
                const auto name = language == ian::Language::Russian
                    ? "main_menu_continue_ru.png" : "main_menu_continue_en.png";
                require(ExportImage(capture, (std::filesystem::path(argv[1]) / name).string().c_str()),
                    "Continue layout comparison screenshot exports");
                UnloadImage(capture);
            }
            UnloadRenderTexture(target);
        }
    }
    ui.shutdown();
    CloseWindow();
}
