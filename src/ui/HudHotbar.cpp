#include "ui/UiViewport.hpp"
#include "ui/HudHotbar.hpp"

#include "buildings/BuildingHotbarLayout.hpp"
#include "buildings/ModularBuildingConstants.hpp"
#include "game/Simulation.hpp"
#include "ui/GameUi.hpp"
#include "ui/HudFormatting.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiItemDescriptions.hpp"
#include "ui/UiItemTooltip.hpp"
#include "ui/UiText.hpp"
#include "localization/Localization.hpp"

#include <raylib.h>
#include <raymath.h>
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
    std::string_view description;
    bool gapBefore{};
    double cooldownRemaining{};
};

float drawHotbarSlots(
    GameUi& ui, std::span<const HotbarSlot> slots,
    float selectionPosition, float selectionAlpha,
    bool showSlotLabels = false,
    bool showSelectedInfo = true,
    std::string_view categoryLabel = {},
    float verticalOffset = 0.0F,
    double invalidActionRemaining = 0.0,
    bool showSwitchLabel = false,
    const ResourceCost* availableResources = nullptr,
    bool avoidVitals = false) {
    if (slots.empty()) return 0.0F;

    const bool iconLayout = std::ranges::any_of(
        slots, [](const HotbarSlot& slot) {
            return IsTextureValid(slot.icon);
        });
    constexpr float GroupGap = 28.0F;
    const float screenWidth = static_cast<float>(uiScreenWidth());
    const auto groups = std::ranges::count_if(slots,
        [](const HotbarSlot& slot) { return slot.gapBefore; });
    const float naturalWidth = 96.0F * static_cast<float>(slots.size()) +
        7.0F * static_cast<float>(slots.size() - 1U) +
        GroupGap * static_cast<float>(groups);
    const float slotScale = std::min(1.0F,
        std::max(1.0F, screenWidth - 80.0F) / naturalWidth);
    const float Gap = 7.0F * slotScale;
    const float SlotSize = 96.0F * slotScale;
    const float SlotRadius = 15.0F * slotScale;
    const float KeyBadgeRadius = 14.45F * slotScale;
    const float KeyFontSize = 16.32F * slotScale;
    const float InnerSize = 86.0F * slotScale;
    const float InnerRadius = 11.0F * slotScale;
    const float SurfaceHeight = 82.0F * slotScale;
    constexpr Color SlotColor{104, 60, 37, 255};
    constexpr Color ShadowColor{147, 115, 73, 255};
    constexpr Color SurfaceColor{230, 188, 147, 255};
    const float totalWidth = naturalWidth * slotScale;
    const float startX = (screenWidth - totalWidth) * 0.5F;
    std::vector<float> slotPositions;
    slotPositions.reserve(slots.size());
    float cursorX = startX;
    for (const HotbarSlot& slot : slots) {
        if (slot.gapBefore) cursorX += GroupGap * slotScale;
        slotPositions.push_back(cursorX);
        cursorX += SlotSize + Gap;
    }
    // A fully unlocked creative catalog is wider than the center of the HUD.
    // Lift only that long row clear of the player's health/resource bars.
    const float vitalsClearance = avoidVitals && startX < 290.0F ? 188.0F : 0.0F;
    const float slotY = static_cast<float>(uiScreenHeight()) -
        SlotSize - 20.0F - vitalsClearance + verticalOffset;
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
        const auto leftIndex = static_cast<std::size_t>(selectionPosition);
        const auto rightIndex = std::min(leftIndex + 1U, slots.size() - 1U);
        const float selectionX = std::lerp(
            slotPositions[leftIndex], slotPositions[rightIndex],
            selectionPosition - static_cast<float>(leftIndex)) + invalidShake;
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
        if (!slots[static_cast<std::size_t>(std::round(selectionPosition))].key.empty()) {
            const Vector2 selectionBadgeCenter{
                selectionX + 5.0F * slotScale,
                slotY - frameLift + 5.0F * slotScale};
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
    }
    const HotbarSlot* hoveredSlot = nullptr;
    for (std::size_t index = 0; index < slots.size();
         ++index) {
        const HotbarSlot& slot = slots[index];
        const float x =
            slotPositions[index] +
            (slot.selected ? invalidShake : 0.0F);
        if (!IsCursorHidden() && CheckCollisionPointRec(uiMousePosition(),{x,slotY,SlotSize,SlotSize})) hoveredSlot = &slot;
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
        const float innerInset = 5.0F * slotScale;
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
            const float IconSize = (iconLayout ? 74.0F : 62.0F) * slotScale;
            const float scale = IconSize / static_cast<float>(
                std::max(slot.icon.width, slot.icon.height));
            const float width = static_cast<float>(slot.icon.width) * scale;
            const float height = static_cast<float>(slot.icon.height) * scale;
            DrawTexturePro(slot.icon,
                {0, 0, static_cast<float>(slot.icon.width),
                 static_cast<float>(slot.icon.height)},
                {x + (SlotSize - width) * 0.5F,
                 raisedY + (iconLayout ? 8.0F : 26.0F) * slotScale +
                     (IconSize - height) * 0.5F,
                 width, height}, {}, 0.0F,
                slot.available ? WHITE : Color{160, 160, 160, 210});
        }
        if (slot.cooldownRemaining > 0.0) {
            const int tenths = static_cast<int>(std::ceil(slot.cooldownRemaining * 10.0));
            const std::string seconds = std::to_string(tenths / 10) + "." +
                std::to_string(tenths % 10) + "s";
            const Rectangle cooldownBadge{
                x + 24.0F * slotScale, raisedY + 64.0F * slotScale,
                56.0F * slotScale, 23.0F * slotScale};
            DrawRectangleRounded(cooldownBadge, 0.45F, 6, {25, 24, 22, 210});
            centeredText(seconds, cooldownBadge.x + cooldownBadge.width * 0.5F,
                cooldownBadge.y + 3.0F * slotScale, 15.0F * slotScale,
                cooldownBadge.width - 5.0F * slotScale, {255, 241, 213, 255});
        }
        if (!slot.key.empty()) {
            const Vector2 keyBadgeCenter{x + 5.0F * slotScale, raisedY + 5.0F * slotScale};
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
                 raisedY - 6.0F * slotScale},
                KeyFontSize, keyStyle);
        }
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
        const float infoY = slotY - 68.0F;
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
            Color{245, 238, 220, 255}, availableResources);
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

    if (hoveredSlot) drawItemTooltip(hoveredSlot->label,hoveredSlot->description,uiMousePosition(),
        {static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())});
    return slotY;
}

void drawConstructionHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view, float transitionOffset) {
    constexpr std::array<std::string_view,
                         MaximumBuildingHotbarSlots> Keys{
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "[", "]", ";"};
    const ConstructionHotbarLayout layout =
        makeConstructionHotbarLayout(
            snapshot.unlockedBuildings,
            snapshot.coreMaxHealth > 0.0,
            snapshot.coreLevel,
            snapshot.sandboxMode || snapshot.unlimitedResources);
    const auto modularLabel = [](ModularBuildPiece piece) {
        switch (piece) {
        case ModularBuildPiece::Foundation:
        case ModularBuildPiece::FloorPlatform: return "PLATFORM";
        case ModularBuildPiece::Ramp: return "RAMP";
        case ModularBuildPiece::Wall: return "WALL";
        }
        return "";
    };
    std::vector<HotbarSlot> slots;
    slots.reserve(layout.count);
    for (std::size_t index = 0; index < layout.count; ++index) {
        const ConstructionHotbarItem& item = layout.items[index];
        if (item.modularPiece) {
            const ModularBuildPiece piece = *item.modularPiece;
            const bool selected = view.foundationBuildMode &&
                view.selectedModularBuildPiece == modularBuildHotbarIndex(piece).value_or(0U);
            const ModularBuildPiece costPiece = selected
                ? view.activeModularBuildPiece : piece;
            const ResourceCost cost = snapshot.modularBuildingCosts[
                static_cast<std::size_t>(costPiece)];
            slots.push_back({
                .key = Keys[index],
                .label = modularLabel(piece),
                .cost = cost,
                .selected = selected,
                .available = snapshot.unlimitedResources ||
                    canAfford(
                        cost, snapshot.wood,
                        snapshot.stone, snapshot.crystals),
                .icon = ui.modularBuildingTexture(piece),
                .description = modularDescription(piece),
                .gapBefore = item.gapBefore,
            });
            continue;
        }
        if (item.buildingType) {
            const BuildingType type = *item.buildingType;
            const ResourceCost cost = snapshot.buildingCosts[
                static_cast<std::size_t>(type)];
            slots.push_back({
                .key = Keys[index],
                .label = buildingDisplayName(type),
                .cost = cost,
                .selected = !view.foundationBuildMode &&
                    snapshot.selectedBuilding == type,
                .available = snapshot.unlimitedResources ||
                    canAfford(cost, snapshot.wood, snapshot.stone, snapshot.crystals),
                .icon = ui.buildingTexture(type),
                .description = buildingDescription(type),
                .gapBefore = item.gapBefore,
            });
        }
    }
    const ResourceCost stock{snapshot.wood, snapshot.stone, snapshot.crystals};
    drawHotbarSlots(
        ui, slots,
        view.foundationHotbarSelectionPosition,
        view.foundationHotbarSelectionAlpha,
        false, true,
        {},
        transitionOffset, view.invalidActionRemaining, false,
        snapshot.unlimitedResources ? nullptr : &stock, true);
}

void drawBuildHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view, float transitionOffset) {
    drawConstructionHotbar(ui, snapshot, view, transitionOffset);
}

void drawLootInventory(
    GameUi& ui, const SimulationSnapshot& snapshot,
    bool expanded, bool coreVisible) {
    constexpr auto Effects = [] {
        std::array<LootUpgradeEffect,LootUpgradeEffectCount> values{};
        for (std::size_t i=0;i<values.size();++i) values[i]=static_cast<LootUpgradeEffect>(i);
        return values;
    }();
    int activeCount = 0;
    for (const LootUpgradeEffect effect : Effects) {
        if (snapshot.lootStacks[lootUpgradeIndex(effect)] > 0) {
            ++activeCount;
        }
    }
    if (activeCount == 0) return;

    constexpr float PanelX = 20.0F;
    constexpr float Pitch = 52.8F;
    constexpr float SlotSize = Pitch - 3.0F;
    constexpr int Columns = 6;
    const int shownCount = activeCount;
    const int rows = (shownCount + Columns - 1) / Columns;
    // Match the 20px screen margin and 52.8px pitch of tomes/defenses.
    // Grow upward from the actual topmost vital bar instead of overlapping it.
    const float vitalTop = static_cast<float>(uiScreenHeight()) - 56.0F -
        (coreVisible ? 44.0F : 0.0F) - (snapshot.playerMaxRecoverableArmor > 0 ? 44.0F : 0.0F);
    const float PanelY = expanded ? std::max(220.0F, vitalTop - static_cast<float>(rows) * Pitch - 12.0F)
        : vitalTop - static_cast<float>(rows) * Pitch - 12.0F;

    const Vector2 mouse = uiMousePosition();
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
            PanelX + static_cast<float>(column) * Pitch,
            PanelY + static_cast<float>(row) * Pitch,
            SlotSize, SlotSize};
        ++slotIndex;
        DrawRectangleRounded(slot, .18F, 4, {24, 29, 27, 160});
        const Texture2D texture = ui.lootTexture(effect);
        if (IsTextureValid(texture)) {
            DrawTexturePro(texture,
                {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                {slot.x + 2, slot.y + 1, slot.width - 4, slot.height - 6}, {}, 0, WHITE);
        }
        const std::string badge = std::to_string(stacks);
        const float badgeFont = Pitch * .19F;
        const Vector2 badgeSize = measureUiText(badge, badgeFont);
        drawStyledSdfUiText(badge, {slot.x + slot.width - badgeSize.x - 2,
            slot.y + slot.height - badgeSize.y - 1}, badgeFont,
            {.fillColor = UiWhite, .outlineColor = {20, 22, 20, 255}, .outlinePixels = 1.5F});
        if (!IsCursorHidden() && CheckCollisionPointRec(mouse, slot)) {
            hovered = effect;
            hoveredStacks = stacks;
        }
    }

    if (hovered && !IsCursorHidden()) drawItemTooltip(
        localizeText(lootUpgradeName(*hovered))+" x"+std::to_string(hoveredStacks),
        lootUpgradeDescription(*hovered),mouse,
        {static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())});
}

