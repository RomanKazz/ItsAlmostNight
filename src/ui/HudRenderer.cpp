#include "ui/UiViewport.hpp"
#include "ui/UiRoundedRect.hpp"
#include "ui/UiPromptKeycap.hpp"
#include "ui/HudRenderer.hpp"

#include "app/UserSettings.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/CoreProgression.hpp"
#include "core/PerformanceStats.hpp"
#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudBuildingContextCard.hpp"
#include "ui/HudFormatting.hpp"
#include "ui/HudHotbar.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"
#include "ui/RunHammerUi.hpp"
#include "ui/RunDefenseInventory.hpp"
#include "ui/NightForecastHud.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ian {
namespace {

HudRenderPerformanceStats hudPerformanceStats{};
std::array<Vector2, 4> cachedHudResourceIconCenters{};

const char* placementMessage(
    PlacementError error, BuildingType) {
    switch (error) {
    case PlacementError::None:
        return "";
    case PlacementError::CoreAlreadyPlaced:
        return "Core already placed";
    case PlacementError::CoreRequired:
        return "Place Core first";
    case PlacementError::InsufficientResources:
        return "";
    case PlacementError::Occupied:
        return "Space occupied";
    case PlacementError::OutsideCoreArea:
        return "Outside Core area";
    case PlacementError::PlayerOverlap:
        return "Player blocks placement";
    case PlacementError::WorldCollision:
        return "Terrain blocks placement";
    case PlacementError::LimitReached:
        return "Building limit reached";
    case PlacementError::OutOfRange:
        return "Placement is too far";
    case PlacementError::BlueprintRequired:
        return "Choose this building on level-up";
    case PlacementError::CoreLevelRequired:
        return "Upgrade Core to unlock";
    case PlacementError::ResourceBlocked:
        return "Clear tree or stone first";
    }
    return "";
}

Color placementMessageColor(PlacementError error) {
    switch (error) {
    case PlacementError::None:
        return {92, 225, 117, 255};
    case PlacementError::InsufficientResources:
        return {255, 176, 62, 255};
    case PlacementError::OutsideCoreArea:
        return {187, 104, 255, 255};
    case PlacementError::OutOfRange:
        return {94, 172, 255, 255};
    case PlacementError::PlayerOverlap:
        return {255, 222, 82, 255};
    case PlacementError::CoreRequired:
    case PlacementError::BlueprintRequired:
    case PlacementError::CoreLevelRequired:
    case PlacementError::CoreAlreadyPlaced:
    case PlacementError::LimitReached:
        return {255, 132, 71, 255};
    case PlacementError::Occupied:
    case PlacementError::WorldCollision:
    case PlacementError::ResourceBlocked:
        return {242, 91, 73, 255};
    }
    return {242, 91, 73, 255};
}

const char* attackDirectionName(AttackDirection direction) {
    switch (direction) {
    case AttackDirection::North:
        return "NORTH";
    case AttackDirection::East:
        return "EAST";
    case AttackDirection::South:
        return "SOUTH";
    case AttackDirection::West:
        return "WEST";
    }
    return "";
}

bool hasAttackDirections(const SimulationSnapshot& snapshot) {
    return std::ranges::any_of(
        snapshot.upcomingAttackDirections,
        [](bool active) { return active; });
}

std::string attackDirectionNames(const SimulationSnapshot& snapshot) {
    std::string names;
    for (std::size_t index = 0;
         index < snapshot.upcomingAttackDirections.size(); ++index) {
        if (!snapshot.upcomingAttackDirections[index]) {
            continue;
        }
        if (!names.empty()) {
            names += " + ";
        }
        names += attackDirectionName(static_cast<AttackDirection>(index));
    }
    return names;
}

std::string compactAmount(int amount) {
    if (amount < 1000) {
        return std::to_string(amount);
    }
    char text[24]{};
    if (amount < 10000) {
        std::snprintf(
            text, sizeof(text), "%.1fk",
            static_cast<double>(amount) / 1000.0);
    } else {
        std::snprintf(
            text, sizeof(text), "%dk", amount / 1000);
    }
    return text;
}

struct VitalBarPalette {
    Color outer;
    Color inner;
    Color fill;
};

constexpr VitalBarPalette PlayerHealthPalette{
    .outer = {216, 0, 56, 255},
    .inner = {255, 64, 86, 255},
    .fill = {255, 158, 134, 255},
};
constexpr VitalBarPalette CoreHealthPalette{
    .outer = {138, 11, 168, 255},
    .inner = {217, 27, 232, 255},
    .fill = {245, 139, 239, 255},
};

void drawVitalBar(GameUi& ui, Rectangle bounds, float fraction,
                  float trailFraction, float damagePulse,
                  const VitalBarPalette& palette,
                  std::string_view text, bool core) {
    constexpr float OuterRadius = 10.0F;
    constexpr float InnerInset = 5.0F;
    constexpr float InnerHeight = 26.0F;
    constexpr float InnerRadius = 6.0F;
    constexpr float FillHeight = 20.0F;
    constexpr float FillRadius = 6.0F;
    constexpr float TextFontSize = 24.0F / 1.68F;

    fraction = std::clamp(fraction, 0.0F, 1.0F);
    trailFraction = std::clamp(
        std::max(trailFraction, fraction), 0.0F, 1.0F);
    damagePulse = std::clamp(damagePulse, 0.0F, 1.0F);
    DrawRectangleRounded(
        bounds, OuterRadius * 2.0F / bounds.height,
        8, palette.outer);
    const Rectangle innerBounds{
        bounds.x + InnerInset,
        bounds.y + InnerInset,
        bounds.width - InnerInset * 2.0F,
        InnerHeight,
    };
    DrawRectangleRounded(
        innerBounds, InnerRadius * 2.0F / InnerHeight,
        8, palette.inner);
    if (trailFraction > fraction + 0.001F) {
        const Rectangle trailBounds{
            innerBounds.x,
            bounds.y + InnerInset,
            innerBounds.width * trailFraction,
            FillHeight,
        };
        DrawRectangleRounded(
            trailBounds,
            std::clamp(
                FillRadius * 2.0F /
                    std::max(1.0F, trailBounds.height),
                0.0F, 1.0F),
            8, {255, 221, 166, 255});
    }
    if (fraction > 0.001F) {
        const Rectangle fillBounds{
            innerBounds.x,
            bounds.y + InnerInset,
            innerBounds.width * fraction,
            FillHeight,
        };
        DrawRectangleRounded(
            fillBounds,
            std::clamp(
                FillRadius * 2.0F /
                    std::max(1.0F, fillBounds.height),
                0.0F, 1.0F),
            8, palette.fill);
    }

    const Vector2 textSize = measureUiText(text, TextFontSize);
    SdfUiTextStyle textStyle{};
    textStyle.fillColor = UiWhite;
    textStyle.outlineColor = {49, 49, 49, 255};
    textStyle.shadowColor = {82, 82, 82, 255};
    textStyle.shadowOffsetPixels = {0.0F, 2.0F};
    textStyle.outlinePixels = 2.0F;
    drawStyledSdfUiText(
        text,
        {bounds.x + (bounds.width - textSize.x) * 0.5F,
         bounds.y + (bounds.height - textSize.y) * 0.5F},
        TextFontSize, textStyle);

    if (damagePulse > 0.001F) {
        DrawRectangleRoundedLinesEx(
            bounds,
            OuterRadius * 2.0F / bounds.height,
            8, 2.0F + damagePulse * 1.5F,
            {255, 73, 80,
             static_cast<unsigned char>(
                 90.0F + damagePulse * 150.0F)});
    }
    const float iconSize =
        40.0F * (1.0F + damagePulse * 0.10F);
    const Rectangle iconBounds{
        bounds.x - 43.0F - (iconSize - 40.0F) * 0.5F,
        bounds.y - 2.0F - (iconSize - 40.0F) * 0.5F,
        iconSize, iconSize};
    ui.drawVitalIcon(iconBounds, core);
}

float dayNightCycleAngle(
    const SimulationSnapshot& snapshot) {
    // One continuous phase coordinate rotates the dial through day,
    // sunset, night and dawn.
    constexpr float DayArc = 2.70F;
    constexpr float SunsetArc = 0.55F;
    constexpr float NightArc = 2.70F;
    constexpr float DawnArc = 2.0F * PI -
        DayArc - SunsetArc - NightArc;
    const float progress = snapshot.phaseDuration > 0.0
        ? std::clamp(
              1.0F - static_cast<float>(
                  snapshot.phaseTimeRemaining /
                  snapshot.phaseDuration),
              0.0F, 1.0F)
        : 0.0F;
    if (snapshot.state == RunState::BuildPhase) {
        return DayArc * progress;
    }
    if (snapshot.state == RunState::Sunset) {
        return DayArc + SunsetArc * progress;
    }
    if (snapshot.state == RunState::Wave) {
        return DayArc + SunsetArc + NightArc * progress;
    }
    if (snapshot.state == RunState::WaveComplete) {
        return DayArc + SunsetArc + NightArc +
            DawnArc * progress;
    }
    return 0.0F;
}

void drawDayNightTimeline(
    Rectangle bounds, const SimulationSnapshot& snapshot) {
    constexpr Color ShadowColor{43, 38, 35, 125};
    constexpr Color FrameColor{49, 49, 49, 255};
    constexpr Color NightColor{126, 103, 205, 255};
    constexpr Color SunsetColor{232, 121, 62, 255};
    constexpr Color DayColor{224, 177, 55, 255};
    constexpr float DayArc = 2.70F;
    constexpr float SunsetArc = 0.55F;
    constexpr float NightArc = 2.70F;
    constexpr float Tau = 2.0F * PI;
    constexpr float ColorBlendArc = 0.16F;
    constexpr float DayEnd = DayArc / Tau;
    constexpr float SunsetEnd = (DayArc + SunsetArc) / Tau;
    constexpr float SunsetBlendEnd =
        (DayArc + ColorBlendArc) / Tau;
    constexpr float NightBlendEnd =
        (DayArc + SunsetArc + ColorBlendArc) / Tau;
    constexpr float NightEnd =
        (DayArc + SunsetArc + NightArc) / Tau;
    constexpr float DawnBlendStart =
        1.0F - ColorBlendArc / Tau;

    DrawRectangleRounded(
        {bounds.x + 1.0F, bounds.y + 3.0F,
         bounds.width, bounds.height},
        1.0F, 16, ShadowColor);

    const Rectangle inner{
        bounds.x, bounds.y,
        bounds.width, bounds.height};
    const float radius = inner.height * 0.5F;
    const float cyclePosition = std::fmod(
        dayNightCycleAngle(snapshot) / Tau + 1.0F, 1.0F);
    const auto wrappedPhase = [](float phase) {
        phase = std::fmod(phase, 1.0F);
        return phase < 0.0F ? phase + 1.0F : phase;
    };
    const auto phaseColor = [&](float phase) {
        phase = wrappedPhase(phase);
        if (phase < DayEnd) {
            return DayColor;
        }
        if (phase < SunsetBlendEnd) {
            const float blend = std::clamp(
                (phase - DayEnd) /
                    std::max(0.0001F,
                             SunsetBlendEnd - DayEnd),
                0.0F, 1.0F);
            return ColorLerp(DayColor, SunsetColor, blend);
        }
        if (phase < SunsetEnd) {
            return SunsetColor;
        }
        if (phase < NightBlendEnd) {
            const float blend = std::clamp(
                (phase - SunsetEnd) /
                    std::max(
                        0.0001F,
                        NightBlendEnd - SunsetEnd),
                0.0F, 1.0F);
            return ColorLerp(
                SunsetColor, NightColor, blend);
        }
        if (phase < NightEnd) {
            return NightColor;
        }
        if (phase < DawnBlendStart) {
            return NightColor;
        }
        const float blend = std::clamp(
            (phase - DawnBlendStart) /
                std::max(0.0001F,
                         1.0F - DawnBlendStart),
            0.0F, 1.0F);
        return ColorLerp(NightColor, DayColor, blend);
    };

    // The marker stays fixed. One complete day/night cycle scrolls beneath
    // it, so upcoming time is always on the right and elapsed time on the
    // left.
    // Draw one continuous strip clipped to the pill silhouette. Previously
    // the two rounded ends were separate circles whose whole color changed
    // when a phase boundary crossed their centers, producing a visible jump.
    constexpr int TimelineSegments = 220;
    for (int index = 0; index < TimelineSegments; ++index) {
        const float start = static_cast<float>(index) /
            static_cast<float>(TimelineSegments);
        const float end = static_cast<float>(index + 1) /
            static_cast<float>(TimelineSegments);
        const float horizontalCenter = (start + end) * 0.5F;
        const float phase =
            cyclePosition + horizontalCenter - 0.5F;
        const float columnX = inner.x + inner.width * start;
        const float columnCenterX =
            inner.x + inner.width * horizontalCenter;
        const float leftCapCenter = inner.x + radius;
        const float rightCapCenter =
            inner.x + inner.width - radius;
        float halfHeight = radius;
        if (columnCenterX < leftCapCenter) {
            const float delta =
                columnCenterX - leftCapCenter;
            halfHeight = std::sqrt(std::max(
                0.0F, radius * radius - delta * delta));
        } else if (columnCenterX > rightCapCenter) {
            const float delta =
                columnCenterX - rightCapCenter;
            halfHeight = std::sqrt(std::max(
                0.0F, radius * radius - delta * delta));
        }
        DrawRectangleRec(
            {columnX,
             inner.y + radius - halfHeight,
             std::min(
                 inner.x + inner.width - columnX,
                 inner.width * (end - start) + 1.0F),
             halfHeight * 2.0F},
            phaseColor(phase));
    }
    DrawRectangleRounded(
        {inner.x + 2.0F, inner.y + 2.0F,
         inner.width - 4.0F, 3.0F},
        1.0F, 4, {255, 255, 255, 35});
    DrawRectangleRoundedLinesEx(
        bounds, 1.0F, 16, 3.0F, FrameColor);

    const float markerX = bounds.x + bounds.width * 0.5F;
    const float markerHalfWidth = bounds.height * 0.25F;
    const float markerInset = bounds.height * 0.14F;
    constexpr float MarkerOverhang = 3.0F;
    DrawRectangle(
        static_cast<int>(markerX),
        static_cast<int>(bounds.y - MarkerOverhang),
        4,
        static_cast<int>(bounds.height + MarkerOverhang * 2.0F),
        {56, 49, 39, 150});
    DrawRectangle(
        static_cast<int>(markerX - 1.0F),
        static_cast<int>(bounds.y - MarkerOverhang),
        3,
        static_cast<int>(bounds.height + MarkerOverhang * 2.0F),
        {238, 232, 199, 255});
    DrawTriangle(
        {markerX - markerHalfWidth,
         bounds.y - MarkerOverhang},
        {markerX + markerHalfWidth,
         bounds.y - MarkerOverhang},
        {markerX, bounds.y + markerInset},
        {238, 232, 199, 255});
    DrawTriangle(
        {markerX - markerHalfWidth,
         bounds.y + bounds.height + MarkerOverhang},
        {markerX,
         bounds.y + bounds.height - markerInset},
        {markerX + markerHalfWidth,
         bounds.y + bounds.height + MarkerOverhang},
        {238, 232, 199, 255});
}

Color objectiveKindColor(ObjectiveKind kind) {
    switch (kind) {
    case ObjectiveKind::Milestone:
        return {184, 151, 255, 255};
    case ObjectiveKind::Challenge:
        return {255, 190, 73, 255};
    case ObjectiveKind::WorldEvent:
        return {79, 220, 207, 255};
    }
    return {235, 214, 170, 255};
}

std::string phaseClock(double seconds) {
    const int total = std::max(0, static_cast<int>(std::ceil(seconds)));
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", total / 60,
                  total % 60);
    return buffer;
}

