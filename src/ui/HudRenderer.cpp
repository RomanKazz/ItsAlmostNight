#include "ui/HudRenderer.hpp"

#include "app/UserSettings.hpp"
#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ian {
namespace {

const char* placementMessage(PlacementError error) {
    switch (error) {
    case PlacementError::None:
        return "LMB place/drag   RMB cancel   Wheel rotate";
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
        return "Core level II required";
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

std::string costText(const ResourceCost& cost) {
    std::string result;
    const auto append = [&result](const char* prefix, int amount) {
        if (amount <= 0) {
            return;
        }
        if (!result.empty()) {
            result += "  ";
        }
        result += prefix;
        result += std::to_string(amount);
    };
    append("W:", cost.wood);
    append("S:", cost.stone);
    append("C:", cost.gold);
    return result.empty() ? "FREE" : result;
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
    case ObjectiveMetric::BareHandsDepletion:
        return "Destroy a tree or stone without a tool";
    case ObjectiveMetric::ResourcesInSixtySeconds:
        return "Gather 50 resources within 60 seconds";
    case ObjectiveMetric::ConsecutiveDepletions:
        return "Destroy 5 resource objects without missing";
    case ObjectiveMetric::NightResourcesGathered:
        return "Gather 30 resources during the night";
    case ObjectiveMetric::FarResourceGathered:
        return "Gather a resource far away from the Core";
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
    case TutorialObjective::BareHandsTraining:
        return "OBJECTIVE: Gather by hand - Wood " +
               std::to_string(snapshot.bareHandsWoodGathered) + "/15  Stone " +
               std::to_string(snapshot.bareHandsStoneGathered) + "/10";
    case TutorialObjective::MineWood:
        return "OBJECTIVE: Mine trees - Wood " +
               std::to_string(snapshot.wood) + "/" +
               std::to_string(snapshot.tutorialWoodTarget);
    case TutorialObjective::PlaceCore:
        return "OBJECTIVE: Place Core [1]";
    case TutorialObjective::MineStone:
        return "OBJECTIVE: Mine rocks - Stone " +
               std::to_string(snapshot.stone) + "/" +
               std::to_string(snapshot.tutorialStoneTarget);
    case TutorialObjective::BuildGoldMine:
        return "OBJECTIVE: Build Crystal Mine [4]";
    case TutorialObjective::PrepareForNight:
        return "OBJECTIVE: Build defenses - [N] starts sunset";
    case TutorialObjective::SurviveFirstWave:
        return "OBJECTIVE: Survive first night";
    }
    return {};
}

void drawBuildingContextCard(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const BuildingInstance& building, const HudViewState& view,
    const Camera3D& camera, const ControlSettings& controls) {
    constexpr float CardWidth = 760.0F;
    constexpr float CardHeight = 480.0F;
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

    drawUiText(buildingDisplayName(building.type), {x + 24.0F, y + 18.0F},
               26.0F, {255, 235, 174, 255});
    drawUiText("LEVEL " + std::to_string(building.level),
               {x + 24.0F, y + 62.0F}, 19.0F, RAYWHITE);

    drawUiText("STATS", {x + 24.0F, y + 105.0F},
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
    rows.reserve(5);
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
        if (building.type == BuildingType::Turret ||
            building.type == BuildingType::Cannon) {
            addOptional(
                "DAMAGE", &BuildingStats::attackDamage, 0, "");
            addOptional(
                "RANGE", &BuildingStats::attackRange, 1, "m");
            addOptional(
                "ATTACK RATE",
                &BuildingStats::attacksPerSecond, 2, "/s");
            if (building.type == BuildingType::Cannon) {
                addOptional(
                    "BLAST RADIUS",
                    &BuildingStats::effectRadius, 1, "m");
            }
        } else if (
            building.type == BuildingType::GoldMine ||
            building.type == BuildingType::LumberMill ||
            building.type == BuildingType::Quarry) {
            addOptional(
                building.type == BuildingType::GoldMine
                    ? "CRYSTALS / CYCLE"
                    : building.type ==
                              BuildingType::LumberMill
                          ? "WOOD / CYCLE"
                          : "STONE / CYCLE",
                &BuildingStats::goldPerCycle, 0, "");
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
              Color{255, 191, 65, 255}, RAYWHITE,
              easedProgress)
        : RAYWHITE;
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
         index < rows.size() && index < 5; ++index) {
        const StatRow& row = rows[index];
        const float rowY =
            y + 141.0F + static_cast<float>(index) * 37.0F;
        drawUiText(
            row.label, {x + 24.0F, rowY}, 15.0F,
            {205, 197, 184, 255});
        double displayedCurrent = row.current;
        if (animatingUpgrade && row.previous) {
            displayedCurrent =
                *row.previous +
                (row.current - *row.previous) *
                    static_cast<double>(easedProgress);
        }
        drawUiText(
            formatValue(
                displayedCurrent, row.decimals, row.suffix),
            {x + 265.0F, rowY - numberPulse * 0.5F},
            18.0F + numberPulse, currentColor);
        if (row.next) {
            drawUiText(
                ">", {x + 390.0F, rowY}, 18.0F,
                {245, 184, 76, 255});
            drawUiText(
                formatValue(*row.next, row.decimals, row.suffix),
                {x + 435.0F, rowY}, 18.0F,
                {105, 231, 132, 255});
            const double delta = *row.next - row.current;
            if (std::abs(delta) > 0.0001) {
                drawUiText(
                    "+" + formatValue(
                              delta, row.decimals, row.suffix),
                    {x + 565.0F, rowY}, 14.0F,
                    {105, 231, 132, 220});
            }
        }
    }

    const bool upgradeUnlocked =
        building.type == BuildingType::Core ||
        snapshot.coreLevel > building.level;
    const bool upgradeAffordable =
        snapshot.aimedBuildingUpgradeCost &&
        upgradeUnlocked &&
        (snapshot.unlimitedResources ||
         (snapshot.wood >=
              snapshot.aimedBuildingUpgradeCost->wood &&
          snapshot.stone >=
              snapshot.aimedBuildingUpgradeCost->stone &&
          snapshot.gold >=
              snapshot.aimedBuildingUpgradeCost->gold));
    constexpr std::string_view ActionPrefix =
        "Q  COPY    F  REPAIR    ";
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
                ActionPrefix, ActionFontSize).x -
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
    drawUiText(
               actionKeyLabel(controls, ControlAction::Copy) +
                   "  COPY    " +
                   actionKeyLabel(controls, ControlAction::Repair) +
                   "  REPAIR    " +
                   actionKeyLabel(controls, ControlAction::Upgrade) +
                   "  UPGRADE",
               {x + 24.0F, y + 340.0F}, 14.0F,
               {222, 210, 194, 255});
    std::string actions;
    if (building.type != BuildingType::Core) {
        actions = "X  SELL";
    }
    if (building.type == BuildingType::Gate) {
        actions += "    E  TOGGLE";
    }
    if (!actions.empty()) {
        drawUiText(actions, {x + 24.0F, y + 371.0F},
                   14.0F, {222, 210, 194, 255});
    }

    const ResourceCost repairCost = buildingRepairCost(building);
    if (repairCost.wood > 0 || repairCost.stone > 0 ||
        repairCost.gold > 0) {
        drawUiText(
            "REPAIR  " + costText(repairCost) + "  +" +
                std::to_string(static_cast<int>(std::ceil(
                    std::max(0.0, building.maxHealth - building.health)))) +
                " HP",
            {x + 24.0F, y + 399.0F}, 13.0F,
            {255, 210, 126, 235});
    } else if (building.health >= building.maxHealth) {
        drawUiText(
            "REPAIR  FULL HEALTH",
            {x + 24.0F, y + 399.0F}, 13.0F,
            {145, 218, 159, 225});
    }

    if (!snapshot.aimedBuildingUpgradeCost) {
        drawUiText("MAX LEVEL", {x + 24.0F, y + 424.0F},
                   19.0F, {245, 184, 76, 255});
        return;
    }

    drawUiText("UPGRADE", {x + 24.0F, y + 427.0F}, 16.0F,
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
            drawUiText(
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
        UiResourceIcon::Crystal, cost.gold, snapshot.gold);
}

struct HotbarSlot {
    std::string_view key;
    std::string_view label;
    ResourceCost cost;
    bool selected{};
    bool available{};
};

void drawHotbarSlots(
    GameUi& ui, std::span<const HotbarSlot> slots,
    float selectionPosition, float selectionAlpha,
    bool showSlotLabels = false,
    bool showSelectedInfo = true,
    std::string_view categoryLabel = {},
    float verticalOffset = 0.0F,
    float selectedInfoOffset = 0.0F) {
    constexpr float Gap = 6.0F;
    constexpr float MaximumSize = 54.0F;
    const float screenWidth =
        static_cast<float>(GetScreenWidth());
    const float availableWidth = screenWidth - 32.0F;
    const float gapWidth =
        Gap * static_cast<float>(slots.size() - 1U);
    const float slotSize = std::min(
        MaximumSize,
        (availableWidth - gapWidth) /
            static_cast<float>(slots.size()));
    const float totalWidth =
        slotSize * static_cast<float>(slots.size()) +
        gapWidth;
    const float startX =
        (screenWidth - totalWidth) * 0.5F;
    const float slotY = static_cast<float>(GetScreenHeight()) -
        slotSize - 12.0F + verticalOffset;
    if (!categoryLabel.empty()) {
        const float labelWidth =
            measureUiText(categoryLabel, 10.0F).x;
        const Rectangle badge{
            startX, slotY - 25.0F,
            labelWidth + 20.0F, 20.0F};
        DrawRectangleRounded(
            badge, 0.42F, 6,
            {22, 26, 33, 225});
        DrawRectangleLinesEx(
            badge, 1.0F,
            {213, 183, 119, 185});
        drawUiText(
            categoryLabel,
            {badge.x + 10.0F, badge.y + 4.0F},
            10.0F, {249, 225, 171, 245});
    }
    const auto centeredText =
        [](std::string_view text, float centerX, float textY,
           float fontSize, float maximumWidth, Color color) {
            const float fittedSize = fitUiTextSize(
                text, fontSize, 6.0F, maximumWidth);
            const float width =
                measureUiText(text, fittedSize).x;
            drawUiText(
                text, {centerX - width * 0.5F, textY},
                fittedSize, color);
        };
    for (std::size_t index = 0; index < slots.size();
         ++index) {
        const HotbarSlot& slot = slots[index];
        const float x =
            startX + static_cast<float>(index) *
                         (slotSize + Gap);
        const Rectangle bounds{x, slotY, slotSize, slotSize};
        ui.drawInsetPanel(bounds, slot.available ? 225 : 145);
        DrawRectangleLinesEx(
            bounds, slot.selected ? 2.5F : 1.0F,
            slot.available
                ? (slot.selected
                    ? Color{255, 224, 139, 255}
                    : Color{156, 143, 119, 190})
                : Color{83, 76, 69, 165});

        centeredText(
            slot.key, x + slotSize * 0.5F,
            slotY + slotSize * (showSlotLabels ? 0.10F : 0.28F),
            showSlotLabels ? 9.0F : 14.0F,
            slotSize - 8.0F,
            slot.selected
                ? Color{255, 239, 190, 255}
                : Color{245, 238, 220, 255});
        if (showSlotLabels) {
            centeredText(
                slot.label, x + slotSize * 0.5F,
                slotY + slotSize * 0.52F, 9.5F,
                slotSize - 8.0F,
                slot.selected
                    ? Color{255, 239, 197, 255}
                    : Color{224, 215, 195, 235});
        }
    }

    const auto selected = std::ranges::find_if(
        slots, &HotbarSlot::selected);
    if (showSelectedInfo && selected != slots.end()) {
        const std::string title{selected->label};
        const std::string cost = costText(selected->cost);
        const float titleWidth = measureUiText(title, 15.0F).x;
        const float costWidth = measureUiText(cost, 12.0F).x;
        const float infoWidth = std::max(titleWidth, costWidth) + 32.0F;
        const float infoX = (screenWidth - infoWidth) * 0.5F;
        const float infoY = slotY - 59.0F + selectedInfoOffset;
        DrawRectangleRounded(
            {infoX, infoY, infoWidth, 49.0F},
            0.22F, 6, {23, 20, 17, 218});
        drawUiText(
            title,
            {infoX + (infoWidth - titleWidth) * 0.5F,
             infoY + 5.0F},
            15.0F, {255, 225, 155, 255});
        drawUiText(
            cost,
            {infoX + (infoWidth - costWidth) * 0.5F,
             infoY + 27.0F},
            12.0F,
            selected->available
                ? Color{226, 218, 197, 245}
                : Color{242, 103, 83, 255});
    }

    if (selectionAlpha > 0.01F && !slots.empty()) {
        selectionPosition = std::clamp(
            selectionPosition, 0.0F,
            static_cast<float>(slots.size() - 1U));
        const float selectionX =
            startX + selectionPosition *
                         (slotSize + Gap);
        const auto alpha =
            [selectionAlpha](float value) {
                return static_cast<unsigned char>(
                    std::clamp(
                        value * selectionAlpha,
                        0.0F, 255.0F));
            };
        DrawRectangleLinesEx(
            {selectionX - 2.0F, slotY - 2.0F,
             slotSize + 4.0F, slotSize + 4.0F},
            2.5F,
            {255, 255, 246, alpha(255.0F)});
    }
}

void drawFoundationHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view) {
    constexpr std::array<std::string_view, 4> Labels{
        "FOUNDATION", "FLOOR", "WALL", "RAMP"};
    constexpr std::array<std::string_view, 4> Keys{
        "1", "2", "3", "4"};
    std::array<HotbarSlot, 4> slots{};
    for (std::size_t index = 0;
         index < slots.size(); ++index) {
        const ResourceCost cost =
            snapshot.modularBuildingCosts[index];
        slots[index] = {
            .key = Keys[index],
            .label = Labels[index],
            .cost = cost,
            .selected =
                view.selectedModularBuildPiece == index,
            .available =
                snapshot.unlimitedResources ||
                (snapshot.wood >= cost.wood &&
                 snapshot.stone >= cost.stone &&
                 snapshot.gold >= cost.gold),
        };
    }
    drawHotbarSlots(
        ui, slots,
        view.foundationHotbarSelectionPosition,
        view.foundationHotbarSelectionAlpha,
        false, true, "MODULAR");
}

void drawBuildHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view) {
    if (view.foundationBuildMode) {
        drawFoundationHotbar(ui, snapshot, view);
        return;
    }
    constexpr std::array<BuildingType, 13> Types{
        BuildingType::Core,     BuildingType::Wall,
        BuildingType::Turret,   BuildingType::GoldMine,
        BuildingType::Cannon,   BuildingType::SlowTrap,
        BuildingType::Gate,     BuildingType::LumberMill,
        BuildingType::Quarry,
        BuildingType::SpikeTrap,
        BuildingType::WoodStorage,
        BuildingType::StoneStorage,
        BuildingType::CrystalStorage,
    };
    constexpr std::array<const char*, 13> Keys{
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
        "SHIFT 1", "SHIFT 2", "SHIFT 3",
    };
    std::array<HotbarSlot, 13> slots{};
    for (std::size_t index = 0; index < Types.size();
         ++index) {
        const BuildingType type = Types[index];
        const ResourceCost cost =
            snapshot.buildingCosts[index];
        const bool selected =
            snapshot.selectedBuilding &&
            *snapshot.selectedBuilding == type;
        bool unlocked =
            type == BuildingType::Core
                ? snapshot.coreMaxHealth <= 0.0
                : snapshot.coreMaxHealth > 0.0;
        if (type == BuildingType::Cannon ||
            type == BuildingType::SlowTrap ||
            type == BuildingType::SpikeTrap ||
            type == BuildingType::LumberMill ||
            type == BuildingType::Quarry) {
            unlocked = unlocked && snapshot.coreLevel >= 2;
        }
        const bool affordable =
            snapshot.unlimitedResources ||
            (snapshot.wood >= cost.wood &&
             snapshot.stone >= cost.stone &&
             snapshot.gold >= cost.gold);
        const bool available = unlocked && affordable;
        slots[index] = {
            .key = Keys[index],
            .label = buildingDisplayName(type),
            .cost = cost,
            .selected = selected,
            .available = available,
        };
    }
    const std::span<const HotbarSlot> buildingSlots{
        slots.data(), 10U};
    const std::span<const HotbarSlot> storageSlots{
        slots.data() + 10U, 3U};
    const bool storageSelected =
        snapshot.selectedBuilding &&
        static_cast<std::size_t>(*snapshot.selectedBuilding) >= 10U;
    drawHotbarSlots(
        ui, buildingSlots,
        view.buildHotbarSelectionPosition,
        storageSelected ? 0.0F : view.buildHotbarSelectionAlpha,
        false, !storageSelected, {},
        -60.0F);
    drawHotbarSlots(
        ui, storageSlots,
        view.buildHotbarSelectionPosition - 10.0F,
        storageSelected ? view.buildHotbarSelectionAlpha : 0.0F,
        false, storageSelected, {}, 0.0F, -60.0F);
}

