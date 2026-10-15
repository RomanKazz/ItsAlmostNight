#pragma once
#include "graphics/ChainLightningEffect.hpp"
namespace ian::laser_beam_effect {
inline constexpr int MaximumVisibleBeams=96;
// The same target point gets the same offset on both sides of a bounce.
// Segment-relative offsets would leave gaps at changes of direction.
inline Vector3 parallelPoint(Vector3 point, Vector3 camera, float lane, float spread) {
    const auto view = fire_wand_effect::normalized(Vector3Subtract(camera, point), {0, 0, 1});
    const auto right = fire_wand_effect::normalized(Vector3CrossProduct(view, {0, 1, 0}), {1, 0, 0});
    return Vector3Add(point, Vector3Scale(right, lane * spread));
}
// Fixed-cost ribbons and a few triangle sparks; no particles, meshes or allocations.
inline void emit(Vector3 from,Vector3 to,Vector3 camera,float focus,float seconds,float visibility=1) {
    using namespace fire_wand_effect;
    focus=std::clamp(focus,0.0F,1.0F);visibility=std::clamp(visibility,0.0F,1.0F);
    const auto delta=Vector3Subtract(to,from);const float length=Vector3Length(delta);
    if(length<.01F || visibility<=0) return;
    const auto direction=Vector3Scale(delta,1/length);
    const auto view=normalized(Vector3Subtract(camera,to),{0,0,1});
    const auto right=normalized(Vector3CrossProduct(view,{0,1,0}),{1,0,0});
    const auto up=Vector3CrossProduct(right,view);
    const float pulse=.93F+.07F*std::sin(seconds*31);
    const float width=(.020F+.025F*focus)*pulse*visibility;
    const Color glow{static_cast<unsigned char>(70+150*focus),static_cast<unsigned char>(190+45*focus),255,120};
    chain_lightning_effect::softRibbon(from,to,camera,.11F+.09F*focus,opacity(glow,visibility));
    chain_lightning_effect::ribbon(from,to,camera,width*1.7F,opacity({70,221,255,210},visibility));
    chain_lightning_effect::ribbon(from,to,camera,width,opacity({235,255,255,255},visibility));
    for(int i=0;i<3;++i) {
        const float t=std::fmod(seconds*1.7F+static_cast<float>(i)/3,1.0F);
        const auto center=Vector3Add(from,Vector3Scale(direction,length*t));
        chain_lightning_effect::star(center,right,up,(.035F+.03F*focus)*visibility,
            opacity({180,248,255,170},visibility),seconds*.7F);
    }
    disc(to,right,up,(.18F+.13F*focus)*visibility,opacity(glow,visibility),{glow.r,glow.g,glow.b,0},10);
    chain_lightning_effect::star(to,right,up,(.14F+.12F*focus)*pulse*visibility,
        opacity({235,255,255,250},visibility),seconds*2.3F);
    chain_lightning_effect::star(from,right,up,(.075F+.04F*focus)*visibility,
        opacity({170,245,255,230},visibility),-seconds*1.8F);
}
}
