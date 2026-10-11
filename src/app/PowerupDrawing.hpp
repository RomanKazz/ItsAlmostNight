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
inline void wisp(Vector3 center, float width, float height, float curl,
                 Vector3 right, Vector3 up, Color tint) {
    // Rounded belly and a bent, tapering tip. A cartoon flame, not a quad particle.
    const std::array<Vector2,10> shape{{{curl,.95F},{-.06F,.51F},{-.28F,.28F},
        {-.4F,.02F},{-.32F,-.23F},{-.1F,-.33F},{.14F,-.3F},
        {.34F,-.12F},{.28F,.18F},{.08F,.47F}}};
    const auto vertex=[&](Vector2 p) { return Vector3Add(center,
        Vector3Add(Vector3Scale(right,p.x*width),Vector3Scale(up,p.y*height))); };
    for (std::size_t i=0;i<shape.size();++i) {
        const auto a=vertex(shape[i]),b=vertex(shape[(i+1)%shape.size()]);
        DrawTriangle3D(center,a,b,tint); DrawTriangle3D(center,b,a,tint);
    }
}
inline void drawWorld(const SimulationSnapshot& snapshot, const Camera3D& camera,
                      Renderer& renderer, bool reducedMotion) {
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward,camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right,forward));
    const float time = static_cast<float>(snapshot.elapsedSeconds);
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
    const float fury = std::clamp(float(snapshot.powerups.remaining[static_cast<std::size_t>(PowerupType::Fury)])/1.0F,0.0F,1.0F);
    if (fury > 0) for (const auto& building : snapshot.buildings) {
        if (!runCombatBuilding(building.type) || building.health <= 0) continue;
        const Vec3 position = buildingWorldPosition(building);
        Vector3 base{float(position.x),float(position.y),float(position.z)};
        if (!visible(base)) continue;
        const float height = std::clamp(float(renderer.buildingVisualHeight(building.type,building.level)),.5F,3.0F);
        // Four layered wisps: bright cores, lavender bodies, sparse sparks.
        // No opaque shell hiding the weapon, no expanding ground circles.
        for (int i=0;i<4;++i) {
            const float seed = float(building.id.index)*.73F+float(i)*1.57F;
            const float rise = reducedMotion ? .45F : std::fmod(time*.7F+seed,1.0F);
            const float angle = seed+(reducedMotion ? 0 : time*.3F);
            Vector3 center = Vector3Add(base,{std::cos(angle)*.58F,.18F+rise*(height+.3F),std::sin(angle)*.58F});
            const float opacity = std::sin(rise*PI)*fury;
            const float curl = reducedMotion ? .1F : std::sin(time*2+seed)*.23F;
            wisp(center,.8F,.6F,curl,right,up,Fade({156,51,246,255},opacity*.47F));
            wisp(Vector3Add(center,Vector3Scale(forward,-.008F)),.4F,.34F,
                curl,right,up,Fade({228,142,255,255},opacity*.66F));
            star(Vector3Add(center,Vector3Scale(up,.7F)),.08F,right,up,
                Fade({245,206,255,255},opacity*.75F));
        }
    }
    rlDrawRenderBatchActive(); EndBlendMode(); rlEnableDepthMask();
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
