#include "TestHarness.hpp"
#include "ui/UiTomeIcon.hpp"
#include "ui/HudHotbar.hpp"
#include "RewardBackdropChecks.hpp"
#include "ShrinePromptChecks.hpp"
#include "BuildingHoverChecks.hpp"
#include "NightForecastChecks.hpp"
#include "WeaponInventoryGraphicsChecks.hpp"
#include "app/AppRunUpgradeOverlay.hpp"
#include "ui/UiText.hpp"
#include "ui/UiUpgradeDescriptions.hpp"
#include "ui/DebugUpgradeMenu.hpp"
#include "progression/Shrine.hpp"
#include "localization/Localization.hpp"

#include <array>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    {
        constexpr auto type=ian::BuildingType::TeslaTower;
        ian::SimulationSnapshot state{};
        const auto effect=*ian::buildingRunUpgrade(type);
        state.buildingBlueprintLevels[static_cast<std::size_t>(type)]=1;
        state.runUpgradeStacks[ian::runUpgradeIndex(effect)]=1;
        std::uint64_t seed=0;
        for(;seed<1000;++seed) {
            const auto roll=ian::rollBuildingStatUpgrade(type,ian::RunUpgradeRarity::Common,seed);
            if(std::find(roll.stats.begin(),roll.stats.begin()+roll.count,ian::AttackStat::Quantity)!=roll.stats.begin()+roll.count) break;
        }
        require(seed<1000,"Tesla preview rolls an attack-count card");
        auto preview=ian::app_detail::buildingAttackCountUpgradePreview(state,type,ian::RunUpgradeRarity::Common,seed);
        require(preview.first==1 && preview.second==2,"Tesla attack-count card starts at one rather than zero");
        state.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::ProjectileCount)]=2;
        state.buildingUpgradePowers[static_cast<std::size_t>(type)][static_cast<std::size_t>(ian::AttackStat::Quantity)]=1.4;
        const auto global=ian::shrineDefenseAttackModifiers(state.runUpgradePower,state.shrines);
        auto tower=std::make_unique<ian::TowerSystem>();tower->setAttackModifiers(global);
        std::array<ian::AttackModifiers,ian::GameBalance::BuildingTypeCount> local{};
        local[static_cast<std::size_t>(type)]=ian::buildingStatUpgradeModifiers(type,state.buildingUpgradePowers);
        tower->setBuildingUpgradeModifiers(local);
        const ian::BuildingInstance building{.type=type};
        preview=ian::app_detail::buildingAttackCountUpgradePreview(state,type,ian::RunUpgradeRarity::Common,seed);
        requireNear(preview.first,ian::effectiveAttackCount(tower->finalStats(building).quantity,1),1e-9,
            "Tesla preview includes existing global and local bonuses");
        require(preview.second==preview.first+1,"Tesla count upgrade matches the added attack");
    }
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
    // Compare the displayed distances against the real combat-system resolvers,
    // including native level growth, prior range cards and size-linked reach.
    for(const auto type:{ian::BuildingType::Turret,ian::BuildingType::Catapult,ian::BuildingType::Mortar,
            ian::BuildingType::Flamethrower,ian::BuildingType::TeslaTower,ian::BuildingType::WindBlower,
            ian::BuildingType::SlowTrap}) {
        for(const int level:{1,4,20}) for(const auto rarity:{ian::RunUpgradeRarity::Common,ian::RunUpgradeRarity::Legendary}) {
            ian::SimulationSnapshot snapshot{};
            const auto effect=*ian::buildingRunUpgrade(type);const auto index=static_cast<std::size_t>(type);
            snapshot.runUpgradeStacks[ian::runUpgradeIndex(effect)]=level;
            snapshot.buildingBlueprintLevels[index]=static_cast<std::uint8_t>(level);
            snapshot.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::AttackSize)]=2;
            snapshot.shrines.buildingPowers[ian::runUpgradeIndex(ian::RunUpgradeEffect::AttackSize)]=1;
            snapshot.buildingUpgradePowers[index][static_cast<std::size_t>(ian::AttackStat::Range)]=level==1?0:2;
            snapshot.buildingUpgradePowers[index][static_cast<std::size_t>(ian::AttackStat::Size)]=level==1?0:1;
            std::uint64_t seed=0;ian::BuildingStatRoll roll;
            for(;seed<1000;++seed) {
                roll=ian::rollBuildingStatUpgrade(type,rarity,seed);
                if(std::find(roll.stats.begin(),roll.stats.begin()+roll.count,ian::AttackStat::Range)!=roll.stats.begin()+roll.count) break;
            }
            require(seed<1000,"range preview fixture obtains a real range card");
            const auto preview=ian::app_detail::buildingRangeUpgradePreview(snapshot,type,rarity,seed);
            const auto actual=[&](const ian::BuildingUpgradePowers& powers,int tier) {
                const auto global=ian::shrineDefenseAttackModifiers(snapshot.runUpgradePower,snapshot.shrines);
                std::array<ian::AttackModifiers,ian::GameBalance::BuildingTypeCount> local{};
                local[index]=ian::buildingStatUpgradeModifiers(type,powers);
                const ian::BuildingInstance building{.type=type,.level=static_cast<std::uint8_t>(tier)};
                ian::FinalAttackStats stats;
                if(type==ian::BuildingType::SlowTrap) {
                    auto system=std::make_unique<ian::TrapSystem>();
                    system->setAttackModifiers(global);system->setBuildingUpgradeModifiers(local);stats=system->finalStats(building);
                } else if(type==ian::BuildingType::Catapult || type==ian::BuildingType::Mortar) {
                    auto system=std::make_unique<ian::CannonSystem>();
                    system->setAttackModifiers(global);system->setBuildingUpgradeModifiers(local);stats=system->finalStats(building);
                } else {
                    auto system=std::make_unique<ian::TowerSystem>();
                    system->setAttackModifiers(global);system->setBuildingUpgradeModifiers(local);stats=system->finalStats(building);
                }
                return stats.range*((type==ian::BuildingType::Flamethrower || type==ian::BuildingType::WindBlower ||
                    type==ian::BuildingType::SlowTrap)?stats.size:1);
            };
            auto upgraded=snapshot.buildingUpgradePowers;ian::applyBuildingStatUpgrade(upgraded,type,roll,rarity);
            require(preview.first>0 && preview.second>preview.first,"range cards show real positive distances from the first upgrade");
            requireNear(preview.first,actual(snapshot.buildingUpgradePowers,level),1e-9,"current card range matches actual combat reach");
            requireNear(preview.second,actual(upgraded,level+1),1e-9,"upgraded card range matches the complete offered roll in combat");
        }
    }
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
    for (const auto& definition : ian::RunUpgradeDefinitions) {
        if (!ian::runUpgradeBuilding(definition.effect) && !ian::runUpgradeWeapon(definition.effect)) continue;
        const auto role = ian::runUpgradeRoleDescription(definition.effect);
        require(!role.empty(), "each building and weapon has a concise acquisition description");
        ian::setLanguage(ian::Language::Russian);
        require(ian::localizeText(role) != role, "acquisition descriptions have Russian translations");
    }
    ian::setLanguage(ian::Language::English);
    ian::initializeUiText();
    ian::GameUi ui;
    ui.initialize();
    checkNightForecast(ui,argc>1 ? argv[1] : nullptr);
    checkWeaponInventory(ui,argc>1 ? argv[1] : nullptr);
    checkShrineResourcePrompt(ui,argc>1 ? argv[1] : nullptr);
    if (argc > 1) {
        SetWindowSize(1280, 800);
        auto hintSnapshot = ian::SimulationSnapshot{};
        hintSnapshot.state = ian::RunState::Gathering;
        hintSnapshot.tutorialObjective = ian::TutorialObjective::MineWood;
        hintSnapshot.playerMaxHealth = hintSnapshot.playerHealth = 100;
        const Camera3D camera{{0, 2, 5}, {0, 1, 0}, {0, 1, 0}, 60, CAMERA_PERSPECTIVE};
        const auto target = LoadRenderTexture(1280, 800);
        for (int stage = 0; stage < 3; ++stage) {
            ian::HudViewState view{};
            view.minimapHidden = true;
            view.tutorialHint = ian::TutorialHintVisual{
                .objective = stage == 2 ? ian::TutorialObjective::OpenBuildMode : ian::TutorialObjective::MineWood,
                .opacity = stage == 2 ? .65F : 1.0F,
                .slideX = stage == 2 ? 8.0F : 0.0F,
                .slideY = stage == 2 ? 2.0F : 0.0F,
                .checkProgress = stage == 1 ? 1.0F : 0.0F,
                .completionPulse = stage == 1 ? .5F : 0.0F,
            };
            BeginTextureMode(target); ClearBackground({91, 132, 70, 255});
            ian::drawHud(ui, hintSnapshot, view, camera, {});
            EndTextureMode();
            auto capture = LoadImageFromTexture(target.texture); ImageFlipVertical(&capture);
            ExportImage(capture, (std::string(argv[1]) + "/tutorial-hint-" + std::to_string(stage) + ".png").c_str());
            UnloadImage(capture);
        }
        UnloadRenderTexture(target);
    }

    if (argc > 1) {
        ian::SimulationSnapshot items{};
        items.playerMaxRecoverableArmor = 25;
        const std::array effects{ian::LootUpgradeEffect::Acorn, ian::LootUpgradeEffect::CrystalPan,
            ian::LootUpgradeEffect::RepairNail, ian::LootUpgradeEffect::LumberBoot,
            ian::LootUpgradeEffect::Abacus, ian::LootUpgradeEffect::WeaponToken,
            ian::LootUpgradeEffect::GhostCandle, ian::LootUpgradeEffect::CrystalSafe};
        int stacks = 1;
        for (const auto effect : effects) items.lootStacks[ian::lootUpgradeIndex(effect)] = stacks++;
        SetWindowSize(1280, 720);
        const auto target = LoadRenderTexture(1280, 720);
        BeginTextureMode(target);
        ClearBackground({91, 132, 70, 255});
        ian::hud_detail::drawLootInventory(ui, items, false, false);
        EndTextureMode();
        auto capture = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&capture);
        ExportImage(capture, (std::string(argv[1]) + "/collected-items.png").c_str());
        UnloadImage(capture); UnloadRenderTexture(target);
    }

    checkBuildingHoverUi(ui,argc>1 ? argv[1] : nullptr);
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
        .2, 1e-12, "legendary venom preview shows its twenty percent chance");
    ian::SimulationSnapshot capacityPreview{};
    {
        ian::SimulationSnapshot xpPreview{};
        xpPreview.runUpgradeChoiceCount = 1;
        xpPreview.runUpgradeChoices[0] = ian::RunUpgradeEffect::ExperienceGain;
        xpPreview.runUpgradeRarities[0] = ian::RunUpgradeRarity::Rare;
        xpPreview.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::ExperienceGain)] = 1;
        xpPreview.upgradeStats.experienceGain = 1.20;
        requireNear(ian::app_detail::previewRunUpgrade(xpPreview, 0).upgradeStats.experienceGain,
            1.34, 1e-12, "experience preview adds rarity power to an existing tome and shrine bonus");
        xpPreview.shrines.offer = ian::ShrineKind::Player;
        requireNear(ian::app_detail::previewRunUpgrade(xpPreview, 0).upgradeStats.experienceGain,
            1.34, 1e-12, "experience shrine preview uses the same additive bonus");
    }
    capacityPreview.totalBuildingLimit = 4; capacityPreview.buildingLimitBonus = 1;
    capacityPreview.runUpgradeChoiceCount = 1;
    capacityPreview.runUpgradeChoices[0] = ian::RunUpgradeEffect::BuildingCapacity;
    capacityPreview.runUpgradeRarities[0] = ian::RunUpgradeRarity::Rare;
    capacityPreview.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::BuildingCapacity)] = 1.6;
    require(ian::app_detail::previewRunUpgrade(capacityPreview, 0).totalBuildingLimit == 6,
        "capacity preview floors accumulated fractional power, matching gameplay");
    capacityPreview.shrines.offer = ian::ShrineKind::Buildings;
    require(ian::app_detail::previewRunUpgrade(capacityPreview, 0).totalBuildingLimit == 5,
        "shrine capacity preview always adds exactly one place");
    const Texture2D upgradeArrow = ui.coreMenuTexture(ian::CoreMenuTexture::UpgradeGreen);
    require(IsTextureValid(upgradeArrow) && upgradeArrow.width == 256 && upgradeArrow.height == 256,
        "building upgrade arrow loads as a 256x256 icon");
    Image arrowImage = LoadImageFromTexture(upgradeArrow);
    const Color arrowCenter = GetImageColor(arrowImage, 128, 128);
    require(GetImageColor(arrowImage, 0, 0).a == 0 && arrowCenter.a >= 250 &&
        arrowCenter.g > arrowCenter.r * 2 && arrowCenter.g > arrowCenter.b,
        "building upgrade arrow has a green face and transparent background");
    UnloadImage(arrowImage);
    for (const auto& definition : ian::RunUpgradeDefinitions)
        if (!ian::retiredRunUpgrade(definition.effect))
            require(IsTextureValid(ui.runUpgradeTexture(definition.effect)),
                "every active upgrade has an actual icon, without text placeholders");
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
    require(ui.lootTexture(ian::LootUpgradeEffect::MaximumHealth).id !=
        ui.runUpgradeTexture(ian::RunUpgradeEffect::MaximumHealth).id,
        "loot hearts retain their item artwork separately from stat tomes");
    // The entire window has one monotonic, symmetric scale/opacity transition.
    const auto hidden = ian::app_detail::runUpgradeOverlayTransition(0, 0, false);
    requireNear(hidden.scale, 0.4, 1e-6, "entrance begins at 40 percent scale");
    requireNear(hidden.opacity, 0, 1e-6, "entrance begins invisible");
    float previousOpacity = -1;
    for (int frame = 0; frame <= 24; ++frame) {
        const double t = static_cast<double>(frame) / 24.0;
        const auto entering = ian::app_detail::runUpgradeOverlayTransition(
            t * ian::app_detail::RunUpgradeOverlayEntranceDuration, 0, false);
        const auto leaving = ian::app_detail::runUpgradeOverlayTransition(
            2, ian::app_detail::RunUpgradeChoiceFeedbackDuration +
                (1 - t) * ian::app_detail::RunUpgradeOverlayExitDuration, true);
        require(entering.opacity >= previousOpacity, "entrance never overshoots or reverses");
        requireNear(entering.scale, 0.4 + 0.6 * entering.opacity, 1e-6,
            "scale and opacity share the same progress");
        requireNear(leaving.scale, entering.scale, 1e-6, "exit reverses entrance scale");
        requireNear(leaving.opacity, entering.opacity, 1e-6, "exit reverses entrance opacity");
        previousOpacity = entering.opacity;
    }
    const auto closed = ian::app_detail::runUpgradeOverlayTransition(
        2, ian::app_detail::RunUpgradeChoiceTotalDuration, true);
    requireNear(closed.scale, 0.4, 1e-6, "exit finishes at 40 percent scale");
    requireNear(closed.opacity, 0, 1e-6, "exit finishes invisible");
    requireNear(ian::app_detail::runUpgradeOverlayTransition(2,
        ian::app_detail::RunUpgradeChoiceFeedbackDuration * 0.5, true).opacity,
        1, 1e-6, "selection feedback stays visible before the final close");
    ian::SimulationSnapshot snapshot{};
    snapshot.runUpgradeChoicePending = true;
    snapshot.runUpgradeChoiceCount = 4;
    snapshot.runUpgradeChoices = {
        ian::RunUpgradeEffect::UnlockCannon, ian::RunUpgradeEffect::CannonHeavyShell,
        ian::RunUpgradeEffect::CannonScattershot, ian::RunUpgradeEffect::CannonClusterShell};
    snapshot.runUpgradeRarities = {
        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Rare,
        ian::RunUpgradeRarity::Rare, ian::RunUpgradeRarity::Rare};
    snapshot.runUpgradePaidChoicePrice = ian::PaidRunUpgradeChoicePrice;
    snapshot.coins = ian::PaidRunUpgradeChoicePrice - 1;
    require(!ian::app_detail::canSelectRunUpgradeChoice(snapshot, 3) &&
        ian::app_detail::canSelectRunUpgradeChoice(snapshot, 0),
        "UI disables only the unaffordable fourth card");
    snapshot.coins = ian::PaidRunUpgradeChoicePrice;
    require(ian::app_detail::canSelectRunUpgradeChoice(snapshot, 3),
        "UI accepts the paid card at its exact coin price");
    snapshot.buildingArsenalCount = 1;
    snapshot.playerMaxHealth = 135;
    snapshot.playerMaxRecoverableArmor = 25;
    snapshot.playerDamageMultiplier = 1.25;
    snapshot.playerMoveSpeedMultiplier = 1.12;
    snapshot.coreBuildRadius = 24;
    snapshot.playerLevel = 3;
    snapshot.runUpgradeRerollTokens = 2;
    snapshot.unlockedWeapons[static_cast<std::size_t>(ian::PlayerWeapon::Axe)] = true;
    snapshot.unlockedWeapons[static_cast<std::size_t>(ian::PlayerWeapon::Pickaxe)] = true;
    snapshot.unlockedWeapons[static_cast<std::size_t>(ian::PlayerWeapon::Rifle)] = true;
    snapshot.unlockedBuildings[static_cast<std::size_t>(ian::BuildingType::GunTurret)] = true;
    snapshot.upgradeStats.luck = 0.2;
    {
        auto shop = snapshot;
        shop.runUpgradeChoicePending = false;
        shop.runShop.pending = true;
        shop.runShop.wave = 3;
        shop.coins = 35;
        const std::array shopCards{ian::RunUpgradeEffect::ExtraBuilding,ian::RunUpgradeEffect::BuildingCapacity,
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
    auto leveledInsight = simulation->insightSystem();
    static_cast<void>(leveledInsight.grantInsight(leveledInsight.progress().requiredInsight,
        ian::InsightSource::Other, ian::InsightCategory::Exploration, {.bypassDiminishing=true}));
    auto progression = simulation->saveProgressionState();
    progression.insight = leveledInsight.saveState();
    require(simulation->loadProgressionState(progression) && simulation->snapshot().playerLevel == 2,
        "preview fixture includes earned class bonuses");
    for (const auto effect : {ian::RunUpgradeEffect::Damage, ian::RunUpgradeEffect::AttackSpeed,
         ian::RunUpgradeEffect::MoveSpeed, ian::RunUpgradeEffect::MaximumHealth,
         ian::RunUpgradeEffect::RecoverableArmor, ian::RunUpgradeEffect::BuildingHealth,
         ian::RunUpgradeEffect::DefenseDamage, ian::RunUpgradeEffect::DefenseFireRate,
         ian::RunUpgradeEffect::ProductionSpeed, ian::RunUpgradeEffect::ExperienceGain, ian::RunUpgradeEffect::RichResources,
         ian::RunUpgradeEffect::Luck, ian::RunUpgradeEffect::GatheringSpeed,
         ian::RunUpgradeEffect::ProjectileCount, ian::RunUpgradeEffect::ClusterPayload,
         ian::RunUpgradeEffect::Damage, ian::RunUpgradeEffect::BuildingHealth,
         ian::RunUpgradeEffect::ProjectileCount, ian::RunUpgradeEffect::Regeneration, ian::RunUpgradeEffect::CannonHeavyShell,
         ian::RunUpgradeEffect::UnlockClub, ian::RunUpgradeEffect::UnlockRifle,
         ian::RunUpgradeEffect::UnlockBomb, ian::RunUpgradeEffect::UnlockIceWand,
        ian::RunUpgradeEffect::UnlockFireWand,
        ian::RunUpgradeEffect::ProjectileSpeed, ian::RunUpgradeEffect::AttackDuration,
        ian::RunUpgradeEffect::AttackSize, ian::RunUpgradeEffect::CriticalChance,
        ian::RunUpgradeEffect::CriticalDamage, ian::RunUpgradeEffect::AttackKnockback}) {
        auto before = simulation->snapshot();
        if (const auto weapon = ian::runUpgradeWeapon(effect);
            weapon && before.unlockedWeapons[static_cast<std::size_t>(*weapon)]) {
            require(!simulation->grantDebugRunUpgrade(effect), "the class starter cannot be acquired twice");
            continue;
        }
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
        requireNear(projected.upgradeStats.experienceGain, actual.upgradeStats.experienceGain, 1e-9, "experience preview matches simulation");
        requireNear(projected.upgradeStats.toolAttackSpeed, actual.upgradeStats.toolAttackSpeed, 1e-9, "tool speed preview matches simulation");
        requireNear(projected.upgradeStats.playerAttackSpeed, actual.upgradeStats.playerAttackSpeed, 1e-9, "attack speed preview matches simulation");
        requireNear(projected.upgradeStats.resourceYield, actual.upgradeStats.resourceYield, 1e-9, "meta/class yield preview matches simulation");
        requireNear(projected.upgradeStats.resourceCapacity, actual.upgradeStats.resourceCapacity, 1e-9,"capacity tome preview matches simulation");
        require(projected.woodCapacity==actual.woodCapacity && projected.stoneCapacity==actual.stoneCapacity &&
            projected.crystalCapacity==actual.crystalCapacity,"world capacity preview leaves carrying capacity unchanged");
        requireNear(projected.upgradeStats.projectileSpeed, actual.upgradeStats.projectileSpeed, 1e-9, "projectile speed preview matches simulation");
        requireNear(projected.upgradeStats.attackDuration, actual.upgradeStats.attackDuration, 1e-9, "duration preview matches simulation");
        requireNear(projected.upgradeStats.attackSize, actual.upgradeStats.attackSize, 1e-9, "size preview matches simulation");
        requireNear(projected.upgradeStats.critChance, actual.upgradeStats.critChance, 1e-9, "critical chance preview matches simulation");
        requireNear(projected.upgradeStats.critDamage, actual.upgradeStats.critDamage, 1e-9, "critical damage preview matches simulation");
        requireNear(projected.upgradeStats.knockback, actual.upgradeStats.knockback, 1e-9, "knockback preview matches simulation");
        requireNear(projected.upgradeStats.defenseDamage, actual.upgradeStats.defenseDamage, 1e-9, "defense damage preview matches simulation");
        requireNear(projected.upgradeStats.defenseFireRate, actual.upgradeStats.defenseFireRate, 1e-9, "defense rate preview matches simulation");
        requireNear(projected.upgradeStats.luck, actual.upgradeStats.luck, 1e-9, "luck preview matches simulation");
        requireNear(projected.upgradeStats.healthRegeneration, actual.upgradeStats.healthRegeneration, 1e-9, "regen hammer preview matches simulation");
        require(projected.unlockedWeapons == actual.unlockedWeapons &&
            projected.bombsRemaining == actual.bombsRemaining, "weapon blueprint preview matches actual acquisition");
        require(projected.coreBuildRadius == actual.coreBuildRadius && projected.runUpgradePower == actual.runUpgradePower &&
            projected.upgradeStats.projectileCountBonus == actual.upgradeStats.projectileCountBonus &&
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
        defense.runUpgradeRarities[0] = ian::RunUpgradeRarity::Rare;
        defense.runUpgradeRollSeeds[0] = 7611;
        auto projected = ian::app_detail::previewRunUpgrade(defense, 0);
        require(projected.buildingArsenalCount == 1 &&
            projected.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockGunTurret)] == 3,
            "repeated defense preview raises level without consuming another slot");
        const auto roll = ian::rollBuildingStatUpgrade(ian::BuildingType::GunTurret,
            ian::RunUpgradeRarity::Rare, defense.runUpgradeRollSeeds[0]);
        for (std::size_t i = 0; i < roll.count; ++i)
            requireNear(projected.buildingUpgradePowers[static_cast<std::size_t>(ian::BuildingType::GunTurret)]
                    [static_cast<std::size_t>(roll.stats[i])], 1.4, 1e-12,
                "building card preview shows the exact stats applied on selection");
        defense.runUpgradeChoices[0] = ian::RunUpgradeEffect::UnlockCatapult;
        defense.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockCatapult)] = 0;
        defense.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockCatapult)] = 0;
        projected = ian::app_detail::previewRunUpgrade(defense, 0);
        require(projected.buildingArsenalCount == 2 &&
            projected.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockCatapult)] == 1,
            "new defense preview adds one type slot at level one");
    }
    if (argc > 1) {
        auto fence = snapshot;
        fence.runUpgradeChoiceCount = 1;
        fence.runUpgradeChoices[0] = ian::RunUpgradeEffect::UnlockWall;
        fence.runUpgradeStacks[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockWall)] = 2;
        fence.runUpgradePower[ian::runUpgradeIndex(ian::RunUpgradeEffect::UnlockWall)] = 2.0;
        fence.runUpgradeRarities[0] = ian::RunUpgradeRarity::Legendary;
        fence.unlockedBuildings[static_cast<std::size_t>(ian::BuildingType::Wall)] = true;
        fence.buildingBlueprintLevels[static_cast<std::size_t>(ian::BuildingType::Wall)] = 2;
        fence.wallBaseMaxHealth = 100;
        fence.upgradeStats.buildingHealth = 1.0;
        SetWindowSize(1280, 720);
        const auto target = LoadRenderTexture(1280, 720);
        const std::array<float, ian::MaximumRunUpgradeChoices> hovers{};
        BeginTextureMode(target);
        ClearBackground({66, 96, 70, 255});
        ian::app_detail::drawRunUpgradeOverlay(ui, fence, 2.0, 0.0, std::nullopt, hovers);
        EndTextureMode();
        auto capture = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&capture);
        require(ExportImage(capture, (std::string(argv[1]) + "/wall-hp-upgrade.png").c_str()), "fence HP preview exported");
        UnloadImage(capture);
        UnloadRenderTexture(target);
    }
    for (const ian::Language language : {ian::Language::English, ian::Language::Russian}) {
        ian::setLanguage(language);
        for (const Vector2 resolution : {Vector2{1920, 1080}, Vector2{1280, 720}, Vector2{1280, 800}, Vector2{1024, 768},
            Vector2{800, 1200}, Vector2{640, 360}, Vector2{2560, 720}}) {
            const int width = static_cast<int>(resolution.x);
            const int height = static_cast<int>(resolution.y);
            SetWindowSize(width, height);
            PollInputEvents();
            for (std::size_t count = 1; count <= ian::MaximumRunUpgradeChoices; ++count) {
                auto choices = snapshot;
                choices.runUpgradeChoiceCount = count;
                for (std::size_t index = 0; index < count; ++index) {
                    const Rectangle card = ian::app_detail::runUpgradeCardBounds(index, count);
                    require(card.x >= 0 && card.y >= 0 && card.x + card.width <= resolution.x &&
                        card.y + card.height <= resolution.y,
                        "one to five centered choices fit the screen");
                    require(ian::app_detail::hoveredRunUpgradeChoice(choices,
                        {card.x + card.width * 0.5F, card.y + card.height * 0.5F}) == index,
                        "portrait and landscape card input uses visible bounds");
                    for (std::size_t other = 0; other < index; ++other)
                        require(!CheckCollisionRecs(card, ian::app_detail::runUpgradeCardBounds(other, count)),
                            "upgrade rows and columns never overlap");
                }
            }
            for (std::size_t index = 0; index < 4; ++index) {
                const Rectangle card = ian::app_detail::runUpgradeCardBounds(index, 4);
                require(card.x >= 0 && card.y >= 0 && card.y + card.height < resolution.y,
                    "cards fit on screen");
                require(ian::app_detail::hoveredRunUpgradeChoice(snapshot,
                    {card.x + card.width * 0.5F, card.y + card.height * 0.5F}) == index,
                    "resized card hit areas track their visible bounds");
            }
            for (int scene = 0; scene < 15; ++scene) {
                auto display = snapshot;
                std::array<float, ian::MaximumRunUpgradeChoices> displayHovers{};
                Vector2 pointer{-100, -100};
                if (scene < 2) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::ReactiveTraps,
                        ian::RunUpgradeEffect::RecoverableArmor, ian::RunUpgradeEffect::UnlockClub,
                        ian::RunUpgradeEffect::Luck};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Epic, ian::RunUpgradeRarity::Uncommon};
                    display.upgradeStats.luck = 0;
                }
                if (scene == 9) {
                    display.runUpgradeChoiceCount = ian::DefaultRunUpgradeChoices;
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::AttackSpeed,
                        ian::RunUpgradeEffect::Luck, ian::RunUpgradeEffect::GatheringSpeed};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Uncommon, ian::RunUpgradeRarity::Rare};
                }
                if (scene==10 || scene==11) {
                    display.runUpgradeChoiceCount=3;
                    display.shrines.offer=scene==10 ? ian::ShrineKind::Player : ian::ShrineKind::Buildings;
                    display.runUpgradeChoices=scene==10 ? decltype(display.runUpgradeChoices){ian::RunUpgradeEffect::Damage,ian::RunUpgradeEffect::AttackSpeed,ian::RunUpgradeEffect::MaximumHealth}
                        : decltype(display.runUpgradeChoices){ian::RunUpgradeEffect::DefenseDamage,ian::RunUpgradeEffect::DefenseFireRate,ian::RunUpgradeEffect::BuildingHealth};
                    display.runUpgradeRarities.fill(ian::RunUpgradeRarity::Common);
                    const auto preview=ian::app_detail::previewRunUpgrade(display,0);
                    require(preview.runUpgradeStacks==display.runUpgradeStacks && preview.tomeCount==display.tomeCount,"shrine preview never consumes inventory slots");
                    if(scene==10) requireNear(preview.upgradeStats.defenseDamage,display.upgradeStats.defenseDamage,1e-12,"player shrine preview does not amplify defenses");
                    else requireNear(preview.playerDamageMultiplier,display.playerDamageMultiplier,1e-12,"building shrine preview leaves player damage unchanged");
                }
                if (scene == 3) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::UnlockLumberMill,
                        ian::RunUpgradeEffect::UnlockQuarry, ian::RunUpgradeEffect::UnlockCrystalMine,
                        ian::RunUpgradeEffect::ProductionSpeed};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Rare};
                    ian::CrystalMineSystem producers;
                    producers.setProductionSpeedMultiplier(display.upgradeStats.productionSpeed);
                    for (const auto type : ian::RunEconomyBuildingTypes) {
                        const auto index = static_cast<std::size_t>(type);
                        display.runUpgradeStacks[ian::runUpgradeIndex(*ian::buildingRunUpgrade(type))] = 2;
                        display.producerAmountsPerCycle[index] = producers.productionAmount(2, type);
                        display.nextProducerAmountsPerCycle[index] = producers.productionAmount(3, type);
                        display.producerRatesPerSecond[index] = display.producerAmountsPerCycle[index] /
                            producers.productionInterval(type, 2);
                        display.nextProducerRatesPerSecond[index] = display.nextProducerAmountsPerCycle[index] /
                            producers.productionInterval(type, 3);
                    }
                    require(ian::app_detail::previewRunUpgrade(display, 0).buildingArsenalCount == display.buildingArsenalCount,
                        "producer improvement preview leaves defense slots unchanged");
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
                if (scene == 7) {
                    display.runUpgradeChoiceCount = ian::MaximumRunUpgradeChoices;
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::GunShrapnel,
                        ian::RunUpgradeEffect::RecoverableArmor, ian::RunUpgradeEffect::ProjectileCount,
                        ian::RunUpgradeEffect::AttackSize, ian::RunUpgradeEffect::WiderChoice};
                    display.runUpgradeStacks.fill(1);
                    display.runUpgradePower.fill(1);
                    display.unlockedWeapons.fill(true);
                    display.unlockedBuildings.fill(true);
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
                if (scene == 12 || scene == 13) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::UnlockGunTurret,
                        ian::RunUpgradeEffect::Damage, ian::RunUpgradeEffect::UnlockCannon,
                        ian::RunUpgradeEffect::UnlockTeslaTower};
                    for (auto effect : {ian::RunUpgradeEffect::UnlockGunTurret,
                            ian::RunUpgradeEffect::UnlockCannon, ian::RunUpgradeEffect::UnlockTeslaTower}) {
                        display.runUpgradeStacks[ian::runUpgradeIndex(effect)] = 2;
                        display.runUpgradePower[ian::runUpgradeIndex(effect)] = 2;
                    }
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Epic,
                        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Legendary,
                        ian::RunUpgradeRarity::Rare};
                    display.coins = scene == 12 ? 40 : 0;
                }
                if (scene == 14) {
                    display.runUpgradeChoices = {ian::RunUpgradeEffect::UnlockWindBlower,
                        ian::RunUpgradeEffect::AttackSize, ian::RunUpgradeEffect::UnlockIceWand,
                        ian::RunUpgradeEffect::CriticalChance};
                    display.runUpgradeRarities = {ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Common, ian::RunUpgradeRarity::Common,
                        ian::RunUpgradeRarity::Uncommon};
                    display.coins = 0;
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
                        std::to_string(width) + "x" + std::to_string(height) + (language == ian::Language::Russian ? "_ru" : "") +
                        (scene == 0 ? "" : scene == 1 ? "_luck" : scene == 2 ? "_heavy" : scene == 3 ? "_economy" : scene == 4 ? "_hammers" : scene == 5 ? "_numeric_hammers" : scene == 6 ? "_weapons" : scene == 8 ? "_defenses" : scene == 9 ? "_three" : scene == 10 ? "_player_shrine" : scene == 11 ? "_building_shrine" : scene == 12 ? "_paid" : scene == 13 ? "_unaffordable" : scene == 14 ? "_acquisition" : "_full") + ".png";
                    require(ExportImage(capture, path.c_str()), "upgrade screenshot exported");
                    UnloadImage(capture);
                }
                if (scene == 4 && language == ian::Language::English && width == 1920) {
                    Image before = LoadImageFromTexture(target.texture);
                    ImageFlipVertical(&before);
                    BeginTextureMode(target);
                    ClearBackground({66, 96, 70, 255});
                    ian::app_detail::drawRunUpgradeOverlay(ui, display, 4.0, 0.0,
                        std::nullopt, displayHovers, pointer);
                    EndTextureMode();
                    Image after = LoadImageFromTexture(target.texture);
                    ImageFlipVertical(&after);
                    const Color* first = LoadImageColors(before);
                    const Color* second = LoadImageColors(after);
                    int changed = 0;
                    for (int pixel = 0; pixel < width * height; ++pixel) {
                        if (first[pixel].r != second[pixel].r || first[pixel].g != second[pixel].g ||
                            first[pixel].b != second[pixel].b) ++changed;
                    }
                    require(changed > 1000, "celebration rays keep animating after the entrance completes");
                    for (std::size_t choice = 0; choice < display.runUpgradeChoiceCount; ++choice) {
                        const auto card = ian::app_detail::runUpgradeCardBounds(choice, display.runUpgradeChoiceCount);
                        const int x = static_cast<int>(card.x + card.width * 0.85F);
                        const int y = static_cast<int>(card.y + card.height * 0.8F);
                        const int pixel = y * width + x;
                        require(first[pixel].r == second[pixel].r && first[pixel].g == second[pixel].g &&
                            first[pixel].b == second[pixel].b,
                            "animated rays stay behind the readable opaque offer cards");
                        if (choice == 0) require(first[pixel].g > first[pixel].r && first[pixel].g > first[pixel].b,
                            "unhovered rare card keeps its green rarity background");
                        if (choice == 1) require(first[pixel].b > first[pixel].r && first[pixel].b > first[pixel].g,
                            "unhovered uncommon card keeps its blue rarity background");
                        if (choice == 2) require(first[pixel].r > first[pixel].g && first[pixel].b > first[pixel].g,
                            "unhovered epic card keeps its purple rarity background");
                    }
                    UnloadImageColors(const_cast<Color*>(first));
                    UnloadImageColors(const_cast<Color*>(second));
                    UnloadImage(before);
                    UnloadImage(after);
                    if (argc > 2 && std::string_view(argv[2]) == "animation") {
                        for (int frame = 0; frame < 60; ++frame) {
                            BeginTextureMode(target);
                            ClearBackground({66, 96, 70, 255});
                            ian::app_detail::drawRunUpgradeOverlay(ui, display, 2.0 + frame / 10.0, 0.0,
                                std::nullopt, displayHovers, pointer);
                            EndTextureMode();
                            Image capture = LoadImageFromTexture(target.texture);
                            ImageFlipVertical(&capture);
                            const std::string path = std::string(argv[1]) + "/ray_frame_" + std::to_string(frame) + ".png";
                            require(ExportImage(capture, path.c_str()), "ray animation frame exported");
                            UnloadImage(capture);
                        }
                    }
                }
                if (scene == 12 && argc > 1 && width == 1920 && language == ian::Language::English) {
                    for (int frame = 0; frame < 4; ++frame) {
                        BeginTextureMode(target);
                        ClearBackground({66, 96, 70, 255});
                        ian::app_detail::drawRunUpgradeOverlay(ui, display, 2.0,
                            (frame + 1) * ian::app_detail::RunUpgradeChoiceFeedbackDuration / 4,
                            std::size_t{2}, displayHovers, pointer);
                        EndTextureMode();
                        Image capture = LoadImageFromTexture(target.texture);
                        ImageFlipVertical(&capture);
                        const std::string path = std::string(argv[1]) + "/ian_upgrades_pick_" +
                            std::to_string(frame) + ".png";
                        require(ExportImage(capture, path.c_str()), "selection feedback frame exported");
                        UnloadImage(capture);
                    }
                }
                if (scene == 7 && argc > 1 && width == 1920 && language == ian::Language::English) {
                    for (int phase = 0; phase < 2; ++phase) {
                        BeginTextureMode(target);
                        ClearBackground({66, 96, 70, 255});
                        ian::app_detail::drawRunUpgradeOverlay(ui, display,
                            phase == 0 ? ian::app_detail::RunUpgradeOverlayEntranceDuration * 0.5 : 2.0,
                            phase == 0 ? 0.0 : ian::app_detail::RunUpgradeChoiceFeedbackDuration +
                                ian::app_detail::RunUpgradeOverlayExitDuration * 0.5,
                            phase == 0 ? std::nullopt : std::optional<std::size_t>{3}, displayHovers, pointer);
                        EndTextureMode();
                        Image capture = LoadImageFromTexture(target.texture);
                        ImageFlipVertical(&capture);
                        const std::string path = std::string(argv[1]) +
                            (phase == 0 ? "/ian_upgrades_enter_half.png" : "/ian_upgrades_exit_half.png");
                        require(ExportImage(capture, path.c_str()), "whole-window transition frame exported");
                        UnloadImage(capture);
                    }
                }
                if (scene == 7) {
                    BeginTextureMode(target);
                    const Color background{66, 96, 70, 255};
                    ClearBackground(background);
                    ian::app_detail::drawRunUpgradeOverlay(ui, display, 2.0,
                        ian::app_detail::RunUpgradeChoiceTotalDuration, 3U, displayHovers, pointer);
                    EndTextureMode();
                    Image capture = LoadImageFromTexture(target.texture);
                    const Color* pixels = LoadImageColors(capture);
                    for (int index = 0; index < width * height; ++index)
                        require(pixels[index].r == background.r && pixels[index].g == background.g &&
                            pixels[index].b == background.b,
                            "all frames, icons and text disappear completely after selection");
                    UnloadImageColors(const_cast<Color*>(pixels));
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
