#include "ui/UiViewport.hpp"
#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"

#include "buildings/CoreProgression.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/BuildingSystem.hpp"
#include "ui/GameUi.hpp"
#include "ui/BuildingCardLayout.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"
#include "ui/UiRoundedRect.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <ranges>
#include <span>
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

using MenuTransform = BuildingCardTransform;

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
    drawSolidRoundedRect(
        bounds, roundness, withOpacity(borderColor, opacity));
    const Rectangle inner{
        bounds.x + border,
        bounds.y + border,
        std::max(0.0F, bounds.width - border * 2.0F),
        std::max(0.0F, bounds.height - border * 2.0F),
    };
    drawSolidRoundedRect(
        inner, roundness, withOpacity(fill, opacity));
}

void drawCoreCardFrame(const MenuTransform& transform,
                       float opacity, float panelWidth,
                       float panelHeight) {
    const float scale = transform.scale;
    // The small dark rim and the warm body form one continuous card.
    drawRoundedPanel(
        transform.rect(407.0F, 284.0F, panelWidth, panelHeight),
        4.0F * scale, 0.09F,
        BodyColor, {50, 34, 24, 255}, opacity);

    const Rectangle header = transform.rect(
        411.0F, 288.0F, panelWidth - 8.0F, 93.0F);
    drawSolidRoundedRect(
        {header.x + 1.0F * scale,
         header.y + 8.0F * scale,
         header.width - 2.0F * scale,
         header.height},
        0.50F,
        withOpacity({179, 91, 24, 95}, opacity));
    drawSolidRoundedRect(
        {header.x, header.y + 4.0F * scale,
         header.width, header.height},
        0.50F,
        withOpacity({219, 125, 30, 165}, opacity));
    drawSolidRoundedRect(
        header, 0.50F,
        withOpacity({250, 158, 33, 255}, opacity));
    const float cornerHeight = 23.0F * scale;
    drawSolidRoundedRect(
        {header.x, header.y, header.width,
         cornerHeight * 2.0F},
        1.0F,
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

void drawFittedRowText(std::string_view value,
                       const MenuTransform& transform,
                       float x, float rowTop, float width,
                       float rowHeight, float designSize,
                       Color color, float opacity) {
    const Rectangle bounds = transform.rect(
        x, rowTop, width, rowHeight);
    const float fontSize = fitUiTextSize(
        value, designSize * transform.scale / UiTextScale,
        20.0F * transform.scale / UiTextScale,
        bounds.width, bounds.height);
    const Vector2 measured = measureUiText(value, fontSize);
    drawSdfUiText(
        value,
        {bounds.x,
         bounds.y + (bounds.height - measured.y) * 0.5F},
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
    drawSolidRoundedRect(
        {center.x - 13.0F * scale, center.y - 6.0F * scale,
         15.0F * scale, 12.0F * scale},
        0.35F, outline);
    DrawTriangle(
        {center.x + 14.0F * scale, center.y},
        {center.x - 1.0F * scale, center.y - 12.0F * scale},
        {center.x - 1.0F * scale, center.y + 12.0F * scale},
        outline);
    drawSolidRoundedRect(
        {center.x - 10.0F * scale, center.y - 3.0F * scale,
         13.0F * scale, 6.0F * scale},
        0.35F, fill);
    DrawTriangle(
        {center.x + 9.0F * scale, center.y},
        {center.x + 1.0F * scale, center.y - 7.0F * scale},
        {center.x + 1.0F * scale, center.y + 7.0F * scale},
        fill);
}

struct StatRow {
    std::optional<CoreMenuTexture> icon;
    std::string_view label;
    double current;
    double next;
    int decimals{};
    std::string_view suffix{};
    bool compare{true};
};

std::string formatStatValue(double value, const StatRow& row) {
    char buffer[48]{};
    const std::string suffix(row.suffix);
    std::snprintf(
        buffer, sizeof(buffer),
        row.decimals == 0 ? "%.0f%s" :
        row.decimals == 1 ? "%.1f%s" : "%.2f%s",
        value, suffix.c_str());
    return buffer;
}

void drawStatRow(const GameUi& ui, const MenuTransform& transform,
                 const StatRow& row, float y, bool hasNextLevel,
                 float reveal, float numberProgress,
                 bool isCore) {
    MenuTransform contentTransform = transform;
    contentTransform.offsetY +=
        (1.0F - reveal) * 10.0F * transform.scale;
    if (row.icon) {
        drawTexture(ui.coreMenuTexture(*row.icon),
                    contentTransform.rect(
                        447.0F, y - 2.0F, 37.0F, 37.0F),
                    withOpacity(WHITE, reveal));
    }
    const float rowTop = y - 2.0F;
    constexpr float RowHeight = 37.0F;
    drawRowText(row.label, contentTransform,
                isCore ? 490.0F : 447.0F,
                rowTop, RowHeight,
                isCore ? 34.0F : 29.0F,
                MutedTextColor, reveal);
    const std::string currentText = formatStatValue(row.current, row);
    if (isCore) {
        drawRowText(currentText, contentTransform,
                    712.0F, rowTop, RowHeight,
                    34.0F, UiWhite, reveal);
    } else {
        drawFittedRowText(currentText, contentTransform,
                          676.0F, rowTop, 108.0F, RowHeight,
                          29.0F, UiWhite, reveal);
    }
    if (!row.compare) return;
    if (!hasNextLevel) {
        drawRowText("MAX", contentTransform,
                    825.0F, rowTop, RowHeight, 34.0F,
                    GreenTextColor, reveal);
        return;
    }
    drawArrow(contentTransform, 795.0F, y + 16.5F, reveal);
    const double animatedNext = row.current +
        (row.next - row.current) * numberProgress;
    const float numberScale =
        0.88F + 0.12F * easeOutBack(numberProgress);
    const std::string nextText = formatStatValue(animatedNext, row);
    if (isCore) {
        drawRowText(nextText, contentTransform,
                    827.0F, rowTop, RowHeight,
                    40.0F * numberScale,
                    GreenTextColor, reveal);
    } else {
        drawFittedRowText(nextText, contentTransform,
                          827.0F, rowTop, 105.0F, RowHeight,
                          32.0F * numberScale,
                          GreenTextColor, reveal);
    }
}

struct ActionCost {
    CoreMenuTexture icon;
    int amount;
};

bool drawActionButton(
    const GameUi& ui, const MenuTransform& transform,
    Rectangle designBounds, CoreMenuTexture actionIcon,
    std::string_view label, std::span<const ActionCost> costs,
    bool enabled, Color darkFill, Color lightFill,
    float opacity, bool pointerActive) {
    Rectangle bounds = transform.rect(
        designBounds.x, designBounds.y,
        designBounds.width, designBounds.height);
    const bool hovered = pointerActive && enabled &&
        opacity >= 0.98F &&
        CheckCollisionPointRec(uiMousePosition(), bounds);
    const bool pressed = hovered &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const float scale = transform.scale;
    const float pressOffset = pressed ? 3.0F * scale : 0.0F;
    bounds.y += pressOffset;

    drawSolidRoundedRect(bounds, 0.28F,
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
    drawSolidRoundedRect(inner, 0.26F,
                         withOpacity(
                             enabled ? darkFill
                                     : Color{110, 103, 91, 255},
                             opacity));
    Rectangle highlight = inner;
    highlight.height = std::max(
        0.0F, highlight.height - 10.0F * scale);
    drawSolidRoundedRect(highlight, 0.26F,
                         withOpacity(
                             enabled ? lightFill
                                     : Color{142, 134, 118, 255},
                             opacity));
    if (hovered) {
        drawSolidRoundedRect(inner, 0.26F,
                             withOpacity(
                                 {255, 255, 255, 24}, opacity));
    }

    const Color tint = withOpacity(
        enabled ? WHITE : Color{190, 184, 170, 235}, opacity);
    const bool compactCosts = costs.size() > 1U;
    drawTexture(ui.coreMenuTexture(actionIcon),
                {bounds.x + 17.0F * scale,
                 bounds.y + (compactCosts ? 23.0F : 18.0F) * scale,
                 (compactCosts ? 56.0F : 67.0F) * scale,
                 (compactCosts ? 56.0F : 67.0F) * scale}, tint);
    drawSdfUiText(label,
                  {bounds.x + (compactCosts ? 79.0F : 91.0F) * scale,
                   bounds.y + 34.0F * scale},
                  (compactCosts ? 31.0F : 38.0F) * scale /
                      UiTextScale,
                  withOpacity(
                      enabled ? WHITE : Color{220, 214, 199, 255},
                      opacity));
    float costRight = bounds.x + bounds.width - 18.0F * scale;
    const float costFontSize =
        (compactCosts ? 29.0F : 39.0F) * scale / UiTextScale;
    const float iconSize =
        (compactCosts ? 32.0F : 43.0F) * scale;
    for (std::size_t index = costs.size(); index > 0U; --index) {
        const ActionCost& cost = costs[index - 1U];
        const std::string costText = std::to_string(cost.amount);
        const float textWidth =
            measureUiText(costText, costFontSize).x;
        const float textX = costRight - textWidth;
        drawSdfUiText(
            costText, {textX, bounds.y + 34.0F * scale},
            costFontSize,
            withOpacity(
                enabled ? WHITE : WarningTextColor, opacity));
        const float iconX = textX - iconSize - 5.0F * scale;
        drawTexture(
            ui.coreMenuTexture(cost.icon),
            {iconX,
             bounds.y + (compactCosts ? 36.0F : 31.0F) * scale,
             iconSize, iconSize}, tint);
        costRight = iconX - 10.0F * scale;
    }

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
    drawSolidRoundedRect(
        pill, 1.0F, withOpacity(BrownColor, reveal));
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
    const ScopedUiCanvas canvas;
    if (!coreDefenseMenuVisible_) return;
    const SimulationSnapshot& snapshot = simulation_.snapshot();
    const auto closeMenu = [this, &snapshot]() {
        coreDefenseMenuVisible_ = false;
        coreDefenseMenuClosing_ = false;
        coreDefenseMenuTarget_.reset();
        coreDefenseMenuEntranceSeconds_ = 0.0;
        coreDefenseMenuAnimatedLevel_ = 0;
        coreMenuPanelBounds_.reset();
        coreMenuLostAimSeconds_ = 0.0;
        coreMenuPointerAwaySeconds_ = 0.0;
        if (coreMenuPointerActive_ && app_detail::acceptsGameplayInput(snapshot.state) &&
            !renderer_->graphicsPanelVisible() && !enemySpawnMenuVisible_ &&
            !itemGrantMenuVisible_ && !sandboxCardMenuVisible_) DisableCursor();
        coreMenuPointerActive_ = false;
    };
    if (!coreDefenseMenuTarget_) {
        closeMenu();
        return;
    }

    const auto targetIt = std::find_if(
        snapshot.buildings.begin(), snapshot.buildings.end(),
        [this](const BuildingInstance& building) {
            return building.id == *coreDefenseMenuTarget_;
        });
    if (targetIt == snapshot.buildings.end()) {
        closeMenu();
        return;
    }
    const BuildingInstance& target = *targetIt;
    const bool isCore = target.type == BuildingType::Core;
    const bool isProducer =
        target.type == BuildingType::CrystalMine ||
        target.type == BuildingType::Quarry ||
        target.type == BuildingType::LumberMill;
    const auto defense = std::ranges::find(
        snapshot.defenseCards, target.id,
        &DefenseCardStatus::buildingId);
    const auto producer = std::ranges::find(
        snapshot.producerCards, target.id,
        &ProducerCardStatus::buildingId);
    if ((!isCore && !isProducer && defense == snapshot.defenseCards.end()) ||
        (isProducer && producer == snapshot.producerCards.end())) {
        closeMenu();
        return;
    }
    const Vec3 corePosition = buildingWorldPosition(target);
    const Vector3 base{static_cast<float>(corePosition.x), static_cast<float>(corePosition.y),
                       static_cast<float>(corePosition.z)};
    const Vector3 observer{static_cast<float>(snapshot.playerPosition.x),
        static_cast<float>(snapshot.playerPosition.y), static_cast<float>(snapshot.playerPosition.z)};
    const Vector2 viewport{static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())};
    const auto projection = projectBuildingCard(
        Vector3Add(base, {0.0F, isCore ? 0.95F : 1.15F, 0.0F}), gameplayCamera_, viewport);
    // Recheck against this frame's camera even when the menu owns the cursor.
    // Never project or clamp a lost target into a persistent screen overlay.
    if (!buildingCardInRange(observer, base) || !projection) {
        closeMenu();
        return;
    }
    const std::uint8_t level = target.level;
    if (coreDefenseMenuAnimatedLevel_ != level) {
        coreDefenseMenuAnimatedLevel_ = level;
    }
    constexpr double AnimationSeconds = 0.40;
    const double frameSeconds = static_cast<double>(
        std::clamp(GetFrameTime(), 0.0F, 0.10F));
    if (coreDefenseMenuClosing_) {
        coreDefenseMenuEntranceSeconds_ = std::max(
            0.0, coreDefenseMenuEntranceSeconds_ - frameSeconds);
        if (coreDefenseMenuEntranceSeconds_ <= 0.0) {
            closeMenu();
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
    const Vector2 coreScreen = projection->anchor;
    const float pixelsPerWorldUnit = projection->pixelsPerWorldUnit;
    const bool directUpgrade = snapshot.sandboxMode || snapshot.unlimitedResources;
    const float panelWidth = isCore && directUpgrade ? 920.0F : 560.0F;
    const float panelHeight = directUpgrade
        ? (isProducer ? 736.0F : 630.0F) : 500.0F;
    const float buildingHalfWidth = static_cast<float>(
        buildingFootprintHalfExtent(target.type));
    const MenuTransform transform = buildingCardTransform(
        *projection, viewport, panelScale,
        (1.0F - panelProgress) * 18.0F,
        panelWidth, panelHeight, buildingHalfWidth, isCore);
    const Rectangle panelBounds = transform.rect(
        407.0F, 284.0F, panelWidth, panelHeight);
    coreMenuPanelBounds_ = canvas.viewport().toScreen(panelBounds);
    const bool hasNextLevel = level < buildingLevelLimit(target.type);
    const std::uint8_t nextLevel = hasNextLevel
        ? static_cast<std::uint8_t>(level + 1)
        : level;

    if (panelScale <= 0.001F) {
        return;
    }

    if (coreMenuPointerActive_ &&
        !coreDefenseMenuClosing_ &&
        entranceSeconds > 0.15F) {
        const Rectangle panel = panelBounds;
        const float coreReach = std::min(pixelsPerWorldUnit *
            (buildingHalfWidth + 0.1F), std::min(viewport.x, viewport.y) * 0.15F);
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
                uiMousePosition(), interactionArea)) {
            coreMenuPointerAwaySeconds_ = 0.0;
        } else {
            coreMenuPointerAwaySeconds_ += frameSeconds;
            if (coreMenuPointerAwaySeconds_ > 0.30) {
                coreDefenseMenuClosing_ = true;
            }
        }
    }

    drawCoreCardFrame(transform, panelOpacity,
                      panelWidth, panelHeight);
    const auto drawHeaderLabel =
        [&transform, titleReveal](
            std::string_view label, float x, float width,
            float designSize) {
            const Rectangle bounds = transform.rect(
                x, 288.0F, width, 93.0F);
            const float fontSize = fitUiTextSize(
                label, designSize * transform.scale / UiTextScale,
                27.0F * transform.scale / UiTextScale,
                bounds.width, bounds.height);
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
    std::string title(buildingDisplayName(target.type));
    std::transform(title.begin(), title.end(), title.begin(),
                   [](unsigned char letter) {
                       return static_cast<char>(std::toupper(letter));
                   });
    drawHeaderLabel(
        title, 425.0F, isCore ? 190.0F : 305.0F,
        isCore ? 53.0F : 45.0F);
    const Rectangle levelPill = transform.rect(
        isCore ? 620.0F : 740.0F,
        309.0F, 202.0F, 50.0F);
    drawSolidRoundedRect(
        levelPill, 1.0F,
        withOpacity(BrownColor, titleReveal));
    drawCenteredMenuText(
        "LEVEL " + std::to_string(level),
        transform,
        {isCore ? 620.0F : 740.0F,
         309.0F, 202.0F, 50.0F},
        36.0F, {254, 247, 229, 255}, titleReveal);
    drawRoundedPanel(transform.rect(432.0F, 397.0F,
                                    506.0F, 367.0F),
                     3.0F * transform.scale, 0.12F,
                     InsetColor, InsetBorderColor, bodyReveal);
    if (isCore && directUpgrade) {
        drawRoundedPanel(transform.rect(952.0F, 397.0F,
                                        350.0F, 367.0F),
                         3.0F * transform.scale, 0.12F,
                         UnlockInsetColor, InsetBorderColor, bodyReveal);
    }

    std::vector<StatRow> rows;
    rows.reserve(7);
    if (isCore) {
        rows = {
            {CoreMenuTexture::Heart, "HEALTH",
             target.maxHealth, target.maxHealth},
            {CoreMenuTexture::Gear, "RANGE",
             static_cast<double>(snapshot.coreBuildRadius),
             static_cast<double>(snapshot.coreBuildRadius), 0, "M"},
            {CoreMenuTexture::Wood, "WOOD",
             static_cast<double>(snapshot.woodCapacity),
             static_cast<double>(snapshot.woodCapacity)},
            {CoreMenuTexture::Rock, "STONE",
             static_cast<double>(snapshot.stoneCapacity),
             static_cast<double>(snapshot.stoneCapacity)},
            {CoreMenuTexture::Gem, "CRYSTAL",
             static_cast<double>(snapshot.crystalCapacity),
             static_cast<double>(snapshot.crystalCapacity)},
        };
    } else {
        const BuildingStatComparison& comparison = isProducer
            ? producer->stats : defense->stats;
        const BuildingStats& current = comparison.current;
        const BuildingStats* next = directUpgrade && comparison.next
            ? &*comparison.next : nullptr;
        rows.push_back({
            std::nullopt, "HEALTH", current.maxHealth,
            next ? next->maxHealth : current.maxHealth});
        if (isProducer) {
            rows.push_back({
                std::nullopt, "OUTPUT",
                current.productionPerCycle.value_or(0.0),
                next ? next->productionPerCycle.value_or(0.0)
                     : current.productionPerCycle.value_or(0.0)});
            rows.push_back({
                std::nullopt, "CYCLE",
                current.productionInterval.value_or(0.0),
                next ? next->productionInterval.value_or(0.0)
                     : current.productionInterval.value_or(0.0),
                2, "S"});
            const auto rate = [](const BuildingStats& stats) {
                return stats.productionInterval.value_or(0.0) > 0.0
                    ? 60.0 * stats.productionPerCycle.value_or(0.0) /
                          *stats.productionInterval
                    : 0.0;
            };
            rows.push_back({
                std::nullopt, "RATE", rate(current),
                next ? rate(*next) : rate(current), 1, "/MIN"});
        } else {
        const auto addOptional = [&rows, &current, next](
            std::string_view label,
            std::optional<double> BuildingStats::* member,
            int decimals, std::string_view suffix) {
            const auto value = current.*member;
            if (!value) return;
            const auto nextValue = next ? next->*member : std::nullopt;
            rows.push_back({
                std::nullopt, label, *value,
                nextValue.value_or(*value), decimals, suffix});
        };
        addOptional("REPAIR RATE", &BuildingStats::repairPerSecond, 1, "/S");
        addOptional("REPAIR INTERVAL", &BuildingStats::repairInterval, 1, "S");
        addOptional("TARGETS", &BuildingStats::attractionTargets, 0, "");
        addOptional("DURATION", &BuildingStats::effectDuration, 1, "S");
        addOptional("COOLDOWN", &BuildingStats::cooldown, 1, "S");
        if (target.type == BuildingType::RepairStation || target.type == BuildingType::DecoyBeacon)
            addOptional("RADIUS", &BuildingStats::effectRadius, 1, "M");
        addOptional("PUSH", &BuildingStats::pushStrength, 1, "M/S");
        addOptional("DAMAGE", &BuildingStats::attackDamage, 0, "");
        addOptional("RANGE", &BuildingStats::attackRange, 1, "M");
        addOptional("ARC", &BuildingStats::attackArcDegrees, 0, " DEG");
        addOptional("ATTACK RATE", &BuildingStats::attacksPerSecond,
                    2, "/S");
        if (target.type == BuildingType::Turret) {
            addOptional("PIERCING", &BuildingStats::piercingCount, 0, "");
        } else if (target.type == BuildingType::Cannon ||
                   target.type == BuildingType::Catapult || target.type == BuildingType::Mortar) {
            addOptional("BLAST RADIUS", &BuildingStats::effectRadius,
                        1, "M");
        }
        }
    }
    constexpr std::array RowY{
        412.0F, 470.0F, 528.0F, 586.0F, 644.0F, 702.0F};
    constexpr std::array CoreRowY{
        409.0F, 458.0F, 507.0F, 556.0F, 605.0F, 654.0F, 703.0F};
    if (!directUpgrade) {
        for (auto& row : rows) row.compare = false;
    }
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const float rowReveal = intervalProgress(
            entranceSeconds,
            0.07F + static_cast<float>(index) * 0.015F,
            0.20F + static_cast<float>(index) * 0.015F);
        const float numberProgress = intervalProgress(
            entranceSeconds,
            0.10F + static_cast<float>(index) * 0.018F,
            0.30F + static_cast<float>(index) * 0.018F);
        drawStatRow(ui_, transform, rows[index],
                    isCore ? CoreRowY[index] : RowY[index],
                    directUpgrade && !isCore && hasNextLevel,
                    rowReveal, numberProgress,
                    isCore);
    }

    if (isCore && directUpgrade) {
        const Rectangle unlockPill = transform.rect(
            965.0F, 386.0F, 324.0F, 42.0F);
        drawSolidRoundedRect(
            unlockPill, 1.0F,
            withOpacity(BrownColor, bodyReveal));
        drawCenteredMenuText(
            hasNextLevel ? "CORE CARD UPGRADES" : "MAX LEVEL",
            transform, {965.0F, 386.0F, 324.0F, 42.0F},
            32.0F, {254, 247, 229, 255}, bodyReveal);
    }

    if (isCore && hasNextLevel && directUpgrade) {
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

    if (isCore && directUpgrade) {
        const bool canUpgrade = hasNextLevel && directUpgrade;
        const bool upgradeClicked = drawActionButton(
            ui_, transform,
            {432.0F, 780.0F, 870.0F, 102.0F},
            CoreMenuTexture::Upgrade, "UPGRADE",
            {}, canUpgrade,
            Color{16, 163, 70, 255},
            Color{64, 206, 90, 255}, buttonReveal,
            coreMenuPointerActive_);
        if (!coreDefenseMenuClosing_ && upgradeClicked) {
            pendingBuildingUpgrade_ =
                UpgradeBuildingCommand{target.id};
            audio_.playUiConfirm();
        }
    } else if (isProducer && directUpgrade) {
        const ResourceCost cost = producer->upgradeCost;
        std::array<ActionCost, 3> costs{};
        std::size_t costCount = 0;
        if (cost.wood > 0) {
            costs[costCount++] = {CoreMenuTexture::Wood, cost.wood};
        }
        if (cost.stone > 0) {
            costs[costCount++] = {CoreMenuTexture::Rock, cost.stone};
        }
        if (cost.crystals > 0) {
            costs[costCount++] = {CoreMenuTexture::Gem, cost.crystals};
        }
        const bool canUpgrade = hasNextLevel &&
            producer->upgradeError == UpgradeError::None;
        const bool upgradeClicked = drawActionButton(
            ui_, transform,
            {432.0F, 780.0F, 506.0F, 102.0F},
            CoreMenuTexture::Upgrade, "UPGRADE",
            std::span<const ActionCost>(costs.data(), costCount),
            canUpgrade, Color{16, 163, 70, 255},
            Color{64, 206, 90, 255}, buttonReveal,
            coreMenuPointerActive_);
        if (!coreDefenseMenuClosing_ && upgradeClicked) {
            pendingBuildingUpgrade_ = UpgradeBuildingCommand{target.id};
            audio_.playUiConfirm();
        }
    } else if (directUpgrade) {
        const ResourceCost cost = defense->upgradeCost;
        std::array<ActionCost, 3> costs{};
        std::size_t costCount = 0;
        if (cost.wood > 0) {
            costs[costCount++] = {CoreMenuTexture::Wood, cost.wood};
        }
        if (cost.stone > 0) {
            costs[costCount++] = {CoreMenuTexture::Rock, cost.stone};
        }
        if (cost.crystals > 0) {
            costs[costCount++] = {CoreMenuTexture::Gem, cost.crystals};
        }
        const bool canUpgrade = hasNextLevel &&
            defense->upgradeError == UpgradeError::None;
        const bool upgradeClicked = drawActionButton(
            ui_, transform,
            {432.0F, 780.0F, 506.0F, 102.0F},
            CoreMenuTexture::Upgrade, "UPGRADE",
            std::span<const ActionCost>(costs.data(), costCount),
            canUpgrade, Color{16, 163, 70, 255},
            Color{64, 206, 90, 255}, buttonReveal,
            coreMenuPointerActive_);
        if (!coreDefenseMenuClosing_ && upgradeClicked) {
            pendingBuildingUpgrade_ = UpgradeBuildingCommand{target.id};
            audio_.playUiConfirm();
        }
    }
}

} // namespace ian
