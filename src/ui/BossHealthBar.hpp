#pragma once

#include "enemies/EnemySystem.hpp"
#include "combat/CombatDisplayUnits.hpp"
#include "ui/UiMenuStyle.hpp"

namespace ian {
namespace bossHud {
inline void crown(Vector2 center,float scale) {
    const auto triangle=[&](float x,float y,float width,float height,Color color) {
        const std::array<Vector2,3> points{{{center.x+x*scale,center.y+y*scale},
            {center.x+(x-width*.5F)*scale,center.y+(y+height)*scale},
            {center.x+(x+width*.5F)*scale,center.y+(y+height)*scale}}};
        drawSolidTriangleFan(points.data(),3,color);
    };
    for(const float x:{-11.0F,0.0F,11.0F}) {
        const float y=x==0 ? -15.0F : -11.0F;
        triangle(x,y,18,23,{15,17,18,255});
        triangle(x,y+3,11,16,menuStyle::Gold);
    }
    drawChamferedFill({center.x-18*scale,center.y+4*scale,36*scale,12*scale},3*scale,BLACK);
    drawChamferedFill({center.x-15*scale,center.y+6*scale,30*scale,7*scale},2*scale,{255,182,32,255});
    drawChamferedFill({center.x-3*scale,center.y+5*scale,6*scale,7*scale},2*scale,{131,228,164,255});
}
inline void fill(Rectangle bounds,Color top,Color bottom) {
    if(bounds.width<=0) return;
    const float cut=std::min(7.0F,std::min(bounds.width,bounds.height)*.5F);
    const float right=bounds.x+bounds.width,base=bounds.y+bounds.height;
    const std::array<Vector2,10> points{{{bounds.x+bounds.width*.5F,bounds.y+bounds.height*.5F},
        {bounds.x+cut,bounds.y},{bounds.x,bounds.y+cut},{bounds.x,base-cut},{bounds.x+cut,base},
        {right-cut,base},{right,base-cut},{right,bounds.y+cut},{right-cut,bounds.y},{bounds.x+cut,bounds.y}}};
    rlCheckRenderBatchLimit(24);rlSetTexture(rlGetTextureIdDefault());rlBegin(RL_TRIANGLES);
    rlNormal3f(0,0,1);rlTexCoord2f(.5F,.5F);
    const auto vertex=[&](Vector2 point) {
        const float t=(point.y-bounds.y)/bounds.height;
        rlColor4ub(static_cast<unsigned char>(top.r+(bottom.r-top.r)*t),
            static_cast<unsigned char>(top.g+(bottom.g-top.g)*t),
            static_cast<unsigned char>(top.b+(bottom.b-top.b)*t),255);
        rlVertex2f(point.x,point.y);
    };
    for(std::size_t i=1;i+1<points.size();++i) {vertex(points[0]);vertex(points[i]);vertex(points[i+1]);}
    rlEnd();rlSetTexture(0);
}
} // namespace bossHud

inline void drawBossHealthBar(std::span<const EnemyInstance> enemies,int width) {
    const EnemyInstance* boss=nullptr;
    for(const auto& unit:enemies) if(unit.active && unit.type==EnemyType::Boss &&
        (!boss || unit.maxHealth>boss->maxHealth)) boss=&unit;
    if(!boss) return;
    const float scale=std::clamp(static_cast<float>(width)/1280.0F,.70F,1.15F);
    const float w=460*scale,x=(static_cast<float>(width)-w)*.5F,y=116;
    const float left=x+48*scale,barY=y+27*scale,barW=w-48*scale;
    const auto box=[&](float a,float b,float c,float d){return Rectangle{a,b,c*scale,d*scale};};
    const Color cream{255,242,190,255};
    menuStyle::text("ALPAKING",box(left+3*scale,y,270,25),23*scale,cream,UiFontRole::Display);
    for(int phase=1;phase<=3;++phase) {
        const float px=x+w-static_cast<float>(4-phase)*21*scale;
        drawChamferedFill(box(px,y+7*scale,15,15),4*scale,BLACK);
        drawChamferedFill(box(px+2*scale,y+9*scale,11,11),3*scale,
            phase<=boss->bossPhase ? menuStyle::Gold : Color{79,76,62,255});
    }
    drawChamferedFill({left,barY+4*scale,barW,34*scale},10*scale,{8,13,17,180});
    drawChamferedFill({left,barY,barW,34*scale},10*scale,BLACK);
    drawChamferedFill({left+3*scale,barY+3*scale,barW-6*scale,28*scale},8*scale,{186,134,43,255});
    const Rectangle inner{left+6*scale,barY+6*scale,barW-12*scale,22*scale};
    drawChamferedFill(inner,6*scale,{59,31,35,255});
    const float health=static_cast<float>(std::clamp(boss->health/std::max(1.0,boss->maxHealth),0.0,1.0));
    const Color top=boss->bossPhase>=3 ? Color{255,156,102,255} : Color{255,228,95,255};
    const Color bottom=boss->bossPhase>=3 ? Color{220,67,61,255} : Color{236,141,25,255};
    bossHud::fill({inner.x,inner.y,inner.width*health,inner.height},top,bottom);
    // Actual HP thresholds, independent of fill width.
    for(const float threshold:{.3F,.6F}) {
        const float px=inner.x+inner.width*threshold;
        drawChamferedFill({px-1*scale,inner.y,2*scale,5*scale},0,{65,36,24,200});
        drawChamferedFill({px-1*scale,inner.y+17*scale,2*scale,5*scale},0,{65,36,24,200});
    }
    menuStyle::text(TextFormat("%d / %d",boss->health>0 ? std::max(1,combatDisplayValue(boss->health)) : 0,
        std::max(1,combatDisplayValue(boss->maxHealth))),
        inner,14*scale,menuStyle::Cream,UiFontRole::Display,1);
    menuStyle::panel(box(x-4*scale,barY-10*scale,57,54),.55F*scale,{67,54,43,255});
    bossHud::crown({x+24*scale,barY+16*scale},scale);
    const char* action=nullptr;
    if(boss->bossInterruptRemaining>0) action="STORM INTERRUPTED";
    else if(boss->state==EnemyState::BossPhaseTransition) action="CROWN SHIELD";
    else if(boss->state==EnemyState::BossVolleyWindup) action=boss->bossAttackPlayer?"ROYAL VOLLEY":"CORE VOLLEY";
    else if(boss->state==EnemyState::BossSlamWindup) action="DIVE - MOVE!";
    else if(boss->state==EnemyState::BossStormWindup) action="BREAK STORM - HIT THE KING!";
    else if(boss->bossStrikesRemaining>0) action=boss->bossAttackPlayer?"FEATHER STORM":"CORE SIEGE";
    else if(boss->state==EnemyState::BossWarCryWindup) action="CALLING THE COURT";
    if(action) menuStyle::text(action,{left,barY+38*scale,barW,21*scale},14*scale,
        cream,UiFontRole::Display,1);
    if(boss->state==EnemyState::BossStormWindup && boss->bossStormInterruptDamage>0) {
        const Rectangle meter{left+barW*.15F,barY+61*scale,barW*.70F,7*scale};
        drawChamferedFill(meter,2*scale,BLACK);
        const float progress=static_cast<float>(std::clamp(boss->bossStormDamageTaken/boss->bossStormInterruptDamage,0.0,1.0));
        drawChamferedFill({meter.x+scale,meter.y+scale,(meter.width-2*scale)*progress,5*scale},scale,
            {131,228,164,255});
    }
}
} // namespace ian
