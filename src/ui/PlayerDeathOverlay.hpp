#pragma once

#include "game/Simulation.hpp"
#include "game/PlayerRespawn.hpp"
#include "ui/GameUi.hpp"
#include "ui/UiMenuLayout.hpp"
#include "ui/UiMenuStyle.hpp"
#include <cmath>
#include <string>

namespace ian {
inline constexpr float PlayerRespawnRevealSeconds = .65F;

inline void drawPlayerDeathOverlay(const GameUi& ui, const SimulationSnapshot& snapshot,
                                   Vector2 viewport, float revealRemaining = 0,
                                   bool reduceMotion = false) {
    using namespace menuStyle;
    if (snapshot.state == RunState::Defeat || snapshot.state == RunState::Victory ||
        snapshot.state == RunState::MainMenu) return;
    if (!snapshot.playerRespawning && snapshot.playerRespawnProtectionRemaining <= 0 && revealRemaining <= 0) return;
    flushSdfUiTextBatch();
    const auto smooth = [](float value) {
        value = std::clamp(value, 0.0F, 1.0F);
        return value * value * (3.0F - 2.0F * value);
    };
    if (!snapshot.playerRespawning) {
        if (snapshot.playerRespawnProtectionRemaining > 0) {
            const auto l = UiMenuLayout::fromViewport(viewport);
            panel(l.rect(836, 858, 248, 52), l.scale, Inset);
            const Texture2D icon = ui.coreMenuTexture(CoreMenuTexture::Shield);
            if (icon.id) DrawTexturePro(icon,
                {0, 0, static_cast<float>(icon.width), static_cast<float>(icon.height)},
                l.rect(850, 865, 36, 36), {}, 0, WHITE);
            text("PROTECTED", l.rect(894, 862, 174, 32), 20 * l.scale,
                 {118, 223, 255, 255}, UiFontRole::Display, 1);
            progress(l.rect(894, 899, 174, 5),
                snapshot.playerRespawnProtectionRemaining / PlayerRespawnProtectionSeconds,
                {91, 209, 250, 255});
        }
        flushSdfUiTextBatch();
        if (revealRemaining > 0) DrawRectangleRec({0, 0, viewport.x, viewport.y},
            Fade(BLACK, smooth(revealRemaining / PlayerRespawnRevealSeconds)));
        return;
    }
    const float elapsed = static_cast<float>(std::max(0.0,
        snapshot.playerRespawnDuration - snapshot.playerRespawnTimeRemaining));
    const float entrance = smooth(elapsed / .28F);
    auto l = UiMenuLayout::fromViewport(viewport);
    if (!reduceMotion) {
        l.scale *= .94F + .06F * entrance;
        l.origin = {(viewport.x - 1920*l.scale)*.5F, (viewport.y - 1080*l.scale)*.5F};
    }
    DrawRectangleRec({0, 0, viewport.x, viewport.y}, Fade({8, 12, 17, 255}, .68F * entrance));
    const bool lost = snapshot.deathLostWood > 0 || snapshot.deathLostStone > 0 || snapshot.deathLostCrystals > 0;
    const float height = lost ? 428.0F : 350.0F;
    const float top = (1080-height)*.5F;
    panel(l.rect(635, top+8, 650, height), l.scale, {6, 9, 8, 255}, entrance*.55F);
    panel(l.rect(635, top, 650, height), l.scale, Frame, entrance);
    panel(l.rect(654, top+19, 612, height-38), l.scale, Inset, entrance);
    // Cream cartoon skull: chunky silhouette, dark eye sockets, no texture loading.
    const Rectangle emblem = l.rect(902, top-54, 116, 116);
    const Vector2 center{emblem.x+emblem.width*.5F, emblem.y+emblem.height*.48F};
    DrawCircleV(center, 58*l.scale, Fade({7, 10, 8, 255}, entrance));
    DrawCircleV(center, 52*l.scale, Fade({112, 58, 52, 255}, entrance));
    DrawCircleV(center, 36*l.scale, Fade(BLACK, entrance));
    DrawCircleV(center, 32*l.scale, Fade(Cream, entrance));
    DrawRectangleRec({center.x-23*l.scale,center.y+15*l.scale,46*l.scale,29*l.scale}, Fade(BLACK,entrance));
    DrawRectangleRec({center.x-18*l.scale,center.y+15*l.scale,36*l.scale,23*l.scale}, Fade(Cream,entrance));
    for (float x : {-13.0F,13.0F}) DrawCircleV({center.x+x*l.scale,center.y},9*l.scale,Fade(Inset,entrance));
    DrawTriangle({center.x,center.y+9*l.scale},{center.x-5*l.scale,center.y+17*l.scale},
                 {center.x+5*l.scale,center.y+17*l.scale},Fade(Inset,entrance));
    for (float x : {-6.0F,6.0F}) DrawRectangleRec({center.x+x*l.scale,center.y+28*l.scale,3*l.scale,10*l.scale},Fade(Inset,entrance));
    text("YOU DIED",l.rect(682,top+75,556,68),48*l.scale,
         Fade({255,117,96,255},entrance),UiFontRole::Display,1);
    text("RESPAWN IN",l.rect(700,top+153,520,30),22*l.scale,
         Fade(Cream,entrance),UiFontRole::Display,1);
    const int seconds = std::max(1,static_cast<int>(std::ceil(snapshot.playerRespawnTimeRemaining)));
    text(std::to_string(seconds),l.rect(900,top+189,120,68),54*l.scale,
         Fade(Gold,entrance),UiFontRole::Display,1);
    const double fraction = snapshot.playerRespawnDuration > 0
        ? 1-snapshot.playerRespawnTimeRemaining/snapshot.playerRespawnDuration : 1;
    progress(l.rect(732,top+273,456,15),fraction,{255,177,80,255});
    if (lost) {
        const std::array<int,3> losses{snapshot.deathLostWood,snapshot.deathLostStone,snapshot.deathLostCrystals};
        const std::array<UiResourceIcon,3> icons{UiResourceIcon::Wood,UiResourceIcon::Stone,UiResourceIcon::Crystal};
        const int count=static_cast<int>(std::count_if(losses.begin(),losses.end(),[](int amount){return amount>0;}));
        int index=0;
        for (std::size_t i=0;i<losses.size();++i) {
            if (losses[i]<=0) continue;
            const float x=960-static_cast<float>(count)*72+static_cast<float>(index++)*144;
            ui.drawResourceIcon(l.rect(x,top+326,40,40),icons[i],Fade(WHITE,entrance));
            text("-"+std::to_string(losses[i]),l.rect(x+46,top+325,88,44),28*l.scale,
                 Fade(Cream,entrance),UiFontRole::Display);
        }
    }
    // Hide the relocation itself, then reveal the new position over .65 seconds.
    const float closing=smooth((.30F-static_cast<float>(snapshot.playerRespawnTimeRemaining))/.30F);
    flushSdfUiTextBatch();
    if (closing>0) DrawRectangleRec({0,0,viewport.x,viewport.y},Fade(BLACK,closing));
}
} // namespace ian
