#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "localization/Localization.hpp"
#include "ui/UiViewport.hpp"

#include <nlohmann/json.hpp>
#include <rlgl.h>

namespace ian {

void App::beginFeedback() {
    if (feedback_.active()) return;
    const auto& snapshot = simulation_.snapshot();
    const auto& position = snapshot.playerPosition;
    const nlohmann::json context{
        {"format_version", 1}, {"game_version", IAN_GAME_VERSION},
        {"build", __DATE__ " " __TIME__},
#if defined(_WIN32)
        {"platform", "Windows"},
#elif defined(__APPLE__)
        {"platform", "macOS"},
#else
        {"platform", "Linux"},
#endif
        {"world_seed", simulation_.terrain().seed()},
        {"run_state", static_cast<int>(snapshot.state)},
        {"player_class", static_cast<int>(snapshot.playerClass)},
        {"wave", snapshot.wave}, {"tick", snapshot.tick},
        {"elapsed_seconds", snapshot.elapsedSeconds},
        {"position", {position.x, position.y, position.z}},
        {"yaw", snapshot.playerYaw}, {"pitch", snapshot.playerPitch},
        {"sandbox", snapshot.sandboxMode},
        {"debug_hotkeys", userSettings_.debugHotkeysEnabled},
        {"fps", GetFPS()},
        {"resolution", {GetRenderWidth(), GetRenderHeight()}},
        {"graphics", {{"quality", static_cast<int>(renderer_->settings().quality)},
            {"fullscreen", renderer_->settings().fullscreen}, {"shadows", renderer_->settings().shadows},
            {"post_processing", renderer_->settings().postProcessing}, {"ssao", renderer_->settings().ssao},
            {"ink_outlines", renderer_->settings().inkOutlines}, {"outline_width", renderer_->settings().outlineWidth},
            {"exposure", renderer_->settings().postExposure}, {"gamma", renderer_->settings().gamma},
            {"saturation", renderer_->settings().colorSaturation}}},
    };
    feedback_.begin(context.dump());
    resumeAfterFeedback_ = snapshot.state != RunState::Paused && snapshot.state != RunState::MainMenu;
    if (resumeAfterFeedback_) simulation_.togglePause();
    fixedStep_.reset();
    suspendGameplayInput();
    input_ = {};
    restartHold_.cancel(false);
    settingsRebindConsumedInput_ = true;
    EnableCursor();
    // Discard the opening key rather than replaying it into a rebind or editor.
    while (GetCharPressed() != 0) {}
    while (GetKeyPressed() != 0) {}
}

void App::closeFeedback() {
    // Keep the frozen form for this final frame so the closing click/Enter
    // cannot activate an underlying pause, upgrade or settings control.
    feedbackClosing_ = feedback_.ready();
    if (resumeAfterFeedback_ && simulation_.snapshot().state == RunState::Paused) simulation_.togglePause();
    resumeAfterFeedback_ = false;
    fixedStep_.reset();
    input_ = {};
    suspendGameplayInput();
    settingsRebindConsumedInput_ = true;
    mainMenuInputBarrier_.blockUntilRelease();
    while (GetCharPressed() != 0) {}
    while (GetKeyPressed() != 0) {}
    if (!feedbackClosing_) finishFeedbackClose();
}

void App::finishFeedbackClose() {
    feedback_.clear();
    feedbackClosing_ = false;
    input_ = {};
    suspendGameplayInput();
    fixedStep_.reset();
    settingsRebindConsumedInput_ = true;
    // HideCursor in the outgoing modal changes GLFW_CURSOR_DISABLED to HIDDEN.
    // Restore relative mouse capture only after that final modal frame is gone.
    const auto& snapshot = simulation_.snapshot();
    const bool pointerNeeded = !app_detail::acceptsGameplayInput(snapshot.state) ||
        snapshot.runUpgradeChoicePending || snapshot.runShop.pending ||
        renderer_->graphicsPanelVisible() || enemySpawnMenuVisible_ || itemGrantMenuVisible_ ||
        sandboxCardMenuVisible_ || coreDefenseMenuVisible_ || objectiveDebugMenuVisible_;
    if (pointerNeeded) EnableCursor();
    else DisableCursor();
    while (GetCharPressed() != 0) {}
    while (GetKeyPressed() != 0) {}
}

void App::processFeedbackInput() {
    const ScopedUiCoordinates coordinates;
    const bool command = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
        IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    const auto pressed = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
    TesterFeedback::Input input{
        .pointer = uiMousePosition(),
        .click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        .close = IsKeyPressed(KEY_ESCAPE),
        .save = command && IsKeyPressed(KEY_ENTER),
        .backspace = pressed(KEY_BACKSPACE),
        .erase = pressed(KEY_DELETE),
        .selectAll = command && IsKeyPressed(KEY_A),
        .shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
        .home = pressed(KEY_HOME), .end = pressed(KEY_END),
        .cursorStep = pressed(KEY_LEFT) ? -1 : pressed(KEY_RIGHT) ? 1 : 0,
        .wheel = GetMouseWheelMove(),
        .text = {},
    };
    if (command && IsKeyPressed(KEY_V)) {
        if (const char* text = GetClipboardText()) input.text = text;
    }
    for (int code = GetCharPressed(); code != 0; code = GetCharPressed()) {
        if (command || code < 32) continue;
        int bytes = 0;
        const char* value = CodepointToUTF8(code, &bytes);
        if (bytes > 0) input.text.append(value, static_cast<std::size_t>(bytes));
    }
    if (!command && IsKeyPressed(KEY_ENTER)) input.text += '\n';
    const auto action = feedback_.input(input,
        {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())});
    switch (action) {
        case TesterFeedback::Action::Close:
            audio_.playUiSelect();
            closeFeedback();
            break;
        case TesterFeedback::Action::Save:
            if (feedback_.save("feedback")) audio_.playUiConfirm();
            else audio_.playUiError();
            break;
        case TesterFeedback::Action::OpenFolder:
            OpenURL(TesterFeedback::folderUrl("feedback").c_str());
            break;
        case TesterFeedback::Action::None: break;
    }
}

void App::captureFeedbackFrame() {
    // Read the completed back buffer before EndDrawing swaps it. No readback
    // or screenshot texture exists during ordinary play.
    rlDrawRenderBatchActive();
    if (feedback_.capture(LoadImageFromScreen())) audio_.playFeedbackCapture();
    else {
        closeFeedback();
        statusMessage_ = currentLanguage() == Language::Russian ? "Не удалось сделать скриншот" : "Could not capture screenshot";
        statusMessageRemaining_ = 4.0;
        audio_.playUiError();
    }
}

void App::drawFeedback() const {
    const ScopedUiCanvas canvas;
    feedback_.draw({static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())},
        currentLanguage() == Language::Russian, userSettings_.accessibility.reduceFlashes);
    HideCursor();
    ui_.drawCursor();
}

} // namespace ian