void drawTutorialCard(
    const SimulationSnapshot& snapshot,
    const ControlSettings& controls, const HudViewState& view) {
    const auto visual = view.tutorialHint.value_or(TutorialHintVisual{
        .objective = snapshot.tutorialObjective, .opacity = 1.0F});
    if (!visual.objective || visual.opacity <= .001F) return;
    const float opacity = visual.opacity;
    const auto fade = [opacity](Color color) {
        color.a = static_cast<unsigned char>(std::lround(color.a * opacity));
        return color;
    };
    std::string key;
    std::string action;
    switch (*visual.objective) {
    case TutorialObjective::MineWood:
        key = "LMB";
        action = "CHOP A TREE";
        break;
    case TutorialObjective::OpenBuildMode:
        key = keyboardKeyName(
            controlKey(controls, ControlAction::BuildMode));
        action = "BUILD MODE";
        break;
    case TutorialObjective::PlaceCore:
        key = "LMB";
        action = "PLACE CORE";
        break;
    case TutorialObjective::PrepareForNight:
        key = "LMB";
        action = "BUILD DEFENSE";
        break;
    case TutorialObjective::SurviveFirstWave:
        key = "!";
        action = "SURVIVE NIGHT";
        break;
    }

    const ScopedUiFontRole displayFont(UiFontRole::Display);
    constexpr float ActionFontSize = 22.0F;
    const float keyWidth = std::max(62.0F, measureUiText(key, 19.0F).x + 22.0F);
    constexpr float CheckSlotWidth = 40.0F;
    const float contentWidth = 14.0F + CheckSlotWidth + keyWidth + 15.0F +
        measureUiText(action, ActionFontSize).x + 18.0F;
    const float width = std::min(
        std::max(285.0F, contentWidth),
        static_cast<float>(uiScreenWidth()) - 32.0F);
    // Match the collapsed minimap position in HudMinimap.cpp. The extra gap
    // clears the south compass label before the prompt begins.
    const float mapSize = std::clamp(
        static_cast<float>(std::min(uiScreenWidth(), uiScreenHeight())) *
            0.185F, 130.0F, 168.0F);
    const float top = 78.0F + mapSize;
    const Rectangle panel{
        std::max(16.0F, static_cast<float>(uiScreenWidth()) - width - 28.0F) + visual.slideX,
        top + visual.slideY, width, 70.0F};
    const float check = visual.checkProgress;
    drawSolidRoundedRect(panel, 0.55F, fade(ColorLerp(
        {17, 31, 37, 132}, {28, 58, 39, 165}, check * .8F)));
    if (visual.completionPulse > 0) drawSolidRoundedRect(panel, .55F,
        fade({147, 237, 139, static_cast<unsigned char>(visual.completionPulse * 19)}));

    const float boxSize = 24 + visual.completionPulse * 3;
    const Rectangle box{panel.x + 14 + (24 - boxSize) * .5F,
        panel.y + (panel.height - boxSize) * .5F, boxSize, boxSize};
    drawSolidRoundedRect(box, .24F, fade(ColorLerp({236, 229, 207, 235}, {110, 231, 132, 255}, check)));
    drawSolidRoundedRect({box.x + 2.5F, box.y + 2.5F, box.width - 5, box.height - 5}, .18F,
        fade(ColorLerp({27, 41, 43, 255}, {29, 75, 40, 255}, check)));
    if (check > 0) {
        const Vector2 start{box.x + box.width * .20F, box.y + box.height * .51F};
        const Vector2 elbow{box.x + box.width * .43F, box.y + box.height * .73F};
        const Vector2 end{box.x + box.width * .83F, box.y + box.height * .27F};
        const float first = std::min(1.0F, check / .4F);
        DrawLineEx(start, Vector2Lerp(start, elbow, first), 3.5F, fade({174, 255, 175, 255}));
        if (check > .4F) DrawLineEx(elbow, Vector2Lerp(elbow, end, (check - .4F) / .6F),
            3.5F, fade({174, 255, 175, 255}));
    }

    const Rectangle keycap{panel.x + 14.0F + CheckSlotWidth, panel.y + 10.0F,
                           keyWidth, 50.0F};
    drawPromptKeycap(keycap, key, false, opacity);

    const float textX = keycap.x + keycap.width + 15.0F;
    const float availableWidth = std::max(
        20.0F, panel.x + panel.width - textX - 14.0F);
    const float actionSize = fitUiTextSize(
        action, ActionFontSize, 15.0F, availableWidth);
    SdfUiTextStyle actionStyle{};
    actionStyle.fillColor = fade(ColorLerp({255, 247, 227, 255}, {204, 255, 194, 255}, check));
    actionStyle.outlineColor = fade({23, 22, 18, 255});
    actionStyle.outlinePixels = 2.4F;
    actionStyle.shadowColor = fade({0, 0, 0, 125});
    actionStyle.shadowOffsetPixels = {0, 1.8F};
    drawStyledSdfUiText(action,
        {textX, panel.y + (panel.height - measureUiText(action, actionSize).y) * 0.5F},
        actionSize, actionStyle);
}

} // namespace

