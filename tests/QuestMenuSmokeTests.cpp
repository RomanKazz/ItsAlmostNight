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
        for (const Vector2 viewport : {Vector2{1920, 1080}, Vector2{1280, 720}, Vector2{1280, 800}, Vector2{1024, 768}, Vector2{1280, 1024}, Vector2{800, 1200}, Vector2{640, 360}, Vector2{2560, 720}}) {
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
            auto clippedProfile = profile;
            ian::QuestMenuState clipped;
            clipped.scroll = 20;
            const auto outsideList = ian::drawQuestMenu(ui, quests, clippedProfile, clipped, viewport,
                {.pointer = {origin.x + 1590 * scale, origin.y + 306 * scale}, .click = true});
            require(!outsideList.rewardClaimed && clippedProfile == profile,
                "clipped part of a reward button cannot collect outside the visible list");
            ClearBackground({61, 82, 66, 255});
            for (std::size_t index = 0; index < featured.size(); ++index) {
                const Rectangle card{origin.x + 30 * scale, (30 + static_cast<float>(index) * 96) * scale,
                    440 * scale, 84 * scale};
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
            require(state.scroll > 0 && state.scroll <= static_cast<float>(quests.definitions().size()) * 66, "wheel scroll clamps to list content");
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
    ian::QuestDefinition paidUnlock{.id="paid_unlock",.title="Harvest resources",.category=ian::QuestCategory::General,
        .conditions={{ian::QuestMetric::ResourcesGathered,ian::QuestScope::Lifetime,1,
            ian::QuestComparison::AtLeast,{},{}}},.reward={0,"building.mortar",5}};
    ian::QuestSystem purchaseQuests({paidUnlock});
    ian::MetaProgression buyer;
    purchaseQuests.beginRun(buyer);
    const std::array milestones{
        ian::QuestSignal{ian::QuestMetric::ResourcesGathered, 1000},
        ian::QuestSignal{ian::QuestMetric::ChestsOpened, 10},
        ian::QuestSignal{ian::QuestMetric::CoinsCollected, 500},
        ian::QuestSignal{ian::QuestMetric::PlayerLevel, 20},
    };
    static_cast<void>(purchaseQuests.record(buyer,milestones));
    buyer.silver = 20;
    ian::QuestMenuState buying;
    buying.category = ian::QuestCategory::General;
    buying.selectedId = "paid_unlock";
    ian::setLanguage(ian::Language::English);
    SetWindowSize(1280, 720);
    PollInputEvents();
    auto purchaseTarget = LoadRenderTexture(1280, 720);
    BeginTextureMode(purchaseTarget);
    auto purchased = ian::drawQuestMenu(ui, purchaseQuests, buyer, buying, {1280, 720},
        {.pointer = {1042, 600}, .click = true});
    require(purchased.purchase && purchaseQuests.ownsUnlock(buyer,"building.mortar") && buyer.silver == 15,
            "eligible purchase button deducts actual silver and grants permanent ownership");
    purchased = ian::drawQuestMenu(ui, purchaseQuests, buyer, buying, {1280, 720},
        {.pointer = {1042, 600}, .click = true});
    require(!purchased.purchase && buyer.silver == 15, "owned unlock has no repeat purchase button");
    EndTextureMode();
    UnloadRenderTexture(purchaseTarget);

    ian::QuestDefinition rewardGoal{.id = "claim", .title = "Defeat 100 enemies",
        .conditions = {{ian::QuestMetric::EnemiesDefeated, ian::QuestScope::Lifetime, 100,
            ian::QuestComparison::AtLeast, {}, {}}}, .reward = {4, {}, 0}};
    ian::QuestSystem collector({rewardGoal});
    ian::MetaProgression wallet;
    collector.beginRun(wallet);
    const std::array rewardSignals{ian::QuestSignal{ian::QuestMetric::EnemiesDefeated, 100}};
    static_cast<void>(collector.record(wallet, rewardSignals));
    require(wallet.silver == 0, "finished quest does not credit before collect button is pressed");
    ian::QuestMenuState collecting;
    auto rewardTarget = LoadRenderTexture(1280, 720);
    ian::setLanguage(ian::Language::Russian);
    BeginTextureMode(rewardTarget);
    ClearBackground({61, 82, 66, 255});
    auto collected = ian::drawQuestMenu(ui, collector, wallet, collecting, {1280, 720},
        {.pointer = {1059, 228}, .click = true});
    require(collected.rewardClaimed && wallet.silver == 4 && wallet.quests.progress.at("claim").rewardClaimed &&
            collecting.rewardFlights.size() == 4, "collect button creates coin flights and one durable wallet credit");
    collected = ian::drawQuestMenu(ui, collector, wallet, collecting, {1280, 720},
        {.pointer = {1059, 228}, .click = true});
    require(!collected.rewardClaimed && wallet.silver == 4 && collecting.rewardFlights.size() == 4,
        "repeated click on collected row cannot duplicate credit or flight particles");
    ClearBackground({61, 82, 66, 255});
    static_cast<void>(ian::drawQuestMenu(ui, collector, wallet, collecting, {1280, 720}, {.deltaTime = 0.52F}));
    require(!collecting.rewardFlights.empty() && collecting.rewardFlights.front().age > 0.5F,
        "flight advances with elapsed seconds rather than draw count");
    EndTextureMode();
    if (argc > 1) {
        Image capture = LoadImageFromTexture(rewardTarget.texture);
        ImageFlipVertical(&capture);
        require(ExportImage(capture, (std::filesystem::path(argv[1]) / "quest_reward_flight.png").string().c_str()),
            "coin flight preview exports");
        UnloadImage(capture);
    }
    auto closing = collecting;
    BeginTextureMode(rewardTarget);
    const auto closed = ian::drawQuestMenu(ui, collector, wallet, closing, {1280, 720}, {.close = true});
    require(closed.close && closing.rewardFlights.empty() && wallet.silver == 4,
        "closing during animation preserves the claimed wallet credit");
    static_cast<void>(ian::drawQuestMenu(ui, collector, wallet, collecting, {1280, 720}, {.deltaTime = 2.0F}));
    require(collecting.rewardFlights.empty() && collecting.counterPulse > 0 && wallet.silver == 4,
        "all coin flights reach the counter and pulse without crediting a second time");
    EndTextureMode();
    UnloadRenderTexture(rewardTarget);
    ian::setLanguage(ian::Language::English);
    ui.shutdown();
    ian::shutdownUiText();
    CloseWindow();
}
