#include "TestHarness.hpp"
#include "ui/RunMenus.hpp"
#include "ui/DebugUpgradeMenu.hpp"
#include "ui/MainMenuHome.hpp"
#include "ui/UiViewport.hpp"
#include "ui/UiItemDescriptions.hpp"
#include "ui/UiItemTooltip.hpp"
#include "localization/Localization.hpp"
#include "game/Simulation.hpp"

#include <filesystem>

int main(int argc, char** argv) {
    using namespace ian;
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_HIGHDPI);
    InitWindow(64, 64, "Run menus smoke");
    if (!IsWindowReady()) return 77;
    initializeLocalization();
    GameUi ui;
    ui.initialize();
    for (const auto& definition : RunUpgradeDefinitions)
        require(!definition.description.empty(),"every run upgrade has a hover description");
    for (const auto weapon : PlayerWeaponHotbarOrder)
        require(!weaponDescription(weapon).empty(),"every current weapon has a hover description");
    for (std::size_t i=0;i<LootUpgradeEffectCount;++i)
        require(std::string_view(lootUpgradeDescription(static_cast<LootUpgradeEffect>(i))).size() > 0,
            "every loot item has a hover description");
    for (const auto building : RunBuildingTypes)
        require(!buildingDescription(building).empty(),"every defensive building has a hover description");
    for (const auto building : RunEconomyBuildingTypes)
        require(!buildingDescription(building).empty(),"every production building has a hover description");
    for (std::size_t i=0;i<ModularBuildPieceCount;++i)
        require(!modularDescription(static_cast<ModularBuildPiece>(i)).empty(),"every construction piece has a hover description");
    for (const auto weapon : {PlayerWeapon::Bomb, PlayerWeapon::Rifle, PlayerWeapon::IceWand, PlayerWeapon::FireWand}) {
        const auto icon = ui.weaponTexture(weapon);
        require(IsTextureValid(icon) && icon.width == 256 && icon.height == 256, "replacement weapon icons load at 256x256");
        auto image = LoadImageFromTexture(icon);
        require(GetImageColor(image, 0, 0).a == 0, "weapon icons retain transparent backgrounds");
        UnloadImage(image);
    }
    // Raylib's automation key-down/up event types, verified in the pinned rcore.c.
    const auto key = [](int code, bool down) { PlayAutomationEvent({0, down ? 2U : 1U, {code, 0, 0, 0}}); };
    key(KEY_TAB, true);
    require(readMenuNavigationInput().tabStep == 1, "native Tab reaches menu navigation");
    key(KEY_LEFT_SHIFT, true);
    require(readMenuNavigationInput().tabStep == -1, "Shift+Tab reverses navigation");
    key(KEY_TAB, false); key(KEY_LEFT_SHIFT, false);
    key(KEY_DOWN, true);
    require(readMenuNavigationInput().verticalStep == 1, "native arrow reaches menu navigation");
    key(KEY_DOWN, false);
    key(KEY_ENTER, true);
    require(readMenuNavigationInput().activate, "native Enter activates the selected control");
    key(KEY_ENTER, false);


    DebugUpgradeMenuState upgrades;
    SimulationSnapshot upgradeSnapshot{};
    auto upgradeTarget = LoadRenderTexture(1920,1200);
    const auto drawUpgrades = [&](DebugUpgradeMenuInput input = {}, Vector2 viewport = {1920,1200}) {
        BeginTextureMode(upgradeTarget);
        ClearBackground({76,114,64,255});
        const auto result = drawDebugUpgradeMenu(ui, upgradeSnapshot, upgrades, viewport, input);
        EndTextureMode(); return result;
    };
    const auto upgradeLayout = debugUpgradeMenuLayout({1920,1200});
    const auto clickUpgrade = [&](Rectangle bounds) {
        return drawUpgrades({.pointer={bounds.x+bounds.width*.5F,bounds.y+bounds.height*.5F},.click=true});
    };
    require(!drawUpgrades().grant, "opening debug catalog never grants automatically");
    clickUpgrade(upgradeLayout.card(0,0));
    clickUpgrade(upgradeLayout.card(1,0));
    require(upgrades.selected[0] && upgrades.selected[1], "debug catalog selects multiple different upgrades");
    clickUpgrade(upgradeLayout.presets[3]);
    clickUpgrade(upgradeLayout.rarities[4]);
    const auto batch = clickUpgrade(upgradeLayout.grant);
    require(batch.grant && batch.grant->count==1000 && batch.grant->rarity==RunUpgradeRarity::Legendary &&
        batch.grant->selected[0] && batch.grant->selected[1], "grant action carries all selected cards, count and rarity");
    require(!batch.close && clickUpgrade(upgradeLayout.grant).grant.has_value(),
        "grant menu stays open and supports repeated issuance");
    upgrades.field=DebugUpgradeField::Count; upgrades.countText="";
    drawUpgrades({.text="20001x"});
    require(debugUpgradeCount(upgrades.countText)==20001,"count accepts values above presets and ignores non-digits");
    upgrades.countText="2147483648";
    require(!clickUpgrade(upgradeLayout.grant).grant,"invalid numeric overflow cannot grant");
    upgrades.field=DebugUpgradeField::Search;
    drawUpgrades({.text="VOLLEY"});
    require(debugUpgradeMatches(RunUpgradeDefinitions[runUpgradeIndex(RunUpgradeEffect::ProjectileCount)],upgrades) &&
        !debugUpgradeMatches(RunUpgradeDefinitions[runUpgradeIndex(RunUpgradeEffect::Damage)],upgrades),
        "search filters names and descriptions without losing hidden selections");
    setLanguage(Language::Russian); upgrades.search="ЗАЛП";
    require(debugUpgradeMatches(RunUpgradeDefinitions[runUpgradeIndex(RunUpgradeEffect::ProjectileCount)],upgrades),
        "search handles localized uppercase Cyrillic");
    std::string cyrillic="КРИСТАЛЛ"; debugUpgradeBackspace(cyrillic);
    require(cyrillic=="КРИСТАЛ","backspace removes one complete UTF-8 character");
    upgrades.field=DebugUpgradeField::Search; upgrades.search=std::string(127,'a');
    drawUpgrades({.text="Я"});
    require(upgrades.search==std::string(127,'a'),"search length limit never truncates a UTF-8 character");
    setLanguage(Language::English);
    upgrades={};
    drawUpgrades({.pointer={upgradeLayout.catalog.x+30,upgradeLayout.catalog.y+30},.wheel=-100});
    const auto selectionBefore=upgrades.selected;
    drawUpgrades({.pointer={upgradeLayout.catalog.x+30,upgradeLayout.catalog.y-3},.click=true});
    require(upgrades.selected==selectionBefore,"clipped cards cannot receive clicks outside the scroll viewport");
    upgradeSnapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)]=1;
    require(debugUpgradeGrantable(RunUpgradeEffect::UnlockCannon,upgradeSnapshot) &&
        !debugUpgradeUnique(RunUpgradeEffect::UnlockCannon) &&
        debugUpgradeCategory(RunUpgradeEffect::UnlockCannon) == DebugUpgradeCategory::Defense,
        "owned combat cards remain repeatable in the defense catalog without core placement");
    upgradeSnapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] = MaxBuildingLevel;
    require(debugUpgradeGrantable(RunUpgradeEffect::UnlockCannon,upgradeSnapshot),
        "defense remains grantable beyond the eighth visual tier");
    upgradeSnapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockCannon)] = MaxStructureLevel;
    require(!debugUpgradeGrantable(RunUpgradeEffect::UnlockCannon,upgradeSnapshot),
        "defense stops at its saved representation limit");
    upgradeSnapshot.runUpgradeStacks[runUpgradeIndex(RunUpgradeEffect::UnlockQuarry)] = 1;
    require(debugUpgradeGrantable(RunUpgradeEffect::UnlockQuarry,upgradeSnapshot) &&
        !debugUpgradeUnique(RunUpgradeEffect::UnlockQuarry),
        "producer cards remain repeatable in the debug catalog");
    upgrades.category=DebugUpgradeCategory::Unlocks; upgrades.search.clear();
    require(!debugUpgradeMatches(RunUpgradeDefinitions[0],upgrades),"category filters exclude other upgrade families");
    debugUpgradeSelect(upgrades,RunUpgradeEffect::CannonHeavyShell,true);
    debugUpgradeSelect(upgrades,RunUpgradeEffect::CannonScattershot,true);
    require(!upgrades.selected[runUpgradeIndex(RunUpgradeEffect::CannonHeavyShell)] &&
        upgrades.selected[runUpgradeIndex(RunUpgradeEffect::CannonScattershot)],"catalog keeps only one cannon specialization selected");

    upgrades={};
    upgrades.countText="2147483648";
    upgrades.rarity=RunUpgradeRarity::Legendary;
    upgrades.selected[0]=true;
    clickUpgrade(upgradeLayout.tabs[6]);
    require(upgrades.category==DebugUpgradeCategory::Powerups,"Shift F5 catalog exposes powerup category");
    for (int i=0;i<static_cast<int>(PowerupTypeCount);++i) {
        clickUpgrade(upgradeLayout.card(i,0));
        const auto activation=clickUpgrade(upgradeLayout.grant);
        require(activation.grant && activation.grant->powerup==static_cast<PowerupType>(i) &&
            std::ranges::none_of(activation.grant->selected,[](bool selected){return selected;}) &&
            activation.grant->count==1,
            "powerup action carries its type, bypasses copies/rarity, and does not grant hidden upgrades");
        require(clickUpgrade(upgradeLayout.grant).grant->powerup==static_cast<PowerupType>(i),
            "powerup remains grantable when activated repeatedly");
    }
    clickUpgrade(upgradeLayout.count);
    require(upgrades.field==DebugUpgradeField::None,"hidden copies input does not receive powerup pane clicks");
    upgrades.field=DebugUpgradeField::Search;
    require(!drawUpgrades({.enter=true}).grant,"powerup search Enter cannot accidentally activate");
    upgrades.field=DebugUpgradeField::None;
    require(drawUpgrades({.enter=true}).grant->powerup==PowerupType::GoldRush,"powerup activation supports Enter");
    if(argc>1) {
        std::filesystem::create_directories(argv[1]);
        const auto capturePowerups=[&](Vector2 viewport,const char* filename) {
            auto target=LoadRenderTexture(static_cast<int>(viewport.x),static_cast<int>(viewport.y));
            BeginTextureMode(target); ClearBackground({76,114,64,255});
            drawDebugUpgradeMenu(ui,upgradeSnapshot,upgrades,viewport); EndTextureMode();
            auto image=LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
            ExportImage(image,(std::filesystem::path(argv[1])/filename).string().c_str());
            UnloadImage(image); UnloadRenderTexture(target);
        };
        capturePowerups({1920,1200},"powerups-menu.png");
        setLanguage(Language::Russian);
        capturePowerups({800,1200},"powerups-menu-ru-portrait.png");
        setLanguage(Language::English);
    }

    // The real window enables small SDF texture caching; render-texture-only
    // tests skip that path and cannot detect scissor-clipped cache textures.
    SetWindowSize(1280,800); PollInputEvents();
    upgrades={}; upgradeSnapshot={};
    Rectangle nativeTitle{};
    for (int frame=0;frame<4;++frame) {
        beginUiTextPerformanceFrame();
        BeginDrawing(); ClearBackground({76,114,64,255});
        {
            const ScopedUiCanvas canvas;
            const Vector2 viewport{static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())};
            const auto nativeLayout=debugUpgradeMenuLayout(viewport);
            const auto card=nativeLayout.card(0,0);
            nativeTitle=canvas.viewport().toScreen({card.x+82*nativeLayout.scale,card.y+12*nativeLayout.scale,
                card.width-98*nativeLayout.scale,44*nativeLayout.scale});
            drawDebugUpgradeMenu(ui,upgradeSnapshot,upgrades,viewport);
        }
        EndDrawing();
    }
    auto nativeImage=LoadImageFromScreen();
    const float pixelScaleX=static_cast<float>(nativeImage.width)/static_cast<float>(GetScreenWidth());
    const float pixelScaleY=static_cast<float>(nativeImage.height)/static_cast<float>(GetScreenHeight());
    int titlePixels=0;
    for(int y=static_cast<int>(nativeTitle.y*pixelScaleY);y<static_cast<int>((nativeTitle.y+nativeTitle.height)*pixelScaleY);++y)
        for(int x=static_cast<int>(nativeTitle.x*pixelScaleX);x<static_cast<int>((nativeTitle.x+nativeTitle.width)*pixelScaleX);++x) {
            const Color pixel=GetImageColor(nativeImage,x,y);
            if(pixel.r>220&&pixel.g>220&&pixel.b>200) ++titlePixels;
        }
    if (titlePixels <= 50) {
        ExportImage(nativeImage,"/tmp/ian-native-upgrade-names-failure.png");
        std::cerr << "upgrade title pixels=" << titlePixels << " bounds=" << nativeTitle.x << "," << nativeTitle.y << "," << nativeTitle.width << "," << nativeTitle.height << " image=" << nativeImage.width << "x" << nativeImage.height << "\n";
    }
    require(titlePixels>50,"native window retains upgrade names after the SDF cache warms under scissor");
    if(argc>1) {
        std::filesystem::create_directories(argv[1]);
        ExportImage(nativeImage,(std::filesystem::path(argv[1])/"native-debug-upgrades.png").string().c_str());
    }
    UnloadImage(nativeImage);
    SetWindowSize(64,64); PollInputEvents();
    UnloadRenderTexture(upgradeTarget);

    MetaProgression profile;
    PlayerClass selected = PlayerClass::Vanguard;
    bool sandbox = false;
    ClassSelectionState classes;
    PauseMenuState pause;
    MainMenuHomeState home;
    auto target = LoadRenderTexture(1920, 1200);
    constexpr Vector2 screen{1920, 1200};
    const auto drawClass = [&](MenuNavigationInput input) {
        BeginTextureMode(target);
        ClearBackground({76, 114, 64, 255});
        const auto result = drawClassSelectionMenu(ui, profile, selected, sandbox, false, classes, screen, input);
        EndTextureMode();
        return result;
    };
    require(!drawClass({}).start, "opening character selection does not start a run");
    drawClass({.horizontalStep = 1});
    require(selected == PlayerClass::Sharpshooter, "Right selects the next class");
    drawClass({.horizontalStep = 1});
    require(selected == PlayerClass::Mechanic, "Right reaches the third class in the single-row roster");
    drawClass({.horizontalStep = 1});
    require(selected == PlayerClass::Miner, "Right reaches the fourth class");
    drawClass({.verticalStep = 1});
    require(classes.focused == 4, "Down reaches Start beneath the roster");
    const auto startSelectedClass = [&] {
        require(!drawClass({.activate = true}).start && classes.launching,
            "Start plays its confirmation before entering the run");
        require(!drawClass({.activate = true, .deltaTime = .09F}).start,
            "a repeated confirmation does not skip or restart the transition");
        return drawClass({.deltaTime = .10F}).start;
    };
    require(startSelectedClass(), "all four test characters can start without progression requirements");
    classes.focused = 3;
    drawClass({.tabStep = 1});
    require(classes.focused == 4 && startSelectedClass(), "Start follows the four character cards");
    drawClass({.tabStep = 1});
    drawClass({.activate = true});
    require(sandbox && !drawClass({.activate = false}).start, "sandbox switch does not start a run");
    classes.focused = 3;
    require(startSelectedClass(), "sandbox can start every test character");
    classes.focused = 5;
    drawClass({.activate = true});
    require(!sandbox && selected == PlayerClass::Miner, "switching modes preserves a valid new character");
    classes.focused = 6;
    require(drawClass({.activate = true}).back, "Back is accessible from the keyboard");
    classes.focused = 4;
    drawClass({.activate = true});
    const auto classActions = classSelectionLayout(screen);
    const auto backBounds = classActions.buttons[2];
    require(drawClass({.pointer = {backBounds.x + 20, backBounds.y + 20}, .click = true}).back &&
        !classes.launching && !drawClass({.deltaTime = .1F}).start,
        "Back cancels a pending start transition");
    require(classSelectionLayout({800, 1200}).columns == 2 && classSelectionLayout(screen).columns == 4,
        "portrait uses two rows while landscape shows all four classes in one row");
    for (const auto legacy : {PlayerClass::Vanguard,PlayerClass::Stump,PlayerClass::Chronomancer}) {
        selected=legacy;drawClass({});
        require(selected==PlayerClass::Miner,"retired classes fall back to the default Miner");
    }
    const auto drawPause = [&](MenuNavigationInput input) {
        BeginTextureMode(target);
        ClearBackground({76, 114, 64, 255});
        const auto result = drawPauseMenu(pause, false, screen, input);
        EndTextureMode(); return result;
    };
    require(drawPause({}) == PauseMenuAction::None, "opening pause does not invoke an action");
    require(drawPause({.activate = true}) == PauseMenuAction::Resume, "Resume receives initial keyboard focus");
    require(drawPause({.tabStep = 1, .activate = true}) == PauseMenuAction::Settings, "Tab can open settings from pause");
    require(drawPause({.verticalStep = 1, .activate = true}) == PauseMenuAction::Restart, "Down can select Restart");
    require(drawPause({.tabStep = -1, .activate = true}) == PauseMenuAction::Settings, "Shift+Tab reverses pause navigation");
    pause.focused = 4;
    require(drawPause({.activate = true}) == PauseMenuAction::Exit, "exit retains its distinct action for saving in App");
    SimulationSnapshot summary{};
    summary.state = RunState::Defeat;
    summary.elapsedSeconds = 754;
    summary.runStatistics.wavesSurvived = 8;
    summary.runStatistics.enemiesDefeated = 17234;
    summary.runStatistics.playerDamageDealt = 845;
    summary.runStatistics.defenseDamageDealt = 3241;
    summary.runStatistics.structuresBuilt = 38;
    summary.runStatistics.structuresLost = 12;
    summary.runStatistics.woodAcquired = 2490;
    summary.runStatistics.stoneAcquired = 1280;
    summary.runStatistics.crystalsAcquired = 743;
    summary.runStatistics.coinsCollected = 158;
    RunSummaryState summaryState;
    const auto drawSummary = [&](MenuNavigationInput input) {
        BeginTextureMode(target);
        ClearBackground({76,114,64,255});
        const auto result = drawRunSummaryMenu(summary, summaryState, screen, input);
        EndTextureMode(); return result;
    };
    require(drawSummary({}) == RunSummaryAction::None, "defeat screen does not automatically restart");
    require(drawSummary({.activate=true}) == RunSummaryAction::Restart, "restart receives initial summary focus");
    require(drawSummary({.tabStep=1,.activate=true}) == RunSummaryAction::MainMenu, "Tab and Enter return to main menu");
    require(drawSummary({.horizontalStep=-1,.activate=true}) == RunSummaryAction::Restart, "arrows navigate summary actions");
    const auto summaryButton = runSummaryLayout(screen).buttons[1];
    require(drawSummary({.pointer={summaryButton.x+20,summaryButton.y+20},.click=true}) == RunSummaryAction::MainMenu,
        "summary actions accept pointer clicks");
    const auto drawHome = [&](MainMenuHomeInput input, bool canContinue = false) {
        BeginTextureMode(target);
        auto result = drawMainMenuHome(ui, profile, {}, canContinue, home, screen, input);
        EndTextureMode(); return result;
    };
    drawHome({.tabStep = 1});
    require(home.selectedButton == 1, "Tab moves from Play to Unlocks");
    drawHome({.horizontalStep = 1});
    require(home.selectedButton == 2, "Right navigates the secondary row");
    drawHome({.verticalStep = 1});
    require(home.selectedButton == 4, "Down reaches the footer settings button");
    drawHome({.activate = true});
    require(drawHome({.deltaTime = .2F}).action == MainMenuHomeAction::Settings, "Enter opens the focused menu after the selection animation");
    home = {};
    drawHome({.tabStep = -1}, true);
    require(home.selectedButton == 6, "Continue participates in reverse Tab navigation when available");
    drawHome({});
    require(home.selectedButton == 0, "an unavailable Continue cannot keep focus");

    const auto inside = [](Rectangle child, Rectangle parent) {
        return child.x >= parent.x && child.y >= parent.y && child.x+child.width <= parent.x+parent.width+.1F && child.y+child.height <= parent.y+parent.height+.1F;
    };
    for (const auto viewport : {Vector2{640,360}, Vector2{1280,1024}, Vector2{1920,1200}, Vector2{2560,1080}, Vector2{800,1200}}) {
        const auto canvas = UiViewport::fromScreen(viewport);
        const Rectangle bounds{0,0,canvas.size.x,canvas.size.y};
        for (const Vector2 pointer : {Vector2{0,0},Vector2{canvas.size.x,canvas.size.y},
            Vector2{canvas.size.x,0},Vector2{0,canvas.size.y}})
            require(inside(itemTooltipBounds(pointer,canvas.size,440,220),bounds),
                "hover descriptions fit at all screen edges and aspect ratios");
        const auto debugLayout=debugUpgradeMenuLayout(canvas.size);
        require(inside(debugLayout.panel,bounds) && inside(debugLayout.catalog,debugLayout.panel) &&
            inside(debugLayout.details,debugLayout.panel) && inside(debugLayout.grant,debugLayout.details),
            "debug catalog and grant controls fit all aspect ratios");
        const auto classLayout = classSelectionLayout(canvas.size);
        require(inside(classLayout.panel, bounds), "class panel stays inside every aspect ratio");
        for (const auto card : classLayout.cards) require(inside(card, classLayout.panel), "all four class cards remain visible");
        require(inside(classLayout.details, classLayout.panel), "class details fit inside the panel");
        for (const auto button : classLayout.buttons) require(inside(button, classLayout.panel), "class actions remain visible");
        const auto pauseLayout = pauseMenuLayout(canvas.size);
        require(inside(pauseLayout.panel,bounds), "pause stays inside every aspect ratio");
        for (const auto button : pauseLayout.buttons) require(inside(button,pauseLayout.panel), "all pause actions remain visible");
        const auto summaryLayout = runSummaryLayout(canvas.size);
        require(inside(summaryLayout.panel,bounds), "summary fits every aspect ratio");
        for (const auto card : summaryLayout.highlights) require(inside(card,summaryLayout.panel), "summary highlights fit");
        for (const auto card : summaryLayout.details) require(inside(card,summaryLayout.panel), "summary detail columns fit");
        for (const auto button : summaryLayout.buttons) require(inside(button,summaryLayout.panel), "summary actions fit");
    }
    if (argc > 1) {
        const std::filesystem::path folder(argv[1]);
        std::filesystem::create_directories(folder);
        for (const auto language : {Language::English, Language::Russian}) {
            setLanguage(language);
            classes = {}; selected = PlayerClass::Vanguard; pause = {};
            const auto capture = [&](const char* name) {
                auto image = LoadImageFromTexture(target.texture); ImageFlipVertical(&image);
                const auto path = folder / (std::string(languageCode(language)) + "-" + name + ".png");
                require(ExportImage(image,path.string().c_str()), "menu preview exports"); UnloadImage(image);
            };
            upgrades={}; upgrades.selected[runUpgradeIndex(RunUpgradeEffect::DefenseDamage)]=true;
            upgrades.selected[runUpgradeIndex(RunUpgradeEffect::ProjectileCount)]=true;
            BeginTextureMode(target); ClearBackground({76,114,64,255});
            drawDebugUpgradeMenu(ui,upgradeSnapshot,upgrades,screen); EndTextureMode(); capture("debug-upgrades");
            BeginTextureMode(target); ClearBackground({76,114,64,255});
            drawDebugUpgradeMenu(ui,upgradeSnapshot,upgrades,{800,1200}); EndTextureMode(); capture("debug-upgrades-portrait");
            for (int frame = 0; frame < 6; ++frame) drawClass({.deltaTime = .1F});
            capture("classes");
            for (int i = 0; i < static_cast<int>(PlayerClassDefinitions.size()); ++i) {
                selected = PlayerClassDefinitions[static_cast<std::size_t>(i)].type;
                classes.focused = i;
                for (int frame = 0; frame < 5; ++frame) drawClass({.deltaTime = .1F});
                capture(("class-" + std::to_string(i)).c_str());
            }
            selected = PlayerClass::Lumberjack; classes.focused = 0;
            drawClass({}); capture("available-roster");
            selected = PlayerClass::Mechanic; classes.focused = 2;
            const auto details = classSelectionLayout(screen);
            for (int frame = 0; frame < 5; ++frame) drawClass({.deltaTime = .1F});
            drawClass({.pointer={details.details.x+details.details.width*.72F+35*details.scale,
                details.details.y+110*details.scale}});
            capture("gear-tooltip");
            auto portraitTarget = LoadRenderTexture(1280, 1920);
            BeginTextureMode(portraitTarget); ClearBackground({76,114,64,255});
            static_cast<void>(drawClassSelectionMenu(ui,profile,selected,sandbox,false,classes,{1280,1920},{}));
            EndTextureMode();
            auto portrait = LoadImageFromTexture(portraitTarget.texture); ImageFlipVertical(&portrait);
            require(ExportImage(portrait,(folder/(std::string(languageCode(language))+"-classes-portrait.png")).string().c_str()),
                "portrait class selection preview exports");
            UnloadImage(portrait); UnloadRenderTexture(portraitTarget);
            classes={};
            drawClass({.deltaTime=.05F}); capture("classes-entering");
            setLanguage(Language::Russian);
            const auto lines = itemTooltipLines(localizeText("Fully repairing a structure releases a knockback shockwave."),180,18);
            require(lines.size()>1,"long localized descriptions wrap into readable lines");
            { const ScopedUiFontRole body(UiFontRole::Body);
                for (const auto& line : lines) require(measureUiText(line,18).x<=180,
                    "localized tooltip wrapping preserves the available text width"); }
            setLanguage(language);
            drawPause({}); capture("pause");
            summaryState = {};
            summary.stageCleared = false; drawSummary({}); capture("defeat");
            summary.stageCleared = true; summary.state = RunState::Victory;
            drawSummary({}); capture("victory");
        }
    }
    UnloadRenderTexture(target);
    ui.shutdown();
    CloseWindow();
}
