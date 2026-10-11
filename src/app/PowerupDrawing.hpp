#pragma once
#include "game/Simulation.hpp"
#include "graphics/Renderer.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/WorldBillboard.hpp"
#include "ui/PowerupIcon.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>

namespace ian::powerup_drawing {
inline Color color(PowerupType type) {
    if (type == PowerupType::Fury) return {192, 85, 255, 255};
    if (type == PowerupType::GoldRush) return {255, 203, 36, 255};
    return {66, 222, 255, 255};
}
inline void star(Vector3 center, float size, Vector3 right, Vector3 up, Color tint) {
    constexpr std::array<Vector2,8> points{{{0,-1},{.18F,-.18F},{1,0},{.18F,.18F},
        {0,1},{-.18F,.18F},{-1,0},{-.18F,-.18F}}};
    const auto vertex = [&](Vector2 p) { return Vector3Add(center,
        Vector3Add(Vector3Scale(right,p.x*size),Vector3Scale(up,p.y*size))); };
    for (std::size_t i=0;i<points.size();++i) {
        const auto a=vertex(points[i]), b=vertex(points[(i+1)%points.size()]);
        DrawTriangle3D(center,a,b,tint); DrawTriangle3D(center,b,a,tint);
    }
}
// Vertex alpha makes the glow soft without textures, square particles or allocations.
inline void energyVertex(Vector3 p, Color tint, float alpha) {
    rlColor4ub(tint.r,tint.g,tint.b,static_cast<unsigned char>(255*std::clamp(alpha,0.0F,1.0F)));
    rlVertex3f(p.x,p.y,p.z);
}
inline void energyGlow(Vector3 center, Vector3 right, Vector3 up,
                       float width, float height, Color tint, float opacity) {
    constexpr int segments = 24;
    static const std::array<Vector2,segments> perimeter=[] {
        std::array<Vector2,segments> points{};
        for(std::size_t i=0;i<points.size();++i) {
            const float angle=float(i)*2*PI/segments;
            points[i]={std::cos(angle),std::sin(angle)};
        }
        return points;
    }();
    for(std::size_t i=0;i<perimeter.size();++i) {
        const auto edge = [&](std::size_t j) {
            const Vector2 p=perimeter[j%perimeter.size()];
            return Vector3Add(center,Vector3Add(Vector3Scale(right,p.x*width),Vector3Scale(up,p.y*height))); };
        energyVertex(center,tint,opacity);
        energyVertex(edge(i),tint,0); energyVertex(edge(i+1),tint,0);
    }
}
inline void energyRibbon(Vector3 base, float radius, float height, float angle,
                         float width, Color tint, float opacity, Vector3 cameraForward) {
    constexpr int segments = 32;
    constexpr std::array<float,5> bands{-1,-.32F,0,.32F,1};
    constexpr std::array<float,5> alphas{0,.38F,1,.38F,0};
    struct Sample { Vector3 center; Vector3 side; float envelope; float opacity; };
    const auto sample=[&](float t) {
        const float a=angle+t*PI*1.35F,c=std::cos(a),s=std::sin(a);
        const float r=radius*(1-.18F*t),dr=-radius*.18F,da=PI*1.35F;
        const Vector3 tangent{dr*c-r*s*da,height,dr*s+r*c*da};
        const float taper=std::max(0.0F,std::sin(t*PI));
        return Sample{Vector3Add(base,{c*r,.12F+t*height,s*r}),
            Vector3Normalize(Vector3CrossProduct(tangent,cameraForward)),
            width*std::pow(taper,.65F),opacity*taper};
    };
    Sample previous=sample(0);
    for(int i=0;i<segments;++i) {
        const Sample next=sample(float(i+1)/segments);
        const auto emit=[&](const Sample& p,std::size_t band) {
            energyVertex(Vector3Add(p.center,Vector3Scale(p.side,bands[band]*p.envelope)),
                         tint,p.opacity*alphas[band]);
        };
        for(std::size_t b=0;b<4;++b) {
            emit(previous,b); emit(next,b+1); emit(next,b);
            emit(previous,b); emit(previous,b+1); emit(next,b+1);
        }
        previous=next;
    }
}
inline void drawFury(const SimulationSnapshot& snapshot, const Camera3D& camera,
                     Renderer& renderer, bool reducedMotion) {
    const float remaining=float(snapshot.powerups.remaining[static_cast<std::size_t>(PowerupType::Fury)]);
    if(remaining<=0) return;
    const float arrival=std::clamp((float(PowerupDefinitions[static_cast<std::size_t>(PowerupType::Fury)].duration)-remaining)/.4F,0.0F,1.0F);
    const float strength=std::clamp(remaining,0.0F,1.0F)*arrival*arrival*(3-2*arrival);
    const float time=reducedMotion ? 0 : float(snapshot.elapsedSeconds);
    const Vector3 forward=Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const Vector3 right=Vector3Normalize(Vector3CrossProduct(forward,camera.up));
    const Vector3 up=Vector3Normalize(Vector3CrossProduct(right,forward));
    rlDrawRenderBatchActive(); BeginBlendMode(BLEND_ADDITIVE);
    rlDisableDepthMask(); rlDisableBackfaceCulling(); rlSetTexture(0);
    // Depth test stays enabled: walls, terrain and the weapon hide rear energy.
    for(const auto& building:snapshot.buildings) {
        if(!runCombatBuilding(building.type) || building.health<=0) continue;
        const Vec3 position=buildingWorldPosition(building);
        const Vector3 base{float(position.x),float(position.y),float(position.z)};
        const Vector3 offset=Vector3Subtract(base,camera.position);
        const float distance=Vector3Length(offset);
        if(Vector3DotProduct(offset,forward)<=0 || distance>=65) continue;
        const float fade=strength*std::clamp((65-distance)/10,0.0F,1.0F);
        const float height=std::clamp(renderer.buildingVisualHeight(building.type,building.level),.35F,3.0F);
        const float seed=float(building.id.index%97U)*2.39996F;
        const float radius=std::clamp(.52F+height*.12F,.58F,.95F);
        const float pulse=reducedMotion ? .85F : .83F+.17F*std::sin(time*2.8F+seed);
        rlBegin(RL_TRIANGLES);
        // A quiet haze connects the flowing filaments without hiding the silhouette.
        energyGlow(Vector3Add(base,{0,height*.48F,0}),right,up,radius*1.35F,
                   height*.65F+.25F,{126,42,225,255},fade*.2F*pulse);
        for(int i=0;i<2;++i) {
            const float angle=seed+float(i)*PI-time*1.65F;
            const Color tint=i==0 ? Color{163,65,255,255} : Color{221,119,255,255};
            energyRibbon(base,radius,height+.25F,angle,.11F,tint,fade*.72F*pulse,forward);
            energyRibbon(base,radius,height+.25F,angle,.28F,{127,38,255,255},fade*.14F*pulse,forward);
        }
        const int sparkCount=distance>30 ? 4 : 7;
        for(int i=0;i<sparkCount;++i) {
            const float phase=seed+float(i)*.618034F;
            const float rise=phase+time*(.55F+float(i%3)*.07F)-std::floor(phase+time*(.55F+float(i%3)*.07F));
            const float angle=phase*6.28318F-time*1.8F+rise*2.1F;
            const float r=radius*(.85F+.22F*rise);
            const Vector3 center=Vector3Add(base,{std::cos(angle)*r,.15F+rise*(height+.55F),std::sin(angle)*r});
            const float opacity=fade*std::sin(rise*PI);
            energyGlow(center,right,up,.13F,.13F,{181,88,255,255},opacity*.42F);
            energyGlow(center,right,up,.04F,.04F,{255,213,255,255},opacity*.95F);
            // Short curved wake, fading towards the older positions.
            for(int j=1;j<=5;++j) {
                const float back=float(j)*.022F,a=angle+back*3.0F;
                const Vector3 tail=Vector3Add(base,{std::cos(a)*r,center.y-base.y-back*(height+.55F),std::sin(a)*r});
                energyGlow(tail,right,up,.035F,.035F,{190,105,255,255},opacity*.36F*(1-float(j)/6));
            }
        }
        rlEnd();
        // Small glints, sparse and asynchronous, rather than a row over the guns.
        for(int i=0;i<2;++i) {
            const float phase=seed+float(i)*2.1F;
            const float a=phase-time*1.8F;
            const float glint=reducedMotion ? .2F : std::pow(.5F+.5F*std::sin(time*3.7F+phase),6.0F);
            const Vector3 center=Vector3Add(base,{std::cos(a)*radius,height*(.45F+float(i)*.3F),std::sin(a)*radius});
            star(center,.045F+.02F*glint,right,up,Fade({255,219,255,255},fade*glint*.8F));
        }
    }
    rlDrawRenderBatchActive(); rlEnableBackfaceCulling(); rlEnableDepthMask(); EndBlendMode();
}
inline void drawWorld(const SimulationSnapshot& snapshot, const Camera3D& camera,
                      Renderer& renderer, bool reducedMotion) {
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward,camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right,forward));
    rlDrawRenderBatchActive(); rlDisableDepthMask(); BeginBlendMode(BLEND_ALPHA);
    const auto visible = [&](Vector3 p) {
        const auto offset = Vector3Subtract(p,camera.position);
        return Vector3DotProduct(offset,forward) > 0 && Vector3LengthSqr(offset) < 65*65;
    };
    for (const auto& pickup : snapshot.powerups.pickups) {
        if (!pickup.active) continue;
        Vector3 p{float(pickup.position.x),float(pickup.position.y)+.85F,float(pickup.position.z)};
        if (!visible(p)) continue;
        const float phase = float(pickup.age)*2.8F;
        if (!reducedMotion) p.y += std::sin(phase)*.1F;
        const float pop = std::clamp(float(pickup.age/.28),0.0F,1.0F);
        const float size = .64F*pop;
        Color tint = color(pickup.type);
        const float fade = std::clamp(float((PowerupSystem::PickupLifetime-pickup.age)/3),0.0F,1.0F);
        tint = Fade(tint,fade);
        drawWorldBillboardRoundedRectangle(p,size*1.8F,size*1.8F,size*.35F,Fade(tint,.12F),right,up);
        drawWorldBillboardRoundedRectangle(p,size*1.35F,size*1.35F,size*.2F,Fade({20,25,33,255},fade),right,up);
        drawWorldBillboardRoundedRectangle(Vector3Add(p,Vector3Scale(forward,-.004F)),size*1.17F,size*1.17F,size*.16F,tint,right,up);
        const Vector3 iconCenter = Vector3Add(p,Vector3Scale(forward,-.012F));
        drawPowerupIconGeometry(pickup.type,[&](Vector2 a, Vector2 b, Vector2 c) {
            const auto v = [&](Vector2 point) { return Vector3Add(iconCenter,Vector3Add(Vector3Scale(right,point.x*size*.43F),Vector3Scale(up,-point.y*size*.43F))); };
            DrawTriangle3D(v(a),v(b),v(c),Fade({255,252,222,255},fade));
            DrawTriangle3D(v(c),v(b),v(a),Fade({255,252,222,255},fade));
        });
        for (int i=0;i<3;++i) {
            const float angle = float(i)*2.1F+phase*.5F;
            const auto spark = Vector3Add(p,Vector3Add(Vector3Scale(right,std::cos(angle)*size),Vector3Scale(up,std::sin(angle)*size)));
            star(spark,size*.16F,right,up,Fade({255,248,206,255},fade*.75F));
        }
    }
    rlDrawRenderBatchActive(); EndBlendMode(); rlEnableDepthMask();
    drawFury(snapshot,camera,renderer,reducedMotion);
}
inline void drawHud(const PowerupRunState& state, float time, int width, int height, bool reduceFlashes) {
    int count = 0;
    for (double timer : state.remaining) if (timer > 0) ++count;
    if (!count) return;
    const float w = 188, gap = 12, y = float(height)-178;
    float x = (float(width)-(w+gap)*float(count)+gap)*.5F;
    for (std::size_t i=0;i<PowerupTypeCount;++i) {
        if (state.remaining[i] <= 0) continue;
        const auto type = static_cast<PowerupType>(i);
        const Color tint = color(type);
        // Soft edge tint, clear center; accessibility disables pulsing/glints.
        const unsigned char alpha = reduceFlashes ? 7 : static_cast<unsigned char>(13+3*std::sin(time*1.5F));
        DrawRectangleGradientH(0,0,70,height,{tint.r,tint.g,tint.b,alpha},{tint.r,tint.g,tint.b,0});
        DrawRectangleGradientH(width-70,0,70,height,{tint.r,tint.g,tint.b,0},{tint.r,tint.g,tint.b,alpha});
        menuStyle::panel({x,y,w,57}, 1.0F, {24,35,32,235});
        drawPowerupIconGeometry(type,[&](Vector2 a,Vector2 b,Vector2 c) {
            const auto v = [&](Vector2 p) { return Vector2{x+26+p.x*13,y+24+p.y*13}; };
            DrawTriangle(v(c),v(b),v(a),tint); DrawTriangle(v(a),v(b),v(c),tint);
        });
        menuStyle::text(PowerupDefinitions[i].name,{x+49,y+5,w-57,25},14,{255,247,215,255},UiFontRole::Display);
        menuStyle::text(TextFormat("%.0fs",std::ceil(state.remaining[i])),{x+49,y+28,w-57,19},15,tint);
        const float progress = float(state.remaining[i]/PowerupDefinitions[i].duration);
        DrawRectangleRec({x+9,y+48,(w-18)*progress,3},tint);
        x += w+gap;
    }
}
} // namespace ian::powerup_drawing
