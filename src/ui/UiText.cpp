#include "ui/UiRenderLayer.hpp"
#include "ui/UiText.hpp"
#include "ui/UiCString.hpp"
#include "core/PerformanceStats.hpp"
#include "localization/Localization.hpp"

#include <rlgl.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ian {
namespace {

UiFontRole currentFontRole{UiFontRole::Display};
std::array<Font, 2> bodyFonts{};
std::array<Font, 2> bodySdfFonts{};
std::array<Font, 2> bodySmallSdfFonts{};
constexpr std::array BodyFontPaths{
    "assets/ui/BalsamiqSans-Regular.ttf", "assets/ui/BalsamiqSans-Bold.ttf"};

Font primaryFont{};
Font cyrillicFallbackFont{};
Font primarySdfFont{};
Font cyrillicFallbackSdfFont{};
Font primarySmallSdfFont{};
Font cyrillicFallbackSmallSdfFont{};
Shader sdfTextShader{};
int sdfFramebufferScaleLocation{-1};
bool initialized{};
UiTextPerformanceStats textPerformanceStats{};
struct QueuedSdfText {
    std::string text;
    Vector2 position{};
    float scaledSize{};
    SdfUiTextStyle style{};
    UiFontRole fontRole{UiFontRole::Display};
    float rotationRadians{};
    Vector2 rotationCenter{};
    bool cacheable{true};
};
std::vector<QueuedSdfText> sdfTextBatch;
bool sdfTextBatchActive{};

struct CachedGlyphQuad {
    Rectangle source{};
    Rectangle destination{};
};

struct CachedGlyphLayout {
    std::vector<CachedGlyphQuad> quads;
};

struct SdfTextTextureKey {
    std::string text;
    unsigned int textureId{};
    std::uint32_t scaledSizeBits{};
    std::uint32_t fillColor{};
    std::uint32_t outlineColor{};
    std::uint32_t shadowColor{};
    std::uint32_t shadowOffsetXBits{};
    std::uint32_t shadowOffsetYBits{};
    std::uint32_t outlinePixelsBits{};
    std::uint32_t softnessBits{};

    bool operator==(const SdfTextTextureKey&) const = default;
};

struct SdfTextTextureKeyHash {
    std::size_t operator()(const SdfTextTextureKey& key) const noexcept;
};

struct CachedSdfTextTexture {
    RenderTexture2D target{};
    Vector2 drawOffset{};
    std::size_t pixels{};
    std::uint64_t lastUsedFrame{};
};

struct SdfTextObservation {
    std::uint64_t lastSeenFrame{};
    unsigned int consecutiveFrames{};
};

struct GlyphLayoutKey {
    std::string text;
    unsigned int textureId{};
    std::uint32_t scaledSizeBits{};

    bool operator==(const GlyphLayoutKey&) const = default;
};

struct GlyphLayoutKeyHash {
    std::size_t operator()(const GlyphLayoutKey& key) const noexcept {
        std::size_t hash = std::hash<std::string>{}(key.text);
        hash ^= std::hash<unsigned int>{}(key.textureId) +
            0x9e3779b9U + (hash << 6U) + (hash >> 2U);
        hash ^= std::hash<std::uint32_t>{}(key.scaledSizeBits) +
            0x9e3779b9U + (hash << 6U) + (hash >> 2U);
        return hash;
    }
};

std::unordered_map<GlyphLayoutKey, CachedGlyphLayout,
                   GlyphLayoutKeyHash> glyphLayoutCache;
std::unordered_map<SdfTextTextureKey, CachedSdfTextTexture,
                   SdfTextTextureKeyHash> sdfTextTextureCache;
std::unordered_map<SdfTextTextureKey, SdfTextObservation,
                   SdfTextTextureKeyHash> sdfTextObservations;
std::size_t cachedSdfTextPixels{};
std::uint64_t sdfTextCacheFrame{};
RenderTexture2D hudTextBatchTarget{};
std::size_t hudTextBatchHash{};
bool hudTextBatchCacheValid{};
std::size_t hudTextBatchCandidateHash{};
unsigned int hudTextBatchCandidateFrames{};
std::vector<Rectangle> hudTextBatchRegions;

class ScopedTextPerformanceSample {
  public:
    explicit ScopedTextPerformanceSample(std::size_t characters)
        : start_(PerformanceClock::now()) {
        ++textPerformanceStats.submissions;
        textPerformanceStats.characters += characters;
    }

    ~ScopedTextPerformanceSample() {
        textPerformanceStats.milliseconds +=
            performanceMilliseconds(start_);
    }

