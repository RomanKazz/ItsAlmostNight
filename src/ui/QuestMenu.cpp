#include "ui/QuestMenu.hpp"

#include "buildings/BuildingSystem.hpp"
#include "localization/Localization.hpp"
#include "progression/MetaProgression.hpp"
#include "progression/RunUpgrade.hpp"
#include "ui/UiText.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace ian {
namespace {

constexpr Color Cream{250, 241, 211, 255};
constexpr Color Gold{255, 205, 105, 255};
constexpr Color Muted{161, 166, 154, 255};
constexpr Color Green{131, 215, 138, 255};
constexpr Color Red{244, 102, 84, 255};
constexpr float RowStep = 68.0F;
constexpr float ListHeight = 472.0F;

struct Layout {
    float scale;
    Vector2 origin;
    Rectangle rect(float x, float y, float width, float height) const {
        return {origin.x + x * scale, origin.y + y * scale, width * scale, height * scale};
    }
};

Layout layoutFor(Vector2 viewport) {
    const float scale = std::max(0.1F, std::min(viewport.x / 1920.0F, viewport.y / 1080.0F));
    return {scale, {(viewport.x - 1920.0F * scale) * 0.5F, (viewport.y - 1080.0F * scale) * 0.5F}};
}

constexpr std::array<std::string_view, 9> TabNames{{
    "ALL", "CLASSES", "BUILDINGS", "WEAPONS", "UPGRADES", "ITEMS", "GENERAL", "CHALLENGES", "COSMETICS",
}};

std::size_t tabIndex(const QuestMenuState& state) {
    return state.category ? static_cast<std::size_t>(*state.category) + 1 : 0;
}

void selectTab(QuestMenuState& state, std::size_t index) {
    state.category = index == 0 ? std::nullopt : std::optional{static_cast<QuestCategory>(index - 1)};
    state.scroll = 0.0F;
    state.selectedId.clear();
    state.draggingScroll = false;
}

void panel(Rectangle bounds, float scale, Color fill, Color border = {116, 79, 48, 255}) {
    DrawRectangleRounded(bounds, 0.065F, 8, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.065F, 8, 2.0F * scale, border);
}

void text(std::string_view value, Rectangle bounds, float size, Color color,
          UiFontRole role = UiFontRole::Body, int alignment = 0) {
    const ScopedUiFontRole font(role);
    const float fitted = fitUiTextSize(value, size, size * 0.60F, bounds.width, bounds.height);
    const Vector2 measured = measureUiText(value, fitted);
    const float x = alignment == 1 ? bounds.x + (bounds.width - measured.x) * 0.5F
        : alignment == 2 ? bounds.x + bounds.width - measured.x : bounds.x;
    drawUiText(value, {x, bounds.y + (bounds.height - measured.y) * 0.5F}, fitted, color);
}

bool button(Rectangle bounds, std::string_view label, const Layout& layout,
            const QuestMenuInput& input, bool enabled = true) {
    const bool hovered = enabled && CheckCollisionPointRec(input.pointer, bounds);
    panel(bounds, layout.scale, !enabled ? Color{46, 50, 51, 255}
        : hovered ? Color{212, 128, 51, 255} : Color{159, 88, 38, 255},
        enabled ? Color{239, 185, 94, 255} : Color{88, 90, 84, 255});
    text(label, {bounds.x + 8 * layout.scale, bounds.y,
        bounds.width - 16 * layout.scale, bounds.height}, 15 * layout.scale,
        enabled ? Cream : Muted, UiFontRole::BodyBold, 1);
    return hovered && input.click;
}

void bar(Rectangle bounds, double fraction, Color color) {
    DrawRectangleRounded(bounds, 0.9F, 6, {17, 23, 26, 255});
    if (fraction > 0.0) DrawRectangleRounded(
        {bounds.x, bounds.y, bounds.width * static_cast<float>(std::clamp(fraction, 0.0, 1.0)), bounds.height},
        0.9F, 6, color);
}

void check(Vector2 center, float size, Color color) {
    DrawLineEx({center.x - size * 0.4F, center.y},
        {center.x - size * 0.1F, center.y + size * 0.3F}, size * 0.14F, color);
    DrawLineEx({center.x - size * 0.1F, center.y + size * 0.3F},
        {center.x + size * 0.45F, center.y - size * 0.35F}, size * 0.14F, color);
}

void icon(const GameUi& ui, const QuestDefinition& definition, Rectangle bounds, float scale) {
    Texture2D texture{};
    for (const auto& condition : definition.conditions) {
        if (condition.subject == "gun_turret") texture = ui.buildingTexture(BuildingType::GunTurret);
        else if (condition.subject == "cannon") texture = ui.buildingTexture(BuildingType::Cannon);
        else if (condition.subject == "turret") texture = ui.buildingTexture(BuildingType::Turret);
        else if (condition.subject == "spike_trap") texture = ui.buildingTexture(BuildingType::SpikeTrap);
        else if (condition.subject == "defense_damage") texture = ui.runUpgradeTexture(RunUpgradeEffect::DefenseDamage);
        else if (condition.subject == "defense_fire_rate") texture = ui.runUpgradeTexture(RunUpgradeEffect::DefenseFireRate);
        else if (condition.subject == "ricochet") texture = ui.runUpgradeTexture(RunUpgradeEffect::Ricochet);
        else if (condition.subject == "cluster_payload") texture = ui.runUpgradeTexture(RunUpgradeEffect::ClusterPayload);
        if (texture.id != 0) break;
    }
    if (texture.id == 0) {
        switch (definition.category) {
        case QuestCategory::Characters: texture = ui.coreMenuTexture(CoreMenuTexture::Shield); break;
        case QuestCategory::Buildings: texture = ui.coreMenuTexture(CoreMenuTexture::Wrench); break;
        case QuestCategory::Weapons: texture = ui.coreMenuTexture(CoreMenuTexture::Turret); break;
        case QuestCategory::Upgrades: texture = ui.coreMenuTexture(CoreMenuTexture::Upgrade); break;
        case QuestCategory::Items: texture = ui.coreMenuTexture(CoreMenuTexture::Gem); break;
        case QuestCategory::General: texture = ui.coreMenuTexture(CoreMenuTexture::Gear); break;
        case QuestCategory::Challenges: texture = ui.coreMenuTexture(CoreMenuTexture::Spikes); break;
        case QuestCategory::Cosmetics: texture = ui.coreMenuTexture(CoreMenuTexture::Heart); break;
        }
    }
    panel(bounds, scale, {27, 31, 34, 255}, {82, 87, 79, 255});
    if (texture.id != 0) {
        const float size = std::min(bounds.width - 8 * scale, bounds.height - 8 * scale);
        DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
            {bounds.x + (bounds.width - size) * 0.5F, bounds.y + (bounds.height - size) * 0.5F, size, size}, {}, 0, WHITE);
    } else {
        DrawPoly({bounds.x + bounds.width * 0.5F, bounds.y + bounds.height * 0.5F},
            6, bounds.width * 0.28F, 30.0F, Gold);
    }
}

