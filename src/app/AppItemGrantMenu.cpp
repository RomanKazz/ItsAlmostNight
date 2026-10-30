#include "ui/UiViewport.hpp"
#include "app/App.hpp"

#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace ian {
namespace {

[[nodiscard]] LootUpgradeEffect adjacentLootEffect(
    LootUpgradeEffect effect, int direction) {
    const int count = static_cast<int>(ActiveLootEffects.size());
    const auto found=std::ranges::find(ActiveLootEffects,effect);
    const int old=found==ActiveLootEffects.end()?0:static_cast<int>(found-ActiveLootEffects.begin());
    return ActiveLootEffects[static_cast<std::size_t>((old+direction+count)%count)];
}


} // namespace

void App::drawItemGrantMenu() {
    const ScopedUiCanvas canvas;
    if (!itemGrantMenuVisible_) {
        return;
    }

    constexpr float Width = 650.0F;
    constexpr float Height = 680.0F;
    constexpr float Padding = 34.0F;
    constexpr float ButtonHeight = 62.0F;
    constexpr float ArrowWidth = 76.0F;
    const float x =
        (static_cast<float>(uiScreenWidth()) - Width) * 0.5F;
    const float y =
        (static_cast<float>(uiScreenHeight()) - Height) * 0.5F;
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
    debugGrantLootRarity_=lootUpgradeRarity(debugGrantLootEffect_);
    ui_.drawInsetPanel({x+Padding,rarityY,contentWidth,ButtonHeight},240);
    ui_.drawLabel({x+Padding,rarityY,contentWidth,ButtonHeight},lootRarityName(debugGrantLootRarity_),1);

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
            "CLOSE")) {
        itemGrantMenuVisible_ = false;
        if (simulation_.snapshot().state != RunState::Paused) {
            DisableCursor();
        }
    }
}

void App::drawSandboxCardMenu(const SimulationSnapshot& snapshot) {
    const ScopedUiCanvas canvas;
    if (!sandboxCardMenuVisible_) return;
    DebugUpgradeMenuInput input;
    input.pointer = uiMousePosition();
    input.click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.wheel = GetMouseWheelMove();
    input.backspace = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE);
    input.enter = IsKeyPressed(KEY_ENTER);
    for (int code = GetCharPressed(); code > 0; code = GetCharPressed()) {
        if (code < 32) continue;
        int bytes = 0;
        const char* text = CodepointToUTF8(code, &bytes);
        input.text.append(text, static_cast<std::size_t>(bytes));
    }
    const auto result = drawDebugUpgradeMenu(ui_, snapshot, sandboxCardMenuState_,
        {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())}, input);
    if (result.grant) pendingSandboxCardGrant_ = result.grant;
    if (result.close) {
        sandboxCardMenuVisible_ = false;
        if (snapshot.state != RunState::Paused) DisableCursor();
    }
}

} // namespace ian