void drawLootInventory(
    GameUi& ui, const SimulationSnapshot& snapshot,
    bool expanded, bool coreVisible) {
    constexpr std::array<LootUpgradeEffect, LootUpgradeEffectCount> Effects{
        LootUpgradeEffect::Damage,
        LootUpgradeEffect::MoveSpeed,
        LootUpgradeEffect::MaximumHealth,
        LootUpgradeEffect::Apple,
        LootUpgradeEffect::Bread,
        LootUpgradeEffect::IronBar,
        LootUpgradeEffect::FuelJerrycan,
        LootUpgradeEffect::Compass,
        LootUpgradeEffect::Nail,
        LootUpgradeEffect::Key,
        LootUpgradeEffect::Map,
        LootUpgradeEffect::Anvil,
        LootUpgradeEffect::Saw,
        LootUpgradeEffect::Potion,
    };
    int activeCount = 0;
    for (const LootUpgradeEffect effect : Effects) {
        if (snapshot.lootStacks[lootUpgradeIndex(effect)] > 0) {
            ++activeCount;
        }
    }
    if (activeCount == 0) return;

    constexpr float PanelX = 12.0F;
    constexpr float SlotSize = 46.0F;
    constexpr float Gap = 6.0F;
    constexpr int Columns = 7;
    const int shownCount = expanded
        ? activeCount
        : std::min(activeCount, 6);
    const int visibleColumns = std::min(shownCount, Columns);
    const int rows = (shownCount + Columns - 1) / Columns;
    const float PanelY = expanded
        ? 548.0F
        : static_cast<float>(GetScreenHeight()) -
              (coreVisible ? 204.0F : 148.0F);
    const float panelWidth = 20.0F +
        static_cast<float>(visibleColumns) * (SlotSize + Gap);
    const float panelHeight = (expanded ? 36.0F : 20.0F) +
        static_cast<float>(rows) * (SlotSize + Gap);
    if (expanded) {
        ui.drawPanel({PanelX, PanelY, panelWidth, panelHeight}, 210);
        drawUiText("ITEMS", {PanelX + 10.0F, PanelY + 7.0F},
                   10.0F, {218, 203, 173, 235});
    }

    const Vector2 mouse = GetMousePosition();
    std::optional<LootUpgradeEffect> hovered;
    int hoveredStacks = 0;
    int slotIndex = 0;
    for (const LootUpgradeEffect effect : Effects) {
        const int stacks = snapshot.lootStacks[lootUpgradeIndex(effect)];
        if (stacks <= 0) continue;
        if (slotIndex >= shownCount) break;
        const int column = slotIndex % Columns;
        const int row = slotIndex / Columns;
        const Rectangle slot{
            PanelX + 10.0F +
                static_cast<float>(column) * (SlotSize + Gap),
            PanelY + (expanded ? 27.0F : 5.0F) +
                static_cast<float>(row) * (SlotSize + Gap),
            SlotSize, SlotSize};
        ++slotIndex;
        DrawRectangleRounded(slot, 0.22F, 5, {24, 32, 35, 235});
        DrawRectangleLinesEx(slot, 2.0F, {75, 184, 225, 245});
        const Vector2 center{slot.x + slot.width * 0.5F,
                             slot.y + slot.height * 0.5F};
        if (effect == LootUpgradeEffect::Apple) {
            DrawCircleV({center.x, center.y + 3.0F}, 12.0F,
                        {219, 65, 58, 255});
            DrawRectangleRec({center.x - 1.5F, center.y - 14.0F,
                              3.0F, 7.0F}, {85, 58, 35, 255});
            DrawEllipse(static_cast<int>(center.x + 5.0F),
                        static_cast<int>(center.y - 11.0F),
                        6.0F, 3.5F, {91, 188, 82, 255});
        } else if (effect == LootUpgradeEffect::Bread) {
            const Rectangle loaf{center.x - 15.0F, center.y - 9.0F,
                                 30.0F, 21.0F};
            DrawRectangleRounded(loaf, 0.42F, 6,
                                 {226, 165, 73, 255});
            for (int cut = 0; cut < 3; ++cut) {
                const float x = loaf.x + 8.0F +
                    static_cast<float>(cut) * 7.0F;
                DrawLineEx({x, loaf.y + 3.0F},
                           {x - 3.0F, loaf.y + 9.0F},
                           2.0F, {151, 94, 44, 220});
            }
        } else if (effect == LootUpgradeEffect::IronBar) {
            DrawRectangleRounded(
                {center.x - 14.0F, center.y - 7.0F,
                 28.0F, 14.0F}, 0.2F, 5,
                {172, 187, 196, 255});
            DrawLineEx(
                {center.x - 8.0F, center.y - 2.0F},
                {center.x + 8.0F, center.y + 2.0F},
                2.0F, {93, 108, 117, 255});
        } else if (effect == LootUpgradeEffect::FuelJerrycan) {
            DrawRectangleRounded(
                {center.x - 10.0F, center.y - 13.0F,
                 20.0F, 25.0F}, 0.2F, 5,
                {209, 151, 66, 255});
            DrawRectangleLinesEx(
                {center.x - 5.0F, center.y - 16.0F,
                 10.0F, 6.0F}, 2.0F, {104, 75, 40, 255});
        } else if (effect == LootUpgradeEffect::Compass) {
            DrawCircleV(center, 13.0F, {171, 129, 75, 255});
            DrawCircleLinesV(center, 13.0F, {240, 218, 155, 255});
            DrawTriangle(
                {center.x, center.y - 10.0F},
                {center.x - 4.0F, center.y + 7.0F},
                {center.x + 4.0F, center.y + 7.0F},
                {219, 80, 62, 255});
        } else if (effect == LootUpgradeEffect::Nail) {
            DrawLineEx(
                {center.x - 10.0F, center.y + 9.0F},
                {center.x + 8.0F, center.y - 9.0F},
                4.0F, {205, 211, 212, 255});
            DrawCircleV(
                {center.x - 10.0F, center.y + 9.0F},
                4.0F, {117, 127, 132, 255});
        } else if (effect == LootUpgradeEffect::Key) {
            DrawCircleLinesV(
                {center.x - 7.0F, center.y - 4.0F},
                6.0F, {235, 198, 83, 255});
            DrawLineEx(
                {center.x - 1.0F, center.y + 1.0F},
                {center.x + 12.0F, center.y + 12.0F},
                4.0F, {235, 198, 83, 255});
        } else if (effect == LootUpgradeEffect::Map) {
            DrawRectangleRec(
                {center.x - 14.0F, center.y - 10.0F,
                 28.0F, 20.0F}, {190, 157, 101, 255});
            DrawLineEx(
                {center.x - 3.0F, center.y - 10.0F},
                {center.x - 3.0F, center.y + 10.0F},
                2.0F, {111, 83, 54, 255});
            DrawLineEx(
                {center.x + 5.0F, center.y - 10.0F},
                {center.x + 5.0F, center.y + 10.0F},
                2.0F, {111, 83, 54, 255});
        } else if (effect == LootUpgradeEffect::Anvil) {
            DrawRectangleRec(
                {center.x - 14.0F, center.y - 8.0F,
                 28.0F, 10.0F}, {159, 169, 177, 255});
            DrawRectangleRec(
                {center.x - 6.0F, center.y + 2.0F,
                 12.0F, 10.0F}, {116, 126, 134, 255});
        } else if (effect == LootUpgradeEffect::Saw) {
            DrawLineEx(
                {center.x - 14.0F, center.y + 7.0F},
                {center.x + 12.0F, center.y - 7.0F},
                4.0F, {209, 214, 215, 255});
            for (int tooth = 0; tooth < 4; ++tooth) {
                const float x = center.x - 8.0F +
                    static_cast<float>(tooth) * 6.0F;
                DrawLineEx(
                    {x, center.y + 4.0F},
                    {x + 3.0F, center.y + 9.0F},
                    1.5F, {122, 130, 133, 255});
            }
        } else if (effect == LootUpgradeEffect::Potion) {
            DrawRectangleRounded(
                {center.x - 9.0F, center.y - 8.0F,
                 18.0F, 19.0F}, 0.3F, 5,
                {190, 70, 102, 255});
            DrawRectangleRec(
                {center.x - 5.0F, center.y - 14.0F,
                 10.0F, 7.0F}, {207, 202, 185, 255});
        } else {
            const std::string initial(1, lootUpgradeName(effect)[0]);
            const Vector2 size = measureUiText(initial, 20.0F);
            drawUiText(initial,
                       {center.x - size.x * 0.5F,
                        center.y - size.y * 0.5F},
                       20.0F, {236, 226, 196, 255});
        }
        const std::string badge = "x" + std::to_string(stacks);
        const Vector2 badgeSize = measureUiText(badge, 9.0F);
        const Rectangle badgeBounds{
            slot.x + slot.width - badgeSize.x - 8.0F,
            slot.y + slot.height - 17.0F,
            badgeSize.x + 7.0F, 15.0F};
        DrawRectangleRounded(badgeBounds, 0.45F, 5,
                             {13, 18, 20, 245});
        drawUiText(badge, {badgeBounds.x + 3.0F, badgeBounds.y - 1.0F},
                   9.0F, RAYWHITE);
        if (CheckCollisionPointRec(mouse, slot)) {
            hovered = effect;
            hoveredStacks = stacks;
            DrawRectangleLinesEx(slot, 3.0F, {213, 246, 255, 255});
        }
    }

    if (!expanded && activeCount > shownCount) {
        drawUiText(
            "+" + std::to_string(activeCount - shownCount),
            {PanelX + panelWidth + 2.0F, PanelY + 18.0F},
            12.0F, {218, 203, 173, 235});
    }

    if (hovered) {
        const std::string tooltipTitle =
            lootUpgradeName(*hovered) +
            std::string("  x") +
            std::to_string(hoveredStacks);
        const std::string tooltipDescription =
            lootUpgradeDescription(*hovered);
        const float tooltipX = PanelX + panelWidth + 8.0F;
        const float availableTooltipWidth = std::max(
            180.0F,
            static_cast<float>(GetScreenWidth()) -
                tooltipX - 12.0F);
        const float maximumTooltipWidth =
            std::min(520.0F, availableTooltipWidth);
        const float tooltipWidth = std::clamp(
            std::max(
                measureUiText(tooltipTitle, 13.0F).x,
                measureUiText(tooltipDescription, 10.0F).x) +
                24.0F,
            std::min(260.0F, maximumTooltipWidth),
            maximumTooltipWidth);
        const Rectangle tooltip{
            tooltipX, PanelY, tooltipWidth, 66.0F};
        ui.drawPanel(tooltip, 238);
        drawUiText(tooltipTitle,
                   {tooltip.x + 12.0F, tooltip.y + 9.0F},
                   fitUiTextSize(
                       tooltipTitle, 13.0F, 9.0F,
                       tooltip.width - 24.0F),
                   {122, 218, 255, 255});
        drawUiText(tooltipDescription,
                   {tooltip.x + 12.0F, tooltip.y + 36.0F},
                   fitUiTextSize(
                       tooltipDescription, 10.0F, 7.0F,
                       tooltip.width - 24.0F),
                   {230, 224, 207, 245});
    }
}

