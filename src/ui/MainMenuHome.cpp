#include "ui/MainMenuHome.hpp"

#include "localization/Localization.hpp"
#include "ui/UiRoundedRect.hpp"
#include "ui/UiChamferedPlate.hpp"
#include "ui/UiText.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/UiNotificationBadge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace ian {
namespace {

using menuStyle::Cream;
using menuStyle::Gold;
using menuStyle::Green;
constexpr float TextScale = 1.68F;
constexpr float SelectionSeconds = MainMenuSelectionSeconds;
constexpr float MenuEdgeMargin = 40.0F;
constexpr float FooterHeight = 72.0F;
constexpr float FooterGap = 12.0F;
constexpr float FooterWidth = 275.0F;
constexpr float ContinueButtonHeight = 92.0F;
constexpr float PlayButtonHeight = 120.0F;
constexpr float SecondaryButtonWidth = 230.0F;
constexpr float SecondaryButtonStep = SecondaryButtonWidth + 10.0F;
constexpr float SecondaryMiddleLeft = 960.0F - SecondaryButtonWidth * 0.5F;
constexpr float ContinueTop = 403.0F;
constexpr float PlayTop = 506.0F;
constexpr float SecondaryTop = 638.0F;

struct HomeButton {
    Rectangle bounds;
    std::string_view label;
    float pixels;
    MainMenuHomeAction action;
};
constexpr std::array<HomeButton, 7> HomeButtons{{
    {{830, PlayTop, 260, PlayButtonHeight}, "PLAY", 66, MainMenuHomeAction::Play},
    {{SecondaryMiddleLeft - SecondaryButtonStep, SecondaryTop, SecondaryButtonWidth, 85},
        "UPGRADES", 37, MainMenuHomeAction::Unlocks},
    {{SecondaryMiddleLeft, SecondaryTop, SecondaryButtonWidth, 86},
        "QUESTS", 40, MainMenuHomeAction::Quests},
    {{SecondaryMiddleLeft + SecondaryButtonStep, SecondaryTop, SecondaryButtonWidth, 86},
        "SHOP", 40, MainMenuHomeAction::Shop},
    {{MenuEdgeMargin,
      1080 - MenuEdgeMargin - (2 * FooterHeight + FooterGap),
      FooterWidth, FooterHeight}, "SETTINGS", 36, MainMenuHomeAction::Settings},
    {{MenuEdgeMargin,
      1080 - MenuEdgeMargin - FooterHeight,
      FooterWidth - 20, FooterHeight}, "EXIT", 36, MainMenuHomeAction::Exit},
    {{790, ContinueTop, 340, ContinueButtonHeight}, "CONTINUE", 56, MainMenuHomeAction::Continue},
}};

struct Layout {
    float scale;
    Vector2 origin;
    Rectangle rect(float x, float y, float width, float height) const {
        return {origin.x + x * scale, origin.y + y * scale,
                width * scale, height * scale};
    }
};

struct HomeLayout {
    Layout center;
    Layout topLeft;
    Layout footer;
    bool canContinue;

