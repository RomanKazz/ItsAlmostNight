#pragma once

#include "game/StumpRoots.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ian::stump_root_effect {
inline float random(unsigned int n) {
    n ^= n >> 16; n *= 0x7feb352dU; n ^= n >> 15; n *= 0x846ca68bU; n ^= n >> 16;
    return static_cast<float>(n & 65535U)/65535.0F;
}
inline float ease(float x) { x=std::clamp(x,0.0F,1.0F); return x*x*(3-2*x); }
inline void vertex(Vector3 p,Vector3 normal,Color color) {
    rlColor4ub(color.r,color.g,color.b,color.a); rlNormal3f(normal.x,normal.y,normal.z);
    rlTexCoord2f(0,0); rlVertex3f(p.x,p.y,p.z);
}
inline void triangle(Vector3 a,Vector3 b,Vector3 c,Color color) {
    const Vector3 normal=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b,a),Vector3Subtract(c,a)));
    vertex(a,normal,color);vertex(b,normal,color);vertex(c,normal,color);
}
inline void branch(Vector3 a,Vector3 b,float r0,float r1,Color color,unsigned int seed) {
    const auto delta=Vector3Subtract(b,a);
    if (Vector3LengthSqr(delta)<1e-8F || r0<.0005F) return;
    const auto axis=Vector3Normalize(delta);
    const auto tangent=Vector3Normalize(Vector3CrossProduct(axis,std::abs(axis.y)>.95F?Vector3{1,0,0}:Vector3{0,1,0}));
    const auto bitangent=Vector3CrossProduct(axis,tangent);
    constexpr int sides=7;
    const float phase=random(seed)*2*PI;
    for (int i=0;i<sides;++i) {
        const float t=phase+static_cast<float>(i)*2*PI/static_cast<float>(sides);
        const float next=t+2*PI/static_cast<float>(sides);
        const auto normal=[&](float angle){return Vector3Add(Vector3Scale(tangent,std::cos(angle)),Vector3Scale(bitangent,std::sin(angle)));};
        const auto n=normal(t),m=normal(next);
        const auto p=Vector3Add(a,Vector3Scale(n,r0)),q=Vector3Add(a,Vector3Scale(m,r0));
        const auto u=Vector3Add(b,Vector3Scale(n,r1)),v=Vector3Add(b,Vector3Scale(m,r1));
        const float shade=.72F+.30F*std::max(0.0F,n.y)+.08F*random(seed+static_cast<unsigned int>(i));
        const Color facet{static_cast<unsigned char>(color.r*shade),static_cast<unsigned char>(color.g*shade),static_cast<unsigned char>(color.b*shade),color.a};
        triangle(p,q,u,facet);triangle(u,q,v,facet);
        triangle(b,u,v,color);
    }
}
inline void leaf(Vector3 p,float size,float angle,float tilt,Color color) {
    const Vector3 axis{std::cos(angle)*size,tilt*size,std::sin(angle)*size};
    const Vector3 side{-.33F*std::sin(angle)*size,0,.33F*std::cos(angle)*size};
    const auto tip=Vector3Add(p,axis),mid=Vector3Add(p,Vector3Scale(axis,.46F));
    auto ridge=mid;ridge.y+=size*.12F;
    const auto left=Vector3Add(mid,side),right=Vector3Subtract(mid,side);
    triangle(p,left,ridge,color);triangle(left,tip,ridge,color);
    const Color light{static_cast<unsigned char>(std::min(255,static_cast<int>(color.r)+35)),static_cast<unsigned char>(std::min(255,static_cast<int>(color.g)+20)),color.b,255};
    triangle(p,ridge,right,light);triangle(right,ridge,tip,light);
}

