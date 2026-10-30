#include "ui/UiMenuLayout.hpp"
#include "ui/UiViewport.hpp"
#include "ui/UnlockMenu.hpp"

#include "buildings/BuildingSystem.hpp"
#include "buildings/ModularBuildingConstants.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "game/LootChestSystem.hpp"
#include "progression/MetaProgression.hpp"
#include "progression/ContentUnlocks.hpp"
#include "progression/RunUpgrade.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiItemDescriptions.hpp"
#include "ui/UiItemTooltip.hpp"
#include "ui/UiText.hpp"
#include "ui/UiMenuStyle.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
constexpr int Columns = 9;
constexpr float CellStep = 151;
constexpr float GridHeight = 466;
constexpr std::array<std::string_view, 6> TabNames{
    "CLASSES", "BUILDINGS", "WEAPONS", "TOMES", "ITEMS", "META"};

bool button(Rectangle bounds, std::string_view label, const UiMenuLayout& l,
            const UnlockMenuInput& input, bool enabled = true) {
    return menuStyle::button(bounds, l.scale, label, 17, input.pointer, input.click, enabled);
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
        for (std::size_t i = 0; i < PlayerClassDefinitions.size(); ++i) {
            const auto& c = PlayerClassDefinitions[i];
            result.push_back({"class:" + std::to_string(i), UnlockCategory::Classes,
                static_cast<int>(i), std::string(c.name), std::string(c.role), c.traits, {}});
        }
        constexpr std::array buildings{BuildingType::Core, BuildingType::Wall, BuildingType::Gate,
            BuildingType::Turret, BuildingType::GunTurret, BuildingType::Cannon, BuildingType::Catapult,
            BuildingType::SpikeTrap, BuildingType::SlowTrap, BuildingType::CrystalMine,
            BuildingType::LumberMill, BuildingType::Quarry, BuildingType::RepairStation, BuildingType::DecoyBeacon, BuildingType::WindBlower,
            BuildingType::Flamethrower, BuildingType::MissileLauncher, BuildingType::TeslaTower, BuildingType::LaserTower, BuildingType::Mortar};
        for (std::size_t i = 0; i < buildings.size(); ++i) result.push_back({
            "building:" + std::to_string(static_cast<int>(buildings[i])), UnlockCategory::Buildings,
            static_cast<int>(buildings[i]), std::string(buildingDisplayName(buildings[i])), std::string(buildingDescription(buildings[i])), {},
            buildingRunUpgrade(buildings[i]) ? std::string(cardUnlockId(*buildingRunUpgrade(buildings[i]))) : std::string{}});
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
        for (std::size_t i = 0; i < PlayerWeaponHotbarOrder.size(); ++i) result.push_back({
            "weapon:" + std::to_string(static_cast<int>(PlayerWeaponHotbarOrder[i])), UnlockCategory::Weapons,
            static_cast<int>(PlayerWeaponHotbarOrder[i]), std::string(weapons[i]), std::string(weaponDescription(PlayerWeaponHotbarOrder[i])), {},
            weaponRunUpgrade(PlayerWeaponHotbarOrder[i]) ? std::string(cardUnlockId(*weaponRunUpgrade(PlayerWeaponHotbarOrder[i]))) : std::string{}});
        for (const auto& upgrade : RunUpgradeDefinitions) {
            if (retiredRunUpgrade(upgrade.effect) || !runTomeDefinition(upgrade.effect)) continue;
            const int index = static_cast<int>(upgrade.effect);
            result.push_back({"upgrade:" + std::to_string(index), UnlockCategory::Upgrades,
                index, std::string(upgrade.name), std::string(upgrade.description), {}, std::string(cardUnlockId(upgrade.effect))});
        }
        for (const auto effect : ActiveLootEffects) {
            const auto i = lootUpgradeIndex(effect);
            result.push_back({"item:" + std::to_string(i), UnlockCategory::Items, static_cast<int>(i),
                lootUpgradeName(effect), lootUpgradeDescription(effect), {}, std::string(itemUnlockId(effect))});
        }
        result.push_back({"meta.refresh", UnlockCategory::Meta, 0, "CARD REFRESH", "Unlocks reroll purchases in the Shop.", {}, "meta.refresh"});
        for (std::size_t i = 0; i < MetaShopDefinitions.size(); ++i) {
            const auto& d = MetaShopDefinitions[i];
            if (!activeMetaShopUpgrade(d.upgrade) || d.requiredUnlock == "meta.refresh") continue;
            result.push_back({std::string(d.requiredUnlock), UnlockCategory::Meta,
                static_cast<int>(i), std::string(d.title), std::string(d.description), {}, std::string(d.requiredUnlock)});
        }
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
    for (const auto& d : CardUnlockDefinitions) if (d.id == entry.unlockId && d.starter) return {};
    for (const auto& d : ItemUnlockDefinitions) if (d.id == entry.unlockId && d.starter) return {};
    for (const auto& status : statuses) {
        if (status.definition->reward.unlockId != entry.unlockId) continue;
        return {status.unlockOwned, status.unlockAvailable, status.definition->reward.purchaseCost, &status};
    }
    return {profile.quests.ownedUnlocks.contains(entry.unlockId), false, 0, nullptr};
}

