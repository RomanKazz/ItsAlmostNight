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
#include <filesystem>
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

namespace {
bool mixedFontFallbackKeepsCiko(const std::string& captures) {
    using namespace ian;
    initializeLocalization();
    const auto oldLanguage=currentLanguage();
    const auto render=[](std::string_view value,Language language,bool batch,bool rotated,UiFontRole role) {
        setLanguage(language);
        const ScopedUiFontRole font(role);
        const auto target=LoadRenderTexture(1000,180);
        BeginTextureMode(target);ClearBackground(BLANK);
        SdfUiTextStyle style{};style.fillColor={254,247,229,255};style.outlineColor=BLACK;
        style.outlinePixels=3;style.shadowColor={30,40,35,255};style.shadowOffsetPixels={0,3};
        if(batch)beginSdfUiTextBatch();
        if(rotated)drawRotatedStyledUiText(value,{500,90},30,8,style);
        else drawStyledSdfUiText(value,{24,45},30,style);
        if(batch)endSdfUiTextBatch();
        EndTextureMode();auto image=LoadImageFromTexture(target.texture);ImageFlipVertical(&image);
        UnloadRenderTexture(target);return image;
    };
    const auto equal=[](Image a,Image b) {
        if(a.width!=b.width || a.height!=b.height)return false;
        auto* x=LoadImageColors(a);auto* y=LoadImageColors(b);bool same=x&&y;
        if(same)for(int i=0;i<a.width*a.height;++i)
            if(x[i].r!=y[i].r || x[i].g!=y[i].g || x[i].b!=y[i].b || x[i].a!=y[i].a){same=false;break;}
        UnloadImageColors(x);UnloadImageColors(y);return same;
    };
    bool okay=true;
    for(const auto role:{UiFontRole::Display,UiFontRole::MenuBodyBold,UiFontRole::BodyBold}) {
        const ScopedUiFontRole font(role);
        setLanguage(Language::English);const auto original=uiFont();const auto width=measureUiText("135/135 +20% F3 LMB RMB TAB",30);
        setLanguage(Language::Russian);
        const bool widthSame=original.texture.id==uiFont().texture.id && width.x==measureUiText("135/135 +20% F3 LMB RMB TAB",30).x;
        if(!widthSame)std::cerr<<"font width mismatch role="<<static_cast<int>(role)<<" text="<<localizeText("135/135 +20% F3 LMB RMB TAB")<<'\n';
        okay &= widthSame;
        okay &= uiFontForCodepoint('1').texture.id==original.texture.id;
        for(const bool batch:{false,true})for(const bool rotate:{false,true}) {
            auto en=render("135/135 +20% F3 LMB RMB TAB",Language::English,batch,rotate,role);
            auto ru=render("135/135 +20% F3 LMB RMB TAB",Language::Russian,batch,rotate,role);
            const bool same=equal(en,ru);if(!same)std::cerr<<"numeric pixels mismatch role="<<static_cast<int>(role)<<" batch="<<batch<<" rotated="<<rotate<<'\n';
            okay &= same;UnloadImage(en);UnloadImage(ru);
        }
    }
    {
        const ScopedUiFontRole display(UiFontRole::Display);
        const auto primary=uiFontForCodepoint('A');const auto cyrillic=uiFontForCodepoint(0x0416);
        okay &= primary.texture.id!=cyrillic.texture.id;
        if(primary.texture.id==cyrillic.texture.id)std::cerr<<"fallback font identity mismatch\n";
        for(int codepoint=0x0410;codepoint<=0x044f;++codepoint)
            {const bool covered=GetGlyphInfo(uiFontForCodepoint(codepoint),codepoint).value==codepoint;
             if(!covered)std::cerr<<"Cyrillic missing "<<codepoint<<'\n';okay &= covered;}
        for(int codepoint:{0x0401,0x0451})okay &= GetGlyphInfo(uiFontForCodepoint(codepoint),codepoint).value==codepoint;
        for(const bool rotate:{false,true}) {
            auto direct=render("ДЕНЬ 12 · УРОН +20%",Language::Russian,false,rotate,UiFontRole::Display);
            auto batched=render("ДЕНЬ 12 · УРОН +20%",Language::Russian,true,rotate,UiFontRole::Display);
            const bool same=equal(direct,batched);
            if(!same)std::cerr<<"mixed pixel mismatch rotated="<<rotate<<'\n';
            okay &= same && inspectTextRegion(direct).visiblePixels>1000;
            if(!captures.empty())ExportImage(direct,(captures+(rotate?"/mixed-rotated.png":"/mixed-inline.png")).c_str());
            UnloadImage(direct);UnloadImage(batched);
        }
        setLanguage(Language::English);auto literal=render("ДЕНЬ 12",Language::English,true,false,UiFontRole::Display);
        auto translated=render("DAY 12",Language::Russian,true,false,UiFontRole::Display);
        const bool translatedSame=equal(literal,translated);
        if(!translatedSame)std::cerr<<"translation pixels mismatch: "<<localizeText("DAY 12")<<'\n';
        okay &= translatedSame;UnloadImage(literal);UnloadImage(translated);
        okay &= uiFont().texture.id==primary.texture.id;
    }
    setLanguage(oldLanguage);
    if(!okay)std::cerr<<"Per-glyph font fallback, Cyrillic coverage, numeric parity or mixed-font layout failed\n";
    return okay;
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
    if(!captureDirectory.empty())std::filesystem::create_directories(captureDirectory);
    const bool fallbackValid=mixedFontFallbackKeepsCiko(captureDirectory);
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
    const bool valid = fallbackValid && validReference(bounds1080) &&
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