// One bounded triangle batch for the entire grove. Growth, soil and leaves share
// the authoritative wave's timing; the terrain sampler keeps every root grounded.
template<class Height>
void emitWave(Vector3 origin,Vector3 forward,float scale,float age,unsigned int seed,Height height) {
    const Vector3 right{forward.z,0,-forward.x};
    constexpr int lanes=5,segments=10;
    for (int lane=0;lane<lanes;++lane) {
        const float fan=static_cast<float>(lane-2)*.155F;
        const unsigned int laneSeed=seed+static_cast<unsigned int>(lane)*359U;
        const auto point=[&](float distance,float lift) {
            const float side=std::tan(fan)*distance+std::sin(distance*1.8F+random(laneSeed)*6)*.17F*scale;
            auto p=Vector3Add(origin,Vector3Add(Vector3Scale(forward,distance),Vector3Scale(right,side)));
            p.y=static_cast<float>(height(p.x,p.z))+lift; return p;
        };
        for (int i=0;i<segments;++i) {
            const auto key=laneSeed+static_cast<unsigned int>(i)*73U;
            const float normalized=static_cast<float>(i)/static_cast<float>(segments);
            const float local=age-normalized*static_cast<float>(stump_roots::TravelSeconds);
            if (local<=0) continue;
            const float grow=ease(local/.11F),sink=ease((local-.40F)/.38F),life=grow*(1-sink);
            if (life<.002F) continue;
            const float distance=(.30F+normalized*static_cast<float>(stump_roots::Range))*scale;
            const float thickness=(.13F-.065F*normalized)*scale*life;
            const float buried=.24F*scale*sink;
            const float h=(.08F+.17F*std::sin(normalized*PI))*scale*grow-buried;
            const auto a=point(distance,h),b=point(distance+.66F*scale*grow,h+.05F*scale*life);
            branch(a,b,thickness,thickness*.72F,{150,83,35,255},key);
            // Bright living grain traces the top, rather than a neon floor ring.
            auto grainA=a,grainB=b;grainA.y+=thickness*.77F;grainB.y+=thickness*.66F;
            branch(grainA,grainB,thickness*.11F,thickness*.07F,{237,180,68,255},key+11);
            constexpr std::array<int,lanes> spikePhase{2,0,1,2,0};
            if (i>0 && i%3==spikePhase[static_cast<std::size_t>(lane)]) {
                const float spike=(.58F+.48F*random(key+17))*scale*life;
                auto knee=point(distance+.18F*scale,h+spike*.61F);
                knee=Vector3Add(knee,Vector3Scale(right,(lane%2==0?1:-1)*.13F*scale*life));
                auto shoulder=point(distance+.36F*scale,h+spike);
                shoulder=Vector3Add(shoulder,Vector3Scale(right,(random(key+21)-.5F)*.3F*scale*life));
                auto tip=point(distance+.72F*scale,h+spike*.86F);
                branch(a,knee,thickness*1.23F,thickness*.85F,{126,65,27,255},key+1);
                branch(knee,shoulder,thickness*.85F,thickness*.42F,{173,104,42,255},key+2);
                branch(shoulder,tip,thickness*.42F,.003F,{229,171,72,255},key+3);
                auto fork=Vector3Add(knee,Vector3Scale(right,(lane<2?-1:1)*.35F*scale*life));fork.y+=spike*.2F;
                branch(knee,fork,thickness*.38F,.003F,{163,96,36,255},key+4);
                leaf(shoulder,.31F*scale*life,fan+random(key)*5, .3F,{95,174,48,255});
                leaf(tip,.22F*scale*life,fan+2.5F,-.2F,{153,211,61,255});
            }
            // A few low-poly clods lift out of the fracture and fall back.
            const float clodLife=std::clamp(local/.40F,0.0F,1.0F);
            if (clodLife<1) for (int k=0;k<2;++k) {
                auto soil=point(distance+(random(key+2)-.5F)*scale,.02F);
                soil=Vector3Add(soil,Vector3Scale(right,(k==0?-1:1)*(.15F+clodLife*.30F)*scale));
                soil.y+=std::sin(clodLife*PI)*(.18F+.20F*random(key+7))*scale;
                auto top=soil;top.y+=.10F*scale*(1-clodLife);
                branch(soil,top,.10F*scale*(1-clodLife),.06F*scale*(1-clodLife),{107,75,43,255},key+static_cast<unsigned int>(k));
            }
            if (local>.1F && local<.65F && i%2==0) {
                const float drift=(local-.1F)/.55F;
                auto p=point(distance,.15F+std::sin(drift*PI)*.65F*scale);
                p=Vector3Add(p,Vector3Scale(right,(random(key+20)-.5F)*drift*1.5F*scale));
                leaf(p,.12F*scale*std::sin(drift*PI),drift*8+random(key)*6,.5F,{192,222,98,255});
            }
        }
    }
}
} // namespace ian::stump_root_effect
