#pragma once

#include "graphics/EnemyMagicEffect.hpp"

namespace ian::royal_boss_effect {
using enemy_magic_effect::Basis;
using enemy_magic_effect::disc;
using enemy_magic_effect::planePoint;
using enemy_magic_effect::star;
using enemy_magic_effect::vertex;
using enemy_magic_effect::opacity;

inline void feather(Vector3 center,Basis b,float size,float angle,Color color) {
    const Vector3 side=Vector3Add(Vector3Scale(b.right,std::cos(angle)),Vector3Scale(b.up,std::sin(angle)));
    const Vector3 direction=Vector3Add(Vector3Scale(b.right,-std::sin(angle)),Vector3Scale(b.up,std::cos(angle)));
    const std::array<Vector2,6> shape{{{0,-1},{-.38F,-.45F},{-.42F,.25F},{0,1},{.42F,.25F},{.38F,-.45F}}};
    for(std::size_t i=0;i<shape.size();++i) {
        vertex(center,color);
        vertex(planePoint(center,side,direction,shape[i].x*size,shape[i].y*size),color);
        const auto next=shape[(i+1)%shape.size()];
        vertex(planePoint(center,side,direction,next.x*size,next.y*size),color);
    }
}
inline void marker(Vector3 center,float radius,float progress,double time,bool detailed) {
    if(radius<=0 || progress<0 || progress>=1) return;
    center.y+=.035F;
    const Basis floor{{1,0,0},{0,0,1},{0,1,0}};
    rlNormal3f(0,1,0);
    const float pulse=detailed ? .5F+.5F*static_cast<float>(std::sin(time*7)) : .5F;
    const float birth=fire_wand_effect::ease(progress/.12F);
    disc(center,floor.right,floor.up,radius,opacity({255,217,93,32},birth),opacity({255,207,68,70},birth),40);
    const float thickness=.05F+progress*.035F;
    for(int i=0;i<40;++i) {
        const float a=static_cast<float>(i)*2*PI/40,b=static_cast<float>(i+1)*2*PI/40;
        const auto point=[&](float angle,float r){return planePoint(center,floor.right,floor.up,std::cos(angle)*r,std::sin(angle)*r);};
        const Color color=opacity({255,static_cast<unsigned char>(184+pulse*42),68,235},birth);
        vertex(point(a,radius),color);vertex(point(b,radius),color);vertex(point(a,radius-thickness),color);
        vertex(point(a,radius-thickness),color);vertex(point(b,radius),color);vertex(point(b,radius-thickness),color);
    }
    for(int i=0;i<8;++i) {
        const float angle=static_cast<float>(i)*PI/4;
        const float travel=.20F*static_cast<float>(std::fmod(time*1.6+i*.13,1.0));
        feather(planePoint(center,floor.right,floor.up,std::cos(angle)*(radius+.22F-travel),
            std::sin(angle)*(radius+.22F-travel)),floor,.19F,angle-PI/2,opacity({255,244,167,230},birth));
    }
    star(Vector3Add(center,{0,.012F,0}),floor,.16F+progress*.10F,
         static_cast<float>(time*.7),opacity({255,246,181,220},birth));
}
inline void impact(Vector3 center,Vector3 camera,float progress,float scale,bool detailed) {
    if(progress<0 || progress>=1 || scale<=0) return;
    const Basis b=enemy_magic_effect::basis(center,camera);
    center.y+=.30F*scale;
    const float ease=1-std::pow(1-progress,3.0F),fade=1-fire_wand_effect::ease((progress-.42F)/.58F);
    for(int i=0;i<8;++i) {
        const float angle=static_cast<float>(i)*2.39996F;
        const Vector3 puff=planePoint(center,b.right,b.up,std::cos(angle)*ease*scale*.65F,
            std::sin(angle)*ease*scale*.35F+progress*scale*.2F);
        disc(puff,b.right,b.up,scale*(.35F-progress*.18F),opacity({255,242,164,255},fade),
             opacity(i%2 ? Color{177,204,75,255} : Color{245,183,50,255},fade),16);
    }
    const Vector3 front=Vector3Add(center,Vector3Scale(b.front,.04F));
    star(front,b,scale*(.5F-progress*.35F),progress*.2F,opacity({255,255,215,255},fade));
    if(detailed) for(int i=0;i<8;++i) {
        const float angle=static_cast<float>(i)*PI/4;
        const Vector3 position=planePoint(front,b.right,b.up,std::cos(angle)*ease*scale*1.1F,
            std::sin(angle)*ease*scale*.7F+std::sin(progress*PI)*scale*.5F);
        feather(position,b,.16F*scale*(1-progress*.55F),angle+progress*2,opacity({255,250,206,255},fade));
    }
}
inline void aura(Vector3 center,Vector3 camera,double time,int phase,float charge,bool shield,bool detailed) {
    const Basis b=enemy_magic_effect::basis(center,camera);
    const int count=detailed ? 8 : 4;
    const float radius=shield ? 2.05F : .85F+charge*.25F;
    for(int i=0;i<count;++i) {
        const float angle=static_cast<float>(time*(shield?.65:1.25))+static_cast<float>(i)*2*PI/static_cast<float>(count);
        Vector3 p=planePoint(center,b.right,b.up,std::cos(angle)*radius,std::sin(angle)*radius*.75F);
        p=Vector3Add(p,Vector3Scale(b.front,std::sin(angle)*.12F));
        const float size=shield ? .23F : .075F+charge*.08F;
        if(shield) feather(p,b,size,angle,{255,240,132,190});
        else star(p,b,size,angle,{255,235,137,static_cast<unsigned char>(100+charge*130)});
    }
    if(phase>=3 && detailed) for(int i=0;i<3;++i) {
        const float a=static_cast<float>(time*2)+static_cast<float>(i)*2*PI/3;
        feather(planePoint(center,b.right,b.up,std::cos(a)*1.3F,std::sin(a)*.8F),b,.13F,a,{201,245,136,160});
    }
}
} // namespace ian::royal_boss_effect
