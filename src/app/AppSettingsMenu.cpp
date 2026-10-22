#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "ui/UiViewport.hpp"

namespace ian {

void App::openSettings(bool debug) {
    const auto snapshot = simulation_.snapshot();
    if (snapshot.runUpgradeChoicePending) return;
    if (!debug && app_detail::acceptsGameplayInput(snapshot.state)) {
        simulation_.togglePause();
        fixedStep_.reset();
        resumeAfterSettings_ = true;
    }
    debugSettings_ = debug;
    // Opening from another menu must not replay its click/Enter on the first option.
    settingsRebindConsumedInput_ = true;
    settingsMenuState_.resetConfirmation = false;
    settingsMenuState_.draggingRow = -1;
    settingsMenuState_.bindingError.clear();
    pendingControlRebind_.reset();
    enemySpawnMenuVisible_ = false;
    itemGrantMenuVisible_ = false;
    sandboxCardMenuVisible_ = false;
    coreDefenseMenuVisible_ = false;
    coreMenuPointerActive_ = false;
    if (!debug) renderer_->setShadowMapVisible(false);
    renderer_->setGraphicsPanelVisible(true);
    EnableCursor();
    audio_.playUiConfirm();
}

void App::closeSettings() {
    renderer_->setGraphicsPanelVisible(false);
    pendingControlRebind_.reset();
    settingsMenuState_.resetConfirmation = false;
    settingsMenuState_.draggingRow = -1;
    persistUserSettings(true);
    if (resumeAfterSettings_) {
        resumeAfterSettings_ = false;
        if (simulation_.snapshot().state == RunState::Paused) {
            simulation_.togglePause();
            fixedStep_.reset();
            DisableCursor();
        }
    }
    audio_.playUiConfirm();
}

void App::drawPlayerSettingsPanel() {
    const ScopedUiCanvas canvas;
    UserSettings settings{
        .graphics = renderer_->settings(),
        .audio = audio_.settings(),
        .motion = {motionBobIntensity_, motionShakeIntensity_,
                   motionLandingIntensity_, motionSwayIntensity_},
        .controls = userSettings_.controls,
        .accessibility = userSettings_.accessibility,
        .language = userSettings_.language,
    };
    const auto previous = settings;
    const auto previousTab = settingsMenuState_.tab;
    const int previousRow = settingsMenuState_.focusedRow;
    const bool keysEnabled = !settingsRebindConsumedInput_ && !pendingControlRebind_;
    const auto navigation = readMenuNavigationInput();
    const bool mainMenu = simulation_.snapshot().state == RunState::MainMenu;
    const SettingsMenuInput input{
        .pointer = uiMousePosition(),
        .wheel = GetMouseWheelMove(),
        .click = !settingsRebindConsumedInput_ && navigation.click,
        .pointerDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
        .tabStep = keysEnabled ? navigation.tabStep : 0,
        .rowStep = keysEnabled ? navigation.verticalStep : 0,
        .valueStep = keysEnabled ? navigation.horizontalStep : 0,
        .activate = keysEnabled && navigation.activate,
        .back = keysEnabled && IsKeyPressed(KEY_ESCAPE),
        .reset = keysEnabled && IsKeyPressed(KEY_DELETE),
        .mainMenu = mainMenu,
        .pendingBinding = pendingControlRebind_,
    };
    const auto result = drawSettingsMenu(settings, settingsMenuState_,
        {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())}, input);
    renderer_->settings() = settings.graphics;
    if (previous.graphics.frameRateLimit != settings.graphics.frameRateLimit) renderer_->applyFrameRateLimit();
    audio_.settings() = settings.audio;
    if (previous.audio != settings.audio) audio_.applySettings();
    motionBobIntensity_ = settings.motion.bobIntensity;
    motionShakeIntensity_ = settings.motion.shakeIntensity;
    motionLandingIntensity_ = settings.motion.landingIntensity;
    motionSwayIntensity_ = settings.motion.swayIntensity;
    userSettings_.controls = settings.controls;
    userSettings_.accessibility = settings.accessibility;
    userSettings_.language = settings.language;
    if (previous.language != settings.language) setLanguage(settings.language);
    if (result.rebind) pendingControlRebind_ = result.rebind;
    if (!result.close && !result.chooseBackground && !result.credits &&
        (input.click || input.tabStep || input.rowStep) &&
        (previousTab != settingsMenuState_.tab || previousRow != settingsMenuState_.focusedRow) &&
        previous == settings && !result.rebind) audio_.playUiSelect();
    if ((previous != settings || result.rebind) &&
        (input.click || input.activate || input.valueStep || input.reset)) audio_.playUiConfirm();
    if (result.close || result.chooseBackground || result.credits) closeSettings();
    if (result.chooseBackground && mainMenu) beginMenuBackgroundEdit(simulation_.snapshot());
    if (result.credits && mainMenu) mainMenuPage_ = MainMenuPage::Credits;
}

} // namespace ian