Texture2D textureFor(const GameUi& ui, const Entry& entry) {
    switch (entry.category) {
    case UnlockCategory::Classes: {
        switch (PlayerClassDefinitions[static_cast<std::size_t>(entry.icon)].type) {
        case PlayerClass::Lumberjack: return ui.weaponTexture(PlayerWeapon::Axe);
        case PlayerClass::Sharpshooter: return ui.weaponTexture(PlayerWeapon::Rifle);
        case PlayerClass::Mechanic: return ui.coreMenuTexture(CoreMenuTexture::Wrench);
        case PlayerClass::Miner: return ui.weaponTexture(PlayerWeapon::Pickaxe);
        default: return {};
        }
    }
    case UnlockCategory::Buildings: return entry.icon < 0
        ? ui.modularBuildingTexture(static_cast<ModularBuildPiece>(-entry.icon - 1))
        : ui.buildingTexture(static_cast<BuildingType>(entry.icon));
    case UnlockCategory::Weapons: return ui.weaponTexture(static_cast<PlayerWeapon>(entry.icon));
    case UnlockCategory::Upgrades: return ui.runUpgradeTexture(static_cast<RunUpgradeEffect>(entry.icon));
    case UnlockCategory::Meta: return ui.coreMenuTexture(CoreMenuTexture::Upgrade);
    case UnlockCategory::Items: return ui.lootTexture(static_cast<LootUpgradeEffect>(entry.icon));
    }
    return {};
}

