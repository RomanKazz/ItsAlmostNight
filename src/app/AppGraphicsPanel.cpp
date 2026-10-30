#include "ui/UiViewport.hpp"
#include "app/App.hpp"
#include "localization/Localization.hpp"
#include "ui/UiText.hpp"
#include "ui/UiSettingsControls.hpp"

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

struct ResolutionOption {
    int width;
    int height;
};

constexpr std::array ResolutionOptions{
    ResolutionOption{1280, 720},
    ResolutionOption{1366, 768},
    ResolutionOption{1440, 810},
    ResolutionOption{1600, 900},
    ResolutionOption{1920, 1080},
    ResolutionOption{2560, 1440},
    ResolutionOption{3840, 2160},
};

void cycleRenderResolution(GraphicsSettings& settings) {
    int current = -1;
    for (std::size_t index = 0; index < ResolutionOptions.size(); ++index) {
        if (ResolutionOptions[index].width == settings.renderWidth &&
            ResolutionOptions[index].height == settings.renderHeight) {
            current = static_cast<int>(index);
            break;
        }
    }
    const std::size_t next = static_cast<std::size_t>(
        (current + 1) % static_cast<int>(ResolutionOptions.size()));
    settings.renderWidth = ResolutionOptions[next].width;
    settings.renderHeight = ResolutionOptions[next].height;
}

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
    if (!debugSettings_) {
        drawPlayerSettingsPanel();
        return;
    }
    const ScopedUiCanvas canvas;
    auto& settings = renderer_->settings();
    const bool liveColorPreview = graphicsPanelTab_ == ColorPage;
    const SettingsMenuUi menuUi;
    constexpr float PanelWidth = 900.0F;
    const bool compact = uiScreenHeight() < 1000;
    float requestedPanelHeight = 920.0F;
    switch (graphicsPanelTab_) {
    case PresetPage:
        // Reserve space for the menu-background button above the footer.
        requestedPanelHeight = compact ? 570.0F : 700.0F;
        break;
    case AudioPage:
        requestedPanelHeight = compact ? 670.0F : 770.0F;
        break;
    case AccessibilityPage:
        requestedPanelHeight = compact ? 740.0F : 950.0F;
        break;
    case ControlsPage:
        requestedPanelHeight = compact ? 760.0F : 980.0F;
        break;
    default:
        break;
    }
    const float PanelHeight = std::min(
        compact ? std::min(requestedPanelHeight,
                           static_cast<float>(uiScreenHeight()) - 32.0F)
                : requestedPanelHeight,
        static_cast<float>(uiScreenHeight()) - 56.0F);
    const float Margin = compact ? 16.0F : 28.0F;
    const float Gap = compact ? 8.0F : 14.0F;
    const float ControlStartY = compact ? 220.0F : 286.0F;
    // Keep all eleven Display rows inside the panel at either UI size.
    const float ButtonHeight = graphicsPanelTab_ == DisplayPage
        ? std::min(compact ? 38.0F : 58.0F,
                   (PanelHeight - ControlStartY - Margin - 52.0F - Gap * 10.0F) /
                       11.0F)
        : compact ? 38.0F : 58.0F;
    const float RowHeight = compact ? 44.0F : 62.0F;
    const float centeredPanelX =
        (static_cast<float>(uiScreenWidth()) - PanelWidth) * 0.5F;
    const float panelX = std::max(
        Margin,
        liveColorPreview
            ? static_cast<float>(uiScreenWidth()) - PanelWidth - Margin
            : centeredPanelX);
    const float panelY = std::max(
        compact ? 16.0F : 28.0F,
        (static_cast<float>(uiScreenHeight()) - PanelHeight) * 0.5F);
    const float contentX = panelX + 28.0F;
    const float contentWidth = PanelWidth - 56.0F;
    const float columnWidth = (contentWidth - Gap) * 0.5F;

    menuUi.drawPanel({panelX, panelY, PanelWidth, PanelHeight});
    const float headerY = compact ? 40.0F : 56.0F;
    const float headerHeight = compact ? 56.0F : 76.0F;
    menuStyle::panel({contentX, panelY + headerY, contentWidth, headerHeight}, 1, menuStyle::Card);
    menuStyle::text("DEBUG SETTINGS",
        {contentX + 16.0F, panelY + headerY + 8.0F,
         contentWidth - 32.0F, headerHeight - 16.0F},
        compact ? 24 : 32, menuStyle::Cream, UiFontRole::Display);

    const ScopedUiFontRole bodyText(UiFontRole::Body);

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
        if (menuUi.drawToggleButton(
                bounds, TabNames[static_cast<std::size_t>(page)],
                graphicsPanelTab_ == page)) {
            graphicsPanelTab_ = page;
        }
    }
    if (menuUi.drawButton(
            {contentX + (tabWidth + Gap) * 4.0F,
             panelY + TabY + TabHeight + Gap,
             tabWidth, TabHeight},
            "CLOSE")) {
        closeSettings();
    }

    const auto toggleButton =
        [&menuUi, contentX, columnWidth, Gap, ButtonHeight](
            int column, float y, const char* name, bool& value) {
            const Rectangle bounds{
                contentX +
                    static_cast<float>(column) * (columnWidth + Gap),
                y, columnWidth, ButtonHeight};
            const std::string label =
                std::string(name) + (value ? ": ON" : ": OFF");
            if (menuUi.drawToggleButton(bounds, label, value)) {
                value = !value;
            }
        };

    if (graphicsPanelTab_ == PresetPage) {
        const auto currentPreset = detectGraphicsPreset(settings);
        const float y = panelY + ControlStartY;
        menuUi.drawInsetPanel(
            {contentX, y, contentWidth,
             compact ? 76.0F : 104.0F}, 245);
        drawSettingsText(
            std::string("GRAPHICS PRESET: ") +
                presetName(currentPreset),
            {contentX + 22.0F, y + 16.0F},
            compact ? 20.0F : 25.0F,
            menuStyle::Cream);
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
            "LOW",
            "MEDIUM",
            "HIGH"};
        for (std::size_t index = 0; index < Presets.size(); ++index) {
            if (menuUi.drawToggleButton(
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
        if (simulation_.snapshot().state == RunState::MainMenu &&
            menuUi.drawButton({contentX, noteY + 38.0F,
                contentWidth, compact ? 38.0F : 42.0F}, "CHOOSE BACKGROUND")) {
            closeSettings();
            beginMenuBackgroundEdit(simulation_.snapshot());
            audio_.playUiConfirm();
        }
    } else if (graphicsPanelTab_ == DisplayPage) {
    float y = panelY + ControlStartY;
    if (menuUi.drawToggleButton(
            {contentX, y, columnWidth, ButtonHeight},
            settings.fullscreen
                ? "FULLSCREEN: ON"
                : "FULLSCREEN: OFF",
            settings.fullscreen)) {
        settings.fullscreen = !settings.fullscreen;
    }
    if (menuUi.drawButton(
            {contentX + columnWidth + Gap, y,
             columnWidth, ButtonHeight},
            TextFormat("RENDER: %d X %d",
                       settings.renderWidth,
                       settings.renderHeight))) {
        cycleRenderResolution(settings);
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
    toggleButton(0, y, "ENEMY LOD", settings.enemyLod);
    y += ButtonHeight + Gap;
    if (menuUi.drawButton(
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
    if (menuUi.drawButton(
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
    if (menuUi.drawButton(
            {contentX, y, smallButtonWidth, ButtonHeight}, "-")) {
        renderer_->adjustPixelSize(-1);
    }
    menuUi.drawInsetPanel(
        {contentX + smallButtonWidth + Gap, y,
         columnWidth - (smallButtonWidth + Gap) * 2.0F,
         ButtonHeight}, 245);
    menuUi.drawLabel(
        {contentX + smallButtonWidth + Gap, y,
         columnWidth - (smallButtonWidth + Gap) * 2.0F,
         ButtonHeight},
        std::string("PIXEL: ") + std::to_string(settings.pixelSize) +
            "X",
        1);
    if (menuUi.drawButton(
            {contentX + columnWidth - smallButtonWidth, y,
             smallButtonWidth, ButtonHeight},
            "+")) {
        renderer_->adjustPixelSize(1);
    }
    if (menuUi.drawButton(
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
    if (menuUi.drawButton(
            {contentX, y, columnWidth, ButtonHeight},
            frameRateLabel)) {
        renderer_->cycleFrameRateLimit();
    }
    toggleButton(1, y, "PERFORMANCE LOGS",
                 settings.performanceLogging);
    y += ButtonHeight + Gap;

    if (menuUi.drawButton(
            {contentX, y, columnWidth, ButtonHeight},
            "RESET DISPLAY")) {
        resetDisplaySettings(settings);
        renderer_->applyFrameRateLimit();
    }
    if (menuUi.drawButton(
            {contentX + columnWidth + Gap, y, columnWidth,
             ButtonHeight},
            "CLOSE")) {
        closeSettings();
    }
    } else if (graphicsPanelTab_ == ColorPage) {
        const auto colorSlider =
            [&menuUi, contentX, columnWidth, panelY, Gap,
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
                drawSettingsText(
                    TextFormat(
                        "%s  %.2f", label, value),
                    {x, y}, 15.0F,
                    menuStyle::Cream);
                menuUi.drawInsetPanel(
                    {x, y + (compact ? 20.0F : 29.0F),
                     columnWidth, compact ? 27.0F : 36.0F},
                    235);
                value = menuUi.drawSliderBar(
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
        if (menuUi.drawButton(
                {contentX, panelY + ActionY,
                 columnWidth, ButtonHeight},
                "RESET COLOR")) {
            resetColorSettings(settings);
        }
        if (menuUi.drawButton(
                {contentX + columnWidth + Gap,
                 panelY + ActionY,
                 columnWidth, ButtonHeight},
                "CLOSE")) {
            closeSettings();
        }
        settings.postProcessing = true;
    } else if (graphicsPanelTab_ == StylePage) {
        const auto styleSlider =
            [&menuUi, contentX, columnWidth, panelY, Gap,
             ControlStartY, RowHeight, compact](
                int row, const char* label, float& value,
                float minimum, float maximum) {
                const float x =
                    contentX + columnWidth + Gap;
                const float y =
                    panelY + ControlStartY +
                    static_cast<float>(row) * RowHeight;
                drawSettingsText(
                    TextFormat("%s  %.2f", label, value),
                    {x, y}, 15.0F,
                    menuStyle::Cream);
                menuUi.drawInsetPanel(
                    {x, y + (compact ? 20.0F : 29.0F),
                     columnWidth, compact ? 27.0F : 36.0F},
                    235);
                value = menuUi.drawSliderBar(
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
        toggleButton(0, y, "OBJECT OUTLINES",
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
        if (menuUi.drawButton(
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
            settings.outlineStrength = 0.68F;
            settings.outlineWidth = 1.5F;
            settings.fogBandCount = 6.0F;
            settings.paperGrainStrength = 0.025F;
        }
        if (menuUi.drawButton(
                {contentX + columnWidth + Gap,
                 panelY + ActionY,
                 columnWidth, ButtonHeight},
                "RESET STYLE")) {
            resetStyleSettings(settings);
        }
        settings.postProcessing = true;
    } else if (graphicsPanelTab_ == MotionPage) {
        const auto motionSlider =
            [&menuUi, contentX, contentWidth, panelY,
             ControlStartY, RowHeight, compact](
                int row, const char* label, float& value) {
                const float y =
                    panelY + ControlStartY +
                    static_cast<float>(row) * RowHeight;
                drawSettingsText(
                    TextFormat("%s  %d%%", label,
                               static_cast<int>(
                                   std::lround(value * 100.0F))),
                    {contentX, y}, 15.0F,
                    menuStyle::Cream);
                menuUi.drawInsetPanel(
                    {contentX,
                     y + (compact ? 20.0F : 29.0F),
                     contentWidth,
                     compact ? 27.0F : 36.0F},
                    235);
                value = menuUi.drawSliderBar(
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
        if (menuUi.drawButton(
                {contentX, actionY, columnWidth, ButtonHeight},
                "RESET MOTION")) {
            const MotionSettings defaults;
            motionBobIntensity_ = defaults.bobIntensity;
            motionShakeIntensity_ = defaults.shakeIntensity;
            motionLandingIntensity_ = defaults.landingIntensity;
            motionSwayIntensity_ = defaults.swayIntensity;
        }
        if (menuUi.drawButton(
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
        if (menuUi.drawToggleButton(
                {contentX, y, contentWidth, ButtonHeight},
                audioSettings.muted ? "ALL SOUND: MUTED"
                                    : "ALL SOUND: ON",
                !audioSettings.muted)) {
            audioSettings.muted = !audioSettings.muted;
        }
        y += ButtonHeight + (compact ? 14.0F : 22.0F);
        const auto volumeSlider =
            [&menuUi, contentX, contentWidth, compact](
                float yPosition, const char* label,
                float& value) {
                drawSettingsText(
                    TextFormat(
                        "%s  %d%%", label,
                        static_cast<int>(
                            std::lround(value * 100.0F))),
                    {contentX, yPosition}, 16.0F,
                    menuStyle::Cream);
                menuUi.drawInsetPanel(
                    {contentX, yPosition + 25.0F,
                     contentWidth, compact ? 30.0F : 38.0F},
                    235);
                value = menuUi.drawSliderBar(
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
        if (menuUi.drawButton(
                {contentX, y, contentWidth, ButtonHeight},
                "RESET AUDIO")) {
            audioSettings = {};
        }
        audio_.applySettings();
    } else if (graphicsPanelTab_ == ControlsPage) {
        auto& controls = userSettings_.controls;
        float y = panelY + ControlStartY;
        drawSettingsText(
            TextFormat(
                "MOUSE SENSITIVITY  %d%%",
                static_cast<int>(
                    std::lround(controls.mouseSensitivity * 100.0F))),
            {contentX, y}, 17.0F,
            menuStyle::Cream);
        menuUi.drawInsetPanel(
            {contentX, y + 28.0F, contentWidth,
             compact ? 32.0F : 40.0F}, 235);
        controls.mouseSensitivity = menuUi.drawSliderBar(
            {contentX + 10.0F, y + 34.0F,
             contentWidth - 20.0F,
             compact ? 20.0F : 26.0F},
            controls.mouseSensitivity, 0.1F, 3.0F);
        y += compact ? 78.0F : 98.0F;
        if (menuUi.drawToggleButton(
                {contentX, y, contentWidth, ButtonHeight},
                controls.invertMouseY
                    ? "INVERT MOUSE Y: ON"
                    : "INVERT MOUSE Y: OFF",
                controls.invertMouseY)) {
            controls.invertMouseY = !controls.invertMouseY;
        }
        y += ButtonHeight + Gap;
        constexpr std::array<
            ControlAction, ControlActionCount - 3U>
            Actions{
                ControlAction::MoveForward,
                ControlAction::MoveLeft,
                ControlAction::MoveBackward,
                ControlAction::MoveRight,
                ControlAction::Jump,
                ControlAction::Sprint,
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
                ControlAction::Map,
                ControlAction::StartWave,
                ControlAction::ToggleHud,
            };
        const std::size_t rowsPerColumn =
            (Actions.size() + 2U) / 3U;
        const float controlColumnWidth =
            (contentWidth - Gap * 2.0F) / 3.0F;
        // Leave room for Reset and the footer even when the panel is capped
        // by a short viewport. Labels and keycaps share the resulting rows.
        const float rowHeight = std::min(compact ? 34.0F : 48.0F,
            (panelY + PanelHeight - 76.0F - y -
             (compact ? 14.0F : 20.0F) - ButtonHeight) /
                static_cast<float>(rowsPerColumn));
        const float keyHeight = std::min(compact ? 32.0F : 48.0F, rowHeight);
        menuUi.drawInsetPanel(
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
            menuStyle::text(
                controlActionName(Actions[index]),
                {x + 12.0F, rowY,
                 controlColumnWidth - keyWidth - 24.0F, keyHeight},
                compact ? 10.0F : 14.0F,
                menuStyle::Cream, UiFontRole::MenuBodyBold);
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
                }
            }
            if (!waiting && menuUi.drawKeyCap(keyBounds, keyLabel)) {
                pendingControlRebind_ = Actions[index];
            } else if (waiting) {
                menuUi.drawKeyCap(keyBounds, keyLabel, true);
            }
        }
        y += rowHeight * static_cast<float>(rowsPerColumn) +
             (compact ? 14.0F : 20.0F);
        if (menuUi.drawButton(
                {contentX, y, contentWidth, ButtonHeight},
                "RESET CONTROLS")) {
            controls = {};
            pendingControlRebind_.reset();
        }
    } else if (graphicsPanelTab_ == AccessibilityPage) {
        auto& accessibility = userSettings_.accessibility;
        float y = panelY + ControlStartY;
        toggleButton(
            0, y, "FPS COUNTER", accessibility.showFps);
        toggleButton(
            1, y, "REDUCE FLASHES", accessibility.reduceFlashes);
        y += ButtonHeight + Gap;
        const std::string languageLabel =
            std::string("LANGUAGE: ") +
            std::string(languageName(userSettings_.language));
        if (menuUi.drawButton(
                {contentX, y, contentWidth, ButtonHeight},
                languageLabel)) {
            userSettings_.language =
                userSettings_.language == Language::English
                    ? Language::Russian
                    : Language::English;
            setLanguage(userSettings_.language);
        }
        y += ButtonHeight + Gap;
        if (menuUi.drawToggleButton(
                {contentX, y, contentWidth, ButtonHeight},
                settings.inkOutlines
                    ? "HIGH VISIBILITY OUTLINES: ON"
                    : "HIGH VISIBILITY OUTLINES: OFF",
                settings.inkOutlines)) {
            settings.inkOutlines = !settings.inkOutlines;
            settings.postProcessing = true;
        }
        y += ButtonHeight + (compact ? 18.0F : 28.0F);
        drawSettingsText(
            "MOTION PROFILE",
            {contentX, y}, 18.0F,
            menuStyle::Cream);
        y += compact ? 28.0F : 38.0F;
        const float profileWidth =
            (contentWidth - Gap * 2.0F) / 3.0F;
        if (menuUi.drawButton(
                {contentX, y, profileWidth, ButtonHeight},
                "FULL")) {
            motionBobIntensity_ = 1.0F;
            motionShakeIntensity_ = 1.0F;
            motionLandingIntensity_ = 1.0F;
            motionSwayIntensity_ = 1.0F;
        }
        if (menuUi.drawButton(
                {contentX + profileWidth + Gap, y,
                 profileWidth, ButtonHeight},
                "REDUCED")) {
            motionBobIntensity_ = 0.25F;
            motionShakeIntensity_ = 0.35F;
            motionLandingIntensity_ = 0.35F;
            motionSwayIntensity_ = 0.25F;
        }
        if (menuUi.drawButton(
                {contentX + (profileWidth + Gap) * 2.0F, y,
                 profileWidth, ButtonHeight},
                "OFF")) {
            motionBobIntensity_ = 0.0F;
            motionShakeIntensity_ = 0.0F;
            motionLandingIntensity_ = 0.0F;
            motionSwayIntensity_ = 0.0F;
        }
        y += ButtonHeight + (compact ? 18.0F : 28.0F);
    } else if (graphicsPanelTab_ == ToolPage) {
        const auto toolSlider =
            [&menuUi, contentX, columnWidth, panelY, Gap,
             ControlStartY, RowHeight, compact](
                int column, int row, const char* label,
                float& value, float minimum, float maximum) {
                const float x =
                    contentX + static_cast<float>(column) *
                                   (columnWidth + Gap);
                const float y =
                    panelY + ControlStartY +
                    static_cast<float>(row) * RowHeight;
                drawSettingsText(
                    TextFormat("%s  %.2f", label, value),
                    {x, y}, 15.0F, menuStyle::Cream);
                menuUi.drawInsetPanel(
                    {x, y + (compact ? 20.0F : 29.0F),
                     columnWidth, compact ? 27.0F : 36.0F},
                    235);
                value = menuUi.drawSliderBar(
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
            if (menuUi.drawToggleButton(
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
        if (menuUi.drawButton(
                {contentX, modelY,
                 columnWidth, ButtonHeight},
                toolPanelPage_ == 0
                    ? "PAGE: POSE"
                    : toolPanelPage_ == 1
                          ? "PAGE: ANIMATION"
                          : "PAGE: STYLE")) {
            toolPanelPage_ = (toolPanelPage_ + 1) % 3;
        }
        if (menuUi.drawButton(
                {contentX + columnWidth + Gap, modelY,
                 columnWidth, ButtonHeight},
                TextFormat("MODEL: %s",
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::Axe ? "AXE" :
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::Pickaxe ? "PICKAXE" :
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::Club ? "CLUB" :
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::IceWand ? "ICE WAND" :
                    toolPanelPreviewVisual_ == FirstPersonToolVisual::FireWand ? "FIRE WAND" :
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
        if (menuUi.drawButton(
                {contentX, actionY,
                 columnWidth, ButtonHeight},
                "PLAY SWING")) {
            toolSwingUsesAxe_ =
                toolPanelPreviewVisual_ == FirstPersonToolVisual::Axe ||
                toolPanelPreviewVisual_ == FirstPersonToolVisual::Club;
            toolSwingDuration_ = activeToolTuning().swingDuration;
            toolSwingRemaining_ = toolSwingDuration_;
            chooseHarvestSwingAnimation(toolPanelPreviewVisual_);
        }
        if (menuUi.drawButton(
                {contentX + columnWidth + Gap, actionY,
                 columnWidth, ButtonHeight},
                "RESET TOOL")) {
            activeToolTuning() = defaultFirstPersonToolTuning(
                toolPanelPreviewVisual_);
        }
        const float bottomY =
            actionY + ButtonHeight + Gap;
        if (menuUi.drawButton(
                {contentX, bottomY, contentWidth, ButtonHeight},
                "CLOSE")) {
            closeSettings();
        }
    }

    drawSettingsText(
        TextFormat("Window: %d x %d   Render: %d x %d",
                   GetScreenWidth(), GetScreenHeight(),
                   GetRenderWidth(), GetRenderHeight()),
        {contentX, panelY + PanelHeight - 48.0F}, 16.0F,
        menuStyle::Cream);

}

} // namespace ian
