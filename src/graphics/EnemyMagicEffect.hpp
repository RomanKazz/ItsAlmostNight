#pragma once

#include "enemies/EnemySystem.hpp"
#include "graphics/FireWandEffect.hpp"

namespace ian::enemy_magic_effect {

// Camera-facing opaque layers, without world ink or lighting. Reuse the
// flame renderer's allocation-free triangle primitives, not its fire palette.
using fire_wand_effect::disc;
using fire_wand_effect::planePoint;
using fire_wand_effect::vertex;
using fire_wand_effect::opacity;

struct Basis { Vector3 right, up, front; };
inline Basis basis(Vector3 center, Vector3 camera) {
    const Vector3 front = fire_wand_effect::normalized(
        Vector3Subtract(camera, center), {0,0,1});
    const Vector3 right = fire_wand_effect::normalized(
        Vector3CrossProduct({0,1,0}, front), {1,0,0});
    return {right, Vector3CrossProduct(front, right), front};
}
inline Vector3 point(Vec3 p) {
    return {static_cast<float>(p.x),static_cast<float>(p.y),static_cast<float>(p.z)};
}
inline void star(Vector3 center, Basis b, float size, float angle, Color color) {
    for (int i=0;i<8;++i) {
        const float a=angle+static_cast<float>(i)*PI/4;
        const float next=a+PI/4;
        const float first=i%2==0?size:size*.25F;
        const float second=i%2==0?size*.25F:size;
        vertex(center,color);
        vertex(planePoint(center,b.right,b.up,std::cos(a)*first,std::sin(a)*first),color);
        vertex(planePoint(center,b.right,b.up,std::cos(next)*second,std::sin(next)*second),color);
    }
}
inline void orb(Vector3 center, Basis b, float radius, double time, float fade=1, bool royal=false) {
    rlNormal3f(b.front.x,b.front.y,b.front.z);
    radius*=1+.06F*static_cast<float>(std::sin(time*10));
    disc(center,b.right,b.up,radius,opacity(royal ? Color{187,208,68,255} : Color{135,55,215,255},fade),
         opacity(royal ? Color{98,119,33,255} : Color{65,24,119,255},fade));
    Vector3 inner=Vector3Add(center,Vector3Scale(b.front,radius*.05F));
    inner=planePoint(inner,b.right,b.up,-radius*.1F,radius*.12F);
    disc(inner,b.right,b.up,radius*.81F,opacity(royal ? Color{255,246,164,255} : Color{238,178,255,255},fade),
         opacity(royal ? Color{255,191,38,255} : Color{178,95,250,255},fade));
    inner=Vector3Add(inner,Vector3Scale(b.front,radius*.05F));
    disc(inner,b.right,b.up,radius*.43F,opacity({255,250,218,255},fade),
         opacity(royal ? Color{255,233,142,255} : Color{250,222,255,255},fade));
    star(planePoint(Vector3Add(inner,Vector3Scale(b.front,radius*.02F)),b.right,b.up,
        -radius*.28F,radius*.26F),b,radius*.28F,0,opacity(WHITE,fade));
}
inline Vector3 projectileHead(const EnemyProjectile& shot,float interpolation) {
    return Vector3Lerp(point(shot.previousPosition),point(shot.position),
                      std::clamp(interpolation,0.0F,1.0F));
}
inline void projectile(const EnemyProjectile& shot,Vector3 camera,double time,
                       float interpolation,bool headPass,bool detailed) {
    if (!shot.active || shot.radius<=0) return;
    const Vector3 head=projectileHead(shot,interpolation);
    const Basis b=basis(head,camera);
    const double clock=time+static_cast<double>(shot.id.index%97U)*.13;
    const float radius=static_cast<float>(shot.radius);
    if(headPass) { orb(head,b,radius,clock,1,shot.royal); return; }
    const float age=static_cast<float>(std::max(0.0,shot.duration-shot.lifetimeRemaining));
    const Vector3 backward=Vector3Negate(fire_wand_effect::normalized(point(shot.velocity),{0,0,1}));
    // Distance behind the head is bounded by actual age: no instant full wake.
    const float length=std::min(.95F,age*static_cast<float>(Vector3Length(point(shot.velocity))));
    const int count=detailed?7:4;
    for(int i=count;i>0;--i) {
        const float p=static_cast<float>(i)/static_cast<float>(count);
        Vector3 tail=Vector3Add(head,Vector3Scale(backward,length*p));
        tail=Vector3Add(tail,Vector3Scale(b.up,std::sin(static_cast<float>(clock*14)-p*7)*radius*.28F*p));
        const float size=radius*(1-p*.85F);
        disc(tail,b.right,b.up,size,opacity(shot.royal ? Color{237,218,84,190} : Color{193,97,250,190},1-p*.75F),
             opacity(shot.royal ? Color{164,205,51,120} : Color{105,44,197,120},1-p));
        if(detailed && i%2==0) star(planePoint(tail,b.right,b.up,
            std::sin(static_cast<float>(clock*8)+p*9)*radius*1.4F,radius*.8F),b,
            .035F*(1-p*.5F),static_cast<float>(clock*2),{255,218,135,220});
    }
}
inline void burst(Vector3 center,Vector3 camera,float progress,float scale,
                  bool impact,bool detailed,std::uint32_t seed,bool royal=false) {
    if(progress<0 || progress>=1 || scale<=0) return;
    const Basis b=basis(center,camera);
    const float ease=1-std::pow(1-progress,3.0F);
    const float fade=1-fire_wand_effect::ease((progress-.45F)/.55F);
    const float radius=(impact?.27F:.17F)*scale*(.55F+ease);
    rlNormal3f(b.front.x,b.front.y,b.front.z);
    const int count=impact?6:4;
    for(int i=0;i<count;++i) {
        const float a=static_cast<float>(i)*2*PI/static_cast<float>(count)+static_cast<float>(seed%11U)*.31F;
        const Vector3 puff=planePoint(center,b.right,b.up,
            std::cos(a)*radius*.75F*ease,std::sin(a)*radius*.75F*ease+progress*.12F*scale);
        disc(puff,b.right,b.up,radius*(.7F-progress*.38F),
            opacity(royal ? Color{255,236,159,255} : Color{226,176,255,255},fade),opacity(royal ? Color{159,192,66,255} : Color{125,61,190,255},fade));
    }
    const Vector3 front=Vector3Add(center,Vector3Scale(b.front,.025F));
    orb(front,b,radius*(1-progress)*.8F,progress*2,fade,royal);
    if(detailed) for(int i=0;i<5;++i) {
        const float a=static_cast<float>(i)*2.39996F+static_cast<float>(seed%7U);
        const float travel=(impact?.8F:.4F)*scale*ease;
        star(planePoint(Vector3Add(center,Vector3Scale(b.front,.04F)),b.right,b.up,
            std::cos(a)*travel,std::sin(a)*travel),b,.075F*scale*(1-progress),a+progress*2,
            opacity({255,231,160,255},fade));
    }
}
} // namespace ian::enemy_magic_effect
