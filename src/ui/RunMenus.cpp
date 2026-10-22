#include "ui/RunMenus.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/UiViewport.hpp"
#include "ui/UiLabels.hpp"
#include "ui/UiItemDescriptions.hpp"
#include "ui/UiItemTooltip.hpp"
#include "localization/Localization.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "combat/CombatDisplayUnits.hpp"
#include "game/Simulation.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace ian {
namespace {
using namespace menuStyle;

std::string caption(std::string_view value) { return localizeText(value); }
int wrap(int value, int count) { return (value % count + count) % count; }
void focusCorners(Rectangle bounds, float scale) {
    const float length = 14 * scale;
    for (int i = 0; i < 4; ++i) {
        const float dx = i % 2 == 0 ? 1.0F : -1.0F;
        const float dy = i < 2 ? 1.0F : -1.0F;
        const Vector2 corner{bounds.x + (dx < 0 ? bounds.width : 0) - dx * 4 * scale,
            bounds.y + (dy < 0 ? bounds.height : 0) - dy * 4 * scale};
        for (const auto end : {Vector2{corner.x + dx * length, corner.y}, Vector2{corner.x, corner.y + dy * length}}) {
            DrawLineEx(corner, end, 5*scale, BLACK);
            DrawLineEx(corner, end, 2*scale, Cream);
        }
    }
}
void texture(Texture2D image, Rectangle slot, Color tint = WHITE) {
    if (!IsTextureValid(image)) return;
    const float size = std::min(slot.width / static_cast<float>(image.width),
        slot.height / static_cast<float>(image.height));
    const Rectangle dest{slot.x + (slot.width - static_cast<float>(image.width) * size) * .5F,
        slot.y + (slot.height - static_cast<float>(image.height) * size) * .5F,
        static_cast<float>(image.width) * size, static_cast<float>(image.height) * size};
    DrawTexturePro(image, {0, 0, static_cast<float>(image.width), static_cast<float>(image.height)}, dest, {}, 0, tint);
}
Texture2D classIcon(const GameUi& ui, int index) {
    if (index == 1) return ui.weaponTexture(PlayerWeapon::Rifle);
    if (index == 3) return ui.weaponTexture(PlayerWeapon::Pickaxe);
    if (index == 4) return ui.weaponTexture(PlayerWeapon::Club);
    if (index == 6) return ui.weaponTexture(PlayerWeapon::FireWand);
    if (index == 7) return ui.lootTexture(LootUpgradeEffect::Hourglass);
    constexpr std::array symbols{CoreMenuTexture::Shield, CoreMenuTexture::Turret,
        CoreMenuTexture::Wrench, CoreMenuTexture::Rock, CoreMenuTexture::Spikes,
        CoreMenuTexture::Heart, CoreMenuTexture::Gem, CoreMenuTexture::Gear};
    return ui.coreMenuTexture(symbols[static_cast<std::size_t>(index)]);
}
bool navigationButton(Rectangle bounds, float scale, std::string_view label,
    bool focused, const MenuNavigationInput& input, bool enabled = true) {
    const bool hovered = CheckCollisionPointRec(input.pointer, bounds);
    panel(bounds, scale, enabled ? selectionFill(focused, hovered) : Disabled);
    text(caption(label), {bounds.x + 14 * scale, bounds.y, bounds.width - 28 * scale, bounds.height},
        25 * scale, enabled ? Cream : Muted, UiFontRole::Display, 1);
    return enabled && ((hovered && input.click) || (focused && input.activate));
}
}

MenuNavigationInput readMenuNavigationInput() {
    const auto pressed = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
    return {.pointer = uiMousePosition(), .click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
        .tabStep = IsKeyPressed(KEY_TAB) ? (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? -1 : 1) : 0,
        .horizontalStep = pressed(KEY_RIGHT) ? 1 : pressed(KEY_LEFT) ? -1 : 0,
        .verticalStep = pressed(KEY_DOWN) ? 1 : pressed(KEY_UP) ? -1 : 0,
        .activate = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)};
}

