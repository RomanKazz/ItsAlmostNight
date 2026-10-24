#include "ui/UiText.hpp"
#include "ui/UiViewport.hpp"
#include "localization/Localization.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
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
    ian::beginSdfUiTextBatch();
    ian::drawRotatedStyledUiText(
        "0/20", {128.0F, 128.0F}, 52.0F / 1.68F,
        90.0F, style);
    ian::endSdfUiTextBatch();
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


std::uint64_t renderFontSample(ian::UiFontRole role, bool queued) {
    RenderTexture2D target = LoadRenderTexture(640, 360);
    if (!IsRenderTextureValid(target)) return 0;
    // Warm the HUD cache, then change only the font on the next invocation.
    for (int frame = 0; frame < 8; ++frame) {
        BeginTextureMode(target);
        ClearBackground(BLANK);
        if (queued) ian::beginSdfUiTextBatch();
        {
            const ian::ScopedUiFontRole font(role);
            const ian::SdfUiTextStyle style{
                .fillColor = ian::UiWhite,
                .outlineColor = BLANK,
                .outlinePixels = 0.0F,
            };
            ian::drawStyledSdfUiText("Defense damage / Урон Ёж", {24, 24}, 18, style);
        }
        // The role must survive until a later batch flush.
        if (queued) ian::endSdfUiTextBatch();
        EndTextureMode();
    }
    Image image = LoadImageFromTexture(target.texture);
    const PixelBounds bounds = inspectTextRegion(image);
    Color* pixels = LoadImageColors(image);
    std::uint64_t hash = 14695981039346656037ULL;
    if (pixels != nullptr) {
        for (int i = 0; i < image.width * image.height; ++i) {
            // RGB and alpha both matter: a substituted face changes the glyphs.
            for (unsigned char channel : {pixels[i].r, pixels[i].g, pixels[i].b, pixels[i].a}) {
                hash = (hash ^ channel) * 1099511628211ULL;
            }
        }
        UnloadImageColors(pixels);
    }
    UnloadImage(image);
    UnloadRenderTexture(target);
    return bounds.visiblePixels > 100 ? hash : 0;
}

bool bodyFontsRenderAndRestore() {
    const unsigned int display = ian::uiFont().texture.id;
    unsigned int regular{};
    {
        const ian::ScopedUiFontRole body(ian::UiFontRole::Body);
        const Font font = ian::uiFont();
        regular = font.texture.id;
        for (int codepoint : {0x401, 0x410, 0x44F, 0x451}) {
            const int index = GetGlyphIndex(font, codepoint);
            if (index < 0 || font.glyphs[index].value != codepoint ||
                font.glyphs[index].advanceX <= 0) return false;
        }
        {
            const ian::ScopedUiFontRole bold(ian::UiFontRole::BodyBold);
            if (ian::uiFont().texture.id == regular ||
                ian::uiFont().texture.id == display) return false;
        }
        if (ian::uiFont().texture.id != regular) return false;
    }
    if (regular == display || ian::uiFont().texture.id != display) return false;
    SetWindowSize(640, 360);
    PollInputEvents();
    const auto regularQueued = renderFontSample(ian::UiFontRole::Body, true);
    const auto boldQueued = renderFontSample(ian::UiFontRole::BodyBold, true);
    const auto regularDirect = renderFontSample(ian::UiFontRole::Body, false);
    const auto boldDirect = renderFontSample(ian::UiFontRole::BodyBold, false);
    return regularQueued != 0 && boldQueued != 0 && regularQueued != boldQueued &&
        regularQueued == regularDirect && boldQueued == boldDirect;
}

std::uint64_t renderUnchangedHudLabel(int resourceAmount) {
    BeginDrawing();
    ClearBackground(BLANK);
    ian::beginUiTextPerformanceFrame();
    {
        const ian::ScopedUiCanvas canvas;
        ian::beginSdfUiTextBatch();
        ian::SdfUiTextStyle style{};
        style.fillColor = ian::UiWhite;
        style.outlineColor = {49, 49, 49, 255};
        style.shadowColor = {82, 82, 82, 255};
        style.shadowOffsetPixels = {0.0F, 4.0F};
        style.outlinePixels = 3.0F;
        ian::drawStyledSdfUiText("HUD LABEL", {60, 90}, 26, style);
        ian::drawStyledSdfUiText(
            std::to_string(resourceAmount), {1000, 90}, 26, style);
        for (int index = 0; index < 7; ++index) {
            ian::drawStyledSdfUiText(
                "OTHER HUD TEXT", {60, 275.0F + index * 70.0F},
                16, style);
        }
        ian::endSdfUiTextBatch();
    }
    rlDrawRenderBatchActive();
    Image image = LoadImageFromScreen();
    Color* pixels = LoadImageColors(image);
    if (pixels == nullptr) {
        UnloadImage(image);
        return 0;
    }
    const float scale = static_cast<float>(image.width) /
        static_cast<float>(GetScreenWidth());
    const int left = static_cast<int>(20.0F * scale);
    const int right = static_cast<int>(210.0F * scale);
    const int top = static_cast<int>(30.0F * scale);
    const int bottom = static_cast<int>(90.0F * scale);
    std::uint64_t hash = 14695981039346656037ULL;
    int visiblePixels = 0;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const Color color = pixels[y * image.width + x];
            if (color.r > 100U) ++visiblePixels;
            for (unsigned char channel : {color.r, color.g, color.b, color.a}) {
                hash = (hash ^ channel) * 1099511628211ULL;
            }
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    EndDrawing();
    return visiblePixels > 100 ? hash : 0;
}

bool resourceUpdateKeepsHudTextAppearance() {
    for (int frame = 0; frame < 3; ++frame) {
        renderUnchangedHudLabel(40);
    }
    const std::uint64_t before = renderUnchangedHudLabel(40);
    const std::uint64_t during = renderUnchangedHudLabel(41);
    return before != 0 && before == during;
}

} // namespace

int main(int argc, char** argv) {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_WINDOW_HIGHDPI);
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
        rotatedTextUsesSdfStyle() && bodyFontsRenderAndRestore() &&
        resourceUpdateKeepsHudTextAppearance();
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
