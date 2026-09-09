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
#include <iterator>
#include <string>
#include <vector>

namespace ian::hud_detail {

struct HotbarSlot {
    std::string_view key;
    std::string_view label;
    ResourceCost cost;
    bool selected{};
    bool available{};
    bool upgradeAvailable{};
    Texture2D icon{};
};

void drawHotbarSlots(
    GameUi& ui, std::span<const HotbarSlot> slots,
    float selectionPosition, float selectionAlpha,
    bool showSlotLabels = false,
    bool showSelectedInfo = true,
    std::string_view categoryLabel = {},
    float verticalOffset = 0.0F,
    float selectedInfoOffset = 0.0F,
    double invalidActionRemaining = 0.0,
    bool showSwitchLabel = false) {
    if (slots.empty()) return;
    static_cast<void>(ui);

    const bool iconLayout = std::ranges::any_of(
        slots, [](const HotbarSlot& slot) {
            return IsTextureValid(slot.icon);
        });
    constexpr float Gap = 7.0F;
    constexpr float SlotSize = 96.0F;
    constexpr float SlotRadius = 15.0F;
    constexpr float KeyBadgeRadius = 14.45F;
    constexpr float KeyFontSize = 16.32F;
    constexpr float InnerSize = 86.0F;
    constexpr float InnerRadius = 11.0F;
    constexpr float SurfaceHeight = 82.0F;
    constexpr Color SlotColor{104, 60, 37, 255};
    constexpr Color ShadowColor{147, 115, 73, 255};
    constexpr Color SurfaceColor{230, 188, 147, 255};
    const float screenWidth =
        static_cast<float>(GetScreenWidth());
    const float gapWidth =
        Gap * static_cast<float>(slots.size() - 1U);
    const float totalWidth =
        SlotSize * static_cast<float>(slots.size()) +
        gapWidth;
    const float startX =
        (screenWidth - totalWidth) * 0.5F;
    const float slotY = static_cast<float>(GetScreenHeight()) -
        SlotSize - 20.0F + verticalOffset;
    selectionPosition = std::clamp(
        selectionPosition, 0.0F,
        static_cast<float>(slots.size() - 1U));
    selectionAlpha = std::clamp(selectionAlpha, 0.0F, 1.0F);
    const auto alpha =
        [](float value) {
            return static_cast<unsigned char>(std::clamp(
                value, 0.0F, 255.0F));
        };
    const auto mixColor =
        [&alpha](Color from, Color to, float amount) {
            amount = std::clamp(amount, 0.0F, 1.0F);
            return Color{
                alpha(static_cast<float>(from.r) +
                      (static_cast<float>(to.r) - from.r) * amount),
                alpha(static_cast<float>(from.g) +
                      (static_cast<float>(to.g) - from.g) * amount),
                alpha(static_cast<float>(from.b) +
                      (static_cast<float>(to.b) - from.b) * amount),
                alpha(static_cast<float>(from.a) +
                      (static_cast<float>(to.a) - from.a) * amount),
            };
        };
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
        drawSdfUiText(
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
            drawSdfUiText(
                text, {centerX - width * 0.5F, textY},
                fittedSize, color);
        };
    const auto selected = std::ranges::find_if(
        slots, &HotbarSlot::selected);
    const bool shakeSelectedSlot =
        selected != slots.end() && !selected->available &&
        invalidActionRemaining > 0.0;
    const float invalidShake = shakeSelectedSlot
        ? std::sin(static_cast<float>(GetTime()) * 92.0F) *
              4.0F * static_cast<float>(std::clamp(
                  invalidActionRemaining / 0.22, 0.0, 1.0))
        : 0.0F;
    if (selectionAlpha > 0.01F && !slots.empty()) {
        const float selectionX =
            startX + selectionPosition *
                         (SlotSize + Gap) + invalidShake;
        const float nearestSlotDistance = std::min(
            0.5F,
            std::abs(selectionPosition - std::round(selectionPosition)));
        const float settledness = 1.0F - nearestSlotDistance * 2.0F;
        const float frameLift = 7.0F * selectionAlpha *
            (0.65F + settledness * 0.35F);
        const Rectangle selectionBounds{
            selectionX - 2.0F, slotY - frameLift - 2.0F,
            SlotSize + 4.0F, SlotSize + 4.0F};
        const Rectangle glowBounds{
            selectionBounds.x - 3.0F,
            selectionBounds.y - 3.0F,
            selectionBounds.width + 6.0F,
            selectionBounds.height + 6.0F};
        DrawRectangleRoundedLinesEx(
            glowBounds,
            SlotRadius * 2.0F / glowBounds.height,
            8, 3.0F,
            {255, 186, 72,
             alpha(72.0F * selectionAlpha)});
        DrawRectangleRoundedLinesEx(
            selectionBounds,
            SlotRadius * 2.0F / selectionBounds.height,
            8, 2.5F,
            {255, 250, 224,
             alpha(255.0F * selectionAlpha)});
        const Vector2 selectionBadgeCenter{
            selectionX + 5.0F,
            slotY - frameLift + 5.0F};
        DrawRing(
            selectionBadgeCenter,
            KeyBadgeRadius + 2.0F,
            KeyBadgeRadius + 5.0F,
            0.0F, 360.0F, 32,
            {255, 186, 72,
             alpha(72.0F * selectionAlpha)});
        DrawRing(
            selectionBadgeCenter,
            KeyBadgeRadius,
            KeyBadgeRadius + 2.5F,
            0.0F, 360.0F, 32,
            {255, 250, 224,
             alpha(255.0F * selectionAlpha)});
    }
    for (std::size_t index = 0; index < slots.size();
         ++index) {
        const HotbarSlot& slot = slots[index];
        const float x =
            startX + static_cast<float>(index) *
                         (SlotSize + Gap) +
            (slot.selected ? invalidShake : 0.0F);
        const float proximity = std::max(
            0.0F,
            1.0F - std::abs(
                static_cast<float>(index) - selectionPosition));
        const float easedProximity =
            proximity * proximity * (3.0F - 2.0F * proximity);
        const float selectionAmount =
            easedProximity * selectionAlpha;
        const float raisedY = slotY - 7.0F * selectionAmount;
        const Rectangle bounds{x, raisedY, SlotSize, SlotSize};
        const float innerInset = 5.0F;
        const Rectangle shadowBounds{
            x + innerInset, raisedY + innerInset,
            InnerSize, InnerSize};
        const Rectangle surfaceBounds{
            x + innerInset, raisedY + innerInset,
            InnerSize, SurfaceHeight};
        DrawRectangleRounded(
            bounds, SlotRadius * 2.0F / SlotSize,
            8, SlotColor);
        DrawRectangleRounded(
            shadowBounds, InnerRadius * 2.0F / InnerSize,
            8, ShadowColor);
        DrawRectangleRounded(
            surfaceBounds, InnerRadius * 2.0F / SurfaceHeight,
            8, SurfaceColor);
        if (selectionAmount > 0.001F) {
            DrawRectangleRounded(
                surfaceBounds,
                InnerRadius * 2.0F / SurfaceHeight,
                8, {255, 228, 170,
                    alpha(selectionAmount * 42.0F)});
        }
        if (!slot.available) {
            DrawRectangleRounded(
                bounds, SlotRadius * 2.0F / SlotSize,
                8, {25, 24, 22, 90});
        }
        if (slot.upgradeAvailable) {
            const float upgradeWave = 0.5F + 0.5F * std::sin(
                static_cast<float>(GetTime()) * 4.8F);
            const Vector2 upgradeCenter{
                x + SlotSize - 12.0F, raisedY - 2.0F};
            DrawCircleV(
                upgradeCenter, 5.5F + upgradeWave * 1.5F,
                {255, 190, 63,
                 alpha(50.0F + upgradeWave * 55.0F)});
            DrawCircleV(
                upgradeCenter, 3.4F,
                {255, 221, 112, 255});
            DrawCircleV(
                {upgradeCenter.x - 1.0F,
                 upgradeCenter.y - 1.0F},
                1.0F, {255, 253, 221, 255});
        }

        const Color keyColor = !slot.available
            ? Color{126, 121, 111, 205}
            : mixColor(
                  {245, 238, 220, 255},
                  {255, 247, 213, 255},
                  selectionAmount);

        const bool hasIcon = IsTextureValid(slot.icon);
        if (hasIcon) {
            const float IconSize = iconLayout ? 74.0F : 62.0F;
            const float scale = IconSize / static_cast<float>(
                std::max(slot.icon.width, slot.icon.height));
            const float width = static_cast<float>(slot.icon.width) * scale;
            const float height = static_cast<float>(slot.icon.height) * scale;
            DrawTexturePro(slot.icon,
                {0, 0, static_cast<float>(slot.icon.width),
                 static_cast<float>(slot.icon.height)},
                {x + (SlotSize - width) * 0.5F,
                 raisedY + (iconLayout ? 8.0F : 26.0F) +
                     (IconSize - height) * 0.5F,
                 width, height}, {}, 0.0F,
                slot.available ? WHITE : Color{160, 160, 160, 210});
        }
        const Vector2 keyBadgeCenter{x + 5.0F, raisedY + 5.0F};
        DrawCircleV(keyBadgeCenter, KeyBadgeRadius, SlotColor);

        const Vector2 keySize = measureUiText(slot.key, KeyFontSize);
        SdfUiTextStyle keyStyle{};
        keyStyle.fillColor = keyColor;
        keyStyle.outlineColor = {49, 49, 49, 255};
        keyStyle.shadowColor = {49, 49, 49, 255};
        keyStyle.shadowOffsetPixels = {0.0F, 2.4F};
        keyStyle.outlinePixels = 1.8F;
        drawStyledSdfUiText(
            slot.key,
            {keyBadgeCenter.x - keySize.x * 0.5F,
             raisedY - 6.0F},
            KeyFontSize, keyStyle);
        if (!hasIcon && (showSlotLabels || iconLayout)) {
            const Color labelColor = !slot.available
                ? Color{112, 108, 101, 190}
                : mixColor(
                      {224, 215, 195, 235},
                      {255, 239, 197, 255},
                      selectionAmount);
            centeredText(
                slot.label, x + SlotSize * 0.5F,
                raisedY + SlotSize * 0.48F, 10.0F,
                SlotSize - 8.0F,
                labelColor);
        }
    }

    if (showSelectedInfo && selected != slots.end()) {
        const std::string title{selected->label};
        const float titleWidth = measureUiText(title, 17.0F).x;
        const float costWidth = resourceCostRowWidth(selected->cost);
        const float infoWidth = std::max(titleWidth, costWidth) + 40.0F;
        const float infoX = (screenWidth - infoWidth) * 0.5F;
        const float infoY = slotY - 68.0F + selectedInfoOffset;
        drawSdfUiText(
            title,
            {infoX + (infoWidth - titleWidth) * 0.5F,
             infoY + 6.0F},
            17.0F, {255, 225, 155, 255});
        drawResourceCostRow(
            ui, selected->cost,
            {infoX + (infoWidth - costWidth) * 0.5F,
             infoY + 32.0F},
            14.0F, 22.0F,
            selected->available
                ? Color{226, 218, 197, 245}
                : Color{242, 103, 83, 255});
    }

    if (showSwitchLabel && selected != slots.end()) {
        const float selectedIndex = static_cast<float>(
            std::distance(slots.begin(), selected));
        const float travel = std::abs(
            selectedIndex - selectionPosition);
        const float labelAlpha = std::clamp(
            travel * 2.4F, 0.0F, 1.0F) * selectionAlpha;
        if (labelAlpha > 0.01F) {
            constexpr float LabelFontSize = 15.0F;
            const float labelWidth =
                measureUiText(selected->label, LabelFontSize).x;
            SdfUiTextStyle labelStyle{};
            labelStyle.fillColor = {
                255, 239, 197, alpha(255.0F * labelAlpha)};
            labelStyle.outlineColor = {
                49, 49, 49, alpha(255.0F * labelAlpha)};
            labelStyle.shadowColor = {
                82, 58, 39, alpha(210.0F * labelAlpha)};
            labelStyle.shadowOffsetPixels = {0.0F, 3.0F};
            labelStyle.outlinePixels = 2.0F;
            drawStyledSdfUiText(
                selected->label,
                {(screenWidth - labelWidth) * 0.5F,
                 slotY - 35.0F - (1.0F - labelAlpha) * 5.0F},
                LabelFontSize, labelStyle);
        }
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
        {},
        0.0F, 0.0F, view.invalidActionRemaining);
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
            snapshot.sandboxMode || snapshot.unlimitedResources);
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
            .icon = ui.buildingTexture(type),
        });
    }
    drawHotbarSlots(
        ui, slots,
        view.buildHotbarSelectionPosition,
        view.buildHotbarSelectionAlpha,
        false, true, {},
        0.0F, -28.0F, view.invalidActionRemaining);
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
        drawSdfUiText("ITEMS", {PanelX + 10.0F, PanelY + 7.0F},
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
            drawSdfUiText(initial,
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
        drawSdfUiText(badge, {badgeBounds.x + 3.0F, badgeBounds.y - 1.0F},
                   9.0F, UiWhite);
        if (CheckCollisionPointRec(mouse, slot)) {
            hovered = effect;
            hoveredStacks = stacks;
            DrawRectangleLinesEx(slot, 3.0F, {213, 246, 255, 255});
        }
    }

    if (!expanded && activeCount > shownCount) {
        drawSdfUiText(
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
        drawSdfUiText(tooltipTitle,
                   {tooltip.x + 12.0F, tooltip.y + 9.0F},
                   fitUiTextSize(
                       tooltipTitle, 13.0F, 9.0F,
                       tooltip.width - 24.0F),
                   {122, 218, 255, 255});
        drawSdfUiText(tooltipDescription,
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

    constexpr float y = 20.0F;
    constexpr float OuterHeight = 24.0F;
    constexpr float LevelSlotWidth = 92.0F;
    constexpr float ExperienceWidth = 220.0F;
    constexpr float OuterRadius = 8.0F;
    constexpr float InnerHeight = 16.0F;
    constexpr float InnerRadius = 5.0F;
    constexpr float FillHeight = 10.0F;
    constexpr float FillRadius = 5.0F;
    constexpr float InnerHorizontalInset = 5.0F;
    constexpr Color OuterColor{0, 122, 255, 255};
    constexpr Color InnerColor{0, 189, 255, 255};
    constexpr Color FillColor{128, 221, 255, 255};
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenMinimum = static_cast<float>(
        std::min(GetScreenWidth(), GetScreenHeight()));
    const float minimapSize = std::clamp(
        screenMinimum * 0.16F, 120.0F, 156.0F);
    constexpr float MinimapRightMargin = 12.0F;
    constexpr float CompassMargin = 32.0F;
    constexpr float PanelMapGap = 18.0F;
    const float minimapOuterLeft = screenWidth - MinimapRightMargin -
        CompassMargin * 2.0F - minimapSize;
    const float totalWidth = LevelSlotWidth + ExperienceWidth;
    const float x = std::max(
        18.0F, minimapOuterLeft - PanelMapGap - totalWidth);
    const float requirement = std::max(
        1.0F, static_cast<float>(snapshot.requiredInsight));
    const float fraction = std::clamp(
        static_cast<float>(view.displayedInsight) / requirement,
        0.0F, 1.0F);
    const Rectangle outerBounds{
        x + LevelSlotWidth, y, ExperienceWidth, OuterHeight};
    const Rectangle innerBounds{
        outerBounds.x + InnerHorizontalInset,
        y + (OuterHeight - InnerHeight) * 0.5F,
        ExperienceWidth - InnerHorizontalInset * 2.0F,
        InnerHeight,
    };
    const Rectangle fillTrack{
        innerBounds.x,
        y + 5.0F,
        innerBounds.width,
        FillHeight,
    };
    DrawRectangleRounded(
        outerBounds, OuterRadius * 2.0F / OuterHeight,
        8, OuterColor);
    DrawRectangleRounded(
        innerBounds, InnerRadius * 2.0F / InnerHeight,
        8, InnerColor);
    if (fraction > 0.001F) {
        const Rectangle fillBounds{
            fillTrack.x, fillTrack.y,
            fillTrack.width * fraction, fillTrack.height};
        DrawRectangleRounded(
            fillBounds, FillRadius * 2.0F / FillHeight,
            8, FillColor);
    }

    const std::string status =
        "LVL " + std::to_string(snapshot.playerLevel);
    constexpr float statusFontSize = 28.0F / 1.68F;
    const Vector2 statusSize = measureUiText(status, statusFontSize);
    SdfUiTextStyle statusStyle{};
    statusStyle.fillColor = UiWhite;
    statusStyle.outlineColor = {49, 49, 49, 255};
    statusStyle.shadowColor = {82, 82, 82, 255};
    statusStyle.shadowOffsetPixels = {0.0F, 4.0F};
    statusStyle.outlinePixels = 3.0F;
    drawStyledSdfUiText(
        status,
        {x + (LevelSlotWidth - statusSize.x) * 0.5F,
         y + (OuterHeight - statusSize.y) * 0.5F},
        statusFontSize, statusStyle);
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
            .upgradeAvailable =
                weapon == PlayerWeapon::Rifle &&
                snapshot.rifleLevel < 3 &&
                snapshot.coreLevel > snapshot.rifleLevel &&
                (snapshot.unlimitedResources ||
                 snapshot.crystals >=
                     snapshot.rifleUpgradeCrystalCost),
            .icon = ui.weaponTexture(weapon),
        };
        ++slotCount;
    }
    drawHotbarSlots(
        ui, std::span<const HotbarSlot>{slots.data(), slotCount},
        view.weaponHotbarSelectionPosition,
        view.weaponHotbarSelectionAlpha,
        true, false, {}, 0.0F, 0.0F,
        view.invalidActionRemaining, true);
}


} // namespace ian::hud_detail
