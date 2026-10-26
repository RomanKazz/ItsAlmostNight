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
    // Match the fixed UI canvas used by the two inventory rows at the left.
    // The defense row ends at y=194.8; keep the notice directly underneath it.
    const float width=std::min(390.0F,screen.x-40);
    const Rectangle bounds{20,
        212+(1-entrance)*(1-entrance)*18+(1-exit)*12,
        width,72};
    drawSolidRoundedRect(bounds,.3F,Fade({22,33,28,235},opacity));
    const auto icon=questCompletionIcon(ui,notice.reward);
    const float pop=reduceFlashes?1:1+std::sin(entrance*PI)*.08F;
    const float iconSize=48*pop;
    if(IsTextureValid(icon)) DrawTexturePro(icon,
        {0,0,static_cast<float>(icon.width),static_cast<float>(icon.height)},
        {bounds.x+36-iconSize*.5F,bounds.y+36-iconSize*.5F,iconSize,iconSize},
        {},0,Fade(WHITE,opacity));
    const float x=bounds.x+72,w=bounds.width-84;
    const bool ru=currentLanguage()==Language::Russian;
    menuStyle::text(ru?"КВЕСТ ВЫПОЛНЕН":"QUEST COMPLETE",
        {x,bounds.y+9,w,24},20,Fade({170,228,104,255},opacity),UiFontRole::Display);
    menuStyle::text(localizeText(notice.title),{x,bounds.y+36,w,25},23,
        Fade(menuStyle::Cream,opacity),UiFontRole::Display);
}
} // namespace ian
