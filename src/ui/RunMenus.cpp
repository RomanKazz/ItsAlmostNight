#include "ui/RunMenus.hpp"
#include "ui/UiMenuLayout.hpp"
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
#include <cmath>
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
    switch (PlayerClassDefinitions[static_cast<std::size_t>(index)].type) {
    case PlayerClass::Lumberjack: return ui.weaponTexture(PlayerWeapon::Axe);
    case PlayerClass::Sharpshooter: return ui.weaponTexture(PlayerWeapon::Rifle);
    case PlayerClass::Mechanic: return ui.coreMenuTexture(CoreMenuTexture::Wrench);
    case PlayerClass::Miner: return ui.weaponTexture(PlayerWeapon::Pickaxe);
    default: return {};
    }
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
        .activate = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE),
        .deltaTime = GetFrameTime()};
}

ClassSelectionLayout classSelectionLayout(Vector2 screen) {
    ClassSelectionLayout layout;
    const bool stacked = screen.y > screen.x;
    layout.columns = stacked ? 2 : 4;
    const float width = stacked ? 900.0F : 1440.0F;
    const float height = stacked ? 1440.0F : 850.0F;
    layout.scale = std::min({1.0F, (screen.x - 64) / width, (screen.y - 64) / height});
    const float s = layout.scale;
    layout.panel = {(screen.x - width * s) * .5F, (screen.y - height * s) * .5F, width * s, height * s};
    const auto rect = [&](float x, float y, float w, float h) {
        return Rectangle{layout.panel.x + x * s, layout.panel.y + y * s, w * s, h * s};
    };
    const float gap = 18;
    const float cardWidth = (width - 64 - gap * static_cast<float>(layout.columns - 1)) /
        static_cast<float>(layout.columns);
    for (int i = 0; i < static_cast<int>(PlayerClassDefinitions.size()); ++i)
        layout.cards[static_cast<std::size_t>(i)] = rect(
            32 + static_cast<float>(i % layout.columns) * (cardWidth + gap),
            124 + static_cast<float>(i / layout.columns) * 288, cardWidth, 264);
    layout.details = stacked ? rect(32, 716, width - 64, 580) : rect(32, 420, width - 64, 286);
    layout.buttons[0] = rect((width - 440) * .5F, height - 96, 440, 64);
    layout.buttons[1] = rect(width - (stacked ? 212 : 282), height - 96, stacked ? 180 : 250, 64);
    layout.buttons[2] = rect(32, height - 96, stacked ? 160 : 180, 64);
    return layout;
}

