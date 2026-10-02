#include "TestHarness.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
#include "ui/UiText.hpp"
#include "localization/Localization.hpp"

#include <array>
#include <memory>
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
    // Compare the readonly projection with real upgrade application, including
    // class bonuses, accumulated upgrades, rarity scaling and discrete caps.
    auto simulation = std::make_unique<ian::Simulation>();
    simulation->startRunFromSeed(ian::PlayerClass::Engineer, 12345);
    for (const auto effect : {ian::RunUpgradeEffect::Damage, ian::RunUpgradeEffect::AttackSpeed,
         ian::RunUpgradeEffect::MoveSpeed, ian::RunUpgradeEffect::MaximumHealth,
         ian::RunUpgradeEffect::RecoverableArmor, ian::RunUpgradeEffect::BuildingHealth,
         ian::RunUpgradeEffect::DefenseDamage, ian::RunUpgradeEffect::DefenseFireRate,
         ian::RunUpgradeEffect::ProductionSpeed, ian::RunUpgradeEffect::RichResources,
         ian::RunUpgradeEffect::BuildRadius, ian::RunUpgradeEffect::Luck,
         ian::RunUpgradeEffect::TwinBatteries, ian::RunUpgradeEffect::GunPiercing,
         ian::RunUpgradeEffect::Ricochet, ian::RunUpgradeEffect::ClusterPayload,
         ian::RunUpgradeEffect::Damage, ian::RunUpgradeEffect::BuildingHealth,
         ian::RunUpgradeEffect::TwinBatteries, ian::RunUpgradeEffect::Regeneration, ian::RunUpgradeEffect::CannonHeavyShell}) {
        auto before = simulation->snapshot();
        before.runUpgradeChoiceCount = 1;
        before.runUpgradeChoices[0] = effect;
        before.runUpgradeRarities[0] = ian::RunUpgradeRarity::Rare;
        const auto projected = ian::app_detail::previewRunUpgrade(before, 0);
        require(simulation->grantDebugRunUpgrade(effect, ian::RunUpgradeRarity::Rare),
            "preview fixture applies the real card");
        const auto actual = simulation->snapshot();
        requireNear(projected.playerDamageMultiplier, actual.playerDamageMultiplier, 1e-9, "damage preview matches simulation");
        requireNear(projected.playerMoveSpeedMultiplier, actual.playerMoveSpeedMultiplier, 1e-9, "movement preview matches simulation");
        requireNear(projected.playerMaxHealth, actual.playerMaxHealth, 1e-9, "HP preview matches simulation");
        requireNear(projected.playerMaxRecoverableArmor, actual.playerMaxRecoverableArmor, 1e-9, "shield preview matches simulation");
        requireNear(projected.upgradeStats.buildingHealth, actual.upgradeStats.buildingHealth, 1e-9, "class building HP preview matches simulation");
        requireNear(projected.upgradeStats.productionSpeed, actual.upgradeStats.productionSpeed, 1e-9, "production preview matches simulation");
        requireNear(projected.upgradeStats.playerAttackSpeed, actual.upgradeStats.playerAttackSpeed, 1e-9, "attack speed preview matches simulation");
        requireNear(projected.upgradeStats.resourceYield, actual.upgradeStats.resourceYield, 1e-9, "yield preview matches simulation");
        requireNear(projected.upgradeStats.defenseDamage, actual.upgradeStats.defenseDamage, 1e-9, "defense damage preview matches simulation");
        requireNear(projected.upgradeStats.defenseFireRate, actual.upgradeStats.defenseFireRate, 1e-9, "defense rate preview matches simulation");
        requireNear(projected.upgradeStats.luck, actual.upgradeStats.luck, 1e-9, "luck preview matches simulation");
        requireNear(projected.upgradeStats.healthRegeneration, actual.upgradeStats.healthRegeneration, 1e-9, "regen hammer preview matches simulation");
        require(projected.coreBuildRadius == actual.coreBuildRadius && projected.runUpgradePower == actual.runUpgradePower &&
            projected.upgradeStats.sideShots == actual.upgradeStats.sideShots &&
            projected.upgradeStats.clusterExplosions == actual.upgradeStats.clusterExplosions &&
            projected.upgradeStats.cannonAttackStyle == actual.upgradeStats.cannonAttackStyle,
            "preview matches real caps and specialization");
        require(before.runUpgradePower[ian::runUpgradeIndex(effect)] +
            ian::runUpgradeAppliedPowerGain(effect, ian::RunUpgradeRarity::Rare, before.runUpgradePower[ian::runUpgradeIndex(effect)]) ==
            projected.runUpgradePower[ian::runUpgradeIndex(effect)], "preview preserves its input snapshot");
    }
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
            for (int scene = 0; scene < 5; ++scene) {
                auto display = snapshot;
                std::array<float, 4> displayHovers{};
                Vector2 pointer{-100, -100};
                if (scene < 2) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::ReactiveTraps,
                        ian::RunUpgradeEffect::RecoverableArmor, ian::RunUpgradeEffect::DoubleDown,
                        ian::RunUpgradeEffect::Luck};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Epic, ian::RunUpgradeRarity::Uncommon};
                    display.upgradeStats.luck = 0;
                }
                if (scene == 3) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::UnlockLumberMill,
                        ian::RunUpgradeEffect::UnlockQuarry, ian::RunUpgradeEffect::UnlockCrystalMine,
                        ian::RunUpgradeEffect::LumberMillProduction};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Rare};
                    require(ian::app_detail::previewRunUpgrade(display, 0).buildingArsenalCount == display.buildingArsenalCount,
                        "economy blueprint preview leaves combat slots unchanged");
                }
                if (scene == 4) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::DefenseDamage,
                        ian::RunUpgradeEffect::DefenseFireRate, ian::RunUpgradeEffect::BuildingHealth,
                        ian::RunUpgradeEffect::Regeneration};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Rare,
                        ian::RunUpgradeRarity::Uncommon, ian::RunUpgradeRarity::Epic, ian::RunUpgradeRarity::Legendary};
                    for (const auto& hammer : ian::RunHammerDefinitions) {
                        display.runUpgradeStacks[ian::runUpgradeIndex(hammer.effect)] = 2;
                        display.runUpgradePower[ian::runUpgradeIndex(hammer.effect)] = 3.0;
                    }
                    display.upgradeStats.healthRegeneration = 1.5;
                }
                if (scene > 0) {
                    const std::size_t choice = scene == 2 ? 1U : 3U;
                    const auto card = ian::app_detail::runUpgradeCardBounds(choice, 4);
                    pointer = {card.x + card.width * 0.5F, card.y + card.height * 0.5F};
                    displayHovers[choice] = 1.0F;
                }
                RenderTexture2D target = LoadRenderTexture(width, height);
                BeginTextureMode(target);
                ClearBackground({66, 96, 70, 255});
                ian::app_detail::drawRunUpgradeOverlay(ui, display, 2.0, 0.0,
                    std::nullopt, displayHovers, pointer);
                EndTextureMode();
                if (argc > 1) {
                    Image capture = LoadImageFromTexture(target.texture);
                    ImageFlipVertical(&capture);
                    const std::string path = std::string(argv[1]) + "/ian_upgrades_" +
                        std::to_string(height) + (language == ian::Language::Russian ? "_ru" : "") +
                        (scene == 0 ? "" : scene == 1 ? "_luck" : scene == 2 ? "_heavy" : scene == 3 ? "_economy" : "_hammers") + ".png";
                    require(ExportImage(capture, path.c_str()), "upgrade screenshot exported");
                    UnloadImage(capture);
                }
                UnloadRenderTexture(target);
            }
        }
    }
    ian::setLanguage(ian::Language::English);
    ui.shutdown();
    ian::shutdownUiText();
    CloseWindow();
}
