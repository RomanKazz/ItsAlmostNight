#pragma once

#include "graphics/FireWandEffect.hpp"
#include "graphics/WandPresentation.hpp"

namespace ian::wand_presentation {

// A single small triangle batch. The solid model remains the silhouette;
// glints and a few converging motes communicate the charge without a shell.
inline void drawHead(bool fire, float time, float charge, float recoil,
                     bool particles, Vector3 right, Vector3 up, Vector3 front) {
    using namespace fire_wand_effect;
    const float energy = std::max(ease(charge), ease((recoil - .7F) / .3F));
    const float flash = releaseFlash(recoil);
    const Color color = fire ? Color{255, 150, 36, 255} : Color{83, 210, 255, 255};
    const Color light = fire ? Color{255, 244, 170, 255} : Color{222, 255, 255, 255};
    const Vector3 center = Vector3Add({0, .35F, 0}, Vector3Scale(front, .075F));
    beginBatch();
    const float glow = .025F + energy * .022F + flash * .026F;
    disc(center, right, up, glow * 2.2F,
         opacity(color, .12F + energy * .14F + flash * .35F), opacity(color, 0), 12);
    const auto glint = [&](Vector3 point, float size, float alpha) {
        const Color c = opacity(light, alpha);
        vertex(planePoint(point, right, up, 0, size), c);
        vertex(planePoint(point, right, up, -size * .26F, 0), c);
        vertex(planePoint(point, right, up, 0, -size), c);
        vertex(planePoint(point, right, up, 0, -size), c);
        vertex(planePoint(point, right, up, size * .26F, 0), c);
        vertex(planePoint(point, right, up, 0, size), c);
    };
    glint(planePoint(center, right, up, -.017F, .023F),
          .008F + energy * .01F + flash * .023F,
          .6F + .15F * std::sin(time * 4.0F) + flash * .2F);
    if (particles && energy > .005F) {
        for (int i = 0; i < 5; ++i) {
            const float seed = static_cast<float>(i);
            const float age = std::fmod(time * (fire ? 1.8F : 1.5F) + seed * .2F, 1.0F);
            const float angle = seed * 2.39996F + time * (fire ? .6F : -.45F);
            const float radius = (.045F + (1.0F - age) * .07F) * energy;
            const Vector3 point = planePoint(center, right, up,
                std::cos(angle) * radius, std::sin(angle) * radius);
            glint(point, .006F * energy, std::sin(age * PI) * energy * .8F);
        }
    }
    if (flash > .001F) {
        for (int i = 0; i < 5; ++i) {
            const float angle = static_cast<float>(i) * 2 * PI / 5 + .3F;
            const Vector3 direction = Vector3Add(Vector3Scale(right, std::cos(angle)),
                                                 Vector3Scale(up, std::sin(angle)));
            const Vector3 side = Vector3Add(Vector3Scale(right, -std::sin(angle)),
                                            Vector3Scale(up, std::cos(angle)));
            const float radius = .035F + (1.0F - recoil) * .14F;
            const Vector3 point = Vector3Add(center, Vector3Scale(direction, radius));
            vertex(Vector3Add(point, Vector3Scale(direction, .025F * flash)), opacity(light, flash));
            vertex(Vector3Add(point, Vector3Scale(side, .007F * flash)), opacity(color, flash));
            vertex(Vector3Subtract(point, Vector3Scale(direction, .015F * flash)), opacity(color, 0));
        }
    }
    endBatch();
}

} // namespace ian::wand_presentation
