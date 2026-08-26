#include "ui/HudHotbar.hpp"

#include "buildings/BuildingHotbarLayout.hpp"
#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudFormatting.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace ian::hud_detail {

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
    if (slots.empty()) return;

    constexpr float Gap = 7.0F;
    constexpr float MaximumSize = 58.0F;
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
            measureUiText(categoryLabel, 12.0F).x;
        const Rectangle badge{
            (screenWidth - labelWidth - 24.0F) * 0.5F,
            slotY - 25.0F,
            labelWidth + 24.0F, 24.0F};
        DrawRectangleRounded(
            badge, 0.42F, 6,
            {22, 26, 33, 225});
        DrawRectangleLinesEx(
            badge, 1.0F,
            {213, 183, 119, 185});
        drawUiText(
            categoryLabel,
            {badge.x + 12.0F, badge.y + 4.0F},
            12.0F, {249, 225, 171, 245});
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
            showSlotLabels ? 10.5F : 16.0F,
            slotSize - 8.0F,
            !slot.available
                ? Color{126, 121, 111, 205}
                : slot.selected
                ? Color{255, 239, 190, 255}
                : Color{245, 238, 220, 255});
        if (showSlotLabels) {
            centeredText(
                slot.label, x + slotSize * 0.5F,
                slotY + slotSize * 0.52F, 11.0F,
                slotSize - 8.0F,
                !slot.available
                    ? Color{112, 108, 101, 190}
                    : slot.selected
                    ? Color{255, 239, 197, 255}
                    : Color{224, 215, 195, 235});
        }
    }

    const auto selected = std::ranges::find_if(
        slots, &HotbarSlot::selected);
    if (showSelectedInfo && selected != slots.end()) {
        const std::string title{selected->label};
        const std::string cost = costText(selected->cost);
        const float titleWidth = measureUiText(title, 17.0F).x;
        const float costWidth = measureUiText(cost, 14.0F).x;
        const float infoWidth = std::max(titleWidth, costWidth) + 40.0F;
        const float infoX = (screenWidth - infoWidth) * 0.5F;
        const float infoY = slotY - 68.0F + selectedInfoOffset;
        DrawRectangleRounded(
            {infoX, infoY, infoWidth, 58.0F},
            0.22F, 6, {23, 20, 17, 218});
        drawUiText(
            title,
            {infoX + (infoWidth - titleWidth) * 0.5F,
             infoY + 6.0F},
            17.0F, {255, 225, 155, 255});
        drawUiText(
            cost,
            {infoX + (infoWidth - costWidth) * 0.5F,
             infoY + 32.0F},
            14.0F,
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
                canAfford(
                    cost, snapshot.wood,
                    snapshot.stone, snapshot.crystals),
        };
    }
    drawHotbarSlots(
        ui, slots,
        view.foundationHotbarSelectionPosition,
        view.foundationHotbarSelectionAlpha,
        false, true,
        "MODULAR  •  C CATEGORY  •  WHEEL ITEM");
}

void drawBuildHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view) {
    if (view.foundationBuildMode) {
        drawFoundationHotbar(ui, snapshot, view);
        return;
    }
    constexpr std::array<const char*, MaximumBuildingHotbarSlots> Keys{
        "1", "2", "3", "4", "5", "6",
    };
    const BuildingHotbarLayout layout =
        makeBuildingHotbarLayout(
            snapshot.unlockedBuildings,
            view.buildingHotbarCategory,
            snapshot.coreMaxHealth > 0.0,
            snapshot.coreLevel,
            snapshot.sandboxMode);
    std::vector<HotbarSlot> slots;
    slots.reserve(layout.count);
    for (std::size_t index = 0; index < layout.count; ++index) {
        const BuildingType type = layout.types[index];
        const std::size_t typeIndex = static_cast<std::size_t>(type);
        const ResourceCost cost =
            snapshot.buildingCosts[typeIndex];
        const bool selected =
            snapshot.selectedBuilding &&
            *snapshot.selectedBuilding == type;
        const bool affordable =
            snapshot.unlimitedResources ||
            canAfford(
                cost, snapshot.wood,
                snapshot.stone, snapshot.crystals);
        slots.push_back({
            .key = Keys[index],
            .label = buildingDisplayName(type),
            .cost = cost,
            .selected = selected,
            .available = affordable,
        });
    }
    const std::string categoryLabel =
        snapshot.coreMaxHealth <= 0.0
            ? "BASE"
            : std::string(buildingHotbarCategoryName(
                  view.buildingHotbarCategory)) +
                  "  •  C CATEGORY  •  WHEEL ITEM";
    drawHotbarSlots(
        ui, slots,
        view.buildHotbarSelectionPosition,
        view.buildHotbarSelectionAlpha,
        false, true, categoryLabel,
        0.0F, -28.0F);
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
        LootUpgradeEffect::Blueprint,
        LootUpgradeEffect::Hourglass,
        LootUpgradeEffect::Rope,
        LootUpgradeEffect::Magnet,
        LootUpgradeEffect::HealthAid,
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
    case PlayerWeapon::LegacyBareHands: return "REMOVED";
    case PlayerWeapon::Axe: return "AXE";
    case PlayerWeapon::Pickaxe: return "PICKAXE";
    case PlayerWeapon::Club: return "CLUB";
    case PlayerWeapon::IceWand: return "ICE WAND";
    case PlayerWeapon::FireWand: return "FIRE WAND";
    case PlayerWeapon::Hammer: return "HAMMER";
    case PlayerWeapon::Rifle: return "RIFLE";
    case PlayerWeapon::Bomb: return "BOMBS";
    }
    return "WEAPON";
}