const char* weaponLabel(PlayerWeapon weapon) {
    switch (weapon) {
    case PlayerWeapon::LegacyBareHands:
    case PlayerWeapon::LegacyHammer: return "REMOVED";
    case PlayerWeapon::Axe: return "AXE";
    case PlayerWeapon::Pickaxe: return "PICKAXE";
    case PlayerWeapon::Club: return "CLUB";
    case PlayerWeapon::IceWand: return "ICE WAND";
    case PlayerWeapon::FireWand: return "FIRE WAND";
    case PlayerWeapon::Rifle: return "RIFLE";
    case PlayerWeapon::Bomb: return "BOMBS";
    }
    return "WEAPON";
}

void drawCompactInsight(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view) {

    constexpr float y = 20.0F;
    constexpr float OuterHeight = 24.0F;
    constexpr float MinimumLevelSlotWidth = 92.0F;
    constexpr float ExperienceWidth = 220.0F;
    constexpr float statusFontSize = 28.0F / 1.68F;
    const std::string status =
        "LVL " + std::to_string(snapshot.playerLevel);
    const Vector2 statusSize = measureUiText(status, statusFontSize);
    const float levelSlotWidth = std::max(
        MinimumLevelSlotWidth, statusSize.x + 22.0F);
    constexpr float OuterRadius = 8.0F;
    constexpr float InnerHeight = 16.0F;
    constexpr float InnerRadius = 5.0F;
    constexpr float FillHeight = 10.0F;
    constexpr float FillRadius = 5.0F;
    constexpr float InnerHorizontalInset = 5.0F;
    constexpr Color OuterColor{0, 122, 255, 255};
    constexpr Color InnerColor{0, 189, 255, 255};
    constexpr Color FillColor{128, 221, 255, 255};
    const float screenWidth = static_cast<float>(uiScreenWidth());
    const float screenMinimum = static_cast<float>(
        std::min(uiScreenWidth(), uiScreenHeight()));
    const float minimapSize = std::clamp(
        screenMinimum * 0.16F, 120.0F, 156.0F);
    constexpr float MinimapRightMargin = 12.0F;
    constexpr float CompassMargin = 32.0F;
    constexpr float PanelMapGap = 18.0F;
    const float minimapOuterLeft = screenWidth - MinimapRightMargin -
        CompassMargin * 2.0F - minimapSize;
    const float totalWidth = levelSlotWidth + ExperienceWidth;
    const float x = std::max(
        18.0F, minimapOuterLeft - PanelMapGap - totalWidth);
    const float requirement = std::max(
        1.0F, static_cast<float>(snapshot.requiredInsight));
    const float fraction = std::clamp(
        view.displayedInsightFraction >= 0.0
            ? static_cast<float>(view.displayedInsightFraction)
            : static_cast<float>(snapshot.currentInsight) / requirement,
        0.0F, 1.0F);
    const Rectangle outerBounds{
        x + levelSlotWidth, y, ExperienceWidth, OuterHeight};
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
        const Color fillColor = ColorLerp(
            FillColor, {215, 248, 255, 255},
            static_cast<float>(std::clamp(
                view.insightPulse, 0.0, 1.0)) * 0.45F);
        DrawRectangleRounded(
            fillBounds, FillRadius * 2.0F / FillHeight,
            8, fillColor);
    }

    SdfUiTextStyle statusStyle{};
    statusStyle.fillColor = UiWhite;
    statusStyle.outlineColor = {49, 49, 49, 255};
    statusStyle.shadowColor = {82, 82, 82, 255};
    statusStyle.shadowOffsetPixels = {0.0F, 2.0F};
    statusStyle.outlinePixels = 3.0F;
    drawStyledSdfUiText(
        status,
        {x + (levelSlotWidth - statusSize.x) * 0.5F,
         y + (OuterHeight - statusSize.y) * 0.5F},
        statusFontSize, statusStyle);

    const float silverOpacity = std::clamp(view.silverOpacity, 0.0F, 1.0F);
    if (silverOpacity > 0.001F) {
        const std::string silverText = std::to_string(
            std::max(0, view.silver));
        const Vector2 silverTextSize = measureUiText(
            silverText, statusFontSize);
        constexpr float SilverIconSize = 33.0F;
        constexpr float SilverIconGap = 5.0F;
        constexpr float SilverLevelGap = 18.0F;
        const float silverX = x - SilverLevelGap - SilverIconSize -
            SilverIconGap - silverTextSize.x;
        ui.drawResourceIcon(
            {silverX, y + (OuterHeight - SilverIconSize) * 0.5F,
             SilverIconSize, SilverIconSize},
            UiResourceIcon::Silver, ColorAlpha(WHITE, silverOpacity));
        auto silverStyle = statusStyle;
        silverStyle.fillColor = ColorAlpha(silverStyle.fillColor, silverOpacity);
        silverStyle.outlineColor = ColorAlpha(silverStyle.outlineColor, silverOpacity);
        silverStyle.shadowColor = ColorAlpha(silverStyle.shadowColor, silverOpacity);
        drawStyledSdfUiText(
            silverText,
            {silverX + SilverIconSize + SilverIconGap,
             y + (OuterHeight - silverTextSize.y) * 0.5F},
            statusFontSize, silverStyle);
    }
}