ClassSelectionLayout classSelectionLayout(Vector2 screen) {
    ClassSelectionLayout layout;
    layout.columns = 2;
    const bool stacked = screen.y > screen.x;
    const float width = stacked ? 900.0F : 1480.0F;
    const float height = stacked ? 1550.0F : 900.0F;
    layout.scale = std::min({1.0F, (screen.x-64)/width, (screen.y-64)/height});
    const float s = layout.scale;
    layout.panel = {(screen.x-width*s)*.5F, (screen.y-height*s)*.5F, width*s, height*s};
    const auto rect = [&](float x, float y, float w, float h) {
        return Rectangle{layout.panel.x+x*s, layout.panel.y+y*s, w*s, h*s};
    };
    const float gridWidth = stacked ? width-64 : 560;
    const float cardWidth = (gridWidth-16)/2;
    for (int i=0; i<8; ++i) layout.cards[static_cast<std::size_t>(i)] = rect(
        32+static_cast<float>(i%2)*(cardWidth+16), 124+static_cast<float>(i/2)*164, cardWidth, 148);
    layout.details = stacked ? rect(32,796,width-64,640) : rect(612,124,width-644,640);
    const float buttonWidth = (width-96)/3;
    for (int i=0; i<3; ++i) layout.buttons[static_cast<std::size_t>(i)] = rect(
        32+static_cast<float>(i)*(buttonWidth+16), height-96, buttonWidth, 64);
    return layout;
}