void drawCompactInsight(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view, bool /*raised*/) {
    static_cast<void>(ui);
    const float pulse = std::clamp(
        static_cast<float>(view.insightPulse), 0.0F, 1.0F);
    const float pointPulse = std::clamp(
        static_cast<float>(view.treePointPulse), 0.0F, 1.0F);

    const float width = static_cast<float>(GetScreenWidth());
    constexpr float x = 0.0F;
    constexpr float y = 0.0F;
    constexpr float height = 22.0F;
    const float requirement = std::max(
        1.0F, static_cast<float>(snapshot.requiredInsight));
    const float fraction = std::clamp(
        static_cast<float>(view.displayedInsight) / requirement,
        0.0F, 1.0F);
    const float glow = std::max(pulse, pointPulse);
    constexpr float FrameThickness = 2.0F;
    constexpr float InnerInset = 3.0F;
    const Rectangle outerBounds{x, y, width, height};
    const Rectangle innerBounds{
        x + InnerInset, y + InnerInset,
        std::max(0.0F, width - InnerInset * 2.0F),
        height - InnerInset * 2.0F,
    };
    DrawRectangleRec(outerBounds, {2, 3, 5, 255});
    DrawRectangleGradientV(
        static_cast<int>(innerBounds.x),
        static_cast<int>(innerBounds.y),
        static_cast<int>(innerBounds.width * fraction),
        static_cast<int>(innerBounds.height),
        {21, 198, 239, 255}, {7, 166, 221, 255});
    if (glow > 0.01F) {
        DrawRectangleRec(
            {innerBounds.x, innerBounds.y,
             innerBounds.width * fraction, innerBounds.height},
            {118, 231, 255,
             static_cast<unsigned char>(glow * 125.0F)});
    }
    if (fraction > 0.001F && fraction < 0.999F) {
        const float fillEdge = innerBounds.x +
            innerBounds.width * fraction;
        DrawRectangleRec(
            {fillEdge - 1.0F, innerBounds.y, 2.0F,
             innerBounds.height},
            {104, 222, 249, 210});
    }
    DrawRectangleLinesEx(
        outerBounds, FrameThickness, {75, 78, 83, 255});
    DrawRectangleLinesEx(
        {x + FrameThickness, y + FrameThickness,
         width - FrameThickness * 2.0F,
         height - FrameThickness * 2.0F},
        1.0F, {17, 19, 22, 255});

    const float screenMinimum = static_cast<float>(
        std::min(GetScreenWidth(), GetScreenHeight()));
    const float minimapSize = std::clamp(
        screenMinimum * 0.16F, 120.0F, 156.0F);
    const std::string status =
        "LVL " + std::to_string(snapshot.playerLevel) +
        "  •  WAVE " +
        std::to_string(std::max(0, snapshot.wave));
    constexpr float statusFontSize = 14.85F;
    const Vector2 statusSize = measureUiText(status, statusFontSize);
    drawUiText(
        status,
        {width - minimapSize - 28.0F - statusSize.x,
         32.0F},
        statusFontSize, {246, 231, 255, 255});
}

void drawWeaponHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view) {
    std::array<HotbarSlot, PlayerWeaponCount> slots{};
    std::array<std::string, PlayerWeaponCount> keys{};
    std::array<std::string, PlayerWeaponCount> labels{};
    std::size_t slotCount = 0;
    for (const PlayerWeapon weapon : PlayerWeaponHotbarOrder) {
        const bool unlocked = snapshot.unlockedWeapons[
            static_cast<std::size_t>(weapon)];
        if (!unlocked) {
            continue;
        }
        keys[slotCount] = std::to_string(slotCount + 1U);
        labels[slotCount] = weapon == PlayerWeapon::Bomb && unlocked
                ? std::string("BOMBS x") +
                    (snapshot.unlimitedResources
                        ? "INF"
                        : std::to_string(snapshot.bombsRemaining)) +
                    (snapshot.selectedWeapon == PlayerWeapon::Bomb
                         ? " · V +" +
                               std::to_string(snapshot.bombPurchaseAmount) +
                               "/" +
                               std::to_string(snapshot.bombPurchaseCoinCost)
                         : "")
                : weaponLabel(weapon);
        slots[slotCount] = {
            .key = keys[slotCount],
            .label = labels[slotCount],
            .cost = {},
            .selected = snapshot.selectedWeapon == weapon,
            .available = true,
        };
        ++slotCount;
    }
    drawHotbarSlots(
        ui, std::span<const HotbarSlot>{slots.data(), slotCount},
        view.weaponHotbarSelectionPosition,
        view.weaponHotbarSelectionAlpha,
        true, false);
}


} // namespace ian::hud_detail
