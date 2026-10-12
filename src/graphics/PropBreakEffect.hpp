#pragma once

#include "graphics/EnemyMagicEffect.hpp"

namespace ian::prop_break_effect {
using enemy_magic_effect::planePoint;
using enemy_magic_effect::opacity;
using enemy_magic_effect::vertex;

inline float random(std::uint32_t seed, std::uint32_t channel) {
    seed ^= channel * 0x9e3779b9U;
    seed ^= seed >> 16U; seed *= 0x7feb352dU; seed ^= seed >> 15U;
    return static_cast<float>(seed & 65535U) / 65535.0F;
}

inline void plank(Vector3 center, enemy_magic_effect::Basis b,
                  float width, float height, float angle, Color color) {
    const Vector3 right=Vector3Add(Vector3Scale(b.right,std::cos(angle)),Vector3Scale(b.up,std::sin(angle)));
    const Vector3 up=Vector3Add(Vector3Scale(b.up,std::cos(angle)),Vector3Scale(b.right,-std::sin(angle)));
    const std::array<Vector2,6> corners{{{-width*.5F,-height*.40F},
        {-width*.30F,-height*.5F},{width*.5F,-height*.5F},
        {width*.5F,height*.40F},{width*.30F,height*.5F},{-width*.5F,height*.5F}}};
    for(std::size_t i=0;i<corners.size();++i) {
        const auto next=corners[(i+1)%corners.size()];
        vertex(center,color);
        vertex(planePoint(center,right,up,corners[i].x,corners[i].y),color);
        vertex(planePoint(center,right,up,next.x,next.y),color);
    }
}

// One depth-tested triangle batch. A soft cream burst, tumbling staves and
// short gold sparkles; no explosion ring, atlas squares or first-use assets.
inline void burst(Vector3 center, Vector3 camera, float progress, float scale,
                  std::uint32_t seed, bool detailed, bool barrel) {
    if(progress<=0 || progress>=1 || scale<=0) return;
    const auto b=enemy_magic_effect::basis(center,camera);
    rlNormal3f(b.front.x,b.front.y,b.front.z);
    const float spread=fire_wand_effect::ease(progress/.55F);
    const float fade=1-fire_wand_effect::ease((progress-.22F)/.78F);
    const int puffs=detailed?7:4;
    for(int i=0;i<puffs;++i) {
        const float angle=static_cast<float>(i)*2.39996F+random(seed,static_cast<std::uint32_t>(i+1));
        const float radius=(.19F+.14F*random(seed,static_cast<std::uint32_t>(i+11)))*scale*
            (.45F+.7F*std::sin(progress*PI))*(1-fire_wand_effect::ease((progress-.7F)/.3F));
        const Vector3 puff=planePoint(center,b.right,b.up,
            std::cos(angle)*spread*.8F*scale,std::sin(angle)*spread*.42F*scale+progress*.26F*scale);
        for(int lobe=0;lobe<3;++lobe) {
            const float a=angle+static_cast<float>(lobe)*2.39996F;
            const auto p=planePoint(puff,b.right,b.up,std::cos(a)*radius*.42F,std::sin(a)*radius*.38F);
            enemy_magic_effect::disc(p,b.right,b.up,radius,
                opacity({245,222,177,235},fade),opacity({176,130,81,220},fade),16);
            enemy_magic_effect::disc(planePoint(Vector3Add(p,Vector3Scale(b.front,.006F)),b.right,b.up,
                -radius*.08F,radius*.10F),b.right,b.up,radius*.82F,
                opacity({255,247,220,235},fade),opacity({241,218,175,225},fade),16);
        }
    }
    const float seconds=progress*.72F;
    const float chipFade=1-fire_wand_effect::ease((progress-.65F)/.35F);
    const int chips=detailed?8:4;
    for(int i=0;i<chips;++i) {
        const auto channel=static_cast<std::uint32_t>(i+31);
        const float angle=static_cast<float>(i)*2.39996F+random(seed,channel);
        const float speed=1.4F+random(seed,channel+20)*1.4F;
        const Vector3 p=planePoint(Vector3Add(center,Vector3Scale(b.front,.025F)),b.right,b.up,
            std::cos(angle)*speed*seconds*scale,
            (std::sin(angle)*speed*seconds+1.8F*seconds-3.4F*seconds*seconds)*scale);
        const float tumble=angle+seconds*(5+random(seed,channel+40)*9);
        const float width=(.10F+.07F*random(seed,channel+60))*scale;
        const float height=(.30F+.20F*random(seed,channel+80))*scale;
        plank(p,b,width,height,tumble,opacity({102,55,28,255},chipFade));
        plank(Vector3Add(p,Vector3Scale(b.front,.004F)),b,width*.76F,height*.87F,tumble,
            opacity(barrel?Color{196,126,59,255}:Color{239,158,76,255},chipFade));
        if(barrel && i%2==0) plank(Vector3Add(p,Vector3Scale(b.front,.008F)),b,
            width*.95F,height*.13F,tumble,opacity({163,193,202,255},chipFade));
    }
    const float sparkle=(1-fire_wand_effect::ease(progress/.5F))*std::sin(std::min(1.0F,progress/.2F)*PI*.5F);
    for(int i=0;i<(detailed?5:3);++i) {
        const float angle=static_cast<float>(i)*2.39996F;
        const auto p=planePoint(Vector3Add(center,Vector3Scale(b.front,.06F)),b.right,b.up,
            std::cos(angle)*spread*1.0F*scale,std::sin(angle)*spread*.75F*scale);
        enemy_magic_effect::star(p,b,(.08F+.08F*random(seed,static_cast<std::uint32_t>(i+121)))*scale*sparkle,
            angle,opacity({255,220,110,255},sparkle));
    }
}
} // namespace ian::prop_break_effect
