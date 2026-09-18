#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

struct PixelBounds {
    int left{};
    int top{};
    int right{};
    int bottom{};
    int visiblePixels{};
    int fillPixels{};
    int outlinePixels{};
    int shadowPixels{};
};

PixelBounds inspectTextRegion(Image image) {
    PixelBounds bounds{
        .left = image.width,
        .top = image.height,
    };
    Color* pixels = LoadImageColors(image);
    if (pixels == nullptr) {
        return bounds;
    }
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const Color color = pixels[y * image.width + x];
            if (color.a <= 12U) {
                continue;
            }
            bounds.left = std::min(bounds.left, x);
            bounds.top = std::min(bounds.top, y);
            bounds.right = std::max(bounds.right, x);
            bounds.bottom = std::max(bounds.bottom, y);
            ++bounds.visiblePixels;
            if (color.r >= 200U && color.g >= 200U && color.b >= 190U) {
                ++bounds.fillPixels;
            }
            if (color.r >= 35U && color.r <= 65U &&
                color.g >= 35U && color.g <= 65U &&
                color.b >= 35U && color.b <= 65U) {
                ++bounds.outlinePixels;
            }
            if (color.r >= 70U && color.r <= 100U &&
                color.g >= 70U && color.g <= 100U &&
                color.b >= 70U && color.b <= 100U) {
                ++bounds.shadowPixels;
            }
        }
    }
    UnloadImageColors(pixels);
    return bounds;
}

PixelBounds renderReferenceHud(int width, int height,
                               const char* capturePath) {
    RenderTexture2D target = LoadRenderTexture(width, height);
    if (!IsRenderTextureValid(target)) {
        return {};
    }
    BeginTextureMode(target);
    ClearBackground(BLANK);
    ian::beginSdfUiTextBatch();
    ian::SdfUiTextStyle waveStyle{};
    waveStyle.fillColor = WHITE;
    waveStyle.outlineColor = {49, 49, 49, 255};
    waveStyle.shadowColor = {82, 82, 82, 255};
    waveStyle.shadowOffsetPixels = {0.0F, 4.0F};
    waveStyle.outlinePixels = 3.0F;
    ian::drawStyledSdfUiText(
        "WAVE 12", {80.0F, 70.0F}, 64.0F / 1.68F,
        waveStyle);
    ian::SdfUiTextStyle resourceStyle{};
    resourceStyle.fillColor = {244, 241, 232, 255};
    resourceStyle.outlineColor = {49, 49, 49, 255};
    resourceStyle.shadowColor = {82, 82, 82, 255};
    resourceStyle.shadowOffsetPixels = {0.0F, 4.0F};
    resourceStyle.outlinePixels = 3.0F;
    constexpr float AmountSize = 52.0F / 1.68F;
    constexpr float CapacitySize = 24.0F / 1.68F;
    const Vector2 amountPosition{80.0F, 145.0F};
    const Vector2 amountBounds = ian::measureUiText("0", AmountSize);
    const std::array resourceRuns{
        ian::SdfUiTextRun{
            "0", amountPosition, AmountSize,
            Color{244, 241, 232, 255}},
        ian::SdfUiTextRun{
            "/60",
            {amountPosition.x + amountBounds.x,
             amountPosition.y + 21.5F},
            CapacitySize, Color{221, 221, 221, 255}},
    };
    ian::drawStyledSdfUiTextRuns(resourceRuns, resourceStyle);
    ian::drawStyledSdfUiText(
        "120", {220.0F, 145.0F}, AmountSize,
        resourceStyle);
    ian::drawStyledSdfUiText(
        "∞", {380.0F, 145.0F}, AmountSize,
        resourceStyle);
    ian::drawSdfUiText(
        "LVL 27", {80.0F, 210.0F}, 14.85F,
        Color{244, 241, 232, 255});
    ian::endSdfUiTextBatch();
    EndTextureMode();

    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    const PixelBounds bounds = inspectTextRegion(image);
    if (capturePath != nullptr) {
        static_cast<void>(ExportImage(image, capturePath));
    }
    UnloadImage(image);
    UnloadRenderTexture(target);
    return bounds;
}

bool validReference(const PixelBounds& bounds) {
    return bounds.visiblePixels > 300 &&
        bounds.fillPixels > 100 &&
        bounds.outlinePixels > 100 &&
        bounds.shadowPixels > 50 &&
        bounds.left < bounds.right &&
        bounds.top < bounds.bottom;
}

bool rotatedTextUsesSdfStyle() {
    RenderTexture2D target = LoadRenderTexture(256, 256);
    if (!IsRenderTextureValid(target)) {
        return false;
    }
    BeginTextureMode(target);
    ClearBackground(BLANK);
    ian::SdfUiTextStyle style{};
    style.fillColor = ian::UiWhite;
    style.outlineColor = {49, 49, 49, 255};
    style.shadowColor = {82, 82, 82, 255};
    style.shadowOffsetPixels = {0.0F, 4.0F};
    style.outlinePixels = 3.0F;
    ian::drawRotatedStyledUiText(
        "0/20", {128.0F, 128.0F}, 52.0F / 1.68F,
        90.0F, style);
    EndTextureMode();

    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    const PixelBounds bounds = inspectTextRegion(image);
    UnloadImage(image);
    UnloadRenderTexture(target);
    return bounds.visiblePixels > 100 &&
        bounds.fillPixels > 50 &&
        bounds.outlinePixels > 50 &&
        bounds.shadowPixels > 20 &&
        bounds.bottom - bounds.top > bounds.right - bounds.left;
}

} // namespace

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "IAN SDF UI text smoke");
    if (!IsWindowReady()) {
        std::cerr << "graphics context unavailable; skipping\n";
        return 77;
    }

    ian::initializeUiText();
    const std::string captureDirectory = argc > 1 ? argv[1] : "";
    const std::string capture1080 = captureDirectory.empty()
        ? ""
        : captureDirectory + "/ian_ui_text_sdf_1080.png";
    const std::string capture1440 = captureDirectory.empty()
        ? ""
        : captureDirectory + "/ian_ui_text_sdf_1440.png";
    const PixelBounds bounds1080 = renderReferenceHud(
        1920, 1080, capture1080.empty() ? nullptr : capture1080.c_str());
    const PixelBounds bounds1440 = renderReferenceHud(
        2560, 1440, capture1440.empty() ? nullptr : capture1440.c_str());

    const int width1080 = bounds1080.right - bounds1080.left;
    const int height1080 = bounds1080.bottom - bounds1080.top;
    const int width1440 = bounds1440.right - bounds1440.left;
    const int height1440 = bounds1440.bottom - bounds1440.top;
    const bool stableBounds =
        std::abs(width1080 - width1440) <= 1 &&
        std::abs(height1080 - height1440) <= 1;
    const bool valid = validReference(bounds1080) &&
        validReference(bounds1440) && stableBounds &&
        rotatedTextUsesSdfStyle();
    if (!valid) {
        std::cerr << "SDF text validation failed: 1080="
                  << bounds1080.visiblePixels << "/"
                  << bounds1080.fillPixels << "/"
                  << bounds1080.outlinePixels << "/"
                  << bounds1080.shadowPixels << " bounds="
                  << width1080 << "x" << height1080
                  << ", 1440=" << bounds1440.visiblePixels << "/"
                  << bounds1440.fillPixels << "/"
                  << bounds1440.outlinePixels << "/"
                  << bounds1440.shadowPixels << " bounds="
                  << width1440 << "x" << height1440 << "\n";
    }

    ian::shutdownUiText();
    CloseWindow();
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
