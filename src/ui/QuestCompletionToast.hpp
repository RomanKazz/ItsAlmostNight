#pragma once

#include "ui/GameUi.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/UiViewport.hpp"
#include "localization/Localization.hpp"
#include "progression/ContentUnlocks.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace ian {
inline constexpr double QuestCompletionNoticeDuration=4.2;
struct QuestCompletionNotice {
    std::string id;
    std::string title;
    QuestReward reward;
};

inline Texture2D questCompletionIcon(const GameUi& ui,const QuestReward& reward) {
    for(const auto& d:CardUnlockDefinitions) if(d.id==reward.unlockId) return ui.runUpgradeTexture(d.effect);
    for(const auto& d:ItemUnlockDefinitions) if(d.id==reward.unlockId) return ui.lootTexture(d.effect);
    for(const auto& d:MetaShopDefinitions) if(!reward.unlockId.empty() && d.requiredUnlock==reward.unlockId)
        return ui.metaShopTexture(d.upgrade);
    return ui.mainMenuTexture(MainMenuTexture::ScrollIcon);
}

inline void drawQuestCompletionToast(const GameUi& ui,const QuestCompletionNotice& notice,
    double remaining,bool reduceFlashes,Vector2 screen) {
    const float age=static_cast<float>(QuestCompletionNoticeDuration-remaining);
    const float entrance=std::clamp(age/.24F,0.0F,1.0F);
    const float exit=std::clamp(static_cast<float>(remaining)/.35F,0.0F,1.0F);
    const float opacity=std::min(entrance,exit);
    if(opacity<=0) return;
    // Match the fixed UI canvas used by the three inventory rows at the left.
    // The last row ends at y=254.8; keep the notice directly underneath it.
    constexpr float s=.60F;
    const float width=std::min(650*s,screen.x-40);
    const Rectangle bounds{20,
        272+(1-entrance)*(1-entrance)*18+(1-exit)*12,
        width,156*s};
    menuStyle::panel({bounds.x-3*s,bounds.y-3*s,bounds.width+6*s,bounds.height+6*s},s,
        {139,205,79,255},opacity);
    menuStyle::panel(bounds,s,{25,37,30,255},opacity);
    const float iconSize=86*s;
    const Vector2 iconCenter{bounds.x+65*s,bounds.y+65*s};
    if(!reduceFlashes) {
        const float sparkle=std::max(0.0F,1-age/.9F);
        for(int i=0;i<8;++i) {
            const float angle=static_cast<float>(i)*PI*.25F+age*.3F;
            const Vector2 a{iconCenter.x+std::cos(angle)*43*s,iconCenter.y+std::sin(angle)*43*s};
            const Vector2 b{iconCenter.x+std::cos(angle)*(43+sparkle*13)*s,
                iconCenter.y+std::sin(angle)*(43+sparkle*13)*s};
            DrawLineEx(a,b,3*s,Fade({231,213,113,255},sparkle*opacity));
        }
    }
    const auto icon=questCompletionIcon(ui,notice.reward);
    const float pop=reduceFlashes?1:1+std::sin(entrance*PI)*.12F;
    if(IsTextureValid(icon)) DrawTexturePro(icon,{0,0,static_cast<float>(icon.width),static_cast<float>(icon.height)},
        {iconCenter.x-iconSize*pop*.5F,iconCenter.y-iconSize*pop*.5F,iconSize*pop,iconSize*pop},{},0,Fade(WHITE,opacity));
    const bool ru=currentLanguage()==Language::Russian;
    const float x=bounds.x+125*s,w=bounds.width-147*s;
    menuStyle::text(ru?"КВЕСТ ВЫПОЛНЕН!":"QUEST COMPLETE!",
        {x,bounds.y+12*s,w,25*s},20*s,Fade({170,228,104,255},opacity),UiFontRole::Display);
    menuStyle::text(localizeText(notice.title),{x,bounds.y+40*s,w,35*s},26*s,
        Fade(menuStyle::Cream,opacity),UiFontRole::Display);
    std::string reward;
    if(!notice.reward.unlockId.empty()) {
        reward=ru?(notice.reward.purchaseCost>0?"ДОСТУПНО В ОТКРЫТИЯХ":"НОВОЕ ОТКРЫТИЕ")
            :(notice.reward.purchaseCost>0?"AVAILABLE IN UNLOCKS":"NEW UNLOCK");
        const auto name=contentUnlockName(notice.reward.unlockId);
        if(!name.empty()) reward+=" — "+localizeText(name);
    } else if(notice.reward.silver>0) {
        reward=(ru?"ЗАБЕРИ В КВЕСТАХ: +":"COLLECT IN QUESTS: +")+
            std::to_string(notice.reward.silver)+(ru?" СЕРЕБРА":" SILVER");
    } else reward=ru?"ПРОГРЕСС СОХРАНЁН":"PROGRESS SAVED";
    menuStyle::text(reward,{x,bounds.y+80*s,w,32*s},17*s,
        Fade(menuStyle::Gold,opacity),UiFontRole::MenuBodyBold);
    if(notice.reward.silver>0 && !notice.reward.unlockId.empty()) {
        menuStyle::text(ru?"НАГРАДА В КВЕСТАХ":"REWARD IN QUESTS",{x,bounds.y+113*s,w-90*s,21*s},14*s,
            Fade(menuStyle::Cream,opacity),UiFontRole::MenuBodyBold);
        const Rectangle badge{bounds.x+bounds.width-96*s,bounds.y+110*s,74*s,26*s};
        menuStyle::panel(badge,s,{42,58,45,255},opacity);
        const auto coin=ui.resourceTexture(UiResourceIcon::Silver);
        if(IsTextureValid(coin)) DrawTexturePro(coin,{0,0,static_cast<float>(coin.width),static_cast<float>(coin.height)},
            {badge.x+7*s,badge.y+5*s,17*s,17*s},{},0,Fade(WHITE,opacity));
        menuStyle::text("+"+std::to_string(notice.reward.silver),{badge.x+27*s,badge.y,badge.width-32*s,badge.height},
            16*s,Fade(menuStyle::Cream,opacity),UiFontRole::MenuBodyBold);
    }
    DrawRectangleRec({x,bounds.y+141*s,w*std::clamp(static_cast<float>(remaining/QuestCompletionNoticeDuration),0.0F,1.0F),3*s},
        Fade({139,205,79,255},opacity));
}
} // namespace ian
