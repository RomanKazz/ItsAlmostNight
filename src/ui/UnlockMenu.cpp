#include "ui/UiMenuLayout.hpp"
#include "ui/UnlockMenu.hpp"

#include "buildings/BuildingSystem.hpp"
#include "buildings/ModularBuildingConstants.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "game/LootChestSystem.hpp"
#include "progression/MetaProgression.hpp"
#include "progression/RunUpgrade.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiText.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ian {
namespace {

constexpr Color Cream{250, 241, 211, 255};
constexpr Color Gold{255, 205, 105, 255};
constexpr Color Muted{159, 169, 167, 255};
constexpr Color Green{131, 215, 138, 255};
constexpr Color Red{244, 102, 84, 255};
constexpr int Columns = 9;
constexpr float CellStep = 151;
constexpr float GridHeight = 466;
constexpr std::array<std::string_view, 6> TabNames{
    "CLASSES", "BUILDINGS", "WEAPONS", "UPGRADES", "ITEMS", "META"};

void panel(Rectangle bounds, float scale, Color fill, Color border = {116, 79, 48, 255}) {
    DrawRectangleRounded(bounds, 0.06F, 8, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.06F, 8, 2 * scale, border);
}
void text(std::string_view value, Rectangle bounds, float size, Color color,
          UiFontRole role = UiFontRole::Body, int align = 0) {
    const ScopedUiFontRole font(role);
    const float fitted = fitUiTextSize(value, size, size * 0.60F, bounds.width, bounds.height);
    const Vector2 measured = measureUiText(value, fitted);
    const float x = align == 1 ? bounds.x + (bounds.width - measured.x) * 0.5F
        : align == 2 ? bounds.x + bounds.width - measured.x : bounds.x;
    drawUiText(value, {x, bounds.y + (bounds.height - measured.y) * 0.5F}, fitted, color);
}
bool button(Rectangle bounds, std::string_view label, const UiMenuLayout& l,
            const UnlockMenuInput& input, bool enabled = true) {
    const bool hovered = enabled && CheckCollisionPointRec(input.pointer, bounds);
    panel(bounds, l.scale, !enabled ? Color{46, 50, 51, 255}
        : hovered ? Color{212, 128, 51, 255} : Color{159, 88, 38, 255},
        enabled ? Gold : Color{88, 90, 84, 255});
    text(label, {bounds.x + 8 * l.scale, bounds.y, bounds.width - 16 * l.scale, bounds.height},
        17 * l.scale, enabled ? Cream : Muted, UiFontRole::BodyBold, 1);
    return hovered && input.click;
}

struct Entry {
    std::string id;
    UnlockCategory category;
    int icon;
    std::string title;
    std::string description;
    std::array<std::string_view, 3> traits{};
    std::string unlockId;
};
const std::vector<Entry>& catalog() {
    static const auto entries = [] {
        std::vector<Entry> result;
        constexpr std::array<std::string_view, 8> classKeys{
            "", "", "", "", "class.berserker", "class.vampire", "class.alchemist", "class.chronomancer"};
        for (std::size_t i = 0; i < PlayerClassDefinitions.size(); ++i) {
            const auto& c = PlayerClassDefinitions[i];
            result.push_back({"class:" + std::to_string(i), UnlockCategory::Classes,
                static_cast<int>(i), std::string(c.name), std::string(c.role), c.traits, std::string(classKeys[i])});
        }
        constexpr std::array buildings{BuildingType::Core, BuildingType::Wall, BuildingType::Gate,
            BuildingType::Turret, BuildingType::GunTurret, BuildingType::Cannon, BuildingType::Catapult,
            BuildingType::SpikeTrap, BuildingType::SlowTrap, BuildingType::CrystalMine,
            BuildingType::LumberMill, BuildingType::Quarry, BuildingType::RepairStation, BuildingType::DecoyBeacon, BuildingType::WindBlower};
        constexpr std::array<std::string_view, 15> descriptions{
            "The heart of your fort. Upgrade it to expand your defenses.",
            "Blocks enemies and protects your defenses.", "An entrance through your fortifications.",
            "Fires bolts at approaching enemies.", "Rapid bullets against the horde.",
            "Explosive shells damage groups of enemies.", "Lobs heavy projectiles at distant enemies.",
            "Damages enemies crossing the trap.", "Slows enemies crossing the trap.",
            "Produces crystals over time.", "Produces wood over time.", "Produces stone over time.",
            "Repairs nearby walls and weapons every 2s, even under attack. Does not repair the Core.", "Light pulses lure nearby enemies. Bosses are immune.", "Gusts push back crowds. Heavy enemies resist; bosses are immune."};
        for (std::size_t i = 0; i < buildings.size(); ++i) result.push_back({
            "building:" + std::to_string(static_cast<int>(buildings[i])), UnlockCategory::Buildings,
            static_cast<int>(buildings[i]), std::string(buildingDisplayName(buildings[i])), std::string(descriptions[i]), {}, {}});
        constexpr std::array<std::string_view, 2> modularNames{"PLATFORM", "RAMP"};
        constexpr std::array<std::string_view, 2> modularDescriptions{
            "Builds foundations on the ground and floors above platforms.",
            "Connects different heights in your fort."};
        for (std::size_t i = 0; i < ModularBuildHotbarOrder.size(); ++i) {
            const int index = static_cast<int>(ModularBuildHotbarOrder[i]);
            result.push_back({"modular:" + std::to_string(index), UnlockCategory::Buildings, -index - 1,
                std::string(modularNames[i]), std::string(modularDescriptions[i]), {}, {}});
        }
        constexpr std::array<std::string_view, 7> weapons{"AXE", "PICKAXE", "CLUB", "RIFLE", "BOMB", "ICE WAND", "FIRE WAND"};
        constexpr std::array<std::string_view, 7> weaponDescriptions{
            "Harvest trees and strike enemies in melee.", "Mine stone and crystals; strike enemies in melee.",
            "A heavy melee strike with knockback.", "Shoot enemies at range.",
            "Throw an explosive into a group of enemies.", "Freeze enemies with icy projectiles.",
            "Ignite enemies with fiery projectiles."};
        for (std::size_t i = 0; i < PlayerWeaponHotbarOrder.size(); ++i) result.push_back({
            "weapon:" + std::to_string(static_cast<int>(PlayerWeaponHotbarOrder[i])), UnlockCategory::Weapons,
            static_cast<int>(PlayerWeaponHotbarOrder[i]), std::string(weapons[i]), std::string(weaponDescriptions[i]), {}, {}});
        for (const auto& upgrade : RunUpgradeDefinitions) {
            if (retiredRunUpgrade(upgrade.effect)) continue;
            const int index = static_cast<int>(upgrade.effect);
            result.push_back({"upgrade:" + std::to_string(index), UnlockCategory::Upgrades,
                index, std::string(upgrade.name), std::string(upgrade.description), {}, {}});
        }
        for (std::size_t i = 0; i < LootUpgradeEffectCount; ++i) {
            const auto effect = static_cast<LootUpgradeEffect>(i);
            result.push_back({"item:" + std::to_string(i), UnlockCategory::Items, static_cast<int>(i),
                lootUpgradeName(effect), lootUpgradeDescription(effect), {}, {}});
        }
        result.push_back({"meta.refresh", UnlockCategory::Meta, 0, "CARD REFRESH", "Start each run with one free card reroll.", {}, "meta.refresh"});
        result.push_back({"meta.skip", UnlockCategory::Meta, 1, "CARD SKIP", "Complete 20 quests", {}, "meta.skip"});
        result.push_back({"meta.banish", UnlockCategory::Meta, 2, "CARD BANISH", "Complete 30 quests", {}, "meta.banish"});
        return result;
    }();
    return entries;
}

struct Availability {
    bool owned{true};
    bool eligible{};
    int price{};
    const QuestStatus* quest{};
};
Availability availability(const Entry& entry, const MetaProgression& profile,
                          const std::vector<QuestStatus>& statuses) {
    if (entry.unlockId.empty()) return {};
    for (const auto& status : statuses) {
        if (status.definition->reward.unlockId != entry.unlockId) continue;
        return {status.unlockOwned, status.unlockAvailable, status.definition->reward.purchaseCost, &status};
    }
    return {profile.quests.ownedUnlocks.contains(entry.unlockId), false, 0, nullptr};
}

Texture2D textureFor(const GameUi& ui, const Entry& entry) {
    switch (entry.category) {
    case UnlockCategory::Classes: {
        constexpr std::array symbols{CoreMenuTexture::Shield, CoreMenuTexture::Turret,
            CoreMenuTexture::Wrench, CoreMenuTexture::Rock, CoreMenuTexture::Spikes,
            CoreMenuTexture::Heart, CoreMenuTexture::Gem, CoreMenuTexture::Gear};
        if (entry.icon == 1) return ui.weaponTexture(PlayerWeapon::Rifle);
        if (entry.icon == 3) return ui.weaponTexture(PlayerWeapon::Pickaxe);
        if (entry.icon == 4) return ui.weaponTexture(PlayerWeapon::Club);
        if (entry.icon == 6) return ui.weaponTexture(PlayerWeapon::FireWand);
        return ui.coreMenuTexture(symbols[static_cast<std::size_t>(entry.icon)]);
    }
    case UnlockCategory::Buildings: return entry.icon < 0
        ? ui.modularBuildingTexture(static_cast<ModularBuildPiece>(-entry.icon - 1))
        : ui.buildingTexture(static_cast<BuildingType>(entry.icon));
    case UnlockCategory::Weapons: return ui.weaponTexture(static_cast<PlayerWeapon>(entry.icon));
    case UnlockCategory::Upgrades: return ui.runUpgradeTexture(static_cast<RunUpgradeEffect>(entry.icon));
    case UnlockCategory::Meta: return ui.coreMenuTexture(CoreMenuTexture::Upgrade);
    case UnlockCategory::Items:
        switch (static_cast<LootUpgradeEffect>(entry.icon)) {
        case LootUpgradeEffect::Damage: return ui.runUpgradeTexture(RunUpgradeEffect::Overkill);
        case LootUpgradeEffect::MoveSpeed: return ui.runUpgradeTexture(RunUpgradeEffect::MoveSpeed);
        case LootUpgradeEffect::MaximumHealth: return ui.runUpgradeTexture(RunUpgradeEffect::MaximumHealth);
        case LootUpgradeEffect::Apple: return ui.coreMenuTexture(CoreMenuTexture::Heart);
        case LootUpgradeEffect::IronBar: return ui.runUpgradeTexture(RunUpgradeEffect::RecoverableArmor);
        case LootUpgradeEffect::Magnet: return ui.runUpgradeTexture(RunUpgradeEffect::Salvager);
        case LootUpgradeEffect::HealthAid: return ui.coreMenuTexture(CoreMenuTexture::Heart);
        default: return {};
        }
    }
    return {};
}

// Small vector item emblems stay crisp at every UI scale and need no model rendering.
void itemEmblem(Rectangle bounds, LootUpgradeEffect effect, Color tint) {
    const float s = std::min(bounds.width, bounds.height) / 100;
    const Vector2 p{bounds.x + bounds.width * 0.5F, bounds.y + bounds.height * 0.5F};
    const auto r = [&](float x, float y, float w, float h, Color color) {
        DrawRectangleRounded({p.x + x * s, p.y + y * s, w * s, h * s}, 0.14F, 6, color);
    };
    const auto line = [&](float x, float y, float xx, float yy, Color color, float width = 5) {
        DrawLineEx({p.x + x * s, p.y + y * s}, {p.x + xx * s, p.y + yy * s}, width * s, color);
    };
    const auto circle = [&](float x, float y, float radius, Color color) {
        DrawCircleV({p.x + x * s, p.y + y * s}, radius * s, color);
    };
    const Color dark{21, 28, 31, tint.a};
    const Color blue{89, 190, 222, tint.a};
    const Color pale{235, 220, 178, tint.a};
    switch (effect) {
    case LootUpgradeEffect::Key:
        circle(-22, 0, 22, dark); circle(-22, 0, 17, Gold); circle(-22, 0, 8, dark);
        r(-6, -7, 48, 14, Gold); r(19, 2, 10, 20, Gold); r(32, 2, 10, 14, Gold); break;
    case LootUpgradeEffect::Potion:
    case LootUpgradeEffect::FuelJerrycan:
        r(-27, -22, 54, 61, dark); r(-22, -17, 44, 51, effect == LootUpgradeEffect::Potion ? Color{207, 88, 154, 255} : Gold);
        r(-11, -37, 22, 21, dark); r(-7, -35, 14, 18, pale); r(-15, -6, 7, 20, Cream); break;
    case LootUpgradeEffect::Hourglass:
        r(-27, -38, 54, 8, Gold); r(-27, 30, 54, 8, Gold);
        line(-21, -27, 21, 27, pale, 6); line(21, -27, -21, 27, pale, 6);
        DrawTriangle({p.x - 18 * s, p.y + 26 * s}, {p.x + 18 * s, p.y + 26 * s}, {p.x, p.y + 5 * s}, Gold); break;
    case LootUpgradeEffect::Compass:
        circle(0, 0, 37, dark); circle(0, 0, 32, pale);
        DrawTriangle({p.x, p.y - 28 * s}, {p.x - 9 * s, p.y + 7 * s}, {p.x + 9 * s, p.y + 7 * s}, Red);
        DrawTriangle({p.x, p.y + 28 * s}, {p.x + 9 * s, p.y - 7 * s}, {p.x - 9 * s, p.y - 7 * s}, blue); break;
    case LootUpgradeEffect::Rope:
        for (int i = 0; i < 3; ++i) {
            const float radius = static_cast<float>(17 + i * 8);
            DrawRing(p, radius * s, (radius + 5) * s, 0, 360, 28, Gold);
        }
        line(23, 28, 39, 38, Gold); break;
    case LootUpgradeEffect::Bread:
        r(-39, -23, 78, 46, dark); r(-35, -19, 70, 38, {200, 131, 61, 255});
        for (int i = 0; i < 3; ++i) line(static_cast<float>(-21 + i * 18), -11, static_cast<float>(-13 + i * 18), 9, pale, 4);
        break;
    case LootUpgradeEffect::Nail:
        line(-23, 28, 21, -24, dark, 12); line(-23, 28, 21, -24, Muted, 7); line(6, -31, 34, -12, Cream, 9); break;
    case LootUpgradeEffect::Saw:
        r(-38, -7, 70, 31, dark); r(-33, -3, 60, 22, Muted); r(17, -25, 27, 34, Gold);
        for (int i = 0; i < 7; ++i) line(static_cast<float>(-30 + i * 8), 21, static_cast<float>(-26 + i * 8), 28, Cream, 3);
        break;
    case LootUpgradeEffect::Anvil:
        r(-22, 19, 52, 13, Muted); r(-11, -7, 24, 29, Muted); r(-34, -27, 70, 23, Muted);
        DrawTriangle({p.x - 34 * s, p.y - 27 * s}, {p.x - 34 * s, p.y - 4 * s}, {p.x - 47 * s, p.y - 23 * s}, Muted); break;
    default:
        r(-30, -37, 60, 74, dark); r(-26, -33, 52, 66, effect == LootUpgradeEffect::Blueprint ? blue : pale);
        if (effect == LootUpgradeEffect::Blueprint) {
            for (int i = 0; i < 3; ++i) line(-17, static_cast<float>(-17 + i * 16), 17, static_cast<float>(-17 + i * 16), Cream, 2);
            line(-7, -25, -7, 25, Cream, 2);
        } else {
            line(-17, -13, 13, 5, {127, 92, 55, 255}, 3); line(13, 5, -13, 20, {127, 92, 55, 255}, 3);
            line(13, 16, 21, 24, Red, 4); line(21, 16, 13, 24, Red, 4);
        }
        break;
    }
}
void icon(const GameUi& ui, const Entry& entry, Rectangle bounds, bool silhouette) {
    const Texture2D texture = textureFor(ui, entry);
    if (texture.id != 0) {
        const float fit = std::min(bounds.width / static_cast<float>(texture.width), bounds.height / static_cast<float>(texture.height));
        const float w = static_cast<float>(texture.width) * fit, h = static_cast<float>(texture.height) * fit;
        DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
            {bounds.x + (bounds.width - w) * 0.5F, bounds.y + (bounds.height - h) * 0.5F, w, h}, {}, 0,
            silhouette ? Color{9, 14, 17, 255} : WHITE);
    } else if (entry.category == UnlockCategory::Items) {
        itemEmblem(bounds, static_cast<LootUpgradeEffect>(entry.icon), WHITE);
    } else {
        const float radius = std::min(bounds.width, bounds.height) * 0.40F;
        const Vector2 center{bounds.x + bounds.width * 0.5F, bounds.y + bounds.height * 0.5F};
        DrawCircleV(center, radius, silhouette ? Color{9, 14, 17, 255} : Color{193, 136, 67, 255});
        if (!silhouette) text(entry.title, bounds, bounds.height * 0.16F, Cream, UiFontRole::BodyBold, 1);
    }
}
void silver(const GameUi& ui, const UiMenuLayout& l, float x, float y, int amount, bool enough = true) {
    ui.drawResourceIcon(l.rect(x, y, 28, 28), UiResourceIcon::Silver);
    text(std::to_string(amount), l.rect(x + 34, y, 90, 28), 20 * l.scale,
        enough ? Cream : Red, UiFontRole::BodyBold);
}
void selectCategory(UnlockMenuState& state, std::size_t category) {
    state.category = static_cast<UnlockCategory>(category);
    state.selectedId.clear(); state.scroll = 0; state.draggingScroll = false;
}

} // namespace

