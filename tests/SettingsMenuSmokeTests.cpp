#include "TestHarness.hpp"
#include "ui/SettingsMenu.hpp"
#include "ui/UiText.hpp"
#include "ui/UiViewport.hpp"
#include "ui/QuestCompletionToast.hpp"

#include <array>
#include <filesystem>

int main(int argc, char** argv) {
    using namespace ian;
    initializeLocalization();
    ControlSettings controls;
    const auto original = controls;
    require(!rebindPlayerControl(controls, ControlAction::Jump, KEY_W, Language::English).empty(),
        "a used key cannot silently replace movement or duplicate another binding");
    require(controls == original, "failed rebind leaves controls unchanged");
    require(!rebindPlayerControl(controls, ControlAction::Jump, KEY_F2, Language::English).empty(),
        "the settings shortcut cannot be rebound to gameplay");
    require(!rebindPlayerControl(controls, ControlAction::Jump, KEY_F3, Language::English).empty(),
        "the feedback shortcut cannot be rebound to gameplay");
    require(rebindPlayerControl(controls, ControlAction::Jump, KEY_Z, Language::English).empty(),
        "a free gameplay key can be bound");
    require(controlKey(controls, ControlAction::Jump) == KEY_Z, "new key applies to gameplay controls");
    require(rebindPlayerControl(controls, ControlAction::Jump, KEY_BACKSPACE, Language::English).empty(),
        "Backspace restores the original key");
    require(controls == original, "restoring a binding preserves the other controls");

    require(rebindPlayerControl(controls, ControlAction::Jump, KEY_T, Language::English).empty(),
        "T is available after removing the time-slowing debug shortcut");
    require(controlKey(controls, ControlAction::Jump) == KEY_T, "T binds as a normal gameplay key");

    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "Settings menu smoke");
    if (!IsWindowReady()) return 77;
    initializeUiText();
    auto target = LoadRenderTexture(1920, 1080);
    UserSettings settings;
    SettingsMenuState state;
    constexpr Vector2 screen{1920, 1080};
    const auto layout = settingsMenuLayout(screen);
    const auto draw = [&](const SettingsMenuInput& input) {
        BeginTextureMode(target);
        ClearBackground({64, 101, 58, 255});
        auto result = drawSettingsMenu(settings, state, screen, input);
        EndTextureMode();
        return result;
    };
    require(!draw({}).close, "drawing the settings does not change navigation");
    const bool initialFullscreen = settings.graphics.fullscreen;
    draw({.valueStep = 1});
    require(settings.graphics.fullscreen != initialFullscreen,
        "an arrow immediately changes the first setting without requiring a prior selection");
    state = {};
    const bool fullscreen = settings.graphics.fullscreen;
    draw({.rowStep = 1, .valueStep = 1});
    require(settings.graphics.fullscreen != fullscreen, "keyboard navigation changes the focused option");
    state.focusedRow = 1;
    settings.graphics.fullscreen = false;
    settings.graphics.displayWidth = 1280;
    settings.graphics.displayHeight = 720;
    settings.graphics.renderWidth = 1280;
    settings.graphics.renderHeight = 720;
    draw({.valueStep = 1});
    require(settings.graphics.displayWidth > 0 && settings.graphics.displayHeight > 0 &&
        (settings.graphics.displayWidth != 1280 || settings.graphics.displayHeight != 720),
        "resolution arrows select output pixel dimensions");
    require(settings.graphics.renderWidth == 1920 && settings.graphics.renderHeight == 1080,
        "output resolution does not inherit the previous relative render scale");
    draw({.tabStep = 1});
    require(state.tab == SettingsTab::Audio, "Tab navigates player settings sections");
    const auto graphics = settings.graphics;
    settings.audio.masterVolume = .3F;
    draw({.reset = true});
    require(state.resetConfirmation && settings.audio.masterVolume == .3F,
        "reset waits for confirmation");
    draw({.back = true});
    require(!state.resetConfirmation && settings.audio.masterVolume == .3F,
        "Escape cancels reset without closing the menu");
    draw({.reset = true});
    draw({.activate = true});
    require(settings.audio.masterVolume == AudioSettings{}.masterVolume && settings.graphics == graphics,
        "reset affects only the chosen section");
    draw({.tabStep = 1});
    require(state.tab == SettingsTab::Controls, "controls section is separate from graphics");
    for (int i = 0; i < 22; ++i) draw({.rowStep = 1});
    require(state.scroll > 0, "keyboard selection scrolls to offscreen bindings");
    const auto binding = draw({.activate = true}).rebind;
    require(binding == ControlAction::Pause, "the last gameplay binding remains accessible");
    draw({.pendingBinding = binding});
    const auto close = draw({.back = true});
    require(close.close, "Escape returns from the settings menu");
    state = {};state.tab=SettingsTab::Game;state.focusedRow=5;
    require(!settings.debugHotkeysEnabled, "debug shortcuts start disabled in the Game settings tab");
    draw({.activate=true});
    require(settings.debugHotkeysEnabled, "the Game toggle enables all debug shortcuts");
    draw({.valueStep=-1});
    require(!settings.debugHotkeysEnabled, "the Game toggle also disables all debug shortcuts");
    settings.debugHotkeysEnabled=true;
    draw({.reset=true});draw({.activate=true});
    require(!settings.debugHotkeysEnabled, "resetting Game settings restores ordinary-player controls");
    state = {};
    state.tab = SettingsTab::Game;
    state.focusedRow = 6;
    require(!draw({.activate = true}).chooseBackground, "background editing does not appear during a run");
    require(state.progressResetConfirmation,"the last in-run Game row opens progress reset confirmation");
    draw({.back=true});
    state={};state.tab=SettingsTab::Game;state.focusedRow=6;
    require(draw({.activate = true, .mainMenu = true}).chooseBackground,
        "menu background editing is available from player settings on the home screen");
    const auto beforeReset=settings;
    state={};state.tab=SettingsTab::Game;state.focusedRow=6;
    require(!draw({.activate=true}).resetProgress && state.progressResetConfirmation,
        "opening progress reset cannot confirm it with the same Enter press");
    require(!draw({.activate=true}).resetProgress && !state.progressResetConfirmation,
        "the destructive dialog defaults to Cancel");
    draw({.activate=true});
    draw({.valueStep=1});
    require(draw({.activate=true}).resetProgress && settings==beforeReset,
        "explicit confirmation requests a progress reset without changing user settings");

    for (const auto viewport : std::array{Vector2{640, 360}, Vector2{1280, 1024},
            Vector2{1920, 1200}, Vector2{2560, 1080}, Vector2{800, 1200}}) {
        const auto ui = UiViewport::fromScreen(viewport);
        const auto responsive = settingsMenuLayout(ui.size);
        require(responsive.panel.x >= 0 && responsive.panel.y >= 0 &&
            responsive.panel.x + responsive.panel.width <= ui.size.x &&
            responsive.panel.y + responsive.panel.height <= ui.size.y,
            "settings remain inside every supported viewport");
        require(responsive.content.height >= 500, "settings preserve readable controls on short screens");
    }
    if (argc > 1) {
        const std::filesystem::path folder(argv[1]);
        std::filesystem::create_directories(folder);
        for (const auto language : {Language::English, Language::Russian}) {
            settings.language = language; setLanguage(language);
            for (int tab = 0; tab < 4; ++tab) {
                state = {}; state.tab = static_cast<SettingsTab>(tab);
                draw({.mainMenu = true});
                auto image = LoadImageFromTexture(target.texture);
                ImageFlipVertical(&image);
                const auto path = folder / (std::string(languageCode(language)) + "-" + std::to_string(tab) + ".png");
                require(ExportImage(image, path.string().c_str()), "settings preview exports");
                UnloadImage(image);
            }
            state={};state.tab=SettingsTab::Game;state.progressResetConfirmation=true;
            draw({.mainMenu=true});
            auto confirm=LoadImageFromTexture(target.texture);ImageFlipVertical(&confirm);
            ExportImage(confirm,(folder/(std::string(languageCode(language))+"-reset-progress.png")).string().c_str());
            UnloadImage(confirm);
        }
    }
    GameUi ui;ui.initialize();
    const QuestCompletionNotice notice{"quest","75 nodes harvested",{3,"item.abacus",5}};
    for(const auto language:{Language::English,Language::Russian}) {
        setLanguage(language);
        BeginTextureMode(target);ClearBackground({64,101,58,255});
        drawQuestCompletionToast(ui,notice,3.5,false,{1920,1080});EndTextureMode();
        auto image=LoadImageFromTexture(target.texture);auto* pixels=LoadImageColors(image);
        int changed=0;
        for(int i=0;i<image.width*image.height;++i) changed+=pixels[i].r!=64 || pixels[i].g!=101 || pixels[i].b!=58;
        if(argc>1) {
            ImageFlipVertical(&image);
            ExportImage(image,(std::filesystem::path(argv[1])/(std::string(languageCode(language))+"-quest-complete.png")).string().c_str());
        }
        if(changed<=20000) std::cerr<<"Toast changed pixels="<<changed<<" screen="<<GetScreenWidth()<<'x'<<GetScreenHeight()<<'\n';
        require(changed>20000,"quest completion notification visibly renders text, frame and reward icon");
        UnloadImageColors(pixels);UnloadImage(image);
    }
    ui.shutdown();
    static_cast<void>(layout);
    UnloadRenderTexture(target);
    shutdownUiText();
    CloseWindow();
}
