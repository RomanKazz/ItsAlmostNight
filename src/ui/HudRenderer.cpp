#include "ui/HudRenderer.hpp"

#include "app/UserSettings.hpp"
#include "buildings/BuildingOrientation.hpp"
#include "buildings/CoreProgression.hpp"
#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudFormatting.hpp"
#include "ui/HudHotbar.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ian {
namespace {

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
        return "Not enough resources";
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
    const float iconSize = 40.0F * (1.0F + damagePulse * 0.10F);
    const Rectangle iconBounds{
        bounds.x - 43.0F - (iconSize - 40.0F) * 0.5F,
        bounds.y - 2.0F - (iconSize - 40.0F) * 0.5F,
        iconSize, iconSize};
    ui.drawVitalIcon(iconBounds, core);
}

void drawDayNightBowl(
    Rectangle bounds, float cornerRadius, Color color) {
    const float safeCornerRadius = std::clamp(
        cornerRadius, 0.0F, bounds.height * 0.5F);
    const float centerY = bounds.y + safeCornerRadius;
    const float radiusY = bounds.height - safeCornerRadius;
    if (safeCornerRadius > 0.0F) {
        const Rectangle topCap{
            bounds.x, bounds.y, bounds.width,
            safeCornerRadius * 2.0F};
        DrawRectangleRounded(
            topCap,
            safeCornerRadius * 2.0F /
                std::max(topCap.height, 1.0F),
            8, color);
    }
    BeginScissorMode(
        static_cast<int>(std::floor(bounds.x)),
        static_cast<int>(std::floor(
            bounds.y + safeCornerRadius)),
        static_cast<int>(std::ceil(bounds.width)),
        static_cast<int>(std::ceil(
            bounds.height - safeCornerRadius)));
    DrawEllipse(
        static_cast<int>(std::lround(
            bounds.x + bounds.width * 0.5F)),
        static_cast<int>(std::lround(centerY)),
        bounds.width * 0.5F, radiusY, color);
    EndScissorMode();
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

void drawDayNightDialSector(
    Rectangle bounds, float cornerRadius,
    float centerAngle, float sectorAngle, Color color) {
    constexpr int SegmentCount = 32;
    const float safeCornerRadius = std::clamp(
        cornerRadius, 0.0F, bounds.height * 0.5F);
    const Vector2 center{
        bounds.x + bounds.width * 0.5F,
        bounds.y + safeCornerRadius,
    };
    const float radiusX = bounds.width * 0.5F;
    const float radiusY = bounds.height - safeCornerRadius;
    const float startAngle = centerAngle - sectorAngle * 0.5F;
    BeginScissorMode(
        static_cast<int>(std::floor(bounds.x)),
        static_cast<int>(std::floor(bounds.y)),
        static_cast<int>(std::ceil(bounds.width)),
        static_cast<int>(std::ceil(bounds.height)));
    // Raylib shape triangles sample an anti-aliased texture on every internal
    // edge, exposing the sector below as colored radial speckles. Submit an
    // untextured fan and overlap only internal edges by a subpixel angle.
    constexpr float SegmentOverlap = 0.0025F;
    const float step = sectorAngle / static_cast<float>(SegmentCount);
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(color.r, color.g, color.b, color.a);
    for (int segment = 0; segment < SegmentCount; ++segment) {
        const float segmentStart = startAngle +
            static_cast<float>(segment) * step -
            (segment > 0 ? SegmentOverlap : 0.0F);
        const float segmentEnd = startAngle +
            static_cast<float>(segment + 1) * step +
            (segment + 1 < SegmentCount ? SegmentOverlap : 0.0F);
        rlVertex2f(center.x, center.y);
        rlVertex2f(
            center.x + std::cos(segmentEnd) * radiusX,
            center.y + std::sin(segmentEnd) * radiusY);
        rlVertex2f(
            center.x + std::cos(segmentStart) * radiusX,
            center.y + std::sin(segmentStart) * radiusY);
    }
    rlEnd();
    EndScissorMode();
}

void drawDayNightIndicator(
    Rectangle bounds, const SimulationSnapshot& snapshot) {
    constexpr Color ShadowColor{82, 82, 82, 255};
    constexpr Color FrameColor{49, 49, 49, 255};
    constexpr Color NightColor{59, 67, 165, 255};
    constexpr Color SunsetColor{232, 121, 62, 255};
    constexpr Color DayColor{224, 177, 39, 255};
    constexpr float FrameInset = 4.0F;
    constexpr float DayArc = 2.70F;
    constexpr float SunsetArc = 0.55F;
    constexpr float NightArc = 2.70F;
    constexpr float DawnArc = 2.0F * PI -
        DayArc - SunsetArc - NightArc;

    const bool urgentPhase = snapshot.phaseDuration > 0.0 &&
        snapshot.phaseTimeRemaining > 0.0 &&
        snapshot.phaseTimeRemaining <= 10.0;
    if (urgentPhase) {
        const float urgency = 1.0F - std::clamp(
            static_cast<float>(snapshot.phaseTimeRemaining / 10.0),
            0.0F, 1.0F);
        const float wave = 0.5F + 0.5F * std::sin(
            static_cast<float>(GetTime()) * 8.0F);
        drawDayNightBowl(
            {bounds.x - 3.0F, bounds.y - 2.0F,
             bounds.width + 6.0F, bounds.height + 6.0F},
            8.0F,
            {255, 105, 60,
             static_cast<unsigned char>(
                 28.0F + urgency * (35.0F + wave * 55.0F))});
    }

    drawDayNightBowl(
        {bounds.x, bounds.y + 4.0F,
         bounds.width, bounds.height},
        6.0F, ShadowColor);
    drawDayNightBowl(bounds, 6.0F, FrameColor);

    const Rectangle inner{
        bounds.x + FrameInset,
        bounds.y + FrameInset,
        bounds.width - FrameInset * 2.0F,
        bounds.height - FrameInset * 2.0F,
    };
    constexpr float InnerCornerRadius = 4.0F;
    drawDayNightBowl(inner, InnerCornerRadius, NightColor);

    // At zero the fixed notch sits exactly on the start of the yellow day
    // sector. Decreasing the angle rotates the dial counter-clockwise.
    float sectorStart = PI * 0.5F - dayNightCycleAngle(snapshot);
    const auto drawRotatingSector =
        [&](float sectorArc, Color color) {
            drawDayNightDialSector(
                inner, InnerCornerRadius,
                sectorStart + sectorArc * 0.5F,
                sectorArc, color);
            sectorStart += sectorArc;
        };
    drawRotatingSector(DayArc, DayColor);
    drawRotatingSector(SunsetArc, SunsetColor);
    drawRotatingSector(NightArc, NightColor);
    drawRotatingSector(DawnArc, SunsetColor);

    const Vector2 center{
        inner.x + inner.width * 0.5F,
        inner.y + InnerCornerRadius};
    DrawTriangle(
        {center.x - 7.0F, inner.y},
        {center.x, inner.y + 11.0F},
        {center.x + 7.0F, inner.y},
        FrameColor);
}

const char* objectiveKindLabel(ObjectiveKind kind) {
    switch (kind) {
    case ObjectiveKind::Milestone:
        return "MILESTONE";
    case ObjectiveKind::Challenge:
        return "CHALLENGE";
    case ObjectiveKind::WorldEvent:
        return "WORLD EVENT";
    }
    return "OBJECTIVE";
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

UiBarColor objectiveBarColor(ObjectiveKind kind) {
    switch (kind) {
    case ObjectiveKind::Milestone:
        return UiBarColor::Purple;
    case ObjectiveKind::Challenge:
        return UiBarColor::Yellow;
    case ObjectiveKind::WorldEvent:
        return UiBarColor::Green;
    }
    return UiBarColor::Yellow;
}

std::string objectiveInstruction(
    const ObjectiveDefinition& objective) {
    if (!objective.description.empty()) {
        return objective.description;
    }
    switch (objective.metric) {
    case ObjectiveMetric::TreesDestroyed:
        return "Fully chop down tree resource objects";
    case ObjectiveMetric::StonesDestroyed:
        return "Fully break stone resource objects";
    case ObjectiveMetric::CrystalsGathered:
        return "Collect crystal resource units";
    case ObjectiveMetric::TotalResourcesGathered:
        return "Collect wood, stone or crystals";
    case ObjectiveMetric::LargeDepositDepleted:
        return "Fully deplete a large resource deposit";
    case ObjectiveMetric::AllResourceTypesInDay:
        return "Gather wood, stone and crystals in one day";
    case ObjectiveMetric::MatchingToolDepletion:
        return "Destroy a tree or stone with its matching starter tool";
    case ObjectiveMetric::ResourcesInSixtySeconds:
        return "Gather 50 resources within 60 seconds";
    case ObjectiveMetric::ConsecutiveDepletions:
        return "Destroy 5 resource objects without missing";
    case ObjectiveMetric::NightResourcesGathered:
        return "Gather 30 resources during the night";
    case ObjectiveMetric::FarResourceGathered:
        return "Gather a resource far away from the Core";
    case ObjectiveMetric::EnemiesKilled:
        return "Defeat enemies by any means";
    case ObjectiveMetric::BuildingsPlaced:
        return "Place complete buildings";
    case ObjectiveMetric::ModularPiecesPlaced:
        return "Place platforms, walls, ramps or gates";
    case ObjectiveMetric::BuildingsUpgraded:
        return "Upgrade existing buildings";
    case ObjectiveMetric::StructuresRepaired:
        return "Repair damaged buildings or modular pieces";
    case ObjectiveMetric::WavesCompleted:
        return "Survive enemy waves";
    case ObjectiveMetric::CoinsCollected:
        return "Pick up Coins dropped in the world";
    case ObjectiveMetric::ChestsOpened:
        return "Open loot chests";
    case ObjectiveMetric::LootCollected:
        return "Collect items from chests or props";
    case ObjectiveMetric::PlayerDashes:
        return "Use Dash while moving";
    case ObjectiveMetric::RifleShots:
        return "Fire the rifle";
    case ObjectiveMetric::ElementalHits:
        return "Damage enemies with Ice or Fire Wand";
    case ObjectiveMetric::TrapHits:
        return "Let traps hit enemies";
    case ObjectiveMetric::CannonShots:
        return "Let cannons fire at enemies";
    case ObjectiveMetric::BombsThrown:
        return "Throw bombs";
    case ObjectiveMetric::EarlyWavesStarted:
        return "Start waves early enough to earn a reward";
    case ObjectiveMetric::StructuresFortified:
        return "Use the hammer to fortify structures";
    case ObjectiveMetric::GatesToggled:
        return "Open or close gates";
    case ObjectiveMetric::BuildingsSold:
        return "Sell buildings";
    case ObjectiveMetric::FallsSaved:
        return "Use Rope to survive a lethal fall";
    case ObjectiveMetric::Count:
        break;
    }
    return objective.title;
}

void drawObjectiveKindIcon(
    ObjectiveKind kind, Vector2 center, Color color) {
    if (kind == ObjectiveKind::Milestone) {
        DrawPoly(center, 4, 9.0F, 45.0F, color);
        DrawPolyLines(center, 4, 12.0F, 45.0F,
                      {color.r, color.g, color.b, 110});
    } else if (kind == ObjectiveKind::Challenge) {
        DrawPoly(center, 5, 10.0F, -90.0F, color);
        DrawCircleV(center, 3.2F, {255, 247, 211, 240});
    } else {
        DrawCircleLines(
            static_cast<int>(center.x),
            static_cast<int>(center.y), 10.0F, color);
        DrawCircleLines(
            static_cast<int>(center.x),
            static_cast<int>(center.y), 5.0F,
            {color.r, color.g, color.b, 175});
        DrawCircleV(center, 2.5F, color);
    }
}

std::string actionKeyLabel(
    const ControlSettings& controls, ControlAction action) {
    const int key = controlKey(controls, action);
    if (action == ControlAction::Attack) {
        if (key == KEY_NULL) {
            return "LMB";
        }
        return "LMB / " + keyboardKeyName(key);
    }
    if (action == ControlAction::Dash && key == KEY_NULL) {
        return "RMB";
    }
    return keyboardKeyName(key);
}

std::string tutorialText(const SimulationSnapshot& snapshot) {
    if (!snapshot.tutorialObjective) {
        return {};
    }
    switch (*snapshot.tutorialObjective) {
    case TutorialObjective::MineWood:
        return "MINE TREES  •  WOOD " +
               std::to_string(snapshot.wood) + "/" +
               std::to_string(snapshot.tutorialWoodTarget);
    case TutorialObjective::PlaceCore:
        return "PLACE CORE  [1]";
    case TutorialObjective::PrepareForNight:
        return "BUILD DEFENSES  •  [N] STARTS SUNSET";
    case TutorialObjective::SurviveFirstWave:
        return "SURVIVE FIRST NIGHT";
    }
    return {};
}

struct TutorialProgress {
    double current{};
    double target{};
};

std::optional<TutorialProgress> tutorialProgress(
    const SimulationSnapshot& snapshot) {
    if (!snapshot.tutorialObjective) return std::nullopt;
    switch (*snapshot.tutorialObjective) {
    case TutorialObjective::MineWood:
        return TutorialProgress{
            static_cast<double>(snapshot.wood),
            static_cast<double>(snapshot.tutorialWoodTarget)};
    case TutorialObjective::PlaceCore:
    case TutorialObjective::PrepareForNight:
    case TutorialObjective::SurviveFirstWave:
        return std::nullopt;
    }
    return std::nullopt;
}

void drawBuildingContextCard(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const BuildingInstance& building, const HudViewState& view,
    const Camera3D& camera, const ControlSettings& controls) {
    constexpr float CardWidth = 760.0F;
    constexpr float CardHeight = 520.0F;
    const Vec3 buildingCenter =
        buildingWorldPosition(building);
    const Vector2 buildingScreen = GetWorldToScreen(
        {static_cast<float>(buildingCenter.x), 1.15F,
         static_cast<float>(buildingCenter.z)},
        camera);
    constexpr float HorizontalGap = 150.0F;
    float x = buildingScreen.x + HorizontalGap;
    if (x + CardWidth >
        static_cast<float>(GetScreenWidth()) - 24.0F) {
        x = buildingScreen.x - CardWidth - HorizontalGap;
    }
    x = std::clamp(
        x, 24.0F,
        std::max(24.0F,
                 static_cast<float>(GetScreenWidth()) -
                     CardWidth - 24.0F));
    const float y = std::clamp(
        buildingScreen.y - CardHeight * 0.5F, 24.0F,
        std::max(24.0F,
                 static_cast<float>(GetScreenHeight()) -
                     CardHeight - 24.0F));
    ui.drawPanel({x, y, CardWidth, CardHeight}, 242);

    drawSdfUiText(buildingDisplayName(building.type), {x + 24.0F, y + 18.0F},
               26.0F, {255, 235, 174, 255});
    drawSdfUiText("LEVEL " + std::to_string(building.level),
               {x + 24.0F, y + 62.0F}, 19.0F, UiWhite);

    drawSdfUiText("STATS", {x + 24.0F, y + 105.0F},
               16.0F, {245, 184, 76, 255});

    struct StatRow {
        const char* label;
        double current;
        std::optional<double> previous;
        std::optional<double> next;
        int decimals;
        const char* suffix;
    };
    std::vector<StatRow> rows;
    rows.reserve(6);
    if (snapshot.aimedBuildingStats) {
        const BuildingStatComparison& comparison =
            *snapshot.aimedBuildingStats;
        const BuildingStats& current = comparison.current;
        const BuildingStats* previous =
            comparison.previous
                ? &*comparison.previous
                : nullptr;
        const BuildingStats* next =
            comparison.next ? &*comparison.next : nullptr;
        rows.push_back({
            "HEALTH", current.maxHealth,
            previous
                ? std::optional<double>(previous->maxHealth)
                : std::nullopt,
            next ? std::optional<double>(next->maxHealth)
                 : std::nullopt,
            0, "",
        });
        const auto addOptional =
            [&rows, &comparison, previous, next](
                const char* label,
                std::optional<double> BuildingStats::* member,
                int decimals, const char* suffix) {
                const auto currentValue =
                    comparison.current.*member;
                if (!currentValue) {
                    return;
                }
                rows.push_back({
                    label, *currentValue,
                    previous ? previous->*member : std::nullopt,
                    next ? next->*member : std::nullopt,
                    decimals, suffix,
                });
            };
        if (building.type == BuildingType::Core) {
            addOptional(
                "DEFENSE LIMIT",
                &BuildingStats::defenseBuildingLimit, 0, "");
            addOptional(
                "EACH PRODUCER",
                &BuildingStats::producerPerTypeLimit, 0, "");
            addOptional(
                "WOOD CAPACITY",
                &BuildingStats::woodCapacity, 0, "");
            addOptional(
                "STONE CAPACITY",
                &BuildingStats::stoneCapacity, 0, "");
            addOptional(
                "CRYSTAL CAPACITY",
                &BuildingStats::crystalCapacity, 0, "");
        } else if (building.type == BuildingType::Turret ||
            building.type == BuildingType::GunTurret ||
            building.type == BuildingType::Cannon ||
            building.type == BuildingType::Catapult) {
            addOptional(
                "DAMAGE", &BuildingStats::attackDamage, 0, "");
            addOptional(
                "RANGE", &BuildingStats::attackRange, 1, "m");
            addOptional(
                "ARC", &BuildingStats::attackArcDegrees, 0, " deg");
            addOptional(
                "ATTACK RATE",
                &BuildingStats::attacksPerSecond, 2, "/s");
            if (building.type == BuildingType::Turret) {
                addOptional(
                    "PIERCING",
                    &BuildingStats::piercingCount, 0, "");
            }
            if (building.type == BuildingType::Cannon ||
                building.type == BuildingType::Catapult) {
                addOptional(
                    "BLAST RADIUS",
                    &BuildingStats::effectRadius, 1, "m");
            }
        } else if (
            building.type == BuildingType::CrystalMine ||
            building.type == BuildingType::LumberMill ||
            building.type == BuildingType::Quarry) {
            addOptional(
                building.type == BuildingType::CrystalMine
                    ? "CRYSTALS / CYCLE"
                    : building.type ==
                              BuildingType::LumberMill
                          ? "WOOD / CYCLE"
                          : "STONE / CYCLE",
                &BuildingStats::productionPerCycle, 0, "");
            addOptional(
                "CYCLE TIME",
                &BuildingStats::productionInterval, 1, "s");
        } else if (building.type == BuildingType::SlowTrap) {
            addOptional(
                "SLOW", &BuildingStats::slowPercent, 0, "%");
            addOptional(
                "RADIUS", &BuildingStats::effectRadius, 1, "m");
            addOptional(
                "DURATION",
                &BuildingStats::effectDuration, 1, "s");
            addOptional(
                "COOLDOWN", &BuildingStats::cooldown, 1, "s");
        } else if (building.type == BuildingType::SpikeTrap) {
            addOptional(
                "DAMAGE", &BuildingStats::attackDamage, 0, "");
            addOptional(
                "RADIUS", &BuildingStats::effectRadius, 2, "m");
            addOptional(
                "COOLDOWN", &BuildingStats::cooldown, 2, "s");
        } else if (
            building.type == BuildingType::WoodStorage ||
            building.type == BuildingType::StoneStorage ||
            building.type == BuildingType::CrystalStorage) {
            addOptional(
                "CAPACITY", &BuildingStats::storageCapacity,
                0, "");
        }
    } else {
        rows.push_back({
            "HEALTH", building.maxHealth,
            std::nullopt, std::nullopt, 0, "",
        });
    }

    const bool animatingUpgrade =
        view.buildingStatsUpgradeEntity &&
        *view.buildingStatsUpgradeEntity == building.id &&
        view.buildingStatsUpgradeRemaining > 0.0 &&
        view.buildingStatsUpgradeDuration > 0.0;
    const float rawProgress = animatingUpgrade
        ? std::clamp(
              static_cast<float>(
                  1.0 -
                  view.buildingStatsUpgradeRemaining /
                      view.buildingStatsUpgradeDuration),
              0.0F, 1.0F)
        : 1.0F;
    const float easedProgress =
        1.0F - std::pow(1.0F - rawProgress, 3.0F);
    const float numberPulse = animatingUpgrade
        ? std::sin(rawProgress * PI) * 3.0F
        : 0.0F;
    const Color currentColor = animatingUpgrade
        ? ColorLerp(
              Color{255, 191, 65, 255}, UiWhite,
              easedProgress)
        : UiWhite;
    const auto formatValue =
        [](double value, int decimals,
           const char* suffix) {
            char buffer[48]{};
            std::snprintf(
                buffer, sizeof(buffer),
                decimals == 0 ? "%.0f%s"
                              : decimals == 1 ? "%.1f%s"
                                              : "%.2f%s",
                value, suffix);
            return std::string(buffer);
        };
    for (std::size_t index = 0;
         index < rows.size() && index < 6; ++index) {
        const StatRow& row = rows[index];
        const float rowY =
            y + 141.0F + static_cast<float>(index) * 31.0F;
        drawSdfUiText(
            row.label, {x + 24.0F, rowY}, 15.0F,
            {205, 197, 184, 255});
        double displayedCurrent = row.current;
        if (animatingUpgrade && row.previous) {
            displayedCurrent =
                *row.previous +
                (row.current - *row.previous) *
                    static_cast<double>(easedProgress);
        }
        drawSdfUiText(
            formatValue(
                displayedCurrent, row.decimals, row.suffix),
            {x + 265.0F, rowY - numberPulse * 0.5F},
            18.0F + numberPulse, currentColor);
        if (row.next) {
            drawSdfUiText(
                ">", {x + 390.0F, rowY}, 18.0F,
                {245, 184, 76, 255});
            drawSdfUiText(
                formatValue(*row.next, row.decimals, row.suffix),
                {x + 435.0F, rowY}, 18.0F,
                {105, 231, 132, 255});
            const double delta = *row.next - row.current;
            if (std::abs(delta) > 0.0001) {
                drawSdfUiText(
                    "+" + formatValue(
                              delta, row.decimals, row.suffix),
                    {x + 565.0F, rowY}, 14.0F,
                    {105, 231, 132, 220});
            }
        }
    }

    const bool globalBlueprint =
        BuildingSystem::usesGlobalBlueprint(building.type);
    const bool upgradeUnlocked =
        !globalBlueprint &&
        (building.type == BuildingType::Core ||
         snapshot.coreLevel > building.level);
    const bool upgradeAffordable =
        snapshot.aimedBuildingUpgradeCost &&
        upgradeUnlocked &&
        (snapshot.unlimitedResources ||
         (snapshot.wood >=
              snapshot.aimedBuildingUpgradeCost->wood &&
          snapshot.stone >=
              snapshot.aimedBuildingUpgradeCost->stone &&
          snapshot.crystals >=
              snapshot.aimedBuildingUpgradeCost->crystals));
    const bool coreActions = building.type == BuildingType::Core;
    const std::string actionPrefix = coreActions
        ? "Q  COPY    F  REPAIR ALL    "
        : "Q  COPY    F  REPAIR    ";
    constexpr float ActionFontSize = 14.0F;
    if (upgradeAffordable) {
        const float glowPulse =
            0.5F +
            0.5F *
                std::sin(
                    static_cast<float>(GetTime()) * 4.2F);
        const float upgradeX =
            x + 24.0F +
            measureUiText(
                actionPrefix, ActionFontSize).x -
            7.0F;
        const Rectangle glowBounds{
            upgradeX, y + 332.0F, 150.0F, 34.0F};
        DrawRectangleRounded(
            glowBounds, 0.24F, 8,
            {
                245, 184, 76,
                static_cast<unsigned char>(
                    std::lround(
                        30.0F + glowPulse * 52.0F)),
            });
        DrawRectangleRoundedLinesEx(
            glowBounds, 0.24F, 8,
            1.5F + glowPulse,
            {
                255, 220, 115,
                static_cast<unsigned char>(
                    std::lround(
                        115.0F + glowPulse * 110.0F)),
            });
    }
    std::string primaryActions =
        actionKeyLabel(controls, ControlAction::Copy) +
        "  COPY    " +
        actionKeyLabel(controls, ControlAction::Repair) +
        (coreActions ? "  REPAIR ALL" : "  REPAIR");
    if (!globalBlueprint) {
        primaryActions += "    " +
            actionKeyLabel(controls, ControlAction::Upgrade) +
            "  UPGRADE";
    }
    drawSdfUiText(
               primaryActions,
               {x + 24.0F, y + 340.0F}, 14.0F,
               {222, 210, 194, 255});
    std::string actions;
    if (building.type != BuildingType::Core) {
        actions = "X  SELL";
    } else {
        actions = actionKeyLabel(controls, ControlAction::Interact) +
            "  MANAGE DEFENSES    F  REPAIR ALL  " +
            std::to_string(snapshot.repairAllCoinCost) +
            " COINS";
        if (snapshot.unlockedWeapons[
                static_cast<std::size_t>(PlayerWeapon::Bomb)]) {
            actions += "    V  +" +
                std::to_string(snapshot.bombPurchaseAmount) +
                " BOMBS  " +
                std::to_string(snapshot.bombPurchaseCoinCost) +
                " COINS";
        }
    }
    if (building.type == BuildingType::Gate) {
        actions += "    E  TOGGLE";
    }
    if (!actions.empty()) {
        drawSdfUiText(actions, {x + 24.0F, y + 371.0F},
                   14.0F, {222, 210, 194, 255});
    }

    const ResourceCost repairCost = buildingRepairCost(building);
    if (repairCost.wood > 0 || repairCost.stone > 0 ||
        repairCost.crystals > 0) {
        drawSdfUiText(
            "REPAIR  " + hud_detail::costText(repairCost) + "  +" +
                std::to_string(static_cast<int>(std::ceil(
                    std::max(0.0, building.maxHealth - building.health)))) +
                " HP",
            {x + 24.0F, y + 399.0F}, 13.0F,
            {255, 210, 126, 235});
    } else if (building.health >= building.maxHealth) {
        drawSdfUiText(
            "REPAIR  FULL HEALTH",
            {x + 24.0F, y + 399.0F}, 13.0F,
            {145, 218, 159, 225});
    }

    if (globalBlueprint) {
        drawSdfUiText(
            "GLOBAL BLUEPRINT LEVEL " +
                std::to_string(building.level) +
                "  -  MANAGE AT CORE",
            {x + 24.0F, y + 424.0F}, 17.0F,
            {245, 184, 76, 255});
        return;
    }
    if (!snapshot.aimedBuildingUpgradeCost) {
        drawSdfUiText("MAX LEVEL", {x + 24.0F, y + 424.0F},
                   19.0F, {245, 184, 76, 255});
        return;
    }

    drawSdfUiText("UPGRADE", {x + 24.0F, y + 427.0F}, 16.0F,
               {245, 184, 76, 255});
    const ResourceCost cost =
        *snapshot.aimedBuildingUpgradeCost;
    float itemX = x + 190.0F;
    const auto drawCost =
        [&ui, &itemX, y](UiResourceIcon icon, int amount,
                         int available) {
            if (amount <= 0) {
                return;
            }
            ui.drawResourceIcon(
                {itemX, y + 413.0F, 48.0F, 48.0F}, icon);
            drawSdfUiText(
                std::to_string(amount),
                {itemX + 54.0F, y + 420.0F}, 18.0F,
                available >= amount
                    ? Color{238, 238, 226, 255}
                    : Color{242, 103, 83, 255});
            itemX += 142.0F;
        };
    drawCost(UiResourceIcon::Wood, cost.wood, snapshot.wood);
    drawCost(UiResourceIcon::Stone, cost.stone, snapshot.stone);
    drawCost(
        UiResourceIcon::Crystal, cost.crystals, snapshot.crystals);
    if (building.type == BuildingType::Core) {
        const std::string_view unlocks = coreLevelUnlockSummary(
            static_cast<int>(building.level) + 1);
        if (!unlocks.empty()) {
            drawSdfUiText(unlocks, {x + 24.0F, y + 470.0F}, 13.0F,
                       {105, 231, 132, 245});
        }
    }
}

std::string phaseClock(double seconds) {
    const int total = std::max(0, static_cast<int>(std::ceil(seconds)));
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", total / 60,
                  total % 60);
    return buffer;
}

} // namespace

