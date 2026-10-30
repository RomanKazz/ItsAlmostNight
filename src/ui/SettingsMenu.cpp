#include "ui/SettingsMenu.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/UiViewport.hpp"

#define GLFW_INCLUDE_NONE
#include <external/glfw/include/GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>
#include <utility>

namespace ian {
namespace {
constexpr float RowHeight = 72;
constexpr std::array PlayerActions{
    ControlAction::MoveForward, ControlAction::MoveBackward,
    ControlAction::MoveLeft, ControlAction::MoveRight,
    ControlAction::Jump, ControlAction::Sprint,
    ControlAction::Attack, ControlAction::ToggleTool, ControlAction::Interact,
    ControlAction::BuildMode, ControlAction::Bomb, ControlAction::Repair,
    ControlAction::Copy, ControlAction::Upgrade, ControlAction::Sell,
    ControlAction::UpgradeWeapon, ControlAction::Map, ControlAction::StartWave,
    ControlAction::Pause};

std::string_view tr(Language language, std::string_view en, std::string_view ru) {
    return language == Language::Russian ? ru : en;
}

struct Row {
    std::string label;
    std::string detail;
    std::string value;
    std::function<void(int)> step;
    std::function<void(float)> slide;
    float fraction{};
    std::optional<ControlAction> binding;
    int action{};
};

std::string percentage(float value) {
    return std::to_string(static_cast<int>(std::lround(value * 100))) + "%";
}

template <typename T, std::size_t N>
void cycle(T& value, const std::array<T, N>& options, int direction) {
    const auto found = std::find(options.begin(), options.end(), value);
    const int current = found == options.end() ? -1 : static_cast<int>(found - options.begin());
    value = options[static_cast<std::size_t>((current + direction + static_cast<int>(N)) % static_cast<int>(N))];
}

void resetTab(UserSettings& settings, SettingsTab tab) {
    switch (tab) {
    case SettingsTab::Video: {
        // Reset player-facing graphics without replacing the authored art style.
        applyGraphicsPreset(settings.graphics, GraphicsQuality::High);
        settings.graphics.frameRateLimit = 60;
        settings.graphics.brightness = 0;
        settings.graphics.displayWidth = 0;
        settings.graphics.displayHeight = 0;
        settings.graphics.renderWidth = 1920;
        settings.graphics.renderHeight = 1080;
        settings.graphics.fullscreen = GraphicsSettings{}.fullscreen;
        break;
    }
    case SettingsTab::Audio: settings.audio = {}; break;
    case SettingsTab::Controls: settings.controls = {}; break;
    case SettingsTab::Game:
        settings.motion = {};
        settings.accessibility = {};
        settings.debugHotkeysEnabled = false;
        break;
    }
}
} // namespace

SettingsMenuLayout settingsMenuLayout(Vector2 screen) {
    const float width = std::min(1160.0F, screen.x - 64);
    const float height = std::min(790.0F, screen.y - 64);
    const Rectangle panel{(screen.x - width) * .5F, (screen.y - height) * .5F, width, height};
    SettingsMenuLayout layout{};
    layout.panel = panel;
    layout.content = {panel.x + 264, panel.y + 112, width - 292, height - 230};
    for (int i = 0; i < 4; ++i) {
        layout.tabs[i] = {panel.x + 24, panel.y + 112 + static_cast<float>(i) * 72, 216, 58};
    }
    layout.close = {panel.x + width - 184, panel.y + height - 70, 156, 44};
    layout.reset = {panel.x + 264, panel.y + height - 70, 264, 44};
    return layout;
}

std::string rebindPlayerControl(
    ControlSettings& controls, ControlAction action, int key, Language language) {
    if (key == KEY_BACKSPACE) key = defaultControlKey(action);
    if (key == KEY_ESCAPE || (key >= KEY_F1 && key <= KEY_F12) ||
        (key >= KEY_ZERO && key <= KEY_NINE) || key == KEY_I ||
        key == KEY_Y || key == KEY_LEFT_BRACKET ||
        key == KEY_RIGHT_BRACKET || key == KEY_BACKSLASH) {
        return std::string(tr(language,
            "This key is reserved. Choose another key.",
            "Эта клавиша занята меню. Выбери другую."));
    }
    if (key != KEY_NULL) {
        for (std::size_t index = 0; index < ControlActionCount; ++index) {
            const auto other = static_cast<ControlAction>(index);
            if (other == ControlAction::UnusedLegacy) continue;
            if (other != action && controlKey(controls, other) == key) {
                return std::string(tr(language, "Already assigned: ", "Уже назначено: ")) +
                    localizeText(controlActionName(other));
            }
        }
    }
    setControlKey(controls, action, key);
    return {};
}

SettingsMenuResult drawSettingsMenu(UserSettings& settings, SettingsMenuState& state,
    Vector2 screen, const SettingsMenuInput& input) {
    const auto layout = settingsMenuLayout(screen);
    const auto language = settings.language;
    const auto t = [language](std::string_view en, std::string_view ru) { return tr(language, en, ru); };
    SettingsMenuResult result{};
    const bool progressDialogWasOpen = state.progressResetConfirmation;
    const bool modal = state.resetConfirmation || state.progressResetConfirmation;
    const bool interactive = !input.pendingBinding && !modal;
    auto selectTab = [&](int tab) {
        state.tab = static_cast<SettingsTab>((tab + 4) % 4);
        state.scroll = 0;
        state.focusedRow = -1;
        state.draggingRow = -1;
        state.bindingError.clear();
    };
    if (interactive && input.tabStep) selectTab(static_cast<int>(state.tab) + input.tabStep);

    menuStyle::panel(layout.panel, 1, menuStyle::Frame);
    const Rectangle header{layout.panel.x + 20, layout.panel.y + 20, layout.panel.width - 40, 68};
    menuStyle::panel(header, 1, menuStyle::Card);
    menuStyle::text(t("SETTINGS", "НАСТРОЙКИ"),
        {header.x + 20, header.y, header.width - 96, header.height}, 36,
        menuStyle::Cream, UiFontRole::Display);
    const Rectangle closeIcon{header.x + header.width - 56, header.y + 12, 44, 44};
    result.close = menuStyle::closeButton(closeIcon, 1, input.pointer, input.click && !modal);
    constexpr std::array<std::string_view, 4> TabsEn{"VIDEO", "AUDIO", "CONTROLS", "GAME"};
    constexpr std::array<std::string_view, 4> TabsRu{"ГРАФИКА", "ЗВУК", "УПРАВЛЕНИЕ", "ИГРА"};
    for (int i = 0; i < 4; ++i) {
        const auto bounds = layout.tabs[i];
        const bool selected = static_cast<int>(state.tab) == i;
        const bool hovered = interactive && CheckCollisionPointRec(input.pointer, bounds);
        menuStyle::panel(bounds, 1, menuStyle::selectionFill(selected, hovered));
        menuStyle::text(language == Language::Russian ? TabsRu[static_cast<std::size_t>(i)] : TabsEn[static_cast<std::size_t>(i)],
            {bounds.x + 12, bounds.y, bounds.width - 24, bounds.height}, 23,
            menuStyle::Cream, UiFontRole::Display, 1);
        if (hovered && input.click) selectTab(i);
    }
    std::vector<Row> rows;
    auto toggle = [&](std::string_view label, std::string_view detail, bool& value) {
        rows.push_back({std::string(label), std::string(detail),
            std::string(value ? t("ON", "ВКЛ") : t("OFF", "ВЫКЛ")),
            [&value](int) { value = !value; }, {}, 0, {}, 0});
    };
    auto slider = [&](std::string_view label, std::string_view detail, float& value, float min, float max) {
        const float fraction = std::clamp((value - min) / (max - min), 0.0F, 1.0F);
        rows.push_back({std::string(label), std::string(detail), percentage(value),
            [&value, min, max](int step) { value = std::clamp(value + static_cast<float>(step) * (max - min) * .05F, min, max); },
            [&value, min, max](float amount) { value = min + amount * (max - min); }, fraction, {}, 0});
    };
    auto action = [&](std::string_view label, std::string_view detail, int id) {
        rows.push_back({std::string(label), std::string(detail), std::string(t("OPEN", "ОТКРЫТЬ")), {}, {}, 0, {}, id});
    };

    auto& graphics = settings.graphics;
    switch (state.tab) {
    case SettingsTab::Video: {
        rows.push_back({std::string(t("Display mode", "Режим экрана")), {},
            std::string(graphics.fullscreen ? t("FULLSCREEN", "ПОЛНЫЙ ЭКРАН") : t("WINDOWED", "ОКНО")),
            [&graphics](int) { graphics.fullscreen = !graphics.fullscreen; }, {}, 0, {}, 0});
        const int monitor = GetCurrentMonitor();
        const int monitorWidth = GetMonitorWidth(monitor);
        const int monitorHeight = GetMonitorHeight(monitor);
        const std::pair currentResolution{
            graphics.displayWidth > 0 ? graphics.displayWidth : GetScreenWidth(),
            graphics.displayHeight > 0 ? graphics.displayHeight : GetScreenHeight()};
        std::vector<std::pair<int, int>> resolutions;
        constexpr std::array commonResolutions{
            std::pair{960, 540}, std::pair{1280, 720}, std::pair{1280, 800},
            std::pair{1366, 768}, std::pair{1440, 900}, std::pair{1600, 900},
            std::pair{1680, 1050}, std::pair{1920, 1080}, std::pair{1920, 1200},
            std::pair{2560, 1080}, std::pair{2560, 1440}, std::pair{2560, 1600},
            std::pair{3440, 1440}, std::pair{3840, 2160}};
        int monitorCount = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
        GLFWmonitor* display = monitor >= 0 && monitor < monitorCount ? monitors[monitor] : nullptr;
        if (graphics.fullscreen && display) {
            int modeCount = 0;
            const GLFWvidmode* modes = glfwGetVideoModes(display, &modeCount);
            for (int i = 0; i < modeCount; ++i) {
                if (modes[i].width >= 960 && modes[i].height >= 540)
                    resolutions.emplace_back(modes[i].width, modes[i].height);
            }
        } else {
            int availableWidth = monitorWidth;
            int availableHeight = monitorHeight;
            if (display) {
                int x = 0, y = 0;
                glfwGetMonitorWorkarea(display, &x, &y, &availableWidth, &availableHeight);
            }
            for (const auto& resolution : commonResolutions) {
                if (resolution.first <= availableWidth && resolution.second <= availableHeight)
                    resolutions.push_back(resolution);
            }
            if (availableWidth > 0 && availableHeight > 0)
                resolutions.emplace_back(availableWidth, availableHeight);
        }
        resolutions.push_back(currentResolution);
        std::sort(resolutions.begin(), resolutions.end());
        resolutions.erase(std::unique(resolutions.begin(), resolutions.end()), resolutions.end());
        rows.push_back({std::string(t("Screen resolution", "Разрешение экрана")), {},
            std::to_string(currentResolution.first) + " x " + std::to_string(currentResolution.second),
            [&graphics, resolutions, currentResolution](int step) {
                const auto found = std::find(resolutions.begin(), resolutions.end(), currentResolution);
                const int count = static_cast<int>(resolutions.size());
                const int current = static_cast<int>(found - resolutions.begin());
                const auto selected = resolutions[static_cast<std::size_t>((current + step + count) % count)];
                graphics.displayWidth = selected.first;
                graphics.displayHeight = selected.second;
                // Do not multiply a chosen output resolution by an old render-scale preference.
                graphics.renderWidth = 1920;
                graphics.renderHeight = 1080;
            }, {}, 0, {}, 0});
        const auto preset = detectGraphicsPreset(graphics);
        const auto presetLabel = !preset ? t("CUSTOM", "СВОИ") : *preset == GraphicsQuality::Low ? t("LOW", "НИЗКОЕ") :
            *preset == GraphicsQuality::Medium ? t("MEDIUM", "СРЕДНЕЕ") : t("HIGH", "ВЫСОКОЕ");
        rows.push_back({std::string(t("Graphics quality", "Качество графики")), {}, std::string(presetLabel),
            [&graphics](int step) {
                auto quality = graphics.quality;
                cycle(quality, std::array{GraphicsQuality::Low, GraphicsQuality::Medium, GraphicsQuality::High}, step);
                applyGraphicsPreset(graphics, quality);
            }, {}, 0, {}, 0});
        rows.push_back({std::string(t("Frame rate limit", "Ограничение FPS")), {},
            graphics.frameRateLimit == 0 ? std::string(t("UNLIMITED", "БЕЗ ЛИМИТА")) : std::to_string(graphics.frameRateLimit),
            [&graphics](int step) { cycle(graphics.frameRateLimit, std::array{30, 60, 90, 120, 144, 165, 240, 0}, step); }, {}, 0, {}, 0});
        slider(t("Brightness", "Яркость"), {}, graphics.brightness, -.3F, .3F);
        toggle(t("Shadows", "Тени"), {}, graphics.shadows);
        toggle(t("Grass", "Трава"), {}, graphics.grass);
        toggle(t("Performance log", "Запись performance log"),
            "performance_logs/session_*.csv", graphics.performanceLogging);
        break;
    }
    case SettingsTab::Audio:
        slider(t("Master volume", "Общая громкость"), {}, settings.audio.masterVolume, 0, 1);
        slider(t("Music", "Музыка"), {}, settings.audio.musicVolume, 0, 1);
        slider(t("Sound effects", "Звуковые эффекты"), {}, settings.audio.sfxVolume, 0, 1);
        toggle(t("Mute all sound", "Выключить звук"), {}, settings.audio.muted);
        break;
    case SettingsTab::Controls:
        slider(t("Mouse sensitivity", "Чувствительность мыши"), {}, settings.controls.mouseSensitivity, .1F, 3);
        toggle(t("Invert mouse Y", "Инверсия мыши по Y"), {}, settings.controls.invertMouseY);
        for (const auto binding : PlayerActions) {
            const int key = controlKey(settings.controls, binding);
            const bool waiting = input.pendingBinding == binding;
            std::string keyName = waiting ? std::string(t("PRESS A KEY", "НАЖМИ КЛАВИШУ")) :
                key == KEY_NULL && binding == ControlAction::Attack ? "LMB" :
                keyboardKeyName(key);
            rows.push_back({localizeText(controlActionName(binding)), {}, keyName, {}, {}, 0, binding, 0});
        }
        break;
    case SettingsTab::Game:
        rows.push_back({std::string(t("Language", "Язык")), {}, std::string(languageName(language)),
            [&settings](int) { settings.language = settings.language == Language::English ? Language::Russian : Language::English; }, {}, 0, {}, 0});
        toggle(t("Show FPS", "Показывать FPS"), {}, settings.accessibility.showFps);
        toggle(t("Reduce flashes", "Уменьшить вспышки"), {}, settings.accessibility.reduceFlashes);
        slider(t("Camera shake", "Тряска камеры"), {}, settings.motion.shakeIntensity, 0, 1);
        slider(t("Camera motion", "Движение камеры"), {}, settings.motion.bobIntensity, 0, 1);
        // Keep the weapon sway and landing response coherent with camera motion.
        rows.back().slide = [&settings](float amount) {
            settings.motion.bobIntensity = settings.motion.swayIntensity = settings.motion.landingIntensity = amount;
        };
        rows.back().step = [&settings](int step) {
            const float amount = std::clamp(settings.motion.bobIntensity + static_cast<float>(step)*.05F, 0.0F, 1.0F);
            settings.motion.bobIntensity = settings.motion.swayIntensity = settings.motion.landingIntensity = amount;
        };
        toggle(t("Debug hotkeys", "Отладочные клавиши"),
            t("Cheats, enemy spawning, debug menus and graphics shortcuts.",
              "Читы, вызов врагов, отладочные меню и графические хоткеи."), settings.debugHotkeysEnabled);
        if (input.mainMenu) {
            action(t("Menu background", "Фон меню"), {}, 1);
            action(t("Credits", "Авторы"), {}, 2);
        }
        action(t("Reset all progress", "Обнулить весь прогресс"),
            t("Quests, unlocks, silver and the saved run.", "Квесты, открытия, серебро и сохранённый забег."), 3);
        rows.back().value=std::string(t("RESET", "СБРОСИТЬ"));
        break;
    }

    const float maximumScroll = std::max(0.0F, static_cast<float>(rows.size()) * RowHeight - layout.content.height);
    if (interactive && !input.rowStep && (input.valueStep || input.activate) && state.focusedRow < 0) {
        state.focusedRow = 0;
        state.scroll = 0;
    }
    if (interactive && CheckCollisionPointRec(input.pointer, layout.content)) state.scroll -= input.wheel * 56;
    if (interactive && input.rowStep) {
        state.focusedRow = std::clamp(state.focusedRow + input.rowStep, 0, static_cast<int>(rows.size()) - 1);
        const float top = static_cast<float>(state.focusedRow) * RowHeight;
        state.scroll = std::clamp(state.scroll, top + RowHeight - layout.content.height, top);
    }
    state.scroll = std::clamp(state.scroll, 0.0F, maximumScroll);
    if (!input.pointerDown || !interactive) state.draggingRow = -1;
    menuStyle::panel(layout.content, 1, menuStyle::Inset);
    beginUiScissor(static_cast<int>(layout.content.x + 5), static_cast<int>(layout.content.y + 5),
        static_cast<int>(layout.content.width - 10), static_cast<int>(layout.content.height - 10));
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto& row = rows[i];
        const float y = layout.content.y + static_cast<float>(i) * RowHeight - state.scroll;
        const Rectangle bounds{layout.content.x + 12, y + 8, layout.content.width - 32, RowHeight - 12};
        const bool visible = bounds.y + bounds.height > layout.content.y + 5 && bounds.y < layout.content.y + layout.content.height - 5;
        const bool hovered = interactive && visible && CheckCollisionPointRec(input.pointer, layout.content) && CheckCollisionPointRec(input.pointer, bounds);
        const bool focused = state.focusedRow == static_cast<int>(i);
        const bool waiting = row.binding && input.pendingBinding == row.binding;
        if (visible) {
            DrawRectangleRec(bounds, hovered || focused || waiting ? menuStyle::Card : menuStyle::Inset);
            if (focused || waiting) DrawRectangleRec({bounds.x, bounds.y + 10, 3, bounds.height - 20}, menuStyle::Gold);
            const float labelWidth = bounds.width * .54F;
            menuStyle::text(row.label, {bounds.x + 12, bounds.y + (row.detail.empty() ? 0 : 2), labelWidth - 20,
                row.detail.empty() ? bounds.height : 28}, 22, menuStyle::Cream, UiFontRole::MenuBodyBold);
            if (!row.detail.empty()) menuStyle::text(row.detail,
                {bounds.x + 12, bounds.y + 31, labelWidth - 20, 23}, 14, menuStyle::Muted);
            const Rectangle control{bounds.x + labelWidth, bounds.y + 9, bounds.width - labelWidth - 10, 42};
            if (row.slide) {
                const Rectangle bar{control.x + 5, control.y + 14, control.width - 74, 14};
                if (hovered && input.click && CheckCollisionPointRec(input.pointer,
                    {bar.x - 10, control.y, bar.width + 20, control.height})) state.draggingRow = static_cast<int>(i);
                if (state.draggingRow == static_cast<int>(i) && input.pointerDown) {
                    row.fraction = std::clamp((input.pointer.x - bar.x) / bar.width, 0.0F, 1.0F);
                    row.slide(row.fraction);
                }
                menuStyle::progress(bar, row.fraction, menuStyle::Gold);
                menuStyle::panel({bar.x + bar.width * row.fraction - 7, bar.y - 5, 14, 24}, .6F, menuStyle::Cream);
                menuStyle::text(row.value, {bar.x + bar.width + 12, control.y, 56, control.height}, 16,
                    menuStyle::Cream, UiFontRole::MenuBodyBold, 2);
            } else {
                menuStyle::panel(control, .8F, waiting ? menuStyle::Selected : menuStyle::Card);
                menuStyle::text(row.value, {control.x + 26, control.y, control.width - 52, control.height},
                    18, menuStyle::Cream, UiFontRole::MenuBodyBold, 1);
                if (row.step) {
                    menuStyle::text("<", {control.x + 5, control.y, 20, control.height}, 20, menuStyle::Muted, UiFontRole::MenuBodyBold, 1);
                    menuStyle::text(">", {control.x + control.width - 25, control.y, 20, control.height}, 20, menuStyle::Muted, UiFontRole::MenuBodyBold, 1);
                }
            }
            const bool mouseActivate = hovered && input.click && CheckCollisionPointRec(input.pointer, control) && !row.slide;
            const bool keyActivate = interactive && focused && input.activate;
            const int step = interactive && focused ? input.valueStep : 0;
            if (hovered && input.click) state.focusedRow = static_cast<int>(i);
            if (row.step && (mouseActivate || keyActivate || step)) {
                const int direction = step ? step : mouseActivate && input.pointer.x < control.x + 26 ? -1 : 1;
                row.step(direction);
            } else if (row.binding && (mouseActivate || keyActivate)) {
                result.rebind = row.binding;
                state.bindingError.clear();
            } else if (row.action && (mouseActivate || keyActivate)) {
                result.chooseBackground = row.action == 1;
                result.credits = row.action == 2;
                if(row.action==3) {
                    state.progressResetConfirmation=true;
                    state.progressResetSelected=false;
                }
            }
        }
    }
    EndScissorMode();
    if (maximumScroll > 0) {
        const float trackHeight = layout.content.height - 24;
        const float thumbHeight = std::max(32.0F, trackHeight * layout.content.height / (maximumScroll + layout.content.height));
        DrawRectangleRec({layout.content.x + layout.content.width - 13, layout.content.y + 12, 5, trackHeight}, menuStyle::Card);
        DrawRectangleRec({layout.content.x + layout.content.width - 13,
            layout.content.y + 12 + (trackHeight - thumbHeight) * state.scroll / maximumScroll, 5, thumbHeight}, menuStyle::Gold);
    }