ClassSelectionResult drawClassSelectionMenu(const GameUi& ui, const MetaProgression& profile,
    PlayerClass& selected, bool& sandbox, bool collectionOnly, ClassSelectionState& state,
    Vector2 screen, const MenuNavigationInput& input) {
    const auto layout = classSelectionLayout(screen);
    const float s = layout.scale;
    ClassSelectionResult result;
    const auto unlocked = [&](int i) { return sandbox || isPlayerClassUnlocked(PlayerClassDefinitions[static_cast<std::size_t>(i)].type, profile); };
    const auto select = [&](int i) {
        state.focused = i;
        if (i < 8) selected = PlayerClassDefinitions[static_cast<std::size_t>(i)].type;
        result.interaction = true;
    };
    state.focused = std::clamp(state.focused, 0, 10);
    if (input.tabStep) {
        int next = wrap(state.focused + input.tabStep, 11);
        if (collectionOnly && next >= 8 && next < 10) next = input.tabStep > 0 ? 10 : 7;
        select(next);
    } else if (input.horizontalStep || input.verticalStep) {
        if (state.focused < 8) {
            const int cols = layout.columns;
            if (input.horizontalStep) select((state.focused / cols) * cols + wrap(state.focused % cols + input.horizontalStep, cols));
            else {
                const int next = state.focused + input.verticalStep * cols;
                select(next < 0 ? (collectionOnly ? 10 : 8) : next >= 8 ? (collectionOnly ? 10 : 8 + std::min(state.focused % cols, 2)) : next);
            }
        } else if (input.verticalStep) {
            const auto current = std::find_if(PlayerClassDefinitions.begin(), PlayerClassDefinitions.end(),
                [&](const auto& definition) { return definition.type == selected; });
            select(current == PlayerClassDefinitions.end() ? 0 : static_cast<int>(current - PlayerClassDefinitions.begin()));
        } else if (!collectionOnly) select(8 + wrap(state.focused - 8 + input.horizontalStep, 3));
    }
    panel(layout.panel, s);
    const Rectangle header{layout.panel.x + 20*s, layout.panel.y + 20*s, layout.panel.width - 40*s, 74*s};
    panel(header, s, Card);
    text(caption(collectionOnly ? "CLASS COLLECTION" : sandbox ? "CHOOSE SANDBOX CLASS" : "CHOOSE YOUR CLASS"),
        {header.x + 20*s, header.y, header.width - 100*s, header.height}, 36*s, Cream, UiFontRole::Display);
    if (closeButton({header.x + header.width - 58*s, header.y + 13*s, 48*s, 48*s}, s, input.pointer, input.click)) result.back = true;
    for (int i = 0; i < 8; ++i) {
        const auto bounds = layout.cards[static_cast<std::size_t>(i)];
        const auto& definition = PlayerClassDefinitions[static_cast<std::size_t>(i)];
        const bool hovered = CheckCollisionPointRec(input.pointer, bounds);
        if (hovered && input.click) select(i);
        const bool chosen = selected == definition.type;
        panel(bounds, s, selectionFill(chosen, hovered, !unlocked(i)));
        const float iconSize = bounds.height - 60*s;
        texture(classIcon(ui, i), {bounds.x + (bounds.width-iconSize)*.5F, bounds.y + 12*s, iconSize, iconSize},
            unlocked(i) ? WHITE : Color{130, 135, 137, 255});
        text(caption(definition.name), {bounds.x + 8*s, bounds.y + bounds.height - 46*s, bounds.width - 16*s, 34*s},
            23*s, unlocked(i) ? Cream : Muted, UiFontRole::Display, 1);
        if (!unlocked(i)) text(caption("LOCKED"), {bounds.x + 10*s, bounds.y + 8*s, bounds.width - 20*s, 20*s},
            13*s, Muted, UiFontRole::MenuBodyBold, 2);
        if (state.focused == i) focusCorners(bounds, s);
    }
    const auto* definition = playerClassDefinition(selected);
    if (!definition) { selected = PlayerClass::Vanguard; definition = &PlayerClassDefinitions[0]; }
    const int selectedIndex = static_cast<int>(definition - PlayerClassDefinitions.data());
    const bool available = unlocked(selectedIndex);
    const auto details = layout.details;
    panel(details, s, Inset);
    const auto slot = [&](float x, float y, float w, float h) {
        return Rectangle{details.x+x*s, details.y+y*s, w*s, h*s};
    };
    const float dw = details.width/s;
    texture(classIcon(ui, selectedIndex), slot(20,16,96,96));
    text(caption(definition->name), slot(136,16,dw-156,40), 34*s, Cream, UiFontRole::Display);
    text(caption(definition->role), slot(136,58,dw-156,26), 22*s);
    text(caption(definition->drawback), slot(136,90,dw-156,24), 18*s, {242,112,91,255}, UiFontRole::MenuBodyBold);

    panel(slot(16,132,dw-32,140), s, Frame);
    text(caption(definition->passiveName), slot(32,142,dw-64,34), 26*s, Gold, UiFontRole::Display);
    for (std::size_t i=0; i<definition->passiveDescription.size(); ++i) {
        text(caption(definition->passiveDescription[i]), slot(32,179+static_cast<float>(i)*28,dw-64,26), 18*s);
    }
    panel(slot(16,284,dw-32,100), s, Frame);
    text(caption("PER LEVEL"), slot(32,294,dw-64,26), 23*s, Green, UiFontRole::Display);
    for (std::size_t i=0; i<definition->levelGrowth.size(); ++i) {
        text(caption(definition->levelGrowth[i]), slot(32,325+static_cast<float>(i)*25,dw-64,24), 18*s);
    }

    text(caption("STARTING GEAR"), slot(20,394,dw-40,32), 25*s, Cream, UiFontRole::Display);
    struct Gear { Texture2D icon; std::string label; std::string_view kind; std::string_view description; bool equipped{}; };
    std::vector<Gear> gear;
    const auto weaponName = [](PlayerWeapon weapon) -> std::string_view {
        constexpr std::array<std::string_view,7> names{"AXE","PICKAXE","CLUB","RIFLE","BOMB","ICE WAND","FIRE WAND"};
        const auto it = std::find(PlayerWeaponHotbarOrder.begin(),PlayerWeaponHotbarOrder.end(),weapon);
        return it == PlayerWeaponHotbarOrder.end() ? "AXE" : names[static_cast<std::size_t>(it-PlayerWeaponHotbarOrder.begin())];
    };
    std::string primaryName = caption(weaponName(definition->startingWeapon));
    gear.push_back({ui.weaponTexture(definition->startingWeapon),primaryName,"EQUIPPED",weaponDescription(definition->startingWeapon),true});
    for (const auto tool : {PlayerWeapon::Axe,PlayerWeapon::Pickaxe}) {
        if (tool != definition->startingWeapon) gear.push_back({ui.weaponTexture(tool),caption(weaponName(tool)),"TOOL",weaponDescription(tool)});
    }
    gear.push_back({ui.buildingTexture(definition->startingBuilding),caption(buildingDisplayName(definition->startingBuilding)),"BLUEPRINT",buildingDescription(definition->startingBuilding)});
    for (const auto item : definition->startingItems) {
        if (item) gear.push_back({ui.lootTexture(*item),caption(lootUpgradeName(*item)),"ITEM",lootUpgradeDescription(*item)});
    }
    const float gap = 8;
    const float gearWidth = (dw-40-gap*static_cast<float>(gear.size()-1))/static_cast<float>(gear.size());
    const Gear* hoveredGear = nullptr;
    for (std::size_t i=0; i<gear.size(); ++i) {
        const float x = 20+static_cast<float>(i)*(gearWidth+gap);
        panel(slot(x,438,gearWidth,120), s, gear[i].equipped ? Card : Frame);
        if (CheckCollisionPointRec(input.pointer,slot(x,438,gearWidth,120))) hoveredGear = &gear[i];
        texture(gear[i].icon, slot(x+(gearWidth-74)*.5F,442,74,74));
        text(gear[i].label, slot(x+5,515,gearWidth-10,24), 16*s, Cream, UiFontRole::MenuBodyBold,1);
        text(caption(gear[i].kind), slot(x+5,541,gearWidth-10,14), 10*s,
            gear[i].equipped ? Gold : Muted, UiFontRole::MenuBodyBold,1);
    }
    text(caption(definition->startingLoadoutLabel), slot(20,566,dw-40,24), 17*s, Gold, UiFontRole::MenuBodyBold);
    if (!available) {
        text(caption(playerClassUnlockRequirement(selected)), slot(20,596,dw-40,22), 17*s, Muted, UiFontRole::MenuBodyBold);
        progress(slot(20,624,dw-40,6),playerClassUnlockProgress(selected,profile));
    }
    if (!collectionOnly) {
        for (int i = 0; i < 3; ++i) {
            if (input.click && CheckCollisionPointRec(input.pointer, layout.buttons[static_cast<std::size_t>(i)])) select(8 + i);
        }
        const bool start = navigationButton(layout.buttons[0], s, sandbox ? "START SANDBOX" : "START RUN", state.focused == 8, input, available);
        result.start = start || (input.activate && state.focused < 8 && available);
        if (navigationButton(layout.buttons[1], s, sandbox ? "NORMAL RUN" : "SANDBOX", state.focused == 9, input)) {
            sandbox = !sandbox;
            if (!sandbox && !isPlayerClassUnlocked(selected, profile)) selected = PlayerClass::Vanguard;
            result.interaction = true;
        }
    }
    if (collectionOnly && input.click && CheckCollisionPointRec(input.pointer, layout.buttons[2])) select(10);
    if (navigationButton(layout.buttons[2], s, "BACK", state.focused == 10, input)) result.back = true;
    if (hoveredGear) drawItemTooltip(hoveredGear->label,hoveredGear->description,input.pointer,screen,s);
    return result;
}

