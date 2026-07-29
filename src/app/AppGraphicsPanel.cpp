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
    constexpr float PanelHeight = 920.0F;
    constexpr float Margin = 28.0F;
    constexpr float Gap = 14.0F;
    constexpr float ButtonHeight = 64.0F;
    const float panelX =
        static_cast<float>(GetScreenWidth()) - PanelWidth - Margin;
    const float panelY = 28.0F;
    const float contentX = panelX + 28.0F;
    const float contentWidth = PanelWidth - 56.0F;
    const float columnWidth = (contentWidth - Gap) * 0.5F;

    ui_.drawPanel({panelX, panelY, PanelWidth, PanelHeight}, 250);
    ui_.drawInsetPanel(
        {contentX, panelY + 56.0F, contentWidth, 76.0F}, 245);
    ui_.drawLabel(
        {contentX + 16.0F, panelY + 66.0F,
         contentWidth - 32.0F, 52.0F},
        "GRAPHICS SETTINGS", 1);

    constexpr float TabY = 150.0F;
    constexpr float TabHeight = 58.0F;
    if (ui_.drawToggleButton(
            {contentX, panelY + TabY, columnWidth,
             TabHeight},
            "DISPLAY", graphicsPanelTab_ == 0)) {
        graphicsPanelTab_ = 0;
    }
    if (ui_.drawToggleButton(
            {contentX + columnWidth + Gap,
             panelY + TabY, columnWidth, TabHeight},
            "COLOR & CURVES",
            graphicsPanelTab_ == 1)) {
        graphicsPanelTab_ = 1;
    }

    const auto toggleButton =
        [this, contentX, columnWidth](
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
    float y = panelY + 226.0F;
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
    toggleButton(0, y, "BLOOM (RESERVED)", settings.bloom);
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
    } else {
        const auto colorSlider =
            [this, contentX, columnWidth, panelY](
                int column, int row, const char* label,
                float& value, float minimum, float maximum) {
                constexpr float RowHeight = 76.0F;
                const float x =
                    contentX +
                    static_cast<float>(column) *
                        (columnWidth + Gap);
                const float y =
                    panelY + 226.0F +
                    static_cast<float>(row) * RowHeight;
                drawUiText(
                    TextFormat(
                        "%s  %.2f", label, value),
                    {x, y}, 15.0F,
                    {245, 220, 174, 255});
                ui_.drawInsetPanel(
                    {x, y + 29.0F, columnWidth, 36.0F},
                    235);
                value = ui_.drawSliderBar(
                    {x + 8.0F, y + 35.0F,
                     columnWidth - 16.0F, 24.0F},
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

        constexpr float ActionY = 770.0F;
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
    }

    drawUiText(
        TextFormat("Window: %d x %d   Render: %d x %d",
                   GetScreenWidth(), GetScreenHeight(),
                   GetRenderWidth(), GetRenderHeight()),
        {contentX, panelY + PanelHeight - 48.0F}, 16.0F,
        {245, 220, 174, 255});

    if (graphicsPanelTab_ == 1) {
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
