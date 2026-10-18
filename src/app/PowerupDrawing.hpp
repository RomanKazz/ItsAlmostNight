#pragma once
#include "game/Simulation.hpp"
#include "graphics/Renderer.hpp"
#include "ui/WorldBillboard.hpp"
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
                     Renderer& renderer, bool reduceFlashes) {
    const float remaining=float(snapshot.powerups.remaining[static_cast<std::size_t>(PowerupType::Fury)]);
    if(remaining<=0) return;
    const float arrival=std::clamp((float(PowerupDefinitions[static_cast<std::size_t>(PowerupType::Fury)].duration)-remaining)/.4F,0.0F,1.0F);
    const float strength=std::clamp(remaining,0.0F,1.0F)*arrival*arrival*(3-2*arrival);
    // Reduce flashes suppresses brightness modulation, never continuous motion.
    const float time=float(snapshot.elapsedSeconds);
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
        const float pulse=reduceFlashes ? .85F : .83F+.17F*std::sin(time*2.8F+seed);
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
            const float glint=reduceFlashes ? .2F : std::pow(.5F+.5F*std::sin(time*3.7F+phase),6.0F);
            const Vector3 center=Vector3Add(base,{std::cos(a)*radius,height*(.45F+float(i)*.3F),std::sin(a)*radius});
            star(center,.045F+.02F*glint,right,up,Fade({255,219,255,255},fade*glint*.8F));
        }
    }
    rlDrawRenderBatchActive(); rlEnableBackfaceCulling(); rlEnableDepthMask(); EndBlendMode();
}
inline void drawWorld(const SimulationSnapshot& snapshot, const Camera3D& camera,
                      Renderer& renderer, bool reduceFlashes) {
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target,camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward,camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right,forward));
    const auto visible = [&](Vector3 p) {
        const auto offset = Vector3Subtract(p,camera.position);
        return Vector3DotProduct(offset,forward) > 0 && Vector3LengthSqr(offset) < 65*65;
    };
    const auto pickupPose = [&](const PowerupPickup& pickup) {
        const float phase = float(pickup.age)*2.3F;
        // Accessibility suppresses bright flashes; continuous float/spin remain.
        return Vector3{float(pickup.position.x),float(pickup.position.y)+.72F+
            std::sin(phase)*.075F,float(pickup.position.z)};
    };
    for (const auto& pickup : snapshot.powerups.pickups) {
        if (!pickup.active) continue;
        const Vector3 p = pickupPose(pickup);
        if (!visible(p)) continue;
        const float arrival = std::clamp(float(pickup.age/.28),0.0F,1.0F);
        const float pop = arrival*arrival*(3-2*arrival);
        const float fade = std::clamp(float((PowerupSystem::PickupLifetime-pickup.age)/3),0.0F,1.0F);
        static_cast<void>(renderer.drawPowerupPickup(static_cast<std::size_t>(pickup.type),
            p,float(pickup.age)*.85F,pop,Fade(WHITE,fade)));
    }
    // Small soft glows have no rectangular backing and never cover scenery.
    rlDrawRenderBatchActive(); rlDisableDepthMask(); BeginBlendMode(BLEND_ADDITIVE);
    rlDisableBackfaceCulling(); rlSetTexture(0);
    for (const auto& pickup : snapshot.powerups.pickups) {
        if (!pickup.active) continue;
        const Vector3 p = pickupPose(pickup);
        if (!visible(p)) continue;
        const float fade = std::clamp(float((PowerupSystem::PickupLifetime-pickup.age)/3),0.0F,1.0F)*
            std::clamp(float(pickup.age/.28),0.0F,1.0F);
        const Color tint = color(pickup.type);
        rlBegin(RL_TRIANGLES);
        energyGlow(Vector3Add(p,{0,-.36F,0}),right,up,.34F,.12F,tint,fade*.16F);
        rlEnd();
        for (int i=0;i<2;++i) {
            const float angle = float(i)*PI+float(pickup.age)*1.4F;
            const auto spark = Vector3Add(p,Vector3Add(Vector3Scale(right,std::cos(angle)*.53F),
                Vector3Scale(up,std::sin(angle)*.37F)));
            star(spark,.038F,right,up,Fade({255,248,222,255},fade*(reduceFlashes ? .4F : .65F)));
        }
    }
    rlDrawRenderBatchActive(); rlEnableBackfaceCulling(); EndBlendMode(); rlEnableDepthMask();
    drawFury(snapshot,camera,renderer,reduceFlashes);
}
// A bounded, texture-free edge mesh keeps the aiming area completely clear.
// Alpha wash and additive wisps share the same continuous contour: no panels,
// countdown text, full-screen render target, or shader compilation on pickup.
inline void drawHud(const PowerupRunState& state, float time, int width, int height, bool reduceFlashes) {
    if (width <= 0 || height <= 0) return;
    const int active = static_cast<int>(std::ranges::count_if(state.remaining,
        [](double remaining) { return remaining > 0.0; }));
    if (active == 0) return;
    constexpr int Segments = 96;
    static constexpr std::array<float, 8> Bands{0, .07F, .16F, .29F, .45F, .63F, .82F, 1};
    constexpr std::array<float, 8> Wash{1, .81F, .58F, .34F, .16F, .055F, .01F, 0};
    const float halfWidth = float(width) * .5F, halfHeight = float(height) * .5F;
    const float shortSide = float(std::min(width, height));
    const auto ease = [](float value) {
        const float t = std::clamp(value, 0.0F, 1.0F);
        return t * t * (3 - 2 * t);
    };
    const auto vertex = [](Vector2 point, Color tint, float alpha) {
        rlColor4ub(tint.r, tint.g, tint.b,
            static_cast<unsigned char>(255 * std::clamp(alpha, 0.0F, 1.0F)));
        rlVertex2f(point.x, point.y);
    };
    struct EdgeSample {
        std::array<Vector2, Bands.size()> points;
        Color tint;
        float corner;
    };
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlSetTexture(0);
    for (std::size_t effect = 0; effect < PowerupTypeCount; ++effect) {
        const float remaining = float(state.remaining[effect]);
        if (remaining <= 0) continue;
        const auto type = static_cast<PowerupType>(effect);
        const bool gold = type == PowerupType::GoldRush;
        const bool fury = type == PowerupType::Fury;
        const float arrival = ease((float(PowerupDefinitions[effect].duration) - remaining) / .65F);
        const float departure = ease(remaining / 1.4F);
        const float strength = arrival * departure * (active > 1 ? .76F : 1.0F);
        if (strength <= 0) continue;
        const float pulse = reduceFlashes ? .88F : .91F + .09F * std::sin(time * 1.6F);
        const float opacity = strength * pulse * (fury ? .40F : gold ? .47F : .18F);
        const float depth = shortSide * (fury ? .17F : gold ? .155F : .105F);
        const float phase = time * (fury ? -.40F : .22F) + float(effect) * 1.7F;
        const auto sample = [&](float angle) {
            const float c = std::cos(angle), s = std::sin(angle);
            const float major = std::max(std::abs(c), std::abs(s));
            const float corner = std::min(std::abs(c), std::abs(s)) / major;
            const float wave = .92F + .075F * std::sin(angle * 5 + phase * 1.8F) +
                .045F * std::sin(angle * 9 - phase);
            EdgeSample result{};
            for (std::size_t band = 0; band < Bands.size(); ++band) {
                const float inset = depth * Bands[band] * wave * (1 + .38F * corner * corner);
                result.points[band] = {
                    halfWidth + (halfWidth - inset) * c / major,
                    halfHeight + (halfHeight - inset) * s / major};
            }
            const float flow = .5F + .5F * std::sin(angle * 3 + phase);
            result.tint = gold ? Color{255, static_cast<unsigned char>(179 + 39 * flow), 42, 255}
                : fury ? Color{static_cast<unsigned char>(148 + 58 * flow), 62, 255, 255}
                       : Color{58, 203, 255, 255};
            result.corner = .76F + .24F * corner * corner;
            return result;
        };
        BeginBlendMode(BLEND_ALPHA);
        rlBegin(RL_TRIANGLES);
        EdgeSample rimPrevious = sample(0);
        for (int segment = 1; segment <= Segments; ++segment) {
            const EdgeSample next = sample(float(segment) * 2 * PI / Segments);
            const auto emit = [&](const EdgeSample& edge, std::size_t band) {
                vertex(edge.points[band], edge.tint, opacity * Wash[band] * edge.corner);
            };
            for (std::size_t band = 0; band + 1 < Bands.size(); ++band) {
                emit(rimPrevious, band); emit(next, band); emit(next, band + 1);
                emit(rimPrevious, band); emit(next, band + 1); emit(rimPrevious, band + 1);
            }
            rimPrevious = next;
        }
        rlEnd();
        EndBlendMode();
        BeginBlendMode(BLEND_ADDITIVE);
        if (fury || gold) {
            // Short, feathered arcs travel along the rim rather than forming
            // a static frame or streaking through the crosshair.
            constexpr int ArcSegments = 24;
            constexpr std::array<float, 5> CrossSection{-1, -.38F, 0, .38F, 1};
            constexpr std::array<float, 5> ArcAlpha{0, .22F, 1, .22F, 0};
            const Color light = gold ? Color{255, 231, 142, 255} : Color{226, 145, 255, 255};
            rlSetTexture(0);
            rlBegin(RL_TRIANGLES);
            for (int arc = 0; arc < 3; ++arc) {
                const float seed = float(arc) * 2.094395F;
                struct ArcSample { std::array<Vector2, 5> points; float opacity; };
                const auto arcSample = [&](float t) {
                    const float angle = seed + phase * .32F + t * .55F;
                    const EdgeSample edge = sample(angle);
                    const float taper = std::sin(t * PI);
                    const float inset = .19F + .10F * std::sin(angle * 4 - phase * 2);
                    ArcSample result{};
                    for (std::size_t band = 0; band < CrossSection.size(); ++band)
                        result.points[band] = Vector2Lerp(edge.points.front(), edge.points.back(),
                            inset + CrossSection[band] * .10F * taper);
                    result.opacity = strength * (reduceFlashes ? .07F : .11F) * taper * taper;
                    return result;
                };
                const auto emit = [&](const ArcSample& point, std::size_t band) {
                    vertex(point.points[band], light, point.opacity * ArcAlpha[band]);
                };
                ArcSample previous = arcSample(0);
                for (int segment = 0; segment < ArcSegments; ++segment) {
                    const ArcSample next = arcSample(float(segment + 1) / ArcSegments);
                    for (std::size_t band = 0; band + 1 < CrossSection.size(); ++band) {
                        emit(previous, band); emit(next, band); emit(next, band + 1);
                        emit(previous, band); emit(next, band + 1); emit(previous, band + 1);
                    }
                    previous = next;
                }
            }
            rlEnd();
        }
        const int sparkCount = gold ? 20 : fury ? 14 : 6;
        for (int spark = 0; spark < sparkCount; ++spark) {
            const float seed = float(spark) * .618034F + float(effect) * .31F;
            const float age = seed + time * (gold ? .14F : .19F);
            const float life = age - std::floor(age);
            const float angle = seed * 2 * PI + time * (gold ? -.055F : .095F);
            const EdgeSample edge = sample(angle);
            const float drift = .15F + .46F * life;
            const Vector2 point = Vector2Lerp(edge.points.front(), edge.points.back(), drift);
            const float visibility = std::sin(life * PI) * strength * (reduceFlashes ? .40F : .65F);
            const float size = shortSide * (.0024F + .0012F * float(spark % 3));
            const Color tint = gold ? Color{255, 222, 117, 255} : fury
                ? Color{229, 174, 255, 255} : Color{165, 239, 255, 255};
            DrawCircleGradient(point, size * 3.8F,
                Fade(color(type), visibility * .13F), BLANK);
            if (gold && spark % 4 == 0) {
                // Small elliptical coin glints stay at the border, away from combat.
                DrawEllipse(static_cast<int>(point.x), static_cast<int>(point.y), size * .72F, size * 1.05F,
                    Fade({255, 185, 37, 255}, visibility));
                DrawEllipse(static_cast<int>(point.x - size * .18F), static_cast<int>(point.y - size * .12F),
                    size * .22F, size * .63F, Fade({255, 248, 190, 255}, visibility));
            } else {
                const std::array<Vector2, 8> points{{{0, -1.6F}, {.16F, -.16F}, {1, 0}, {.16F, .16F},
                    {0, 1.6F}, {-.16F, .16F}, {-1, 0}, {-.16F, -.16F}}};
                rlSetTexture(0);
                rlBegin(RL_TRIANGLES);
                for (std::size_t i = 0; i < points.size(); ++i) {
                    const Vector2 a = Vector2Add(point, Vector2Scale(points[i], size));
                    const Vector2 b = Vector2Add(point, Vector2Scale(points[(i + 1) % points.size()], size));
                    vertex(point, tint, visibility); vertex(a, tint, 0); vertex(b, tint, 0);
                }
                rlEnd();
            }
        }
        EndBlendMode();
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}
} // namespace ian::powerup_drawing
