#pragma once

#include "app/AppFreezeDrawing.hpp"

namespace ian::app_poison_drawing {

// Rounded cel-shaded bubbles, not textured quads. Camera-facing geometry
// stays readable from the ground and shares world depth/selection coverage.
inline void bubble(Vector3 center, Vector3 right, Vector3 up, Vector3 front,
                   float radius, float amount) {
    using namespace app_freeze_drawing;
    const auto disc = [&](Vector3 p, float r, Color color) {
        color = opacity(color, amount);
        for (int i = 0; i < 16; ++i) {
            const float a = static_cast<float>(i) * PI / 8;
            const float b = a + PI / 8;
            triangle(p, planePoint(p, right, up, {std::cos(a) * r, std::sin(a) * r}),
                planePoint(p, right, up, {std::cos(b) * r, std::sin(b) * r}), color);
        }
    };
    disc(center, radius, {43, 94, 48, 215});
    center = Vector3Add(center, Vector3Scale(front, radius * .025F));
    disc(planePoint(center, right, up, {-radius * .08F, radius * .08F}), radius * .84F,
        {133, 226, 52, 230});
    center = Vector3Add(center, Vector3Scale(front, radius * .025F));
    disc(planePoint(center, right, up, {-radius * .18F, radius * .2F}), radius * .58F,
        {195, 247, 86, 240});
    center = Vector3Add(center, Vector3Scale(front, radius * .025F));
    disc(planePoint(center, right, up, {-radius * .32F, radius * .34F}), radius * .19F,
        {247, 255, 203, 250});
}

inline void emitPoison(Vector3 base, Vector3 right, Vector3 up, Vector3 front,
    float radius, float height, unsigned int seed, double seconds, float amount, bool detailed) {
    const float clock = static_cast<float>(std::fmod(seconds, 4096.0)) +
        app_effect_drawing::atmosphereUnit(seed) * 12;
    // Three gently swelling puffs hug the lower silhouette. Soft edges keep
    // the clouds translucent without square smoke cards or hard floor rings.
    for (int i = 0; i < 3; ++i) {
        const float phase = clock * 1.9F + static_cast<float>(i) * 2.1F;
        const float x = (static_cast<float>(i) - 1) * radius * .56F;
        Vector3 p = Vector3Add(base, Vector3Scale(right, x));
        p.y += height * .22F + std::sin(phase) * radius * .07F;
        p = Vector3Add(p, Vector3Scale(front, radius * .72F));
        app_effect_drawing::emitSoftBillboardDisc(p, right, up,
            radius * (.48F + .06F * std::sin(phase)),
            app_freeze_drawing::opacity({111, 206, 67, 115}, amount), 16);
    }
    const int count = detailed ? 7 : 3;
    for (int i = 0; i < count; ++i) {
        const unsigned int slot = seed * 31U + static_cast<unsigned int>(i) * 137U;
        const float offset = app_effect_drawing::atmosphereUnit(slot);
        const float life = std::fmod(clock * .52F + offset, 1.0F);
        const float fade = std::sin(life * PI);
        const float x = (app_effect_drawing::atmosphereUnit(slot + 17U) - .5F) * radius * 1.9F;
        Vector3 p = Vector3Add(base, Vector3Scale(right, x + std::sin(clock * 2 + offset * 9) * radius * .07F));
        p.y += height * (.15F + life * .92F);
        p = Vector3Add(p, Vector3Scale(front, radius * .90F));
        const float size = radius * (.11F + .065F * offset) * (.72F + life * .45F);
        bubble(p, right, up, front, size, amount * fade);
    }
}

} // namespace ian::app_poison_drawing
