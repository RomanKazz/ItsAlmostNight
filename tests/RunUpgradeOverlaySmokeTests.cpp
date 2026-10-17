#include "TestHarness.hpp"
#include "ui/UiTomeIcon.hpp"
#include "RewardBackdropChecks.hpp"
#include "ShrinePromptChecks.hpp"
#include "NightForecastChecks.hpp"
#include "WeaponInventoryGraphicsChecks.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
#include "ui/UiText.hpp"
#include "ui/DebugUpgradeMenu.hpp"
#include "localization/Localization.hpp"

#include <array>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    require(!ian::debugUpgradeMatches(
        ian::RunUpgradeDefinitions[ian::runUpgradeIndex(ian::RunUpgradeEffect::RiskyInvestment)], {}),
        "high stakes is absent from the debug catalog");
    ian::SimulationSnapshot debugSnapshot{};
    for (const auto effect : {ian::RunUpgradeEffect::UpgradeAxe, ian::RunUpgradeEffect::UpgradePickaxe,
            ian::RunUpgradeEffect::UnlockRifle, ian::RunUpgradeEffect::UnlockClub,
            ian::RunUpgradeEffect::UnlockIceWand, ian::RunUpgradeEffect::UnlockFireWand}) {
        debugSnapshot.runUpgradeStacks[ian::runUpgradeIndex(effect)] = 1;
        require(!ian::debugUpgradeUnique(effect) && ian::debugUpgradeGrantable(effect, debugSnapshot),
            "debug catalog can grant another level of owned weapons and tools");
        debugSnapshot.runUpgradeStacks[ian::runUpgradeIndex(effect)] = ian::MaximumPlayerWeaponLevel;
        require(!ian::debugUpgradeGrantable(effect, debugSnapshot),
            "debug catalog respects the eight-level weapon limit");
    }
    require(ian::debugUpgradeUnique(ian::RunUpgradeEffect::UnlockBomb),
        "bomb acquisition remains unique in the debug catalog");
    using ian::app_detail::advanceRunUpgradeInputDelay;
    constexpr double quietDuration = ian::app_detail::RunUpgradeChoiceInputQuietDuration;
    double inputDelay = quietDuration;
    for (int frame = 0; frame < 180; ++frame) {
        inputDelay = advanceRunUpgradeInputDelay(inputDelay, false, 1.0 / 60.0);
        require(inputDelay > 0, "held attack never arms upgrade selection");
    }
    // Rapid attack clicks include release frames, so a release-only barrier
    // cannot prevent them from accidentally selecting a newly opened card.
    for (int frame = 0; frame < 180; ++frame) {
        inputDelay = advanceRunUpgradeInputDelay(inputDelay, frame % 6 != 0, 1.0 / 60.0);
        require(inputDelay > 0, "ongoing attack clicks never arm upgrade selection");
    }
    inputDelay = quietDuration;
    inputDelay = advanceRunUpgradeInputDelay(inputDelay, true, 2.0);
    require(inputDelay > 0, "a stalled opening frame cannot bypass the input guard");
    requireNear(advanceRunUpgradeInputDelay(inputDelay, true, -1.0), inputDelay, 1e-9,
        "negative frame time cannot affect the input guard");
    for (int frame = 0; frame < 30; ++frame)
        inputDelay = advanceRunUpgradeInputDelay(inputDelay, true, 1.0 / 60.0);
    requireNear(inputDelay, 0, 1e-9, "a short quiet interval enables a fresh upgrade press");
    inputDelay = advanceRunUpgradeInputDelay(quietDuration, true, 0.05);
    requireNear(advanceRunUpgradeInputDelay(inputDelay, false, 0.05), quietDuration, 1e-9,
        "another premature click restarts the quiet interval for queued upgrades and rerolls");

    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_HIGHDPI);
    InitWindow(64, 64, "Upgrade overlay smoke");
    if (!IsWindowReady()) return 77;
    ian::initializeLocalization("assets/data/localization.json");
    ian::initializeUiText();
    ian::GameUi ui;
    ui.initialize();
    checkNightForecast(ui,argc>1 ? argv[1] : nullptr);
    checkWeaponInventory(ui,argc>1 ? argv[1] : nullptr);
    checkShrineResourcePrompt(ui,argc>1 ? argv[1] : nullptr);
    const Texture2D venomIcon = ui.runUpgradeTexture(ian::RunUpgradeEffect::PoisonChance);
    require(IsTextureValid(venomIcon), "venom has its own inventory icon");
    Image venomImage = LoadImageFromTexture(venomIcon);
    int venomGreenPixels = 0;
    for (int y = 0; y < venomImage.height; ++y)
        for (int x = 0; x < venomImage.width; ++x) {
            const Color color = GetImageColor(venomImage, x, y);
            if (color.a > 200 && color.g > color.r + 20 && color.g > color.b + 20)
                ++venomGreenPixels;
        }
    require(GetImageColor(venomImage, 0, 0).a == 0 && venomGreenPixels > 3000,
        "venom tome has transparent padding and green cover");
    UnloadImage(venomImage);
    ian::SimulationSnapshot venomPreview{};
    venomPreview.runUpgradeChoiceCount = 1;
    venomPreview.runUpgradeChoices[0] = ian::RunUpgradeEffect::PoisonChance;
    venomPreview.runUpgradeRarities[0] = ian::RunUpgradeRarity::Legendary;
    const auto venomProjected = ian::app_detail::previewRunUpgrade(venomPreview, 0);
    requireNear(ian::runPoisonChance(venomProjected.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::PoisonChance)]),
        .3, 1e-12, "legendary venom preview shows its thirty percent chance");
    const Texture2D upgradeArrow = ui.coreMenuTexture(ian::CoreMenuTexture::UpgradeGreen);
    require(IsTextureValid(upgradeArrow) && upgradeArrow.width == 256 && upgradeArrow.height == 256,
        "building upgrade arrow loads as a 256x256 icon");
    Image arrowImage = LoadImageFromTexture(upgradeArrow);
    const Color arrowCenter = GetImageColor(arrowImage, 128, 128);
    require(GetImageColor(arrowImage, 0, 0).a == 0 && arrowCenter.a >= 250 &&
        arrowCenter.g > arrowCenter.r * 2 && arrowCenter.g > arrowCenter.b,
        "building upgrade arrow has a green face and transparent background");
    UnloadImage(arrowImage);
    for (const auto& tome : ian::RunTomeDefinitions) {
        if (!ian::retiredRunUpgrade(tome.effect)) {
            const char* path = ian::runTomeIconPath(tome.effect);
            require(path && FileExists(path), "every active tome ships its generated PNG");
        }
        const auto icon = ui.runUpgradeTexture(tome.effect);
        require(icon.width == 256 && icon.height == 256, "tomes share cached book artwork dimensions");
        Image image = LoadImageFromTexture(icon);
        int opaquePixels = 0;
        for (int y = 0; y < image.height; ++y)
            for (int x = 0; x < image.width; ++x)
                if (GetImageColor(image, x, y).a > 200) ++opaquePixels;
        require(GetImageColor(image, 0, 0).a == 0 &&
            GetImageColor(image, 255, 255).a == 0 && opaquePixels > 15000,
            "book covers render with transparent outer padding");
        UnloadImage(image);
    }
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
    {
        auto shop = snapshot;
        shop.runUpgradeChoicePending = false;
        shop.runShop.pending = true;
        shop.runShop.wave = 3;
        shop.coins = 35;
        const std::array shopCards{ian::RunUpgradeEffect::Damage,ian::RunUpgradeEffect::MoveSpeed,
            ian::RunUpgradeEffect::UnlockQuarry,ian::RunUpgradeEffect::PoisonChance};
        for (std::size_t i=0;i<4;++i) {
            shop.runShop.offers[i] = {.effect=shopCards[i],.rarity=static_cast<ian::RunUpgradeRarity>(i),
                .price=12+static_cast<int>(i)*15,.present=true,.locked=i==1};
            shop.runShopBuyable[i] = true;
        }
        for (const auto language : {ian::Language::English,ian::Language::Russian}) {
            ian::setLanguage(language);
            SetWindowSize(1920,1080); PollInputEvents();
            const auto layout = ian::app_detail::runShopLayout();
            for (std::size_t i=0;i<4;++i) {
                require(layout.cards[i].width > 0 && layout.cards[i].x >= 0 &&
                    layout.cards[i].x+layout.cards[i].width <= static_cast<float>(GetScreenWidth()),"all four shop cards fit the window");
                require(CheckCollisionRecs(layout.cards[i],layout.buy[i]) &&
                    !CheckCollisionRecs(layout.cards[i],layout.locks[i]),"price and lock hit areas match card layout");
                for (std::size_t j=i+1;j<4;++j)
                    require(!CheckCollisionRecs(layout.cards[i],layout.cards[j]),"shop cards never overlap");
            }
            const auto target = LoadRenderTexture(1920,1080);
            for (int frame=0;frame<3;++frame) {
                BeginTextureMode(target); ClearBackground({40,70,65,255});
                ian::app_detail::drawRunShopOverlay(ui,shop,std::nullopt,Vector2{-100,-100});
                EndTextureMode();
            }
            Image image = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&image);
            if (argc > 1) {
                const std::string path = std::string(argv[1]) +
                    (language == ian::Language::English ? "/shop-en.png" : "/shop-ru.png");
                require(ExportImage(image,path.c_str()),"shop capture exported");
            }
            const auto buy = layout.buy[3];
            const int sampleX = static_cast<int>(buy.x+12*layout.scale);
            const int sampleY = static_cast<int>(buy.y+buy.height*.5F);
            const Color disabledFill = GetImageColor(image,sampleX,sampleY);
            auto godShop = shop;
            godShop.unlimitedResources = true; godShop.coins = 0;
            BeginTextureMode(target); ClearBackground({40,70,65,255});
            ian::app_detail::drawRunShopOverlay(ui,godShop,std::nullopt,Vector2{-100,-100});
            EndTextureMode();
            Image godImage = LoadImageFromTexture(target.texture); ImageFlipVertical(&godImage);
            const Color enabledFill = GetImageColor(godImage,sampleX,sampleY);
            require(enabledFill.r != disabledFill.r || enabledFill.g != disabledFill.g || enabledFill.b != disabledFill.b,
                "godmode displays purchasable cards as enabled even with zero coins");
            if (argc > 1) {
                const std::string path = std::string(argv[1]) +
                    (language == ian::Language::English ? "/shop-god-en.png" : "/shop-god-ru.png");
                require(ExportImage(godImage,path.c_str()),"godmode shop capture exported");
            }
            UnloadImage(godImage);
            UnloadImage(image);
            UnloadRenderTexture(target);
        }
        ian::setLanguage(ian::Language::English);
        checkRewardBackdrops(ui,shop,argc>1?argv[1]:nullptr);
    }
    // Compare the readonly projection with real upgrade application, including
    // class bonuses, accumulated upgrades, rarity scaling and discrete caps.
    auto simulation = std::make_unique<ian::Simulation>();
    simulation->startRunFromSeed(ian::PlayerClass::Engineer, 12345);
    for (const auto effect : {ian::RunUpgradeEffect::Damage, ian::RunUpgradeEffect::AttackSpeed,
         ian::RunUpgradeEffect::MoveSpeed, ian::RunUpgradeEffect::MaximumHealth,
         ian::RunUpgradeEffect::RecoverableArmor, ian::RunUpgradeEffect::BuildingHealth,
         ian::RunUpgradeEffect::DefenseDamage, ian::RunUpgradeEffect::DefenseFireRate,
         ian::RunUpgradeEffect::ProductionSpeed, ian::RunUpgradeEffect::RichResources,
         ian::RunUpgradeEffect::Luck, ian::RunUpgradeEffect::GatheringSpeed,
         ian::RunUpgradeEffect::TwinBatteries, ian::RunUpgradeEffect::GunPiercing,
         ian::RunUpgradeEffect::Ricochet, ian::RunUpgradeEffect::ClusterPayload,
         ian::RunUpgradeEffect::Damage, ian::RunUpgradeEffect::BuildingHealth,
         ian::RunUpgradeEffect::TwinBatteries, ian::RunUpgradeEffect::Regeneration, ian::RunUpgradeEffect::CannonHeavyShell,
         ian::RunUpgradeEffect::UnlockClub, ian::RunUpgradeEffect::UnlockRifle,
         ian::RunUpgradeEffect::UnlockBomb, ian::RunUpgradeEffect::UnlockIceWand,
         ian::RunUpgradeEffect::UnlockFireWand}) {
        auto before = simulation->snapshot();
        before.runUpgradeChoiceCount = 1;
        before.runUpgradeChoices[0] = effect;
        before.runUpgradeRarities[0] = ian::RunUpgradeRarity::Rare;
        const auto projected = ian::app_detail::previewRunUpgrade(before, 0);
        require(ian::runUpgradeWeapon(effect)
                ? simulation->grantDebugRunUpgradeBatch(effect, 1, ian::RunUpgradeRarity::Rare)
                : simulation->grantDebugRunUpgrade(effect, ian::RunUpgradeRarity::Rare),
            "preview fixture applies the real card, using explicit debug access for the complete weapon catalog");
        const auto actual = simulation->snapshot();
        requireNear(projected.playerDamageMultiplier, actual.playerDamageMultiplier, 1e-9, "damage preview matches simulation");
        requireNear(projected.playerMoveSpeedMultiplier, actual.playerMoveSpeedMultiplier, 1e-9, "movement preview matches simulation");
        requireNear(projected.playerMaxHealth, actual.playerMaxHealth, 1e-9, "HP preview matches simulation");
        requireNear(projected.playerMaxRecoverableArmor, actual.playerMaxRecoverableArmor, 1e-9, "shield preview matches simulation");
        requireNear(projected.upgradeStats.buildingHealth, actual.upgradeStats.buildingHealth, 1e-9, "class building HP preview matches simulation");
        requireNear(projected.upgradeStats.productionSpeed, actual.upgradeStats.productionSpeed, 1e-9, "production preview matches simulation");
        requireNear(projected.upgradeStats.toolAttackSpeed, actual.upgradeStats.toolAttackSpeed, 1e-9, "tool speed preview matches simulation");
        requireNear(projected.upgradeStats.playerAttackSpeed, actual.upgradeStats.playerAttackSpeed, 1e-9, "attack speed preview matches simulation");
        requireNear(projected.upgradeStats.resourceYield, actual.upgradeStats.resourceYield, 1e-9, "meta/class yield preview matches simulation");
        requireNear(projected.upgradeStats.resourceCapacity, actual.upgradeStats.resourceCapacity, 1e-9,"capacity tome preview matches simulation");
        require(projected.woodCapacity==actual.woodCapacity && projected.stoneCapacity==actual.stoneCapacity &&
            projected.crystalCapacity==actual.crystalCapacity,"world capacity preview leaves carrying capacity unchanged");
        requireNear(projected.upgradeStats.defenseDamage, actual.upgradeStats.defenseDamage, 1e-9, "defense damage preview matches simulation");
        requireNear(projected.upgradeStats.defenseFireRate, actual.upgradeStats.defenseFireRate, 1e-9, "defense rate preview matches simulation");
        requireNear(projected.upgradeStats.luck, actual.upgradeStats.luck, 1e-9, "luck preview matches simulation");
        requireNear(projected.upgradeStats.healthRegeneration, actual.upgradeStats.healthRegeneration, 1e-9, "regen hammer preview matches simulation");
        require(projected.unlockedWeapons == actual.unlockedWeapons &&
            projected.bombsRemaining == actual.bombsRemaining, "weapon blueprint preview matches actual acquisition");
        require(projected.coreBuildRadius == actual.coreBuildRadius && projected.runUpgradePower == actual.runUpgradePower &&
            projected.upgradeStats.sideShots == actual.upgradeStats.sideShots &&
            projected.upgradeStats.clusterExplosions == actual.upgradeStats.clusterExplosions &&
            projected.upgradeStats.cannonAttackStyle == actual.upgradeStats.cannonAttackStyle,
            "preview matches real caps and specialization");
        require(before.runUpgradePower[ian::runUpgradeIndex(effect)] +
            ian::runUpgradeAppliedPowerGain(effect, ian::RunUpgradeRarity::Rare, before.runUpgradePower[ian::runUpgradeIndex(effect)]) ==
            projected.runUpgradePower[ian::runUpgradeIndex(effect)], "preview preserves its input snapshot");
    }
    {
        auto defense = snapshot;
        defense.runUpgradeChoiceCount = 1;
        defense.runUpgradeRarities.fill(ian::RunUpgradeRarity::Common);
        defense.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockGunTurret)] = 2;
        defense.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockGunTurret)] = 2;
        defense.buildingArsenalCount = 1;
        defense.runUpgradeChoices[0] = ian::RunUpgradeEffect::UnlockGunTurret;
        auto projected = ian::app_detail::previewRunUpgrade(defense, 0);
        require(projected.buildingArsenalCount == 1 &&
            projected.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockGunTurret)] == 3,
            "repeated defense preview raises level without consuming another slot");
        defense.runUpgradeChoices[0] = ian::RunUpgradeEffect::UnlockCatapult;
        defense.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockCatapult)] = 0;
        defense.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockCatapult)] = 0;
        projected = ian::app_detail::previewRunUpgrade(defense, 0);
        require(projected.buildingArsenalCount == 2 &&
            projected.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockCatapult)] == 1,
            "new defense preview adds one type slot at level one");
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
            for (int scene = 0; scene < 7; ++scene) {
                auto display = snapshot;
                std::array<float, 4> displayHovers{};
                Vector2 pointer{-100, -100};
                if (scene < 2) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::ReactiveTraps,
                        ian::RunUpgradeEffect::RecoverableArmor, ian::RunUpgradeEffect::UnlockClub,
                        ian::RunUpgradeEffect::Luck};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Epic, ian::RunUpgradeRarity::Uncommon};
                    display.upgradeStats.luck = 0;
                }
                if (scene == 3) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::UnlockLumberMill,
                        ian::RunUpgradeEffect::UnlockQuarry, ian::RunUpgradeEffect::UnlockCrystalMine,
                        ian::RunUpgradeEffect::ProductionSpeed};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Rare};
                    require(ian::app_detail::previewRunUpgrade(display, 0).buildingArsenalCount == display.buildingArsenalCount + 1,
                        "economy blueprint preview consumes a shared structure slot");
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
                if (scene == 5) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::AttackSpeed,
                        ian::RunUpgradeEffect::Luck, ian::RunUpgradeEffect::GatheringSpeed,
                        ian::RunUpgradeEffect::Damage};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Uncommon, ian::RunUpgradeRarity::Rare, ian::RunUpgradeRarity::Epic};
                }
                if (scene == 6) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::UnlockRifle,
                        ian::RunUpgradeEffect::UnlockBomb, ian::RunUpgradeEffect::UnlockIceWand,
                        ian::RunUpgradeEffect::UnlockFireWand};
                    display.runUpgradeRarities.fill(ian::RunUpgradeRarity::Common);
                }
                if (scene == 8) {
                    display.playerClass = ian::PlayerClass::Vanguard;
                    display.sandboxMode = display.unlimitedResources = false;
                    display.runUpgradeStacks.fill(0);
                    display.runUpgradePower.fill(0);
                    display.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockGunTurret)] = 2;
                    display.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockGunTurret)] = 2;
                    display.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockCannon)] = 5;
                    display.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockCannon)] = 5;
                    display.buildingArsenalCount = 2;
                    display.unlockedBuildings.fill(false);
                    display.unlockedBuildings[static_cast<std::size_t>(ian::BuildingType::GunTurret)] = true;
                    display.unlockedBuildings[static_cast<std::size_t>(ian::BuildingType::Cannon)] = true;
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::UnlockGunTurret,
                        ian::RunUpgradeEffect::UnlockCannon, ian::RunUpgradeEffect::UnlockCatapult,
                        ian::RunUpgradeEffect::UnlockSlowTrap};
                    display.runUpgradeRarities.fill(ian::RunUpgradeRarity::Common);
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
                        (scene == 0 ? "" : scene == 1 ? "_luck" : scene == 2 ? "_heavy" : scene == 3 ? "_economy" : scene == 4 ? "_hammers" : scene == 5 ? "_numeric_hammers" : "_weapons") + ".png";
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
