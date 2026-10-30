#include "ui/UiMenuLayout.hpp"
#include "ui/UiViewport.hpp"
#include "ui/QuestMenu.hpp"
#include "progression/ContentUnlocks.hpp"

#include "buildings/BuildingSystem.hpp"
#include "localization/Localization.hpp"
#include "progression/MetaProgression.hpp"
#include "progression/RunUpgrade.hpp"
#include "ui/UiText.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/UiNotificationBadge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace ian {
namespace {

using menuStyle::Cream;
using menuStyle::Gold;
using menuStyle::Muted;
using menuStyle::Green;
using menuStyle::panel;
using menuStyle::text;
constexpr Color Red{244, 102, 84, 255};
constexpr float RowStep = 68.0F;
constexpr float ListHeight = 472.0F;


constexpr std::array<std::string_view, 9> TabNames{{
    "ALL", "CLASSES", "BUILDINGS", "WEAPONS", "TOMES", "ITEMS", "GENERAL", "CHALLENGES", "COSMETICS",
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

bool button(Rectangle bounds, std::string_view label, const UiMenuLayout& layout,
            const QuestMenuInput& input, bool enabled = true) {
    return menuStyle::button(bounds, layout.scale, label, 15, input.pointer, input.click, enabled);
}

void bar(Rectangle bounds, double fraction, Color color) {
    menuStyle::progress(bounds, fraction, color);
}

void check(Vector2 center, float size, Color color) {
    DrawLineEx({center.x - size * 0.4F, center.y},
        {center.x - size * 0.1F, center.y + size * 0.3F}, size * 0.14F, color);
    DrawLineEx({center.x - size * 0.1F, center.y + size * 0.3F},
        {center.x + size * 0.45F, center.y - size * 0.35F}, size * 0.14F, color);
}

void icon(const GameUi& ui, const QuestDefinition& definition, Rectangle bounds, float scale) {
    Texture2D texture{};
    for (const auto& d : CardUnlockDefinitions) {
        if (d.id != definition.reward.unlockId) continue;
        if (const auto building = runUpgradeBuilding(d.effect)) texture = ui.buildingTexture(*building);
        else if (const auto weapon = runUpgradeWeapon(d.effect)) texture = ui.weaponTexture(*weapon);
        else texture = ui.runUpgradeTexture(d.effect);
    }
    for (const auto& d : ItemUnlockDefinitions)
        if (d.id == definition.reward.unlockId) texture = ui.lootTexture(d.effect);
    for (const auto& condition : definition.conditions) {
        if (texture.id != 0) break;
        if (condition.subject == "gun_turret") texture = ui.buildingTexture(BuildingType::GunTurret);
        else if (condition.subject == "cannon") texture = ui.buildingTexture(BuildingType::Cannon);
        else if (condition.subject == "turret") texture = ui.buildingTexture(BuildingType::Turret);
        else if (condition.subject == "spike_trap") texture = ui.buildingTexture(BuildingType::SpikeTrap);
        else if (condition.subject == "defense_damage") texture = ui.runUpgradeTexture(RunUpgradeEffect::DefenseDamage);
        else if (condition.subject == "defense_fire_rate") texture = ui.runUpgradeTexture(RunUpgradeEffect::DefenseFireRate);
        else if (condition.subject == "unlock_gun_turret") texture = ui.buildingTexture(BuildingType::GunTurret);
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
    panel(bounds, scale, menuStyle::Inset);
    if (texture.id != 0) {
        const float size = std::min(bounds.width - 8 * scale, bounds.height - 8 * scale);
        DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
            {bounds.x + (bounds.width - size) * 0.5F, bounds.y + (bounds.height - size) * 0.5F, size, size}, {}, 0, WHITE);
    } else {
        DrawPoly({bounds.x + bounds.width * 0.5F, bounds.y + bounds.height * 0.5F},
            6, bounds.width * 0.28F, 30.0F, Gold);
    }
}

void silver(const GameUi& ui, const UiMenuLayout& layout, float x, float y, int amount, float size = 28) {
    ui.drawResourceIcon(layout.rect(x, y, size, size), UiResourceIcon::Silver);
    text(std::to_string(amount), layout.rect(x + size + 7, y, 80, size), size * 0.64F * layout.scale,
        Cream, UiFontRole::BodyBold);
}

bool rewardButton(const GameUi& ui, Rectangle bounds, const UiMenuLayout& layout,
                  const QuestMenuInput& input, int amount) {
    const bool hovered = CheckCollisionPointRec(input.pointer, bounds);
    panel(bounds, layout.scale, hovered ? Color{255, 204, 0, 255} : menuStyle::Selected);
    text("COLLECT", {bounds.x + 8 * layout.scale, bounds.y, 100 * layout.scale, bounds.height},
        14 * layout.scale, Cream, UiFontRole::Display, 1);
    ui.drawResourceIcon({bounds.x + 116 * layout.scale, bounds.y + 8 * layout.scale,
        28 * layout.scale, 28 * layout.scale}, UiResourceIcon::Silver);
    text(std::to_string(amount), {bounds.x + 148 * layout.scale, bounds.y, 28 * layout.scale, bounds.height},
        18 * layout.scale, Cream, UiFontRole::BodyBold, 1);
    return hovered && input.click;
}

void startRewardFlight(QuestMenuState& state, Vector2 source, int silverAmount) {
    if (silverAmount <= 0) return;
    const int count = std::clamp(silverAmount, 3, 8);
    for (int index = 0; index < count; ++index) {
        const float angle = static_cast<float>(index) * 2.399963F;
        state.rewardFlights.push_back({
            .start = source,
            .burst = {source.x + std::cos(angle) * (35 + static_cast<float>(index) * 4),
                      source.y - 44 - std::abs(std::sin(angle)) * 34},
            .delay = static_cast<float>(index) * 0.035F,
            .silver = silverAmount / count + (index < silverAmount % count ? 1 : 0),
        });
    }
}

constexpr float RewardFlightDuration = 0.82F;

void updateRewardFlights(QuestMenuState& state, float deltaTime) {
    const float step = std::isfinite(deltaTime) ? std::max(0.0F, deltaTime) : 0.0F;
    state.counterPulse = std::max(0.0F, state.counterPulse - step);
    for (auto& coin : state.rewardFlights) coin.age += step;
    std::erase_if(state.rewardFlights, [&](const auto& coin) {
        if (coin.age < coin.delay + RewardFlightDuration) return false;
        state.counterPulse = 0.18F;
        return true;
    });
}

Vector2 rewardCoinPosition(const QuestRewardFlight& coin) {
    const float age = std::max(0.0F, coin.age - coin.delay);
    if (age < 0.18F) {
        const float phase = age / 0.18F;
        const float ease = 1 - (1 - phase) * (1 - phase);
        return {coin.start.x + (coin.burst.x - coin.start.x) * ease,
                coin.start.y + (coin.burst.y - coin.start.y) * ease};
    }
    const float phase = std::clamp((age - 0.18F) / (RewardFlightDuration - 0.18F), 0.0F, 1.0F);
    const float t = phase * phase;
    const Vector2 target{1489, 141};
    const Vector2 control{coin.burst.x - 65, std::min(coin.burst.y, target.y) - 70};
    const float inverse = 1 - t;
    return {inverse * inverse * coin.burst.x + 2 * inverse * t * control.x + t * t * target.x,
            inverse * inverse * coin.burst.y + 2 * inverse * t * control.y + t * t * target.y};
}

void drawRewardFlights(const GameUi& ui, const UiMenuLayout& layout, const QuestMenuState& state) {
    const auto texture = ui.resourceTexture(UiResourceIcon::Silver);
    if (texture.id == 0) return;
    for (const auto& coin : state.rewardFlights) {
        if (coin.age < coin.delay) continue;
        const Vector2 position = rewardCoinPosition(coin);
        const float phase = std::clamp((coin.age - coin.delay) / RewardFlightDuration, 0.0F, 1.0F);
        const float size = 25 + std::sin(phase * 3.141593F) * 8;
        const auto center = layout.rect(position.x, position.y, 0, 0);
        DrawCircleV({center.x, center.y}, size * 0.64F * layout.scale, {175, 224, 247, 25});
        DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
            {center.x, center.y, size * layout.scale, size * layout.scale},
            {size * layout.scale * 0.5F, size * layout.scale * 0.5F},
            std::sin(phase * 6.283185F) * 24, WHITE);
    }
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
    const auto content = contentUnlockName(id);
    if (!content.empty()) return content;
    for (const auto& d : MetaShopDefinitions) if (d.requiredUnlock == id) return d.title;
    return "PERMANENT UNLOCK";
}

} // namespace

Rectangle questMenuPanelBounds(Vector2 viewport) {
    return UiMenuLayout::fromViewport(viewport).rect(180, 90, 1560, 890);
}

bool drawQuestSummaryCard(
    const GameUi& ui, const QuestStatus& status, Rectangle bounds, float scale,
    Vector2 pointer, bool click) {
    const bool hovered = CheckCollisionPointRec(pointer, bounds);
    panel(bounds, scale, hovered ? menuStyle::Hover : menuStyle::Frame);
    icon(ui, *status.definition,
        {bounds.x + 10 * scale, bounds.y + 14 * scale, 48 * scale, 48 * scale}, scale);
    const float x = bounds.x + 72 * scale;
    const float width = bounds.width - 86 * scale;
    const bool hasUnlock = !status.definition->reward.unlockId.empty();
    const std::string heading = localizeText(hasUnlock
        ? unlockName(status.definition->reward.unlockId) : std::string_view{status.definition->title});
    const ScopedUiFontRole body(UiFontRole::BodyBold);
    std::size_t split = std::string::npos;
    float longest = measureUiText(heading, 17 * scale).x;
    if (longest > width) {
        for (std::size_t at = heading.find(' '); at != std::string::npos; at = heading.find(' ', at + 1)) {
            const float candidate = std::max(measureUiText(heading.substr(0, at), 17 * scale).x,
                measureUiText(heading.substr(at + 1), 17 * scale).x);
            if (candidate < longest) { longest = candidate; split = at; }
        }
    }
    const Color headingColor = status.rewardAvailable ? Gold : Cream;
    if (split == std::string::npos) {
        text(heading, {x, bounds.y + 7 * scale, width, 22 * scale}, 17 * scale, headingColor, UiFontRole::BodyBold);
    } else {
        text(heading.substr(0, split), {x, bounds.y + 5 * scale, width, 20 * scale}, 17 * scale, headingColor, UiFontRole::BodyBold);
        text(heading.substr(split + 1), {x, bounds.y + 24 * scale, width, 20 * scale}, 17 * scale, headingColor, UiFontRole::BodyBold);
    }
    if (hasUnlock) {
        text(status.definition->title, {x, bounds.y + 30 * scale, width, 22 * scale}, 14 * scale, Muted);
    }
    const float captionY = bounds.y + bounds.height - 31 * scale;
    if (status.rewardAvailable) {
        text("COLLECT REWARD", {x, captionY, width * 0.57F, 18 * scale}, 12 * scale, Gold, UiFontRole::BodyBold);
    } else if (!hasUnlock && status.definition->reward.silver > 0) {
        ui.drawResourceIcon({x, captionY, 17 * scale, 17 * scale}, UiResourceIcon::Silver);
        text("+" + std::to_string(status.definition->reward.silver),
            {x + 23 * scale, captionY, width * 0.4F, 18 * scale}, 13 * scale, Muted);
    }
    text(counter(status), {x, captionY, width, 18 * scale}, 13 * scale, Green, UiFontRole::BodyBold, 2);
    bar({x, bounds.y + bounds.height - 10 * scale, width, 5 * scale}, status.fraction, Green);
    return hovered && click;
}

QuestMenuResult drawQuestMenu(
    const GameUi& ui, const QuestSystem& quests, MetaProgression& profile,
    QuestMenuState& state, Vector2 viewport, const QuestMenuInput& input) {
    const auto layout = UiMenuLayout::fromViewport(viewport);
    updateRewardFlights(state, input.deltaTime);
    QuestMenuResult result{.close = input.close};
    if (input.categoryStep != 0) {
        const int index = (static_cast<int>(tabIndex(state)) + input.categoryStep + 9) % 9;
        selectTab(state, static_cast<std::size_t>(index));
        result.interaction = true;
    }
    const auto bounds = questMenuPanelBounds(viewport);
    panel(bounds, layout.scale, menuStyle::Frame);
    panel(layout.rect(194, 104, 1532, 84), layout.scale, menuStyle::Card);
    text("QUESTS", layout.rect(218, 110, 360, 68), 32 * layout.scale, Cream, UiFontRole::Display);
    const auto all = quests.statuses(profile, state.completedLast);
    const auto done = static_cast<std::size_t>(std::ranges::count_if(all, [](const auto& row) { return row.completed; }));
    const auto total = all.size();
    const std::string summary = std::to_string(done) + " / " + std::to_string(total);
    text("COMPLETED", layout.rect(590, 115, 220, 28), 13 * layout.scale, Muted, UiFontRole::BodyBold);
    text(summary, layout.rect(810, 114, 230, 30), 17 * layout.scale, Cream, UiFontRole::BodyBold, 2);
    bar(layout.rect(590, 158, 450, 8), total == 0 ? 0 : static_cast<double>(done) / static_cast<double>(total), Green);
    // Wallet writes happen on claim; only its presentation waits for arrivals.
    // Closing the screen or quitting mid-flight cannot lose a claimed reward.
    std::int64_t inFlightSilver = 0;
    for (const auto& coin : state.rewardFlights) inFlightSilver += coin.silver;
    if (state.counterPulse > 0) {
        const auto center = layout.rect(1489, 141, 0, 0);
        DrawCircleV({center.x, center.y}, (25 + state.counterPulse * 55) * layout.scale,
            ColorAlpha(Color{173, 235, 255, 255}, state.counterPulse * 1.3F));
    }
    const int displayedSilver = static_cast<int>(std::max<std::int64_t>(0, profile.silver - inFlightSilver));
    silver(ui, layout, 1470, 122, displayedSilver, 38);
    if (menuStyle::closeButton(layout.rect(1653, 118, 52, 52), layout.scale, input.pointer, input.click)) result.close = true;

    for (std::size_t i = 0; i < TabNames.size(); ++i) {
        const float x = 204 + static_cast<float>(i) * 168;
        const auto tab = layout.rect(x, 204, 160, 62);
        if (CheckCollisionPointRec(input.pointer, tab) && input.click) {
            selectTab(state, i);
            result.interaction = true;
        }
        const auto category = quests.categoryProgress(profile,
            i == 0 ? std::nullopt : std::optional{static_cast<QuestCategory>(i - 1)});
        const bool selected = tabIndex(state) == i;
        panel(tab, layout.scale, menuStyle::selectionFill(selected, CheckCollisionPointRec(input.pointer, tab)));
        text(TabNames[i], layout.rect(x + 8, 209, category.unclaimedRewards > 0 ? 122 : 144, 27), 13 * layout.scale,
            !selected && category.total == 0 ? Muted : Cream, UiFontRole::Display, 1);
        text(std::to_string(category.completed) + " / " + std::to_string(category.total),
            layout.rect(x + 8, 238, 144, 19), 11 * layout.scale, selected ? Cream : Muted,
            UiFontRole::BodyBold, 1);
        const auto badge = layout.rect(x + 148, 205, 0, 0);
        drawUiNotificationBadge({badge.x, badge.y}, 13 * layout.scale, category.unclaimedRewards);
    }
    const auto sortBounds = layout.rect(204, 278, 300, 24);
    if (CheckCollisionPointRec(input.pointer, sortBounds) && input.click) {
        state.completedLast = !state.completedLast;
        state.scroll = 0;
        result.interaction = true;
    }
    DrawRectangleRec(layout.rect(205, 279, 21, 21), BLACK);
    DrawRectangleRec(layout.rect(208, 282, 15, 15), menuStyle::Inset);
    if (state.completedLast) check({layout.rect(215, 289, 0, 0).x, layout.rect(215, 289, 0, 0).y}, 17 * layout.scale, Green);
    text("COMPLETED LAST", layout.rect(238, 278, 260, 24), 12 * layout.scale, Cream);


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

    beginUiScissor(static_cast<int>(list.x), static_cast<int>(list.y),
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
        panel(rowBounds, layout.scale, menuStyle::selectionFill(active, hovered, !row.prerequisitesMet));
        icon(ui, *row.definition, layout.rect(213, y + 7, 46, 46), layout.scale);
        text(row.definition->title, layout.rect(278, y + 9, 835, 42), 18 * layout.scale,
            active || row.prerequisitesMet ? Cream : Muted, UiFontRole::BodyBold);
        text(counter(row), layout.rect(1140, y + 4, 265, 36), 17 * layout.scale,
            active ? Cream : row.completed ? Green : Cream, UiFontRole::BodyBold, 1);
        bar(layout.rect(1140, y + 44, 265, 6), row.fraction, Green);
        if (row.rewardAvailable) {
            if (rewardButton(ui, layout.rect(1496, y + 8, 184, 44), layout, input, row.definition->reward.silver) && hovered) {
                const auto claim = quests.claimReward(profile, row.definition->id);
                if (claim.claimed) {
                    startRewardFlight(state, {1630, y + 30}, claim.silverGranted);
                    result.rewardClaimed = true;
                    result.interaction = true;
                }
            }
        } else if (row.completed) check({layout.rect(1628, y + 31, 0, 0).x, layout.rect(1628, y + 31, 0, 0).y}, 28 * layout.scale, Green);
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
        DrawRectangleRounded(track, 0.8F, 6, menuStyle::Inset);
        DrawRectangleRounded(layout.rect(1700, 312 + state.scroll / maximumScroll * (ListHeight - thumbHeight),
            10, thumbHeight), 0.8F, 6, {158, 166, 151, 255});
    }

    panel(layout.rect(204, 800, 1508, 154), layout.scale, menuStyle::Inset);
    if (!rows.empty()) {
        const auto& row = rows[selectedIndex];
        const auto& definition = *row.definition;
        icon(ui, definition, layout.rect(222, 819, 96, 96), layout.scale);
        text(definition.title, layout.rect(342, 814, 1060, 37), 20 * layout.scale, Cream, UiFontRole::Display);
        const bool runGoal = std::ranges::any_of(definition.conditions, [](const auto& condition) { return condition.scope == QuestScope::Run; });
        const std::string description = definition.description != definition.title ? definition.description
            : runGoal ? "IN ONE RUN. UNFINISHED PROGRESS RESETS ON A NEW RUN."
                      : "LIFETIME GOAL. PROGRESS IS SAVED BETWEEN RUNS.";
        text(description, layout.rect(342, 858, 1060, 27), 13 * layout.scale, Cream);
        std::string detail = row.rewardAvailable ? localizeText("COMPLETED — COLLECT REWARD")
            : row.completed ? localizeText("COMPLETED — REWARD RECEIVED")
            : localizeText(runGoal ? "ONE RUN" : "ALL RUNS") + "   " + counter(row);
        if (!row.prerequisitesMet) {
            detail = localizeText("REQUIRES") + ": ";
            for (const auto& id : definition.prerequisites) {
                const auto found = std::ranges::find(quests.definitions(), id, &QuestDefinition::id);
                if (found != quests.definitions().end()) detail += localizeText(found->title) + "  ";
            }
        }
        if (!row.completed && row.failedThisRun)
            detail += " — " + localizeText("FAILED THIS RUN");
        text(detail, layout.rect(342, 900, 1060, 28), 13 * layout.scale,
            row.completed ? Green : !row.prerequisitesMet ? Muted : Cream, UiFontRole::BodyBold);
        if (row.unlockAvailable && !row.unlockOwned && definition.reward.purchaseCost > 0) {
            const std::string label = localizeText("BUY") + "  " + std::to_string(definition.reward.purchaseCost);
            text(unlockName(definition.reward.unlockId), layout.rect(1428, 817, 261, 30), 15 * layout.scale, Gold, UiFontRole::BodyBold, 1);
            text("AVAILABLE TO BUY", layout.rect(1428, 851, 261, 18), 10 * layout.scale, Green, UiFontRole::BodyBold, 1);
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
                : row.rewardAvailable ? std::string_view{"COLLECT REWARD"}
                : row.completed ? std::string_view{"REWARD RECEIVED"} : std::string_view{"REWARD"};
            text(label, layout.rect(1428, 822, 261, 30),
                12 * layout.scale, row.completed ? Green : Muted, UiFontRole::BodyBold, 1);
            silver(ui, layout, 1505, 871, definition.reward.silver, 38);
        }
    }
    drawRewardFlights(ui, layout, state);
    if (result.close) {
        state.rewardFlights.clear();
        state.counterPulse = 0;
    }
    return result;
}

} // namespace ian
