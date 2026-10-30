#pragma once

#include "ui/UiMenuStyle.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>

namespace ian {

// Shared, elapsed-time motion for the main-menu pages. Hit areas stay fixed.
struct UiMenuMotion {
    float elapsed{};
    float sectionAge{};
    float selectionAge{1};
    float hoverAge{1};
    float feedback{};
    int section{-1};
    std::string selection;
    std::string hovered;
    std::string previousHover;
    bool reduced{};

    void advance(float deltaTime, bool reduceEffects = false) {
        const float dt = std::isfinite(deltaTime) ? std::clamp(deltaTime, 0.0F, .1F) : 0;
        elapsed += dt;
        sectionAge += dt;
        selectionAge += dt;
        hoverAge += dt;
        feedback = std::max(0.0F, feedback - dt);
        reduced = reduceEffects;
    }
    void select(int page, std::string_view id) {
        if (section != page) { section = page; sectionAge = 0; }
        if (selection != id) { selection = id; selectionAge = 0; }
    }
    void hover(std::string_view id) {
        if (hovered == id) return;
        previousHover = hovered;
        hovered = id;
        hoverAge = 0;
    }
    static float ease(float value) {
        const float t = std::clamp(value, 0.0F, 1.0F);
        return 1 - (1 - t) * (1 - t) * (1 - t);
    }
    float hoverAmount(std::string_view id) const {
        const float amount = ease(hoverAge / .14F);
        return hovered == id ? amount : previousHover == id ? 1 - amount : 0;
    }
    float reveal(int order = 0) const {
        return reduced ? 1 : ease((sectionAge - static_cast<float>(std::min(order, 10)) * .018F) / .28F);
    }
    float selectionPulse() const {
        return reduced ? 0 : std::sin(std::clamp(selectionAge / .28F, 0.0F, 1.0F) * PI);
    }
    Rectangle icon(Rectangle bounds, int order, bool selected, std::string_view id) const {
        const float amount = reduced ? 1 : .94F + reveal(order) * .06F +
            hoverAmount(id) * .045F + (selected ? selectionPulse() * .05F : 0);
        const float width = bounds.width * amount, height = bounds.height * amount;
        return {bounds.x + (bounds.width - width) * .5F,
            bounds.y + (bounds.height - height) * .5F + (1 - reveal(order)) * bounds.height * .08F,
            width, height};
    }
};

inline constexpr Color MenuPageSurface{33, 43, 36, 255};

inline void menuPageTitle(Rectangle bounds, std::string_view title, float scale, const UiMenuMotion& motion) {
    bounds.y += (1 - motion.reveal()) * 7 * scale;
    menuStyle::text(title, bounds, 38 * scale, menuStyle::Cream, UiFontRole::Display, 1);
}

inline void menuChoicePlate(Rectangle bounds, float scale, bool selected, bool hovered,
    bool locked, const UiMenuMotion& motion) {
    menuStyle::panel(bounds, scale, locked ? menuStyle::Disabled : selected ? Color{87, 91, 71, 255}
        : hovered ? menuStyle::Hover : menuStyle::Card);
    if (selected) {
        const float inset = 10 * scale;
        DrawRectangleRec({bounds.x + inset, bounds.y + bounds.height - 5 * scale,
            bounds.width - 2 * inset, 2 * scale}, menuStyle::Gold);
        if (!motion.reduced && motion.feedback > 0)
            DrawRectangleLinesEx(bounds, 3 * scale, Fade(menuStyle::Gold, motion.feedback / .36F));
    }
}

inline bool menuPrimaryButton(Rectangle bounds, float scale, std::string_view label,
    Vector2 pointer, bool click, bool enabled = true) {
    const bool hovered = enabled && CheckCollisionPointRec(pointer,bounds);
    menuStyle::panel(bounds,scale,!enabled?menuStyle::Disabled:hovered?Color{255,220,66,255}:menuStyle::Gold);
    menuStyle::text(label,{bounds.x+8*scale,bounds.y,bounds.width-16*scale,bounds.height},
        22*scale,enabled?menuStyle::Cream:menuStyle::Muted,UiFontRole::Display,1);
    return hovered && click;
}

inline void menuActionBurst(Vector2 center, float radius, const UiMenuMotion& motion) {
    if (motion.reduced || motion.feedback <= 0) return;
    const float t = 1 - motion.feedback / .36F;
    for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i) * PI / 4;
        DrawCircleV({center.x + std::cos(angle) * radius * (1 + t * .6F),
            center.y + std::sin(angle) * radius * (1 + t * .6F)}, radius * .055F * (1 - t),
            Fade(menuStyle::Gold, (1 - t) * .75F));
    }
}

} // namespace ian
