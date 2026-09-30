#include "TestHarness.hpp"
#include "localization/Localization.hpp"
#include "progression/MetaProgression.hpp"
#include "ui/UnlockMenu.hpp"
#include "ui/UiText.hpp"

#include <array>
#include <filesystem>

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_HIGHDPI);
    InitWindow(64, 64, "Unlock menu smoke");
    if (!IsWindowReady()) return 77;
    ian::initializeLocalization();
    ian::initializeUiText();
    ian::GameUi ui;
    ui.initialize();
    ian::QuestSystem quests;
    ian::MetaProgression profile;
    quests.initialize(profile);
    for (const auto language : {ian::Language::English, ian::Language::Russian}) {
        ian::setLanguage(language);
        for (const Vector2 viewport : {Vector2{1920,1080}, Vector2{1280,720}, Vector2{1280,800}}) {
            SetWindowSize(static_cast<int>(viewport.x), static_cast<int>(viewport.y));
            PollInputEvents();
            const auto panel = ian::unlockMenuPanelBounds(viewport);
            require(panel.x > 0 && panel.y > 0 && panel.x + panel.width < viewport.x && panel.y + panel.height < viewport.y,
                "unlock panel fits different aspect ratios");
            const float scale = std::min(viewport.x / 1920, viewport.y / 1080);
            const Vector2 origin{(viewport.x - 1920 * scale) * .5F, (viewport.y - 1080 * scale) * .5F};
            const auto point = [&](float x, float y) { return Vector2{origin.x + x * scale, origin.y + y * scale}; };
            auto target = LoadRenderTexture(static_cast<int>(viewport.x), static_cast<int>(viewport.y));
            ian::UnlockMenuState state;
            BeginTextureMode(target);
            auto result = ian::drawUnlockMenu(ui, quests, profile, state, viewport);
            require(!result.close && state.selectedId == "class:0", "opening selects a real class");
            result = ian::drawUnlockMenu(ui, quests, profile, state, viewport,
                {.pointer = point(949,370), .click = true});
            require(state.selectedId == "class:4", "locked class can be selected for details");
            result = ian::drawUnlockMenu(ui, quests, profile, state, viewport,
                {.pointer = point(1520,880), .click = true});
            require(result.questId == "class_berserker", "locked class links to its actual quest");
            EndTextureMode();
            for (int tab = 0; tab < 6; ++tab) {
                state.category = static_cast<ian::UnlockCategory>(tab);
                state.selectedId = "missing-entry";
                BeginTextureMode(target);
                ClearBackground({61,82,66,255});
                result = ian::drawUnlockMenu(ui, quests, profile, state, viewport);
                require(!state.selectedId.empty(), "every category resolves stale selection to real content");
                EndTextureMode();
                if (argc > 1) {
                    Image capture = LoadImageFromTexture(target.texture);
                    ImageFlipVertical(&capture);
                    const auto name = "unlocks_" + std::to_string(tab) + "_" + std::to_string(static_cast<int>(viewport.x)) + "x" +
                        std::to_string(static_cast<int>(viewport.y)) + (language == ian::Language::Russian ? "_ru.png" : "_en.png");
                    require(ExportImage(capture, (std::filesystem::path(argv[1])/name).string().c_str()), "preview exported");
                    UnloadImage(capture);
                }
            }
            BeginTextureMode(target);
            state.category = ian::UnlockCategory::Items;
            result = ian::drawUnlockMenu(ui, quests, profile, state, viewport,
                {.pointer=point(330,380), .wheel=-1000});
            require(state.scroll == 0, "wheel cannot scroll beyond a grid that fits all content");
            result = ian::drawUnlockMenu(ui, quests, profile, state, viewport,
                {.pointer=point(310,230), .click=true});
            require(state.category == ian::UnlockCategory::Classes && state.selectedId == "class:0" && state.scroll == 0,
                "tab click resets scroll and changes grid immediately");
            state.category = ian::UnlockCategory::Items;
            state.selectedId = "item:0";
            result = ian::drawUnlockMenu(ui, quests, profile, state, viewport, {.selectionStep=18});
            require(state.selectedId == "item:18" && state.scroll == 0, "keyboard reaches the last row without scrolling fitting content");
            result = ian::drawUnlockMenu(ui, quests, profile, state, viewport, {.categoryStep=1});
            require(state.category == ian::UnlockCategory::Meta && state.selectedId == "meta.refresh", "tab navigation selects new category");
            result = ian::drawUnlockMenu(ui, quests, profile, state, viewport, {.close=true});
            require(result.close, "close input dismisses menu");
            EndTextureMode();
            UnloadRenderTexture(target);
        }
    }
    ian::setLanguage(ian::Language::English);
    SetWindowSize(1920,1080);
    auto target = LoadRenderTexture(1920,1080);
    BeginTextureMode(target);
    ian::UnlockMenuState state;
    state.category = ian::UnlockCategory::Meta;
    profile.silver = 30;
    auto result = ian::drawUnlockMenu(ui, quests, profile, state, {1920,1080},
        {.pointer={1520,880}, .click=true});
    require(!result.purchase && profile.silver == 30, "silver alone cannot bypass quest prerequisite");
    quests.beginRun(profile);
    const std::array signals{
        ian::QuestSignal{ian::QuestMetric::EnemiesDefeated, 1000, "gun_turret"},
        ian::QuestSignal{ian::QuestMetric::BuildingsPlaced, 30},
        ian::QuestSignal{ian::QuestMetric::WaveReached, 10},
        ian::QuestSignal{ian::QuestMetric::ItemsCollected, 30},
        ian::QuestSignal{ian::QuestMetric::ResourcesGathered, 1000},
        ian::QuestSignal{ian::QuestMetric::ChestsOpened, 10},
        ian::QuestSignal{ian::QuestMetric::CoinsCollected, 500},
        ian::QuestSignal{ian::QuestMetric::PlayerLevel, 20},
    };
    static_cast<void>(quests.record(profile, signals));
    if (argc > 1) {
        ClearBackground({61,82,66,255});
        result = ian::drawUnlockMenu(ui, quests, profile, state, {1920,1080});
        EndTextureMode();
        Image capture = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&capture);
        require(ExportImage(capture, (std::filesystem::path(argv[1])/"unlocks_ready_to_buy.png").string().c_str()), "eligible purchase preview exported");
        UnloadImage(capture);
        BeginTextureMode(target);
    }
    profile.silver = 9;
    result = ian::drawUnlockMenu(ui, quests, profile, state, {1920,1080}, {.pointer={1520,880}, .click=true});
    require(!result.purchase && !quests.ownsUnlock(profile,"meta.refresh"), "insufficient silver disables purchase");
    profile.silver = 30;
    result = ian::drawUnlockMenu(ui, quests, profile, state, {1920,1080}, {.pointer={1520,880}, .click=true});
    require(result.purchase && profile.silver == 20 && quests.ownsUnlock(profile,"meta.refresh"), "purchase spends actual price and grants persistent ownership");
    result = ian::drawUnlockMenu(ui, quests, profile, state, {1920,1080}, {.pointer={1520,880}, .click=true});
    require(!result.purchase && profile.silver == 20, "owned entry cannot be bought twice");
    state.category = ian::UnlockCategory::Classes;
    state.selectedId = "class:4";
    result = ian::drawUnlockMenu(ui, quests, profile, state, {1920,1080}, {.pointer={1520,880}, .click=true});
    require(result.questId.empty() && ian::isPlayerClassUnlocked(ian::PlayerClass::Berserker,profile), "quest completion unlocks same class in collection and class selection");
    EndTextureMode();
    UnloadRenderTexture(target);
    ui.shutdown();
    ian::shutdownUiText();
    CloseWindow();
    return 0;
}