const char* weaponLabel(PlayerWeapon weapon) {
    switch (weapon) {
    case PlayerWeapon::BareHands: return "BARE HANDS";
    case PlayerWeapon::Axe: return "AXE";
    case PlayerWeapon::Pickaxe: return "PICKAXE";
    case PlayerWeapon::Club: return "CLUB";
    case PlayerWeapon::IceWand: return "ICE WAND";
    case PlayerWeapon::Hammer: return "HAMMER";
    case PlayerWeapon::Rifle: return "RIFLE";
    }
    return "WEAPON";
}

void drawCompactInsight(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view, bool buildModeActive) {
    const float width = std::clamp(
        static_cast<float>(GetScreenWidth()) * 0.30F,
        320.0F, 470.0F);
    const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
    const float y = static_cast<float>(GetScreenHeight()) -
        (buildModeActive ? 157.0F : 91.0F);
    const float requirement = std::max(
        1.0F, static_cast<float>(snapshot.requiredInsight));
    const float fraction = std::clamp(
        static_cast<float>(view.displayedInsight) / requirement,
        0.0F, 1.0F);
    const float pulse = std::clamp(
        static_cast<float>(view.insightPulse), 0.0F, 1.0F);
    const float pointPulse = std::clamp(
        static_cast<float>(view.treePointPulse), 0.0F, 1.0F);
    if (pulse > 0.01F || pointPulse > 0.01F) {
        DrawRectangleRounded(
            {centerX - width * 0.5F - 5.0F - pointPulse * 4.0F,
             y - 5.0F - pointPulse * 4.0F,
             width + 10.0F + pointPulse * 8.0F,
             22.0F + pointPulse * 8.0F},
            0.45F, 7,
            {151, 112, 255,
             static_cast<unsigned char>(55.0F +
                 95.0F * std::max(pulse, pointPulse))});
    }
    ui.drawProgressBar(
        {centerX - width * 0.5F, y, width, 12.0F},
        fraction, UiBarColor::Purple);
    const std::string value = std::to_string(static_cast<int>(
        std::floor(std::max(0.0, view.displayedInsight)))) +
        " / " + std::to_string(static_cast<int>(
            std::ceil(snapshot.requiredInsight)));
    const float valueWidth = measureUiText(value, 10.0F).x;
    drawUiText(
        value, {centerX + width * 0.5F - valueWidth, y - 18.0F},
        10.0F, {224, 211, 251, 245});
    drawUiText("INSIGHT", {centerX - width * 0.5F, y - 20.0F}, 11.0F,
               {208, 187, 245, 245});
    const float pointCenterX = centerX - width * 0.5F + 91.0F;
    DrawPoly({pointCenterX, y - 12.5F - pointPulse * 2.0F}, 4,
             5.0F + pointPulse * 1.5F, 45.0F,
             {208, 177, 255, 255});
    const std::string points = std::to_string(snapshot.skillPoints);
    drawUiText(
        points,
        {pointCenterX + 9.0F, y - 20.0F - pointPulse * 2.0F},
        13.0F + pointPulse * 2.0F, {208, 177, 255, 255});
    if (view.insightGainRemaining > 0.0 &&
        view.insightGainAmount > 0.0) {
        const float gainProgress = static_cast<float>(std::clamp(
            1.0 - view.insightGainRemaining /
                      std::max(0.001, view.insightGainDuration),
            0.0, 1.0));
        const std::string gain = "+" + std::to_string(
            static_cast<int>(std::lround(view.insightGainAmount))) +
            " INSIGHT";
        const float gainWidth = measureUiText(gain, 11.0F).x;
        drawUiText(
            gain,
            {centerX - gainWidth * 0.5F,
             y - 38.0F - gainProgress * 7.0F},
            11.0F,
            {218, 193, 255,
             static_cast<unsigned char>((1.0F - gainProgress) * 255.0F)});
    }
}

void drawWeaponHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view) {
    std::array<HotbarSlot, PlayerWeaponCount> slots{};
    std::array<std::string, PlayerWeaponCount> keys{};
    const std::span<const PlayerWeapon> order =
        view.actionMode == ActionMode::Tools
            ? std::span<const PlayerWeapon>{PlayerToolHotbarOrder}
            : std::span<const PlayerWeapon>{PlayerCombatHotbarOrder};
    std::size_t visibleCount = 0;
    for (const PlayerWeapon weapon : order) {
        if (snapshot.unlockedWeapons[static_cast<std::size_t>(weapon)]) {
            keys[visibleCount] = std::to_string(visibleCount + 1U);
            slots[visibleCount] = {
                .key = keys[visibleCount],
                .label = weapon == PlayerWeapon::BareHands
                    ? "HANDS"
                    : weaponLabel(weapon),
                .cost = {},
                .selected = snapshot.selectedWeapon == weapon,
                .available = true,
            };
            ++visibleCount;
        }
    }
    drawHotbarSlots(
        ui, std::span<const HotbarSlot>{slots.data(), visibleCount},
        view.weaponHotbarSelectionPosition,
        view.weaponHotbarSelectionAlpha,
        true, false, actionModeLabel(view.actionMode));
}

std::string phaseClock(double seconds) {
    const int total = std::max(0, static_cast<int>(std::ceil(seconds)));
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", total / 60,
                  total % 60);
    return buffer;
}

} // namespace