void icon(const GameUi& ui, const Entry& entry, Rectangle bounds, bool silhouette) {
    const Texture2D texture = textureFor(ui, entry);
    if (texture.id != 0) {
        const float fit = std::min(bounds.width / static_cast<float>(texture.width), bounds.height / static_cast<float>(texture.height));
        const float w = static_cast<float>(texture.width) * fit, h = static_cast<float>(texture.height) * fit;
        DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
            {bounds.x + (bounds.width - w) * 0.5F, bounds.y + (bounds.height - h) * 0.5F, w, h}, {}, 0,
            silhouette ? Color{9, 14, 17, 255} : WHITE);
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
    UnlockMenuResult result{.close = input.close, .questId = {}, .shopUnlockId = {}};
    state.motion.advance(input.deltaTime,input.reduceEffects);
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
    state.motion.select(static_cast<int>(state.category),state.selectedId);
    panel(unlockMenuPanelBounds(viewport), l.scale, MenuPageSurface);
    menuPageTitle(l.rect(600,132,720,64),"UNLOCKS",l.scale,state.motion);
    silver(ui, l, 1465, 150, profile.silver);
    if (menuStyle::closeButton(l.rect(1600, 138, 52, 52), l.scale, input.pointer, input.click)) result.close = true;

    for (std::size_t tab = 0; tab < TabNames.size(); ++tab) {
        const float x = 266 + static_cast<float>(tab) * 233;
        const Rectangle bounds = l.rect(x, 220, 216, 66);
        const bool active = static_cast<std::size_t>(state.category) == tab;
        const bool hovered = CheckCollisionPointRec(input.pointer, bounds);
        menuChoicePlate(bounds,l.scale,active,hovered,false,state.motion);
        std::size_t ready = 0;
        for (const auto& e : catalog()) {
            if (static_cast<std::size_t>(e.category) != tab) continue;
            const auto a = availability(e, profile, statuses);
            if (!a.owned && a.eligible) ++ready;
        }
        text(TabNames[tab],l.rect(x+8,228,200,46),21*l.scale,Cream,UiFontRole::Display,1);
        if (ready > 0) DrawCircleV({bounds.x + bounds.width - 11 * l.scale, bounds.y + 11 * l.scale}, 5 * l.scale, Gold);
    }
    const Rectangle grid = l.rect(270, 309, 1348, GridHeight);

    if (CheckCollisionPointRec(input.pointer, grid) && std::isfinite(input.wheel)) state.scroll = std::clamp(state.scroll - input.wheel * 65, 0.0F, maxScroll);
    const Rectangle track = l.rect(1636, 316, 9, GridHeight - 14);
    if (maxScroll > 0) {
        const float thumbHeight = track.height * GridHeight / (GridHeight + maxScroll);
        if (input.click && CheckCollisionPointRec(input.pointer, l.rect(1625, 309, 33, GridHeight))) state.draggingScroll = true;
        if (!input.pointerDown) state.draggingScroll = false;
        if (state.draggingScroll) state.scroll = std::clamp((input.pointer.y - track.y - thumbHeight * 0.5F) / (track.height - thumbHeight), 0.0F, 1.0F) * maxScroll;
        DrawRectangleRounded(track, 1, 6, menuStyle::Disabled);
        DrawRectangleRounded({track.x, track.y + (track.height - thumbHeight) * state.scroll / maxScroll, track.width, thumbHeight}, 1, 6, Muted);
    }
    const Entry* hoveredEntry = nullptr;
    beginUiScissor(static_cast<int>(grid.x), static_cast<int>(grid.y), static_cast<int>(grid.width), static_cast<int>(grid.height));
    for (std::size_t i = 0; i < visible.size(); ++i) {
        const Entry& e = *visible[i]; const auto a = availability(e, profile, statuses);
        const bool character=state.category==UnlockCategory::Classes;
        const float x=272+static_cast<float>(i%(character?4:Columns))*(character?341:CellStep);
        const float y=character?410:314+static_cast<float>(i/Columns)*CellStep-state.scroll;
        const Rectangle cell=l.rect(x,y,character?323:137,character?264:137);
        if (cell.y + cell.height < grid.y || cell.y > grid.y + grid.height) continue;
        const bool hovered = CheckCollisionPointRec(input.pointer, cell) && CheckCollisionPointRec(input.pointer, grid);
        if (hovered) hoveredEntry = &e;
        if (hovered && input.click) { state.selectedId = e.id; result.interaction = true; }
        const bool active = state.selectedId == e.id;
        menuChoicePlate(cell,l.scale,active,hovered,!a.owned && !a.eligible,state.motion);
        const Rectangle artwork=character?l.rect(x+74,y+18,175,175):l.rect(x+12,y+12,113,108);
        icon(ui,e,state.motion.icon(artwork,static_cast<int>(i),active,e.id),!a.owned && !a.eligible);
        if(character) text(e.title,l.rect(x+12,y+210,299,42),27*l.scale,Cream,UiFontRole::Display,1);
        if (a.eligible && !a.owned) {
            panel(l.rect(x + 20, y + 42, 97, 36), l.scale, menuStyle::Inset);
            silver(ui, l, x + 28, y + 46, a.price, profile.silver >= a.price);
        }
        if(canToggleContent(profile,e.unlockId) && profile.disabledUnlocks.contains(e.unlockId))
            text("OFF",l.rect(x+89,y+6,39,22),12*l.scale,Red,UiFontRole::Display,2);
        if (!a.owned && !a.eligible) text("?", l.rect(x + 108, y + 6, 20, 22), 18 * l.scale, Muted, UiFontRole::BodyBold, 1);
        if (a.eligible && !a.owned) text("!", l.rect(x + 108, y + 6, 20, 22), 18 * l.scale, Gold, UiFontRole::BodyBold, 1);
    }
    EndScissorMode();
    state.motion.hover(hoveredEntry?hoveredEntry->id:"");
    state.motion.select(static_cast<int>(state.category),state.selectedId);

    panel(l.rect(248, 802, 1424, 159), l.scale, menuStyle::Inset);
    selected = std::find_if(visible.begin(), visible.end(), [&](const Entry* e) { return e->id == state.selectedId; });
    if (selected != visible.end()) {
        const Entry& e = **selected; const auto a = availability(e, profile, statuses);
        panel(l.rect(268, 820, 119, 119), l.scale, menuStyle::Card);
        icon(ui,e,state.motion.icon(l.rect(279,830,98,98),0,true,e.id),!a.owned && !a.eligible);
        text(e.title, l.rect(410, 814, 970, 36), 26 * l.scale, Cream, UiFontRole::Display);
        text(e.description, l.rect(410, 858, 970, 38), 20 * l.scale, Cream);
        if (e.category == UnlockCategory::Classes && a.owned) {
            // One readable line replaces the role plus three repeated trait lines.
            const std::string traits=localizeText(e.traits[0])+"   /   "+localizeText(e.traits[1]);
            text(traits,l.rect(410,897,970,28),18*l.scale,Gold,UiFontRole::MenuBodyBold);
            text(e.traits[2],l.rect(410,928,970,24),17*l.scale,Red,UiFontRole::MenuBodyBold);
        } else if (a.quest && !a.owned) {
            text(a.eligible?"QUEST COMPLETE":a.quest->definition->description,
                l.rect(410,902,970,30),18*l.scale,a.eligible?Green:Muted);
            menuStyle::progress(l.rect(410,941,970,5),a.quest->fraction);
        }
        if (a.owned) {
            const bool disabled=canToggleContent(profile,e.unlockId) && profile.disabledUnlocks.contains(e.unlockId);
            text(disabled ? "DISABLED" : "UNLOCKED", l.rect(1415, e.category == UnlockCategory::Meta ? 813 : 841, 230, 40), 20 * l.scale, disabled ? Red : Green, UiFontRole::BodyBold, 1);
            if(canToggleContent(profile,e.unlockId)) {
                if(button(l.rect(1415,890,230,44),disabled ? "ENABLE" : "DISABLE",l,input)) {
                    result.toggled=toggleContent(profile,e.unlockId);result.interaction=true;
                }

            }
            if (e.category == UnlockCategory::Meta && button(l.rect(1415, 858, 230, 44), "OPEN SHOP", l, input)) {
                result.shopUnlockId = e.unlockId;
                result.interaction = true;
            }
        } else if (a.eligible && a.price > 0) {
            silver(ui, l, 1474, 815, a.price, profile.silver >= a.price);
            if (button(l.rect(1415, 858, 230, 44), "BUY", l, input, profile.silver >= a.price)) {
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
    if(result.purchase || result.toggled) state.motion.feedback=.36F;
    const auto burst=l.rect(328,880,0,0);
    menuActionBurst({burst.x,burst.y},58*l.scale,state.motion);
    if (hoveredEntry && hoveredEntry->id!=state.selectedId) drawItemTooltip(hoveredEntry->title,hoveredEntry->description,input.pointer,viewport,l.scale);
    return result;
}

} // namespace ian
