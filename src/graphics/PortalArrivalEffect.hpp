#pragma once

#include "graphics/EnemyMagicEffect.hpp"
#include "graphics/SplitterPresentation.hpp"

namespace ian::portal_arrival {
using enemy_magic_effect::planePoint;
using enemy_magic_effect::opacity;
inline constexpr double Duration=EnemyPortalArrivalDuration;

inline SplitterPose pose(float progress,bool large=false) {
    const float p=std::clamp(progress,0.0F,1.0F);
    const float reveal=fire_wand_effect::ease(p/.36F);
    const float settle=std::sin(std::clamp((p-.22F)/.78F,0.0F,1.0F)*PI*2)*(1-p);
    const float size=(large?.65F:.30F)+(large?.35F:.70F)*reveal;
    return {{size*(1-.12F*settle),size*(1+.19F*settle),size*(1-.12F*settle)},
        (large?.10F:.28F)*std::sin(p*PI),.13F*std::sin(p*PI*2)*(1-p)};
}

// Gate-plane ribbons open into discrete clouds and stars. All triangles use
// the existing unlit, depth-tested effect batch: no atlas, shader or particles
// allocated on a first spawn. Time comes from the simulation birth timer.
inline void gate(Vector3 center,Vector3 forward,Vector3 camera,float progress,bool detailed) {
    if(progress<0 || progress>=1) return;
    const auto b=enemy_magic_effect::basis(center,camera);
    const Vector3 side=fire_wand_effect::normalized(Vector3CrossProduct({0,1,0},forward),{1,0,0});
    const Vector3 up{0,1,0};
    const float spread=fire_wand_effect::ease(progress/.55F);
    const float fade=1-fire_wand_effect::ease((progress-.30F)/.70F);
    const int strands=detailed?5:3;
    for(int i=0;i<strands;++i) {
        const float angle=static_cast<float>(i)*2.39996F+progress*1.4F;
        const float orbit=(1.18F+static_cast<float>(i%3)*.10F)*(.45F+spread*.55F);
        Vector3 puff=planePoint(center,side,up,std::cos(angle)*orbit,std::sin(angle)*orbit*.85F);
        puff=Vector3Add(puff,Vector3Scale(forward,.15F+spread*.7F));
        const float size=(.13F+.16F*std::sin(progress*PI))*fade;
        for(int lobe=0;lobe<3;++lobe) {
            const float a=static_cast<float>(lobe)*2.39996F+angle;
            const auto p=planePoint(puff,b.right,b.up,std::cos(a)*size*.45F,std::sin(a)*size*.32F);
            enemy_magic_effect::disc(p,b.right,b.up,size*(lobe==1?.65F:1),
                opacity({231,179,255,220},fade),opacity({152,67,223,170},fade),14);
        }
        if(detailed) {
            // Thin curved streaks, tapering to cream/cyan spark tips.
            const auto trail=[&](float t) {
                const float a=angle-.8F*t;
                Vector3 node=planePoint(center,side,up,std::cos(a)*orbit,std::sin(a)*orbit*.85F);
                return Vector3Add(node,Vector3Scale(forward,(.15F+spread*.7F)*(1-t)));
            };
            for(int j=0;j<7;++j) {
                const float t=static_cast<float>(j)/7,next=static_cast<float>(j+1)/7;
                const auto first=trail(t),last=trail(next);
                const auto normal=fire_wand_effect::normalized(Vector3CrossProduct(Vector3Subtract(last,first),b.front),b.up);
                const float width=.055F*(1-t),end=.055F*(1-next);
                const auto a=Vector3Add(first,Vector3Scale(normal,width)),c=Vector3Subtract(first,Vector3Scale(normal,width));
                const auto d=Vector3Add(last,Vector3Scale(normal,end)),e=Vector3Subtract(last,Vector3Scale(normal,end));
                const auto bright=opacity({198,109,255,180},fade*(1-t)),dim=opacity({174,65,244,80},fade*(1-next));
                enemy_magic_effect::vertex(a,bright);enemy_magic_effect::vertex(c,bright);enemy_magic_effect::vertex(d,dim);
                enemy_magic_effect::vertex(d,dim);enemy_magic_effect::vertex(c,bright);enemy_magic_effect::vertex(e,dim);
            }
            if(i%2==0) enemy_magic_effect::star(Vector3Add(puff,Vector3Scale(b.front,.04F)),b,
                .11F*fade,angle+progress*2,opacity(i%2?Color{163,246,255,255}:Color{255,240,197,255},fade));
        }
    }
}

inline void exit(Vector3 root,Vector3 camera,float progress,float scale,bool detailed,std::uint32_t seed) {
    if(progress<0 || progress>=1) return;
    const auto b=enemy_magic_effect::basis(root,camera);
    const float fade=1-fire_wand_effect::ease((progress-.18F)/.82F);
    const float travel=fire_wand_effect::ease(progress/.65F);
    const int count=detailed?5:3;
    for(int i=0;i<count;++i) {
        const float angle=static_cast<float>(i)*2.39996F+static_cast<float>(seed%13U)*.3F;
        Vector3 p=planePoint(root,b.right,b.up,std::cos(angle)*scale*travel*.7F,
            (.25F+std::sin(angle)*travel*.3F+progress*.35F)*scale);
        enemy_magic_effect::disc(p,b.right,b.up,scale*(.16F+.08F*travel)*fade,
            opacity({212,139,255,205},fade),opacity({110,42,181,155},fade),14);
        if(detailed) enemy_magic_effect::star(Vector3Add(p,Vector3Scale(b.front,.025F)),b,
            .065F*scale*fade,angle+progress*2,opacity({185,246,255,240},fade));
    }
}
} // namespace ian::portal_arrival