    result.close = menuStyle::button(layout.close, 1, t("BACK", "НАЗАД"), 21,
        input.pointer, input.click && !modal) || result.close;
    if (menuStyle::button(layout.reset, 1, t("RESET SECTION", "СБРОСИТЬ РАЗДЕЛ"), 18,
        input.pointer, input.click && interactive) || (interactive && input.reset)) state.resetConfirmation = true;
    if (!modal && !state.progressResetConfirmation && input.back) result.close = true;
    if (!state.bindingError.empty()) {
        menuStyle::text(state.bindingError,
            {layout.content.x, layout.content.y + layout.content.height + 8, layout.content.width, 24},
            15, menuStyle::Gold);
    }
    if (state.resetConfirmation) {
        const Rectangle dialog{layout.panel.x + (layout.panel.width - 560) * .5F,
            layout.panel.y + (layout.panel.height - 220) * .5F, 560, 220};
        DrawRectangleRec(layout.panel, {0, 0, 0, 150});
        menuStyle::panel(dialog, 1, menuStyle::Frame);
        menuStyle::text(t("RESET THIS SECTION?", "СБРОСИТЬ ЭТОТ РАЗДЕЛ?"),
            {dialog.x + 24, dialog.y + 24, 512, 54}, 26, menuStyle::Cream, UiFontRole::Display, 1);
        menuStyle::text(t("Other settings will stay unchanged.", "Остальные настройки сохранятся."),
            {dialog.x + 24, dialog.y + 80, 512, 30}, 18, menuStyle::Muted, UiFontRole::Body, 1);
        const bool confirm = menuStyle::button({dialog.x + 292, dialog.y + 142, 240, 48}, 1,
            t("RESET", "СБРОСИТЬ"), 20, input.pointer, input.click) || input.activate;
        const bool cancel = menuStyle::button({dialog.x + 28, dialog.y + 142, 240, 48}, 1,
            t("CANCEL", "ОТМЕНА"), 20, input.pointer, input.click) || input.back;
        if (confirm) { resetTab(settings, state.tab); state.resetConfirmation = false; }
        else if (cancel) state.resetConfirmation = false;
    }
    if(state.progressResetConfirmation) {
        const Rectangle dialog{layout.panel.x+(layout.panel.width-640)*.5F,
            layout.panel.y+(layout.panel.height-290)*.5F,640,290};
        DrawRectangleRec(layout.panel,{0,0,0,175});
        menuStyle::panel(dialog,1,menuStyle::Frame);
        menuStyle::text(t("RESET ALL PROGRESS?","ОБНУЛИТЬ ВЕСЬ ПРОГРЕСС?"),
            {dialog.x+24,dialog.y+22,592,48},27,{255,122,100,255},UiFontRole::Display,1);
        menuStyle::text(t("Quests, unlocks, purchases and silver will be erased.",
            "Квесты, открытия, покупки и серебро будут удалены."),
            {dialog.x+24,dialog.y+88,592,28},19,menuStyle::Cream,UiFontRole::Body,1);
        menuStyle::text(t("The current run will end. This cannot be undone.",
            "Текущий забег завершится. Отменить сброс нельзя."),
            {dialog.x+24,dialog.y+121,592,28},19,menuStyle::Cream,UiFontRole::Body,1);
        menuStyle::text(t("Video, sound and controls will stay unchanged.",
            "Настройки графики, звука и управления сохранятся."),
            {dialog.x+24,dialog.y+159,592,25},17,menuStyle::Muted,UiFontRole::Body,1);
        if(progressDialogWasOpen && input.valueStep) state.progressResetSelected=input.valueStep>0;
        if(progressDialogWasOpen && input.tabStep) state.progressResetSelected=!state.progressResetSelected;
        const Rectangle cancel{dialog.x+24,dialog.y+216,280,48},confirm{dialog.x+336,dialog.y+216,280,48};
        const bool confirmClicked=menuStyle::button(confirm,1,t("ERASE PROGRESS","УДАЛИТЬ ПРОГРЕСС"),19,
            input.pointer,progressDialogWasOpen && input.click);
        const bool cancelClicked=menuStyle::button(cancel,1,t("CANCEL","ОТМЕНА"),21,
            input.pointer,progressDialogWasOpen && input.click);
        DrawRectangleLinesEx(state.progressResetSelected?confirm:cancel,2,menuStyle::Gold);
        if(progressDialogWasOpen && (input.back || cancelClicked || (input.activate && !state.progressResetSelected)))
            state.progressResetConfirmation=false;
        else if(progressDialogWasOpen && (confirmClicked || (input.activate && state.progressResetSelected))) {
            result.resetProgress=true;
            state.progressResetConfirmation=false;
        }
    }
    return result;
}
} // namespace ian
