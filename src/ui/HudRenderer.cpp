#include "ui/HudRenderer.hpp"

#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>

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

std::string tutorialText(const SimulationSnapshot& snapshot) {
    if (!snapshot.tutorialObjective) {
        return {};
    }
    switch (*snapshot.tutorialObjective) {
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
    const Camera3D& camera) {
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
    drawUiText("Q  COPY    F  REPAIR    U  UPGRADE",
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

struct BuildHotbarSlot {
    std::string_view key;
    std::string_view label;
    ResourceCost cost;
    bool selected{};
    bool available{};
};

void drawBuildHotbarSlots(
    GameUi& ui, const SimulationSnapshot& snapshot,
    std::span<const BuildHotbarSlot> slots,
    float selectionPosition, float selectionAlpha) {
    constexpr float Gap = 10.0F;
    constexpr float MaximumSize = 112.0F;
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
    const float slotY =
        static_cast<float>(GetScreenHeight()) -
        slotSize - 22.0F;
    const auto centeredText =
        [](std::string_view text, float centerX, float textY,
           float fontSize, Color color) {
            const float width =
                measureUiText(text, fontSize).x;
            drawUiText(
                text, {centerX - width * 0.5F, textY},
                fontSize, color);
        };
    constexpr std::array<UiResourceIcon, 3> CostIcons{
        UiResourceIcon::Wood,
        UiResourceIcon::Stone,
        UiResourceIcon::Crystal,
    };
    const std::array<int, 3> inventory{
        snapshot.wood, snapshot.stone, snapshot.gold};

    for (std::size_t index = 0; index < slots.size();
         ++index) {
        const BuildHotbarSlot& slot = slots[index];
        const float x =
            startX + static_cast<float>(index) *
                         (slotSize + Gap);
        const Rectangle bounds{x, slotY, slotSize, slotSize};
        ui.drawInsetPanel(
            bounds, slot.available ? 238 : 188);
        DrawRectangleLinesEx(
            bounds, 2.0F,
            slot.available
                ? Color{185, 169, 139, 215}
                : Color{112, 101, 91, 180});

        const float keySize = std::clamp(
            slotSize * 0.27F, 22.0F, 30.0F);
        const Rectangle keyBounds{
            x + 7.0F, slotY + 7.0F,
            keySize, keySize};
        DrawRectangleRounded(
            keyBounds, 0.22F, 4,
            slot.selected
                ? Color{240, 240, 232, 245}
                : Color{35, 31, 27, 225});
        centeredText(
            slot.key,
            keyBounds.x + keyBounds.width * 0.5F,
            keyBounds.y + 4.0F, 10.0F,
            slot.selected
                ? Color{38, 34, 29, 255}
                : Color{245, 238, 220, 255});
        centeredText(
            slot.label, x + slotSize * 0.5F,
            slotY + slotSize * 0.56F, 9.0F,
            slot.available
                ? Color{235, 222, 190, 235}
                : Color{132, 122, 110, 210});

        const std::array<int, 3> amounts{
            slot.cost.wood,
            slot.cost.stone,
            slot.cost.gold};
        const float iconSize =
            std::clamp(
                slotSize * 0.3F, 28.0F, 34.0F);
        constexpr float IconGap = 2.0F;
        const float iconRowWidth =
            iconSize * 3.0F + IconGap * 2.0F;
        const float iconRowX =
            x + (slotSize - iconRowWidth) * 0.5F;
        const float iconY =
            slotY - iconSize - 13.0F;
        DrawRectangleRounded(
            {x - 3.0F, iconY - 4.0F,
             slotSize + 6.0F, iconSize + 12.0F},
            0.2F, 5, Color{24, 21, 18, 205});
        for (std::size_t resource = 0;
             resource < CostIcons.size(); ++resource) {
            const float iconX =
                iconRowX +
                static_cast<float>(resource) *
                    (iconSize + IconGap);
            ui.drawResourceIcon(
                {iconX, iconY, iconSize, iconSize},
                CostIcons[resource]);
            const std::string amount =
                std::to_string(amounts[resource]);
            const float amountWidth =
                measureUiText(amount, 10.0F).x;
            const float badgeWidth =
                std::max(25.0F, amountWidth + 9.0F);
            const Rectangle badge{
                iconX + iconSize - badgeWidth + 4.0F,
                iconY + iconSize - 18.0F,
                badgeWidth, 21.0F};
            DrawRectangleRounded(
                badge, 0.45F, 5,
                Color{20, 18, 16, 245});
            drawUiText(
                amount,
                {badge.x +
                     (badge.width - amountWidth) * 0.5F,
                 badge.y - 1.0F},
                10.0F,
                snapshot.unlimitedResources ||
                        inventory[resource] >=
                            amounts[resource]
                    ? Color{244, 235, 214, 255}
                    : Color{242, 103, 83, 255});
        }
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
            {selectionX - 5.0F, slotY - 5.0F,
             slotSize + 10.0F, slotSize + 10.0F},
            8.0F,
            {255, 218, 132, alpha(42.0F)});
        DrawRectangleLinesEx(
            {selectionX - 2.0F, slotY - 2.0F,
             slotSize + 4.0F, slotSize + 4.0F},
            4.0F,
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
    std::array<BuildHotbarSlot, 4> slots{};
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
    drawBuildHotbarSlots(
        ui, snapshot, slots,
        view.foundationHotbarSelectionPosition,
        view.foundationHotbarSelectionAlpha);
}

void drawBuildHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view) {
    if (view.foundationBuildMode) {
        drawFoundationHotbar(ui, snapshot, view);
        return;
    }
    constexpr std::array<BuildingType, 9> Types{
        BuildingType::Core,     BuildingType::Wall,
        BuildingType::Turret,   BuildingType::GoldMine,
        BuildingType::Cannon,   BuildingType::SlowTrap,
        BuildingType::Gate,     BuildingType::LumberMill,
        BuildingType::Quarry,
    };
    constexpr std::array<const char*, 9> Keys{
        "1", "2", "3", "4", "5", "6", "7", "8", "9",
    };
    std::array<BuildHotbarSlot, 9> slots{};
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
    drawBuildHotbarSlots(
        ui, snapshot, slots,
        view.buildHotbarSelectionPosition,
        view.buildHotbarSelectionAlpha);
}

void drawMinimap(GameUi& ui, const SimulationSnapshot& snapshot,
                 float expansion) {
    const float rawExpansion = std::clamp(expansion, 0.0F, 1.0F);
    const float expanded =
        rawExpansion * rawExpansion * (3.0F - 2.0F * rawExpansion);
    const float screenMinimum = static_cast<float>(
        std::min(GetScreenWidth(), GetScreenHeight()));
    const float collapsedMapSize = std::clamp(
        screenMinimum * 0.28F, 164.0F, 228.0F);
    const float expandedMapSize = std::max(
        collapsedMapSize,
        std::min(
            static_cast<float>(GetScreenWidth()) * 0.68F,
            static_cast<float>(GetScreenHeight()) * 0.70F));
    const float mapSize = std::lerp(
        collapsedMapSize, expandedMapSize, expanded);
    const float panelPadding = std::lerp(13.0F, 20.0F, expanded);
    const float headerHeight = std::lerp(30.0F, 42.0F, expanded);
    const float panelWidth = mapSize + panelPadding * 2.0F;
    const float panelHeight =
        mapSize + headerHeight + panelPadding * 2.0F;
    const float collapsedPanelWidth =
        collapsedMapSize + 13.0F * 2.0F;
    const float collapsedPanelX =
        static_cast<float>(GetScreenWidth()) -
        collapsedPanelWidth - 12.0F;
    const float expandedPanelX =
        (static_cast<float>(GetScreenWidth()) - panelWidth) * 0.5F;
    const float expandedPanelY =
        static_cast<float>(GetScreenHeight()) * 0.5F -
        mapSize * 0.5F - panelPadding - headerHeight;
    const float panelX = std::lerp(
        collapsedPanelX, expandedPanelX, expanded);
    const float panelY = std::lerp(12.0F, expandedPanelY, expanded);
    const Rectangle mapBounds{
        panelX + panelPadding,
        panelY + panelPadding + headerHeight,
        mapSize, mapSize,
    };
    const float worldLimit = std::max(
        static_cast<float>(snapshot.worldLimit), 1.0F);
    const float mapScale = mapSize * 0.5F / worldLimit;

    if (expanded > 0.001F) {
        DrawRectangle(
            0, 0, GetScreenWidth(), GetScreenHeight(),
            {24, 11, 5,
             static_cast<unsigned char>(
                 std::lround(168.0F * expanded))});
    }
    ui.drawPanel(
        {panelX, panelY, panelWidth, panelHeight}, 218);
    drawUiText(
        snapshot.activeEnemyCount > 0U
            ? (expanded > 0.5F ? "TACTICAL MAP  •  " : "MAP  •  ") +
                  std::to_string(snapshot.activeEnemyCount) +
                  " HOSTILES"
            : expanded > 0.5F ? "TACTICAL MAP" : "MAP",
        {panelX + panelPadding,
         panelY + std::lerp(11.0F, 15.0F, expanded)},
        std::lerp(13.0F, 18.0F, expanded),
        snapshot.activeEnemyCount > 0U
            ? Color{246, 131, 95, 255}
            : Color{224, 205, 171, 255});

    DrawRectangleRec(mapBounds, {29, 43, 35, 238});
    DrawRectangleLinesEx(
        mapBounds, std::max(5.0F, mapSize * 0.045F),
        {61, 76, 58, 245});
    const Rectangle playableBounds{
        mapBounds.x + mapSize * 0.075F,
        mapBounds.y + mapSize * 0.075F,
        mapSize * 0.85F,
        mapSize * 0.85F,
    };
    DrawRectangleLinesEx(
        playableBounds, 1.0F, {116, 135, 91, 95});
    DrawLineEx(
        {mapBounds.x + mapSize * 0.5F, mapBounds.y},
        {mapBounds.x + mapSize * 0.5F,
         mapBounds.y + mapSize},
        1.0F, {214, 205, 169, 24});
    DrawLineEx(
        {mapBounds.x, mapBounds.y + mapSize * 0.5F},
        {mapBounds.x + mapSize,
         mapBounds.y + mapSize * 0.5F},
        1.0F, {214, 205, 169, 24});

    const float symbolScale = std::lerp(1.0F, 1.55F, expanded);
    const auto mapPoint =
        [mapBounds, mapScale](double worldX, double worldZ) {
            return Vector2{
                mapBounds.x + mapBounds.width * 0.5F +
                    static_cast<float>(worldX) * mapScale,
                mapBounds.y + mapBounds.height * 0.5F +
                    static_cast<float>(worldZ) * mapScale,
            };
        };

    BeginScissorMode(
        static_cast<int>(mapBounds.x),
        static_cast<int>(mapBounds.y),
        static_cast<int>(mapBounds.width),
        static_cast<int>(mapBounds.height));

    for (const ResourceNode& resource : snapshot.resourceNodes) {
        if (!resource.active) {
            continue;
        }
        const Vector2 point = mapPoint(
            resource.position.x, resource.position.z);
        DrawCircleV(
            point,
            (resource.type == ResourceType::Wood ? 1.35F : 1.2F) *
                symbolScale,
            resource.type == ResourceType::Wood
                ? Color{91, 143, 75, 125}
                : Color{143, 149, 145, 135});
    }

    const double cellSize = std::max(snapshot.worldCellSize, 0.01);
    const auto modularPoint =
        [&mapPoint, cellSize](GridCoord anchor,
                              double widthCells,
                              double depthCells) {
            return mapPoint(
                (static_cast<double>(anchor.x) + widthCells * 0.5) *
                    cellSize,
                (static_cast<double>(anchor.z) + depthCells * 0.5) *
                    cellSize);
        };
    for (const PlatformFrameInstance& frame : snapshot.platformFrames) {
        const Vector2 point = modularPoint(
            frame.anchor, PlatformFrameWidthCells,
            PlatformFrameWidthCells);
        DrawRectangleRec(
            {point.x - 1.7F * symbolScale,
             point.y - 1.7F * symbolScale,
             3.4F * symbolScale, 3.4F * symbolScale},
            {164, 144, 111, 185});
    }
    for (const WallInstance& wall : snapshot.modularWalls) {
        const Vector2 point = modularPoint(wall.anchor, 1.0, 1.0);
        const bool alongX =
            wall.rotation == Rotation::Deg0 ||
            wall.rotation == Rotation::Deg180;
        DrawRectangleRec(
            {point.x - (alongX ? 2.5F : 0.8F) * symbolScale,
             point.y - (alongX ? 0.8F : 2.5F) * symbolScale,
             (alongX ? 5.0F : 1.6F) * symbolScale,
             (alongX ? 1.6F : 5.0F) * symbolScale},
            {194, 160, 108, 210});
    }
    for (const RampInstance& ramp : snapshot.ramps) {
        const bool alongZ =
            ramp.rotation == Rotation::Deg0 ||
            ramp.rotation == Rotation::Deg180;
        const Vector2 point = modularPoint(
            ramp.anchor,
            alongZ ? ModularRampWidthCells : ModularRampRunCells,
            alongZ ? ModularRampRunCells : ModularRampWidthCells);
        DrawCircleV(
            point, 2.0F * symbolScale,
            {205, 174, 119, 190});
    }

    for (const BuildingInstance& building : snapshot.buildings) {
        const Vec3 position = buildingWorldPosition(building);
        const Vector2 point = mapPoint(position.x, position.z);
        switch (building.type) {
        case BuildingType::Core:
            DrawPoly(point, 4, 5.5F * symbolScale, 45.0F,
                     {255, 210, 83, 255});
            DrawCircleV(
                point, 1.7F * symbolScale,
                {255, 245, 188, 255});
            break;
        case BuildingType::Turret:
        case BuildingType::Cannon:
            DrawCircleV(
                point,
                (building.type == BuildingType::Cannon ? 3.5F : 3.0F) *
                    symbolScale,
                {238, 182, 89, 245});
            DrawCircleV(
                point, 1.1F * symbolScale,
                {73, 54, 38, 255});
            break;
        case BuildingType::Wall:
        case BuildingType::Gate:
            DrawRectangleRec(
                {point.x - 2.5F * symbolScale,
                 point.y - 1.4F * symbolScale,
                 5.0F * symbolScale, 2.8F * symbolScale},
                building.type == BuildingType::Gate
                    ? Color{231, 203, 131, 240}
                    : Color{188, 142, 86, 230});
            break;
        case BuildingType::SlowTrap:
            DrawRing(point, 2.1F * symbolScale,
                     3.0F * symbolScale, 0.0F, 360.0F, 12,
                     {102, 190, 220, 235});
            break;
        case BuildingType::GoldMine:
        case BuildingType::LumberMill:
        case BuildingType::Quarry: {
            Color color{120, 209, 218, 240};
            if (building.type == BuildingType::LumberMill) {
                color = {112, 184, 91, 240};
            } else if (building.type == BuildingType::Quarry) {
                color = {167, 174, 171, 240};
            }
            DrawRectangleRec(
                {point.x - 3.0F * symbolScale,
                 point.y - 3.0F * symbolScale,
                 6.0F * symbolScale, 6.0F * symbolScale},
                color);
            break;
        }
        }
    }

    for (const EnemyInstance& enemy : snapshot.enemies) {
        if (!enemy.active || enemy.state == EnemyState::Dead) {
            continue;
        }
        const Vector2 point = mapPoint(enemy.position.x, enemy.position.z);
        float radius = 2.25F;
        Color color{239, 75, 66, 245};
        if (enemy.type == EnemyType::Fast) {
            radius = 1.8F;
        } else if (enemy.type == EnemyType::Heavy) {
            radius = 2.8F;
        } else if (enemy.type == EnemyType::Boss) {
            radius = 4.6F;
            color = {255, 123, 55, 255};
        } else if (enemy.type == EnemyType::Flying) {
            color = {220, 105, 232, 250};
        }
        DrawCircleV(
            point, (radius + 1.0F) * symbolScale,
            {51, 16, 17, 210});
        DrawCircleV(point, radius * symbolScale, color);
    }

    const Vector2 player = mapPoint(
        snapshot.playerPosition.x, snapshot.playerPosition.z);
    const Vector2 direction{
        static_cast<float>(std::sin(snapshot.playerYaw)),
        static_cast<float>(-std::cos(snapshot.playerYaw)),
    };
    const Vector2 side{-direction.y, direction.x};
    const Color playerColor = snapshot.playerRespawning
        ? Color{174, 181, 181, 240}
        : Color{250, 250, 244, 255};
    const Vector2 outerTip{
        player.x + direction.x * 12.5F * symbolScale,
        player.y + direction.y * 12.5F * symbolScale,
    };
    const Vector2 outerBase{
        player.x + direction.x * 5.8F * symbolScale,
        player.y + direction.y * 5.8F * symbolScale,
    };
    const Vector2 outerLeft{
        outerBase.x - side.x * 5.2F * symbolScale,
        outerBase.y - side.y * 5.2F * symbolScale,
    };
    const Vector2 outerRight{
        outerBase.x + side.x * 5.2F * symbolScale,
        outerBase.y + side.y * 5.2F * symbolScale,
    };
    DrawTriangle(
        outerTip, outerLeft, outerRight,
        {30, 35, 38, 255});
    DrawCircleV(
        player, 7.2F * symbolScale,
        {30, 35, 38, 255});

    const Vector2 innerTip{
        player.x + direction.x * 10.5F * symbolScale,
        player.y + direction.y * 10.5F * symbolScale,
    };
    const Vector2 innerBase{
        player.x + direction.x * 5.8F * symbolScale,
        player.y + direction.y * 5.8F * symbolScale,
    };
    const Vector2 innerLeft{
        innerBase.x - side.x * 3.5F * symbolScale,
        innerBase.y - side.y * 3.5F * symbolScale,
    };
    const Vector2 innerRight{
        innerBase.x + side.x * 3.5F * symbolScale,
        innerBase.y + side.y * 3.5F * symbolScale,
    };
    DrawTriangle(
        innerTip, innerLeft, innerRight,
        playerColor);
    DrawCircleV(
        player, 5.2F * symbolScale,
        playerColor);

    if (snapshot.upcomingAttackDirection) {
        Vector2 marker{
            mapBounds.x + mapBounds.width * 0.5F,
            mapBounds.y + mapBounds.height * 0.5F,
        };
        Vector2 inward{};
        switch (*snapshot.upcomingAttackDirection) {
        case AttackDirection::North:
            marker.y = mapBounds.y + 5.0F;
            inward.y = 1.0F;
            break;
        case AttackDirection::East:
            marker.x = mapBounds.x + mapBounds.width - 5.0F;
            inward.x = -1.0F;
            break;
        case AttackDirection::South:
            marker.y = mapBounds.y + mapBounds.height - 5.0F;
            inward.y = -1.0F;
            break;
        case AttackDirection::West:
            marker.x = mapBounds.x + 5.0F;
            inward.x = 1.0F;
            break;
        }
        const Vector2 perpendicular{-inward.y, inward.x};
        DrawTriangle(
            {marker.x + inward.x * 7.0F,
             marker.y + inward.y * 7.0F},
            {marker.x - inward.x * 3.0F + perpendicular.x * 4.5F,
             marker.y - inward.y * 3.0F + perpendicular.y * 4.5F},
            {marker.x - inward.x * 3.0F - perpendicular.x * 4.5F,
             marker.y - inward.y * 3.0F - perpendicular.y * 4.5F},
            {255, 103, 64, 245});
    }

    EndScissorMode();
    DrawRectangleLinesEx(mapBounds, 2.0F, {196, 172, 126, 230});
    const float compassFontSize =
        std::lerp(15.0F, 24.0F, expanded);
    const float compassInset = compassFontSize * 0.72F;
    const auto drawCompassLabel =
        [compassFontSize](std::string_view label, Vector2 center) {
            const Vector2 size = measureUiText(label, compassFontSize);
            const Vector2 position{
                center.x - size.x * 0.5F,
                center.y - size.y * 0.5F,
            };
            drawUiText(
                label, {position.x + 1.5F, position.y + 1.5F},
                compassFontSize, {18, 22, 20, 235});
            drawUiText(
                label, position, compassFontSize,
                {248, 235, 201, 255});
        };
    drawCompassLabel(
        "N", {mapBounds.x + mapBounds.width * 0.5F,
              mapBounds.y + compassInset});
    drawCompassLabel(
        "E", {mapBounds.x + mapBounds.width - compassInset,
              mapBounds.y + mapBounds.height * 0.5F});
    drawCompassLabel(
        "S", {mapBounds.x + mapBounds.width * 0.5F,
              mapBounds.y + mapBounds.height - compassInset});
    drawCompassLabel(
        "W", {mapBounds.x + compassInset,
              mapBounds.y + mapBounds.height * 0.5F});
}

} // namespace

void drawMinimapHud(GameUi& ui, const SimulationSnapshot& snapshot,
                    float expansion) {
    drawMinimap(ui, snapshot, expansion);
}

void drawHud(GameUi& ui, const SimulationSnapshot& snapshot,
             const HudViewState& view, const Camera3D& camera) {
    const float woodY = -view.woodResourceBounce;
    const float stoneY = -view.stoneResourceBounce;
    const float goldY = -view.goldResourceBounce;
    ui.drawPanel({12.0F, 12.0F, 398.0F, 68.0F}, 218);
    ui.drawResourceIcon({22.0F, 20.0F + woodY, 48.0F, 48.0F},
                        UiResourceIcon::Wood);
    drawUiText(compactAmount(snapshot.wood),
               {76.0F, 28.0F + woodY},
               20.0F, RAYWHITE);
    ui.drawResourceIcon({144.0F, 20.0F + stoneY, 48.0F, 48.0F},
                        UiResourceIcon::Stone);
    drawUiText(compactAmount(snapshot.stone),
               {198.0F, 28.0F + stoneY},
               20.0F, RAYWHITE);
    ui.drawResourceIcon({276.0F, 20.0F + goldY, 48.0F, 48.0F},
                        UiResourceIcon::Crystal);
    drawUiText(compactAmount(snapshot.gold),
               {330.0F, 28.0F + goldY},
               20.0F, RAYWHITE);
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
        {17.0F, 17.0F, 116.0F, 58.0F},
        view.woodResourcePulse);
    drawResourcePulse(
        {139.0F, 17.0F, 126.0F, 58.0F},
        view.stoneResourcePulse);
    drawResourcePulse(
        {271.0F, 17.0F, 134.0F, 58.0F},
        view.goldResourcePulse);

    ui.drawPanel({12.0F, 90.0F, 314.0F, 158.0F}, 216);
    const double healthFraction =
        snapshot.playerHealth / snapshot.playerMaxHealth;
    drawUiText(
        "YOU  " +
            std::to_string(static_cast<int>(snapshot.playerHealth)) +
            " / " +
            std::to_string(static_cast<int>(snapshot.playerMaxHealth)),
        {26.0F, 104.0F}, 16.0F,
        healthFraction > 0.3 ? Color{176, 225, 179, 255}
                             : Color{240, 116, 98, 255});
    ui.drawProgressBar(
        {26.0F, 135.0F, 286.0F, 17.0F},
        static_cast<float>(healthFraction),
        healthFraction > 0.3 ? UiBarColor::Green : UiBarColor::Red);

    if (snapshot.coreMaxHealth > 0.0) {
        drawUiText(
            "CORE L" + std::to_string(snapshot.coreLevel) + "  " +
                std::to_string(static_cast<int>(snapshot.coreHealth)) +
                " / " +
                std::to_string(static_cast<int>(snapshot.coreMaxHealth)),
            {26.0F, 166.0F}, 16.0F,
            {232, 196, 123, 255});
        ui.drawProgressBar(
            {26.0F, 197.0F, 286.0F, 17.0F},
            static_cast<float>(snapshot.coreHealth /
                               snapshot.coreMaxHealth),
            UiBarColor::Yellow);
    } else {
        drawUiText("CORE NOT BUILT", {26.0F, 178.0F}, 16.0F,
                   {180, 154, 116, 255});
    }
    if (snapshot.unlimitedResources) {
        drawUiText("∞", {286.0F, 103.0F}, 20.0F,
                   {111, 220, 151, 255});
    }
    drawUiText(
        "WAVE " + std::to_string(snapshot.wave) +
            "   •   BEST " + std::to_string(snapshot.bestWave),
        {26.0F, 219.0F}, 12.0F,
        {190, 184, 169, 235});

    if (snapshot.state == RunState::BuildPhase ||
        snapshot.state == RunState::Sunset ||
        snapshot.state == RunState::Wave ||
        snapshot.state == RunState::WaveComplete) {
        ui.drawPanel(
            {static_cast<float>(GetScreenWidth()) * 0.5F - 235.0F,
             12.0F, 470.0F, 68.0F},
            210);
    }

    if (snapshot.state == RunState::BuildPhase) {
        const std::string text =
            "WAVE " + std::to_string(snapshot.wave + 1) + " IN " +
            std::to_string(
                static_cast<int>(snapshot.phaseTimeRemaining) + 1) +
            "   •   N START";
        drawCenteredUiText(text, 25.0F, 16.0F,
                           {245, 184, 76, 255});
    } else if (snapshot.state == RunState::Sunset) {
        std::string text =
            "SUNSET   •   WAVE " +
            std::to_string(snapshot.wave + 1) + " IN " +
            std::to_string(
                static_cast<int>(snapshot.phaseTimeRemaining) + 1);
        drawCenteredUiText(text, 25.0F, 16.0F,
                           {255, 146, 79, 255});
    } else if (snapshot.state == RunState::Wave) {
        std::string text =
            "WAVE " + std::to_string(snapshot.wave) +
            "   •   " +
            std::to_string(snapshot.activeEnemyCount) +
            " ACTIVE   •   " +
            std::to_string(snapshot.pendingEnemyCount);
        const bool bossCharging = std::any_of(
            snapshot.enemies.begin(), snapshot.enemies.end(),
            [](const EnemyInstance& enemy) {
                return enemy.active &&
                       enemy.state == EnemyState::BossRamWindup;
            });
        if (bossCharging) {
            text += "   BOSS RAM INCOMING";
        }
        drawCenteredUiText(text, 25.0F, 16.0F,
                           {235, 92, 72, 255});
    } else if (snapshot.state == RunState::WaveComplete) {
        const std::string text =
            "DAWN   +" +
            std::to_string(snapshot.waveCompletionReward) +
            " CRYSTALS   •   DAY IN " +
            std::to_string(
                static_cast<int>(snapshot.phaseTimeRemaining) + 1);
        drawCenteredUiText(text, 25.0F, 16.0F,
                           {255, 194, 92, 255});
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
            objective, attackWarningVisible ? 184.0F : 91.0F,
            16.0F,
                           {255, 224, 146, 255});
    }

    if (!view.hideBottomHints) {
        drawBuildHotbar(ui, snapshot, view);
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
                ui, snapshot, *aimed, view, camera);
        }
    } else if (snapshot.aimedEnemy) {
        drawCenteredUiText(
            "Attack", static_cast<float>(GetScreenHeight() / 2 + 24),
            18.0F, ORANGE);
    } else if (snapshot.aimedResource) {
        drawCenteredUiText(
            "Mine", static_cast<float>(GetScreenHeight() / 2 + 24),
            18.0F, YELLOW);
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
