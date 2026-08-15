#include "app/App.hpp"
#include "localization/Localization.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace ian {
namespace {

enum SettingsPage {
    PresetPage,
    DisplayPage,
    ColorPage,
    StylePage,
    MotionPage,
    AudioPage,
    ControlsPage,
    AccessibilityPage,
    ToolPage,
};

const char* presetName(
    std::optional<GraphicsQuality> preset) {
    if (!preset) {
        return "CUSTOM";
    }
    switch (*preset) {
    case GraphicsQuality::Low:
        return "LOW";
    case GraphicsQuality::Medium:
        return "MEDIUM";
    case GraphicsQuality::High:
        return "HIGH";
    }
    return "CUSTOM";
}

} // namespace

void App::drawGraphicsPanel() {
    auto& settings = renderer_->settings();
    DrawRectangle(
        0, 0, GetScreenWidth(), GetScreenHeight(),
        {7, 10, 14, 190});
    constexpr float PanelWidth = 900.0F;
    const bool compact = GetScreenHeight() < 900;
    const float PanelHeight = std::min(
        compact ? static_cast<float>(GetScreenHeight()) - 32.0F
                : 920.0F,
        static_cast<float>(GetScreenHeight()) - 56.0F);
    const float Margin = compact ? 16.0F : 28.0F;
    const float Gap = compact ? 8.0F : 14.0F;
    const float ButtonHeight = compact ? 38.0F : 58.0F;
    const float ControlStartY = compact ? 220.0F : 286.0F;
    const float RowHeight = compact ? 44.0F : 62.0F;
    const float panelX = std::max(
        Margin,
        (static_cast<float>(GetScreenWidth()) - PanelWidth) * 0.5F);
    const float panelY = 28.0F;
    const float contentX = panelX + 28.0F;
    const float contentWidth = PanelWidth - 56.0F;
    const float columnWidth = (contentWidth - Gap) * 0.5F;

    ui_.drawPanel({panelX, panelY, PanelWidth, PanelHeight}, 250);
    const float headerY = compact ? 40.0F : 56.0F;
    const float headerHeight = compact ? 56.0F : 76.0F;
    ui_.drawInsetPanel(
        {contentX, panelY + headerY, contentWidth, headerHeight}, 245);
    ui_.drawLabel(
        {contentX + 16.0F, panelY + headerY + 8.0F,
         contentWidth - 32.0F, headerHeight - 16.0F},
        "SETTINGS", 1);

    const float TabY = compact ? 108.0F : 150.0F;
    const float TabHeight = compact ? 40.0F : 52.0F;
    const float tabWidth =
        (contentWidth - Gap * 4.0F) / 5.0F;
    constexpr std::array TabNames{
        "PRESET", "DISPLAY", "COLOR", "STYLE LAB", "MOTION",
        "AUDIO", "CONTROLS", "ACCESSIBILITY", "TOOL"};
    for (int page = PresetPage; page <= ToolPage; ++page) {
        const int row = page < 5 ? 0 : 1;
        const int column = page < 5 ? page : page - 5;
        const Rectangle bounds{
            contentX + static_cast<float>(column) * (tabWidth + Gap),
            panelY + TabY +
                static_cast<float>(row) * (TabHeight + Gap),
            tabWidth, TabHeight};
        if (ui_.drawToggleButton(
                bounds, TabNames[static_cast<std::size_t>(page)],
                graphicsPanelTab_ == page)) {
            graphicsPanelTab_ = page;
        }
    }
    if (ui_.drawButton(
            {contentX + (tabWidth + Gap) * 4.0F,
             panelY + TabY + TabHeight + Gap,
             tabWidth, TabHeight},
            "CLOSE")) {
        renderer_->setGraphicsPanelVisible(false);
    }

    const auto toggleButton =
        [this, contentX, columnWidth, Gap, ButtonHeight](
            int column, float y, const char* name, bool& value) {
            const Rectangle bounds{
                contentX +
                    static_cast<float>(column) * (columnWidth + Gap),
                y, columnWidth, ButtonHeight};
            const std::string label =
                std::string(name) + (value ? ": ON" : ": OFF");
            if (ui_.drawToggleButton(bounds, label, value)) {
                value = !value;
            }
        };

    if (graphicsPanelTab_ == PresetPage) {
        const auto currentPreset = detectGraphicsPreset(settings);
        const float y = panelY + ControlStartY;
        ui_.drawInsetPanel(
            {contentX, y, contentWidth,
             compact ? 76.0F : 104.0F}, 245);
        drawUiText(
            std::string("GRAPHICS PRESET: ") +
                presetName(currentPreset),
            {contentX + 22.0F, y + 16.0F},
            compact ? 20.0F : 25.0F,
            {255, 235, 174, 255});
        drawUiText(
            "Changes performance options together. Color, style and controls stay yours.",
            {contentX + 22.0F,
             y + (compact ? 45.0F : 58.0F)},
            compact ? 13.0F : 16.0F,
            {199, 174, 142, 255});

        const float cardY =
            y + (compact ? 90.0F : 124.0F);
        const float cardGap = Gap;
        const float cardWidth =
            (contentWidth - cardGap * 2.0F) / 3.0F;
        constexpr std::array Presets{
            GraphicsQuality::Low,
            GraphicsQuality::Medium,
            GraphicsQuality::High};
        constexpr std::array PresetLabels{
            "LOW\nMAXIMUM FPS",
            "MEDIUM\nRECOMMENDED",
            "HIGH\nBEST QUALITY"};
        for (std::size_t index = 0; index < Presets.size(); ++index) {
            if (ui_.drawToggleButton(
                    {contentX + static_cast<float>(index) *
                                    (cardWidth + cardGap),
                     cardY, cardWidth,
                     compact ? 84.0F : 112.0F},
                    PresetLabels[index],
                    currentPreset && *currentPreset == Presets[index])) {
                applyGraphicsPreset(settings, Presets[index]);
            }
        }
        const float noteY =
            cardY + (compact ? 102.0F : 136.0F);
        drawUiText(
            "Preset applies instantly. Fine-tune any option in Display, Color, Style or Motion.",
            {contentX, noteY}, compact ? 14.0F : 17.0F,
            {245, 220, 174, 255});
    } else if (graphicsPanelTab_ == DisplayPage) {
    float y = panelY + ControlStartY;
    if (ui_.drawToggleButton(
            {contentX, y, contentWidth, ButtonHeight},
            settings.fullscreen
                ? "FULLSCREEN: ON"
                : "FULLSCREEN: OFF",
            settings.fullscreen)) {
        settings.fullscreen = !settings.fullscreen;
    }
    y += ButtonHeight + Gap;
    toggleButton(0, y, "SHADOWS", settings.shadows);
    toggleButton(1, y, "FOG", settings.fog);
    y += ButtonHeight + Gap;
    toggleButton(0, y, "PIXEL PIPELINE",
                 settings.postProcessing);
    toggleButton(1, y, "PARTICLES", settings.particles);
    y += ButtonHeight + Gap;
    toggleButton(0, y, "CONTACT AO", settings.blobShadows);
    toggleButton(1, y, "WORLD SHADER", settings.worldShader);
    y += ButtonHeight + Gap;
    toggleButton(0, y, "PROCEDURAL SKY", settings.sky);
    toggleButton(1, y, "INSTANCED GRASS", settings.grass);
    y += ButtonHeight + Gap;
    toggleButton(0, y, "PIXEL BLOOM", settings.bloom);
    toggleButton(1, y, "SCREEN-SPACE AO", settings.ssao);
    y += ButtonHeight + Gap;
    if (ui_.drawButton(
            {contentX, y, columnWidth,
             ButtonHeight},
            std::string("QUALITY: ") +
                (settings.quality == GraphicsQuality::Low
                     ? "LOW"
                     : settings.quality == GraphicsQuality::Medium
                           ? "MEDIUM"
                           : "HIGH"))) {
        renderer_->cycleQuality();
    }
    if (ui_.drawButton(
            {contentX + columnWidth + Gap, y,
             columnWidth, ButtonHeight},
            std::string("SHADOW QUALITY: ") +
                (settings.shadowMapSize <= 512
                     ? "LOW"
                     : settings.shadowMapSize <= 1024
                           ? "MEDIUM"
                           : "HIGH"))) {
        renderer_->cycleShadowQuality();
    }
    y += ButtonHeight + Gap;

    const float smallButtonWidth = 90.0F;
    if (ui_.drawButton(
            {contentX, y, smallButtonWidth, ButtonHeight}, "-")) {
        renderer_->adjustPixelSize(-1);
    }
    ui_.drawInsetPanel(
        {contentX + smallButtonWidth + Gap, y,
         columnWidth - (smallButtonWidth + Gap) * 2.0F,
         ButtonHeight}, 245);
    ui_.drawLabel(
        {contentX + smallButtonWidth + Gap, y,
         columnWidth - (smallButtonWidth + Gap) * 2.0F,
         ButtonHeight},
        std::string("PIXEL: ") + std::to_string(settings.pixelSize) +
            "X",
        1);
    if (ui_.drawButton(
            {contentX + columnWidth - smallButtonWidth, y,
             smallButtonWidth, ButtonHeight},
            "+")) {
        renderer_->adjustPixelSize(1);
    }
    if (ui_.drawButton(
            {contentX + columnWidth + Gap, y, columnWidth,
             ButtonHeight},
            std::string("AO: ") +
                std::to_string(
                    static_cast<int>(settings.aoStrength * 100.0F)) +
                "%")) {
        renderer_->cycleAoStrength();
    }
    y += ButtonHeight + Gap;

    const std::string frameRateLabel =
        settings.frameRateLimit == 0
            ? "FPS LIMIT: UNLIMITED"
            : std::string("FPS LIMIT: ") +
                  std::to_string(settings.frameRateLimit);
    if (ui_.drawButton(
            {contentX, y, contentWidth, ButtonHeight},
            frameRateLabel)) {
        renderer_->cycleFrameRateLimit();
    }
    y += ButtonHeight + Gap;

    if (ui_.drawButton(
            {contentX, y, columnWidth, ButtonHeight},
            "RESET DISPLAY")) {
        resetDisplaySettings(settings);
        renderer_->applyFrameRateLimit();
    }
    if (ui_.drawButton(
            {contentX + columnWidth + Gap, y, columnWidth,
             ButtonHeight},
            "CLOSE [F2]")) {
        renderer_->setGraphicsPanelVisible(false);
    }
    } else if (graphicsPanelTab_ == ColorPage) {
        const auto colorSlider =
            [this, contentX, columnWidth, panelY, Gap,
             ControlStartY, RowHeight, compact](
                int column, int row, const char* label,
                float& value, float minimum, float maximum) {
                const float x =
                    contentX +
                    static_cast<float>(column) *
                        (columnWidth + Gap);
                const float y =
                    panelY + ControlStartY +
                    static_cast<float>(row) * RowHeight;
                drawUiText(
                    TextFormat(
                        "%s  %.2f", label, value),
                    {x, y}, 15.0F,
                    {245, 220, 174, 255});
                ui_.drawInsetPanel(
                    {x, y + (compact ? 20.0F : 29.0F),
                     columnWidth, compact ? 27.0F : 36.0F},
                    235);
                value = ui_.drawSliderBar(
                    {x + 8.0F, y + (compact ? 23.0F : 35.0F),
                     columnWidth - 16.0F,
                     compact ? 20.0F : 24.0F},
                    value, minimum, maximum);
            };
        colorSlider(
            0, 0, "EXPOSURE", settings.postExposure,
            -2.0F, 2.0F);
        colorSlider(
            0, 1, "BRIGHTNESS", settings.brightness,
            -0.5F, 0.5F);
        colorSlider(
            0, 2, "CONTRAST", settings.contrast,
            0.5F, 1.8F);
        colorSlider(
            0, 3, "SATURATION",
            settings.colorSaturation, 0.0F, 2.0F);
        colorSlider(
            0, 4, "HUE", settings.hueDegrees,
            -180.0F, 180.0F);
        colorSlider(
            0, 5, "TEMPERATURE",
            settings.temperature, -1.0F, 1.0F);
        colorSlider(
            0, 6, "TINT", settings.tint,
            -1.0F, 1.0F);

        colorSlider(
            1, 0, "GAMMA", settings.gamma,
            0.5F, 2.2F);
        colorSlider(
            1, 1, "BLACK POINT",
            settings.blackPoint, 0.0F, 0.25F);
        colorSlider(
            1, 2, "CURVE: SHADOWS",
            settings.curveShadows, -1.0F, 1.0F);
        colorSlider(
            1, 3, "CURVE: MIDTONES",
            settings.curveMidtones, -1.0F, 1.0F);
        colorSlider(
            1, 4, "CURVE: HIGHLIGHTS",
            settings.curveHighlights, -1.0F, 1.0F);
        colorSlider(
            1, 5, "SHARPNESS",
            settings.sharpness, 0.0F, 1.0F);
        colorSlider(
            1, 6, "VIGNETTE",
            settings.vignette, 0.0F, 0.7F);

        const float ActionY =
            ControlStartY + RowHeight * 7.0F +
            (compact ? 4.0F : 12.0F);
        if (ui_.drawButton(
                {contentX, panelY + ActionY,
                 columnWidth, ButtonHeight},
                "RESET COLOR")) {
            resetColorSettings(settings);
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap,
                 panelY + ActionY,
                 columnWidth, ButtonHeight},
                "CLOSE [F2]")) {
            renderer_->setGraphicsPanelVisible(false);
        }
        settings.postProcessing = true;
    } else if (graphicsPanelTab_ == StylePage) {
        const auto styleSlider =
            [this, contentX, columnWidth, panelY, Gap,
             ControlStartY, RowHeight, compact](
                int row, const char* label, float& value,
                float minimum, float maximum) {
                const float x =
                    contentX + columnWidth + Gap;
                const float y =
                    panelY + ControlStartY +
                    static_cast<float>(row) * RowHeight;
                drawUiText(
                    TextFormat("%s  %.2f", label, value),
                    {x, y}, 15.0F,
                    {245, 220, 174, 255});
                ui_.drawInsetPanel(
                    {x, y + (compact ? 20.0F : 29.0F),
                     columnWidth, compact ? 27.0F : 36.0F},
                    235);
                value = ui_.drawSliderBar(
                    {x + 8.0F, y + (compact ? 23.0F : 35.0F),
                     columnWidth - 16.0F,
                     compact ? 20.0F : 24.0F},
                    value, minimum, maximum);
            };

        float y = panelY + ControlStartY;
        toggleButton(0, y, "LIMITED PALETTE",
                     settings.paletteQuantization);
        styleSlider(0, "COLOR LEVELS", settings.paletteLevels,
                    2.0F, 16.0F);
        y += RowHeight;
        toggleButton(0, y, "BAYER DITHER",
                     settings.dithering);
        styleSlider(1, "DITHER STRENGTH",
                    settings.ditherStrength, 0.0F, 1.0F);
        y += RowHeight;
        toggleButton(0, y, "TOON SHADING",
                     settings.posterizedLighting);
        styleSlider(2, "TOON BANDS", settings.lightingSteps,
                    2.0F, 12.0F);
        y += RowHeight;
        toggleButton(0, y, "PIXEL BLOOM", settings.bloom);
        styleSlider(3, "BLOOM STRENGTH",
                    settings.bloomStrength, 0.0F, 1.0F);
        y += RowHeight;
        toggleButton(0, y, "INK OUTLINES",
                     settings.inkOutlines);
        styleSlider(4, "OUTLINE STRENGTH",
                    settings.outlineStrength, 0.0F, 1.0F);
        y += RowHeight;
        styleSlider(5, "OUTLINE WIDTH",
                    settings.outlineWidth, 1.0F, 4.0F);
        y += RowHeight;
        toggleButton(0, y, "FOG BANDS", settings.fogBands);
        styleSlider(6, "FOG LEVELS", settings.fogBandCount,
                    2.0F, 12.0F);
        y += RowHeight;
        toggleButton(0, y, "PAPER GRAIN",
                     settings.paperGrain);
        styleSlider(7, "GRAIN STRENGTH",
                    settings.paperGrainStrength, 0.0F, 0.15F);

        const float ActionY =
            ControlStartY + RowHeight * 8.0F +
            (compact ? 4.0F : 12.0F);
        if (ui_.drawButton(
                {contentX, panelY + ActionY,
                 columnWidth, ButtonHeight},
                "SIGNATURE PRESET")) {
            settings.paletteQuantization = true;
            settings.dithering = true;
            settings.posterizedLighting = true;
            settings.bloom = true;
            settings.inkOutlines = true;
            settings.fogBands = true;
            settings.paperGrain = true;
            settings.paletteLevels = 9.0F;
            settings.ditherStrength = 0.42F;
            settings.lightingSteps = 6.0F;
            settings.bloomStrength = 0.24F;
            settings.outlineStrength = 0.3F;
            settings.outlineWidth = 1.25F;
            settings.fogBandCount = 6.0F;
            settings.paperGrainStrength = 0.025F;
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap,
                 panelY + ActionY,
                 columnWidth, ButtonHeight},
                "RESET STYLE")) {
            resetStyleSettings(settings);
        }
        settings.postProcessing = true;
    } else if (graphicsPanelTab_ == MotionPage) {
        const auto motionSlider =
            [this, contentX, contentWidth, panelY,
             ControlStartY, RowHeight, compact](
                int row, const char* label, float& value) {
                const float y =
                    panelY + ControlStartY +
                    static_cast<float>(row) * RowHeight;
                drawUiText(
                    TextFormat("%s  %d%%", label,
                               static_cast<int>(
                                   std::lround(value * 100.0F))),
                    {contentX, y}, 15.0F,
                    {245, 220, 174, 255});
                ui_.drawInsetPanel(
                    {contentX,
                     y + (compact ? 20.0F : 29.0F),
                     contentWidth,
                     compact ? 27.0F : 36.0F},
                    235);
                value = ui_.drawSliderBar(
                    {contentX + 8.0F,
                     y + (compact ? 23.0F : 35.0F),
                     contentWidth - 16.0F,
                     compact ? 20.0F : 24.0F},
                    value, 0.0F, 1.5F);
            };
        motionSlider(
            0, "CAMERA BOB", motionBobIntensity_);
        motionSlider(
            1, "CAMERA SHAKE", motionShakeIntensity_);
        motionSlider(
            2, "LANDING RESPONSE", motionLandingIntensity_);
        motionSlider(
            3, "LOOK & STRAFE SWAY", motionSwayIntensity_);

        const float actionY =
            panelY + ControlStartY + RowHeight * 4.0F +
            (compact ? 8.0F : 16.0F);
        if (ui_.drawButton(
                {contentX, actionY, columnWidth, ButtonHeight},
                "RESET MOTION")) {
            const MotionSettings defaults;
            motionBobIntensity_ = defaults.bobIntensity;
            motionShakeIntensity_ = defaults.shakeIntensity;
            motionLandingIntensity_ = defaults.landingIntensity;
            motionSwayIntensity_ = defaults.swayIntensity;
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap, actionY,
                 columnWidth, ButtonHeight},
                "DISABLE MOTION")) {
            motionBobIntensity_ = 0.0F;
            motionShakeIntensity_ = 0.0F;
            motionLandingIntensity_ = 0.0F;
            motionSwayIntensity_ = 0.0F;
        }
    } else if (graphicsPanelTab_ == AudioPage) {
        auto& audioSettings = audio_.settings();
        float y = panelY + ControlStartY;
        if (ui_.drawToggleButton(
                {contentX, y, contentWidth, ButtonHeight},
                audioSettings.muted ? "ALL SOUND: MUTED"
                                    : "ALL SOUND: ON",
                !audioSettings.muted)) {
            audioSettings.muted = !audioSettings.muted;
        }
        y += ButtonHeight + (compact ? 14.0F : 22.0F);
        const auto volumeSlider =
            [this, contentX, contentWidth, compact](
                float yPosition, const char* label,
                float& value) {
                drawUiText(
                    TextFormat(
                        "%s  %d%%", label,
                        static_cast<int>(
                            std::lround(value * 100.0F))),
                    {contentX, yPosition}, 16.0F,
                    {245, 220, 174, 255});
                ui_.drawInsetPanel(
                    {contentX, yPosition + 25.0F,
                     contentWidth, compact ? 30.0F : 38.0F},
                    235);
                value = ui_.drawSliderBar(
                    {contentX + 10.0F, yPosition + 31.0F,
                     contentWidth - 20.0F,
                     compact ? 18.0F : 24.0F},
                    value, 0.0F, 1.0F);
            };
        const float sliderStep = compact ? 66.0F : 86.0F;
        volumeSlider(y, "MASTER VOLUME", audioSettings.masterVolume);
        y += sliderStep;
        volumeSlider(y, "SOUND EFFECTS", audioSettings.sfxVolume);
        y += sliderStep;
        volumeSlider(y, "MUSIC", audioSettings.musicVolume);
        y += sliderStep + 8.0F;
        if (ui_.drawButton(
                {contentX, y, contentWidth, ButtonHeight},
                "RESET AUDIO")) {
            audioSettings = {};
        }
        audio_.applySettings();
    } else if (graphicsPanelTab_ == ControlsPage) {
        auto& controls = userSettings_.controls;
        float y = panelY + ControlStartY;
        drawUiText(
            TextFormat(
                "MOUSE SENSITIVITY  %d%%",
                static_cast<int>(
                    std::lround(controls.mouseSensitivity * 100.0F))),
            {contentX, y}, 17.0F,
            {245, 220, 174, 255});
        ui_.drawInsetPanel(
            {contentX, y + 28.0F, contentWidth,
             compact ? 32.0F : 40.0F}, 235);
        controls.mouseSensitivity = ui_.drawSliderBar(
            {contentX + 10.0F, y + 34.0F,
             contentWidth - 20.0F,
             compact ? 20.0F : 26.0F},
            controls.mouseSensitivity, 0.1F, 3.0F);
        y += compact ? 78.0F : 98.0F;
        if (ui_.drawToggleButton(
                {contentX, y, contentWidth, ButtonHeight},
                controls.invertMouseY
                    ? "INVERT MOUSE Y: ON"
                    : "INVERT MOUSE Y: OFF",
                controls.invertMouseY)) {
            controls.invertMouseY = !controls.invertMouseY;
        }
        y += ButtonHeight + Gap;
        drawUiText(
            "CLICK A KEY TO REBIND  •  ESC CANCELS  •  LMB REMAINS PRIMARY",
            {contentX, y}, compact ? 11.0F : 14.0F,
            {199, 174, 142, 255});
        y += compact ? 22.0F : 30.0F;

        constexpr std::array<ControlAction, ControlActionCount>
            Actions{
                ControlAction::MoveForward,
                ControlAction::MoveLeft,
                ControlAction::MoveBackward,
                ControlAction::MoveRight,
                ControlAction::Jump,
                ControlAction::Sprint,
                ControlAction::Dash,
                ControlAction::Attack,
                ControlAction::ToggleTool,
                ControlAction::Interact,
                ControlAction::Bomb,
                ControlAction::Repair,
                ControlAction::Copy,
                ControlAction::Upgrade,
                ControlAction::Sell,
                ControlAction::UpgradeWeapon,
                ControlAction::BuildMode,
                ControlAction::Pause,
                ControlAction::Skills,
                ControlAction::Map,
                ControlAction::StartWave,
                ControlAction::Restart,
            };
        const std::size_t rowsPerColumn =
            (Actions.size() + 2U) / 3U;
        const float controlColumnWidth =
            (contentWidth - Gap * 2.0F) / 3.0F;
        const float rowHeight = compact ? 34.0F : 48.0F;
        const float keyHeight = compact ? 32.0F : 48.0F;
        ui_.drawInsetPanel(
            {contentX, y, contentWidth,
             rowHeight * static_cast<float>(rowsPerColumn) +
                 (compact ? 8.0F : 12.0F)}, 235);
        // Keep square-ish keycaps for single keys, but give named keys
        // enough width for readable labels (SPACE, L-SHIFT, BACKSPACE).
        const float keyWidth = compact ? 52.0F : 72.0F;
        for (std::size_t index = 0; index < Actions.size(); ++index) {
            const int column = static_cast<int>(index / rowsPerColumn);
            const std::size_t row = index % rowsPerColumn;
            const float x = contentX +
                static_cast<float>(column) *
                    (controlColumnWidth + Gap);
            const float rowY = y + 4.0F +
                static_cast<float>(row) * rowHeight;
            drawUiText(
                controlActionName(Actions[index]),
                {x + 12.0F, rowY + (compact ? 5.0F : 10.0F)},
                compact ? 10.0F : 14.0F,
                {245, 220, 174, 255});
            const Rectangle keyBounds{
                x + controlColumnWidth - keyWidth, rowY,
                keyWidth, keyHeight};
            const bool waiting = pendingControlRebind_ &&
                *pendingControlRebind_ == Actions[index];
            std::string keyLabel;
            if (waiting) {
                keyLabel = "?";
            } else {
                keyLabel = keyboardKeyName(
                    controlKey(controls, Actions[index]));
                if (Actions[index] == ControlAction::Attack &&
                    controlKey(controls, Actions[index]) == KEY_NULL) {
                    keyLabel = "LMB";
                } else if (
                    Actions[index] == ControlAction::Dash &&
                    controlKey(controls, Actions[index]) == KEY_NULL) {
                    keyLabel = "RMB";
                }
            }
            if (!waiting && ui_.drawKeyCap(keyBounds, keyLabel)) {
                pendingControlRebind_ = Actions[index];
            } else if (waiting) {
                ui_.drawKeyCap(keyBounds, keyLabel, true);
            }
        }
        y += rowHeight * static_cast<float>(rowsPerColumn) +
             (compact ? 14.0F : 20.0F);
        if (ui_.drawButton(
                {contentX, y, contentWidth, ButtonHeight},
                "RESET CONTROLS")) {
            controls = {};
            pendingControlRebind_.reset();
        }
    } else if (graphicsPanelTab_ == AccessibilityPage) {
        auto& accessibility = userSettings_.accessibility;
        float y = panelY + ControlStartY;
        if (ui_.drawToggleButton(
                {contentX, y, contentWidth, ButtonHeight},
                accessibility.reduceFlashes
                    ? "REDUCE FLASHES: ON"
                    : "REDUCE FLASHES: OFF",
                accessibility.reduceFlashes)) {
            accessibility.reduceFlashes =
                !accessibility.reduceFlashes;
        }
        y += ButtonHeight + Gap;
        const std::string languageLabel =
            std::string("LANGUAGE: ") +
            std::string(languageName(userSettings_.language));
        if (ui_.drawButton(
                {contentX, y, contentWidth, ButtonHeight},
                languageLabel)) {
            userSettings_.language =
                userSettings_.language == Language::English
                    ? Language::Russian
                    : Language::English;
            setLanguage(userSettings_.language);
        }
        y += ButtonHeight + Gap;
        if (ui_.drawToggleButton(
                {contentX, y, contentWidth, ButtonHeight},
                settings.inkOutlines
                    ? "HIGH VISIBILITY OUTLINES: ON"
                    : "HIGH VISIBILITY OUTLINES: OFF",
                settings.inkOutlines)) {
            settings.inkOutlines = !settings.inkOutlines;
            settings.postProcessing = true;
        }
        y += ButtonHeight + (compact ? 18.0F : 28.0F);
        drawUiText(
            "MOTION PROFILE",
            {contentX, y}, 18.0F,
            {255, 235, 174, 255});
        y += compact ? 28.0F : 38.0F;
        const float profileWidth =
            (contentWidth - Gap * 2.0F) / 3.0F;
        if (ui_.drawButton(
                {contentX, y, profileWidth, ButtonHeight},
                "FULL")) {
            motionBobIntensity_ = 1.0F;
            motionShakeIntensity_ = 1.0F;
            motionLandingIntensity_ = 1.0F;
            motionSwayIntensity_ = 1.0F;
        }
        if (ui_.drawButton(
                {contentX + profileWidth + Gap, y,
                 profileWidth, ButtonHeight},
                "REDUCED")) {
            motionBobIntensity_ = 0.25F;
            motionShakeIntensity_ = 0.35F;
            motionLandingIntensity_ = 0.35F;
            motionSwayIntensity_ = 0.25F;
        }
        if (ui_.drawButton(
                {contentX + (profileWidth + Gap) * 2.0F, y,
                 profileWidth, ButtonHeight},
                "OFF")) {
            motionBobIntensity_ = 0.0F;
            motionShakeIntensity_ = 0.0F;
            motionLandingIntensity_ = 0.0F;
            motionSwayIntensity_ = 0.0F;
        }
        y += ButtonHeight + (compact ? 18.0F : 28.0F);
        ui_.drawInsetPanel(
            {contentX, y, contentWidth,
             compact ? 94.0F : 124.0F}, 235);
        drawUiText(
            "Fine motion controls remain available in the Motion tab.\n"
            "Outline controls remain available in Style Lab.",
            {contentX + 20.0F, y + 20.0F},
            compact ? 14.0F : 17.0F,
            {199, 174, 142, 255});
    } else if (graphicsPanelTab_ == ToolPage) {
        const auto toolSlider =
            [this, contentX, columnWidth, panelY, Gap,
             ControlStartY, RowHeight, compact](
                int column, int row, const char* label,
                float& value, float minimum, float maximum) {
                const float x =
                    contentX + static_cast<float>(column) *
                                   (columnWidth + Gap);
                const float y =
                    panelY + ControlStartY +
                    static_cast<float>(row) * RowHeight;
                drawUiText(
                    TextFormat("%s  %.2f", label, value),
                    {x, y}, 15.0F, {245, 220, 174, 255});
                ui_.drawInsetPanel(
                    {x, y + (compact ? 20.0F : 29.0F),
                     columnWidth, compact ? 27.0F : 36.0F},
                    235);
                value = ui_.drawSliderBar(
                    {x + 8.0F,
                     y + (compact ? 23.0F : 35.0F),
                     columnWidth - 16.0F,
                     compact ? 20.0F : 24.0F},
                    value, minimum, maximum);
            };

        if (toolPanelPage_ == 0) {
            toolSlider(0, 0, "POSITION X", activeToolTuning().position.x,
                       -0.8F, 0.8F);
            toolSlider(0, 1, "POSITION Y", activeToolTuning().position.y,
                       -1.0F, 0.3F);
            toolSlider(0, 2, "POSITION Z", activeToolTuning().position.z,
                       -2.0F, -0.25F);
            toolSlider(0, 3, "SCALE", activeToolTuning().scale,
                       0.2F, 2.0F);
            toolSlider(1, 0, "ROTATION X", activeToolTuning().rotation.x,
                       -180.0F, 180.0F);
            toolSlider(1, 1, "ROTATION Y", activeToolTuning().rotation.y,
                       -180.0F, 180.0F);
            toolSlider(1, 2, "ROTATION Z", activeToolTuning().rotation.z,
                       -180.0F, 180.0F);
        } else if (toolPanelPage_ == 1) {
            toolSlider(0, 0, "WINDUP ANGLE",
                       activeToolTuning().windupDegrees,
                       -140.0F, 140.0F);
            toolSlider(0, 1, "STRIKE ANGLE",
                       activeToolTuning().strikeDegrees,
                       -160.0F, 160.0F);
            toolSlider(0, 2, "DEPTH PUSH",
                       activeToolTuning().depthPush,
                       -0.25F, 0.25F);
            toolSlider(0, 3, "HIT POINT",
                       activeToolTuning().hitProgress,
                       0.25F, 0.65F);
            toolSlider(1, 0, "SWING DURATION",
                       activeToolTuning().swingDuration,
                       0.15F, 1.5F);
            toolSlider(1, 1, "WALK BOB",
                       activeToolTuning().movementBob,
                       0.0F, 2.0F);
            toolSlider(1, 2, "SWAP DURATION",
                       activeToolTuning().swapDuration,
                       0.1F, 1.2F);
            toolSlider(1, 3, "SWAP DROP",
                       activeToolTuning().swapDrop,
                       0.2F, 1.5F);
        } else {
            toolSlider(0, 0, "OUTLINE WIDTH",
                       activeToolTuning().outlineWidth,
                       0.5F, 5.0F);
            toolSlider(0, 1, "OUTLINE STRENGTH",
                       activeToolTuning().outlineStrength,
                       0.0F, 1.0F);
            toolSlider(0, 2, "RIM LIGHT",
                       activeToolTuning().rimStrength,
                       0.0F, 1.5F);
            toolSlider(1, 0, "BRIGHTNESS",
                       activeToolTuning().brightness,
                       0.5F, 2.0F);
            toolSlider(1, 1, "SATURATION",
                       activeToolTuning().saturation,
                       0.0F, 2.0F);
            const float toggleY =
                panelY + ControlStartY + RowHeight * 2.0F;
            if (ui_.drawToggleButton(
                    {contentX + columnWidth + Gap, toggleY,
                     columnWidth, ButtonHeight},
                    activeToolTuning().outlineEnabled
                        ? "OUTLINE: ON"
                        : "OUTLINE: OFF",
                    activeToolTuning().outlineEnabled)) {
                activeToolTuning().outlineEnabled =
                    !activeToolTuning().outlineEnabled;
            }
        }

        const float modelY =
            panelY + ControlStartY + RowHeight * 4.0F;
        if (ui_.drawButton(
                {contentX, modelY,
                 columnWidth, ButtonHeight},
                toolPanelPage_ == 0
                    ? "PAGE: POSE"
                    : toolPanelPage_ == 1
                          ? "PAGE: ANIMATION"
                          : "PAGE: STYLE")) {
            toolPanelPage_ = (toolPanelPage_ + 1) % 3;
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap, modelY,
                 columnWidth, ButtonHeight},
                TextFormat("MODEL: %s",
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::Axe ? "AXE" :
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::Pickaxe ? "PICKAXE" :
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::Club ? "CLUB" :
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::IceWand ? "ICE WAND" :
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::FireWand ? "FIRE WAND" :
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::Hammer ? "HAMMER" :
                    "BOMB"))) {
            int visual = static_cast<int>(toolPanelPreviewVisual_) + 1;
            if (visual > static_cast<int>(FirstPersonToolVisual::Bomb)) {
                visual = static_cast<int>(FirstPersonToolVisual::Axe);
            }
            toolPanelPreviewVisual_ =
                static_cast<FirstPersonToolVisual>(visual);
        }
        const float actionY =
            panelY + ControlStartY + RowHeight * 5.0F;
        if (ui_.drawButton(
                {contentX, actionY,
                 columnWidth, ButtonHeight},
                "SAVE TOOL")) {
            static_cast<void>(saveFirstPersonToolTuning(
                toolTuningPath(toolPanelPreviewVisual_),
                activeToolTuning()));
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap, actionY,
                 columnWidth, ButtonHeight},
                "PLAY SWING")) {
            toolSwingUsesAxe_ =
                toolPanelPreviewVisual_ == FirstPersonToolVisual::Axe ||
                toolPanelPreviewVisual_ == FirstPersonToolVisual::Club;
            toolSwingDuration_ = activeToolTuning().swingDuration;
            toolSwingRemaining_ = toolSwingDuration_;
        }
        const float bottomY =
            actionY + ButtonHeight + Gap;
        if (ui_.drawButton(
                {contentX, bottomY, columnWidth, ButtonHeight},
                "RESET TOOL")) {
            activeToolTuning() = {};
            static_cast<void>(saveFirstPersonToolTuning(
                toolTuningPath(toolPanelPreviewVisual_),
                activeToolTuning()));
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap, bottomY,
                 columnWidth, ButtonHeight},
                "CLOSE [F2]")) {
            renderer_->setGraphicsPanelVisible(false);
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            static_cast<void>(saveFirstPersonToolTuning(
                toolTuningPath(toolPanelPreviewVisual_),
                activeToolTuning()));
        }
    }

    drawUiText(
        TextFormat("Window: %d x %d   Render: %d x %d",
                   GetScreenWidth(), GetScreenHeight(),
                   GetRenderWidth(), GetRenderHeight()),
        {contentX, panelY + PanelHeight - 48.0F}, 16.0F,
        {245, 220, 174, 255});

}

} // namespace ian