void silver(const GameUi& ui, const Layout& layout, float x, float y, int amount, float size = 28) {
    ui.drawResourceIcon(layout.rect(x, y, size, size), UiResourceIcon::Silver);
    text(std::to_string(amount), layout.rect(x + size + 7, y, 80, size), size * 0.64F * layout.scale,
        Cream, UiFontRole::BodyBold);
}

std::string number(double value) {
    if (value == std::floor(value)) return std::to_string(static_cast<int>(std::min(value, 2147483647.0)));
    std::array<char, 40> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.1f", value);
    return buffer.data();
}

std::string counter(const QuestStatus& row) {
    const auto& conditions = row.definition->conditions;
    for (std::size_t i = 0; i < conditions.size(); ++i) {
        if (conditions[i].comparison == QuestComparison::AtLeast) {
            return number(row.progress[i]) + " / " + number(conditions[i].target);
        }
    }
    return {};
}

std::string_view unlockName(std::string_view id) {
    if (id == "class.berserker") return "BERSERKER";
    if (id == "class.vampire") return "VAMPIRE";
    if (id == "class.alchemist") return "ALCHEMIST";
    if (id == "class.chronomancer") return "CHRONOMANCER";
    if (id == "meta.refresh") return "CARD REFRESH";
    if (id == "meta.skip") return "CARD SKIP";
    if (id == "meta.banish") return "CARD BANISH";
    return "PERMANENT UNLOCK";
}

} // namespace

