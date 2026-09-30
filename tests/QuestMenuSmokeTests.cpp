#include "TestHarness.hpp"
#include "localization/Localization.hpp"
#include "progression/MetaProgression.hpp"
#include "ui/QuestMenu.hpp"
#include "ui/UiText.hpp"

#include <algorithm>
#include <array>
#include <filesystem>

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_HIGHDPI);
    InitWindow(64, 64, "Quest menu smoke");
    if (!IsWindowReady()) return 77;
    ian::initializeLocalization();
    ian::initializeUiText();
    ian::GameUi ui;
    ui.initialize();
    ian::QuestSystem quests;
    ian::MetaProgression profile;
    quests.beginRun(profile);
    const std::array signals{
        ian::QuestSignal{ian::QuestMetric::EnemiesDefeated, 284, "gun_turret"},
        ian::QuestSignal{ian::QuestMetric::BuildingsPlaced, 29},
        ian::QuestSignal{ian::QuestMetric::WaveReached, 6},
        ian::QuestSignal{ian::QuestMetric::ItemsCollected, 12},
        ian::QuestSignal{ian::QuestMetric::ChallengesCompleted, 3},
    };
    static_cast<void>(quests.record(profile, signals));
    profile.silver = 12;
    for (const auto language : {ian::Language::English, ian::Language::Russian}) {
        ian::setLanguage(language);
        for (const Vector2 viewport : {Vector2{1920, 1080}, Vector2{1280, 720}, Vector2{1280, 800}}) {
            SetWindowSize(static_cast<int>(viewport.x), static_cast<int>(viewport.y));
            PollInputEvents();
            const auto panel = ian::questMenuPanelBounds(viewport);
            require(panel.x > 0 && panel.y > 0 && panel.x + panel.width < viewport.x &&
                panel.y + panel.height < viewport.y, "quest panel fits standard and taller screens");
            auto target = LoadRenderTexture(static_cast<int>(viewport.x), static_cast<int>(viewport.y));
            ian::QuestMenuState state;
            BeginTextureMode(target);
            ClearBackground({61, 82, 66, 255});
            auto result = ian::drawQuestMenu(ui, quests, profile, state, viewport);
            require(!result.close && !state.selectedId.empty(), "quest screen selects a real quest on opening");
            EndTextureMode();
            if (argc > 1) {
                Image capture = LoadImageFromTexture(target.texture);
                ImageFlipVertical(&capture);
                const auto name = std::string("quests_") + std::to_string(static_cast<int>(viewport.x)) + "x" +
                    std::to_string(static_cast<int>(viewport.y)) + (language == ian::Language::Russian ? "_ru" : "_en") + ".png";
                require(ExportImage(capture, (std::filesystem::path(argv[1]) / name).string().c_str()), "quest screenshot exported");
                UnloadImage(capture);
            }
            const auto featured = quests.recommendedStatuses(profile);
            require(!featured.empty(), "preview profile has unfinished featured quests");
            BeginTextureMode(target);
            ClearBackground({61, 82, 66, 255});
            const float scale = std::min(viewport.x / 1920.0F, viewport.y / 1080.0F);
            const Vector2 origin{(viewport.x - 1920 * scale) * 0.5F, (viewport.y - 1080 * scale) * 0.5F};
            for (std::size_t index = 0; index < featured.size(); ++index) {
                const Rectangle card{origin.x + 47 * scale, origin.y + (349 + static_cast<float>(index) * 109) * scale,
                    396 * scale, 98 * scale};
                const Vector2 pointer{card.x + card.width * 0.5F, card.y + card.height * 0.5F};
                require(ian::drawQuestSummaryCard(ui, featured[index], card, scale, pointer, true),
                    "featured quest card opens on a click within its bounds");
            }
            EndTextureMode();
            if (argc > 1) {
                Image capture = LoadImageFromTexture(target.texture);
                ImageFlipVertical(&capture);
                const auto name = std::string("featured_") + std::to_string(static_cast<int>(viewport.x)) + "x" +
                    std::to_string(static_cast<int>(viewport.y)) + (language == ian::Language::Russian ? "_ru" : "_en") + ".png";
                require(ExportImage(capture, (std::filesystem::path(argv[1]) / name).string().c_str()), "featured quest screenshot exported");
                UnloadImage(capture);
            }
            BeginTextureMode(target);
            state.selectedId = featured.back().definition->id;
            result = ian::drawQuestMenu(ui, quests, profile, state, viewport);
            require(state.selectedId == featured.back().definition->id,
                "opening a featured quest retains the same catalog ID and details in the full screen");
            result = ian::drawQuestMenu(ui, quests, profile, state, viewport,
                {.pointer = {panel.x + 100, panel.y + panel.height * 0.5F}, .wheel = -1000});
            require(state.scroll > 0 && state.scroll < 2500, "wheel scroll clamps to list content");
            state.category = ian::QuestCategory::Cosmetics;
            state.selectedId = "obsolete.id";
            result = ian::drawQuestMenu(ui, quests, profile, state, viewport);
            require(state.scroll == 0 && state.selectedId.empty(), "empty category clears stale selection and scroll safely");
            result = ian::drawQuestMenu(ui, quests, profile, state, viewport, {.close = true});
            require(result.close, "close command dismisses quest screen");
            EndTextureMode();
            UnloadRenderTexture(target);
        }
    }
    // Exercise the real purchase button against backend eligibility and wallet.
    ian::MetaProgression buyer = profile;
    const std::array milestones{
        ian::QuestSignal{ian::QuestMetric::ResourcesGathered, 1000},
        ian::QuestSignal{ian::QuestMetric::ChestsOpened, 10},
        ian::QuestSignal{ian::QuestMetric::CoinsCollected, 500},
        ian::QuestSignal{ian::QuestMetric::PlayerLevel, 20},
    };
    static_cast<void>(quests.record(buyer, milestones));
    buyer.silver = 20;
    ian::QuestMenuState buying;
    buying.category = ian::QuestCategory::General;
    buying.selectedId = "quests_10";
    ian::setLanguage(ian::Language::English);
    SetWindowSize(1280, 720);
    PollInputEvents();
    auto purchaseTarget = LoadRenderTexture(1280, 720);
    BeginTextureMode(purchaseTarget);
    auto purchased = ian::drawQuestMenu(ui, quests, buyer, buying, {1280, 720},
        {.pointer = {1042, 600}, .click = true});
    require(purchased.purchase && quests.ownsUnlock(buyer, "meta.refresh") && buyer.silver == 10,
            "eligible purchase button deducts actual silver and grants permanent ownership");
    purchased = ian::drawQuestMenu(ui, quests, buyer, buying, {1280, 720},
        {.pointer = {1042, 600}, .click = true});
    require(!purchased.purchase && buyer.silver == 10, "owned unlock has no repeat purchase button");
    EndTextureMode();
    UnloadRenderTexture(purchaseTarget);
    ian::setLanguage(ian::Language::English);
    ui.shutdown();
    ian::shutdownUiText();
    CloseWindow();
}