using namespace hud_detail;

void drawHud(GameUi& ui, const SimulationSnapshot& snapshot,
             const HudViewState& view, const Camera3D& camera,
             const ControlSettings& controls) {
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
            ui.drawResourceIcon(
                {x + iconInset - popInset,
                 ResourcePanelY + 4.0F + iconInset * 0.5F - popInset,
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
        if (snapshot.unlimitedResources) return std::string{"∞"};
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
    drawResourceCell(
        0U, UiResourceIcon::Wood,
        resourceValue(
            displayedAmount(view.displayedWood, snapshot.wood),
            snapshot.woodCapacity),
        woodPop, UiWhite);
    drawResourceCell(
        1U, UiResourceIcon::Stone,
        resourceValue(
            displayedAmount(view.displayedStone, snapshot.stone),
            snapshot.stoneCapacity),
        stonePop, UiWhite);
    drawResourceCell(
        2U, UiResourceIcon::Crystal,
        resourceValue(
            displayedAmount(view.displayedCrystals, snapshot.crystals),
            snapshot.crystalCapacity),
        crystalPop,
        snapshot.crystalStorageFull
            ? Color{255, 174, 112, 255}
            : UiWhite);
    drawResourceCell(
        3U, UiResourceIcon::Coin,
        snapshot.unlimitedResources
            ? "∞"
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

    int objectiveCount = 0;
    for (int index : snapshot.recommendedObjectives) {
        if (index >= 0 &&
            static_cast<std::size_t>(index) < snapshot.objectives.size())
            ++objectiveCount;
    }
    const float screenMinimum = static_cast<float>(
        std::min(GetScreenWidth(), GetScreenHeight()));
    const float compactMapSize = std::clamp(
        screenMinimum * 0.16F, 120.0F, 156.0F);
    constexpr float TrackerWidth = 330.0F;
    const float trackerX = static_cast<float>(GetScreenWidth()) -
        TrackerWidth - 12.0F;
    constexpr float MinimapTop = 0.0F;
    constexpr float MinimapCompassMargin = 32.0F;
    const float trackerY = view.minimapHidden
        ? 32.0F
        : MinimapTop + compactMapSize +
            MinimapCompassMargin * 2.0F + 20.0F;
    const bool quietBuildPhase =
        snapshot.state == RunState::BuildPhase;
    if (!view.mapOverlayOpen && quietBuildPhase &&
        snapshot.tutorialObjective) {
        const std::string tutorial = tutorialText(snapshot);
        const Rectangle card{
            trackerX, trackerY, TrackerWidth, 54.0F};
        DrawRectangleRounded(
            card, 0.17F, 5, {20, 18, 16, 166});
        DrawRectangleRoundedLinesEx(
            card, 0.17F, 5, 1.2F,
            {255, 194, 82, 135});
        DrawPoly(
            {card.x + 17.0F, card.y + 19.0F},
            4, 6.0F, 45.0F, {255, 194, 82, 255});
        drawSdfUiText(
            tutorial,
            {card.x + 32.0F, card.y + 8.0F},
            fitUiTextSize(
                tutorial, 12.0F, 9.0F,
                card.width - 44.0F, 18.0F),
            {247, 238, 215, 255});
        if (const auto progress = tutorialProgress(snapshot);
            progress && progress->target > 0.0) {
            const float fraction = std::clamp(
                static_cast<float>(progress->current /
                                   progress->target),
                0.0F, 1.0F);
            DrawRectangleRounded(
                {card.x + 32.0F, card.y + 35.0F,
                 card.width - 44.0F, 6.0F},
                0.8F, 3, {47, 41, 35, 210});
            DrawRectangleRounded(
                {card.x + 32.0F, card.y + 35.0F,
                 (card.width - 44.0F) * fraction, 6.0F},
                0.8F, 3, {255, 194, 82, 245});
        }
    }
    if (!view.mapOverlayOpen &&
        objectiveCount > 0 &&
        view.informationExpansion > 0.65F) {
        constexpr float ObjectiveX = 12.0F;
        constexpr float ObjectiveY = 374.0F;
        constexpr float ObjectiveWidth = 530.0F;
        constexpr float HeaderHeight = 48.0F;
        constexpr float CardHeight = 88.0F;
        constexpr float CardGap = 7.0F;
        const float objectiveHeight = HeaderHeight + 9.0F +
            static_cast<float>(objectiveCount) * CardHeight +
            static_cast<float>(objectiveCount - 1) * CardGap;
        ui.drawPanel(
            {ObjectiveX, ObjectiveY, ObjectiveWidth,
             objectiveHeight}, 224);
        DrawCircleV(
            {ObjectiveX + 24.0F, ObjectiveY + 23.0F},
            11.0F, {102, 78, 48, 230});
        DrawPoly(
            {ObjectiveX + 24.0F, ObjectiveY + 23.0F},
            4, 6.5F, 45.0F, {255, 215, 105, 255});
        drawSdfUiText(
            "OBJECTIVE TRACKER",
            {ObjectiveX + 43.0F, ObjectiveY + 9.0F},
            14.0F, {255, 225, 157, 255});
        drawSdfUiText(
            "Complete tasks to earn XP",
            {ObjectiveX + 43.0F, ObjectiveY + 27.0F},
            10.5F, {190, 178, 157, 225});
        const std::string activeCount =
            "TRACKED  " + std::to_string(objectiveCount);
        const float activeWidth =
            measureUiText(activeCount, 10.5F).x;
        drawSdfUiText(
            activeCount,
            {ObjectiveX + ObjectiveWidth - activeWidth - 15.0F,
             ObjectiveY + 17.0F},
            10.5F, {232, 205, 139, 230});
        int row = 0;
        for (int rawIndex : snapshot.recommendedObjectives) {
            if (rawIndex < 0 ||
                static_cast<std::size_t>(rawIndex) >= snapshot.objectives.size())
                continue;
            const ObjectiveStatus& objective =
                snapshot.objectives[static_cast<std::size_t>(rawIndex)];
            const float rowY = ObjectiveY + HeaderHeight +
                static_cast<float>(row) * (CardHeight + CardGap);
            const Rectangle card{
                ObjectiveX + 9.0F, rowY,
                ObjectiveWidth - 18.0F, CardHeight};
            const Color kindColor =
                objectiveKindColor(objective.definition.kind);
            const bool pulsing =
                view.objectivePulse > 0.0 &&
                view.objectivePulseId == objective.definition.id;
            if (pulsing) {
                const float pulse = static_cast<float>(
                    std::sin(view.objectivePulse * PI));
                DrawRectangleRounded(
                    {card.x - 4.0F - pulse * 2.0F,
                     card.y - 4.0F - pulse * 2.0F,
                     card.width + 8.0F + pulse * 4.0F,
                     card.height + 8.0F + pulse * 4.0F},
                    0.11F, 8,
                    {kindColor.r, kindColor.g, kindColor.b,
                     static_cast<unsigned char>(70.0F * pulse)});
            }
            ui.drawInsetPanel(card, 238);
            const bool completed =
                objective.definition.target > 0.0 &&
                objective.progress + 1e-6 >=
                    objective.definition.target;
            if (pulsing && completed) {
                const float completionPulse = static_cast<float>(
                    std::sin(view.objectivePulse * PI));
                DrawRectangleRounded(
                    card, 0.09F, 8,
                    {255, 198, 71,
                     static_cast<unsigned char>(
                         completionPulse * 88.0F)});
            }
            DrawRectangleRoundedLinesEx(
                card, 0.09F, 8, pulsing ? 2.2F : 1.2F,
                {kindColor.r, kindColor.g, kindColor.b,
                 static_cast<unsigned char>(pulsing ? 235 : 115)});
            DrawRectangleRounded(
                {card.x + 3.0F, card.y + 7.0F,
                 4.0F, card.height - 14.0F},
                0.8F, 4, kindColor);
            drawObjectiveKindIcon(
                objective.definition.kind,
                {card.x + 25.0F, card.y + 27.0F},
                kindColor);

            const std::string eyebrow =
                std::string(objectiveKindLabel(
                    objective.definition.kind)) +
                "  •  " + objective.definition.title;
            const float eyebrowSize = fitUiTextSize(
                eyebrow, 11.0F, 9.0F,
                card.width - 165.0F, 17.0F);
            drawSdfUiText(
                eyebrow,
                {card.x + 44.0F, card.y + 8.0F},
                eyebrowSize, kindColor);
            const std::string reward = "+" +
                std::to_string(static_cast<int>(std::lround(
                    objective.definition.insightReward))) +
                " XP";
            const float rewardWidth =
                measureUiText(reward, 10.0F).x;
            drawSdfUiText(
                reward,
                {card.x + card.width - rewardWidth - 12.0F,
                 card.y + 9.0F},
                10.0F, {226, 207, 255, 245});

            const std::string instruction =
                objectiveInstruction(objective.definition);
            const float instructionSize = fitUiTextSize(
                instruction, 14.0F, 10.5F,
                card.width - 142.0F, 22.0F);
            drawSdfUiText(
                instruction,
                {card.x + 44.0F, card.y + 31.0F},
                instructionSize, {247, 238, 215, 255});

            const int current = static_cast<int>(
                std::floor(std::min(
                    objective.progress,
                    objective.definition.target)));
            const int target = static_cast<int>(
                std::ceil(objective.definition.target));
            const std::string progress =
                std::to_string(current) + " / " +
                std::to_string(target);
            const float progressWidth =
                measureUiText(progress, 13.0F).x;
            drawSdfUiText(
                progress,
                {card.x + card.width - progressWidth - 12.0F,
                 card.y + 33.0F},
                13.0F, {255, 239, 189, 255});
            const float fraction = objective.definition.target > 0.0
                ? std::clamp(static_cast<float>(
                      objective.progress /
                      objective.definition.target), 0.0F, 1.0F)
                : 0.0F;
            const Rectangle progressBar{
                card.x + 44.0F, card.y + 61.0F,
                card.width - 58.0F, 14.0F};
            ui.drawProgressBar(
                progressBar, fraction,
                objectiveBarColor(objective.definition.kind));
            if (fraction > 0.01F) {
                const float tipX = progressBar.x +
                    progressBar.width * fraction;
                const float shimmer = 0.55F + 0.45F *
                    std::sin(static_cast<float>(
                        snapshot.elapsedSeconds * 7.0) +
                        static_cast<float>(row));
                DrawCircleV(
                    {tipX, progressBar.y + progressBar.height * 0.5F},
                    pulsing ? 6.0F : 3.2F,
                    {kindColor.r, kindColor.g, kindColor.b,
                     static_cast<unsigned char>(150.0F +
                         shimmer * 95.0F)});
            }
            ++row;
        }
    } else if (!view.mapOverlayOpen && quietBuildPhase &&
               !snapshot.tutorialObjective && objectiveCount > 0) {
        constexpr float RowHeight = 54.0F;
        int row = 0;
        for (int rawIndex : snapshot.recommendedObjectives) {
            if (rawIndex < 0 ||
                static_cast<std::size_t>(rawIndex) >=
                    snapshot.objectives.size()) {
                continue;
            }
            const ObjectiveStatus& objective =
                snapshot.objectives[
                    static_cast<std::size_t>(rawIndex)];
            const float rowY = trackerY +
                static_cast<float>(row) * RowHeight;
            const Color color = objectiveKindColor(
                objective.definition.kind);
            const bool pulsing =
                view.objectivePulse > 0.0 &&
                view.objectivePulseId == objective.definition.id;
            DrawRectangleRounded(
                {trackerX, rowY, TrackerWidth, RowHeight - 3.0F},
                0.18F, 5,
                {20, 18, 16,
                 static_cast<unsigned char>(pulsing ? 232 : 190)});
            const bool completed =
                objective.definition.target > 0.0 &&
                objective.progress + 1e-6 >=
                    objective.definition.target;
            if (pulsing && completed) {
                const float completionPulse = static_cast<float>(
                    std::sin(view.objectivePulse * PI));
                DrawRectangleRounded(
                    {trackerX, rowY,
                     TrackerWidth, RowHeight - 3.0F},
                    0.18F, 5,
                    {255, 198, 71,
                     static_cast<unsigned char>(
                         completionPulse * 92.0F)});
            }
            DrawRectangleRoundedLinesEx(
                {trackerX, rowY, TrackerWidth, RowHeight - 3.0F},
                0.18F, 5, 1.0F,
                {255, 196, 76,
                 static_cast<unsigned char>(pulsing ? 150 : 72)});
            drawObjectiveKindIcon(
                objective.definition.kind,
                {trackerX + 17.0F, rowY + 20.0F}, color);
            const std::string instruction =
                objectiveInstruction(objective.definition);
            const float instructionSize = fitUiTextSize(
                instruction, 12.0F, 9.0F, 205.0F, 18.0F);
            drawSdfUiText(
                instruction,
                {trackerX + 32.0F, rowY + 7.0F},
                instructionSize, {242, 234, 214, 250});
            const int current = static_cast<int>(std::floor(
                std::min(objective.progress,
                         objective.definition.target)));
            const int target = static_cast<int>(std::ceil(
                objective.definition.target));
            const std::string progress =
                std::to_string(current) + "/" +
                std::to_string(target);
            const std::string reward = "+" +
                std::to_string(static_cast<int>(std::lround(
                    objective.definition.insightReward)));
            const float progressWidth =
                measureUiText(progress, 12.0F).x;
            const float rewardWidth =
                measureUiText(reward, 10.0F).x;
            drawSdfUiText(
                progress,
                {trackerX + TrackerWidth - progressWidth - 9.0F,
                 rowY + 6.0F},
                12.0F, {255, 235, 176, 255});
            drawSdfUiText(
                reward,
                {trackerX + TrackerWidth - rewardWidth - 22.0F,
                 rowY + 27.0F},
                10.0F, {195, 174, 245, 235});
            DrawPoly(
                {trackerX + TrackerWidth - 10.0F, rowY + 34.0F},
                4, 4.0F, 45.0F, {195, 174, 245, 235});
            const float fraction = objective.definition.target > 0.0
                ? std::clamp(static_cast<float>(
                      objective.progress /
                      objective.definition.target), 0.0F, 1.0F)
                : 0.0F;
            DrawRectangleRounded(
                {trackerX + 32.0F, rowY + 37.0F,
                 210.0F, 5.0F},
                0.8F, 3, {47, 41, 35, 210});
            DrawRectangleRounded(
                {trackerX + 32.0F, rowY + 37.0F,
                 210.0F * fraction, 5.0F},
                0.8F, 3, color);
            ++row;
            break;
        }
    }

    if (!view.mapOverlayOpen) {
        drawLootInventory(
            ui, snapshot, false,
            coreBarVisible);
    }

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
    Color phaseColor{245, 184, 76, 255};
    if (snapshot.state == RunState::BuildPhase) {
        if (snapshot.sandboxMode) {
            phaseText = "SANDBOX  •  BUILD MODE";
            phaseSubtext = actionKeyLabel(
                controls, ControlAction::StartWave) +
                "  START NIGHT  •  F5  CARDS";
            phaseColor = {111, 220, 151, 255};
        } else {
            phaseText = "SUNSET  " +
                phaseClock(snapshot.phaseTimeRemaining);
        }
        if (!snapshot.sandboxMode && snapshot.earlyWaveBonus > 0) {
            phaseSubtext = "N  EARLY  +" +
                std::to_string(snapshot.earlyWaveBonus) +
                " CRYSTALS";
            if (snapshot.earlyWaveCoinBonus > 0) {
                phaseSubtext += "  +" +
                    std::to_string(
                        snapshot.earlyWaveCoinBonus) +
                    " COINS";
            }
            if (snapshot.earlyWaveInsightBonus > 0) {
                phaseSubtext += "  +" +
                    std::to_string(
                        snapshot.earlyWaveInsightBonus) +
                    " XP";
            }
        }
    } else if (snapshot.state == RunState::Sunset) {
        phaseText = "SUNSET  " +
            phaseClock(snapshot.phaseTimeRemaining);
        phaseSubtext = "FINAL BUILD / REPAIR";
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
        const int displayedWave = snapshot.state == RunState::Sunset
            ? std::max(0, snapshot.wave + 1)
            : std::max(0, snapshot.wave);
        const std::string waveText =
            "WAVE " + std::to_string(displayedWave);
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

        constexpr float DayNightIndicatorWidth = 84.0F;
        constexpr float DayNightIndicatorHeight = 42.0F;
        constexpr float DayNightIndicatorY = 68.0F;
        drawDayNightIndicator(
            {
                (static_cast<float>(GetScreenWidth()) -
                 DayNightIndicatorWidth) * 0.5F,
                DayNightIndicatorY,
                DayNightIndicatorWidth,
                DayNightIndicatorHeight,
            },
            snapshot);

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
    }

    const bool attackWarningVisible =
        snapshot.state == RunState::Sunset &&
        hasAttackDirections(snapshot);
    float nextCenterHudY = chestCompassVisible ? 216.0F : 190.0F;
    if (attackWarningVisible) {
        const std::string warning =
            "ATTACK FROM " + attackDirectionNames(snapshot);
        constexpr float WarningFontSize = 24.0F;
        const float warningWidth =
            measureUiText(warning, WarningFontSize).x;
        const float centerX =
            static_cast<float>(GetScreenWidth()) * 0.5F;
        DrawRectangleRounded(
            {centerX - warningWidth * 0.5F - 22.0F,
             nextCenterHudY, warningWidth + 44.0F, 48.0F},
            0.24F, 8, {32, 10, 9, 205});
        drawCenteredSdfUiText(
            warning, nextCenterHudY + 8.0F,
            WarningFontSize, {255, 134, 88, 255});
    }

    const bool buildModeActive =
        view.actionMode == ActionMode::Buildings;
    drawCompactInsight(
        ui, snapshot, view,
        buildModeActive || view.showCoreHealth);
    if (buildModeActive) {
        drawBuildHotbar(ui, snapshot, view);
    } else {
        drawWeaponHotbar(ui, snapshot, view);
    }

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
    const float gap =
        5.0F - hitPulse * 2.8F +
        std::clamp(view.weaponRecoilAmount, 0.0F, 1.0F) *
            5.5F;
    constexpr float ArmLength = 8.0F;
    DrawLineEx(
        {crosshairCenter.x - gap - ArmLength,
         crosshairCenter.y},
        {crosshairCenter.x - gap, crosshairCenter.y},
        2.2F, crosshairColor);
    DrawLineEx(
        {crosshairCenter.x + gap, crosshairCenter.y},
        {crosshairCenter.x + gap + ArmLength,
         crosshairCenter.y},
        2.2F, crosshairColor);
    DrawLineEx(
        {crosshairCenter.x, crosshairCenter.y - gap - ArmLength},
        {crosshairCenter.x, crosshairCenter.y - gap},
        2.2F, crosshairColor);
    DrawLineEx(
        {crosshairCenter.x, crosshairCenter.y + gap},
        {crosshairCenter.x,
         crosshairCenter.y + gap + ArmLength},
        2.2F, crosshairColor);
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
    if (snapshot.dashUnlocked && snapshot.dashCooldownDuration > 0.0) {
        const float ready = std::clamp(
            static_cast<float>(
                1.0 - snapshot.dashCooldownRemaining /
                          snapshot.dashCooldownDuration),
            0.0F, 1.0F);
        const Color dashColor = snapshot.dashing
            ? Color{185, 242, 255, 255}
            : ready >= 0.999F
                  ? Color{91, 201, 244, 235}
                  : Color{77, 126, 151, 190};
        DrawRing(
            crosshairCenter, 25.0F, 27.0F,
            -90.0F, 270.0F, 48,
            {22, 35, 43, 170});
        DrawRing(
            crosshairCenter, 25.0F, 27.0F,
            -90.0F, -90.0F + ready * 360.0F,
            48, dashColor);
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
        const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
        DrawRectangleRounded(
            {centerX - statusWidth * 0.5F - 18.0F,
             statusY - 7.0F, statusWidth + 36.0F, 40.0F},
            0.3F, 7, {24, 20, 16, 198});
        drawCenteredSdfUiText(
            status, statusY, StatusFontSize,
            {255, 194, 92, 255});
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
                  messageWidth) * 0.5F,
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

}

void drawRunStateOverlay(const SimulationSnapshot& snapshot) {
    if (snapshot.state == RunState::Paused) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      {0, 0, 0, 150});
        drawCenteredSdfUiText(
            "PAUSED", static_cast<float>(GetScreenHeight() / 2 - 24),
            48.0F, UiWhite);
    } else if (snapshot.state == RunState::StageClear) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      {4, 7, 10, 218});
        const float centerX =
            static_cast<float>(GetScreenWidth()) * 0.5F;
        const float centerY =
            static_cast<float>(GetScreenHeight()) * 0.5F;
        const float panelWidth = std::min(
            760.0F, static_cast<float>(GetScreenWidth()) - 40.0F);
        const float x = centerX - panelWidth * 0.5F;
        DrawRectangleRounded(
            {x, centerY - 190.0F, panelWidth, 380.0F},
            0.06F, 10, {24, 25, 28, 248});
        DrawRectangleRoundedLinesEx(
            {x, centerY - 190.0F, panelWidth, 380.0F},
            0.06F, 10, 3.0F, {232, 178, 74, 255});
        drawCenteredSdfUiText(
            "FORT DEFENDED", centerY - 145.0F,
            46.0F, {255, 205, 92, 255});
        drawCenteredSdfUiText(
            "STAGE CLEARED  •  WAVE 12 BOSS DEFEATED",
            centerY - 78.0F, 19.0F, {229, 219, 190, 255});
        drawCenteredSdfUiText(
            "Your rewards are secured. Bank the run, or test the build",
            centerY - 25.0F, 16.0F, {184, 186, 179, 255});
        drawCenteredSdfUiText(
            "against an endless night that grows stronger every wave.",
            centerY + 3.0F, 16.0F, {184, 186, 179, 255});
        drawCenteredSdfUiText(
            "ENTER / F   FACE THE FINAL NIGHT",
            centerY + 72.0F, 21.0F, {242, 108, 82, 255});
        drawCenteredSdfUiText(
            "B   BANK REWARDS AND END RUN",
            centerY + 118.0F, 18.0F, {142, 207, 147, 255});
    } else if (snapshot.state == RunState::Defeat ||
               snapshot.state == RunState::Victory) {
        const bool successfulRun = snapshot.stageCleared;
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      {4, 7, 10, 210});
        const float screenWidth = static_cast<float>(GetScreenWidth());
        const float screenHeight = static_cast<float>(GetScreenHeight());
        const float panelWidth = std::min(760.0F, screenWidth - 40.0F);
        const float panelHeight = std::min(570.0F, screenHeight - 34.0F);
        const float x = (screenWidth - panelWidth) * 0.5F;
        const float y = (screenHeight - panelHeight) * 0.5F;
        DrawRectangleRounded(
            {x, y, panelWidth, panelHeight}, 0.055F, 10,
            {24, 25, 28, 246});
        DrawRectangleRoundedLinesEx(
            {x, y, panelWidth, panelHeight}, 0.055F, 10, 3.0F,
            successfulRun ? Color{188, 145, 65, 255}
                          : Color{142, 96, 65, 255});

        drawCenteredSdfUiText(
            successfulRun ? "FORT DEFENDED" : "CORE DESTROYED",
            y + 24.0F, 43.0F,
            successfulRun ? Color{255, 205, 92, 255}
                          : Color{242, 91, 73, 255});
        char survival[96]{};
        const int elapsed = std::max(
            0, static_cast<int>(snapshot.elapsedSeconds));
        std::snprintf(
            survival, sizeof(survival),
            "SURVIVED  %02d:%02d   •   WAVES CLEARED  %d",
            elapsed / 60, elapsed % 60,
            snapshot.runStatistics.wavesSurvived);
        drawCenteredSdfUiText(
            survival, y + 82.0F, 19.0F,
            {224, 205, 170, 255});

        const float cardGap = 16.0F;
        const float cardX = x + 28.0F;
        const float cardY = y + 124.0F;
        const float cardWidth = (panelWidth - 72.0F) * 0.5F;
        const auto drawSummaryCard = [&](float left, std::string_view label,
                                         std::string value, Color color) {
            DrawRectangleRounded(
                {left, cardY, cardWidth, 91.0F}, 0.12F, 7,
                {38, 38, 40, 245});
            drawSdfUiText(label, {left + 18.0F, cardY + 12.0F}, 14.0F,
                       {169, 162, 151, 255});
            drawSdfUiText(value, {left + 18.0F, cardY + 37.0F}, 31.0F,
                       color);
        };
        const double totalDamage =
            snapshot.runStatistics.playerDamageDealt +
            snapshot.runStatistics.defenseDamageDealt;
        drawSummaryCard(
            cardX, "ENEMIES DEFEATED",
            compactAmount(snapshot.runStatistics.enemiesDefeated),
            {255, 177, 84, 255});
        drawSummaryCard(
            cardX + cardWidth + cardGap, "TOTAL DAMAGE",
            compactAmount(static_cast<int>(std::lround(totalDamage))),
            {239, 116, 88, 255});

        const float detailsY = cardY + 118.0F;
        const float leftX = x + 46.0F;
        const float rightX = x + panelWidth * 0.54F;
        const auto drawStat = [&](float left, float top,
                                  std::string_view label,
                                  std::string value, Color valueColor) {
            drawSdfUiText(label, {left, top}, 15.0F,
                       {155, 151, 145, 255});
            const Vector2 valueSize = measureUiText(value, 18.0F);
            drawSdfUiText(
                value,
                {left + panelWidth * 0.39F - valueSize.x, top - 2.0F},
                18.0F, valueColor);
        };
        drawStat(leftX, detailsY, "PLAYER DAMAGE",
                 compactAmount(static_cast<int>(std::lround(
                     snapshot.runStatistics.playerDamageDealt))),
                 {242, 204, 119, 255});
        drawStat(leftX, detailsY + 38.0F, "DEFENSE DAMAGE",
                 compactAmount(static_cast<int>(std::lround(
                     snapshot.runStatistics.defenseDamageDealt))),
                 {115, 199, 239, 255});
        drawStat(leftX, detailsY + 76.0F, "STRUCTURES BUILT",
                 std::to_string(snapshot.runStatistics.structuresBuilt),
                 {145, 217, 151, 255});
        drawStat(leftX, detailsY + 114.0F, "STRUCTURES LOST",
                 std::to_string(snapshot.runStatistics.structuresLost),
                 {239, 125, 105, 255});

        drawStat(rightX, detailsY, "WOOD ACQUIRED",
                 compactAmount(snapshot.runStatistics.woodAcquired),
                 {218, 164, 93, 255});
        drawStat(rightX, detailsY + 38.0F, "STONE ACQUIRED",
                 compactAmount(snapshot.runStatistics.stoneAcquired),
                 {185, 194, 204, 255});
        drawStat(rightX, detailsY + 76.0F, "CRYSTALS ACQUIRED",
                 compactAmount(snapshot.runStatistics.crystalsAcquired),
                 {186, 121, 239, 255});
        drawStat(rightX, detailsY + 114.0F, "COINS COLLECTED",
                 compactAmount(snapshot.runStatistics.coinsCollected),
                 {255, 198, 65, 255});

        DrawLineEx(
            {x + 36.0F, y + panelHeight - 72.0F},
            {x + panelWidth - 36.0F, y + panelHeight - 72.0F},
            2.0F, {85, 70, 58, 190});
        drawCenteredSdfUiText(
            "R  RESTART     •     ESC  MENU",
            y + panelHeight - 47.0F, 18.0F,
            {238, 230, 210, 255});
    } else if (snapshot.playerRespawning) {
        constexpr int PanelWidth = 460;
        constexpr int PanelHeight = 150;
        const int panelX =
            (GetScreenWidth() - PanelWidth) / 2;
        const int panelY =
            (GetScreenHeight() - PanelHeight) / 2;
        DrawRectangleRounded(
            {static_cast<float>(panelX),
             static_cast<float>(panelY),
             static_cast<float>(PanelWidth),
             static_cast<float>(PanelHeight)},
            0.12F, 8, {12, 14, 18, 224});

        char respawnText[64]{};
        std::snprintf(
            respawnText, sizeof(respawnText),
            "RESPAWN IN %.1f",
            snapshot.playerRespawnTimeRemaining);
        drawCenteredSdfUiText(
            respawnText,
            static_cast<float>(panelY + 30), 34.0F,
            UiWhite);

        char lossText[128]{};
        std::snprintf(
            lossText, sizeof(lossText),
            "Lost  Wood %d   Stone %d   Crystals %d",
            snapshot.deathLostWood,
            snapshot.deathLostStone,
            snapshot.deathLostCrystals);
        drawCenteredSdfUiText(
            lossText,
            static_cast<float>(panelY + 91), 19.0F,
            {235, 148, 112, 255});
    }
}

} // namespace ian