Rectangle questMenuPanelBounds(Vector2 viewport) {
    return layoutFor(viewport).rect(180, 90, 1560, 890);
}

bool drawQuestSummaryCard(
    const GameUi& ui, const QuestStatus& status, Rectangle bounds, float scale,
    Vector2 pointer, bool click) {
    const bool hovered = CheckCollisionPointRec(pointer, bounds);
    panel(bounds, scale, hovered ? Color{62, 69, 68, 250} : Color{53, 58, 59, 245},
        hovered ? Gold : Color{124, 98, 71, 255});
    icon(ui, *status.definition,
        {bounds.x + 9 * scale, bounds.y + 17 * scale, 42 * scale, 42 * scale}, scale);
    const float x = bounds.x + 62 * scale;
    const float width = bounds.width - 76 * scale;
    const auto heading = status.definition->reward.unlockId.starts_with("class.")
        ? unlockName(status.definition->reward.unlockId)
        : TabNames[static_cast<std::size_t>(status.definition->category) + 1];
    text(heading, {x, bounds.y + 6 * scale, width, 20 * scale},
        13 * scale, Gold, UiFontRole::BodyBold);

    // Split long descriptions at a word boundary into two balanced lines.
    const ScopedUiFontRole body(UiFontRole::Body);
    const std::string title = localizeText(status.definition->title);
    std::size_t split = std::string::npos;
    float longest = measureUiText(title, 12 * scale).x;
    if (longest > width) {
        for (std::size_t at = title.find(' '); at != std::string::npos; at = title.find(' ', at + 1)) {
            const float candidate = std::max(measureUiText(title.substr(0, at), 12 * scale).x,
                measureUiText(title.substr(at + 1), 12 * scale).x);
            if (candidate < longest) { longest = candidate; split = at; }
        }
    }
    if (split == std::string::npos) {
        text(title, {x, bounds.y + 27 * scale, width, 28 * scale}, 12 * scale, Cream);
    } else {
        text(title.substr(0, split), {x, bounds.y + 26 * scale, width, 16 * scale}, 12 * scale, Cream);
        text(title.substr(split + 1), {x, bounds.y + 42 * scale, width, 16 * scale}, 12 * scale, Cream);
    }
    text(counter(status), {x, bounds.y + 61 * scale, width, 16 * scale},
        12 * scale, Green, UiFontRole::BodyBold, 2);
    bar({x, bounds.y + bounds.height - 14 * scale, width, 7 * scale}, status.fraction, Green);
    return hovered && click;
}