void drawClassAbilityStatus(const SimulationSnapshot& snapshot) {
    const auto* character = playerClassDefinition(snapshot.playerClass);
    if (!character) return;
    if (snapshot.playerClass == PlayerClass::Stump) {
        const float scale=std::min(static_cast<float>(uiScreenWidth())/1920.0F,static_cast<float>(uiScreenHeight())/1080.0F);
        const float y=static_cast<float>(uiScreenHeight())-151*scale;
        const float x=static_cast<float>(uiScreenWidth())*.5F;
        const float pulse=static_cast<float>(std::clamp(snapshot.classAbilityState.stumpPulseRemaining/.26,0.0,1.0));
        const int charge=std::clamp(snapshot.classAbilityState.stumpCombo,0,3);
        const bool erupted=charge==0 && snapshot.classAbilityState.stumpPulseRemaining>.35;
        const int visibleCharge=erupted?4:charge;
        const bool ready=charge==3;
        DrawRectangleRounded({x-82*scale,y-8*scale,164*scale,42*scale},.65F,12,{15,27,17,180});
        for(int i=0;i<4;++i) {
            const Vector2 p{x+static_cast<float>(i*31-46)*scale,y+12*scale};
            const float r=(9+((erupted || i==charge-1)?pulse*2:0))*scale;
            DrawCircleV(p,r+2*scale,{33,47,22,255});
            DrawCircleV(p,r,i<visibleCharge?Color{167,217,76,255}:Color{64,78,45,255});
            DrawLineEx({p.x-3*scale,p.y+4*scale},{p.x+3*scale,p.y-4*scale},2*scale,
                i<visibleCharge?Color{249,239,177,255}:Color{103,119,73,255});
        }
        if(ready) {
            const float glow=.55F+.25F*std::sin(static_cast<float>(snapshot.elapsedSeconds)*6);
            DrawRectangleRoundedLinesEx({x-83*scale,y-9*scale,166*scale,44*scale},.65F,12,2*scale,
                {189,225,97,static_cast<unsigned char>(glow*255)});
        }
        return;
    }
    std::string_view status;
    double remaining = 0;
    Color color = menuStyle::Green;
    const auto& ability = snapshot.classAbilityState;
    if (ability.rageRemaining > 0) { status = "RAGE"; remaining = ability.rageRemaining; }
    else if (snapshot.playerClass == PlayerClass::Alchemist && snapshot.battlePotionBerserkRemaining > 0) {
        status = "UNSTABLE BREW"; remaining = snapshot.battlePotionBerserkRemaining;
    } else if (ability.exhaustionRemaining > 0) {
        status = "EXHAUSTED"; remaining = ability.exhaustionRemaining;
        color = {242,112,91,255};
    }
    if (status.empty()) return;
    std::string label = localizeText(status);
    if (remaining > 0) label += " " + std::to_string(static_cast<int>(std::ceil(remaining))) + "s";
    const Rectangle badge{320,static_cast<float>(uiScreenHeight())-75,270,44};
    menuStyle::panel(badge,0.6F,menuStyle::Inset);
    menuStyle::text(label,{badge.x+12,badge.y,badge.width-24,badge.height},18,color,UiFontRole::MenuBodyBold,1);
    if (!IsCursorHidden() && CheckCollisionPointRec(uiMousePosition(),badge)) drawItemTooltip(
        character->passiveName,localizeText(character->passiveDescription[0])+" "+localizeText(character->drawback),
        uiMousePosition(),{static_cast<float>(uiScreenWidth()),static_cast<float>(uiScreenHeight())});
}