ClassSelectionResult drawClassSelectionMenu(const GameUi& ui, const MetaProgression& profile,
    PlayerClass& selected, bool& sandbox, bool collectionOnly, ClassSelectionState& state,
    Vector2 screen, const MenuNavigationInput& input) {
    const auto layout = classSelectionLayout(screen);
    const float s = layout.scale;
    const bool stacked = layout.columns == 2;
    constexpr int ClassCount = static_cast<int>(PlayerClassDefinitions.size());
    constexpr int ControlCount = ClassCount + 3;
    ClassSelectionResult result;
    const float dt = std::isfinite(input.deltaTime) ? std::clamp(input.deltaTime, 0.0F, .1F) : 0.0F;
    state.elapsed += dt;
    state.selectionAge += dt;
    const bool wasLaunching = state.launching;
    if (wasLaunching) {
        state.launchElapsed += dt;
        if (state.launchElapsed >= .18F) {
            state.launching = false;
            result.start = true;
        }
    }
    if (!isSelectablePlayerClass(selected)) selected = DefaultPlayerClass;
    const auto unlocked = [&](int i) {
        return sandbox || isPlayerClassUnlocked(PlayerClassDefinitions[static_cast<std::size_t>(i)].type, profile);
    };
    const auto select = [&](int i) {
        state.focused = i;
        if (i < ClassCount) selected = PlayerClassDefinitions[static_cast<std::size_t>(i)].type;
        result.interaction = true;
    };
    state.focused = std::clamp(state.focused, 0, ControlCount - 1);
    if (!wasLaunching) {
        if (input.tabStep) {
            int next = wrap(state.focused + input.tabStep, ControlCount);
            if (collectionOnly && next >= ClassCount && next < ClassCount + 2)
                next = input.tabStep > 0 ? ClassCount + 2 : ClassCount - 1;
            select(next);
        } else if (input.horizontalStep || input.verticalStep) {
            if (state.focused < ClassCount) {
                const int cols = layout.columns;
                if (input.horizontalStep)
                    select((state.focused / cols) * cols + wrap(state.focused % cols + input.horizontalStep, cols));
                else {
                    const int next = state.focused + input.verticalStep * cols;
                    select(next < 0 || next >= ClassCount ? (collectionOnly ? ClassCount + 2 : ClassCount) : next);
                }
            } else if (input.verticalStep) {
                const auto current = std::find_if(PlayerClassDefinitions.begin(), PlayerClassDefinitions.end(),
                    [&](const auto& definition) { return definition.type == selected; });
                select(current == PlayerClassDefinitions.end() ? 0 : static_cast<int>(current - PlayerClassDefinitions.begin()));
            } else if (!collectionOnly) {
                constexpr std::array ButtonOrder{2, 0, 1}; // Match the actual left-to-right footer.
                const auto current = std::find(ButtonOrder.begin(), ButtonOrder.end(), state.focused - ClassCount);
                select(ClassCount + ButtonOrder[static_cast<std::size_t>(wrap(
                    static_cast<int>(current - ButtonOrder.begin()) + input.horizontalStep, 3))]);
            }
        }
        for (int i = 0; i < ClassCount; ++i)
            if (input.click && CheckCollisionPointRec(input.pointer, layout.cards[static_cast<std::size_t>(i)])) select(i);
    }
    if (state.shownClass != selected) {
        state.shownClass = selected;
        state.selectionAge = 0;
    }
    const auto ease = [](float value) {
        const float t = std::clamp(value, 0.0F, 1.0F);
        return 1.0F - (1.0F - t) * (1.0F - t) * (1.0F - t);
    };
    const auto accent = [](int i) {
        constexpr std::array Colors{Color{137, 205, 99, 255}, Color{110, 203, 233, 255},
            Color{244, 182, 91, 255}, Color{191, 153, 237, 255}};
        return Colors[static_cast<std::size_t>(i)];
    };
    const auto blendColor = [](Color from, Color to, float amount) {
        return Color{
            static_cast<unsigned char>(static_cast<float>(from.r) + static_cast<float>(to.r - from.r) * amount),
            static_cast<unsigned char>(static_cast<float>(from.g) + static_cast<float>(to.g - from.g) * amount),
            static_cast<unsigned char>(static_cast<float>(from.b) + static_cast<float>(to.b - from.b) * amount), 255};
    };
    panel(layout.panel, s, {33, 43, 36, 255});
    const Rectangle header{layout.panel.x + 32 * s, layout.panel.y + 26 * s, layout.panel.width - 64 * s, 64 * s};
    text(caption(collectionOnly ? "CLASS COLLECTION" : sandbox ? "CHOOSE SANDBOX CLASS" : "CHOOSE YOUR CLASS"),
        {header.x + 60 * s, header.y, header.width - 120 * s, header.height}, 40 * s, Cream, UiFontRole::Display, 1);
    if (closeButton({header.x + header.width - 46 * s, header.y + 10 * s, 44 * s, 44 * s}, s,
        input.pointer, input.click)) result.back = true;

    const float follow = 1.0F - std::exp(-15.0F * dt);
    for (int i = 0; i < ClassCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        auto bounds = layout.cards[index];
        const auto& definition = PlayerClassDefinitions[index];
        const bool chosen = selected == definition.type;
        const bool hovered = !wasLaunching && CheckCollisionPointRec(input.pointer, bounds);
        state.hover[index] += ((hovered ? 1.0F : 0.0F) - state.hover[index]) * follow;
        state.selection[index] += ((chosen ? 1.0F : 0.0F) - state.selection[index]) * follow;
        const float reveal = input.reduceEffects ? 1.0F : ease((state.elapsed - static_cast<float>(i) * .045F) / .32F);
        bounds.y += (1.0F - reveal) * 22 * s - state.hover[index] * 5 * s;
        const float selectedAmount = state.selection[index];
        panel({bounds.x, bounds.y + 7 * s, bounds.width, bounds.height}, s, {17, 24, 19, 255});
        panel(bounds, s, blendColor(Card, accent(i), selectedAmount * .24F + state.hover[index] * .05F));
        beginUiScissor(static_cast<int>(bounds.x + 4 * s), static_cast<int>(bounds.y + 4 * s),
            static_cast<int>(bounds.width - 8 * s), static_cast<int>(bounds.height - 8 * s));
        const Vector2 center{bounds.x + bounds.width * .5F, bounds.y + 112 * s};
        DrawCircleGradient(center, 115 * s,
            Fade(accent(i), .13F + selectedAmount * .13F), Fade(accent(i), 0));
        if (selectedAmount > .01F && !input.reduceEffects) {
            for (int ray = 0; ray < 8; ++ray) {
                const float angle = static_cast<float>(ray) * PI / 4 + state.elapsed * .09F;
                const auto point = [&](float offset, float radius) {
                    return Vector2{center.x + std::cos(angle + offset) * radius * s,
                        center.y + std::sin(angle + offset) * radius * s};
                };
                DrawTriangle(center, point(.12F, 150), point(-.12F, 150), Fade(accent(i), .055F * selectedAmount));
            }
        }
        const float pulse = !input.reduceEffects && chosen ?
            std::sin(std::min(state.selectionAge / .28F, 1.0F) * PI) * .10F : 0;
        const float iconSize = (162 + selectedAmount * 12 + state.hover[index] * 8 + pulse * 100) * s;
        const float bob = !input.reduceEffects && chosen ? std::sin(state.elapsed * 2) * 2.5F * s : 0;
        const Rectangle iconSlot{center.x - iconSize * .5F, center.y - iconSize * .5F + bob, iconSize, iconSize};
        texture(classIcon(ui, i), {iconSlot.x + 5 * s, iconSlot.y + 8 * s, iconSize, iconSize}, Fade(BLACK, .20F));
        texture(classIcon(ui, i), iconSlot, unlocked(i) ? WHITE : Color{125, 130, 127, 255});
        EndScissorMode();
        DrawRectangleRec({bounds.x + 18 * s, bounds.y + bounds.height - 65 * s, bounds.width - 36 * s, 2 * s},
            Fade(accent(i), chosen ? .75F : .20F));
        text(caption(definition.name), {bounds.x + 12 * s, bounds.y + bounds.height - 54 * s,
            bounds.width - 24 * s, 42 * s}, 27 * s, unlocked(i) ? Cream : Muted, UiFontRole::Display, 1);
        if (chosen) {
            const Vector2 check{bounds.x + bounds.width - 28 * s, bounds.y + 26 * s};
            DrawCircleV(check, 13 * s, Gold);
            DrawLineEx({check.x - 6 * s, check.y}, {check.x - 1 * s, check.y + 5 * s}, 3 * s, Inset);
            DrawLineEx({check.x - 1 * s, check.y + 5 * s}, {check.x + 7 * s, check.y - 5 * s}, 3 * s, Inset);
        }
        if (!unlocked(i)) text(caption("LOCKED"), {bounds.x + 16 * s, bounds.y + 12 * s,
            bounds.width - 32 * s, 24 * s}, 15 * s, Muted, UiFontRole::Display);
        if (state.focused == i || chosen) focusCorners(bounds, s);
    }
    const auto* definition = playerClassDefinition(selected);
    const int selectedIndex = static_cast<int>(definition - PlayerClassDefinitions.data());
    const bool available = unlocked(selectedIndex);
    auto details = layout.details;
    details.y += input.reduceEffects ? 0 : (1.0F - ease(state.selectionAge / .22F)) * 9 * s;
    panel(details, s, Inset);
    const auto slot = [&](float x, float y, float w, float h) {
        return Rectangle{details.x + x * s, details.y + y * s, w * s, h * s};
    };
    const float dw = details.width / s;
    const float passiveWidth = stacked ? dw - 48 : dw * .46F - 48;
    const float growthX = stacked ? 24 : dw * .46F + 24;
    const float growthY = stacked ? 320 : 24;
    const float growthWidth = dw * (stacked ? .46F : .23F) - 40;
    const float gearX = dw * (stacked ? .51F : .72F);
    const float gearY = stacked ? 320 : 24;
    const float gearWidth = dw - gearX - 24;
    const auto body = [&](std::string_view value, float x, float y, float width, Color color = Cream, float size = 21) {
        const auto lines = itemTooltipLines(caption(value), width * s, size * s);
        for (std::size_t i = 0; i < lines.size(); ++i)
            text(lines[i], slot(x, y + static_cast<float>(i) * (size + 6), width, size + 6), size * s, color);
        return y + static_cast<float>(lines.size()) * (size + 6);
    };
    text(caption(definition->passiveName), slot(24, 20, passiveWidth, 40), 29 * s,
        accent(selectedIndex), UiFontRole::Display);
    float passiveY = body(definition->passiveDescription[0], 24, 74, passiveWidth);
    if (selected != PlayerClass::Miner)
        passiveY = body(definition->passiveDescription[1], 24, passiveY + 8, passiveWidth);
    body(definition->drawback, 24, std::max(passiveY + 14, 220.0F), passiveWidth,
        {244, 141, 125, 255}, 19);
    text(caption("PER LEVEL"), slot(growthX, growthY, growthWidth, 30), 18 * s, Green, UiFontRole::Display);
    float growthCursor = growthY + 50;
    for (const auto growth : definition->levelGrowth) growthCursor = body(growth, growthX, growthCursor, growthWidth) + 10;
    if (stacked) DrawLineEx({details.x + 24 * s, details.y + 288 * s},
        {details.x + details.width - 24 * s, details.y + 288 * s}, s, Frame);
    else for (const float x : {dw * .46F, dw * .70F}) DrawLineEx(
        {details.x + x * s, details.y + 24 * s}, {details.x + x * s, details.y + details.height - 24 * s}, s, Frame);
    text(caption("STARTING GEAR"), slot(gearX, gearY, gearWidth, 30), 18 * s, Muted, UiFontRole::Display);
    struct Gear { Texture2D icon; std::string label; std::string_view description; };
    std::vector<Gear> gear;
    constexpr std::array<std::string_view, 7> WeaponNames{"AXE", "PICKAXE", "CLUB", "RIFLE", "BOMB", "ICE WAND", "FIRE WAND"};
    const auto weapon = std::find(PlayerWeaponHotbarOrder.begin(), PlayerWeaponHotbarOrder.end(), definition->startingWeapon);
    const auto weaponIndex = static_cast<std::size_t>(weapon - PlayerWeaponHotbarOrder.begin());
    gear.push_back({ui.weaponTexture(definition->startingWeapon), caption(WeaponNames[weaponIndex]), weaponDescription(definition->startingWeapon)});
    gear.push_back({ui.buildingTexture(definition->startingBuilding), caption(buildingDisplayName(definition->startingBuilding)), buildingDescription(definition->startingBuilding)});
    for (const auto item : definition->startingItems) if (item)
        gear.push_back({ui.lootTexture(*item), caption(lootUpgradeName(*item)), lootUpgradeDescription(*item)});
    const float gearSize = std::min(96.0F, (gearWidth - static_cast<float>(gear.size() - 1) * 12) / static_cast<float>(gear.size()));
    const Gear* hoveredGear = nullptr;
    for (std::size_t i = 0; i < gear.size(); ++i) {
        const float intro = input.reduceEffects ? 1.0F : ease((state.selectionAge - static_cast<float>(i) * .035F) / .22F);
        const auto bounds = slot(gearX + static_cast<float>(i) * (gearSize + 12), gearY + 56 + (1 - intro) * 10, gearSize, gearSize);
        const bool hovered = CheckCollisionPointRec(input.pointer, bounds);
        panel(bounds, s, hovered ? Card : Frame);
        texture(gear[i].icon, {bounds.x + 8 * s, bounds.y + 8 * s, bounds.width - 16 * s, bounds.height - 16 * s});
        if (hovered) hoveredGear = &gear[i];
    }
    if (!available) {
        text(caption(playerClassUnlockRequirement(selected)), slot(gearX, gearY + 172, gearWidth, 54), 16 * s, Muted);
        progress(slot(gearX, gearY + 236, gearWidth, 6), playerClassUnlockProgress(selected, profile));
    }
    if (!collectionOnly) {
        const auto startBounds = layout.buttons[0];
        const bool startHover = CheckCollisionPointRec(input.pointer, startBounds);
        auto startVisual = startBounds;
        const float press = state.launching && !input.reduceEffects ? std::sin(std::min(state.launchElapsed / .18F, 1.0F) * PI) : 0;
        startVisual.x += press * 5 * s; startVisual.width -= press * 10 * s;
        startVisual.y += press * 3 * s;
        panel({startVisual.x, startVisual.y + 6 * s, startVisual.width, startVisual.height}, s, {16, 25, 18, 255});
        panel(startVisual, s, available ? (startHover || state.launching ? Color{255, 220, 66, 255} : Gold) : Disabled);
        text(caption(sandbox ? "START SANDBOX" : "START RUN"), startVisual, 30 * s,
            available ? Cream : Muted, UiFontRole::Display, 1);
        if (state.focused == ClassCount) focusCorners(startVisual, s);
        if (!wasLaunching) {
            if (available && ((input.click && startHover) ||
                (input.activate && state.focused <= ClassCount))) {
                state.launching = true;
                state.launchElapsed = 0;
                result.interaction = true;
            }
            if (input.click && startHover) state.focused = ClassCount;
            if (input.click && CheckCollisionPointRec(input.pointer, layout.buttons[1])) select(ClassCount + 1);
            if (navigationButton(layout.buttons[1], s, sandbox ? "NORMAL RUN" : "SANDBOX", state.focused == ClassCount + 1, input)) {
                sandbox = !sandbox;
                if (!sandbox && !isPlayerClassUnlocked(selected, profile)) selected = DefaultPlayerClass;
                result.interaction = true;
            }
        } else navigationButton(layout.buttons[1], s, sandbox ? "NORMAL RUN" : "SANDBOX", false, {}, false);
    }
    if (input.click && CheckCollisionPointRec(input.pointer, layout.buttons[2])) select(ClassCount + 2);
    if (navigationButton(layout.buttons[2], s, "BACK", state.focused == ClassCount + 2, input)) result.back = true;
    if (result.back) { state.launching = false; result.start = false; }
    if (hoveredGear && !state.launching) drawItemTooltip(hoveredGear->label, hoveredGear->description, input.pointer, screen, s);
    return result;
}