Rectangle unlockMenuPanelBounds(Vector2 viewport) { return UiMenuLayout::fromViewport(viewport).rect(230, 110, 1460, 870); }

UnlockMenuResult drawUnlockMenu(const GameUi& ui, const QuestSystem& quests, MetaProgression& profile,
                               UnlockMenuState& state, Vector2 viewport, const UnlockMenuInput& input) {
    const UiMenuLayout l = UiMenuLayout::fromViewport(viewport);
    UnlockMenuResult result{.close = input.close, .questId = {}};
    if (static_cast<std::size_t>(state.category) >= TabNames.size()) selectCategory(state, 0);
    if (input.categoryStep != 0) {
        const int next = (static_cast<int>(state.category) + (input.categoryStep > 0 ? 1 : -1) + 6) % 6;
        selectCategory(state, static_cast<std::size_t>(next)); result.interaction = true;
    }
    if (input.click) {
        for (std::size_t tab = 0; tab < TabNames.size(); ++tab) {
            if (CheckCollisionPointRec(input.pointer, l.rect(266 + static_cast<float>(tab) * 233, 220, 216, 66)) &&
                tab != static_cast<std::size_t>(state.category)) {
                selectCategory(state, tab); result.interaction = true;
                break;
            }
        }
    }
    const auto statuses = quests.statuses(profile, false);
    std::vector<const Entry*> visible;
    for (const auto& e : catalog()) if (e.category == state.category) visible.push_back(&e);
    auto selected = std::find_if(visible.begin(), visible.end(), [&](const Entry* e) { return e->id == state.selectedId; });
    if (selected == visible.end()) {
        state.selectedId = visible.empty() ? "" : visible.front()->id;
        selected = visible.begin();
    }
    const float maxScroll = std::max(0.0F, static_cast<float>((visible.size() + Columns - 1) / Columns) * CellStep - GridHeight);
    state.scroll = std::isfinite(state.scroll) ? std::clamp(state.scroll, 0.0F, maxScroll) : 0;
    if (!visible.empty() && input.selectionStep != 0) {
        const auto index = static_cast<int>(std::distance(visible.begin(), selected));
        const int next = std::clamp(index + input.selectionStep, 0, static_cast<int>(visible.size()) - 1);
        state.selectedId = visible[static_cast<std::size_t>(next)]->id;
        const float top = static_cast<float>(next / Columns) * CellStep;
        state.scroll = std::clamp(top < state.scroll ? top : top + CellStep > state.scroll + GridHeight
            ? top + CellStep - GridHeight : state.scroll, 0.0F, maxScroll);
        result.interaction = true;
    }
    panel(unlockMenuPanelBounds(viewport), l.scale, {26, 33, 36, 250});
    panel(l.rect(248, 128, 1424, 73), l.scale, {35, 47, 48, 255});
    text("UNLOCKS", l.rect(270, 138, 650, 50), 32 * l.scale, Cream, UiFontRole::Display);
    silver(ui, l, 1465, 150, profile.silver);
    if (button(l.rect(1616, 142, 40, 40), "X", l, input)) result.close = true;

    for (std::size_t tab = 0; tab < TabNames.size(); ++tab) {
        const float x = 266 + static_cast<float>(tab) * 233;
        const Rectangle bounds = l.rect(x, 220, 216, 66);
        const bool active = static_cast<std::size_t>(state.category) == tab;
        const bool hovered = CheckCollisionPointRec(input.pointer, bounds);
        panel(bounds, l.scale, active ? Color{90, 66, 40, 255} : hovered ? Color{48, 58, 61, 255} : Color{33, 42, 45, 255},
            active ? Gold : Color{63, 74, 75, 255});
        std::size_t total = 0, owned = 0, ready = 0;
        for (const auto& e : catalog()) {
            if (static_cast<std::size_t>(e.category) != tab) continue;
            ++total; const auto a = availability(e, profile, statuses);
            if (a.owned) ++owned;
            else if (a.eligible) ++ready;
        }
        text(TabNames[tab], l.rect(x + 8, 226, 200, 27), 18 * l.scale, active ? Gold : Cream, UiFontRole::BodyBold, 1);
        text(std::to_string(owned) + " / " + std::to_string(total), l.rect(x + 8, 255, 200, 20), 13 * l.scale, Muted, UiFontRole::Body, 1);
        if (ready > 0) DrawCircleV({bounds.x + bounds.width - 11 * l.scale, bounds.y + 11 * l.scale}, 5 * l.scale, Gold);
    }
    const Rectangle grid = l.rect(270, 309, 1348, GridHeight);
    panel(l.rect(248, 300, 1424, 487), l.scale, {20, 28, 31, 255}, {56, 66, 67, 255});
    if (CheckCollisionPointRec(input.pointer, grid) && std::isfinite(input.wheel)) state.scroll = std::clamp(state.scroll - input.wheel * 65, 0.0F, maxScroll);
    const Rectangle track = l.rect(1636, 316, 9, GridHeight - 14);
    if (maxScroll > 0) {
        const float thumbHeight = track.height * GridHeight / (GridHeight + maxScroll);
        if (input.click && CheckCollisionPointRec(input.pointer, l.rect(1625, 309, 33, GridHeight))) state.draggingScroll = true;
        if (!input.pointerDown) state.draggingScroll = false;
        if (state.draggingScroll) state.scroll = std::clamp((input.pointer.y - track.y - thumbHeight * 0.5F) / (track.height - thumbHeight), 0.0F, 1.0F) * maxScroll;
        DrawRectangleRounded(track, 1, 6, {46, 57, 60, 255});
        DrawRectangleRounded({track.x, track.y + (track.height - thumbHeight) * state.scroll / maxScroll, track.width, thumbHeight}, 1, 6, Muted);
    }
    BeginScissorMode(static_cast<int>(grid.x), static_cast<int>(grid.y), static_cast<int>(grid.width), static_cast<int>(grid.height));
    for (std::size_t i = 0; i < visible.size(); ++i) {
        const Entry& e = *visible[i]; const auto a = availability(e, profile, statuses);
        const float x = 272 + static_cast<float>(i % Columns) * CellStep;
        const float y = 314 + static_cast<float>(i / Columns) * CellStep - state.scroll;
        const Rectangle cell = l.rect(x, y, 137, 137);
        if (cell.y + cell.height < grid.y || cell.y > grid.y + grid.height) continue;
        const bool hovered = CheckCollisionPointRec(input.pointer, cell) && CheckCollisionPointRec(input.pointer, grid);
        if (hovered && input.click) { state.selectedId = e.id; result.interaction = true; }
        const bool active = state.selectedId == e.id;
        panel(cell, l.scale, a.owned ? Color{49, 64, 67, 255} : a.eligible ? Color{62, 59, 42, 255} : Color{34, 42, 46, 255},
            active ? Gold : hovered ? Cream : a.owned ? Color{82, 122, 119, 255} : Color{63, 74, 78, 255});
        icon(ui, e, l.rect(x + 18, y + 9, 101, 91), !a.owned && !a.eligible);
        if (a.eligible && !a.owned) {
            panel(l.rect(x + 20, y + 42, 97, 36), l.scale, {18, 26, 30, 239}, {117, 107, 68, 255});
            silver(ui, l, x + 28, y + 46, a.price, profile.silver >= a.price);
        }
        text(e.title, l.rect(x + 5, y + 106, 127, 23), 12 * l.scale,
            a.owned ? Cream : a.eligible ? Gold : Muted, UiFontRole::BodyBold, 1);
        if (!a.owned && !a.eligible) text("?", l.rect(x + 108, y + 6, 20, 22), 18 * l.scale, Muted, UiFontRole::BodyBold, 1);
        if (a.eligible && !a.owned) text("!", l.rect(x + 108, y + 6, 20, 22), 18 * l.scale, Gold, UiFontRole::BodyBold, 1);
    }
    EndScissorMode();

    panel(l.rect(248, 802, 1424, 159), l.scale, {21, 28, 30, 255}, {77, 76, 60, 255});
    selected = std::find_if(visible.begin(), visible.end(), [&](const Entry* e) { return e->id == state.selectedId; });
    if (selected != visible.end()) {
        const Entry& e = **selected; const auto a = availability(e, profile, statuses);
        panel(l.rect(268, 820, 119, 119), l.scale, {41, 52, 54, 255}, a.owned ? Color{82, 122, 119, 255} : Muted);
        icon(ui, e, l.rect(283, 834, 90, 90), !a.owned && !a.eligible);
        text(e.title, l.rect(410, 814, 970, 32), 22 * l.scale, Gold, UiFontRole::BodyBold);
        text(e.description, l.rect(410, 850, 970, 30), 17 * l.scale, Cream);
        if (e.category == UnlockCategory::Classes && a.owned) {
            for (std::size_t i = 0; i < e.traits.size(); ++i) text(e.traits[i],
                l.rect(410, 885 + static_cast<float>(i) * 19, 970, 20), 13 * l.scale, Muted);
        } else if (a.quest && !a.owned) {
            text(a.eligible ? "READY TO UNLOCK" : a.quest->definition->description, l.rect(410, 886, 865, 25), 15 * l.scale, a.eligible ? Green : Muted);
            text(std::to_string(static_cast<int>(std::clamp(a.quest->fraction, 0.0, 1.0) * 100)) + "%",
                l.rect(1285, 886, 95, 25), 15 * l.scale, Green, UiFontRole::BodyBold, 2);
            const Rectangle progress = l.rect(410, 925, 970, 7);
            DrawRectangleRounded(progress, 1, 6, {44, 55, 58, 255});
            if (a.quest->fraction > 0) DrawRectangleRounded({progress.x, progress.y, progress.width * static_cast<float>(std::clamp(a.quest->fraction, 0.0, 1.0)), progress.height}, 1, 6, Green);
        } else {
            const auto source = e.category == UnlockCategory::Buildings ? "Unlocked in runs through Core upgrades and blueprints."
                : e.category == UnlockCategory::Weapons ? "Available through run progression and class loadouts."
                : e.category == UnlockCategory::Upgrades ? "Available when choosing a level-up upgrade."
                : e.category == UnlockCategory::Items ? "Available from chests and crate drops." : e.unlockId == "meta.refresh" ? "Applies from your next run." : "Saved unlock; its run effects will be added later.";
            text(source, l.rect(410, 887, 970, 36), 15 * l.scale, Muted);
        }
        if (e.category == UnlockCategory::Meta && e.unlockId != "meta.refresh") text("Run effects coming later", l.rect(410, 933, 970, 20), 12 * l.scale, Muted);
        if (a.owned) {
            text("UNLOCKED", l.rect(1415, 841, 230, 40), 20 * l.scale, Green, UiFontRole::BodyBold, 1);
        } else if (a.eligible && a.price > 0) {
            silver(ui, l, 1474, 815, a.price, profile.silver >= a.price);
            if (button(l.rect(1415, 858, 230, 44), "UNLOCK", l, input, profile.silver >= a.price)) {
                result.purchase = quests.purchaseUnlock(profile, e.unlockId) == QuestPurchaseResult::Purchased;
                result.interaction = true;
            }
            if (!result.purchase && profile.silver < a.price) text("NOT ENOUGH SILVER", l.rect(1415, 911, 230, 24), 13 * l.scale, Red, UiFontRole::BodyBold, 1);
        } else if (a.quest) {
            text("LOCKED", l.rect(1415, 818, 230, 30), 18 * l.scale, Muted, UiFontRole::BodyBold, 1);
            if (button(l.rect(1415, 861, 230, 44), "VIEW QUEST", l, input)) {
                result.questId = a.quest->definition->id; result.interaction = true;
            }
        }
    }
    return result;
}

} // namespace ian
