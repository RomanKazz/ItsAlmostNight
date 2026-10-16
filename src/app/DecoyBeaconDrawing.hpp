#pragma once
#include "game/Simulation.hpp"
#include "enemies/EnemyCollision.hpp"
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <cmath>

namespace ian::decoy_beacon_drawing {
// Bounded, batched geometry: no lights, emitters or particle allocations.
inline void draw(const SimulationSnapshot& snapshot, const Camera3D& camera,
    bool reduceFlashes, const TerrainHeightfield& terrain) {
    if(snapshot.decoyBeacons.empty())return;
    const Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const Vector3 right=Vector3Normalize(Vector3CrossProduct(forward,camera.up));
    const Vector3 up=Vector3Normalize(Vector3CrossProduct(right,forward));
    const auto vertex=[](Vector3 p,Color c) {rlColor4ub(c.r,c.g,c.b,c.a);rlVertex3f(p.x,p.y,p.z);};
    const auto glow=[&](Vector3 center,float radius,float alpha) {
        constexpr int segments=20;
        for(int i=0;i<segments;++i) {
            const auto point=[&](int j) {const float a=float(j)*2*PI/segments;return Vector3Add(center,
                Vector3Add(Vector3Scale(right,std::cos(a)*radius),Vector3Scale(up,std::sin(a)*radius)));};
            vertex(center,{255,223,98,static_cast<unsigned char>(alpha)});
            vertex(point(i),{255,187,47,0});vertex(point(i+1),{255,187,47,0});
        }
    };
    rlDrawRenderBatchActive();BeginBlendMode(BLEND_ALPHA);
    rlDisableDepthMask();rlDisableBackfaceCulling();rlSetTexture(0);rlBegin(RL_TRIANGLES);
    int visible=0;
    for(const auto& beacon:snapshot.decoyBeacons) {
        const Vector3 origin{float(beacon.position.x),float(beacon.position.y),float(beacon.position.z)};
        const auto offset=Vector3Subtract(origin,camera.position);
        if(Vector3LengthSqr(offset)>2500 || Vector3DotProduct(offset,forward)<-8)continue;
        if(++visible>16)break;
        const float light=static_cast<float>(decoyBeaconLightStrength(beacon));
        glow(Vector3Add(origin,{0,.6536F,0}),.36F+.06F*light,(reduceFlashes ? 28 : 62)*light);
        const float flash=std::exp(-float(beacon.pulseAge)*4);
        for(const auto lamp:std::array<Vector3,4>{{{0,.6705F,-.505F},{-.505F,.6705F,0},{0,.6705F,.505F},{.505F,.6705F,0}}})
            glow(Vector3Add(origin,lamp),.20F+.12F*flash,(reduceFlashes ? 24 : 95)*flash);
        if(beacon.pulseAge>=DecoyBeaconPulseDuration)continue;
        const float progress=float(beacon.pulseAge/DecoyBeaconPulseDuration);
        const float fade=std::sin(PI*progress);
        const float radius=static_cast<float>(decoyBeaconRadius(beacon.level))*progress;
        constexpr int segments=64;
        constexpr std::array<float,5> bands{{-.30F,-.15F,0,.15F,.30F}};
        constexpr std::array<float,5> opacity{{0,.45F,1,.45F,0}};
        const auto building=std::ranges::find(snapshot.buildings,beacon.buildingId,&BuildingInstance::id);
        const bool raised=building!=snapshot.buildings.end() && building->platformStorey>=0;
        const auto point=[&](int i,int band) {
            const float a=float(i)*2*PI/segments,r=std::max(0.0F,radius+bands[static_cast<std::size_t>(band)]);
            const float x=origin.x+std::cos(a)*r,z=origin.z+std::sin(a)*r;
            // Flat platforms retain their elevation; terrain waves follow slopes.
            float y=float(terrain.getHeight(x,z));
            if(raised && r<1.0F)y=std::max(y,origin.y);
            return Vector3{x,y+.075F,z};
        };
        for(int segment=0;segment<segments;++segment)for(int band=0;band<4;++band) {
            const Color a{255,213,88,static_cast<unsigned char>((reduceFlashes ? 48 : 100)*fade*opacity[static_cast<std::size_t>(band)])};
            const Color b{255,213,88,static_cast<unsigned char>((reduceFlashes ? 48 : 100)*fade*opacity[static_cast<std::size_t>(band+1)])};
            const auto p00=point(segment,band),p01=point(segment,band+1),p10=point(segment+1,band),p11=point(segment+1,band+1);
            vertex(p00,a);vertex(p10,a);vertex(p11,b);vertex(p00,a);vertex(p11,b);vertex(p01,b);
        }
    }
    int flashes=0;
    for(const auto& enemy:snapshot.enemies) {
        if(!enemy.active || enemy.decoyFlashRemaining<=0)continue;
        const auto cylinder=enemyCollisionCylinder(enemy);
        Vector3 center{float(cylinder.center.x),float(cylinder.center.y+cylinder.halfHeight)+.2F,float(cylinder.center.z)};
        if(Vector3DistanceSqr(center,camera.position)>2500)continue;
        if(++flashes>128)break;
        const float t=1-float(enemy.decoyFlashRemaining/.65);
        center.y+=t*.35F;
        const float size=.11F+.10F*t,alpha=std::sin(PI*t)*(reduceFlashes ? 100 : 210);
        const Vector3 top=Vector3Add(center,Vector3Scale(up,size*1.7F)),bottom=Vector3Add(center,Vector3Scale(up,-size*1.7F));
        const Vector3 left=Vector3Add(center,Vector3Scale(right,-size)),r=Vector3Add(center,Vector3Scale(right,size));
        const Color c{255,231,128,static_cast<unsigned char>(alpha)};
        vertex(top,c);vertex(left,c);vertex(r,c);vertex(left,c);vertex(bottom,c);vertex(r,c);
    }
    rlEnd();rlDrawRenderBatchActive();rlEnableBackfaceCulling();rlEnableDepthMask();EndBlendMode();
}
} // namespace ian::decoy_beacon_drawing