using namespace hud_detail;

const HudRenderPerformanceStats&
hudRenderPerformanceStats() noexcept {
    return hudPerformanceStats;
}

std::array<Vector2, 4>
hudResourceIconCenters() noexcept {
    return cachedHudResourceIconCenters;
}

void drawHud(GameUi& ui, const SimulationSnapshot& snapshot,
             const HudViewState& view, const Camera3D& camera,
             const ControlSettings& controls) {
    hudPerformanceStats = {};
    auto sectionStart = PerformanceClock::now();
    const float woodPop = std::clamp(view.woodResourcePop, 0.0F, 1.0F);
    const float stonePop = std::clamp(view.stoneResourcePop, 0.0F, 1.0F);
    const float crystalPop = std::clamp(
        view.crystalResourcePop, 0.0F, 1.0F);
    const float coinPop = std::clamp(view.coinResourcePop, 0.0F, 1.0F);
    constexpr float ResourcePanelX = 18.0F;
    constexpr float ResourcePanelY = 9.0F;
    constexpr float ResourceIconSize = 54.0F;
    constexpr float ResourceIconGap = 6.0F;
    constexpr float ResourceItemGap = 20.0F;
    // Figma uses 52 for the current amount and 24 for the /capacity suffix.
    // UiText applies the established 1.68 font correction internally.
    constexpr float ResourceAmountFontSize = 52.0F / 1.68F;
    constexpr float ResourceCapacityFontSize = 24.0F / 1.68F;
    constexpr float ResourceCapacityYOffset = 21.5F;
    std::array<Vector2, 4> resourceAmountPositions{};
    float nextResourceX = ResourcePanelX + 6.0F;
    const auto drawResourceCell =
        [&ui, &nextResourceX, &resourceAmountPositions](
            std::size_t index, UiResourceIcon icon,
            std::string value, float pop, Color color) {
            const float x = nextResourceX;
            const float iconSize = icon == UiResourceIcon::Coin
                ? 44.0F
                : ResourceIconSize;
            const float iconInset = ResourceIconSize - iconSize;
            const float poppedIconSize = iconSize * (1.0F + pop * 0.11F);
            const float popInset = (poppedIconSize - iconSize) * 0.5F;
            const float iconX = x + iconInset;
            const float iconY =
                ResourcePanelY + 4.0F + iconInset * 0.5F;
            cachedHudResourceIconCenters[index] = {
                iconX + iconSize * 0.5F,
                iconY + iconSize * 0.5F,
            };
            ui.drawResourceIcon(
                {iconX - popInset,
                 iconY - popInset,
                 poppedIconSize, poppedIconSize}, icon);
            SdfUiTextStyle resourceTextStyle{};
            resourceTextStyle.fillColor = color;
            resourceTextStyle.outlineColor = {49, 49, 49, 255};
            resourceTextStyle.shadowColor = {82, 82, 82, 255};
            resourceTextStyle.shadowOffsetPixels = {0.0F, 4.0F};
            resourceTextStyle.outlinePixels = 3.0F;
            const float amountFontSize = value == "∞"
                ? ResourceAmountFontSize * 0.80F
                : ResourceAmountFontSize;
            const Vector2 amountPosition{
                x + ResourceIconSize + ResourceIconGap,
                ResourcePanelY + 8.0F +
                    (ResourceAmountFontSize - amountFontSize) * 0.42F};
            resourceAmountPositions[index] = amountPosition;
            const std::size_t separator = value.find('/');
            float textWidth = 0.0F;
            if (separator == std::string::npos) {
                drawStyledSdfUiText(
                    value, amountPosition,
                    amountFontSize, resourceTextStyle);
                textWidth = measureUiText(
                    value, amountFontSize).x;
            } else {
                const std::string amount = value.substr(0, separator);
                const std::string capacity = value.substr(separator);
                const Vector2 amountSize = measureUiText(
                    amount, ResourceAmountFontSize);
                const Vector2 capacitySize = measureUiText(
                    capacity, ResourceCapacityFontSize);
                const std::array runs{
                    SdfUiTextRun{
                        amount, amountPosition, ResourceAmountFontSize,
                        color},
                    SdfUiTextRun{
                        capacity,
                        {amountPosition.x + amountSize.x,
                         amountPosition.y + ResourceCapacityYOffset},
                        ResourceCapacityFontSize,
                        Color{221, 221, 221, 255}},
                };
                drawStyledSdfUiTextRuns(runs, resourceTextStyle);
                textWidth = amountSize.x + capacitySize.x;
            }
            const float contentRight = amountPosition.x + textWidth;
            nextResourceX = contentRight + ResourceItemGap;
        };
    const auto resourceValue = [&snapshot](int amount, int capacity) {
        if (snapshot.unlimitedResources) {
            return std::string{"99/99"};
        }
        return capacity > 0
            ? compactAmount(amount) + "/" + compactAmount(capacity)
            : compactAmount(amount);
    };
    const auto displayedAmount = [](double animated, int fallback) {
        if (animated < 0.0) {
            return fallback;
        }
        return std::max(0, static_cast<int>(std::floor(animated + 1e-6)));
    };
    const auto storageFull = [&snapshot](int amount, int capacity) {
        return !snapshot.unlimitedResources && capacity > 0 &&
            amount >= capacity;
    };
    constexpr Color FullStorageTextColor{255, 174, 112, 255};
    drawResourceCell(
        0U, UiResourceIcon::Wood,
        resourceValue(
            displayedAmount(view.displayedWood, snapshot.wood),
            snapshot.woodCapacity),
        woodPop,
        storageFull(snapshot.wood, snapshot.woodCapacity)
            ? FullStorageTextColor
            : UiWhite);
    drawResourceCell(
        1U, UiResourceIcon::Stone,
        resourceValue(
            displayedAmount(view.displayedStone, snapshot.stone),
            snapshot.stoneCapacity),
        stonePop,
        storageFull(snapshot.stone, snapshot.stoneCapacity)
            ? FullStorageTextColor
            : UiWhite);
    drawResourceCell(
        2U, UiResourceIcon::Crystal,
        resourceValue(
            displayedAmount(view.displayedCrystals, snapshot.crystals),
            snapshot.crystalCapacity),
        crystalPop,
        snapshot.crystalStorageFull ? FullStorageTextColor : UiWhite);
    drawResourceCell(
        3U, UiResourceIcon::Coin,
        snapshot.unlimitedResources
            ? "99"
            : compactAmount(displayedAmount(
                  view.displayedCoins, snapshot.coins)),
        coinPop, UiWhite);
    const auto drawFullResourceGlint =
        [&resourceAmountPositions, snapshot](
            std::size_t index, bool full) {
            if (!full) {
                return;
            }
            const float glintWave = 0.5F + 0.5F * std::sin(
                static_cast<float>(snapshot.elapsedSeconds) * 2.4F +
                static_cast<float>(index) * 1.7F);
            const float glint = std::pow(glintWave, 6.0F);
            const float iconX = resourceAmountPositions[index].x -
                ResourceIconSize - ResourceIconGap;
            const Vector2 glintCenter{
                iconX + ResourceIconSize * 0.78F,
                ResourcePanelY + 12.0F};
            const Color glintColor{
                255, 247, 211,
                static_cast<unsigned char>(
                    40.0F + glint * 210.0F)};
            DrawLineEx(
                {glintCenter.x - 5.0F * glint, glintCenter.y},
                {glintCenter.x + 5.0F * glint, glintCenter.y},
                1.5F + glint, glintColor);
            DrawLineEx(
                {glintCenter.x, glintCenter.y - 5.0F * glint},
                {glintCenter.x, glintCenter.y + 5.0F * glint},
                1.5F + glint, glintColor);
            DrawCircleV(
                glintCenter, 1.0F + glint * 1.8F, glintColor);
        };
    drawFullResourceGlint(
        0U, snapshot.woodCapacity > 0 &&
                snapshot.wood >= snapshot.woodCapacity);
    drawFullResourceGlint(
        1U, snapshot.stoneCapacity > 0 &&
                snapshot.stone >= snapshot.stoneCapacity);
    drawFullResourceGlint(2U, snapshot.crystalStorageFull);
    drawRunHammerInventory(ui, snapshot, {20.0F, 82.0F, std::min(600.0F, GetScreenWidth() * 0.4F), 52.8F});
    if (!snapshot.unlimitedResources &&
        snapshot.coins >= snapshot.chestRevealCoinCost &&
        !snapshot.nearestChestPosition) {
        const std::string revealHint =
            actionKeyLabel(controls, ControlAction::RevealChest) +
            "  REVEAL CHEST  •  " +
            std::to_string(snapshot.chestRevealCoinCost) + " COINS";
        drawSdfUiText(
            revealHint,
            {ResourcePanelX + 6.0F, ResourcePanelY + 54.0F},
            9.0F, {231, 213, 172, 215});
    }

    if (!snapshot.sandboxMode && !snapshot.unlimitedResources)
        drawRunDefenseInventory(ui, snapshot, {20.0F, 142.0F,
            std::min(600.0F, uiScreenWidth() * 0.4F), 52.8F});
    hudPerformanceStats.resourcesMs =
        performanceMilliseconds(sectionStart);
    sectionStart = PerformanceClock::now();

    constexpr float VitalBarX = 58.0F;
    constexpr float VitalBarWidth = 256.0F;
    constexpr float VitalBarHeight = 36.0F;
    constexpr float VitalBarGap = 8.0F;
    constexpr float VitalBarBottomMargin = 12.0F;
    const bool coreBarVisible = snapshot.coreMaxHealth > 0.0;
    const float lowerBarY = static_cast<float>(GetScreenHeight()) -
        VitalBarBottomMargin - VitalBarHeight;
    const float playerBarY = coreBarVisible
        ? lowerBarY - VitalBarGap - VitalBarHeight
        : lowerBarY;
    const float playerHealthFraction = snapshot.playerMaxHealth > 0.0
        ? static_cast<float>(
              snapshot.playerHealth / snapshot.playerMaxHealth)
        : 0.0F;
    const std::string playerHealthText =
        std::to_string(static_cast<int>(snapshot.playerHealth)) + "/" +
        std::to_string(static_cast<int>(snapshot.playerMaxHealth));
    drawVitalBar(
        ui,
        {VitalBarX, playerBarY, VitalBarWidth, VitalBarHeight},
        playerHealthFraction,
        snapshot.playerMaxHealth > 0.0
            ? static_cast<float>(
                  view.playerHealthTrail / snapshot.playerMaxHealth)
            : playerHealthFraction,
        view.playerDamagePulse,
        PlayerHealthPalette,
        playerHealthText, false);

    const bool hasRecoverableArmor =
        snapshot.playerMaxRecoverableArmor > 0.0;
    if (hasRecoverableArmor) {
        std::string armorText =
            "ARMOR " +
            std::to_string(static_cast<int>(std::ceil(
                snapshot.playerRecoverableArmor))) +
            "/" +
            std::to_string(static_cast<int>(std::ceil(
                snapshot.playerMaxRecoverableArmor)));
        if (snapshot.playerRecoverableArmor <
                snapshot.playerMaxRecoverableArmor &&
            snapshot.playerArmorRechargeDelayRemaining > 0.05) {
            armorText += "  " + std::to_string(
                static_cast<int>(std::ceil(
                    snapshot.playerArmorRechargeDelayRemaining))) + "s";
        }
        drawSdfUiText(
            armorText,
            {VitalBarX + VitalBarWidth + 12.0F,
             playerBarY + 8.0F},
            9.0F,
            snapshot.playerArmorRechargeDelayRemaining > 0.05
                ? Color{127, 172, 190, 235}
                : Color{139, 225, 250, 255});
    }

    if (coreBarVisible) {
        const std::string coreHealthText =
            std::to_string(static_cast<int>(snapshot.coreHealth)) + "/" +
            std::to_string(static_cast<int>(snapshot.coreMaxHealth));
        drawVitalBar(
            ui,
            {VitalBarX, lowerBarY, VitalBarWidth, VitalBarHeight},
            static_cast<float>(
                snapshot.coreHealth / snapshot.coreMaxHealth),
            static_cast<float>(
                snapshot.coreHealth / snapshot.coreMaxHealth),
            0.0F,
            CoreHealthPalette, coreHealthText, true);
    }
    if (snapshot.sandboxMode) {
        drawSdfUiText(
            "SANDBOX  ∞",
            {VitalBarX + VitalBarWidth + 12.0F,
             playerBarY - 2.0F},
            14.0F, {111, 220, 151, 255});
    } else if (snapshot.unlimitedResources) {
        drawSdfUiText(
            "∞",
            {VitalBarX + VitalBarWidth + 12.0F,
             playerBarY - 2.0F},
            20.0F, {111, 220, 151, 255});
    }

    hudPerformanceStats.vitalityMs =
        performanceMilliseconds(sectionStart);
    sectionStart = PerformanceClock::now();
    const auto informationStart = sectionStart;

    const float screenMinimum = static_cast<float>(
        std::min(GetScreenWidth(), GetScreenHeight()));
    const float compactMapSize = std::clamp(
        screenMinimum * 0.16F, 120.0F, 156.0F);
    constexpr float CompletionCardWidth = 360.0F;
    constexpr float MinimapTop = 0.0F;
    constexpr float MinimapCompassMargin = 32.0F;
    const float trackerY = view.minimapHidden
        ? 32.0F
        : MinimapTop + compactMapSize +
            MinimapCompassMargin * 2.0F + 20.0F;

    const ObjectiveStatus* trackedObjective = nullptr;
    for (const int index : snapshot.recommendedObjectives) {
        if (index < 0 ||
            static_cast<std::size_t>(index) >=
                snapshot.objectives.size()) {
            continue;
        }
        const ObjectiveStatus& candidate =
            snapshot.objectives[static_cast<std::size_t>(index)];
        if (candidate.active && !candidate.completed) {
            trackedObjective = &candidate;
            break;
        }
    }

    const bool completionVisible =
        !view.objectiveCompletionTitle.empty();
    const bool trackerVisible =
        snapshot.tutorialObjective.has_value() ||
        trackedObjective != nullptr;
    if (!view.mapOverlayOpen &&
        (completionVisible || trackerVisible)) {
        constexpr Color FrameColor{76, 43, 29, 255};
        constexpr Color LowerColor{126, 72, 43, 255};
        constexpr Color SurfaceColor{57, 43, 34, 248};
        constexpr Color TextColor{255, 241, 211, 255};
        const auto withAlpha =
            [](Color color, float alpha) {
                color.a = static_cast<unsigned char>(
                    static_cast<float>(color.a) *
                    std::clamp(alpha, 0.0F, 1.0F));
                return color;
            };

        float visibility = 1.0F;
        float slide = 0.0F;
        if (completionVisible) {
            const float progress = std::clamp(
                static_cast<float>(
                    view.objectiveCompletionProgress),
                0.0F, 1.0F);
            const float enter = std::clamp(
                progress / 0.14F, 0.0F, 1.0F);
            const float exit = std::clamp(
                (1.0F - progress) / 0.20F,
                0.0F, 1.0F);
            visibility = std::min(
                1.0F - std::pow(1.0F - enter, 3.0F),
                exit * exit * (3.0F - 2.0F * exit));
            slide = (1.0F - visibility) * 42.0F;
        }

        const float trackerExpansion = std::clamp(
            view.objectiveTrackerExpansion, 0.0F, 1.0F);
        const float cardWidth = completionVisible
            ? CompletionCardWidth
            : std::lerp(238.0F, 310.0F, trackerExpansion);
        const float cardHeight = completionVisible
            ? 92.0F
            : std::lerp(50.0F, 62.0F, trackerExpansion);
        const float trackerX =
            static_cast<float>(GetScreenWidth()) - cardWidth - 12.0F;
        const Rectangle card{
            trackerX + slide, trackerY,
            cardWidth, cardHeight};
        const Rectangle lower{
            card.x + 5.0F, card.y + 7.0F,
            card.width - 10.0F, card.height - 10.0F};
        const Rectangle surface{
            card.x + 5.0F, card.y + 4.0F,
            card.width - 10.0F, card.height - 13.0F};
        DrawRectangleRounded(
            card, 0.20F, 8,
            withAlpha(FrameColor, visibility));
        DrawRectangleRounded(
            lower, 0.17F, 8,
            withAlpha(LowerColor, visibility));
        DrawRectangleRounded(
            surface, 0.17F, 8,
            withAlpha(
                completionVisible
                    ? Color{82, 48, 28, 250}
                    : SurfaceColor,
                visibility));

        const float iconWidth = completionVisible
            ? 50.0F
            : std::lerp(36.0F, 42.0F, trackerExpansion);
        const float iconHeight = completionVisible
            ? 60.0F
            : iconWidth;
        const Rectangle iconFrame{
            card.x + (completionVisible ? 10.0F : 8.0F),
            card.y + (card.height - iconHeight) * 0.5F,
            iconWidth, iconHeight};
        const Rectangle iconLower{
            iconFrame.x + 3.0F, iconFrame.y + 5.0F,
            iconFrame.width - 6.0F,
            iconFrame.height - 7.0F};
        const Rectangle iconSurface{
            iconFrame.x + 3.0F, iconFrame.y + 3.0F,
            iconFrame.width - 6.0F,
            iconFrame.height - 10.0F};
        DrawRectangleRounded(
            iconFrame, 0.24F, 7,
            withAlpha(FrameColor, visibility));
        DrawRectangleRounded(
            iconLower, 0.20F, 7,
            withAlpha({178, 111, 67, 255}, visibility));
        DrawRectangleRounded(
            iconSurface, 0.20F, 7,
            withAlpha({235, 179, 123, 255}, visibility));
        const Vector2 iconCenter{
            iconFrame.x + iconFrame.width * 0.5F,
            iconFrame.y + iconFrame.height * 0.45F};

        if (completionVisible) {
            const float pulse = 0.5F + 0.5F * std::sin(
                static_cast<float>(GetTime()) * 7.0F);
            DrawCircleV(
                iconCenter, 14.0F,
                withAlpha(
                    {255, 194, 67, 255},
                    visibility));
            DrawLineEx(
                {iconCenter.x - 6.0F,
                 iconCenter.y + 0.5F},
                {iconCenter.x - 1.0F,
                 iconCenter.y + 5.5F},
                3.5F,
                withAlpha(
                    {91, 52, 29, 255},
                    visibility));
            DrawLineEx(
                {iconCenter.x - 1.0F,
                 iconCenter.y + 5.5F},
                {iconCenter.x + 7.0F,
                 iconCenter.y - 6.0F},
                3.5F,
                withAlpha(
                    {91, 52, 29, 255},
                    visibility));

            drawSdfUiText(
                "OBJECTIVE COMPLETE",
                {card.x + 72.0F, card.y + 13.0F},
                11.5F,
                withAlpha(
                    {255, 190, 69, 255},
                    visibility));
            const float titleSize = fitUiTextSize(
                view.objectiveCompletionTitle,
                17.0F, 11.0F, 205.0F);
            drawSdfUiText(
                view.objectiveCompletionTitle,
                {card.x + 72.0F, card.y + 39.0F},
                titleSize,
                withAlpha(TextColor, visibility));

            if (view.objectiveCompletionReward > 0) {
                const std::string reward = "+" +
                    std::to_string(
                        view.objectiveCompletionReward) +
                    " XP";
                const float rewardWidth =
                    measureUiText(reward, 13.0F).x;
                const Rectangle rewardBadge{
                    card.x + card.width - rewardWidth - 25.0F,
                    card.y + 31.0F,
                    rewardWidth + 16.0F, 28.0F};
                DrawRectangleRounded(
                    rewardBadge, 0.38F, 7,
                    withAlpha(
                        {42, 91, 116, 255},
                        visibility));
                drawSdfUiText(
                    reward,
                    {rewardBadge.x + 8.0F,
                     rewardBadge.y + 5.0F},
                    13.0F,
                    withAlpha(
                        {151, 231, 255, 255},
                        visibility));
            }

            DrawRectangleRounded(
                {surface.x + 9.0F,
                 surface.y + surface.height - 8.0F,
                 surface.width - 18.0F, 4.0F},
                0.9F, 4,
                withAlpha(
                    {255, 177, 46,
                     static_cast<unsigned char>(
                         155.0F + pulse * 80.0F)},
                    visibility));
            for (int index = 0; index < 3; ++index) {
                const float phase =
                    static_cast<float>(GetTime()) * 2.4F +
                    static_cast<float>(index) * 2.1F;
                const Vector2 sparkle{
                    card.x + 20.0F +
                        static_cast<float>(index) * 145.0F,
                    card.y - 5.0F + std::sin(phase) * 3.0F};
                DrawPoly(
                    sparkle, 4, 3.0F, 45.0F,
                    withAlpha(
                        {255, 211, 91, 210},
                        visibility));
            }
        } else {
            std::string title;
            double current = 0.0;
            double target = 0.0;
            ObjectiveKind kind = ObjectiveKind::Milestone;
            std::string pulseId;

            if (snapshot.tutorialObjective) {
                switch (*snapshot.tutorialObjective) {
                case TutorialObjective::MineWood:
                    title = "CHOP TREES";
                    current = snapshot.wood;
                    target = snapshot.tutorialWoodTarget;
                    break;
                case TutorialObjective::PlaceCore:
                    title = "PLACE THE CORE";
                    break;
                case TutorialObjective::PrepareForNight:
                    title = "BUILD DEFENSES";
                    break;
                case TutorialObjective::SurviveFirstWave:
                    title = "SURVIVE THE NIGHT";
                    break;
                }
            } else if (trackedObjective != nullptr) {
                title = trackedObjective->definition.title;
                current = trackedObjective->progress;
                target = trackedObjective->definition.target;
                kind = trackedObjective->definition.kind;
                pulseId = trackedObjective->definition.id;
            }

            const Color accent =
                objectiveKindColor(kind);
            DrawPoly(
                iconCenter, 4, 8.0F, 45.0F,
                withAlpha(accent, visibility));
            DrawPolyLines(
                iconCenter, 4, 11.0F, 45.0F,
                withAlpha(
                    {255, 239, 196, 190},
                    visibility));

            std::string progress;
            if (target > 0.0) {
                progress =
                    std::to_string(static_cast<int>(
                        std::floor(std::min(current, target)))) +
                    "/" +
                    std::to_string(static_cast<int>(
                        std::ceil(target)));
            }
            const float textX = iconFrame.x + iconFrame.width + 10.0F;
            const float progressSize = 12.0F;
            const float progressWidth = progress.empty()
                ? 0.0F
                : measureUiText(progress, progressSize).x;
            const float titleWidth = std::max(
                70.0F,
                card.x + card.width - 10.0F - textX -
                    (progress.empty() ? 0.0F : progressWidth + 12.0F));
            const float titleSize = fitUiTextSize(
                title, 16.0F, 10.5F, titleWidth);
            const float textY = card.y +
                (card.height - measureUiText(title, titleSize).y) *
                    0.5F - trackerExpansion * 3.0F;
            drawSdfUiText(
                title, {textX, textY}, titleSize,
                withAlpha(TextColor, visibility));
            if (!progress.empty()) {
                drawSdfUiText(
                    progress,
                    {card.x + card.width - progressWidth - 10.0F,
                     card.y +
                         (card.height -
                          measureUiText(progress, progressSize).y) *
                             0.5F - trackerExpansion * 3.0F},
                    progressSize,
                    withAlpha(TextColor, visibility));

                const float fraction = std::clamp(
                    static_cast<float>(current / target),
                    0.0F, 1.0F);
                const bool pulsing =
                    view.objectivePulse > 0.0 &&
                    !pulseId.empty() &&
                    view.objectivePulseId == pulseId;
                const float pulse = pulsing
                    ? std::sin(static_cast<float>(
                          view.objectivePulse) * PI)
                    : 0.0F;
                const Rectangle track{
                    textX,
                    card.y + card.height - 10.0F,
                    card.x + card.width - 10.0F - textX,
                    5.0F};
                DrawRectangleRounded(
                    track, 0.8F, 5,
                    withAlpha(
                        {104, 60, 37, 190},
                        visibility * trackerExpansion));
                if (fraction > 0.001F) {
                    DrawRectangleRounded(
                        {track.x, track.y,
                         track.width * fraction,
                         track.height},
                        0.8F, 5,
                        withAlpha(
                            {accent.r, accent.g, accent.b,
                             static_cast<unsigned char>(
                                 235.0F + pulse * 20.0F)},
                            visibility * trackerExpansion));
                }
            }
        }
    }

    hudPerformanceStats.objectivesMs =
        performanceMilliseconds(sectionStart);
    sectionStart = PerformanceClock::now();

    if (!view.mapOverlayOpen) drawTutorialCard(snapshot, controls, view);

    if (!view.mapOverlayOpen) {
        drawLootInventory(
            ui, snapshot, false,
            coreBarVisible);
    }

    hudPerformanceStats.lootMs =
        performanceMilliseconds(sectionStart);
    sectionStart = PerformanceClock::now();

    bool chestCompassVisible = false;
    if (snapshot.nearestChestPosition &&
        snapshot.nearestChestDistance > 0.0) {
        constexpr std::array<std::string_view, 8> Directions{
            "N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        constexpr double Octant = PI * 0.25;
        const double deltaX =
            snapshot.nearestChestPosition->x -
            snapshot.playerPosition.x;
        const double deltaZ =
            snapshot.nearestChestPosition->z -
            snapshot.playerPosition.z;
        double relative = std::atan2(deltaX, -deltaZ) -
            snapshot.playerYaw;
        while (relative <= -PI) relative += 2.0 * PI;
        while (relative > PI) relative -= 2.0 * PI;
        int directionIndex = static_cast<int>(std::lround(
            relative / Octant));
        directionIndex = (directionIndex % 8 + 8) % 8;
        const std::string compassText =
            "CHEST " + std::string(Directions[
                static_cast<std::size_t>(directionIndex)]) +
            "  " + std::to_string(static_cast<int>(
                std::lround(snapshot.nearestChestDistance))) + "m";
        const float compassWidth =
            std::max(188.0F,
                     measureUiText(compassText, 13.0F).x + 30.0F);
        ui.drawPanel({static_cast<float>(GetScreenWidth()) * 0.5F -
                          compassWidth * 0.5F,
                      172.0F, compassWidth, 31.0F}, 188);
        drawCenteredSdfUiText(
            compassText, 179.0F, 13.0F,
            {255, 215, 116, 255});
        chestCompassVisible = true;
    }

    std::string phaseText;
    std::string phaseSubtext;
    bool showEarlyBonus = false;
    Color phaseColor{245, 184, 76, 255};
    if (snapshot.state == RunState::BuildPhase) {
        if (snapshot.sandboxMode) {
            phaseText = "SANDBOX  •  BUILD MODE";
            phaseSubtext = actionKeyLabel(
                controls, ControlAction::StartWave) +
                "  START NIGHT" + (view.debugHotkeysEnabled ? std::string("  •  F5  CARDS") : std::string());
            phaseColor = {111, 220, 151, 255};
        } else {
            phaseText = "SUNSET  " +
                phaseClock(snapshot.phaseTimeRemaining);
        }
        if (!snapshot.sandboxMode && snapshot.earlyWaveBonus > 0) {
            showEarlyBonus = true;
        }
    } else if (snapshot.state == RunState::Sunset) {
        phaseText = "SUNSET  " +
            phaseClock(snapshot.phaseTimeRemaining);
        phaseColor = {255, 146, 79, 255};
    } else if (snapshot.state == RunState::Wave) {
        phaseText = snapshot.finalNight
            ? "FINAL NIGHT  •  " +
                  std::to_string(snapshot.activeEnemyCount) + " ACTIVE"
            : "DAWN  " + phaseClock(snapshot.phaseTimeRemaining);
        phaseColor = {242, 108, 82, 255};
        const auto activeBoss = std::find_if(
            snapshot.enemies.begin(), snapshot.enemies.end(),
            [](const EnemyInstance& enemy) {
                return enemy.active && enemy.type == EnemyType::Boss;
            });
        if (activeBoss != snapshot.enemies.end()) {
            phaseText += "  •  BOSS P" +
                std::to_string(activeBoss->bossPhase);
            if (activeBoss->state == EnemyState::BossRamWindup) {
                phaseText += ": RAM";
            } else if (activeBoss->state == EnemyState::BossSlamWindup) {
                phaseText += ": SLAM";
            } else if (activeBoss->state == EnemyState::BossWarCryWindup) {
                phaseText += ": WAR CRY";
            } else if (activeBoss->state == EnemyState::BossPhaseTransition) {
                phaseText += ": ENRAGING";
            }
        }
    } else if (snapshot.state == RunState::WaveComplete) {
        phaseText = "DAWN  " +
            phaseClock(snapshot.phaseTimeRemaining);
        phaseColor = {255, 194, 92, 255};
    }
    if (!phaseText.empty()) {
        const int displayedDay = snapshot.state == RunState::Wave
            ? std::max(1, snapshot.wave)
            : std::max(1, snapshot.wave + 1);
        const std::string waveText =
            "DAY " + std::to_string(displayedDay);
        SdfUiTextStyle waveStyle{};
        waveStyle.fillColor = UiWhite;
        waveStyle.outlineColor = {49, 49, 49, 255};
        waveStyle.shadowColor = {82, 82, 82, 255};
        waveStyle.shadowOffsetPixels = {0.0F, 4.0F};
        waveStyle.outlinePixels = 3.0F;
        constexpr float BaseWaveFontSize = 56.0F / 1.68F;
        float waveEntrance = 0.0F;
        if (snapshot.state == RunState::Wave &&
            snapshot.phaseDuration > 0.0) {
            const float waveElapsed = static_cast<float>(
                snapshot.phaseDuration - snapshot.phaseTimeRemaining);
            waveEntrance = 1.0F - std::clamp(
                waveElapsed / 0.55F, 0.0F, 1.0F);
            waveEntrance *= waveEntrance;
        }
        const float waveFontSize =
            BaseWaveFontSize * (1.0F + waveEntrance * 0.16F);
        waveStyle.shadowOffsetPixels.y =
            4.0F + waveEntrance * 2.0F;
        drawCenteredStyledSdfUiText(
            waveText,
            16.0F - waveEntrance * 3.0F,
            waveFontSize, waveStyle);

        if (!(snapshot.sandboxMode &&
              snapshot.state == RunState::BuildPhase) &&
            snapshot.phaseDuration > 0.0) {
            constexpr float TimelineWidth = 180.0F;
            drawDayNightTimeline(
                {static_cast<float>(GetScreenWidth()) * 0.5F -
                     TimelineWidth * 0.5F,
                 82.0F, TimelineWidth, 22.0F},
                snapshot);
        }

        if (snapshot.overtimeActive) {
            const double wholeSeconds = std::floor(snapshot.overtimeSeconds);
            const std::string timer = "OVERTIME  " + phaseClock(wholeSeconds);
            const float width = std::max(192.0F, measureUiText(timer, 12.0F).x + 24.0F);
            const float startX = static_cast<float>(GetScreenWidth()) * 0.5F - timelineWidth * 0.5F - width - 22.0F;
            ui.drawPanel({startX, 32.0F, width, 58.0F}, 210);
            const RunState pressurePhase = snapshot.state == RunState::Paused ? snapshot.stateBeforePause : snapshot.state;
            const bool fighting = pressurePhase == RunState::Wave && !snapshot.runShop.pending && !snapshot.runUpgradeChoicePending;
            const float pulse = fighting ? static_cast<float>(0.5 + 0.5 * std::sin(snapshot.overtimeSeconds * 3.0)) : 0.0F;
            drawSdfUiText(timer, {startX + 12.0F, 40.0F}, 12.0F,
                {255, static_cast<unsigned char>(139.0F + pulse * 40.0F), 90, 255});
            char pressure[80]{};
            std::snprintf(pressure, sizeof(pressure), "HP x%.2g   DMG x%.2g",
                snapshot.overtime.health, snapshot.overtime.damage);
            drawSdfUiText(pressure, {startX + 12.0F, 65.0F}, 11.0F, {255, 230, 201, 255});
        }

        const bool phaseTextContainsTime =
            (snapshot.state == RunState::BuildPhase &&
             !snapshot.sandboxMode) ||
            snapshot.state == RunState::Sunset ||
            (snapshot.state == RunState::Wave &&
             !snapshot.finalNight) ||
            snapshot.state == RunState::WaveComplete;
        if (!phaseTextContainsTime) {
            drawCenteredSdfUiText(
                phaseText, 137.0F, 12.15F, phaseColor);
        }
        if (!phaseSubtext.empty()) {
            drawCenteredSdfUiText(
                phaseSubtext, 160.0F, 9.0F,
                {221, 210, 187, 225});
        }
        if (showEarlyBonus) {
            const std::string bonusText =
                "+" +
                std::to_string(snapshot.earlyWaveBonus);
            constexpr float BonusFontSize = 18.0F;
            constexpr float CrystalIconSize = 32.0F;
            constexpr float IconGap = 6.0F;
            const Vector2 textSize = measureUiText(
                bonusText, BonusFontSize);
            const float startX =
                static_cast<float>(GetScreenWidth()) * 0.5F +
                102.0F;
            SdfUiTextStyle bonusStyle{};
            bonusStyle.fillColor = {255, 236, 184, 255};
            bonusStyle.outlineColor = {49, 49, 49, 255};
            bonusStyle.shadowColor = {82, 82, 82, 220};
            bonusStyle.shadowOffsetPixels = {0.0F, 3.0F};
            bonusStyle.outlinePixels = 2.5F;
            drawStyledSdfUiText(
                bonusText, {startX, 78.0F},
                BonusFontSize, bonusStyle);
            ui.drawResourceIcon(
                {startX + textSize.x + IconGap,
                 77.0F, CrystalIconSize, CrystalIconSize},
                UiResourceIcon::Crystal);
        }
    }

    if (phaseState == RunState::Wave && !snapshot.finalNight &&
        snapshot.phaseTimeRemaining <= 0.0) {
        drawCenteredSdfUiText(
            "ENEMIES: " + std::to_string(snapshot.activeEnemyCount + snapshot.pendingEnemyCount),
            96.0F, 12.15F, {255, 222, 155, 255});
    }

    const bool forecastVisible = !snapshot.sandboxMode && snapshot.coreMaxHealth > 0 &&
        view.nightForecastOpacity > 0.001F;
    if (forecastVisible) {
        const float width = nightForecastWidth(view.nightForecast);
        drawNightForecast(ui, view.nightForecast,
            {static_cast<float>(uiScreenWidth()) * .5F - width * .5F,
             96 - 6 * (1 - view.nightForecastOpacity), width, 48}, view.nightForecastOpacity);
    }

    const bool attackWarningVisible =
        snapshot.state == RunState::Sunset &&
        hasAttackDirections(snapshot);
    if (attackWarningVisible) {
        const float attackWarningY =
            chestCompassVisible ? 213.0F : forecastVisible ? 184.0F : 162.0F;
        const std::string directions =
            attackDirectionNames(snapshot);
        const float elapsed = static_cast<float>(std::max(
            0.0,
            snapshot.phaseDuration - snapshot.phaseTimeRemaining));
        const float entrance = std::clamp(
            elapsed / 0.34F, 0.0F, 1.0F);
        const float shifted = entrance - 1.0F;
        const float overshoot = 1.0F +
            2.70158F * shifted * shifted * shifted +
            1.70158F * shifted * shifted;

        SdfUiTextStyle headingStyle{};
        headingStyle.fillColor = {
            255, 225, 176,
            static_cast<unsigned char>(225.0F * entrance)};
        headingStyle.outlineColor = {
            69, 42, 35,
            static_cast<unsigned char>(255.0F * entrance)};
        headingStyle.shadowColor = {
            20, 16, 14,
            static_cast<unsigned char>(180.0F * entrance)};
        headingStyle.shadowOffsetPixels = {0.0F, 2.0F};
        headingStyle.outlinePixels = 1.75F;
        drawCenteredStyledSdfUiText(
            "ATTACK FROM",
            attackWarningY + (1.0F - entrance) * 5.0F,
            10.5F, headingStyle);

        const float directionBaseSize = fitUiTextSize(
            directions, 28.0F, 19.0F,
            std::min(420.0F,
                     static_cast<float>(GetScreenWidth()) - 80.0F));
        SdfUiTextStyle directionStyle{};
        directionStyle.fillColor = {
            255, 119, 76,
            static_cast<unsigned char>(255.0F * entrance)};
        directionStyle.outlineColor = {
            70, 31, 28,
            static_cast<unsigned char>(255.0F * entrance)};
        directionStyle.shadowColor = {
            20, 16, 14,
            static_cast<unsigned char>(205.0F * entrance)};
        directionStyle.shadowOffsetPixels = {0.0F, 3.0F};
        directionStyle.outlinePixels = 2.75F;
        drawCenteredStyledSdfUiText(
            directions, attackWarningY + 23.0F,
            directionBaseSize * overshoot, directionStyle);
    }

    hudPerformanceStats.informationMs =
        performanceMilliseconds(informationStart);
    hudPerformanceStats.phaseMs =
        performanceMilliseconds(sectionStart);
    sectionStart = PerformanceClock::now();

    const bool buildModeActive =
        view.actionMode == ActionMode::Buildings;
    drawCompactInsight(snapshot, view);
    const float modeBlend = std::clamp(
        view.hotbarModeBlend, 0.0F, 1.0F);
    const float easedModeBlend = modeBlend * modeBlend *
        (3.0F - 2.0F * modeBlend);
    const float weaponOffset = easedModeBlend * 160.0F;
    const float buildOffset =
        (1.0F - easedModeBlend) * 210.0F;
    if (buildModeActive) {
        if (modeBlend < 1.0F) {
            drawWeaponHotbar(ui, snapshot, view, weaponOffset);
        }
        drawBuildHotbar(ui, snapshot, view, buildOffset);
    } else {
        if (modeBlend > 0.0F) {
            drawBuildHotbar(ui, snapshot, view, buildOffset);
        }
        drawWeaponHotbar(ui, snapshot, view, weaponOffset);
    }

    hudPerformanceStats.hotbarMs =
        performanceMilliseconds(sectionStart);
    sectionStart = PerformanceClock::now();

    const float hitProgress =
        view.crosshairHitRemaining > 0.0 &&
                view.crosshairHitDuration > 0.0
            ? std::clamp(
                  static_cast<float>(
                      1.0 -
                      view.crosshairHitRemaining /
                          view.crosshairHitDuration),
                  0.0F, 1.0F)
            : 0.0F;
    const float hitPulse =
        view.crosshairHitRemaining > 0.0
            ? std::sin(hitProgress * PI)
            : 0.0F;
    const float invalidShake =
        view.invalidActionRemaining > 0.0
            ? std::sin(
                  static_cast<float>(GetTime()) * 92.0F) *
                  3.0F *
                  static_cast<float>(
                      std::min(
                          view.invalidActionRemaining / 0.22,
                          1.0))
            : 0.0F;
    const Vector2 crosshairCenter{
        static_cast<float>(GetScreenWidth()) * 0.5F +
            invalidShake,
        static_cast<float>(GetScreenHeight()) * 0.5F,
    };
    if (view.repairSweepActive) {
        drawCenteredSdfUiText(
            "REPAIR MODE", crosshairCenter.y + 42.0F,
            17.0F, {104, 232, 163, 255});
    }
    const Color crosshairColor =
        view.invalidActionRemaining > 0.0
            ? Color{255, 76, 62, 255}
            : view.crosshairHitRemaining > 0.0 &&
                      view.crosshairHitCritical
                  ? Color{255, 215, 62, 255}
                  : UiWhite;
    const float dotRadius = 3.0F + hitPulse * 1.1F;
    DrawCircleV(crosshairCenter, dotRadius, crosshairColor);
    if (view.crosshairHitRemaining > 0.0) {
        const float markerRadius =
            12.0F - hitPulse * 4.0F;
        const float markerHalf = 3.6F + hitPulse * 1.6F;
        const auto markerAlpha =
            static_cast<unsigned char>(
                std::lround(
                    (1.0F - hitProgress) * 255.0F));
        const Color markerColor{
            crosshairColor.r, crosshairColor.g,
            crosshairColor.b, markerAlpha};
        for (const Vector2 sign :
             std::array<Vector2, 4>{{
                 {-1.0F, -1.0F},
                 {1.0F, -1.0F},
                 {-1.0F, 1.0F},
                 {1.0F, 1.0F},
             }}) {
            const Vector2 outer{
                crosshairCenter.x +
                    sign.x * markerRadius,
                crosshairCenter.y +
                    sign.y * markerRadius,
            };
            const Vector2 inner{
                crosshairCenter.x +
                    sign.x *
                        (markerRadius - markerHalf),
                crosshairCenter.y +
                    sign.y *
                        (markerRadius - markerHalf),
            };
            DrawLineEx(outer, inner, 2.6F, markerColor);
        }
    }
    if (snapshot.selectedWeapon == PlayerWeapon::Rifle &&
        snapshot.rifleReloading &&
        snapshot.rifleReloadDuration > 0.0) {
        const float reloadProgress = std::clamp(
            static_cast<float>(
                1.0 -
                snapshot.rifleReloadRemaining /
                    snapshot.rifleReloadDuration),
            0.0F, 1.0F);
        DrawRing(
            crosshairCenter, 17.0F, 20.0F,
            -90.0F, 270.0F, 48,
            {35, 31, 27, 205});
        DrawRing(
            crosshairCenter, 17.0F, 20.0F,
            -90.0F,
            -90.0F + reloadProgress * 360.0F,
            48, {255, 205, 92, 245});
    }
    const float indicatorRadius =
        static_cast<float>(
            std::min(GetScreenWidth(), GetScreenHeight())) *
        0.31F;
    const Vector2 screenCenter{
        static_cast<float>(GetScreenWidth()) * 0.5F,
        static_cast<float>(GetScreenHeight()) * 0.5F,
    };
    for (const DamageIndicator& indicator :
         view.damageIndicators) {
        const float outwardX =
            static_cast<float>(std::sin(indicator.relativeAngle));
        const float outwardY =
            static_cast<float>(-std::cos(indicator.relativeAngle));
        const Vector2 center{
            screenCenter.x + outwardX * indicatorRadius,
            screenCenter.y + outwardY * indicatorRadius,
        };
        const Vector2 perpendicular{-outwardY, outwardX};
        const Vector2 tip{center.x + outwardX * 13.0F,
                          center.y + outwardY * 13.0F};
        const Vector2 baseLeft{
            center.x - outwardX * 9.0F +
                perpendicular.x * 9.0F,
            center.y - outwardY * 9.0F +
                perpendicular.y * 9.0F,
        };
        const Vector2 baseRight{
            center.x - outwardX * 9.0F -
                perpendicular.x * 9.0F,
            center.y - outwardY * 9.0F -
                perpendicular.y * 9.0F,
        };
        const double fade =
            indicator.remaining / indicator.duration;
        const auto alpha = static_cast<unsigned char>(
            220.0 * std::clamp(fade, 0.0, 1.0));
        const Color color =
            indicator.severe ? Color{255, 92, 42, alpha}
                             : Color{235, 62, 62, alpha};
        DrawTriangle(tip, baseLeft, baseRight, color);
    }

    if (view.statusMessageRemaining > 0.0 &&
        !view.statusMessage.empty()) {
        const std::string status{view.statusMessage};
        constexpr float StatusFontSize = 22.0F;
        const float statusWidth = measureUiText(
            status, StatusFontSize).x;
        const float statusY = static_cast<float>(
            GetScreenHeight() / 2 + 78);
        const float statusShake =
            view.statusMessageShakeRemaining > 0.0
                ? std::sin(
                      static_cast<float>(GetTime()) * 105.0F) *
                      5.0F * static_cast<float>(std::min(
                          view.statusMessageShakeRemaining / 0.22,
                          1.0))
                : 0.0F;
        SdfUiTextStyle statusStyle{};
        statusStyle.fillColor = {255, 194, 92, 255};
        statusStyle.outlineColor = {49, 49, 49, 255};
        statusStyle.shadowColor = {20, 16, 14, 210};
        statusStyle.shadowOffsetPixels = {0.0F, 3.0F};
        statusStyle.outlinePixels = 2.0F;
        drawStyledSdfUiText(
            status,
            {(static_cast<float>(GetScreenWidth()) -
              statusWidth) * 0.5F + statusShake,
             statusY},
            StatusFontSize, statusStyle);
    }

    if (snapshot.buildingPreview) {
        const BuildingPreview& preview = *snapshot.buildingPreview;
        const std::string placementText = placementMessage(
            preview.placement.error, preview.type);
        if (!placementText.empty()) {
            constexpr float MessageFontSize = 16.0F;
            const float messageY = static_cast<float>(
                GetScreenHeight()) * 0.5F + 72.0F;
            const float messageWidth = measureUiText(
                placementText, MessageFontSize).x;
            SdfUiTextStyle messageStyle{};
            messageStyle.fillColor = placementMessageColor(
                preview.placement.error);
            messageStyle.outlineColor = {49, 49, 49, 255};
            messageStyle.shadowColor = {20, 16, 14, 210};
            messageStyle.shadowOffsetPixels = {0.0F, 3.0F};
            messageStyle.outlinePixels = 2.0F;
            drawStyledSdfUiText(
                placementText,
                {(static_cast<float>(GetScreenWidth()) -
                  messageWidth) * 0.5F + invalidShake,
                 messageY},
                MessageFontSize, messageStyle);
        }
    } else if (
        snapshot.aimedBuilding &&
        view.showBuildingContextCard) {
        const auto aimed = std::find_if(
            snapshot.buildings.begin(), snapshot.buildings.end(),
            [&snapshot](const BuildingInstance& building) {
                return building.id == *snapshot.aimedBuilding;
            });
        if (aimed != snapshot.buildings.end()) {
            drawBuildingContextCard(
                ui, snapshot, *aimed, view, camera, controls);
        }
    }

    if (snapshot.aimedModularBuilding && view.showBuildingContextCard)
        drawModularBuildingContextCard(snapshot, camera,
            view.buildingContextCardEntranceSeconds);

    hudPerformanceStats.feedbackMs =
        performanceMilliseconds(sectionStart);
}

} // namespace ian
