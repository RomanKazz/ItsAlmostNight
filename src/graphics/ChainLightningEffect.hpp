#pragma once
#include "graphics/FireWandEffect.hpp"
#include "core/DeterministicRandom.hpp"
namespace ian::chain_lightning_effect {
inline constexpr int MaximumVisibleArcs=96;
inline constexpr int MaximumSegments=18;
inline Vector3 point(Vec3 p) {return {static_cast<float>(p.x),static_cast<float>(p.y),static_cast<float>(p.z)};}
inline Vector3 ribbonSide(Vector3 from,Vector3 to,Vector3 camera,float width) {
    const Vector3 center=Vector3Scale(Vector3Add(from,to),.5F);
    const Vector3 side=Vector3CrossProduct(Vector3Subtract(to,from),Vector3Subtract(camera,center));
    return Vector3LengthSqr(side)>1e-8F ? Vector3Scale(Vector3Normalize(side),width) : Vector3{width,0,0};
}
inline void ribbon(Vector3 from,Vector3 to,Vector3 camera,float width,Color color,float taper=1) {
    const auto a=ribbonSide(from,to,camera,width), b=Vector3Scale(a,taper);
    const auto vertex=fire_wand_effect::vertex;
    vertex(Vector3Subtract(from,a),color);vertex(Vector3Add(from,a),color);vertex(Vector3Add(to,b),color);
    vertex(Vector3Subtract(from,a),color);vertex(Vector3Add(to,b),color);vertex(Vector3Subtract(to,b),color);
}
inline void softRibbon(Vector3 from,Vector3 to,Vector3 camera,float width,Color color) {
    const auto side=ribbonSide(from,to,camera,width);
    const Color edge{color.r,color.g,color.b,0};const auto vertex=fire_wand_effect::vertex;
    for(float sign:{-1.0F,1.0F}) {
        const Vector3 a=Vector3Add(from,Vector3Scale(side,sign)), b=Vector3Add(to,Vector3Scale(side,sign));
        vertex(from,color);vertex(a,edge);vertex(b,edge);
        vertex(from,color);vertex(b,edge);vertex(to,color);
    }
}
inline void star(Vector3 center,Vector3 right,Vector3 up,float radius,Color color,float twist) {
    const auto vertex=fire_wand_effect::vertex;
    for(int spike=0;spike<4;++spike) {
        const float angle=twist+static_cast<float>(spike)*PI*.5F;
        const Vector3 axis=Vector3Add(Vector3Scale(right,std::cos(angle)),Vector3Scale(up,std::sin(angle)));
        const Vector3 side=Vector3Add(Vector3Scale(right,-std::sin(angle)),Vector3Scale(up,std::cos(angle)));
        vertex(Vector3Add(center,Vector3Scale(side,radius*.16F)),color);
        vertex(Vector3Add(center,Vector3Scale(axis,radius)),color);
        vertex(Vector3Subtract(center,Vector3Scale(side,radius*.16F)),color);
    }
}
// One bounded triangle batch for every Nail/Tesla arc. No particle objects or cylinders.
inline void emit(Vector3 start,Vector3 end,Vector3 camera,float seconds,float duration,
    std::uint64_t seed,float size=1.0F,float phaseTime=-1.0F) {
    const float length=Vector3Distance(start,end);
    if(length<.01F || duration<=0) return;
    const float progress=std::clamp(seconds/duration,0.0F,1.0F);
    const float pulse=fire_wand_effect::ease(progress/.055F)*(1-fire_wand_effect::ease((progress-.38F)/.62F));
    if(pulse<=0) return;
    const float clock=phaseTime>=0 ? phaseTime : seconds;
    const auto frame=static_cast<std::uint64_t>(std::max(0.0F,std::floor(clock*26)));
    const float flicker=.84F+.16F*static_cast<float>(unitRandom(seed^frame));
    const Vector3 direction=Vector3Scale(Vector3Subtract(end,start),1/length);
    const Vector3 side=fire_wand_effect::normalized(Vector3CrossProduct(direction,{0,1,0}),{1,0,0});
    const Vector3 lift=Vector3CrossProduct(side,direction);
    const int segments=std::clamp(static_cast<int>(std::ceil(length*2.4F)),7,MaximumSegments);
    const float amplitude=std::min(.48F,length*.10F);
    std::array<Vector3,MaximumSegments+1> points{};points[0]=start;
    for(int i=1;i<=segments;++i) {
        float t=static_cast<float>(i)/static_cast<float>(segments);
        if(i<segments) t+=static_cast<float>(unitRandom(seed+static_cast<std::uint64_t>(i*31))-.5)*.65F/static_cast<float>(segments);
        Vector3 next=Vector3Lerp(start,end,t);
        if(i<segments) {
            const auto sample=seed^(frame*0x9e3779b97f4a7c15ULL)^static_cast<std::uint64_t>(i*73);
            const float envelope=std::sin(t*PI)*amplitude;
            next=Vector3Add(next,Vector3Add(Vector3Scale(side,static_cast<float>(unitRandom(sample)*2-1)*envelope),
                Vector3Scale(lift,static_cast<float>(unitRandom(sample^0x57bcULL)*2-1)*envelope*.7F)));
        }
        points[static_cast<std::size_t>(i)]=next;
        const float wave=.6F+.4F*std::sin(t*PI);
        const float width=size*wave*flicker;
        // The wide halo fades sideways; there are no three solid neon bands.
        softRibbon(points[static_cast<std::size_t>(i-1)],next,camera,.10F*width,
            fire_wand_effect::opacity({103,125,255,85},pulse));
        ribbon(points[static_cast<std::size_t>(i-1)],next,camera,.017F*width,
            fire_wand_effect::opacity({70,221,255,220},pulse));
        ribbon(points[static_cast<std::size_t>(i-1)],next,camera,.0045F*width,
            fire_wand_effect::opacity({235,255,255,255},pulse));
    }
    // Short tapered forks grow and dissolve, rather than duplicating the main beam.
    for(int branch=0;branch<3;++branch) {
        const auto sample=seed+static_cast<std::uint64_t>(branch*113+41);
        const int index=2+static_cast<int>(unitRandom(sample)*static_cast<double>(segments-3));
        Vector3 previous=points[static_cast<std::size_t>(index)];
        const float sign=branch%2==0 ? 1.0F : -1.0F;
        const float forkLength=std::min(.9F,length*.23F)*(.55F+static_cast<float>(unitRandom(sample+1))*.45F);
        const Vector3 fork=Vector3Add(Vector3Scale(side,sign*.86F),Vector3Add(Vector3Scale(direction,.38F),Vector3Scale(lift,.30F)));
        for(int step=1;step<=3;++step) {
            const float t=static_cast<float>(step)/3;
            const Vector3 next=Vector3Add(points[static_cast<std::size_t>(index)],Vector3Add(Vector3Scale(fork,forkLength*t),
                Vector3Scale(lift,static_cast<float>(unitRandom(sample+static_cast<std::uint64_t>(step)+frame)*2-1)*.10F)));
            softRibbon(previous,next,camera,.03F*size*(1-t*.6F),fire_wand_effect::opacity({104,187,255,80},pulse*(1-t*.6F)));
            ribbon(previous,next,camera,.007F*size*(1-t*.7F),fire_wand_effect::opacity({179,243,255,210},pulse*(1-t*.8F)),.6F);
            previous=next;
        }
    }
    const Vector3 view=fire_wand_effect::normalized(Vector3Subtract(camera,end),{0,0,1});
    const Vector3 right=fire_wand_effect::normalized(Vector3CrossProduct(view,{0,1,0}),{1,0,0});
    const Vector3 up=Vector3CrossProduct(right,view);
    const float flash=pulse*(1-fire_wand_effect::ease(progress/.75F));
    const float twist=static_cast<float>(unitRandom(seed))*PI*.5F;
    fire_wand_effect::disc(end,right,up,.32F*size,fire_wand_effect::opacity({94,182,255,80},pulse),{94,182,255,0},8);
    star(end,right,up,(.12F+.25F*flash)*size,fire_wand_effect::opacity({95,192,255,150},flash),twist);
    star(Vector3Add(end,Vector3Scale(view,.006F)),right,up,(.08F+.16F*flash)*size,
        fire_wand_effect::opacity({230,253,255,245},flash),twist);
    star(start,right,up,(.09F+.08F*flash)*size,fire_wand_effect::opacity({193,250,255,210},pulse),twist+.4F);
    // Four tiny diamond embers fly from the hit; fixed arithmetic, no runtime particles.
    for(int spark=0;spark<4;++spark) {
        const float angle=twist+static_cast<float>(spark)*PI*.5F+.3F;
        const float travel=.10F+progress*.65F;
        const Vector3 center=Vector3Add(end,Vector3Add(Vector3Scale(right,std::cos(angle)*travel),Vector3Scale(up,std::sin(angle)*travel)));
        star(center,right,up,.025F*size*(1-progress),fire_wand_effect::opacity({149,235,255,230},pulse*(1-progress)),angle);
    }
}
}
