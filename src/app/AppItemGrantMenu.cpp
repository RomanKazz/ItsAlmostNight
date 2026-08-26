#include "app/App.hpp"

#include "ui/UiText.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>

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

void App::drawSandboxCardMenu(
    const SimulationSnapshot& snapshot) {
    if (!sandboxCardMenuVisible_ || !snapshot.sandboxMode) {
        return;
    }

    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const float width = std::min(1120.0F, screenWidth - 48.0F);
    const float height = std::min(880.0F, screenHeight - 48.0F);
    const float x = (screenWidth - width) * 0.5F;
    const float y = (screenHeight - height) * 0.5F;
    constexpr float Padding = 28.0F;
    constexpr float RowHeight = 70.0F;
    constexpr float RowGap = 8.0F;
    constexpr float GrantWidth = 144.0F;

    ui_.drawPanel({x, y, width, height}, 252);
    drawUiText(
        "SANDBOX CARDS",
        {x + Padding, y + 22.0F}, 25.0F,
        {250, 224, 151, 255});
    drawUiText(
        "CLICK A CARD TO ADD ONE STACK",
        {x + Padding, y + 54.0F}, 10.0F,
        {202, 198, 181, 225});

    const float tabY = y + 80.0F;
    const float tabWidth = (width - Padding * 2.0F - 12.0F) * 0.5F;
    if (ui_.drawToggleButton(
            {x + Padding, tabY, tabWidth, 48.0F},
            "POWER CARDS", sandboxCardMenuTab_ == 0)) {
        sandboxCardMenuTab_ = 0;
        sandboxCardMenuScroll_ = 0.0F;
    }
    if (ui_.drawToggleButton(
            {x + Padding + tabWidth + 12.0F, tabY,
             tabWidth, 48.0F},
            "UNLOCK CARDS", sandboxCardMenuTab_ == 1)) {
        sandboxCardMenuTab_ = 1;
        sandboxCardMenuScroll_ = 0.0F;
    }

    const float viewportY = y + 144.0F;
    const float footerHeight = 76.0F;
    const float viewportHeight = height - 144.0F - footerHeight;
    const Rectangle viewport{
        x + Padding, viewportY,
        width - Padding * 2.0F, viewportHeight};
    const std::size_t cardCount = sandboxCardMenuTab_ == 0
        ? RunUpgradeDefinitions.size()
        : simulation_.skillTree().nodes().size();
    const float contentHeight = static_cast<float>(cardCount) *
        (RowHeight + RowGap) - RowGap;
    const float maximumScroll = std::max(
        0.0F, contentHeight - viewport.height);
    if (CheckCollisionPointRec(GetMousePosition(), viewport)) {
        sandboxCardMenuScroll_ -= GetMouseWheelMove() * 72.0F;
    }
    sandboxCardMenuScroll_ = std::clamp(
        sandboxCardMenuScroll_, 0.0F, maximumScroll);

    BeginScissorMode(
        static_cast<int>(viewport.x),
        static_cast<int>(viewport.y),
        static_cast<int>(viewport.width),
        static_cast<int>(viewport.height));
    for (std::size_t index = 0; index < cardCount; ++index) {
        const float rowY = viewport.y +
            static_cast<float>(index) * (RowHeight + RowGap) -
            sandboxCardMenuScroll_;
        if (rowY + RowHeight < viewport.y ||
            rowY > viewport.y + viewport.height) {
            continue;
        }
        const Rectangle row{
            viewport.x, rowY, viewport.width, RowHeight};
        ui_.drawInsetPanel(row, 238);

        std::string title;
        std::string description;
        std::string stateLabel;
        ProgressionCardId card{};
        bool grantable = true;
        if (sandboxCardMenuTab_ == 0) {
            const RunUpgradeDefinition& definition =
                RunUpgradeDefinitions[index];
            card = progressionCardId(definition.effect);
            title = std::string(definition.name);
            description = std::string(definition.description);
            stateLabel = "x" + std::to_string(
                snapshot.runUpgradeStacks[
                    runUpgradeIndex(definition.effect)]);
        } else {
            const SkillNodeDefinition& definition =
                simulation_.skillTree().nodes()[index];
            card = skillProgressionCardId(index);
            title = definition.title;
            description = definition.description;
            grantable = simulation_.skillTree().state(index) !=
                SkillNodeState::Unlocked;
            stateLabel = grantable ? "LOCKED" : "UNLOCKED";
        }

        const float textWidth = row.width - GrantWidth - 64.0F;
        drawUiText(
            title + "  •  " + stateLabel,
            {row.x + 18.0F, row.y + 10.0F},
            14.0F,
            grantable
                ? Color{250, 231, 185, 255}
                : Color{119, 214, 151, 255});
        const float descriptionSize = fitUiTextSize(
            description, 10.0F, 7.0F, textWidth);
        drawUiText(
            description,
            {row.x + 18.0F, row.y + 39.0F},
            descriptionSize, {198, 195, 181, 225});
        if (ui_.drawButton(
                {row.x + row.width - GrantWidth - 12.0F,
                 row.y + 10.0F, GrantWidth, RowHeight - 20.0F},
                grantable ? "GRANT" : "ACTIVE") && grantable) {
            pendingSandboxCardGrant_ = card;
        }
    }
    EndScissorMode();

    if (maximumScroll > 0.0F) {
        const float fraction = sandboxCardMenuScroll_ / maximumScroll;
        const float trackHeight = viewport.height;
        const float thumbHeight = std::max(
            42.0F, trackHeight * viewport.height / contentHeight);
        DrawRectangleRounded(
            {viewport.x + viewport.width + 7.0F,
             viewport.y + fraction * (trackHeight - thumbHeight),
             5.0F, thumbHeight},
            1.0F, 4, {231, 203, 128, 210});
    }

    if (ui_.drawButton(
            {x + width - Padding - 220.0F,
             y + height - 58.0F, 220.0F, 42.0F},
            "CLOSE [F5]")) {
        sandboxCardMenuVisible_ = false;
        if (snapshot.state != RunState::Paused) {
            DisableCursor();
        }
    }
    drawUiText(
        "MOUSE WHEEL  SCROLL",
        {x + Padding, y + height - 45.0F},
        10.0F, {202, 198, 181, 220});
}

} // namespace ian