    // Keep animated focus geometry in the center's design coordinates,
    // so it can travel continuously between differently anchored groups.
    Rectangle buttonBounds(std::size_t index) const {
        const Layout& anchor = index >= 4U && index <= 5U ? footer : center;
        Rectangle bounds = HomeButtons[index].bounds;
        (void)canContinue;
        bounds.x += (anchor.origin.x - center.origin.x) / center.scale;
        bounds.y += (anchor.origin.y - center.origin.y) / center.scale;
        return bounds;
    }
};

void artwork(Texture2D texture, Rectangle bounds, Color tint = WHITE,
             float rotation = 0.0F) {
    if (!IsTextureValid(texture)) return;
    // Rotate about the slot's center so all four Figma focus corners retain
    // their original 20x20 geometry.
    DrawTexturePro(texture,
        {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
        {bounds.x + bounds.width * 0.5F, bounds.y + bounds.height * 0.5F,
         bounds.width, bounds.height},
        {bounds.width * 0.5F, bounds.height * 0.5F}, rotation, tint);
}

void text(std::string_view value, Rectangle bounds, float pixels,
          const Layout& layout, UiFontRole role = UiFontRole::Display,
          Color color = Cream, int align = 0, float outline = 2.5F) {
    const ScopedUiFontRole font(role);
    const float size = fitUiTextSize(value, pixels / TextScale * layout.scale,
        pixels * 0.5F / TextScale * layout.scale, bounds.width, bounds.height);
    const Vector2 measured = measureUiText(value, size);
    const float x = align == 1 ? bounds.x + (bounds.width - measured.x) * 0.5F
        : align == 2 ? bounds.x + bounds.width - measured.x : bounds.x;
    SdfUiTextStyle style{};
    style.fillColor = color;
    style.outlineColor = {18, 18, 15, 255};
    style.outlinePixels = outline * layout.scale;
    style.shadowColor = BLACK;
    style.shadowOffsetPixels = {0, pixels * 0.0625F * layout.scale};
    style.softness = 0.6F;
    drawStyledSdfUiText(value,
        {x, bounds.y + (bounds.height - measured.y) * 0.5F}, size, style);
}

void classicPlate(Rectangle bounds, float scale, Color fill) {
    const float cut = 17.0F * scale;
    drawChamferedFill({bounds.x, bounds.y + 4.0F * scale,
        bounds.width, bounds.height}, cut, BLACK);
    drawChamferedFill(bounds, cut, BLACK);
    drawChamferedFill({bounds.x + 5.0F * scale,
        bounds.y + 5.0F * scale,
        bounds.width - 10.0F * scale,
        bounds.height - 10.0F * scale},
        cut - 3.0F * scale, fill);
}

void button(Rectangle bounds,
            std::string_view label, float pixels, const Layout& layout,
            const MainMenuHomeInput& input, float selection) {
    const bool hovered = CheckCollisionPointRec(input.pointer, bounds);
    const bool pressed = hovered && input.pointerDown;
    if (pressed) bounds.y += 2.0F * layout.scale;
    const float amount = std::clamp(selection, 0.0F, 1.0F);
    const float brightness = pressed ? 0.89F : hovered ? 1.08F : 1.0F;
    const auto channel = [amount, brightness](int base, int gold) {
        return static_cast<unsigned char>(std::clamp(
            (static_cast<float>(base) + static_cast<float>(gold - base) * amount) * brightness,
            0.0F, 255.0F));
    };
    const Color face{channel(59, 255), channel(68, 194), channel(69, 0), 255};
    classicPlate(bounds, layout.scale, face);
    text(label, {bounds.x + 16 * layout.scale, bounds.y,
        bounds.width - 32 * layout.scale, bounds.height - 4 * layout.scale},
        pixels, layout, UiFontRole::Display, Cream, 1, 3.0F);
}

void updateSelection(MainMenuHomeState& state, const HomeLayout& anchors,
                     const MainMenuHomeInput& input, bool canContinue) {
    const float dt = std::isfinite(input.deltaTime)
        ? std::clamp(input.deltaTime, 0.0F, 0.25F) : 0.0F;
    state.activationRemaining = std::max(0.0F, state.activationRemaining - dt);
    const auto select = [&](std::size_t index) {
        if (state.selectedButton == index) return;
        state.selectedButton = index;
        state.focusStart = state.focusBounds;
        state.selectionStart = state.selectionAmounts;
        state.selectionElapsed = 0.0F;
    };
    if (!canContinue && state.selectedButton == 6U) {
        select(0);
        state.pendingAction = MainMenuHomeAction::None;
    }
    if (input.tabStep || input.horizontalStep || input.verticalStep) {
        state.pendingAction = MainMenuHomeAction::None;
        const std::array<std::size_t, 7> order{6, 0, 1, 2, 3, 4, 5};
        const int first = canContinue ? 0 : 1;
        const int count = 7 - first;
        const auto current = std::find(order.begin() + first, order.end(), state.selectedButton);
        const int position = static_cast<int>(current - order.begin()) - first;
        if (input.tabStep) {
            select(order[static_cast<std::size_t>(first + (position + input.tabStep + count) % count)]);
        } else if (input.horizontalStep && state.selectedButton >= 1 && state.selectedButton <= 3) {
            select(static_cast<std::size_t>(1 + (static_cast<int>(state.selectedButton) - 1 + input.horizontalStep + 3) % 3));
        } else if (input.horizontalStep) {
            select(order[static_cast<std::size_t>(first + (position + input.horizontalStep + count) % count)]);
        } else if (input.verticalStep) {
            const auto index = state.selectedButton;
            if (input.verticalStep > 0) select(index == 6 ? 0 : index == 0 ? 2 : index <= 3 ? 4 : index == 4 ? 5 : canContinue ? 6 : 0);
            else select(index == 5 ? 4 : index == 4 ? 1 : index >= 1 && index <= 3 ? 0 : index == 0 && canContinue ? 6 : 5);
        }
    }
    if (input.activate) {
        state.pendingAction = HomeButtons[state.selectedButton].action;
        state.activationRemaining = std::max(0.04F, SelectionSeconds - state.selectionElapsed);
    }
    if (input.click || (input.pointerDown && !state.pointerWasDown)) {
        state.pendingAction = MainMenuHomeAction::None;
        for (std::size_t index = 0; index < HomeButtons.size(); ++index) {
            if (index == 6U && !canContinue) continue;
            const auto& slot = HomeButtons[index];
            const Rectangle bounds = anchors.buttonBounds(index);
            if (CheckCollisionPointRec(input.pointer, anchors.center.rect(
                    bounds.x, bounds.y, bounds.width, bounds.height))) {
                select(index);
                if (input.click) {
                    state.pendingAction = slot.action;
                    state.activationRemaining = std::max(0.04F,
                        SelectionSeconds - state.selectionElapsed);
                }
                break;
            }
        }
    }
    state.pointerWasDown = input.pointerDown;
    state.selectionElapsed = std::min(SelectionSeconds, state.selectionElapsed + dt);
    const float t = state.selectionElapsed / SelectionSeconds;
    const float eased = t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
    const auto mix = [eased](float from, float to) { return from + (to - from) * eased; };
    const Rectangle target = anchors.buttonBounds(state.selectedButton);
    state.focusBounds = {mix(state.focusStart.x, target.x), mix(state.focusStart.y, target.y),
        mix(state.focusStart.width, target.width), mix(state.focusStart.height, target.height)};
    for (std::size_t index = 0; index < state.selectionAmounts.size(); ++index) {
        state.selectionAmounts[index] = mix(state.selectionStart[index],
            state.selectedButton == index ? 1.0F : 0.0F);
    }
}

void focusCorners(const GameUi& ui, Rectangle bounds, const Layout& layout) {
    const float left = bounds.x - 9;
    const float right = bounds.x + bounds.width - 11;
    const float top = bounds.y - 10;
    const float bottom = bounds.y + bounds.height - 11;
    artwork(ui.mainMenuTexture(MainMenuTexture::FocusTopLeft), layout.rect(left, top, 20, 20));
    artwork(ui.mainMenuTexture(MainMenuTexture::FocusBottomRight), layout.rect(right, bottom, 20, 20), WHITE, 180);
    artwork(ui.mainMenuTexture(MainMenuTexture::FocusBottomLeft), layout.rect(left, bottom, 20, 20), WHITE, -90);
    artwork(ui.mainMenuTexture(MainMenuTexture::FocusTopRight), layout.rect(right, top, 20, 20), WHITE, 90);
}

std::string number(double value) {
    value = std::clamp(value, 0.0, 2147483647.0);
    if (value == std::floor(value)) return std::to_string(static_cast<int>(value));
    std::array<char, 32> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.1f", value);
    return buffer.data();
}

MainMenuHomeResult questWidget(const GameUi& ui,
    std::span<const QuestStatus> featured, const MainMenuHomeState& state,
    const Layout& layout, const MainMenuHomeInput& input) {
    const Rectangle header = layout.rect(45, 68, 470, 70);
    classicPlate(header, layout.scale, {41, 55, 49, 255});
    text("QUESTS", layout.rect(65, 78, 260, 44), 43,
        layout, UiFontRole::Display, Cream, 0, 2.5F);
    text(std::to_string(state.completedQuestCount) + "/" + std::to_string(state.totalQuestCount),
        layout.rect(365, 85, 126, 36), 29, layout, UiFontRole::MenuBodyBold, Gold, 2, 1.6F);
    MainMenuHomeResult result;
    std::size_t row = 0;
    for (const auto& status : featured) {
        if (!status.definition) continue;
        if (row == MainMenuFeaturedQuestCount) break;
        const float offset = 105.0F * static_cast<float>(row++);
        const Rectangle body = layout.rect(45, 134 + offset, 470, 112);
        const bool hovered = CheckCollisionPointRec(input.pointer, body);
        classicPlate(body, layout.scale,
            hovered ? Color{56, 68, 63, 255} : Color{34, 43, 41, 255});
        const auto& definition = *status.definition;
        const Texture2D icon = ui.mainMenuTexture(MainMenuTexture::Gear);
        artwork(icon, layout.rect(63, 148 + offset, 78, 78));
        text(definition.title, layout.rect(158, 144 + offset, 337, 36), 30,
            layout, UiFontRole::MenuBodyBold, WHITE, 0, 1.6F);
        const Rectangle track = layout.rect(158, 190 + offset, 217, 18);
        drawSolidRoundedRect(track, 0.75F, BLACK);
        const Rectangle inner{track.x + 2 * layout.scale, track.y + 2 * layout.scale,
            track.width - 4 * layout.scale, track.height - 4 * layout.scale};
        drawSolidRoundedRect(inner, 0.6F, {174, 179, 173, 255});
        const float fraction = static_cast<float>(std::clamp(status.fraction, 0.0, 1.0));
        if (fraction > 0) drawSolidRoundedRect(
            {inner.x, inner.y, inner.width * fraction, inner.height}, 0.6F, Green);
        std::string counter;
        if (status.rewardAvailable) counter = localizeText("COLLECT");
        else for (std::size_t index = 0; index < definition.conditions.size(); ++index) {
            const auto& goal = definition.conditions[index];
            if (goal.comparison != QuestComparison::AtLeast) continue;
            counter = number(index < status.progress.size() ? status.progress[index] : 0) +
                "/" + number(goal.target);
            break;
        }
        text(counter, layout.rect(385, 185 + offset, 107, 30), 28,
            layout, UiFontRole::MenuBodyBold, Green, 2, 1.6F);
        if (input.click && hovered) {
            result.action = MainMenuHomeAction::Quests;
            result.questId = definition.id;
        }
    }
    if (row == 0) {
        classicPlate(layout.rect(45, 134, 470, 112), layout.scale, {34, 43, 41, 255});
        text(state.allQuestsCompleted ? "ALL QUESTS COMPLETED" : "NO AVAILABLE QUESTS",
            layout.rect(65, 155, 425, 66), 26, layout, UiFontRole::MenuBodyBold, Cream, 1);
    }
    if (input.click && CheckCollisionPointRec(input.pointer, header)) {
        result.action = MainMenuHomeAction::Quests;
        result.questId.clear();
    }
    return result;
}

void footerButton(const GameUi& ui, std::size_t index, Rectangle bounds,
                  const Layout& layout, const MainMenuHomeInput& input, float selection) {
    const bool hovered = CheckCollisionPointRec(input.pointer, bounds);
    const float amount = hovered ? 1.0F : selection;
    const Color ink{
        static_cast<unsigned char>(Cream.r + (Gold.r - Cream.r) * amount),
        static_cast<unsigned char>(Cream.g + (Gold.g - Cream.g) * amount),
        static_cast<unsigned char>(Cream.b + (Gold.b - Cream.b) * amount), 255};
    const Texture2D icon = ui.mainMenuTexture(index == 4U
        ? MainMenuTexture::SettingsIcon : MainMenuTexture::ExitIcon);
    const Rectangle iconBounds{bounds.x + 16 * layout.scale,
        bounds.y + (bounds.height - 50 * layout.scale) * 0.5F,
        50 * layout.scale, 50 * layout.scale};
    constexpr std::array<Vector2, 8> OutlineOffsets{{
        {-1,-1}, {0,-1}, {1,-1}, {-1,0}, {1,0}, {-1,1}, {0,1}, {1,1},
    }};
    for (const Vector2 offset : OutlineOffsets) {
        artwork(icon, {iconBounds.x + offset.x * 2 * layout.scale,
            iconBounds.y + offset.y * 2 * layout.scale, iconBounds.width, iconBounds.height}, BLACK);
    }
    artwork(icon, iconBounds, ink);
    const float inset = 80 * layout.scale;
    text(HomeButtons[index].label,
        {bounds.x + inset, bounds.y, bounds.width - inset, bounds.height},
        HomeButtons[index].pixels, layout, UiFontRole::Display, ink, 0, 2.8F);
}

struct LeaderboardEntry {
    std::string_view player;
    int wave;
    bool localPlayer{};
};

// Design fixtures only. Steam results will replace this data source; drawing
// and tab selection do not read or write the player's actual run records.
constexpr std::array<LeaderboardEntry, 10> PreviewGlobalLeaderboard{{
    {"Bonker", 342}, {"spaghetti1", 321}, {"lorenzo", 317},
    {"judassy", 305}, {"angelboy", 287}, {"lilith", 283},
    {"mommy", 270}, {"you", 265, true}, {"rocky", 253}, {"Nexty", 249},
}};
constexpr std::array<LeaderboardEntry, 10> PreviewFriendsLeaderboard{{
    {"Bonker", 342}, {"you", 265, true}, {"rocky", 253},
    {"Nexty", 249}, {"Alex", 231}, {"Luna", 218},
    {"Max", 204}, {"Sam", 193}, {"Robin", 181}, {"Kai", 172},
}};

void leaderboard(MainMenuHomeState& state,
                 const Layout& layout, const MainMenuHomeInput& input, float plateScale) {
    constexpr float TabWidth = 169.5F;
    // Update first so the active tab and its rows change in the same frame.
    for (int tab = 0; tab < 2; ++tab) {
        const auto bounds = layout.rect(1537 + static_cast<float>(tab) * TabWidth,
            640, TabWidth, 39.5F);
        if (input.click && CheckCollisionPointRec(input.pointer, bounds)) {
            state.friendsLeaderboardVisible = tab == 1;
        }
    }
    classicPlate(layout.rect(1520.07F, 574.44F, 369.356F, 531.537F),
        plateScale, {30, 39, 39, 255});
    classicPlate(layout.rect(1510.84F, 572.59F, 387.819F, 77.8306F),
        plateScale, {41, 55, 49, 255});
    text("LEADERBOARDS", layout.rect(1534, 588, 342, 49), 39.0F,
        layout, UiFontRole::Display, Cream, 1, 3.0F);
    for (int tab = 0; tab < 2; ++tab) {
        const bool active = state.friendsLeaderboardVisible == (tab == 1);
        classicPlate(layout.rect(1537 + static_cast<float>(tab) * TabWidth,
            640, TabWidth + 4.5F, 53.0F), plateScale,
            active ? Color{255, 187, 13, 255} : Color{61, 70, 72, 255});
    }
    drawChamferedFill(layout.rect(1528, 694, 354, 400), 12 * layout.scale,
        {25, 33, 32, 255});
    for (int tab = 0; tab < 2; ++tab) {
        text(tab == 0 ? "GLOBAL" : "FRIENDS",
            layout.rect(1537 + static_cast<float>(tab) * TabWidth, 648, TabWidth, 36),
            25.4F, layout, UiFontRole::Display,
            state.friendsLeaderboardVisible == (tab == 1) ? Cream : Color{189, 194, 193, 255},
            1, 1.6F);
    }
    text("#", layout.rect(1544, 697, 76, 39), 26,
        layout, UiFontRole::Display, Cream, 1, 1.8F);
    text("PLAYER", layout.rect(1628, 697, 167, 39), 26,
        layout, UiFontRole::Display, Cream, 0, 1.8F);
    text("WAVE", layout.rect(1799, 697, 74, 39), 26,
        layout, UiFontRole::Display, Cream, 1, 1.8F);

    const auto& entries = state.friendsLeaderboardVisible
        ? PreviewFriendsLeaderboard : PreviewGlobalLeaderboard;
    constexpr float Top = 740;
    constexpr std::size_t VisibleRows = 8;
    constexpr float RowHeight = 350.0F / static_cast<float>(VisibleRows);
    constexpr Color DarkRow = menuStyle::Inset;
    constexpr Color LightRow = menuStyle::AlternateRow;
    drawSolidRoundedRect(layout.rect(1537, Top, 339, RowHeight * static_cast<float>(VisibleRows)), 0.08F, DarkRow);
    for (std::size_t index = 0; index < VisibleRows; ++index) {
        const auto& entry = entries[index];
        const float y = Top + static_cast<float>(index) * RowHeight;
        const Rectangle bounds = layout.rect(1537, y, 339, RowHeight);
        if (entry.localPlayer || index % 2U != 0U) {
            const Color fill = entry.localPlayer ? Color{172, 139, 24, 255} : LightRow;
            if (index == VisibleRows - 1U) {
                drawSolidRoundedRect(bounds, 0.9F, fill);
                DrawRectangleRec({bounds.x, bounds.y, bounds.width, bounds.height * 0.5F}, fill);
            } else {
                DrawRectangleRec(bounds, fill);
            }
        }
        const Color ink = entry.localPlayer ? Gold : WHITE;
        text(std::to_string(index + 1U), layout.rect(1544, y, 76, RowHeight), 29,
            layout, UiFontRole::MenuBodyBold, ink, 1, 1.8F);
        text(entry.player, layout.rect(1628, y, 167, RowHeight), 29,
            layout, UiFontRole::MenuBodyBold, ink, 0, 1.8F);
        text(std::to_string(entry.wave), layout.rect(1799, y, 74, RowHeight), 29,
            layout, UiFontRole::MenuBodyBold, ink, 1, 1.8F);
    }
}

} // namespace

MainMenuHomeResult drawMainMenuHome(
    const GameUi& ui, const MetaProgression& profile,
    std::span<const QuestStatus> featured, bool canContinue,
    MainMenuHomeState& state, Vector2 viewport, const MainMenuHomeInput& input) {
    const float scale = std::max(0.1F, std::min(viewport.x / 1920.0F, viewport.y / 1080.0F));
    const Layout layout{scale, {(viewport.x - 1920 * scale) * 0.5F,
                               (viewport.y - 1080 * scale) * 0.5F}};
    const float margin = MenuEdgeMargin * scale;
    const HomeLayout anchors{
        layout, {scale, {margin - 45 * scale, margin - 70 * scale}},
        {scale, {0, viewport.y - 1060 * scale}},
        canContinue,
    };
    MainMenuHomeResult result;
    updateSelection(state, anchors, input, canContinue);
    const Texture2D logo = ui.mainMenuLogoTexture();
    if (IsTextureValid(logo)) {
        const float logoHeight = 800.0F * static_cast<float>(logo.height) / static_cast<float>(logo.width);
        artwork(logo, layout.rect(560, 147, 800, logoHeight));
    }
    const std::string silver = std::to_string(std::max(0, profile.silver));
    const ScopedUiFontRole display(UiFontRole::Display);
    const Rectangle silverPlate{viewport.x - margin - 110 * scale,
        margin - 15 * scale, 110 * scale, 72 * scale};
    artwork(ui.mainMenuTexture(MainMenuTexture::Silver),
        {silverPlate.x + 3 * scale, silverPlate.y + 10 * scale,
         48 * scale, 48 * scale});
    text(silver, {silverPlate.x + 51 * scale, silverPlate.y,
        silverPlate.width - 51 * scale, silverPlate.height - 5 * scale},
        48, layout, UiFontRole::Display, Cream, 1, 2.88F);
    result = questWidget(ui, featured, state, anchors.topLeft, input);
    for (std::size_t index = 0; index < HomeButtons.size(); ++index) {
        if (index == 6U && !canContinue) continue;
        const auto& slot = HomeButtons[index];
        const Rectangle bounds = anchors.buttonBounds(index);
        const Rectangle screenBounds = layout.rect(bounds.x, bounds.y, bounds.width, bounds.height);
        if (index >= 4U && index <= 5U) footerButton(ui, index, screenBounds, layout,
            input, state.selectionAmounts[index]);
        else button(screenBounds, slot.label, slot.pixels,
            layout, input, state.selectionAmounts[index]);
        if (slot.action == MainMenuHomeAction::Quests) {
            drawUiNotificationBadge(
                {screenBounds.x + screenBounds.width - 16 * scale, screenBounds.y + 12 * scale},
                16 * scale, state.unclaimedQuestRewards);
        }
    }
    focusCorners(ui, state.focusBounds, layout);
    constexpr float leaderboardScale = 1.04F;
    const float leaderboardUiScale = scale * leaderboardScale;
    const float leaderboardBottom = viewport.y - margin - 65 * scale;
    const Layout leaderboardLayout{scale * leaderboardScale,
        {viewport.x - margin - (1510.84F + 387.819F) * leaderboardUiScale,
         leaderboardBottom - 1052.975F * leaderboardUiScale}};
    leaderboard(state, leaderboardLayout, input, scale);
    if (result.action != MainMenuHomeAction::None) {
        state.pendingAction = MainMenuHomeAction::None;
    } else if (state.pendingAction != MainMenuHomeAction::None &&
               state.activationRemaining <= 0.0F) {
        result.action = state.pendingAction;
        state.pendingAction = MainMenuHomeAction::None;
    }
    return result;
}

} // namespace ian
