#pragma once
#include "game/Simulation.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <cmath>
#include <array>

namespace ian::wind_blower_drawing {
// One alpha batch, no particle pool, lights or per-frame allocations. Curled
// ribbons travel from the muzzle and taper at both ends of the pulse.
inline void draw(const SimulationSnapshot& snapshot, const Camera3D& camera, bool reduceFlashes) {
    const Vector3 view=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    rlDrawRenderBatchActive();BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();rlDisableBackfaceCulling();rlSetTexture(0);rlBegin(RL_TRIANGLES);
    const auto vertex=[](Vector3 p,Color c) {rlColor4ub(c.r,c.g,c.b,c.a);rlVertex3f(p.x,p.y,p.z);};
    int visible=0;
    for(const auto& tower:snapshot.towers) {
        const double duration=std::max(WindBlowerPulseSeconds,tower.attack.stats.duration);
        if(tower.type!=BuildingType::WindBlower || tower.windAge>=duration)continue;
        const Vector3 origin{float(tower.windOrigin.x),float(tower.windOrigin.y),float(tower.windOrigin.z)};
        const Vector3 offset=Vector3Subtract(origin,camera.position);
        if(Vector3LengthSqr(offset)>2500 || Vector3DotProduct(offset,view)<-8)continue;
        if(++visible>24)break;
        const float age=float(tower.windAge/duration);
        const float length=float(windBlowerRange(tower.attack.stats))-.479F;
        const Vector3 forward{-float(std::sin(tower.windYaw)),0,-float(std::cos(tower.windYaw))};
        const Vector3 side{-forward.z,0,forward.x};
        constexpr int ribbons=9,segments=12;
        for(int ribbon=0;ribbon<ribbons;++ribbon) {
            const float lane=float(ribbon-4)/4;
            const float phase=std::clamp(age*1.35F-float(ribbon%3)*.08F,0.0F,1.0F);
            const float head=phase*length,tail=std::max(0.0F,head-length*.33F);
            if(head-tail<.005F)continue;
            const auto point=[&](int segment) {
                const float t=float(segment)/segments,distance=tail+(head-tail)*t;
                const float curl=std::sin(t*PI*1.7F+float(ribbon)*1.4F+age*5)*.12F;
                const float spread=lane*(.15F+distance*.46F)+curl;
                Vector3 p=Vector3Add(origin,Vector3Add(Vector3Scale(forward,distance),Vector3Scale(side,spread)));
                p.y+=std::sin(float(ribbon)*2.4F)*(.10F+distance*.08F)+curl*.5F;
                return p;
            };
            // One cross-section per node. Adjacent quads reuse EXACTLY the
            // same positions; per-segment camera-facing axes leave cracks at
            // bends, especially when seen along the stream.
            std::array<Vector3,segments+1> centers{},widths{};
            for(int node=0;node<=segments;++node)
                centers[static_cast<std::size_t>(node)]=point(node);
            for(int node=0;node<=segments;++node) {
                const auto index=static_cast<std::size_t>(node);
                const auto before=static_cast<std::size_t>(std::max(0,node-1));
                const auto after=static_cast<std::size_t>(std::min(segments,node+1));
                const auto tangent=Vector3Subtract(centers[after],centers[before]);
                auto axis=Vector3CrossProduct(tangent,Vector3Subtract(camera.position,centers[index]));
                if(Vector3LengthSqr(axis)<1e-10F)axis=Vector3CrossProduct(tangent,camera.up);
                const float t=float(node)/segments;
                widths[index]=Vector3Scale(Vector3Normalize(axis),.025F+.04F*std::sin(PI*t));
            }
            // Feather only the outside edges, never the internal segment joins.
            constexpr std::array<float,4> bands{-1.0F,-.65F,.65F,1.0F};
            constexpr std::array<float,4> opacity{0.0F,1.0F,1.0F,0.0F};
            const float fade=std::sin(PI*age)*(reduceFlashes?.65F:1.0F);
            const auto section=[&](int node,int band) {
                const auto index=static_cast<std::size_t>(node);
                return Vector3Add(centers[index],Vector3Scale(widths[index],bands[static_cast<std::size_t>(band)]));
            };
            const auto color=[&](int node,int band) {
                return Color{211,250,255,static_cast<unsigned char>(170*fade*std::sin(PI*float(node)/segments)*opacity[static_cast<std::size_t>(band)])};
            };
            for(int segment=0;segment<segments;++segment)for(int band=0;band<3;++band) {
                const auto a=section(segment,band),b=section(segment,band+1),
                    c=section(segment+1,band+1),d=section(segment+1,band);
                vertex(a,color(segment,band));vertex(b,color(segment,band+1));vertex(c,color(segment+1,band+1));
                vertex(a,color(segment,band));vertex(c,color(segment+1,band+1));vertex(d,color(segment+1,band));
            }
        }
    }
    rlEnd();rlDrawRenderBatchActive();rlEnableBackfaceCulling();rlEnableDepthMask();EndBlendMode();
}
}