QuestMenuResult drawQuestMenu(
    const GameUi& ui, const QuestSystem& quests, MetaProgression& profile,
    QuestMenuState& state, Vector2 viewport, const QuestMenuInput& input) {
    const auto layout = layoutFor(viewport);
    QuestMenuResult result{.close = input.close};
    if (input.categoryStep != 0) {
        const int index = (static_cast<int>(tabIndex(state)) + input.categoryStep + 9) % 9;
        selectTab(state, static_cast<std::size_t>(index));
        result.interaction = true;
    }
    DrawRectangle(0, 0, static_cast<int>(viewport.x), static_cast<int>(viewport.y), {7, 11, 13, 175});
    const auto bounds = questMenuPanelBounds(viewport);
    panel({bounds.x + 5 * layout.scale, bounds.y + 7 * layout.scale, bounds.width, bounds.height},
        layout.scale, {9, 11, 13, 160}, {9, 11, 13, 160});
    panel(bounds, layout.scale, {29, 34, 38, 248});
    panel(layout.rect(194, 104, 1532, 84), layout.scale, {43, 62, 54, 255});
    text("QUESTS", layout.rect(218, 110, 360, 68), 32 * layout.scale, Cream, UiFontRole::Display);
    const auto all = quests.statuses(profile, state.completedLast);
    const auto done = static_cast<std::size_t>(std::ranges::count_if(all, [](const auto& row) { return row.completed; }));
    const auto total = all.size();
    const std::string summary = std::to_string(done) + " / " + std::to_string(total);
    text("COMPLETED", layout.rect(590, 115, 220, 28), 13 * layout.scale, Muted, UiFontRole::BodyBold);
    text(summary, layout.rect(810, 114, 230, 30), 17 * layout.scale, Cream, UiFontRole::BodyBold, 2);
    bar(layout.rect(590, 158, 450, 8), total == 0 ? 0 : static_cast<double>(done) / static_cast<double>(total), Green);
    silver(ui, layout, 1470, 122, profile.silver, 38);
    if (button(layout.rect(1653, 118, 52, 52), "X", layout, input)) result.close = true;

    for (std::size_t i = 0; i < TabNames.size(); ++i) {
        const float x = 204 + static_cast<float>(i) * 168;
        const auto tab = layout.rect(x, 204, 160, 62);
        if (CheckCollisionPointRec(input.pointer, tab) && input.click) {
            selectTab(state, i);
            result.interaction = true;
        }
        const auto category = i == 0 ? QuestCategoryProgress{done, total}
            : quests.categoryProgress(profile, static_cast<QuestCategory>(i - 1));
        const bool selected = tabIndex(state) == i;
        panel(tab, layout.scale, selected ? Color{61, 86, 75, 255} : Color{39, 44, 47, 255},
            selected ? Gold : Color{77, 82, 76, 255});
        text(TabNames[i], layout.rect(x + 8, 209, 144, 27), 13 * layout.scale,
            category.total == 0 ? Muted : selected ? Gold : Cream, UiFontRole::BodyBold, 1);
        text(std::to_string(category.completed) + " / " + std::to_string(category.total),
            layout.rect(x + 8, 238, 144, 19), 11 * layout.scale, category.completed > 0 ? Green : Muted,
            UiFontRole::Body, 1);
    }
    const auto sortBounds = layout.rect(204, 278, 300, 24);
    if (CheckCollisionPointRec(input.pointer, sortBounds) && input.click) {
        state.completedLast = !state.completedLast;
        state.scroll = 0;
        result.interaction = true;
    }
    panel(layout.rect(205, 279, 21, 21), layout.scale, {22, 29, 31, 255}, {120, 125, 109, 255});
    if (state.completedLast) check({layout.rect(215, 289, 0, 0).x, layout.rect(215, 289, 0, 0).y}, 17 * layout.scale, Green);
    text("COMPLETED LAST", layout.rect(238, 278, 260, 24), 12 * layout.scale, Cream);
    text("SCROLL  LIST     ARROWS  SELECT     TAB  CATEGORY     ESC  BACK",
        layout.rect(620, 278, 1080, 24), 11 * layout.scale, Muted, UiFontRole::Body, 2);

    // Rebuild order after a sort/tab click, retaining selection by stable ID.
    auto rows = quests.statuses(profile, state.completedLast);
    if (state.category) std::erase_if(rows, [&](const auto& row) { return row.definition->category != *state.category; });
    auto selected = std::ranges::find_if(rows, [&](const auto& row) { return row.definition->id == state.selectedId; });
    std::size_t selectedIndex = selected == rows.end() ? 0 : static_cast<std::size_t>(selected - rows.begin());
    if (!rows.empty()) {
        if (input.selectionStep != 0) {
            selectedIndex = static_cast<std::size_t>(std::clamp(
                static_cast<int>(selectedIndex) + input.selectionStep, 0, static_cast<int>(rows.size()) - 1));
            const float y = static_cast<float>(selectedIndex) * RowStep;
            if (y < state.scroll) state.scroll = y;
            if (y + 60 > state.scroll + ListHeight) state.scroll = y + 60 - ListHeight;
            result.interaction = true;
        }
        state.selectedId = rows[selectedIndex].definition->id;
    } else state.selectedId.clear();
    const auto list = layout.rect(204, 312, 1488, ListHeight);
    const float maximumScroll = std::max(0.0F, static_cast<float>(rows.size()) * RowStep - 8 - ListHeight);
    if (CheckCollisionPointRec(input.pointer, list)) state.scroll -= input.wheel * RowStep;
    const auto track = layout.rect(1700, 312, 10, ListHeight);
    const float thumbHeight = std::max(30.0F, ListHeight * ListHeight /
        std::max(ListHeight, static_cast<float>(rows.size()) * RowStep - 8));
    if (maximumScroll > 0 && input.click && CheckCollisionPointRec(input.pointer, track)) state.draggingScroll = true;
    if (!input.pointerDown) state.draggingScroll = false;
    if (state.draggingScroll && maximumScroll > 0) {
        state.scroll = (input.pointer.y - track.y - thumbHeight * layout.scale * 0.5F) /
            ((ListHeight - thumbHeight) * layout.scale) * maximumScroll;
    }
    state.scroll = std::clamp(state.scroll, 0.0F, maximumScroll);

    BeginScissorMode(static_cast<int>(list.x), static_cast<int>(list.y),
        static_cast<int>(list.width), static_cast<int>(list.height));
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        const float y = 312 + static_cast<float>(i) * RowStep - state.scroll;
        if (y + 60 <= 312 || y >= 312 + ListHeight) continue;
        const auto rowBounds = layout.rect(204, y, 1488, 60);
        const bool hovered = CheckCollisionPointRec(input.pointer, rowBounds) && CheckCollisionPointRec(input.pointer, list);
        if (hovered && input.click) {
            state.selectedId = row.definition->id;
            selectedIndex = i;
            result.interaction = true;
        }
        const bool active = row.definition->id == state.selectedId;
        panel(rowBounds, layout.scale, row.completed ? Color{45, 67, 65, 255}
            : active ? Color{60, 69, 71, 255} : hovered ? Color{49, 57, 61, 255} : Color{39, 44, 49, 255},
            active ? Gold : row.completed ? Color{91, 142, 111, 255} : Color{82, 89, 89, 255});
        icon(ui, *row.definition, layout.rect(213, y + 7, 46, 46), layout.scale);
        text(row.definition->title, layout.rect(278, y + 9, 835, 42), 18 * layout.scale,
            row.prerequisitesMet ? Cream : Muted, UiFontRole::BodyBold);
        text(counter(row), layout.rect(1140, y + 4, 265, 36), 17 * layout.scale,
            row.completed ? Green : Cream, UiFontRole::BodyBold, 1);
        bar(layout.rect(1140, y + 44, 265, 6), row.fraction, row.completed ? Green : Color{90, 179, 178, 255});
        if (row.completed) check({layout.rect(1628, y + 31, 0, 0).x, layout.rect(1628, y + 31, 0, 0).y}, 28 * layout.scale, Green);
        else {
            silver(ui, layout, 1540, y + 16, row.definition->reward.silver);
            if (!row.prerequisitesMet) text("LOCKED", layout.rect(1430, y + 17, 95, 26), 11 * layout.scale, Muted, UiFontRole::BodyBold, 1);
        }
    }
    EndScissorMode();
    if (rows.empty()) {
        text("NO QUESTS IN THIS CATEGORY YET", layout.rect(400, 477, 1100, 60), 20 * layout.scale,
            Muted, UiFontRole::BodyBold, 1);
    }
    if (maximumScroll > 0) {
        DrawRectangleRounded(track, 0.8F, 6, {17, 23, 26, 255});
        DrawRectangleRounded(layout.rect(1700, 312 + state.scroll / maximumScroll * (ListHeight - thumbHeight),
            10, thumbHeight), 0.8F, 6, {158, 166, 151, 255});
    }

    panel(layout.rect(204, 800, 1508, 154), layout.scale, {23, 29, 31, 255});
    if (!rows.empty()) {
        const auto& row = rows[selectedIndex];
        const auto& definition = *row.definition;
        icon(ui, definition, layout.rect(222, 819, 96, 96), layout.scale);
        text(definition.title, layout.rect(342, 814, 1060, 37), 20 * layout.scale, Gold, UiFontRole::BodyBold);
        const bool runGoal = std::ranges::any_of(definition.conditions, [](const auto& condition) { return condition.scope == QuestScope::Run; });
        const std::string description = definition.description != definition.title ? definition.description
            : runGoal ? "IN ONE RUN. UNFINISHED PROGRESS RESETS ON A NEW RUN."
                      : "LIFETIME GOAL. PROGRESS IS SAVED BETWEEN RUNS.";
        text(description, layout.rect(342, 858, 1060, 27), 13 * layout.scale, Cream);
        std::string detail = row.completed ? localizeText("COMPLETED — REWARD RECEIVED")
            : localizeText(runGoal ? "ONE RUN" : "ALL RUNS") + "   " + counter(row);
        if (!row.prerequisitesMet) {
            detail = localizeText("REQUIRES") + ": ";
            for (const auto& id : definition.prerequisites) {
                const auto found = std::ranges::find(quests.definitions(), id, &QuestDefinition::id);
                if (found != quests.definitions().end()) detail += localizeText(found->title) + "  ";
            }
        }
        if (!row.completed) for (std::size_t i = 0; i < definition.conditions.size(); ++i) {
            if (definition.conditions[i].comparison == QuestComparison::AtMost) {
                detail += "   " + localizeText("NO CHESTS");
                if (row.progress[i] > definition.conditions[i].target) detail += " — " + localizeText("FAILED THIS RUN");
            }
        }
        text(detail, layout.rect(342, 900, 1060, 28), 13 * layout.scale,
            row.completed ? Green : !row.prerequisitesMet ? Muted : Cream, UiFontRole::BodyBold);
        if (row.unlockAvailable && !row.unlockOwned && definition.reward.purchaseCost > 0) {
            const std::string label = localizeText("BUY") + "  " + std::to_string(definition.reward.purchaseCost);
            text(unlockName(definition.reward.unlockId), layout.rect(1428, 817, 261, 30), 15 * layout.scale, Gold, UiFontRole::BodyBold, 1);
            text("UNLOCK AVAILABLE", layout.rect(1428, 851, 261, 18), 10 * layout.scale, Green, UiFontRole::BodyBold, 1);
            const bool affordable = profile.silver >= definition.reward.purchaseCost;
            if (button(layout.rect(1437, 873, 252, 53), label, layout, input, affordable)) {
                result.purchase = quests.purchaseUnlock(profile, definition.reward.unlockId) == QuestPurchaseResult::Purchased;
                result.interaction = true;
            }
            if (!affordable) text("NOT ENOUGH SILVER", layout.rect(1428, 933, 261, 17),
                10 * layout.scale, Red, UiFontRole::BodyBold, 1);
        } else if (row.unlockOwned) {
            text(unlockName(definition.reward.unlockId), layout.rect(1428, 818, 261, 35), 15 * layout.scale, Gold, UiFontRole::BodyBold, 1);
            text("UNLOCKED", layout.rect(1428, 873, 261, 30), 16 * layout.scale, Green, UiFontRole::BodyBold, 1);
        } else {
            const auto label = !definition.reward.unlockId.empty() ? unlockName(definition.reward.unlockId)
                : row.completed ? std::string_view{"REWARD RECEIVED"} : std::string_view{"REWARD"};
            text(label, layout.rect(1428, 822, 261, 30),
                12 * layout.scale, row.completed ? Green : Muted, UiFontRole::BodyBold, 1);
            silver(ui, layout, 1505, 871, definition.reward.silver, 38);
        }
    }
    return result;
}

} // namespace ian