void drawWeaponHotbar(
    GameUi& ui, const SimulationSnapshot& snapshot,
    const HudViewState& view, float transitionOffset) {
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
        const int bombCooldownTenths = static_cast<int>(
            std::ceil(snapshot.bombCooldownRemaining * 10.0));
        labels[slotCount] = weapon == PlayerWeapon::Bomb && unlocked
                ? std::string("BOMBS · ") +
                    (bombCooldownTenths > 0
                        ? std::to_string(bombCooldownTenths / 10) + "." +
                              std::to_string(bombCooldownTenths % 10) + "s"
                        : "READY")
                : localizeText(weaponLabel(weapon)) + " · " + localizeText("LVL") + " " +
                    std::to_string(std::max(1,snapshot.weaponLevels[static_cast<std::size_t>(weapon)]));
        slots[slotCount] = {
            .key = keys[slotCount],
            .label = labels[slotCount],
            .cost = {},
            .selected = snapshot.selectedWeapon == weapon,
            .available = true,
            .upgradeAvailable = false,
            .icon = ui.weaponTexture(weapon),
            .description = weaponDescription(weapon),
            .cooldownRemaining = weapon == PlayerWeapon::Bomb
                ? snapshot.bombCooldownRemaining : 0.0,
        };
        ++slotCount;
    }
    drawHotbarSlots(
        ui, std::span<const HotbarSlot>{slots.data(), slotCount},
        view.weaponHotbarSelectionPosition,
        view.weaponHotbarSelectionAlpha,
        true, false, {}, transitionOffset,
        view.invalidActionRemaining, false);
}


} // namespace ian::hud_detail