  private:
    PerformanceClock::time_point start_;
};

constexpr int AsciiFirst = 32;
constexpr int AsciiLast = 126;
constexpr int CyrillicFirst = 0x0400;
constexpr int CyrillicLast = 0x04ff;
constexpr std::array<int, 5> ExtraUiCodepoints{
    0x00b7, // middle dot: ·
    0x2014, // em dash: —
    0x2022, // bullet: •
    0x221e, // infinity: ∞
    0x2713, // check mark: ✓
};
constexpr int GlyphCount =
    (AsciiLast - AsciiFirst + 1) +
    (CyrillicLast - CyrillicFirst + 1) +
    static_cast<int>(ExtraUiCodepoints.size());
constexpr int LatinGlyphCount =
    (AsciiLast - AsciiFirst + 1) +
    static_cast<int>(ExtraUiCodepoints.size());
constexpr float UiTextScale = 1.68F;
constexpr float TwoPi = 6.28318530717958647692F;
constexpr int SdfBaseGlyphSize = 64;
constexpr int SmallSdfBaseGlyphSize = 32;
constexpr float SmallSdfMaximumDrawSize = 40.0F;
constexpr int SdfAtlasPackingPadding = 1;
constexpr std::size_t MaximumCachedGlyphLayouts = 2048U;
constexpr std::size_t MaximumCachedSdfTextTextures = 384U;
constexpr std::size_t MaximumCachedSdfTextPixels = 12U * 1024U * 1024U;
constexpr std::size_t MaximumSdfTextObservations = 2048U;
constexpr Color DefaultSdfOutlineColor{48, 50, 56, 255};

void hashCombine(std::size_t& hash, std::size_t value) noexcept {
    hash ^= value + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
}

std::uint32_t packedColor(Color color) noexcept {
    return static_cast<std::uint32_t>(color.r) |
        (static_cast<std::uint32_t>(color.g) << 8U) |
        (static_cast<std::uint32_t>(color.b) << 16U) |
        (static_cast<std::uint32_t>(color.a) << 24U);
}

std::size_t SdfTextTextureKeyHash::operator()(
    const SdfTextTextureKey& key) const noexcept {
    std::size_t hash = std::hash<std::string>{}(key.text);
    hashCombine(hash, key.textureId);
    hashCombine(hash, key.scaledSizeBits);
    hashCombine(hash, key.fillColor);
    hashCombine(hash, key.outlineColor);
    hashCombine(hash, key.shadowColor);
    hashCombine(hash, key.shadowOffsetXBits);
    hashCombine(hash, key.shadowOffsetYBits);
    hashCombine(hash, key.outlinePixelsBits);
    hashCombine(hash, key.softnessBits);
    return hash;
}

bool isDrawableFont(const Font& font) {
    return IsFontValid(font) && IsTextureValid(font.texture);
}

Font loadSdfFont(const char* path, const int* codepoints,
                 int codepointCount, int baseGlyphSize) {
    int fileSize = 0;
    unsigned char* fileData = LoadFileData(path, &fileSize);
    if (fileData == nullptr || fileSize <= 0) {
        return {};
    }

    Font font{};
    font.baseSize = baseGlyphSize;
    font.glyphPadding = 0;
    font.glyphs = LoadFontData(
        fileData, fileSize, font.baseSize,
        codepoints, codepointCount, FONT_SDF,
        &font.glyphCount);
    UnloadFileData(fileData);
    if (font.glyphs == nullptr || font.glyphCount <= 0) {
        return {};
    }

    Image atlas = GenImageFontAtlas(
        font.glyphs, &font.recs, font.glyphCount,
        font.baseSize, SdfAtlasPackingPadding, 1);
    if (!IsImageValid(atlas) || font.recs == nullptr) {
        if (IsImageValid(atlas)) {
            UnloadImage(atlas);
        }
        UnloadFontData(font.glyphs, font.glyphCount);
        if (font.recs != nullptr) {
            MemFree(font.recs);
        }
        return {};
    }
    font.texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    if (!IsTextureValid(font.texture)) {
        UnloadFontData(font.glyphs, font.glyphCount);
        MemFree(font.recs);
        return {};
    }
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(font.texture, TEXTURE_WRAP_CLAMP);
    return font;
}

Font activeSdfFont(float scaledSize, UiFontRole role) {
    const bool preferSmall = scaledSize <= SmallSdfMaximumDrawSize;
    if (role != UiFontRole::Display) {
        const std::size_t index = role == UiFontRole::BodyBold ? 1U : 0U;
        if (preferSmall && isDrawableFont(bodySmallSdfFonts[index])) {
            return bodySmallSdfFonts[index];
        }
        if (isDrawableFont(bodySdfFonts[index])) return bodySdfFonts[index];
    }
    if (currentLanguage() == Language::English) {
        if (preferSmall && isDrawableFont(primarySmallSdfFont)) {
            return primarySmallSdfFont;
        }
        if (isDrawableFont(primarySdfFont)) {
            return primarySdfFont;
        }
        if (isDrawableFont(primarySmallSdfFont)) {
            return primarySmallSdfFont;
        }
    }
    if (preferSmall && isDrawableFont(cyrillicFallbackSmallSdfFont)) {
        return cyrillicFallbackSmallSdfFont;
    }
    if (isDrawableFont(cyrillicFallbackSdfFont)) {
        return cyrillicFallbackSdfFont;
    }
    if (isDrawableFont(cyrillicFallbackSmallSdfFont)) {
        return cyrillicFallbackSmallSdfFont;
    }
    if (isDrawableFont(primarySdfFont)) {
        return primarySdfFont;
    }
    return isDrawableFont(primarySmallSdfFont)
        ? primarySmallSdfFont : Font{};
}

bool isSdfTextReady(float scaledSize) {
    return IsShaderValid(sdfTextShader) &&
        isDrawableFont(activeSdfFont(scaledSize, currentFontRole)) &&
        sdfFramebufferScaleLocation >= 0;
}

float defaultOutlinePixels(float fontSize) {
    return std::clamp(fontSize * 0.13F, 1.5F, 3.5F);
}

void drawBitmapUiText(const char* value, Vector2 position,
                      float scaledSize, Color color) {
    const Color shadow{
        7, 9, 11,
        static_cast<unsigned char>(
            static_cast<unsigned int>(color.a) * 3U / 4U),
    };
    DrawTextEx(
        uiFont(), value,
        {position.x + 1.5F, position.y + 2.0F},
        scaledSize, scaledSize * 0.02F, shadow);
    DrawTextEx(
        uiFont(), value, position, scaledSize,
        scaledSize * 0.02F, color);
}

enum class SdfTextLayer {
    Shadow,
    Outline,
    Fill,
    Combined,
};

bool isStandaloneInfinity(std::string_view text);
bool isStandaloneCheck(std::string_view text);

void configureSdfTextShader(
    std::optional<float> forcedFramebufferScale = std::nullopt) {
    // The active projection also handles offscreen targets, whose size can
    // differ from the window. Window dimensions would inflate their outline.
    const Matrix projection = rlGetMatrixProjection();
    const float framebufferScale = forcedFramebufferScale.value_or(
        0.25F * (
            static_cast<float>(std::max(GetRenderWidth(), 1)) *
                std::abs(projection.m0) +
            static_cast<float>(std::max(GetRenderHeight(), 1)) *
                std::abs(projection.m5)));
    SetShaderValue(
        sdfTextShader, sdfFramebufferScaleLocation,
        &framebufferScale, SHADER_UNIFORM_FLOAT);
}

CachedGlyphLayout buildGlyphLayout(
    std::string_view text, Font font, float scaledSize) {
    CachedGlyphLayout layout;
    layout.quads.reserve(text.size());
    const float scaleFactor = scaledSize /
        static_cast<float>(font.baseSize);
    const float spacing = scaledSize * 0.02F;
    float offsetX = 0.0F;
    float offsetY = 0.0F;
    for (std::size_t byteIndex = 0; byteIndex < text.size();) {
        int byteCount = 0;
        const int codepoint = GetCodepointNext(
            text.data() + byteIndex, &byteCount);
        if (byteCount <= 0) byteCount = 1;
        byteIndex += static_cast<std::size_t>(byteCount);
        const int glyphIndex = GetGlyphIndex(font, codepoint);
        if (glyphIndex < 0 || glyphIndex >= font.glyphCount) continue;
        if (codepoint == '\n') {
            // SDF HUD strings are single-line. Match raylib's default line
            // spacing for defensive multiline support.
            offsetY += scaledSize + 15.0F;
            offsetX = 0.0F;
            continue;
        }
        if (codepoint != ' ' && codepoint != '\t') {
            const Rectangle source = font.recs[glyphIndex];
            layout.quads.push_back({
                .source = source,
                .destination = {
                    offsetX + static_cast<float>(
                        font.glyphs[glyphIndex].offsetX) * scaleFactor,
                    offsetY + static_cast<float>(
                        font.glyphs[glyphIndex].offsetY) * scaleFactor,
                    source.width * scaleFactor,
                    source.height * scaleFactor,
                },
            });
        }
        const int advance = font.glyphs[glyphIndex].advanceX;
        offsetX += (advance == 0
            ? font.recs[glyphIndex].width * scaleFactor
            : static_cast<float>(advance) * scaleFactor) + spacing;
    }
    return layout;
}

const CachedGlyphLayout& cachedGlyphLayout(
    std::string_view text, Font font, float scaledSize) {
    if (glyphLayoutCache.size() >= MaximumCachedGlyphLayouts) {
        glyphLayoutCache.clear();
    }
    GlyphLayoutKey key{
        .text = std::string{text},
        .textureId = font.texture.id,
        .scaledSizeBits = std::bit_cast<std::uint32_t>(scaledSize),
    };
    auto found = glyphLayoutCache.find(key);
    if (found != glyphLayoutCache.end()) return found->second;
    auto [inserted, unused] = glyphLayoutCache.emplace(
        std::move(key), buildGlyphLayout(text, font, scaledSize));
    static_cast<void>(unused);
    return inserted->second;
}

void emitCachedText(const QueuedSdfText& queued, Font font,
                    const SdfUiTextStyle& pass, Vector2 offset) {
    const CachedGlyphLayout& layout = cachedGlyphLayout(
        queued.text, font, queued.scaledSize);
    rlColor4ub(pass.fillColor.r, pass.fillColor.g,
               pass.fillColor.b, pass.fillColor.a);
    // The 2D batch normal channel carries per-glyph style parameters.
    rlNormal3f(std::clamp(pass.outlinePixels, 0.0F, 4.0F),
               std::clamp(pass.softness, 0.5F, 1.5F), 0.0F);
    const float inverseWidth = 1.0F / static_cast<float>(font.texture.width);
    const float inverseHeight = 1.0F / static_cast<float>(font.texture.height);
    const float cosine = std::cos(queued.rotationRadians);
    const float sine = std::sin(queued.rotationRadians);
    const auto vertex = [&queued, cosine, sine](float x, float y) {
        if (queued.rotationRadians == 0.0F) {
            rlVertex2f(x, y);
            return;
        }
        const float localX = x - queued.rotationCenter.x;
        const float localY = y - queued.rotationCenter.y;
        rlVertex2f(
            queued.rotationCenter.x + localX * cosine - localY * sine,
            queued.rotationCenter.y + localX * sine + localY * cosine);
    };
    for (const CachedGlyphQuad& glyph : layout.quads) {
        const float u0 = glyph.source.x * inverseWidth;
        const float v0 = glyph.source.y * inverseHeight;
        const float u1 = (glyph.source.x + glyph.source.width) * inverseWidth;
        const float v1 = (glyph.source.y + glyph.source.height) * inverseHeight;
        const float x0 = queued.position.x + offset.x + glyph.destination.x;
        const float y0 = queued.position.y + offset.y + glyph.destination.y;
        const float x1 = x0 + glyph.destination.width;
        const float y1 = y0 + glyph.destination.height;
        rlTexCoord2f(u0, v0); vertex(x0, y0);
        rlTexCoord2f(u0, v1); vertex(x0, y1);
        rlTexCoord2f(u1, v1); vertex(x1, y1);
        rlTexCoord2f(u1, v0); vertex(x1, y0);
    }
}

SdfUiTextStyle textLayerStyle(const SdfUiTextStyle& style,
                             SdfTextLayer layer) {
    SdfUiTextStyle pass = style;
    if (layer == SdfTextLayer::Fill) {
        pass.outlinePixels = 0.0F;
        pass.outlineColor = BLANK;
    } else {
        pass.fillColor = layer == SdfTextLayer::Shadow
            ? style.shadowColor : style.outlineColor;
        pass.fillColor.a = static_cast<unsigned char>(
            static_cast<unsigned int>(pass.fillColor.a) *
            style.fillColor.a / 255U);
        pass.outlineColor = pass.fillColor;
        // The shader multiplies outline alpha by vertex alpha.
        pass.outlineColor.a = 255;
    }
    pass.shadowColor = BLANK;
    pass.shadowOffsetPixels = {};
    return pass;
}

bool queueSdfText(std::string_view text, Vector2 position,
                  float scaledSize, const SdfUiTextStyle& style,
                  float rotationRadians = 0.0F,
                  Vector2 rotationCenter = {},
                  bool cacheable = true) {
    if (!sdfTextBatchActive || !isSdfTextReady(scaledSize) ||
        isStandaloneInfinity(text) || isStandaloneCheck(text)) {
        return false;
    }
    std::string renderedText = currentLanguage() == Language::English
        ? std::string{text}
        : localizeText(text);
    if (isStandaloneInfinity(renderedText) ||
        isStandaloneCheck(renderedText)) {
        return false;
    }
    sdfTextBatch.push_back({
        .text = std::move(renderedText),
        .position = position,
        .scaledSize = scaledSize,
        .style = style,
        .fontRole = currentFontRole,
        .rotationRadians = rotationRadians,
        .rotationCenter = rotationCenter,
        .cacheable = cacheable,
    });
    return true;
}

bool textLayerVisible(const SdfUiTextStyle& style,
                      SdfTextLayer layer) {
    switch (layer) {
    case SdfTextLayer::Shadow:
        return style.shadowColor.a > 0U;
    case SdfTextLayer::Outline:
        return style.outlineColor.a > 0U &&
            style.outlinePixels > 0.0F;
    case SdfTextLayer::Fill:
        return style.fillColor.a > 0U;
    case SdfTextLayer::Combined:
        return true;
    }
    return false;
}

void drawSdfTextItemsDirect(
    std::span<const QueuedSdfText* const> items,
    std::optional<float> forcedFramebufferScale = std::nullopt) {
    if (items.empty()) return;
    configureSdfTextShader(forcedFramebufferScale);
    BeginShaderMode(sdfTextShader);
    for (const SdfTextLayer layer : {
             SdfTextLayer::Shadow,
             SdfTextLayer::Outline,
             SdfTextLayer::Fill}) {
        std::array<unsigned int, 4> submittedTextures{};
        std::size_t submittedTextureCount = 0U;
        for (const QueuedSdfText* queued : items) {
            if (!textLayerVisible(queued->style, layer)) {
                continue;
            }
            const Font font = activeSdfFont(queued->scaledSize, queued->fontRole);
            const auto submittedEnd = submittedTextures.begin() +
                static_cast<std::ptrdiff_t>(submittedTextureCount);
            if (std::find(
                    submittedTextures.begin(), submittedEnd,
                    font.texture.id) != submittedEnd) {
                continue;
            }
            submittedTextures[submittedTextureCount++] = font.texture.id;
            rlSetTexture(font.texture.id);
            rlBegin(RL_QUADS);
            for (const QueuedSdfText* candidate : items) {
                if (!textLayerVisible(candidate->style, layer)) {
                    continue;
                }
                const Font candidateFont = activeSdfFont(
                    candidate->scaledSize, candidate->fontRole);
                if (candidateFont.texture.id != font.texture.id) {
                    continue;
                }
                const SdfUiTextStyle pass = textLayerStyle(
                    candidate->style, layer);
                const Vector2 offset = layer == SdfTextLayer::Shadow
                    ? candidate->style.shadowOffsetPixels
                    : Vector2{};
                emitCachedText(
                    *candidate, candidateFont, pass, offset);
            }
            rlEnd();
        }
    }
    rlSetTexture(0);
    rlNormal3f(0.0F, 0.0F, 1.0F);
    EndShaderMode();
}

SdfTextTextureKey sdfTextTextureKey(
    const QueuedSdfText& queued, Font font) {
    return {
        .text = queued.text,
        .textureId = font.texture.id,
        .scaledSizeBits =
            std::bit_cast<std::uint32_t>(queued.scaledSize),
        .fillColor = packedColor(queued.style.fillColor),
        .outlineColor = packedColor(queued.style.outlineColor),
        .shadowColor = packedColor(queued.style.shadowColor),
        .shadowOffsetXBits = std::bit_cast<std::uint32_t>(
            queued.style.shadowOffsetPixels.x),
        .shadowOffsetYBits = std::bit_cast<std::uint32_t>(
            queued.style.shadowOffsetPixels.y),
        .outlinePixelsBits = std::bit_cast<std::uint32_t>(
            queued.style.outlinePixels),
        .softnessBits = std::bit_cast<std::uint32_t>(
            queued.style.softness),
    };
}

void unloadCachedSdfTextTexture(CachedSdfTextTexture& cached) {
    if (IsRenderTextureValid(cached.target)) {
        UnloadRenderTexture(cached.target);
    }
    cached.target = {};
}

bool makeRoomForCachedSdfText(std::size_t pixels) {
    while (sdfTextTextureCache.size() >=
               MaximumCachedSdfTextTextures ||
           cachedSdfTextPixels + pixels >
               MaximumCachedSdfTextPixels) {
        auto victim = sdfTextTextureCache.end();
        std::uint64_t oldestFrame =
            std::numeric_limits<std::uint64_t>::max();
        for (auto iterator = sdfTextTextureCache.begin();
             iterator != sdfTextTextureCache.end(); ++iterator) {
            if (iterator->second.lastUsedFrame == sdfTextCacheFrame) {
                continue;
            }
            if (iterator->second.lastUsedFrame < oldestFrame) {
                oldestFrame = iterator->second.lastUsedFrame;
                victim = iterator;
            }
        }
        if (victim == sdfTextTextureCache.end()) return false;
        cachedSdfTextPixels -= victim->second.pixels;
        unloadCachedSdfTextTexture(victim->second);
        sdfTextTextureCache.erase(victim);
    }
    return true;
}

bool sdfTextWasStable(const SdfTextTextureKey& key) {
    if (sdfTextObservations.size() >= MaximumSdfTextObservations) {
        sdfTextObservations.clear();
    }
    auto [iterator, inserted] = sdfTextObservations.try_emplace(
        key, SdfTextObservation{sdfTextCacheFrame, 1U});
    if (inserted) return false;
    SdfTextObservation& observation = iterator->second;
    if (observation.lastSeenFrame == sdfTextCacheFrame ||
        observation.lastSeenFrame + 1U == sdfTextCacheFrame) {
        ++observation.consecutiveFrames;
    } else {
        observation.consecutiveFrames = 1U;
    }
    observation.lastSeenFrame = sdfTextCacheFrame;
    return observation.consecutiveFrames >= 2U;
}

std::optional<CachedSdfTextTexture> buildCachedSdfTextTexture(
    const QueuedSdfText& queued, Font font) {
    const CachedGlyphLayout& layout = cachedGlyphLayout(
        queued.text, font, queued.scaledSize);
    if (layout.quads.empty()) return std::nullopt;

    float minimumX = std::numeric_limits<float>::max();
    float minimumY = std::numeric_limits<float>::max();
    float maximumX = std::numeric_limits<float>::lowest();
    float maximumY = std::numeric_limits<float>::lowest();
    const auto includeQuads = [&](Vector2 offset) {
        for (const CachedGlyphQuad& glyph : layout.quads) {
            minimumX = std::min(
                minimumX, glyph.destination.x + offset.x);
            minimumY = std::min(
                minimumY, glyph.destination.y + offset.y);
            maximumX = std::max(
                maximumX,
                glyph.destination.x + glyph.destination.width + offset.x);
            maximumY = std::max(
                maximumY,
                glyph.destination.y + glyph.destination.height + offset.y);
        }
    };
    includeQuads({});
    if (queued.style.shadowColor.a > 0U) {
        includeQuads(queued.style.shadowOffsetPixels);
    }
    constexpr float Padding = 2.0F;
    const float drawOffsetX = std::floor(minimumX - Padding);
    const float drawOffsetY = std::floor(minimumY - Padding);
    const int width = std::max(
        1, static_cast<int>(std::ceil(
            maximumX + Padding - drawOffsetX)));
    const int height = std::max(
        1, static_cast<int>(std::ceil(
            maximumY + Padding - drawOffsetY)));
    const std::size_t pixels =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height);
    if (pixels > MaximumCachedSdfTextPixels ||
        !makeRoomForCachedSdfText(pixels)) {
        return std::nullopt;
    }