bool drawCreditsMenu(UiMenuMotion& motion, Vector2 screen, const MenuNavigationInput& input) {
    const auto layout=UiMenuLayout::fromViewport(screen);
    motion.advance(input.deltaTime,input.reduceEffects);
    panel(layout.rect(445,190,1030,695),layout.scale,MenuPageSurface);
    menuPageTitle(layout.rect(580,218,760,64),"CREDITS",layout.scale,motion);
    constexpr std::array<std::string_view,5> Credits{
        "MADE WITH RAYLIB","ASSETS: KENNEY","ASSETS: KAYKIT","FONTS: CIKO / RUBIK ONE / BALSAMIQ SANS / DICO","EFFECTS: BRACKEYS"};
    for(std::size_t i=0;i<Credits.size();++i) {
        const float y=340+static_cast<float>(i)*70+(1-motion.reveal(static_cast<int>(i)))*12;
        text(Credits[i],layout.rect(560,y,800,44),26*layout.scale,
            i==0?Gold:Cream,UiFontRole::Display,1);
    }
    const bool close=closeButton(layout.rect(1398,218,46,46),layout.scale,input.pointer,input.click);
    const bool back=menuPrimaryButton(layout.rect(810,774,300,64),layout.scale,"BACK",input.pointer,input.click);
    return close || back || input.activate;
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