PauseMenuLayout pauseMenuLayout(Vector2 screen) {
    PauseMenuLayout layout;
    layout.scale = std::min({1.0F, (screen.x - 64)/560, (screen.y - 64)/560});
    const float s = layout.scale;
    layout.panel = {(screen.x-560*s)*.5F, (screen.y-560*s)*.5F, 560*s, 560*s};
    for (int i = 0; i < 5; ++i) layout.buttons[static_cast<std::size_t>(i)] = {
        layout.panel.x + 32*s, layout.panel.y + (126 + static_cast<float>(i)*82)*s, 496*s, 66*s};
    return layout;
}

PauseMenuAction drawPauseMenu(PauseMenuState& state, bool sandbox, Vector2 screen, const MenuNavigationInput& input) {
    const auto layout = pauseMenuLayout(screen);
    const float s = layout.scale;
    const int step = input.tabStep ? input.tabStep : input.verticalStep;
    state.focused = wrap(state.focused + step, 5);
    panel(layout.panel, s);
    const Rectangle header{layout.panel.x + 20*s, layout.panel.y + 20*s, layout.panel.width - 40*s, 80*s};
    panel(header, s, Card);
    text(caption("PAUSED"), header, 40*s, Cream, UiFontRole::Display, 1);
    const std::array<std::string_view, 5> labels{"RESUME", "SETTINGS", "RESTART RUN",
        sandbox ? "MAIN MENU" : "SAVE & MAIN MENU", sandbox ? "EXIT" : "SAVE & EXIT"};
    PauseMenuAction result = PauseMenuAction::None;
    for (int i = 0; i < 5; ++i) {
        const auto bounds = layout.buttons[static_cast<std::size_t>(i)];
        if (input.click && CheckCollisionPointRec(input.pointer, bounds)) state.focused = i;
        if (navigationButton(bounds, s, labels[static_cast<std::size_t>(i)], state.focused == i, input)) result = static_cast<PauseMenuAction>(i+1);
    }
    return result;
}

RunSummaryLayout runSummaryLayout(Vector2 screen) {
    RunSummaryLayout layout;
    layout.scale = std::min({1.0F, (screen.x - 64) / 960, (screen.y - 64) / 700});
    const float s = layout.scale;
    layout.panel = {(screen.x - 960*s)*.5F, (screen.y - 700*s)*.5F, 960*s, 700*s};
    const auto rect = [&](float x, float y, float w, float h) {
        return Rectangle{layout.panel.x+x*s, layout.panel.y+y*s, w*s, h*s};
    };
    layout.header = rect(20, 20, 920, 86);
    for (int i = 0; i < 4; ++i) layout.highlights[static_cast<std::size_t>(i)] =
        rect(32 + static_cast<float>(i)*228, 128, 212, 112);
    for (int i = 0; i < 2; ++i) {
        layout.details[static_cast<std::size_t>(i)] = rect(32 + static_cast<float>(i)*456, 260, 440, 310);
        layout.buttons[static_cast<std::size_t>(i)] = rect(32 + static_cast<float>(i)*456, 600, 440, 68);
    }
    return layout;
}