    RenderTexture2D target = LoadRenderTexture(width, height);
    if (!IsRenderTextureValid(target)) return std::nullopt;
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    QueuedSdfText local = queued;
    local.position = {-drawOffsetX, -drawOffsetY};
    const QueuedSdfText* localPointer = &local;
    BeginTextureMode(target);
    ClearBackground(BLANK);
    rlSetBlendFactorsSeparate(
        RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
        RL_ONE, RL_ONE_MINUS_SRC_ALPHA,
        RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    drawSdfTextItemsDirect(
        std::span<const QueuedSdfText* const>{&localPointer, 1U},
        1.0F);
    EndBlendMode();
    EndTextureMode();
    restoreUiRenderLayer();
    return CachedSdfTextTexture{
        .target = target,
        .drawOffset = {drawOffsetX, drawOffsetY},
        .pixels = pixels,
        .lastUsedFrame = sdfTextCacheFrame,
    };
}

CachedSdfTextTexture* cachedSdfTextTexture(
    const QueuedSdfText& queued) {
    const Font font = activeSdfFont(queued.scaledSize, queued.fontRole);
    SdfTextTextureKey key = sdfTextTextureKey(queued, font);
    auto found = sdfTextTextureCache.find(key);
    if (found != sdfTextTextureCache.end()) {
        found->second.lastUsedFrame = sdfTextCacheFrame;
        return &found->second;
    }
    if (!sdfTextWasStable(key)) return nullptr;
    std::optional<CachedSdfTextTexture> cached =
        buildCachedSdfTextTexture(queued, font);
    if (!cached) return nullptr;
    cachedSdfTextPixels += cached->pixels;
    sdfTextObservations.erase(key);
    auto [inserted, unused] = sdfTextTextureCache.emplace(
        std::move(key), std::move(*cached));
    static_cast<void>(unused);
    return &inserted->second;
}

struct CachedSdfTextDraw {
    Texture2D texture{};
    Rectangle destination{};
};

// Text is rendered on the main thread. Retain these buffers between frames so
// ordinary HUD and combat batches do not allocate temporary pointer arrays.
std::vector<const QueuedSdfText*> sdfTextPointerScratch;
std::vector<CachedSdfTextDraw> sdfTextCachedDrawScratch;

std::size_t currentSdfTextBatchHash() {
    std::size_t hash = sdfTextBatch.size();
    hashCombine(hash, static_cast<std::size_t>(GetScreenWidth()));
    hashCombine(hash, static_cast<std::size_t>(GetScreenHeight()));
    for (const QueuedSdfText& queued : sdfTextBatch) {
        hashCombine(hash, std::hash<std::string>{}(queued.text));
        hashCombine(hash, static_cast<std::size_t>(queued.fontRole));
        hashCombine(hash, std::bit_cast<std::uint32_t>(queued.position.x));
        hashCombine(hash, std::bit_cast<std::uint32_t>(queued.position.y));
        hashCombine(hash, std::bit_cast<std::uint32_t>(queued.scaledSize));
        hashCombine(hash, packedColor(queued.style.fillColor));
        hashCombine(hash, packedColor(queued.style.outlineColor));
        hashCombine(hash, packedColor(queued.style.shadowColor));
        hashCombine(hash, std::bit_cast<std::uint32_t>(
            queued.style.shadowOffsetPixels.x));
        hashCombine(hash, std::bit_cast<std::uint32_t>(
            queued.style.shadowOffsetPixels.y));
        hashCombine(hash, std::bit_cast<std::uint32_t>(
            queued.style.outlinePixels));
        hashCombine(hash, std::bit_cast<std::uint32_t>(
            queued.style.softness));
        hashCombine(hash, std::bit_cast<std::uint32_t>(
            queued.rotationRadians));
        hashCombine(hash, std::bit_cast<std::uint32_t>(
            queued.rotationCenter.x));
        hashCombine(hash, std::bit_cast<std::uint32_t>(
            queued.rotationCenter.y));
    }
    return hash;
}

bool ensureHudTextBatchTarget() {
    const int width = std::max(GetScreenWidth(), 1);
    const int height = std::max(GetScreenHeight(), 1);
    if (IsRenderTextureValid(hudTextBatchTarget) &&
        hudTextBatchTarget.texture.width == width &&
        hudTextBatchTarget.texture.height == height) {
        return true;
    }
    if (IsRenderTextureValid(hudTextBatchTarget)) {
        UnloadRenderTexture(hudTextBatchTarget);
    }
    hudTextBatchTarget = LoadRenderTexture(width, height);
    hudTextBatchCacheValid = false;
    hudTextBatchRegions.clear();
    if (!IsRenderTextureValid(hudTextBatchTarget)) {
        return false;
    }
    SetTextureFilter(
        hudTextBatchTarget.texture, TEXTURE_FILTER_BILINEAR);
    return true;
}

std::vector<Rectangle> buildHudTextBatchRegions(
    std::span<const QueuedSdfText* const> items) {
    constexpr int TileSize = 128;
    constexpr float SamplingPadding = 2.0F;
    const int width = std::max(GetScreenWidth(), 1);
    const int height = std::max(GetScreenHeight(), 1);
    const int columnCount = (width + TileSize - 1) / TileSize;
    const int rowCount = (height + TileSize - 1) / TileSize;
    std::vector<unsigned char> occupied(
        static_cast<std::size_t>(columnCount * rowCount), 0U);

    for (const QueuedSdfText* queued : items) {
        const Font font = activeSdfFont(queued->scaledSize, queued->fontRole);
        const CachedGlyphLayout& layout = cachedGlyphLayout(
            queued->text, font, queued->scaledSize);
        if (layout.quads.empty()) continue;

        float minimumX = std::numeric_limits<float>::max();
        float minimumY = std::numeric_limits<float>::max();
        float maximumX = std::numeric_limits<float>::lowest();
        float maximumY = std::numeric_limits<float>::lowest();
        for (const CachedGlyphQuad& glyph : layout.quads) {
            minimumX = std::min(minimumX, glyph.destination.x);
            minimumY = std::min(minimumY, glyph.destination.y);
            maximumX = std::max(
                maximumX,
                glyph.destination.x + glyph.destination.width);
            maximumY = std::max(
                maximumY,
                glyph.destination.y + glyph.destination.height);
        }

        minimumX += queued->position.x + std::min(
            0.0F, queued->style.shadowOffsetPixels.x) - SamplingPadding;
        minimumY += queued->position.y + std::min(
            0.0F, queued->style.shadowOffsetPixels.y) - SamplingPadding;
        maximumX += queued->position.x + std::max(
            0.0F, queued->style.shadowOffsetPixels.x) + SamplingPadding;
        maximumY += queued->position.y + std::max(
            0.0F, queued->style.shadowOffsetPixels.y) + SamplingPadding;

        const int firstColumn = std::clamp(
            static_cast<int>(std::floor(minimumX / TileSize)),
            0, columnCount - 1);
        const int lastColumn = std::clamp(
            static_cast<int>(std::floor(maximumX / TileSize)),
            0, columnCount - 1);
        const int firstRow = std::clamp(
            static_cast<int>(std::floor(minimumY / TileSize)),
            0, rowCount - 1);
        const int lastRow = std::clamp(
            static_cast<int>(std::floor(maximumY / TileSize)),
            0, rowCount - 1);
        if (maximumX < 0.0F || maximumY < 0.0F ||
            minimumX >= static_cast<float>(width) ||
            minimumY >= static_cast<float>(height)) {
            continue;
        }
        for (int row = firstRow; row <= lastRow; ++row) {
            for (int column = firstColumn;
                 column <= lastColumn; ++column) {
                occupied[static_cast<std::size_t>(
                    row * columnCount + column)] = 1U;
            }
        }
    }

    std::vector<Rectangle> regions;
    for (int row = 0; row < rowCount; ++row) {
        for (int column = 0; column < columnCount;) {
            if (occupied[static_cast<std::size_t>(
                    row * columnCount + column)] == 0U) {
                ++column;
                continue;
            }
            const int firstColumn = column;
            while (column < columnCount &&
                   occupied[static_cast<std::size_t>(
                       row * columnCount + column)] != 0U) {
                ++column;
            }
            const float x = static_cast<float>(firstColumn * TileSize);
            const float y = static_cast<float>(row * TileSize);
            const float regionWidth = static_cast<float>(
                std::min(column * TileSize, width) - firstColumn * TileSize);
            const float regionHeight = static_cast<float>(
                std::min((row + 1) * TileSize, height) - row * TileSize);

            auto previous = std::find_if(
                regions.rbegin(), regions.rend(),
                [x, y, regionWidth](const Rectangle& region) {
                    return region.x == x && region.width == regionWidth &&
                        region.y + region.height == y;
                });
            if (previous != regions.rend()) {
                previous->height += regionHeight;
            } else {
                regions.push_back({x, y, regionWidth, regionHeight});
            }
        }
    }
    return regions;
}

bool drawCachedHudTextBatch(
    std::span<const QueuedSdfText* const> items) {
    if (!ensureHudTextBatchTarget()) {
        return false;
    }
    const std::size_t hash = currentSdfTextBatchHash();
    if (!hudTextBatchCacheValid || hash != hudTextBatchHash) {
        // Compass labels, counters and animated prompts can change this hash
        // every frame. Rebuilding a full-screen render target for a value
        // that will never be reused is slower than submitting the atlas mesh
        // directly. Promote a batch to the texture cache only after it has
        // survived for two consecutive frames.
        if (hash != hudTextBatchCandidateHash) {
            hudTextBatchCandidateHash = hash;
            hudTextBatchCandidateFrames = 1U;
            return false;
        }
        ++hudTextBatchCandidateFrames;
        if (hudTextBatchCandidateFrames < 2U) {
            return false;
        }
        BeginTextureMode(hudTextBatchTarget);
        ClearBackground(BLANK);
        rlSetBlendFactorsSeparate(
            RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
            RL_ONE, RL_ONE_MINUS_SRC_ALPHA,
            RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        drawSdfTextItemsDirect(items, 1.0F);
        EndBlendMode();
        EndTextureMode();
    restoreUiRenderLayer();
        hudTextBatchRegions = buildHudTextBatchRegions(items);
        hudTextBatchHash = hash;
        hudTextBatchCacheValid = true;
    }
    hudTextBatchCandidateHash = hash;
    hudTextBatchCandidateFrames = 2U;
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    for (const Rectangle& region : hudTextBatchRegions) {
        DrawTexturePro(
            hudTextBatchTarget.texture,
            {region.x,
             static_cast<float>(hudTextBatchTarget.texture.height) -
                 region.y - region.height,
             region.width, -region.height},
            region, {}, 0.0F, WHITE);
    }
    EndBlendMode();
    return true;
}

void drawSdfTextBatch() {
    // Large HUD batches are cheaper as glyph geometry from one font atlas
    // than as many separately cached text textures and texture switches.
    constexpr std::size_t DirectAtlasBatchThreshold = 8U;
    if (sdfTextBatch.size() >= DirectAtlasBatchThreshold) {
        sdfTextPointerScratch.clear();
        sdfTextPointerScratch.reserve(sdfTextBatch.size());
        for (const QueuedSdfText& queued : sdfTextBatch) {
            sdfTextPointerScratch.push_back(&queued);
        }
        const bool cacheable = std::ranges::all_of(
            sdfTextBatch,
            [](const QueuedSdfText& queued) {
                return queued.cacheable;
            });
        if (cacheable && rlGetActiveFramebuffer() == 0U &&
            drawCachedHudTextBatch(sdfTextPointerScratch)) {
            return;
        }
        drawSdfTextItemsDirect(sdfTextPointerScratch);
        return;
    }

    sdfTextPointerScratch.clear();
    sdfTextPointerScratch.reserve(sdfTextBatch.size());
    sdfTextCachedDrawScratch.clear();
    sdfTextCachedDrawScratch.reserve(sdfTextBatch.size());

    // On-demand cache rendering temporarily changes the framebuffer. Keep
    // nested render-texture users on the direct path so their target remains
    // untouched.
    const bool mayBuildCache = rlGetActiveFramebuffer() == 0U;
    for (const QueuedSdfText& queued : sdfTextBatch) {
        // Rotated text uses a screen-space pivot, so caching it into a small
        // local texture would change the pivot and produce incorrect output.
        CachedSdfTextTexture* cached =
            mayBuildCache && queued.cacheable
            ? cachedSdfTextTexture(queued) : nullptr;
        if (cached == nullptr) {
            sdfTextPointerScratch.push_back(&queued);
            continue;
        }
        sdfTextCachedDrawScratch.push_back({
            .texture = cached->target.texture,
            .destination = {
                queued.position.x + cached->drawOffset.x,
                queued.position.y + cached->drawOffset.y,
                static_cast<float>(cached->target.texture.width),
                static_cast<float>(cached->target.texture.height),
            },
        });
    }

    drawSdfTextItemsDirect(sdfTextPointerScratch);
    if (!sdfTextCachedDrawScratch.empty()) {
        BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
        for (const CachedSdfTextDraw& draw : sdfTextCachedDrawScratch) {
            DrawTexturePro(
                draw.texture,
                {0.0F, 0.0F,
                 static_cast<float>(draw.texture.width),
                 -static_cast<float>(draw.texture.height)},
                draw.destination, {}, 0.0F, WHITE);
        }
        EndBlendMode();
    }
}

void drawSdfTextValue(const char* value, Vector2 position,
                      float scaledSize,
                      const SdfUiTextStyle& style) {
    const ScopedTextPerformanceSample performanceSample(
        std::char_traits<char>::length(value));
    if (!isSdfTextReady(scaledSize)) {
        drawBitmapUiText(value, position, scaledSize, style.fillColor);
        return;
    }
    sdfTextBatch.push_back({
        .text = value,
        .position = position,
        .scaledSize = scaledSize,
        .style = style,
        .fontRole = currentFontRole,
    });
    drawSdfTextBatch();
    sdfTextBatch.clear();
}

void configureFontFiltering(Font& loadedFont) {
    if (!IsFontValid(loadedFont) ||
        !IsTextureValid(loadedFont.texture)) {
        return;
    }
    SetTextureFilter(
        loadedFont.texture, TEXTURE_FILTER_BILINEAR);
}

bool isStandaloneInfinity(std::string_view text) {
    return text == "∞";
}

bool isStandaloneCheck(std::string_view text) {
    return text == "✓";
}

float specialGlyphAdvance(float scaledSize) {
    return scaledSize * 0.9F;
}

void drawInfinityGlyph(Vector2 position, float scaledSize, Color color,
                       float thicknessExpansion = 0.0F) {
    constexpr int Segments = 40;
    const Vector2 center{
        position.x + scaledSize * 0.45F,
        position.y + scaledSize * 0.51F};
    const float radiusX = scaledSize * 0.40F;
    const float radiusY = scaledSize * 0.18F;
    const float thickness = std::max(1.5F, scaledSize * 0.085F) +
        std::max(thicknessExpansion, 0.0F);
    std::array<Vector2, Segments + 1> points{};
    for (int index = 0; index <= Segments; ++index) {
        const float angle = TwoPi * static_cast<float>(index) /
                            static_cast<float>(Segments);
        points[static_cast<std::size_t>(index)] = {
            center.x + std::sin(angle) * radiusX,
            center.y + std::sin(angle) * std::cos(angle) * radiusY};
    }
    // Four unlimited-resource counters can render twelve styled infinity
    // layers every frame. DrawLineEx per segment produced hundreds of tiny
    // submissions. Emit every segment of one layer in a single rlgl batch.
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlBegin(RL_TRIANGLES);
    for (int index = 0; index < Segments; ++index) {
        const Vector2 start = points[static_cast<std::size_t>(index)];
        const Vector2 end = points[static_cast<std::size_t>(index + 1)];
        const Vector2 delta = Vector2Subtract(end, start);
        const float length = Vector2Length(delta);
        if (length <= 0.0001F) {
            continue;
        }
        const Vector2 normal{
            -delta.y / length * thickness * 0.5F,
            delta.x / length * thickness * 0.5F,
        };
        const Vector2 first = Vector2Add(start, normal);
        const Vector2 second = Vector2Subtract(start, normal);
        const Vector2 third = Vector2Subtract(end, normal);
        const Vector2 fourth = Vector2Add(end, normal);
        rlVertex2f(first.x, first.y);
        rlVertex2f(second.x, second.y);
        rlVertex2f(third.x, third.y);
        rlVertex2f(first.x, first.y);
        rlVertex2f(third.x, third.y);
        rlVertex2f(fourth.x, fourth.y);
    }
    rlEnd();
    rlColor4ub(255, 255, 255, 255);
}

void drawCheckGlyph(Vector2 position, float scaledSize, Color color,
                    float thicknessExpansion = 0.0F) {
    const float thickness = std::max(1.5F, scaledSize * 0.10F) +
        std::max(thicknessExpansion, 0.0F);
    const Vector2 start{
        position.x + scaledSize * 0.12F,
        position.y + scaledSize * 0.52F};
    const Vector2 middle{
        position.x + scaledSize * 0.37F,
        position.y + scaledSize * 0.75F};
    const Vector2 end{
        position.x + scaledSize * 0.84F,
        position.y + scaledSize * 0.22F};
    DrawLineEx(start, middle, thickness, color);
    DrawLineEx(middle, end, thickness, color);
}

bool drawStandaloneSpecialGlyph(
    std::string_view text, Vector2 position, float scaledSize,
    Color color, float thicknessExpansion = 0.0F) {
    if (isStandaloneInfinity(text)) {
        drawInfinityGlyph(
            position, scaledSize, color, thicknessExpansion);
        return true;
    }
    if (isStandaloneCheck(text)) {
        drawCheckGlyph(
            position, scaledSize, color, thicknessExpansion);
        return true;
    }
    return false;
}

bool drawStyledStandaloneSpecialGlyphLayer(
    std::string_view text, Vector2 position, float scaledSize,
    const SdfUiTextStyle& style, SdfTextLayer layer) {
    if (!isStandaloneInfinity(text) && !isStandaloneCheck(text)) {
        return false;
    }
    const float outlineExpansion = 2.0F * std::max(
        style.outlinePixels, 0.0F);
    switch (layer) {
    case SdfTextLayer::Shadow:
        if (style.shadowColor.a == 0U) return true;
        drawStandaloneSpecialGlyph(
            text,
            {position.x + style.shadowOffsetPixels.x,
             position.y + style.shadowOffsetPixels.y},
            scaledSize, style.shadowColor, outlineExpansion);
        break;
    case SdfTextLayer::Outline:
        if (style.outlineColor.a == 0U || outlineExpansion <= 0.0F) {
            return true;
        }
        drawStandaloneSpecialGlyph(
            text, position, scaledSize,
            style.outlineColor, outlineExpansion);
        break;
    case SdfTextLayer::Fill:
        drawStandaloneSpecialGlyph(
            text, position, scaledSize, style.fillColor);
        break;
    case SdfTextLayer::Combined:
        if (style.outlineColor.a != 0U && outlineExpansion > 0.0F) {
            drawStandaloneSpecialGlyph(
                text, position, scaledSize,
                style.outlineColor, outlineExpansion);
        }
        drawStandaloneSpecialGlyph(
            text, position, scaledSize, style.fillColor);
        break;
    }
    return true;
}

bool drawStyledStandaloneSpecialGlyph(
    std::string_view text, Vector2 position, float scaledSize,
    const SdfUiTextStyle& style) {
    if (!isStandaloneInfinity(text) && !isStandaloneCheck(text)) {
        return false;
    }
    drawStyledStandaloneSpecialGlyphLayer(
        text, position, scaledSize, style, SdfTextLayer::Shadow);
    drawStyledStandaloneSpecialGlyphLayer(
        text, position, scaledSize, style, SdfTextLayer::Outline);
    drawStyledStandaloneSpecialGlyphLayer(
        text, position, scaledSize, style, SdfTextLayer::Fill);
    return true;
}

} // namespace

void beginUiTextPerformanceFrame() noexcept {
    textPerformanceStats = {};
    ++sdfTextCacheFrame;
}

UiTextPerformanceStats uiTextPerformanceStats() noexcept {
    return textPerformanceStats;
}

void beginSdfUiTextBatch() {
    if (sdfTextBatchActive) return;
    sdfTextBatch.clear();
    sdfTextBatchActive = true;
}

void endSdfUiTextBatch() {
    if (!sdfTextBatchActive) return;
    sdfTextBatchActive = false;
    if (sdfTextBatch.empty()) return;
    const auto start = PerformanceClock::now();
    drawSdfTextBatch();
    textPerformanceStats.milliseconds += performanceMilliseconds(start);
    sdfTextBatch.clear();
}

void initializeUiText() {
    if (initialized) {
        return;
    }
    constexpr std::size_t ExpectedHudTextItems = 128U;
    sdfTextBatch.reserve(ExpectedHudTextItems);
    sdfTextPointerScratch.reserve(ExpectedHudTextItems);
    sdfTextCachedDrawScratch.reserve(ExpectedHudTextItems);
    std::array<int, GlyphCount> codepoints{};
    int index = 0;
    for (int codepoint = AsciiFirst; codepoint <= AsciiLast;
         ++codepoint) {
        codepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    for (int codepoint = CyrillicFirst;
         codepoint <= CyrillicLast; ++codepoint) {
        codepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    for (const int codepoint : ExtraUiCodepoints) {
        codepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    std::array<int, LatinGlyphCount> latinCodepoints{};
    index = 0;
    for (int codepoint = AsciiFirst; codepoint <= AsciiLast;
         ++codepoint) {
        latinCodepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    for (const int codepoint : ExtraUiCodepoints) {
        latinCodepoints[static_cast<std::size_t>(index++)] = codepoint;
    }
    primaryFont = LoadFontEx(
        "assets/ui/CIKORegular.ttf", 96,
        latinCodepoints.data(), LatinGlyphCount);
    cyrillicFallbackFont = LoadFontEx(
        "assets/ui/FredokaOneCyrillic-Regular.ttf", 64,
        codepoints.data(), GlyphCount);
    for (std::size_t face = 0; face < BodyFontPaths.size(); ++face) {
        bodyFonts[face] = LoadFontEx(BodyFontPaths[face], 64,
            codepoints.data(), GlyphCount);
        configureFontFiltering(bodyFonts[face]);
        bodySdfFonts[face] = loadSdfFont(BodyFontPaths[face],
            codepoints.data(), GlyphCount, SdfBaseGlyphSize);
        bodySmallSdfFonts[face] = loadSdfFont(BodyFontPaths[face],
            codepoints.data(), GlyphCount, SmallSdfBaseGlyphSize);
    }
    configureFontFiltering(primaryFont);
    configureFontFiltering(cyrillicFallbackFont);
    primarySdfFont = loadSdfFont(
        "assets/ui/CIKORegular.ttf",
        latinCodepoints.data(), LatinGlyphCount,
        SdfBaseGlyphSize);
    cyrillicFallbackSdfFont = loadSdfFont(
        "assets/ui/FredokaOneCyrillic-Regular.ttf",
        codepoints.data(), GlyphCount,
        SdfBaseGlyphSize);
    primarySmallSdfFont = loadSdfFont(
        "assets/ui/CIKORegular.ttf",
        latinCodepoints.data(), LatinGlyphCount,
        SmallSdfBaseGlyphSize);
    cyrillicFallbackSmallSdfFont = loadSdfFont(
        "assets/ui/FredokaOneCyrillic-Regular.ttf",
        codepoints.data(), GlyphCount,
        SmallSdfBaseGlyphSize);
    sdfTextShader = LoadShader(
        "assets/shaders/ui_text_sdf.vs", "assets/shaders/ui_text_sdf.fs");
    if (IsShaderValid(sdfTextShader)) {
        sdfFramebufferScaleLocation = GetShaderLocation(
            sdfTextShader, "framebufferScale");
    }
    initialized = IsFontValid(primaryFont) ||
        IsFontValid(cyrillicFallbackFont);
}

void shutdownUiText() {
    sdfTextBatch.clear();
    glyphLayoutCache.clear();
    for (auto& [unused, cached] : sdfTextTextureCache) {
        static_cast<void>(unused);
        unloadCachedSdfTextTexture(cached);
    }
    sdfTextTextureCache.clear();
    sdfTextObservations.clear();
    if (IsRenderTextureValid(hudTextBatchTarget)) {
        UnloadRenderTexture(hudTextBatchTarget);
    }
    hudTextBatchTarget = {};
    hudTextBatchCacheValid = false;
    hudTextBatchHash = 0U;
    hudTextBatchCandidateHash = 0U;
    hudTextBatchCandidateFrames = 0U;
    hudTextBatchRegions.clear();
    cachedSdfTextPixels = 0U;
    sdfTextCacheFrame = 0U;
    sdfTextBatchActive = false;
    if (isDrawableFont(primarySdfFont)) {
        UnloadFont(primarySdfFont);
    }
    if (isDrawableFont(cyrillicFallbackSdfFont)) {
        UnloadFont(cyrillicFallbackSdfFont);
    }
    if (isDrawableFont(primarySmallSdfFont)) {
        UnloadFont(primarySmallSdfFont);
    }
    if (isDrawableFont(cyrillicFallbackSmallSdfFont)) {
        UnloadFont(cyrillicFallbackSmallSdfFont);
    }
    if (IsShaderValid(sdfTextShader)) {
        UnloadShader(sdfTextShader);
    }
    if (IsFontValid(primaryFont)) UnloadFont(primaryFont);
    if (IsFontValid(cyrillicFallbackFont)) {
        UnloadFont(cyrillicFallbackFont);
    }
    for (std::size_t face = 0; face < BodyFontPaths.size(); ++face) {
        if (IsFontValid(bodyFonts[face])) UnloadFont(bodyFonts[face]);
        if (isDrawableFont(bodySdfFonts[face])) UnloadFont(bodySdfFonts[face]);
        if (isDrawableFont(bodySmallSdfFonts[face])) UnloadFont(bodySmallSdfFonts[face]);
    }
    bodyFonts = {};
    bodySdfFonts = {};
    bodySmallSdfFonts = {};
    currentFontRole = UiFontRole::Display;
    primaryFont = {};
    cyrillicFallbackFont = {};
    primarySdfFont = {};
    cyrillicFallbackSdfFont = {};
    primarySmallSdfFont = {};
    cyrillicFallbackSmallSdfFont = {};
    sdfTextShader = {};
    sdfFramebufferScaleLocation = -1;
    initialized = false;
}

ScopedUiFontRole::ScopedUiFontRole(UiFontRole role) noexcept
    : previous_(currentFontRole) {
    currentFontRole = role;
}

ScopedUiFontRole::~ScopedUiFontRole() {
    currentFontRole = previous_;
}

Font uiFont() {
    if (currentFontRole != UiFontRole::Display) {
        const std::size_t index = currentFontRole == UiFontRole::BodyBold ? 1U : 0U;
        if (IsFontValid(bodyFonts[index])) return bodyFonts[index];
    }
    if (currentLanguage() == Language::English &&
        IsFontValid(primaryFont)) {
        return primaryFont;
    }
    if (IsFontValid(cyrillicFallbackFont)) {
        return cyrillicFallbackFont;
    }
    return IsFontValid(primaryFont) ? primaryFont : GetFontDefault();
}

Vector2 measureUiText(std::string_view text, float fontSize) {
    const float scaledSize = fontSize * UiTextScale;
    if (isStandaloneInfinity(text) || isStandaloneCheck(text)) {
        return {specialGlyphAdvance(scaledSize), scaledSize};
    }
    if (currentLanguage() == Language::English) {
        return withNullTerminatedUiText(
            text, [scaledSize](const char* value) {
                return MeasureTextEx(
                    uiFont(), value, scaledSize,
                    scaledSize * 0.02F);
            });
    }
    const std::string localized = localizeText(text);
    return withNullTerminatedUiText(
        localized, [scaledSize](const char* value) {
            return MeasureTextEx(
                uiFont(), value, scaledSize,
                scaledSize * 0.02F);
        });
}

float fitUiTextSize(
    std::string_view text, float preferredSize,
    float minimumSize, float maximumWidth,
    float maximumHeight) {
    float size = std::max(preferredSize, minimumSize);
    const float safeWidth = std::max(maximumWidth, 1.0F);
    const float safeHeight = std::max(maximumHeight, 1.0F);
    while (size > minimumSize) {
        const Vector2 measured = measureUiText(text, size);
        if (measured.x <= safeWidth && measured.y <= safeHeight) {
            break;
        }
        size = std::max(minimumSize, size - 0.5F);
    }
    return size;
}

void drawUiText(std::string_view text, Vector2 position,
                float fontSize, Color color) {
    const ScopedTextPerformanceSample performanceSample(text.size());
    const float scaledSize = fontSize * UiTextScale;
    const bool body = currentFontRole != UiFontRole::Display;
    const Vector2 shadowOffset = body ? Vector2{0.0F, 1.0F} : Vector2{1.5F, 2.0F};
    const Color shadow{
        7, 9, 11,
        static_cast<unsigned char>(
            static_cast<unsigned int>(color.a) * (body ? 2U : 3U) / 4U),
    };
    const auto draw = [position, scaledSize, shadow, shadowOffset, color](
                          const char* value) {
        if (drawStandaloneSpecialGlyph(
                value, {position.x + shadowOffset.x, position.y + shadowOffset.y},
                scaledSize, shadow)) {
            drawStandaloneSpecialGlyph(value, position, scaledSize, color);
            return;
        }
        DrawTextEx(
            uiFont(), value,
            {position.x + shadowOffset.x, position.y + shadowOffset.y},
            scaledSize, scaledSize * 0.02F, shadow);
        DrawTextEx(
            uiFont(), value, position, scaledSize,
            scaledSize * 0.02F, color);
    };
    if (currentLanguage() == Language::English) {
        withNullTerminatedUiText(text, draw);
    } else {
        const std::string localized = localizeText(text);
        withNullTerminatedUiText(localized, draw);
    }
}

void drawRotatedStyledUiText(
    std::string_view text, Vector2 center,
    float fontSize, float rotationDegrees,
    const SdfUiTextStyle& style) {
    const ScopedTextPerformanceSample performanceSample(text.size());
    const float scaledSize = fontSize * UiTextScale;
    if (sdfTextBatchActive) {
        const Vector2 measured = measureUiText(text, fontSize);
        const Vector2 position{
            center.x - measured.x * 0.5F,
            center.y - measured.y * 0.5F,
        };
        if (queueSdfText(
                text, position, scaledSize, style,
                rotationDegrees * DEG2RAD, center, false)) {
            return;
        }
    }
    const auto draw = [center, scaledSize, rotationDegrees, style](
                          const char* value) {
        const Font font = uiFont();
        const float spacing = scaledSize * 0.02F;
        const Vector2 measured = MeasureTextEx(
            font, value, scaledSize, spacing);
        const Vector2 origin{
            measured.x * 0.5F, measured.y * 0.5F};
        if (isSdfTextReady(scaledSize)) {
            const QueuedSdfText queued{
                .text = value,
                .position = {
                    center.x - origin.x,
                    center.y - origin.y,
                },
                .scaledSize = scaledSize,
                .style = style,
                .fontRole = currentFontRole,
                .rotationRadians = rotationDegrees * DEG2RAD,
                .rotationCenter = center,
            };
            const QueuedSdfText* pointer = &queued;
            drawSdfTextItemsDirect(
                std::span<const QueuedSdfText* const>{&pointer, 1U});
            return;
        }
        if (style.shadowColor.a > 0U) {
            DrawTextPro(
                font, value,
                {center.x + style.shadowOffsetPixels.x,
                 center.y + style.shadowOffsetPixels.y},
                origin, rotationDegrees,
                scaledSize, spacing, style.shadowColor);
        }
        if (style.outlineColor.a > 0U &&
            style.outlinePixels > 0.0F) {
            constexpr std::array<Vector2, 8> Directions{{
                {-1.0F, 0.0F}, {1.0F, 0.0F},
                {0.0F, -1.0F}, {0.0F, 1.0F},
                {-0.7071F, -0.7071F},
                {0.7071F, -0.7071F},
                {-0.7071F, 0.7071F},
                {0.7071F, 0.7071F},
            }};
            for (const Vector2 direction : Directions) {
                DrawTextPro(
                    font, value,
                    {center.x + direction.x * style.outlinePixels,
                     center.y + direction.y * style.outlinePixels},
                    origin, rotationDegrees,
                    scaledSize, spacing, style.outlineColor);
            }
        }
        DrawTextPro(
            font, value, center, origin,
            rotationDegrees, scaledSize, spacing,
            style.fillColor);
    };
    if (currentLanguage() == Language::English) {
        withNullTerminatedUiText(text, draw);
    } else {
        const std::string localized = localizeText(text);
        withNullTerminatedUiText(localized, draw);
    }
}

void drawStyledSdfUiText(std::string_view text, Vector2 position,
                         float fontSize,
                         const SdfUiTextStyle& style) {
    const float scaledSize = fontSize * UiTextScale;
    if (queueSdfText(text, position, scaledSize, style)) {
        ++textPerformanceStats.submissions;
        textPerformanceStats.characters += text.size();
        return;
    }
    const auto draw = [position, scaledSize, style](const char* value) {
        if (drawStyledStandaloneSpecialGlyph(
                value, position, scaledSize, style)) {
            return;
        }
        drawSdfTextValue(value, position, scaledSize, style);
    };
    if (currentLanguage() == Language::English) {
        withNullTerminatedUiText(text, draw);
    } else {
        const std::string localized = localizeText(text);
        withNullTerminatedUiText(localized, draw);
    }
}

void drawStyledSdfUiTextRuns(
    std::span<const SdfUiTextRun> runs,
    const SdfUiTextStyle& style) {
    const bool batchable = sdfTextBatchActive && std::all_of(
        runs.begin(), runs.end(), [](const SdfUiTextRun& run) {
            return isSdfTextReady(run.fontSize * UiTextScale) &&
                !isStandaloneInfinity(run.text) &&
                !isStandaloneCheck(run.text);
        });
    if (batchable) {
        std::size_t characterCount = 0U;
        const std::size_t batchStart = sdfTextBatch.size();
        bool queuedAll = true;
        for (const SdfUiTextRun& run : runs) {
            SdfUiTextStyle runStyle = style;
            runStyle.fillColor =
                run.fillColor.value_or(style.fillColor);
            queuedAll = queueSdfText(
                run.text, run.position,
                run.fontSize * UiTextScale, runStyle) && queuedAll;
            characterCount += run.text.size();
        }
        if (queuedAll) {
            ++textPerformanceStats.submissions;
            textPerformanceStats.characters += characterCount;
            return;
        }
        sdfTextBatch.resize(batchStart);
    }
    for (const SdfUiTextRun& run : runs) {
        SdfUiTextStyle runStyle = style;
        runStyle.fillColor = run.fillColor.value_or(style.fillColor);
        drawStyledSdfUiText(
            run.text, run.position, run.fontSize, runStyle);
    }
}

void drawSdfUiText(std::string_view text, Vector2 position,
                   float fontSize, Color fillColor) {
    SdfUiTextStyle style{};
    style.fillColor = fillColor;
    style.outlineColor = DefaultSdfOutlineColor;
    style.outlinePixels = defaultOutlinePixels(fontSize);
    drawStyledSdfUiText(text, position, fontSize, style);
}

void drawCenteredUiText(std::string_view text, float y,
                        float fontSize, Color color) {
    const Vector2 size = measureUiText(text, fontSize);
    drawUiText(
        text,
        {(static_cast<float>(GetScreenWidth()) - size.x) * 0.5F,
         y},
        fontSize, color);
}

void drawCenteredStyledSdfUiText(
    std::string_view text, float y, float fontSize,
    const SdfUiTextStyle& style) {
    const Vector2 size = measureUiText(text, fontSize);
    drawStyledSdfUiText(
        text,
        {(static_cast<float>(GetScreenWidth()) - size.x) * 0.5F,
         y},
        fontSize, style);
}

void drawCenteredSdfUiText(std::string_view text, float y,
                           float fontSize, Color fillColor) {
    SdfUiTextStyle style{};
    style.fillColor = fillColor;
    style.outlineColor = DefaultSdfOutlineColor;
    style.outlinePixels = defaultOutlinePixels(fontSize);
    drawCenteredStyledSdfUiText(text, y, fontSize, style);
}

} // namespace ian
