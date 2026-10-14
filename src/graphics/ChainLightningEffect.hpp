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
// Adjacent segments share the exact same edge vertices, including the soft halo.
inline void connectedRibbon(const Vector3* points,const Vector3* offsets,int count,float scale,Color color,bool soft) {
    const auto vertex=fire_wand_effect::vertex;
    const Color edge{color.r,color.g,color.b,0};
    for(int i=1;i<count;++i) {
        const auto from=points[i-1],to=points[i];
        const auto a=Vector3Scale(offsets[i-1],scale),b=Vector3Scale(offsets[i],scale);
        if(soft) {
            for(float sign:{-1.0F,1.0F}) {
                const auto outerA=Vector3Add(from,Vector3Scale(a,sign));
                const auto outerB=Vector3Add(to,Vector3Scale(b,sign));
                vertex(from,color);vertex(outerA,edge);vertex(outerB,edge);
                vertex(from,color);vertex(outerB,edge);vertex(to,color);
            }
        } else {
            vertex(Vector3Subtract(from,a),color);vertex(Vector3Add(from,a),color);vertex(Vector3Add(to,b),color);
            vertex(Vector3Subtract(from,a),color);vertex(Vector3Add(to,b),color);vertex(Vector3Subtract(to,b),color);
        }
    }
}
inline Vector3 joinedSide(const Vector3* points,int count,int index,Vector3 camera) {
    if(index==0) return ribbonSide(points[0],points[1],camera,1);
    if(index==count-1) return ribbonSide(points[index-1],points[index],camera,1);
    const auto before=ribbonSide(points[index-1],points[index],camera,1);
    const auto after=ribbonSide(points[index],points[index+1],camera,1);
    const auto miter=fire_wand_effect::normalized(Vector3Add(before,after),after);
    return Vector3Scale(miter,1/std::max(.5F,Vector3DotProduct(miter,after)));
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
    }
    std::array<Vector3,MaximumSegments+1> offsets{};
    for(int i=0;i<=segments;++i) {
        const float t=static_cast<float>(i)/static_cast<float>(segments);
        offsets[static_cast<std::size_t>(i)]=Vector3Scale(joinedSide(points.data(),segments+1,i,camera),
            size*(.6F+.4F*std::sin(t*PI))*flicker);
    }
    // Draw each whole layer in order so a later segment's halo cannot cover its core.
    connectedRibbon(points.data(),offsets.data(),segments+1,.20F,fire_wand_effect::opacity({120,190,255,155},pulse),true);
    connectedRibbon(points.data(),offsets.data(),segments+1,.055F,fire_wand_effect::opacity({105,235,255,250},pulse),false);
    connectedRibbon(points.data(),offsets.data(),segments+1,.018F,fire_wand_effect::opacity({235,255,255,255},pulse),false);
    // Short tapered forks grow and dissolve, rather than duplicating the main beam.
    for(int branch=0;branch<3;++branch) {
        const auto sample=seed+static_cast<std::uint64_t>(branch*113+41);
        const int index=2+static_cast<int>(unitRandom(sample)*static_cast<double>(segments-3));
        std::array<Vector3,4> forkPoints{};forkPoints[0]=points[static_cast<std::size_t>(index)];
        const float sign=branch%2==0 ? 1.0F : -1.0F;
        const float forkLength=std::min(.9F,length*.23F)*(.55F+static_cast<float>(unitRandom(sample+1))*.45F);
        const Vector3 fork=Vector3Add(Vector3Scale(side,sign*.86F),Vector3Add(Vector3Scale(direction,.38F),Vector3Scale(lift,.30F)));
        for(int step=1;step<=3;++step) {
            const float t=static_cast<float>(step)/3;
            const Vector3 next=Vector3Add(points[static_cast<std::size_t>(index)],Vector3Add(Vector3Scale(fork,forkLength*t),
                Vector3Scale(lift,static_cast<float>(unitRandom(sample+static_cast<std::uint64_t>(step)+frame)*2-1)*.10F)));
            forkPoints[static_cast<std::size_t>(step)]=next;
        }
        std::array<Vector3,4> forkOffsets{};
        for(int step=0;step<4;++step) {
            const float t=static_cast<float>(step)/3;
            forkOffsets[static_cast<std::size_t>(step)]=Vector3Scale(joinedSide(forkPoints.data(),4,step,camera),size*(1-t*.8F));
        }
        connectedRibbon(forkPoints.data(),forkOffsets.data(),4,.075F,fire_wand_effect::opacity({130,215,255,145},pulse),true);
        connectedRibbon(forkPoints.data(),forkOffsets.data(),4,.022F,fire_wand_effect::opacity({215,252,255,250},pulse),false);
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
