#pragma once

#include "enemies/EnemySystem.hpp"
#include "localization/Localization.hpp"
#include "ui/UiMenuStyle.hpp"

#include <array>
#include <cmath>
#include <string>

namespace ian {

struct EnemySpawnMenuInput {
    Vector2 pointer{-1, -1};
    bool click{};
    bool down{};
    bool enter{};
    bool fastStep{};
    float wheel{};
};

struct EnemySpawnMenuResult { bool summon{}; bool close{}; };

namespace enemy_spawn_menu {

struct Entry {
    EnemyType type;
    const char* name;
    const char* role;
    const char* description;
    Color color;
};
inline constexpr std::array<Entry, 9> Enemies{{
    {EnemyType::Basic, "BASIC", "THE HORDE", "Classic crowd. Perfect for\ntrying out a full wave.", {222, 105, 171, 255}},
    {EnemyType::Fast, "FAST", "QUICK FEET", "Fast runners that close\nthe distance quickly.", {196, 116, 66, 255}},
    {EnemyType::Heavy, "HEAVY", "TOUGH TARGET", "A sturdy front line for\ntesting sustained damage.", {114, 157, 192, 255}},
    {EnemyType::Boss, "BOSS", "BIG TROUBLE", "A boss encounter with powerful\nattacks and abilities.", {176, 96, 196, 255}},
    {EnemyType::Ranged, "RANGED", "KEEP YOUR DISTANCE", "Ranged attackers for testing\nincoming projectiles.", {143, 110, 203, 255}},
    {EnemyType::Sapper, "SAPPER", "EXPLOSIVE", "An explosive visitor.\nKeep an eye on your buildings.", {226, 134, 67, 255}},
    {EnemyType::Flying, "FLYING", "ABOVE THE GROUND", "Airborne enemies for testing\nanti-air defenses.", {103, 182, 199, 255}},
    {EnemyType::Splitter, "SPLITTER", "THREE LITTLE SURPRISES", "Splits into three little enemies\nwhen defeated.", {121, 170, 101, 255}},
    {EnemyType::Splitling, "SPLITLING", "SMALL BUT LIVELY", "The little splitter offspring,\nspawned directly.", {143, 191, 103, 255}},
}};
inline constexpr std::array<EliteAffix, 4> Variants{
    EliteAffix::None, EliteAffix::Berserker, EliteAffix::Warden, EliteAffix::Volatile};
inline constexpr std::array<const char*, 4> VariantNames{"NORMAL", "BERSERKER", "WARDEN", "VOLATILE"};
inline constexpr std::array<Color, 4> VariantColors{
    menuStyle::Cream, Color{255, 125, 104, 255}, Color{123, 202, 255, 255}, Color{255, 186, 74, 255}};
inline constexpr std::array<int, 6> Presets{1, 10, 50, 100, 500, 1000};

struct Layout {
    Rectangle panel{}, catalog{}, details{}, close{}, minus{}, plus{}, count{}, slider{}, summon{};
    std::array<Rectangle, 9> cards{};
    std::array<Rectangle, 4> variants{};
    std::array<Rectangle, 6> presets{};
    float scale{1};
    bool portrait{};
};

inline Layout layout(Vector2 screen) {
    Layout l;
    l.portrait = screen.x / std::max(1.0F, screen.y) < 1.15F;
    const float width = l.portrait ? 760.0F : 1240.0F;
    const float height = l.portrait ? 1190.0F : 792.0F;
    l.scale = std::max(.05F, std::min({l.portrait ? 2.0F : 1.0F,
        (screen.x - 40) / width, (screen.y - 40) / height}));
    const float s = l.scale;
    l.panel = {(screen.x - width * s) * .5F, (screen.y - height * s) * .5F, width * s, height * s};
    const auto r = [&](float x, float y, float w, float h) {
        return Rectangle{l.panel.x + x * s, l.panel.y + y * s, w * s, h * s};
    };
    l.close = r(width - 74, 24, 46, 46);
    l.catalog = r(28, 124, l.portrait ? 704.0F : 788.0F, l.portrait ? 498.0F : 588.0F);
    const float gap = 14 * s;
    const float cw = (l.catalog.width - 2 * gap) / 3;
    const float ch = (l.catalog.height - 2 * gap) / 3;
    for (std::size_t i = 0; i < l.cards.size(); ++i)
        l.cards[i] = {l.catalog.x + static_cast<float>(i % 3) * (cw + gap),
            l.catalog.y + static_cast<float>(i / 3) * (ch + gap), cw, ch};
    l.details = l.portrait ? r(28, 642, 704, 520) : r(840, 124, 372, 640);
    const auto d = [&](float x, float y, float w, float h) {
        return Rectangle{l.details.x + x * s, l.details.y + y * s, w * s, h * s};
    };
    const float dw = l.details.width / s;
    const float variantY = l.portrait ? 196.0F : 208.0F;
    for (std::size_t i = 0; i < l.variants.size(); ++i) {
        const float columns = l.portrait ? 4.0F : 2.0F;
        const float vw = (dw - 40 - (columns - 1) * 8) / columns;
        l.variants[i] = d(20 + static_cast<float>(l.portrait ? i : i % 2) * (vw + 8),
            variantY + static_cast<float>(l.portrait ? 0 : i / 2) * 50, vw, 42);
    }
    const float countY = l.portrait ? 280.0F : 354.0F;
    l.minus = d(20, countY, 48, 54);
    l.plus = d(dw - 68, countY, 48, 54);
    l.count = d(78, countY, dw - 156, 54);
    l.slider = d(28, countY + 66, dw - 56, 16);
    for (std::size_t i = 0; i < l.presets.size(); ++i) {
        const float columns = l.portrait ? 6.0F : 3.0F;
        const float pw = (dw - 40 - (columns - 1) * 8) / columns;
        l.presets[i] = d(20 + static_cast<float>(l.portrait ? i : i % 3) * (pw + 8),
            countY + 98 + static_cast<float>(l.portrait ? 0 : i / 3) * 44, pw, 36);
    }
    l.summon = d(20, l.details.height / s - 80, dw - 40, 58);
    return l;
}

// Lightweight vector portraits; no per-frame model targets or asset loading.
inline void portrait(EnemyType type, Rectangle bounds, Color pigment, bool elite = false) {
    const float u = std::min(bounds.width, bounds.height);
    const Vector2 c{bounds.x + bounds.width * .5F, bounds.y + bounds.height * .54F};
    const auto ellipse = [&](float x, float y, float rx, float ry, Color color) {
        DrawEllipse(static_cast<int>(std::lround(c.x + x * u)),
            static_cast<int>(std::lround(c.y + y * u)), rx * u, ry * u, color);
    };
    const auto triangle = [&](Vector2 a, Vector2 b, Vector2 d, Color color) {
        std::array<Vector2, 3> vertices{{{c.x + a.x * u, c.y + a.y * u},
            {c.x + b.x * u, c.y + b.y * u}, {c.x + d.x * u, c.y + d.y * u}}};
        drawSolidTriangleFan(vertices.data(), 3, color);
    };
    if (type == EnemyType::Flying) {
        triangle({-.15F, -.04F}, {-.48F, -.30F}, {-.40F, .12F}, pigment);
        triangle({.15F, -.04F}, {.40F, .12F}, {.48F, -.30F}, pigment);
    }
    if (type == EnemyType::Splitter || type == EnemyType::Splitling) {
        for (int i = 0; i < 5; ++i) {
            const float x = -.28F + static_cast<float>(i) * .14F;
            triangle({x - .06F, -.18F}, {x + .06F, -.18F}, {x, type == EnemyType::Splitter ? -.52F : -.35F},
                {200, 218, 206, 255});
        }
    }
    ellipse(0, .04F, .35F, .32F, {17, 23, 19, 255});
    ellipse(0, .025F, .32F, .30F, pigment);
    ellipse(-.13F, -.105F, .15F, .08F, ColorBrightness(pigment, .14F));
    if (type == EnemyType::Heavy) {
        menuStyle::panel({c.x - .38F * u, c.y + .04F * u, .76F * u, .20F * u}, u / 180,
            {91, 117, 141, 255});
    }
    ellipse(0, -.08F, .16F, .15F, {23, 28, 25, 255});
    ellipse(0, -.09F, .14F, .13F, {248, 245, 224, 255});
    ellipse(.025F, -.09F, .068F, type == EnemyType::Fast ? .027F : .045F, {26, 32, 27, 255});
    ellipse(-.02F, -.12F, .02F, .018F, WHITE);
    if (type == EnemyType::Fast) {
        DrawLineEx({c.x - .27F * u, c.y - .07F * u}, {c.x + .28F * u, c.y - .07F * u}, .05F * u, {73, 47, 41, 255});
    }
    if (type == EnemyType::Ranged) {
        triangle({-.28F, -.26F}, {.28F, -.26F}, {-.03F, -.54F}, {103, 73, 172, 255});
        ellipse(0, -.24F, .31F, .04F, {238, 196, 89, 255});
    }
    if (type == EnemyType::Sapper) {
        DrawCircleV({c.x + .28F * u, c.y + .18F * u}, .13F * u, {35, 40, 36, 255});
        DrawLineEx({c.x + .30F * u, c.y + .06F * u}, {c.x + .34F * u, c.y}, .025F * u, menuStyle::Gold);
        DrawCircleV({c.x + .34F * u, c.y}, .04F * u, {255, 163, 62, 255});
    }
    if (type == EnemyType::Boss || elite) {
        const Color crown = menuStyle::Gold;
        triangle({-.25F, -.25F}, {-.07F, -.25F}, {-.27F, -.46F}, crown);
        triangle({-.12F, -.25F}, {.12F, -.25F}, {0, -.50F}, crown);
        triangle({.07F, -.25F}, {.25F, -.25F}, {.27F, -.46F}, crown);
        DrawLineEx({c.x - .24F * u, c.y - .25F * u}, {c.x + .24F * u, c.y - .25F * u}, .07F * u, crown);
    }
}

} // namespace enemy_spawn_menu

inline EnemySpawnMenuResult drawEnemySpawnCatalog(EnemyType& selected, EliteAffix& variant, int& count,
    int activeEnemies, Vector2 screen, const EnemySpawnMenuInput& input = {}) {
    namespace es = enemy_spawn_menu;
    namespace ms = menuStyle;
    const auto l = es::layout(screen);
    const float s = l.scale;
    constexpr int Maximum = static_cast<int>(EnemySystem::MaxActiveEnemies);
    count = std::clamp(count, 1, Maximum);
    const ScopedSdfUiTextCache directText(false);
    const auto label = [&](std::string_view value, Rectangle r, float size, Color color = ms::Cream,
                           UiFontRole role = UiFontRole::Display, int align = 0) {
        ms::text(value, r, size * s, color, role, align);
    };
    DrawRectangle(0, 0, static_cast<int>(screen.x), static_cast<int>(screen.y), {6, 12, 9, 185});
    ms::panel(l.panel, s);
    label("SUMMON ENEMIES", {l.panel.x + 28 * s, l.panel.y + 20 * s, l.panel.width - 130 * s, 44 * s}, 32, ms::Gold);
    label("DEBUG SPAWNER · B", {l.panel.x + 28 * s, l.panel.y + 70 * s, l.panel.width - 130 * s, 26 * s},
        16, ms::Muted, UiFontRole::Body);
    EnemySpawnMenuResult result;
    result.close = ms::closeButton(l.close, s, input.pointer, input.click);
    const es::Entry* focused = &es::Enemies.front();
    for (std::size_t i = 0; i < es::Enemies.size(); ++i) {
        const auto& enemy = es::Enemies[i];
        const Rectangle card = l.cards[i];
        const bool hovered = CheckCollisionPointRec(input.pointer, card);
        if (hovered && input.click) selected = enemy.type;
        const bool chosen = selected == enemy.type;
        if (chosen) focused = &enemy;
        ms::panel(card, s, ms::selectionFill(chosen, hovered));
        es::portrait(enemy.type, {card.x + 16 * s, card.y + 6 * s, card.width - 32 * s, card.height - 64 * s}, enemy.color);
        label(enemy.name, {card.x + 12 * s, card.y + card.height - 57 * s, card.width - 24 * s, 28 * s}, 21, ms::Cream, UiFontRole::Display, 1);
        label(enemy.role, {card.x + 10 * s, card.y + card.height - 27 * s, card.width - 20 * s, 18 * s},
            11, chosen ? Color{67, 61, 38, 255} : ms::Muted, UiFontRole::BodyBold, 1);
    }
    ms::panel(l.details, s, ms::Inset);
    const float dw = l.details.width;
    es::portrait(selected, {l.details.x + 20 * s, l.details.y + 18 * s, 84 * s, 84 * s},
        focused->color, variant != EliteAffix::None);
    label(focused->name, {l.details.x + 112 * s, l.details.y + 20 * s, dw - 132 * s, 38 * s}, 25, ms::Gold);
    label(focused->role, {l.details.x + 112 * s, l.details.y + 64 * s, dw - 132 * s, 28 * s}, 12, ms::Muted, UiFontRole::BodyBold);
    label(focused->description, {l.details.x + 20 * s, l.details.y + 110 * s, dw - 40 * s, 42 * s},
        15, ms::Cream, UiFontRole::Body);
    const float variantTop = l.variants[0].y;
    label("VARIANT", {l.details.x + 20 * s, variantTop - 36 * s, dw - 40 * s, 28 * s}, 19, ms::Muted);
    for (std::size_t i = 0; i < es::Variants.size(); ++i) {
        const Rectangle r = l.variants[i];
        const bool hovered = CheckCollisionPointRec(input.pointer, r);
        if (hovered && input.click) variant = es::Variants[i];
        ms::panel(r, s, ms::selectionFill(variant == es::Variants[i], hovered));
        label(es::VariantNames[i], r, 16, variant == es::Variants[i] ? ms::Cream : es::VariantColors[i], UiFontRole::Display, 1);
    }
    label("HOW MANY?", {l.details.x + 20 * s, l.count.y - 36 * s, dw - 40 * s, 28 * s}, 19, ms::Muted);
    const int step = input.fastStep ? 10 : 1;
    if (ms::button(l.minus, s, "-", 24, input.pointer, input.click, count > 1)) count = std::max(1, count - step);
    if (ms::button(l.plus, s, "+", 24, input.pointer, input.click, count < Maximum)) count = std::min(Maximum, count + step);
    if (CheckCollisionPointRec(input.pointer, l.count))
        count = std::clamp(count + static_cast<int>(std::clamp(input.wheel, -100.0F, 100.0F)) * step, 1, Maximum);
    ms::panel(l.count, s, ms::Card);
    label(std::to_string(count), l.count, 34, ms::Cream, UiFontRole::Display, 1);
    const float fraction = static_cast<float>(count - 1) / static_cast<float>(Maximum - 1);
    ms::progress(l.slider, fraction, ms::Gold);
    const Rectangle sliderHit{l.slider.x - 8 * s, l.slider.y - 8 * s, l.slider.width + 16 * s, l.slider.height + 16 * s};
    if (input.down && CheckCollisionPointRec(input.pointer, sliderHit))
        count = std::clamp(1 + static_cast<int>(std::lround(
            std::clamp((input.pointer.x - l.slider.x) / l.slider.width, 0.0F, 1.0F) * static_cast<float>(Maximum - 1))), 1, Maximum);
    for (std::size_t i = 0; i < es::Presets.size(); ++i) {
        const Rectangle r = l.presets[i];
        const bool hovered = CheckCollisionPointRec(input.pointer, r);
        if (hovered && input.click) count = es::Presets[i];
        ms::panel(r, s, ms::selectionFill(count == es::Presets[i], hovered));
        label(std::to_string(es::Presets[i]), r, 17, ms::Cream, UiFontRole::Display, 1);
    }
    const bool available = activeEnemies < Maximum;
    result.summon = !result.close && (ms::button(l.summon, s, localizeText("SUMMON"), 26,
        input.pointer, input.click, available) || (input.enter && available));
    if (!l.portrait) {
        label(localizeText("ACTIVE") + ": " + std::to_string(activeEnemies) + " / " + std::to_string(Maximum),
            {l.catalog.x, l.panel.y + l.panel.height - 64 * s, l.catalog.width, 26 * s}, 16, ms::Muted, UiFontRole::Body);
        label("Enter: summon  ·  Esc / B: close  ·  Shift: +/- 10",
            {l.catalog.x, l.panel.y + l.panel.height - 34 * s, l.catalog.width, 20 * s}, 13, ms::Muted, UiFontRole::Body);
    }
    return result;
}

} // namespace ian