void drawHud(GameUi& ui, const SimulationSnapshot& snapshot,
             const HudViewState& view, const Camera3D& camera,
             const ControlSettings& controls) {
    const float woodY = -view.woodResourceBounce;
    const float stoneY = -view.stoneResourceBounce;
    const float goldY = -view.goldResourceBounce;
    const float coinY = -view.coinResourceBounce;
    constexpr float ResourcePanelWidth = 454.0F;
    ui.drawPanel(
        {12.0F, 12.0F, ResourcePanelWidth, 46.0F}, 155);
    ui.drawResourceIcon({19.0F, 18.0F + woodY, 30.0F, 30.0F},
                        UiResourceIcon::Wood);
    drawUiText(snapshot.unlimitedResources
                   ? "∞"
                   : compactAmount(snapshot.wood) + "/" +
                         compactAmount(snapshot.woodCapacity),
               {51.0F, 22.0F + woodY},
               15.0F, RAYWHITE);
    ui.drawResourceIcon({125.0F, 18.0F + stoneY, 30.0F, 30.0F},
                        UiResourceIcon::Stone);
    drawUiText(snapshot.unlimitedResources
                   ? "∞"
                   : compactAmount(snapshot.stone) + "/" +
                         compactAmount(snapshot.stoneCapacity),
               {157.0F, 22.0F + stoneY},
               15.0F, RAYWHITE);
    ui.drawResourceIcon({231.0F, 18.0F + goldY, 30.0F, 30.0F},
                        UiResourceIcon::Crystal);
    drawUiText(snapshot.unlimitedResources
                   ? "∞"
                   : compactAmount(snapshot.gold) + "/" +
                         compactAmount(snapshot.goldCapacity),
               {263.0F, 22.0F + goldY},
               15.0F, RAYWHITE);
    ui.drawResourceIcon({367.0F, 18.0F + coinY, 30.0F, 30.0F},
                        UiResourceIcon::Gold);
    drawUiText(snapshot.unlimitedResources
                   ? "∞"
                   : compactAmount(snapshot.coins),
               {399.0F, 22.0F + coinY},
               15.0F, {255, 236, 152, 255});
    const auto drawResourcePulse =
        [](Rectangle bounds, float remaining) {
            if (remaining <= 0.0F) {
                return;
            }
            const float progress =
                1.0F - std::clamp(remaining, 0.0F, 1.0F);
            const float pulse =
                std::sin(progress * PI);
            const auto alpha = static_cast<unsigned char>(
                std::lround(pulse * 245.0F));
            DrawRectangleLinesEx(
                bounds, 3.0F + pulse * 2.0F,
                {255, 236, 160, alpha});
        };
    drawResourcePulse(
        {16.0F, 16.0F, 103.0F, 38.0F},
        view.woodResourcePulse);
    drawResourcePulse(
        {122.0F, 16.0F, 103.0F, 38.0F},
        view.stoneResourcePulse);
    drawResourcePulse(
        {228.0F, 16.0F, 133.0F, 38.0F},
        view.goldResourcePulse);
    drawResourcePulse(
        {364.0F, 16.0F, 85.0F, 38.0F},
        view.coinResourcePulse);

    const float healthY = static_cast<float>(GetScreenHeight()) -
        (view.showCoreHealth ? 132.0F : 76.0F);
    const double healthFraction =
        snapshot.playerHealth / snapshot.playerMaxHealth;
    drawUiText(
        "YOU  " +
            std::to_string(static_cast<int>(snapshot.playerHealth)) +
            " / " +
            std::to_string(static_cast<int>(snapshot.playerMaxHealth)),
        {18.0F, healthY}, 13.0F,
        healthFraction > 0.3 ? Color{176, 225, 179, 255}
                             : Color{240, 116, 98, 255});
    ui.drawProgressBar(
        {18.0F, healthY + 24.0F, 244.0F, 13.0F},
        static_cast<float>(healthFraction),
        healthFraction > 0.3 ? UiBarColor::Green : UiBarColor::Red);

    if (view.showCoreHealth && snapshot.coreMaxHealth > 0.0) {
        drawUiText(
            "CORE L" + std::to_string(snapshot.coreLevel) + "  " +
                std::to_string(static_cast<int>(snapshot.coreHealth)) +
                " / " +
                std::to_string(static_cast<int>(snapshot.coreMaxHealth)),
            {18.0F, healthY + 52.0F}, 13.0F,
            {232, 196, 123, 255});
        ui.drawProgressBar(
            {18.0F, healthY + 76.0F, 244.0F, 13.0F},
            static_cast<float>(snapshot.coreHealth /
                               snapshot.coreMaxHealth),
            UiBarColor::Yellow);
    }
    if (snapshot.unlimitedResources) {
        drawUiText("∞", {238.0F, healthY - 2.0F}, 18.0F,
                   {111, 220, 151, 255});
    }

    int objectiveCount = 0;
    for (int index : snapshot.recommendedObjectives) {
        if (index >= 0 &&
            static_cast<std::size_t>(index) < snapshot.objectives.size())
            ++objectiveCount;
    }
    if (objectiveCount > 0 &&
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
        drawUiText(
            "OBJECTIVE TRACKER",
            {ObjectiveX + 43.0F, ObjectiveY + 9.0F},
            14.0F, {255, 225, 157, 255});
        drawUiText(
            "Complete tasks to earn Insight",
            {ObjectiveX + 43.0F, ObjectiveY + 27.0F},
            10.5F, {190, 178, 157, 225});
        const std::string activeCount =
            "TRACKED  " + std::to_string(objectiveCount);
        const float activeWidth =
            measureUiText(activeCount, 10.5F).x;
        drawUiText(
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
            drawUiText(
                eyebrow,
                {card.x + 44.0F, card.y + 8.0F},
                eyebrowSize, kindColor);
            const std::string reward = "+" +
                std::to_string(static_cast<int>(std::lround(
                    objective.definition.insightReward))) +
                " INSIGHT";
            const float rewardWidth =
                measureUiText(reward, 10.0F).x;
            drawUiText(
                reward,
                {card.x + card.width - rewardWidth - 12.0F,
                 card.y + 9.0F},
                10.0F, {226, 207, 255, 245});

            const std::string instruction =
                objectiveInstruction(objective.definition);
            const float instructionSize = fitUiTextSize(
                instruction, 14.0F, 10.5F,
                card.width - 142.0F, 22.0F);
            drawUiText(
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
            drawUiText(
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
    } else if (objectiveCount > 0) {
        const float screenMinimum = static_cast<float>(
            std::min(GetScreenWidth(), GetScreenHeight()));
        const float compactMapSize = std::clamp(
            screenMinimum * 0.18F, 132.0F, 176.0F);
        constexpr float TrackerWidth = 344.0F;
        constexpr float RowHeight = 43.0F;
        const float trackerX = static_cast<float>(GetScreenWidth()) -
            TrackerWidth - 12.0F;
        const float trackerY = view.minimapHidden
            ? 18.0F
            : 30.0F + compactMapSize;
        drawUiText(
            "OBJECTIVES",
            {trackerX + 7.0F, trackerY},
            11.0F, {226, 208, 170, 225});
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
            const float rowY = trackerY + 22.0F +
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
                 static_cast<unsigned char>(pulsing ? 218 : 152)});
            drawObjectiveKindIcon(
                objective.definition.kind,
                {trackerX + 16.0F, rowY + 18.0F}, color);
            const std::string instruction =
                objectiveInstruction(objective.definition);
            const float instructionSize = fitUiTextSize(
                instruction, 11.5F, 8.5F, 210.0F, 18.0F);
            drawUiText(
                instruction,
                {trackerX + 32.0F, rowY + 6.0F},
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
                measureUiText(progress, 11.0F).x;
            const float rewardWidth =
                measureUiText(reward, 9.0F).x;
            drawUiText(
                progress,
                {trackerX + TrackerWidth - progressWidth - 9.0F,
                 rowY + 4.0F},
                11.0F, {255, 235, 176, 255});
            drawUiText(
                reward,
                {trackerX + TrackerWidth - rewardWidth - 22.0F,
                 rowY + 22.0F},
                9.0F, {195, 174, 245, 235});
            DrawPoly(
                {trackerX + TrackerWidth - 10.0F, rowY + 29.0F},
                4, 4.0F, 45.0F, {195, 174, 245, 235});
            const float fraction = objective.definition.target > 0.0
                ? std::clamp(static_cast<float>(
                      objective.progress /
                      objective.definition.target), 0.0F, 1.0F)
                : 0.0F;
            DrawRectangleRounded(
                {trackerX + 32.0F, rowY + 29.0F,
                 210.0F, 5.0F},
                0.8F, 3, {47, 41, 35, 210});
            DrawRectangleRounded(
                {trackerX + 32.0F, rowY + 29.0F,
                 210.0F * fraction, 5.0F},
                0.8F, 3, color);
            ++row;
        }
    }

    drawLootInventory(
        ui, snapshot, view.informationExpansion > 0.65F,
        view.showCoreHealth);

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
                      82.0F, compassWidth, 31.0F}, 188);
        drawCenteredUiText(
            compassText, 89.0F, 13.0F,
            {255, 215, 116, 255});
        chestCompassVisible = true;
    }

    std::string phaseText;
    Color phaseColor{245, 184, 76, 255};
    bool importantPhase = false;
    if (snapshot.state == RunState::BuildPhase) {
        phaseText = "TO SUNSET  " +
            phaseClock(snapshot.phaseTimeRemaining);
    } else if (snapshot.state == RunState::Sunset) {
        phaseText = "SUNSET  •  WAVE " +
            std::to_string(snapshot.wave + 1) + " IN " +
            phaseClock(snapshot.phaseTimeRemaining);
        phaseColor = {255, 146, 79, 255};
        importantPhase = true;
    } else if (snapshot.state == RunState::Wave) {
        phaseText = "WAVE " + std::to_string(snapshot.wave) +
            "     " +
            std::to_string(snapshot.activeEnemyCount +
                           snapshot.pendingEnemyCount) +
            " ENEMIES";
        phaseColor = {242, 108, 82, 255};
        importantPhase = true;
        const bool bossCharging = std::any_of(
            snapshot.enemies.begin(), snapshot.enemies.end(),
            [](const EnemyInstance& enemy) {
                return enemy.active &&
                       enemy.state == EnemyState::BossRamWindup;
            });
        if (bossCharging) {
            phaseText += "  •  BOSS RAM";
        }
    } else if (snapshot.state == RunState::WaveComplete) {
        phaseText = "DAWN  •  DAY IN " +
            phaseClock(snapshot.phaseTimeRemaining);
        phaseColor = {255, 194, 92, 255};
        importantPhase = true;
    }
    if (!phaseText.empty()) {
        const float textWidth = measureUiText(phaseText, 16.0F).x;
        const float centerX = static_cast<float>(GetScreenWidth()) * 0.5F;
        if (importantPhase) {
            DrawRectangleRounded(
                {centerX - textWidth * 0.5F - 18.0F,
                 12.0F, textWidth + 36.0F, 36.0F},
                0.34F, 7, {25, 20, 18, 188});
        }
        drawCenteredUiText(phaseText, 20.0F, 16.0F, phaseColor);
    }

    if (snapshot.state == RunState::Sunset &&
        snapshot.upcomingAttackDirection &&
        snapshot.phaseDuration > 0.0) {
        const double elapsed =
            snapshot.phaseDuration - snapshot.phaseTimeRemaining;
        if (elapsed < 3.2) {
            const float fade = std::clamp(
                static_cast<float>((3.2 - elapsed) / 0.65),
                0.0F, 1.0F);
            const std::string warning =
                "ATTACK FROM " + std::string(attackDirectionName(
                    *snapshot.upcomingAttackDirection));
            const float warningWidth =
                measureUiText(warning, 32.0F).x;
            DrawRectangleRounded(
                {static_cast<float>(GetScreenWidth()) * 0.5F -
                     warningWidth * 0.5F - 28.0F,
                 104.0F, warningWidth + 56.0F, 66.0F},
                0.22F, 8,
                {32, 10, 9,
                 static_cast<unsigned char>(fade * 190.0F)});
            drawCenteredUiText(
                warning, 113.0F, 32.0F,
                {255, 134, 88,
                 static_cast<unsigned char>(fade * 255.0F)});
        }
    }

    const std::string objective = tutorialText(snapshot);
    if (!objective.empty()) {
        const bool attackWarningVisible =
            snapshot.state == RunState::Sunset &&
            snapshot.upcomingAttackDirection &&
            snapshot.phaseDuration > 0.0 &&
            snapshot.phaseDuration - snapshot.phaseTimeRemaining < 3.2;
        drawCenteredUiText(
            objective,
            attackWarningVisible
                ? 184.0F
                : chestCompassVisible ? 125.0F : 91.0F,
            16.0F,
                           {255, 224, 146, 255});
    }

    const bool buildModeActive =
        view.actionMode == ActionMode::Buildings ||
        view.actionMode == ActionMode::Modular;
    drawCompactInsight(ui, snapshot, view, buildModeActive);
    if (!view.hideBottomHints && buildModeActive) {
        drawBuildHotbar(ui, snapshot, view);
    } else if (!view.hideBottomHints) {
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
        drawCenteredUiText(
            "REPAIR MODE", crosshairCenter.y + 42.0F,
            17.0F, {104, 232, 163, 255});
    }
    const Color crosshairColor =
        view.invalidActionRemaining > 0.0
            ? Color{255, 76, 62, 255}
            : view.crosshairHitRemaining > 0.0 &&
                      view.crosshairHitCritical
                  ? Color{255, 215, 62, 255}
                  : RAYWHITE;
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
        drawCenteredUiText(
            status, static_cast<float>(GetScreenHeight() / 2 + 78),
            20.0F, {255, 194, 92, 255});
    }

    if (snapshot.buildingPreview) {
        const BuildingPreview& preview = *snapshot.buildingPreview;
        const ResourceCost cost = preview.placement.cost;
        const std::string buildText =
            std::string(buildingDisplayName(preview.type)) +
            "  W:" + std::to_string(cost.wood) +
            " S:" + std::to_string(cost.stone) +
            " C:" + std::to_string(cost.gold);
        const Vec3 previewCenter = buildingWorldPosition(
            preview.type, preview.gridPosition);
        const Vector2 anchor = GetWorldToScreen(
            {static_cast<float>(previewCenter.x), 2.35F,
             static_cast<float>(previewCenter.z)},
            camera);
        constexpr float PanelWidth = 520.0F;
        constexpr float PanelHeight = 104.0F;
        const float panelX = std::clamp(
            anchor.x - PanelWidth * 0.5F, 18.0F,
            std::max(
                18.0F,
                static_cast<float>(GetScreenWidth()) -
                    PanelWidth - 18.0F));
        const float panelY = std::clamp(
            anchor.y - PanelHeight - 34.0F, 18.0F,
            std::max(
                18.0F,
                static_cast<float>(GetScreenHeight()) -
                    PanelHeight - 18.0F));
        ui.drawPanel(
            {panelX, panelY, PanelWidth, PanelHeight}, 236);
        drawUiText(
            buildText, {panelX + 18.0F, panelY + 14.0F},
            16.0F, RAYWHITE);
        drawUiText(
            placementMessage(preview.placement.error),
            {panelX + 18.0F, panelY + 55.0F}, 14.0F,
            placementMessageColor(
                preview.placement.error));
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
        drawCenteredUiText(
            "PAUSED", static_cast<float>(GetScreenHeight() / 2 - 24),
            48.0F, RAYWHITE);
    } else if (snapshot.state == RunState::Defeat) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      {0, 0, 0, 170});
        drawCenteredUiText(
            "CORE DESTROYED",
            static_cast<float>(GetScreenHeight() / 2 - 48), 48.0F,
            {235, 92, 72, 255});
        drawCenteredUiText(
            "R: restart", static_cast<float>(GetScreenHeight() / 2 + 24),
            24.0F, RAYWHITE);
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
        drawCenteredUiText(
            respawnText,
            static_cast<float>(panelY + 30), 34.0F,
            RAYWHITE);

        char lossText[128]{};
        std::snprintf(
            lossText, sizeof(lossText),
            "Lost  Wood %d   Stone %d   Crystals %d",
            snapshot.deathLostWood,
            snapshot.deathLostStone,
            snapshot.deathLostGold);
        drawCenteredUiText(
            lossText,
            static_cast<float>(panelY + 91), 19.0F,
            {235, 148, 112, 255});
    }
}

} // namespace ian
