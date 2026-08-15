#include "app/App.hpp"

#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace ian {
namespace {

[[nodiscard]] LootUpgradeEffect adjacentLootEffect(
    LootUpgradeEffect effect, int direction) {
    const int count = static_cast<int>(LootUpgradeEffectCount);
    const int index = (static_cast<int>(effect) + direction + count) % count;
    return static_cast<LootUpgradeEffect>(index);
}

[[nodiscard]] LootRarity adjacentLootRarity(
    LootRarity rarity, int direction) {
    constexpr int Count = 4;
    const int index = (static_cast<int>(rarity) + direction + Count) % Count;
    return static_cast<LootRarity>(index);
}

} // namespace

void App::drawItemGrantMenu() {
    if (!itemGrantMenuVisible_) {
        return;
    }

    constexpr float Width = 650.0F;
    constexpr float Height = 680.0F;
    constexpr float Padding = 34.0F;
    constexpr float ButtonHeight = 62.0F;
    constexpr float ArrowWidth = 76.0F;
    const float x =
        (static_cast<float>(GetScreenWidth()) - Width) * 0.5F;
    const float y =
        (static_cast<float>(GetScreenHeight()) - Height) * 0.5F;
    const float contentWidth = Width - Padding * 2.0F;

    ui_.drawPanel({x, y, Width, Height}, 250);
    ui_.drawInsetPanel(
        {x + Padding, y + 26.0F, contentWidth, 72.0F}, 245);
    ui_.drawLabel(
        {x + Padding, y + 33.0F, contentWidth, 56.0F},
        "GRANT ITEM", 1);

    drawUiText(
        "ITEM", {x + Padding, y + 122.0F},
        17.0F, {245, 220, 174, 255});
    const float itemY = y + 154.0F;
    if (ui_.drawButton(
            {x + Padding, itemY, ArrowWidth, ButtonHeight}, "<")) {
        debugGrantLootEffect_ = adjacentLootEffect(
            debugGrantLootEffect_, -1);
    }
    ui_.drawInsetPanel(
        {x + Padding + ArrowWidth + 12.0F, itemY,
         contentWidth - ArrowWidth * 2.0F - 24.0F, ButtonHeight},
        240);
    ui_.drawLabel(
        {x + Padding + ArrowWidth + 12.0F, itemY,
         contentWidth - ArrowWidth * 2.0F - 24.0F, ButtonHeight},
        lootUpgradeName(debugGrantLootEffect_), 1);
    if (ui_.drawButton(
            {x + Width - Padding - ArrowWidth, itemY,
             ArrowWidth, ButtonHeight}, ">")) {
        debugGrantLootEffect_ = adjacentLootEffect(
            debugGrantLootEffect_, 1);
    }

    ui_.drawInsetPanel(
        {x + Padding, y + 230.0F, contentWidth, 58.0F}, 225);
    ui_.drawLabel(
        {x + Padding + 10.0F, y + 234.0F,
         contentWidth - 20.0F, 50.0F},
        lootUpgradeDescription(debugGrantLootEffect_), 0);

    drawUiText(
        "RARITY", {x + Padding, y + 316.0F},
        17.0F, {245, 220, 174, 255});
    const float rarityY = y + 348.0F;
    if (ui_.drawButton(
            {x + Padding, rarityY, ArrowWidth, ButtonHeight}, "<")) {
        debugGrantLootRarity_ = adjacentLootRarity(
            debugGrantLootRarity_, -1);
    }
    ui_.drawInsetPanel(
        {x + Padding + ArrowWidth + 12.0F, rarityY,
         contentWidth - ArrowWidth * 2.0F - 24.0F, ButtonHeight},
        240);
    ui_.drawLabel(
        {x + Padding + ArrowWidth + 12.0F, rarityY,
         contentWidth - ArrowWidth * 2.0F - 24.0F, ButtonHeight},
        lootRarityName(debugGrantLootRarity_), 1);
    if (ui_.drawButton(
            {x + Width - Padding - ArrowWidth, rarityY,
             ArrowWidth, ButtonHeight}, ">")) {
        debugGrantLootRarity_ = adjacentLootRarity(
            debugGrantLootRarity_, 1);
    }

    drawUiText(
        TextFormat("COUNT: %d", debugGrantLootCount_),
        {x + Padding, y + 444.0F}, 18.0F,
        {245, 220, 174, 255});
    ui_.drawInsetPanel(
        {x + Padding, y + 478.0F, contentWidth, 44.0F}, 235);
    const float selectedCount = ui_.drawSliderBar(
        {x + Padding + 12.0F, y + 486.0F,
         contentWidth - 24.0F, 28.0F},
        static_cast<float>(debugGrantLootCount_), 1.0F, 10.0F);
    debugGrantLootCount_ = std::clamp(
        static_cast<int>(std::lround(selectedCount)), 1, 10);

    const float actionY = y + 558.0F;
    if (ui_.drawButton(
            {x + Padding, actionY,
             contentWidth * 0.66F - 7.0F, ButtonHeight},
            "GRANT")) {
        pendingLootGrant_ = PendingLootGrant{
            debugGrantLootEffect_, debugGrantLootRarity_,
            debugGrantLootCount_};
        audio_.playUiConfirm();
    }
    if (ui_.drawButton(
            {x + Padding + contentWidth * 0.66F + 7.0F,
             actionY, contentWidth * 0.34F - 7.0F,
             ButtonHeight},
            "CLOSE [F5]")) {
        itemGrantMenuVisible_ = false;
        if (simulation_.snapshot().state != RunState::Paused) {
            DisableCursor();
        }
    }
}

} // namespace ian
