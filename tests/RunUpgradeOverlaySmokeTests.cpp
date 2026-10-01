#include "TestHarness.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
#include "ui/UiText.hpp"
#include "localization/Localization.hpp"

#include <array>
#include <string>

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_HIGHDPI);
    InitWindow(64, 64, "Upgrade overlay smoke");
    if (!IsWindowReady()) return 77;
    ian::initializeLocalization("assets/data/localization.json");
    ian::initializeUiText();
    ian::GameUi ui;
    ui.initialize();
    ian::SimulationSnapshot snapshot{};
    snapshot.runUpgradeChoicePending = true;
    snapshot.runUpgradeChoiceCount = 4;
    snapshot.runUpgradeChoices = {
        ian::RunUpgradeEffect::UnlockCannon, ian::RunUpgradeEffect::CannonHeavyShell,
        ian::RunUpgradeEffect::CannonScattershot, ian::RunUpgradeEffect::CannonClusterShell};
    snapshot.runUpgradeRarities = {
        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Rare,
        ian::RunUpgradeRarity::Rare, ian::RunUpgradeRarity::Rare};
    snapshot.buildingArsenalCount = 1;
    snapshot.playerMaxHealth = 135;
    snapshot.playerMaxRecoverableArmor = 25;
    snapshot.playerDamageMultiplier = 1.25;
    snapshot.playerMoveSpeedMultiplier = 1.12;
    snapshot.coreBuildRadius = 24;
    snapshot.upgradeStats.luck = 0.2;
    const std::array<float, 4> hovers{};
    for (const ian::Language language : {ian::Language::English, ian::Language::Russian}) {
        ian::setLanguage(language);
        for (const Vector2 resolution : {Vector2{1920, 1080}, Vector2{1280, 720}}) {
            const int width = static_cast<int>(resolution.x);
            const int height = static_cast<int>(resolution.y);
            SetWindowSize(width, height);
            PollInputEvents();
            const Rectangle panel = ian::app_detail::runUpgradeStatsPanelBounds(4);
            for (std::size_t index = 0; index < 4; ++index) {
                const Rectangle card = ian::app_detail::runUpgradeCardBounds(index, 4);
                require(card.x >= 0 && card.y >= 0 && card.y + card.height < resolution.y &&
                    card.x + card.width < panel.x,
                    "cards fit on screen without overlapping the stats panel");
                require(ian::app_detail::hoveredRunUpgradeChoice(snapshot,
                    {card.x + card.width * 0.5F, card.y + card.height * 0.5F}) == index,
                    "resized card hit areas track their visible bounds");
            }
            RenderTexture2D target = LoadRenderTexture(width, height);
            BeginTextureMode(target);
            ClearBackground({66, 96, 70, 255});
            ian::app_detail::drawRunUpgradeOverlay(ui, snapshot, 2.0, 0.0,
                std::nullopt, hovers);
            EndTextureMode();
            if (argc > 1) {
                Image capture = LoadImageFromTexture(target.texture);
                ImageFlipVertical(&capture);
                const std::string path = std::string(argv[1]) + "/ian_upgrades_" +
                    std::to_string(height) + (language == ian::Language::Russian ? "_ru" : "") + ".png";
                require(ExportImage(capture, path.c_str()), "upgrade screenshot exported");
                UnloadImage(capture);
            }
            UnloadRenderTexture(target);
        }
    }
    ian::setLanguage(ian::Language::English);
    ui.shutdown();
    ian::shutdownUiText();
    CloseWindow();
}
