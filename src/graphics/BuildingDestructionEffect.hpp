#pragma once

#include "graphics/PropBreakEffect.hpp"

namespace ian::building_destruction_effect {

inline float scaleAt(float progress) {
    const float p = std::clamp(progress, 0.0F, 1.0F);
    return (1 - fire_wand_effect::ease((p - .12F) / .88F)) *
        (1 + .055F * std::sin(std::min(1.0F, p / .25F) * PI));
}
inline float squashAt(float progress) {
    return 1 - .22F * std::sin(std::clamp(progress / .25F, 0.0F, 1.0F) * PI);
}

// A single shared depth-tested triangle batch, at most 768 vertices per burst.
// The pieces are visual only: no rigid bodies, lights or per-frame allocations.
inline void burst(Vector3 origin, Vector3 camera, float progress, float scale,
                  std::uint32_t seed, bool detailed, int material) {
    if (!std::isfinite(progress) || progress <= 0 || progress >= 1 || scale <= 0) return;
    scale = std::clamp(scale, .65F, 1.65F);
    const auto b = enemy_magic_effect::basis(origin,camera);
    rlNormal3f(b.front.x,b.front.y,b.front.z);
    const float spread = 1-(1-progress)*(1-progress);
    const float fade = 1-fire_wand_effect::ease((progress-.25F)/.75F);
    for (int i=0;i<(detailed?8:4);++i) {
        const auto channel=static_cast<std::uint32_t>(i+1);
        const float angle=static_cast<float>(i)*2.39996F+prop_break_effect::random(seed,channel);
        const float distance=(.25F+spread*1.30F)*scale;
        const float size=(.16F+.20F*std::sin(PI*std::sqrt(progress)))*scale;
        const Vector3 p{origin.x+std::cos(angle)*distance,
            origin.y+(.12F+progress*.26F)*scale,origin.z+std::sin(angle)*distance};
        enemy_magic_effect::disc(p,b.right,b.up,size,
            enemy_magic_effect::opacity({220,195,151,215},fade),
            enemy_magic_effect::opacity({165,134,95,100},fade),10);
        enemy_magic_effect::disc(fire_wand_effect::planePoint(
            Vector3Add(p,Vector3Scale(b.front,.006F)),b.right,b.up,-size*.14F,size*.20F),
            b.right,b.up,size*.72F,enemy_magic_effect::opacity({255,235,192,210},fade),
            enemy_magic_effect::opacity({230,204,158,120},fade),10);
    }
    const float seconds=progress*.90F;
    const float chipFade=1-fire_wand_effect::ease((progress-.60F)/.40F);
    for (int i=0;i<(detailed?8:4);++i) {
        const auto channel=static_cast<std::uint32_t>(i+41);
        const float angle=static_cast<float>(i)*2.39996F+prop_break_effect::random(seed,channel);
        const float speed=1.5F+prop_break_effect::random(seed,channel+20)*1.5F;
        const float distance=(.12F+speed*seconds)*scale;
        // One short hop, then shrink at the surface. No sinking fragments.
        const Vector3 p{origin.x+std::cos(angle)*distance,
            origin.y+(.12F+std::max(0.0F,2.9F*seconds-4.6F*seconds*seconds))*scale,
            origin.z+std::sin(angle)*distance};
        const bool metal=(material==2 || material==3) && i%3!=0;
        const bool crystal=material==3 && i%3==0;
        const Color dark=crystal?Color{96,38,137,255}:metal?Color{47,69,85,255}:Color{110,57,25,255};
        const Color light=crystal?Color{214,99,246,255}:metal?(material==3?Color{255,210,58,255}:Color{174,203,214,255}):Color{243,149,65,255};
        const float shrink=1-fire_wand_effect::ease((progress-.66F)/.34F);
        const float width=(metal?.16F:.12F)*scale*shrink;
        const float height=(metal?.24F:.42F)*scale*shrink;
        const float tumble=angle+seconds*(5+prop_break_effect::random(seed,channel+60)*8);
        prop_break_effect::plank(p,b,width,height,tumble,enemy_magic_effect::opacity(dark,chipFade));
        prop_break_effect::plank(Vector3Add(p,Vector3Scale(b.front,.008F)),b,
            width*.68F,height*.82F,tumble,enemy_magic_effect::opacity(light,chipFade));
    }
}
} // namespace ian::building_destruction_effect