RunSummaryAction drawRunSummaryMenu(const SimulationSnapshot& snapshot, RunSummaryState& state,
    Vector2 screen, const MenuNavigationInput& input) {
    const auto layout = runSummaryLayout(screen);
    const float s = layout.scale;
    const int step = input.tabStep ? input.tabStep : input.horizontalStep ? input.horizontalStep : input.verticalStep;
    state.focused = wrap(state.focused + step, 2);
    DrawRectangleRec({0, 0, screen.x, screen.y}, {0, 0, 0, 120});
    panel(layout.panel, s);
    panel(layout.header, s, Card);
    text(caption(snapshot.stageCleared ? "FORT DEFENDED" : "CORE DESTROYED"), layout.header,
        42*s, snapshot.stageCleared ? Gold : Cream, UiFontRole::Display, 1);
    const auto amount = [](int value) {
        if (value < 1000) return std::to_string(value);
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), value < 10000 ? "%.1fk" : "%.0fk", static_cast<double>(value)/1000);
        return std::string{buffer};
    };
    const int elapsed = std::max(0, static_cast<int>(snapshot.elapsedSeconds));
    char time[32]{};
    std::snprintf(time, sizeof(time), "%02d:%02d", elapsed/60, elapsed%60);
    const auto& stats = snapshot.runStatistics;
    const std::array<std::string_view, 4> labels{"SURVIVED", "WAVES CLEARED", "ENEMIES DEFEATED", "TOTAL DAMAGE"};
    const std::array<std::string, 4> values{time, std::to_string(stats.wavesSurvived),
        amount(stats.enemiesDefeated), amount(combatDisplayValue(stats.playerDamageDealt+stats.defenseDamageDealt))};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const auto bounds = layout.highlights[i];
        panel(bounds, s, Inset);
        text(caption(labels[i]), {bounds.x+12*s, bounds.y+10*s, bounds.width-24*s, 30*s},
            17*s, Muted, UiFontRole::MenuBodyBold, 1);
        text(values[i], {bounds.x+12*s, bounds.y+46*s, bounds.width-24*s, 52*s},
            36*s, Gold, UiFontRole::Display, 1);
    }
    const std::array<std::string_view, 8> statLabels{"PLAYER DAMAGE", "DEFENSE DAMAGE", "STRUCTURES BUILT",
        "STRUCTURES LOST", "WOOD ACQUIRED", "STONE ACQUIRED", "CRYSTALS ACQUIRED", "COINS COLLECTED"};
    const std::array<int, 8> statValues{combatDisplayValue(stats.playerDamageDealt),
        combatDisplayValue(stats.defenseDamageDealt), stats.structuresBuilt, stats.structuresLost,
        stats.woodAcquired, stats.stoneAcquired, stats.crystalsAcquired, stats.coinsCollected};
    for (std::size_t column = 0; column < layout.details.size(); ++column) {
        const auto bounds = layout.details[column];
        panel(bounds, s, Inset);
        text(caption(column == 0 ? "COMBAT" : "RESOURCES"),
            {bounds.x+20*s, bounds.y+14*s, bounds.width-40*s, 42*s}, 27*s, Cream, UiFontRole::Display);
        for (std::size_t row = 0; row < 4; ++row) {
            const std::size_t i = column*4+row;
            const Rectangle line{bounds.x+14*s, bounds.y+(72+static_cast<float>(row)*54)*s, bounds.width-28*s, 48*s};
            if (row%2 == 0) DrawRectangleRec(line, AlternateRow);
            text(caption(statLabels[i]), {line.x+8*s, line.y, line.width-98*s, line.height}, 18*s, Muted, UiFontRole::MenuBodyBold);
            text(amount(statValues[i]), {line.x+line.width-88*s, line.y, 80*s, line.height},
                23*s, Cream, UiFontRole::MenuBodyBold, 2);
        }
    }
    RunSummaryAction result = RunSummaryAction::None;
    constexpr std::array<std::string_view, 2> actions{"RESTART RUN", "RETURN TO MAIN MENU"};
    for (std::size_t i = 0; i < actions.size(); ++i) {
        const auto bounds = layout.buttons[i];
        if (input.click && CheckCollisionPointRec(input.pointer, bounds)) state.focused = static_cast<int>(i);
        if (navigationButton(bounds, s, actions[i], state.focused == static_cast<int>(i), input))
            result = i == 0 ? RunSummaryAction::Restart : RunSummaryAction::MainMenu;
    }
    return result;
}

} // namespace ian
