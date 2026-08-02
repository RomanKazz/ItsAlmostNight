#include "app/App.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {

void App::drawGraphicsPanel() {
    auto& settings = renderer_->settings();
    constexpr float PanelWidth = 900.0F;
    const bool compact = GetScreenHeight() < 900;
    const float PanelHeight = compact
                                  ? static_cast<float>(GetScreenHeight()) - 32.0F
                                  : 920.0F;
    const float Margin = compact ? 16.0F : 28.0F;
    const float Gap = compact ? 8.0F : 14.0F;
    const float ButtonHeight = compact ? 46.0F : 64.0F;
    const float ControlStartY = compact ? 172.0F : 226.0F;
    const float RowHeight = compact ? 52.0F : 76.0F;
    const float panelX = graphicsPanelTab_ == 4
                             ? Margin
                             : static_cast<float>(GetScreenWidth()) -
                                   PanelWidth - Margin;
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
        "GRAPHICS SETTINGS", 1);

    const float TabY = compact ? 108.0F : 150.0F;
    const float TabHeight = compact ? 44.0F : 58.0F;
    const float tabWidth =
        (contentWidth - Gap * 4.0F) / 5.0F;
    if (ui_.drawToggleButton(
            {contentX, panelY + TabY, tabWidth,
             TabHeight},
            "DISPLAY", graphicsPanelTab_ == 0)) {
        graphicsPanelTab_ = 0;
    }
    if (ui_.drawToggleButton(
            {contentX + tabWidth + Gap,
             panelY + TabY, tabWidth, TabHeight},
            "COLOR",
            graphicsPanelTab_ == 1)) {
        graphicsPanelTab_ = 1;
    }
    if (ui_.drawToggleButton(
            {contentX + (tabWidth + Gap) * 2.0F,
             panelY + TabY, tabWidth, TabHeight},
            "STYLE LAB", graphicsPanelTab_ == 2)) {
        graphicsPanelTab_ = 2;
    }
    if (ui_.drawToggleButton(
            {contentX + (tabWidth + Gap) * 3.0F,
             panelY + TabY, tabWidth, TabHeight},
            "MOTION", graphicsPanelTab_ == 3)) {
        graphicsPanelTab_ = 3;
    }
    if (ui_.drawToggleButton(
            {contentX + (tabWidth + Gap) * 4.0F,
             panelY + TabY, tabWidth, TabHeight},
            "TOOL", graphicsPanelTab_ == 4)) {
        graphicsPanelTab_ = 4;
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

    if (graphicsPanelTab_ == 0) {
    float y = panelY + ControlStartY;
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
    toggleButton(1, y, "SSAO (RESERVED)", settings.ssao);
    y += ButtonHeight + Gap;
    if (ui_.drawButton(
            {contentX, y, contentWidth,
             ButtonHeight},
            std::string("QUALITY: ") +
                (settings.quality == GraphicsQuality::Low
                     ? "LOW"
                     : settings.quality == GraphicsQuality::Medium
                           ? "MEDIUM"
                           : "HIGH"))) {
        renderer_->cycleQuality();
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

    if (ui_.drawToggleButton(
            {contentX, y, columnWidth, ButtonHeight},
            "SHADOW MAP PREVIEW", renderer_->shadowMapVisible())) {
        renderer_->setShadowMapVisible(
            !renderer_->shadowMapVisible());
    }
    if (ui_.drawButton(
            {contentX + columnWidth + Gap, y, columnWidth,
             ButtonHeight},
            "CLOSE [F2]")) {
        renderer_->setGraphicsPanelVisible(false);
    }
    } else if (graphicsPanelTab_ == 1) {
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
            settings.postExposure = 0.0F;
            settings.brightness = 0.0F;
            settings.contrast = 1.0F;
            settings.colorSaturation = 1.0F;
            settings.hueDegrees = 0.0F;
            settings.temperature = 0.0F;
            settings.tint = 0.0F;
            settings.gamma = 1.0F;
            settings.blackPoint = 0.0F;
            settings.curveShadows = 0.0F;
            settings.curveMidtones = 0.0F;
            settings.curveHighlights = 0.0F;
            settings.sharpness = 0.0F;
            settings.vignette = 0.0F;
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap,
                 panelY + ActionY,
                 columnWidth, ButtonHeight},
                "CLOSE [F2]")) {
            renderer_->setGraphicsPanelVisible(false);
        }
        settings.postProcessing = true;
    } else if (graphicsPanelTab_ == 2) {
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
        toggleButton(0, y, "LIGHT STEPS",
                     settings.posterizedLighting);
        styleSlider(2, "LIGHT LEVELS", settings.lightingSteps,
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
        toggleButton(0, y, "FOG BANDS", settings.fogBands);
        styleSlider(5, "FOG LEVELS", settings.fogBandCount,
                    2.0F, 12.0F);
        y += RowHeight;
        toggleButton(0, y, "PAPER GRAIN",
                     settings.paperGrain);
        styleSlider(6, "GRAIN STRENGTH",
                    settings.paperGrainStrength, 0.0F, 0.15F);

        const float ActionY =
            ControlStartY + RowHeight * 7.0F +
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
            settings.fogBandCount = 6.0F;
            settings.paperGrainStrength = 0.025F;
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap,
                 panelY + ActionY,
                 columnWidth, ButtonHeight},
                "RESET STYLE")) {
            settings.paletteQuantization = false;
            settings.dithering = false;
            settings.posterizedLighting = false;
            settings.bloom = false;
            settings.inkOutlines = false;
            settings.fogBands = false;
            settings.paperGrain = false;
            settings.paletteLevels = 8.0F;
            settings.ditherStrength = 0.35F;
            settings.lightingSteps = 5.0F;
            settings.bloomStrength = 0.28F;
            settings.outlineStrength = 0.35F;
            settings.fogBandCount = 5.0F;
            settings.paperGrainStrength = 0.035F;
        }
        settings.postProcessing = true;
    } else if (graphicsPanelTab_ == 3) {
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
            motionBobIntensity_ = 1.0F;
            motionShakeIntensity_ = 1.0F;
            motionLandingIntensity_ = 1.0F;
            motionSwayIntensity_ = 1.0F;
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
    } else {
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
            toolSlider(0, 0, "POSITION X", toolTuning_.position.x,
                       -0.8F, 0.8F);
            toolSlider(0, 1, "POSITION Y", toolTuning_.position.y,
                       -1.0F, 0.3F);
            toolSlider(0, 2, "POSITION Z", toolTuning_.position.z,
                       -2.0F, -0.25F);
            toolSlider(0, 3, "SCALE", toolTuning_.scale,
                       0.2F, 2.0F);
            toolSlider(1, 0, "ROTATION X", toolTuning_.rotation.x,
                       -180.0F, 180.0F);
            toolSlider(1, 1, "ROTATION Y", toolTuning_.rotation.y,
                       -180.0F, 180.0F);
            toolSlider(1, 2, "ROTATION Z", toolTuning_.rotation.z,
                       -180.0F, 180.0F);
        } else if (toolPanelPage_ == 1) {
            toolSlider(0, 0, "WINDUP ANGLE",
                       toolTuning_.windupDegrees,
                       -140.0F, 140.0F);
            toolSlider(0, 1, "STRIKE ANGLE",
                       toolTuning_.strikeDegrees,
                       -160.0F, 160.0F);
            toolSlider(0, 2, "DEPTH PUSH",
                       toolTuning_.depthPush,
                       -0.25F, 0.25F);
            toolSlider(1, 0, "SWING DURATION",
                       toolTuning_.swingDuration,
                       0.15F, 1.5F);
            toolSlider(1, 1, "WALK BOB",
                       toolTuning_.movementBob,
                       0.0F, 2.0F);
        } else {
            toolSlider(0, 0, "OUTLINE WIDTH",
                       toolTuning_.outlineWidth,
                       0.5F, 5.0F);
            toolSlider(0, 1, "OUTLINE STRENGTH",
                       toolTuning_.outlineStrength,
                       0.0F, 1.0F);
            toolSlider(0, 2, "RIM LIGHT",
                       toolTuning_.rimStrength,
                       0.0F, 1.5F);
            toolSlider(1, 0, "BRIGHTNESS",
                       toolTuning_.brightness,
                       0.5F, 2.0F);
            toolSlider(1, 1, "SATURATION",
                       toolTuning_.saturation,
                       0.0F, 2.0F);
            const float toggleY =
                panelY + ControlStartY + RowHeight * 2.0F;
            if (ui_.drawToggleButton(
                    {contentX + columnWidth + Gap, toggleY,
                     columnWidth, ButtonHeight},
                    toolTuning_.outlineEnabled
                        ? "OUTLINE: ON"
                        : "OUTLINE: OFF",
                    toolTuning_.outlineEnabled)) {
                toolTuning_.outlineEnabled =
                    !toolTuning_.outlineEnabled;
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
                toolPanelPreviewUsesAxe_
                    ? "MODEL: AXE"
                    : "MODEL: PICKAXE")) {
            toolPanelPreviewUsesAxe_ =
                !toolPanelPreviewUsesAxe_;
        }
        const float actionY =
            panelY + ControlStartY + RowHeight * 5.0F;
        if (ui_.drawButton(
                {contentX, actionY,
                 columnWidth, ButtonHeight},
                "SAVE TOOL")) {
            static_cast<void>(saveFirstPersonToolTuning(
                "user_settings/first_person_tool.json",
                toolTuning_));
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap, actionY,
                 columnWidth, ButtonHeight},
                "PLAY SWING")) {
            toolSwingUsesAxe_ = toolPanelPreviewUsesAxe_;
            toolSwingDuration_ = toolTuning_.swingDuration;
            toolSwingRemaining_ = toolSwingDuration_;
        }
        const float bottomY =
            actionY + ButtonHeight + Gap;
        if (ui_.drawButton(
                {contentX, bottomY, columnWidth, ButtonHeight},
                "RESET TOOL")) {
            toolTuning_ = {};
            static_cast<void>(saveFirstPersonToolTuning(
                "user_settings/first_person_tool.json",
                toolTuning_));
        }
        if (ui_.drawButton(
                {contentX + columnWidth + Gap, bottomY,
                 columnWidth, ButtonHeight},
                "CLOSE [F2]")) {
            renderer_->setGraphicsPanelVisible(false);
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            static_cast<void>(saveFirstPersonToolTuning(
                "user_settings/first_person_tool.json",
                toolTuning_));
        }
    }

    drawUiText(
        TextFormat("Window: %d x %d   Render: %d x %d",
                   GetScreenWidth(), GetScreenHeight(),
                   GetRenderWidth(), GetRenderHeight()),
        {contentX, panelY + PanelHeight - 48.0F}, 16.0F,
        {245, 220, 174, 255});

    if (graphicsPanelTab_ != 0 && graphicsPanelTab_ != 4) {
        constexpr float PreviewGap = 18.0F;
        const float previewPanelX = Margin;
        const float previewPanelWidth =
            std::max(
                260.0F,
                panelX - Margin - PreviewGap);
        const float previewWidth =
            previewPanelWidth - 36.0F;
        const float previewHeight =
            std::min(
                previewWidth * 9.0F / 16.0F,
                PanelHeight - 116.0F);
        const float previewPanelHeight =
            previewHeight + 94.0F;
        ui_.drawPanel(
            {previewPanelX, panelY,
             previewPanelWidth, previewPanelHeight},
            250);
        drawUiText(
            "LIVE PREVIEW",
            {previewPanelX + 20.0F, panelY + 18.0F},
            20.0F, {255, 235, 174, 255});
        const Rectangle previewBounds{
            previewPanelX + 18.0F,
            panelY + 58.0F,
            previewWidth, previewHeight,
        };
        renderer_->drawScenePreview(previewBounds);
        DrawRectangleLinesEx(
            previewBounds, 3.0F,
            {245, 220, 174, 220});
        return;
    }

    if (graphicsPanelTab_ == 4) {
        return;
    }

    constexpr float AudioPanelWidth = 320.0F;
    constexpr float AudioPanelHeight = 552.0F;
    constexpr float AudioGap = 18.0F;
    const float audioX = std::max(
        Margin,
        panelX - AudioPanelWidth - AudioGap);
    const float audioContentX = audioX + 24.0F;
    constexpr float AudioContentWidth =
        AudioPanelWidth - 48.0F;
    ui_.drawPanel(
        {audioX, panelY, AudioPanelWidth, AudioPanelHeight},
        250);
    ui_.drawInsetPanel(
        {audioContentX, panelY + 56.0F,
         AudioContentWidth, 76.0F},
        245);
    ui_.drawLabel(
        {audioContentX + 12.0F, panelY + 66.0F,
         AudioContentWidth - 24.0F, 52.0F},
        "AUDIO", 1);

    auto& audioSettings = audio_.settings();
    float audioY = panelY + 150.0F;
    if (ui_.drawToggleButton(
            {audioContentX, audioY, AudioContentWidth,
             ButtonHeight},
            audioSettings.muted ? "SOUND: MUTED"
                                : "SOUND: ON",
            !audioSettings.muted)) {
        audioSettings.muted = !audioSettings.muted;
    }
    audioY += ButtonHeight + 18.0F;
    const auto volumeSlider =
        [this, audioContentX](
            float yPosition, const char* label,
            float& value) {
            drawUiText(
                TextFormat(
                    "%s  %d%%", label,
                    static_cast<int>(
                        std::lround(value * 100.0F))),
                {audioContentX, yPosition}, 15.0F,
                {245, 220, 174, 255});
            ui_.drawInsetPanel(
                {audioContentX, yPosition + 31.0F,
                 AudioContentWidth, 34.0F},
                235);
            value = ui_.drawSliderBar(
                {audioContentX + 8.0F, yPosition + 36.0F,
                 AudioContentWidth - 16.0F, 24.0F},
                value, 0.0F, 1.0F);
        };
    volumeSlider(audioY, "MASTER",
                 audioSettings.masterVolume);
    audioY += 78.0F;
    volumeSlider(audioY, "SFX", audioSettings.sfxVolume);
    audioY += 78.0F;
    volumeSlider(
        audioY, "MUSIC", audioSettings.musicVolume);
    audioY += 82.0F;
    if (ui_.drawButton(
            {audioContentX, audioY, AudioContentWidth,
             54.0F},
            "RESET MIX")) {
        audioSettings = {};
    }
    audio_.applySettings();
}

} // namespace ian
