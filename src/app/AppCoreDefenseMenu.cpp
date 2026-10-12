#include "app/App.hpp"

#include "buildings/CoreProgression.hpp"
#include "buildings/BuildingSystem.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace ian {
namespace {

// UiText accepts logical sizes and applies this project-wide multiplier
// internally. Figma measurements are already physical pixels.
constexpr float UiTextScale = 1.68F;

constexpr Color BodyColor{255, 228, 192, 255};
constexpr Color InsetColor{255, 236, 207, 255};
constexpr Color UnlockInsetColor{247, 219, 186, 255};
constexpr Color InsetWarmColor{255, 237, 210, 255};
constexpr Color InsetBorderColor{239, 207, 167, 255};
constexpr Color BrownColor{108, 61, 39, 255};
constexpr Color MutedTextColor{213, 197, 179, 255};
constexpr Color GreenTextColor{0, 241, 123, 255};
constexpr Color WarningTextColor{244, 102, 84, 255};

struct MenuTransform {
    float scale{1.0F};
    float offsetX{};
    float offsetY{};

    [[nodiscard]] Rectangle rect(
        float x, float y, float width, float height) const {
        return {
            offsetX + x * scale,
            offsetY + y * scale,
            width * scale,
            height * scale,
        };
    }

    [[nodiscard]] Vector2 point(float x, float y) const {
        return {offsetX + x * scale, offsetY + y * scale};
    }
};

Color withOpacity(Color color, float opacity) {
    color.a = static_cast<unsigned char>(std::lround(
        static_cast<float>(color.a) *
        std::clamp(opacity, 0.0F, 1.0F)));
    return color;
}

float smooth01(float value) {
    const float t = std::clamp(value, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

float easeOutBack(float value) {
    const float t = std::clamp(value, 0.0F, 1.0F) - 1.0F;
    constexpr float Overshoot = 1.35F;
    return 1.0F + (Overshoot + 1.0F) * t * t * t +
           Overshoot * t * t;
}

float intervalProgress(float seconds, float start, float end) {
    return smooth01((seconds - start) / std::max(end - start, 0.001F));
}

MenuTransform menuTransform(Vector2 coreScreen,
                            float pixelsPerWorldUnit,
                            float animatedScale,
                            float slideY) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    constexpr float PanelLeft = 407.0F;
    constexpr float PanelTop = 284.0F;
    constexpr float PanelWidth = 920.0F;
    constexpr float PanelHeight = 630.0F;
    constexpr float PanelScaleWidth = 1100.0F;
    constexpr float PanelScaleWorldWidth = 3.12F;
    constexpr float CoreHalfWidth = 1.0F;
    constexpr float WorldGap = 0.015F;
    constexpr float Margin = 18.0F;
    const float worldScale =
        pixelsPerWorldUnit * PanelScaleWorldWidth /
        PanelScaleWidth;
    const float attachmentOffset =
        pixelsPerWorldUnit * (CoreHalfWidth + WorldGap);
    const float rightX = coreScreen.x + attachmentOffset;
    const float leftEdge = coreScreen.x - attachmentOffset;
    const float rightSpace =
        std::max(0.0F, screenWidth - Margin - rightX);
    const float leftSpace =
        std::max(0.0F, leftEdge - Margin);
    const float desiredWidth = PanelWidth * worldScale;
    const bool rightFits =
        desiredWidth <= rightSpace;
    const bool leftFits = desiredWidth <= leftSpace;
    const bool placeRight = rightFits ||
        (!leftFits && rightSpace >= leftSpace);
    const float sideSpace =
        placeRight ? rightSpace : leftSpace;
    const float usableScreenWidth = std::max(
        0.0F, screenWidth - 2.0F * Margin);
    const float minimumWidth = std::min(
        desiredWidth, usableScreenWidth * 0.35F);
    const float screenScale = std::min(
        std::min(usableScreenWidth,
                 std::max(sideSpace, minimumWidth)) /
            PanelWidth,
        std::max(0.0F, screenHeight - 2.0F * Margin) /
            PanelHeight);
    const float settledScale = std::max(
        0.0F, std::min(worldScale, screenScale));
    const float settledWidth = PanelWidth * settledScale;
    const float panelX = std::clamp(
        placeRight ? rightX : leftEdge - settledWidth,
        Margin,
        std::max(Margin, screenWidth - Margin - settledWidth));
    const float scale = std::min(
        settledScale * std::max(0.0F, animatedScale),
        std::min(screenScale,
                 (screenWidth - Margin - panelX) / PanelWidth));
    const float panelHeight = PanelHeight * scale;
    const float panelY = std::clamp(
        coreScreen.y - panelHeight * 0.5F +
            pixelsPerWorldUnit * 0.18F + slideY * scale,
        Margin,
        std::max(Margin, screenHeight - Margin - panelHeight));
    return {
        .scale = scale,
        .offsetX = panelX - PanelLeft * scale,
        .offsetY = panelY - PanelTop * scale,
    };
}

void drawTexture(Texture2D texture, Rectangle bounds,
                 Color tint = WHITE) {
    if (!IsTextureValid(texture)) return;
    DrawTexturePro(
        texture,
        {0.0F, 0.0F, static_cast<float>(texture.width),
         static_cast<float>(texture.height)},
        bounds, {}, 0.0F, tint);
}

void drawRoundedPanel(Rectangle bounds, float border,
                      float roundness, Color fill,
                      Color borderColor, float opacity = 1.0F) {
    DrawRectangleRounded(
        bounds, roundness, 20, withOpacity(borderColor, opacity));
    const Rectangle inner{
        bounds.x + border,
        bounds.y + border,
        std::max(0.0F, bounds.width - border * 2.0F),
        std::max(0.0F, bounds.height - border * 2.0F),
    };
    DrawRectangleRounded(
        inner, roundness, 20, withOpacity(fill, opacity));
}

void drawCoreCardFrame(const MenuTransform& transform,
                       float opacity) {
    const float scale = transform.scale;
    // The small dark rim and the warm body form one continuous card.
    drawRoundedPanel(
        transform.rect(407.0F, 284.0F, 920.0F, 630.0F),
        4.0F * scale, 0.09F,
        BodyColor, {50, 34, 24, 255}, opacity);

    const Rectangle header = transform.rect(
        411.0F, 288.0F, 912.0F, 93.0F);
    DrawRectangleRounded(
        {header.x + 1.0F * scale,
         header.y + 8.0F * scale,
         header.width - 2.0F * scale,
         header.height},
        0.50F, 32,
        withOpacity({179, 91, 24, 95}, opacity));
    DrawRectangleRounded(
        {header.x, header.y + 4.0F * scale,
         header.width, header.height},
        0.50F, 32,
        withOpacity({219, 125, 30, 165}, opacity));
    DrawRectangleRounded(
        header, 0.50F, 32,
        withOpacity({250, 158, 33, 255}, opacity));
    const float cornerHeight = 23.0F * scale;
    DrawRectangleRounded(
        {header.x, header.y, header.width,
         cornerHeight * 2.0F},
        1.0F, 32,
        withOpacity({255, 190, 62, 255}, opacity));
    DrawRectangleGradientV(
        static_cast<int>(std::round(header.x)),
        static_cast<int>(std::round(header.y + cornerHeight)),
        static_cast<int>(std::round(header.width)),
        static_cast<int>(std::round(header.height -
                                    2.0F * cornerHeight)),
        withOpacity({255, 188, 59, 255}, opacity),
        withOpacity({250, 158, 33, 255}, opacity));
    DrawRectangleRoundedLinesEx(
        {header.x + 2.0F * scale,
         header.y + 2.0F * scale,
         header.width - 4.0F * scale,
         header.height - 5.0F * scale},
        0.50F, 32, 1.5F * scale,
        withOpacity({255, 222, 132, 110}, opacity));
}

void drawCenteredMenuText(std::string_view value,
                          const MenuTransform& transform,
                          Rectangle designBounds, float size,
                          Color color, float opacity) {
    const Rectangle bounds = transform.rect(
        designBounds.x, designBounds.y,
        designBounds.width, designBounds.height);
    const float fontSize =
        size * transform.scale / UiTextScale;
    const Vector2 measured = measureUiText(value, fontSize);
    drawSdfUiText(
        value,
        {bounds.x + (bounds.width - measured.x) * 0.5F,
         bounds.y + (bounds.height - measured.y) * 0.5F},
        fontSize, withOpacity(color, opacity));
}

void drawRowText(std::string_view value,
                 const MenuTransform& transform,
                 float x, float rowTop, float rowHeight,
                 float size, Color color, float opacity) {
    const float fontSize =
        size * transform.scale / UiTextScale;
    const Vector2 measured = measureUiText(value, fontSize);
    const Vector2 topLeft = transform.point(x, rowTop);
    drawSdfUiText(
        value,
        {topLeft.x,
         topLeft.y +
             (rowHeight * transform.scale - measured.y) * 0.5F},
        fontSize, withOpacity(color, opacity));
}

void drawArrow(const MenuTransform& transform, float centerX,
               float centerY, float opacity) {
    const float scale = transform.scale;
    const Vector2 center = transform.point(centerX, centerY);
    const Color outline = withOpacity({83, 47, 22, 255}, opacity);
    const Color fill = withOpacity({255, 190, 28, 255}, opacity);
    // Two convex primitives avoid the self-intersection artifacts produced
    // by a triangle fan over the arrow's concave silhouette.
    DrawRectangleRounded(
        {center.x - 13.0F * scale, center.y - 6.0F * scale,
         15.0F * scale, 12.0F * scale},
        0.35F, 8, outline);
    DrawTriangle(
        {center.x + 14.0F * scale, center.y},
        {center.x - 1.0F * scale, center.y - 12.0F * scale},
        {center.x - 1.0F * scale, center.y + 12.0F * scale},
        outline);
    DrawRectangleRounded(
        {center.x - 10.0F * scale, center.y - 3.0F * scale,
         13.0F * scale, 6.0F * scale},
        0.35F, 8, fill);
    DrawTriangle(
        {center.x + 9.0F * scale, center.y},
        {center.x + 1.0F * scale, center.y - 7.0F * scale},
        {center.x + 1.0F * scale, center.y + 7.0F * scale},
        fill);
}

struct StatRow {
    CoreMenuTexture icon;
    const char* label;
    int current;
    int next;
};

void drawStatRow(const GameUi& ui, const MenuTransform& transform,
                 const StatRow& row, float y, bool hasNextLevel,
                 float reveal, float numberProgress) {
    MenuTransform contentTransform = transform;
    contentTransform.offsetY +=
        (1.0F - reveal) * 10.0F * transform.scale;
    drawTexture(ui.coreMenuTexture(row.icon),
                contentTransform.rect(
                    447.0F, y - 2.0F, 37.0F, 37.0F),
                withOpacity(WHITE, reveal));
    const float rowTop = y - 2.0F;
    constexpr float RowHeight = 37.0F;
    drawRowText(row.label, contentTransform,
                490.0F, rowTop, RowHeight, 34.0F,
                MutedTextColor, reveal);
    drawRowText(std::to_string(row.current), contentTransform,
                712.0F, rowTop, RowHeight, 34.0F,
                UiWhite, reveal);
    if (!hasNextLevel) {
        drawRowText("MAX", contentTransform,
                    825.0F, rowTop, RowHeight, 34.0F,
                    GreenTextColor, reveal);
        return;
    }
    drawArrow(contentTransform, 795.0F, y + 16.5F, reveal);
    const int animatedNext = static_cast<int>(std::lround(
        static_cast<float>(row.current) +
        static_cast<float>(row.next - row.current) * numberProgress));
    const float numberScale =
        0.88F + 0.12F * easeOutBack(numberProgress);
    drawRowText(std::to_string(animatedNext), contentTransform,
                827.0F, rowTop, RowHeight,
                40.0F * numberScale, GreenTextColor, reveal);
}

bool drawActionButton(
    const GameUi& ui, const MenuTransform& transform,
    Rectangle designBounds, CoreMenuTexture actionIcon,
    std::string_view label, CoreMenuTexture costIcon, int cost,
    bool enabled, Color darkFill, Color lightFill,
    float opacity, bool pointerActive) {
    Rectangle bounds = transform.rect(
        designBounds.x, designBounds.y,
        designBounds.width, designBounds.height);
    const bool hovered = pointerActive && enabled &&
        opacity >= 0.98F &&
        CheckCollisionPointRec(GetMousePosition(), bounds);
    const bool pressed = hovered &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const float scale = transform.scale;
    const float pressOffset = pressed ? 3.0F * scale : 0.0F;
    bounds.y += pressOffset;

    DrawRectangleRounded(bounds, 0.28F, 20,
                         withOpacity(
                             enabled ? Color{43, 40, 39, 255}
                                     : Color{65, 62, 58, 255},
                             opacity));
    Rectangle inner{
        bounds.x + 3.0F * scale,
        bounds.y + 3.0F * scale,
        bounds.width - 6.0F * scale,
        bounds.height - 6.0F * scale,
    };
    DrawRectangleRounded(inner, 0.26F, 20,
                         withOpacity(
                             enabled ? darkFill
                                     : Color{110, 103, 91, 255},
                             opacity));
    Rectangle highlight = inner;
    highlight.height = std::max(
        0.0F, highlight.height - 10.0F * scale);
    DrawRectangleRounded(highlight, 0.26F, 20,
                         withOpacity(
                             enabled ? lightFill
                                     : Color{142, 134, 118, 255},
                             opacity));
    if (hovered) {
        DrawRectangleRounded(inner, 0.26F, 20,
                             withOpacity(
                                 {255, 255, 255, 24}, opacity));
    }

    const Color tint = withOpacity(
        enabled ? WHITE : Color{190, 184, 170, 235}, opacity);
    drawTexture(ui.coreMenuTexture(actionIcon),
                {bounds.x + 17.0F * scale,
                 bounds.y + 18.0F * scale,
                 67.0F * scale, 67.0F * scale}, tint);
    drawSdfUiText(label,
                  {bounds.x + 91.0F * scale,
                   bounds.y + 34.0F * scale},
                  38.0F * scale / UiTextScale,
                  withOpacity(
                      enabled ? WHITE : Color{220, 214, 199, 255},
                      opacity));
    const std::string costText = std::to_string(cost);
    const float costFontSize = 39.0F * scale / UiTextScale;
    const float costWidth = measureUiText(costText, costFontSize).x;
    const float costRight = bounds.x + bounds.width - 18.0F * scale;
    const float costX = costRight - costWidth;
    drawTexture(ui.coreMenuTexture(costIcon),
                {costX - 48.0F * scale,
                 bounds.y + 31.0F * scale,
                 43.0F * scale, 43.0F * scale}, tint);
    drawSdfUiText(costText,
                  {costX, bounds.y + 34.0F * scale},
                  costFontSize,
                  withOpacity(
                      enabled ? WHITE : WarningTextColor, opacity));

    return hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

struct UnlockCard {
    Texture2D icon;
    std::string_view label;
};

std::vector<UnlockCard> unlockCardsForLevel(
    const GameUi& ui, std::uint8_t level) {
    std::vector<UnlockCard> cards;
    if (level > 1U && level <= MaxBuildingLevel) {
        cards.push_back({ui.coreMenuTexture(CoreMenuTexture::Gear), "MORE RANGE"});
        cards.push_back({ui.coreMenuTexture(CoreMenuTexture::Ballista), "MORE DEFENSES"});
        cards.push_back({ui.coreMenuTexture(CoreMenuTexture::Gem), "MORE STORAGE"});
    }
    return cards;
}

Rectangle scaledAroundCenter(Rectangle bounds, float scale) {
    const float width = bounds.width * scale;
    const float height = bounds.height * scale;
    return {
        bounds.x + (bounds.width - width) * 0.5F,
        bounds.y + (bounds.height - height) * 0.5F,
        width, height,
    };
}

void drawUnlockCard(const MenuTransform& transform,
                    float x, float y, float width, float height,
                    const UnlockCard& unlock, float reveal) {
    const float popScale = 0.84F + 0.16F * easeOutBack(reveal);
    const Rectangle card = scaledAroundCenter(
        transform.rect(x, y, width, height), popScale);
    drawRoundedPanel(card, 3.0F * transform.scale, 0.24F,
                     InsetWarmColor, InsetBorderColor, reveal);
    const bool compact = height < 120.0F;
    const float iconSize = compact
        ? std::min(card.height - 12.0F * transform.scale,
                   58.0F * transform.scale)
        : 120.0F * transform.scale * popScale;
    drawTexture(
        unlock.icon,
        {card.x + (compact ? 6.0F : 15.0F) * transform.scale,
         card.y + (compact ? 6.0F : 8.0F) * transform.scale,
         iconSize, iconSize},
        withOpacity(WHITE, reveal));
    const Rectangle pill = compact
        ? Rectangle{
              card.x + 63.0F * transform.scale,
              card.y + 21.0F * transform.scale,
              std::max(0.0F,
                       card.width - 69.0F * transform.scale),
              34.0F * transform.scale}
        : Rectangle{
              card.x + 12.0F * transform.scale,
              card.y + 127.0F * transform.scale * popScale,
              126.0F * transform.scale * popScale,
              29.0F * transform.scale * popScale};
    DrawRectangleRounded(
        pill, 1.0F, 16, withOpacity(BrownColor, reveal));
    const float size = fitUiTextSize(
        unlock.label,
        (compact ? 18.0F : 20.0F) * transform.scale /
            UiTextScale,
        11.0F * transform.scale / UiTextScale,
        pill.width - 12.0F * transform.scale,
        pill.height - 4.0F * transform.scale);
    const Vector2 measured = measureUiText(unlock.label, size);
    drawSdfUiText(unlock.label,
                  {pill.x + (pill.width - measured.x) * 0.5F,
                   pill.y + (pill.height - measured.y) * 0.5F},
                  size, withOpacity(UiWhite, reveal));
}

} // namespace

void App::drawCoreDefenseMenu() {
    if (!coreDefenseMenuVisible_) return;
    const SimulationSnapshot& snapshot = simulation_.snapshot();
    if (!snapshot.coreId) {
        coreDefenseMenuVisible_ = false;
        coreDefenseMenuClosing_ = false;
        coreDefenseMenuEntranceSeconds_ = 0.0;
        if (coreMenuPointerActive_) {
            coreMenuPointerActive_ = false;
            DisableCursor();
        }
        return;
    }

    const auto coreIt = std::find_if(
        snapshot.buildings.begin(), snapshot.buildings.end(),
        [&snapshot](const BuildingInstance& building) {
            return building.id == *snapshot.coreId;
        });
    if (coreIt == snapshot.buildings.end()) {
        coreDefenseMenuVisible_ = false;
        coreDefenseMenuClosing_ = false;
        coreDefenseMenuEntranceSeconds_ = 0.0;
        if (coreMenuPointerActive_) {
            coreMenuPointerActive_ = false;
            DisableCursor();
        }
        return;
    }
    const BuildingInstance& core = *coreIt;
    if (coreDefenseMenuAnimatedLevel_ != core.level) {
        coreDefenseMenuAnimatedLevel_ = core.level;
    }
    constexpr double AnimationSeconds = 0.40;
    const double frameSeconds = static_cast<double>(
        std::clamp(GetFrameTime(), 0.0F, 0.10F));
    if (coreDefenseMenuClosing_) {
        coreDefenseMenuEntranceSeconds_ = std::max(
            0.0, coreDefenseMenuEntranceSeconds_ - frameSeconds);
        if (coreDefenseMenuEntranceSeconds_ <= 0.0) {
            coreDefenseMenuVisible_ = false;
            coreDefenseMenuClosing_ = false;
            coreDefenseMenuAnimatedLevel_ = 0;
            if (coreMenuPointerActive_ &&
                snapshot.state != RunState::Paused) {
                DisableCursor();
            }
            coreMenuPointerActive_ = false;
            return;
        }
    } else {
        coreDefenseMenuEntranceSeconds_ = std::min(
            coreDefenseMenuEntranceSeconds_ + frameSeconds,
            AnimationSeconds);
    }
    const float entranceSeconds = static_cast<float>(
        coreDefenseMenuEntranceSeconds_);
    const float panelProgress = intervalProgress(
        entranceSeconds, 0.0F, 0.28F);
    const float panelScale = easeOutBack(panelProgress);
    const float panelOpacity = intervalProgress(
        entranceSeconds, 0.0F, 0.12F);
    const float titleReveal = intervalProgress(
        entranceSeconds, 0.02F, 0.14F);
    const float bodyReveal = intervalProgress(
        entranceSeconds, 0.04F, 0.18F);
    const float buttonReveal = intervalProgress(
        entranceSeconds, 0.11F, 0.27F);
    const Vec3 corePosition = buildingWorldPosition(core);
    const Vector3 coreAnchor{
        static_cast<float>(corePosition.x),
        static_cast<float>(corePosition.y + 0.95),
        static_cast<float>(corePosition.z),
    };
    const Vector2 coreScreen = GetWorldToScreen(
        coreAnchor, gameplayCamera_);
    const Vector3 cameraRight = Vector3Normalize(
        Vector3CrossProduct(
            Vector3Subtract(
                gameplayCamera_.target,
                gameplayCamera_.position),
            gameplayCamera_.up));
    const Vector2 unitScreen = GetWorldToScreen(
        Vector3Add(coreAnchor, cameraRight),
        gameplayCamera_);
    const float pixelsPerWorldUnit = std::max(
        1.0F, Vector2Distance(coreScreen, unitScreen));
    const MenuTransform transform = menuTransform(
        coreScreen, pixelsPerWorldUnit, panelScale,
        (1.0F - panelProgress) * 18.0F);
    const bool hasNextLevel = core.level < MaxBuildingLevel;
    const std::uint8_t nextLevel = hasNextLevel
        ? static_cast<std::uint8_t>(core.level + 1)
        : core.level;

    if (panelScale <= 0.001F) {
        return;
    }

    if (coreMenuPointerActive_ &&
        !coreDefenseMenuClosing_ &&
        entranceSeconds > 0.15F) {
        const Rectangle panel = transform.rect(
            407.0F, 284.0F, 920.0F, 630.0F);
        const float coreReach = pixelsPerWorldUnit * 1.1F;
        const float left = std::min(
            panel.x, coreScreen.x - coreReach);
        const float top = std::min(
            panel.y, coreScreen.y - coreReach);
        const float right = std::max(
            panel.x + panel.width,
            coreScreen.x + coreReach);
        const float bottom = std::max(
            panel.y + panel.height,
            coreScreen.y + coreReach);
        constexpr float PointerMargin = 20.0F;
        const Rectangle interactionArea{
            left - PointerMargin,
            top - PointerMargin,
            right - left + 2.0F * PointerMargin,
            bottom - top + 2.0F * PointerMargin};
        if (CheckCollisionPointRec(
                GetMousePosition(), interactionArea)) {
            coreMenuPointerAwaySeconds_ = 0.0;
        } else {
            coreMenuPointerAwaySeconds_ += frameSeconds;
            if (coreMenuPointerAwaySeconds_ > 0.30) {
                coreDefenseMenuClosing_ = true;
            }
        }
    }

    drawCoreCardFrame(transform, panelOpacity);
    const auto drawHeaderLabel =
        [&transform, titleReveal](
            std::string_view label, float x, float width,
            float designSize) {
            const Rectangle bounds = transform.rect(
                x, 288.0F, width, 93.0F);
            const float fontSize =
                designSize * transform.scale / UiTextScale;
            const Vector2 measured = measureUiText(
                label, fontSize);
            drawSdfUiText(
                label,
                {bounds.x + (bounds.width - measured.x) * 0.5F,
                 bounds.y + (bounds.height - measured.y) * 0.5F},
                fontSize,
                withOpacity(
                    {254, 247, 229, 255}, titleReveal));
        };
    drawHeaderLabel("CORE", 425.0F, 190.0F, 53.0F);
    const Rectangle levelPill = transform.rect(
        620.0F, 309.0F, 202.0F, 50.0F);
    DrawRectangleRounded(
        levelPill, 1.0F, 20,
        withOpacity(BrownColor, titleReveal));
    drawCenteredMenuText(
        "LEVEL " + std::to_string(core.level),
        transform, {620.0F, 309.0F, 202.0F, 50.0F},
        36.0F, {254, 247, 229, 255}, titleReveal);
    drawRoundedPanel(transform.rect(432.0F, 397.0F,
                                    506.0F, 367.0F),
                     3.0F * transform.scale, 0.12F,
                     InsetColor, InsetBorderColor, bodyReveal);
    drawRoundedPanel(transform.rect(952.0F, 397.0F,
                                    350.0F, 367.0F),
                     3.0F * transform.scale, 0.12F,
                     UnlockInsetColor, InsetBorderColor, bodyReveal);

    const double healthStep = hasNextLevel
        ? core.maxHealth *
              ((1.0 + 0.15 * static_cast<double>(core.level)) /
               (1.0 + 0.15 * static_cast<double>(core.level - 1)))
        : core.maxHealth;
    const std::array<StatRow, 7> rows{
        StatRow{CoreMenuTexture::Heart, "HEALTH",
                static_cast<int>(std::lround(core.maxHealth)),
                static_cast<int>(std::lround(healthStep))},
        StatRow{CoreMenuTexture::Gear, "RANGE",
                snapshot.coreBuildRadius,
                snapshot.coreBuildRadius + (hasNextLevel ? CoreBuildRadiusPerLevel : 0)},
        StatRow{CoreMenuTexture::Shield, "DEFENSES",
                buildingLimitForCoreLevel(BuildingType::Turret,
                                          core.level),
                buildingLimitForCoreLevel(BuildingType::Turret,
                                          nextLevel)},
        StatRow{CoreMenuTexture::Gear, "PRODUCERS",
                buildingLimitForCoreLevel(BuildingType::LumberMill,
                                          core.level),
                buildingLimitForCoreLevel(BuildingType::LumberMill,
                                          nextLevel)},
        StatRow{CoreMenuTexture::Wood, "WOOD",
                coreResourceCapacity(ResourceCapacityType::Wood,
                                     core.level),
                coreResourceCapacity(ResourceCapacityType::Wood,
                                     nextLevel)},
        StatRow{CoreMenuTexture::Rock, "STONE",
                coreResourceCapacity(ResourceCapacityType::Stone,
                                     core.level),
                coreResourceCapacity(ResourceCapacityType::Stone,
                                     nextLevel)},
        StatRow{CoreMenuTexture::Gem, "CRYSTAL",
                coreResourceCapacity(ResourceCapacityType::Crystal,
                                     core.level),
                coreResourceCapacity(ResourceCapacityType::Crystal,
                                     nextLevel)},
    };
    constexpr std::array RowY{
        409.0F, 458.0F, 507.0F, 556.0F, 605.0F, 654.0F, 703.0F};
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const float rowReveal = intervalProgress(
            entranceSeconds,
            0.07F + static_cast<float>(index) * 0.015F,
            0.20F + static_cast<float>(index) * 0.015F);
        const float numberProgress = intervalProgress(
            entranceSeconds,
            0.10F + static_cast<float>(index) * 0.018F,
            0.30F + static_cast<float>(index) * 0.018F);
        drawStatRow(ui_, transform, rows[index], RowY[index],
                    hasNextLevel, rowReveal, numberProgress);
    }

    const Rectangle unlockPill = transform.rect(
        965.0F, 386.0F, 324.0F, 42.0F);
    DrawRectangleRounded(
        unlockPill, 1.0F, 20,
        withOpacity(BrownColor, bodyReveal));
    drawCenteredMenuText(
        hasNextLevel
            ? "LVL " + std::to_string(nextLevel) +
                  " EXPANDS"
            : "MAX LEVEL",
        transform, {965.0F, 386.0F, 324.0F, 42.0F},
        32.0F, {254, 247, 229, 255}, bodyReveal);

    if (hasNextLevel) {
        const std::vector<UnlockCard> unlocks =
            unlockCardsForLevel(ui_, nextLevel);
        const bool compact = unlocks.size() > 4U;
        const float cardHeight = compact ? 75.0F : 150.0F;
        const float rowStep = compact ? 80.0F : 160.0F;
        for (std::size_t index = 0; index < unlocks.size(); ++index) {
            const bool centeredLast =
                index + 1U == unlocks.size() &&
                unlocks.size() % 2U != 0U;
            const float x = centeredLast
                ? 1049.0F
                : (index % 2U == 0U ? 965.0F : 1132.0F);
            const float y = 438.0F +
                static_cast<float>(index / 2U) * rowStep;
            const float reveal = intervalProgress(
                entranceSeconds,
                0.10F + static_cast<float>(index) * 0.018F,
                0.24F + static_cast<float>(index) * 0.018F);
            drawUnlockCard(
                transform, x, y, 150.0F, cardHeight,
                unlocks[index], reveal);
        }
    }

    const ResourceCost upgradeCost = buildingUpgradeCost(core);
    const int coreUpgradeCost = upgradeCost.crystals;
    const bool canUpgrade = hasNextLevel &&
        (snapshot.unlimitedResources ||
         snapshot.crystals >= coreUpgradeCost);
    const bool upgradeClicked = drawActionButton(
            ui_, transform,
            {432.0F, 780.0F, 870.0F, 102.0F},
            CoreMenuTexture::Upgrade, "UPGRADE",
            CoreMenuTexture::Gem, coreUpgradeCost,
            canUpgrade, Color{16, 163, 70, 255},
            Color{64, 206, 90, 255}, buttonReveal,
            coreMenuPointerActive_);
    if (!coreDefenseMenuClosing_ && upgradeClicked) {
        pendingBuildingUpgrade_ = UpgradeBuildingCommand{core.id};
        audio_.playUiConfirm();
    }
}

} // namespace ian
