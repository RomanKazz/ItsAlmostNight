#include "ui/UiMenuLayout.hpp"
#include "ui/ShopMenu.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "progression/RunUpgrade.hpp"
#include "ui/UiText.hpp"
#include "ui/UiMenuStyle.hpp"
#include "ui/UiItemTooltip.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace ian {
namespace {
using menuStyle::Cream;
using menuStyle::Gold;
using menuStyle::Muted;
using menuStyle::Green;
using menuStyle::panel;
using menuStyle::text;
constexpr Color Blue{87,197,229,255}, Red{244,102,84,255};
bool button(Rectangle r, std::string_view title, const UiMenuLayout& l, const ShopMenuInput& input, bool enabled=true) {
    return menuStyle::button(r,l.scale,title,18,input.pointer,input.click,enabled);
}
void silver(const GameUi& ui, const UiMenuLayout& l, float x, float y, int amount,
            bool enough=true, float numberWidth=130) {
    ui.drawResourceIcon(l.rect(x,y,32,32),UiResourceIcon::Silver);
    text(std::to_string(amount),l.rect(x+40,y,numberWidth,32),22*l.scale,enough?Cream:Red,UiFontRole::BodyBold);
}
void icon(const GameUi& ui, MetaShopUpgrade upgrade, Rectangle r, bool locked) {
    const Texture2D texture=ui.metaShopTexture(upgrade);
    if (texture.id==0) return;
    const float fit=std::min(r.width/static_cast<float>(texture.width),r.height/static_cast<float>(texture.height));
    const float w=static_cast<float>(texture.width)*fit,h=static_cast<float>(texture.height)*fit;
    DrawTexturePro(texture,{0,0,static_cast<float>(texture.width),static_cast<float>(texture.height)},
        {r.x+(r.width-w)*.5F,r.y+(r.height-h)*.5F,w,h},{},0,locked?Color{9,14,17,255}:WHITE);
}
void levels(Rectangle r, int current, int max) {
    const float gap=3*r.height/12;
    const float size=std::min(r.height,(r.width-static_cast<float>(max-1)*gap)/static_cast<float>(max));
    const float total=static_cast<float>(max)*size+static_cast<float>(max-1)*gap;
    for(int i=0;i<max;++i) DrawRectangleRounded({r.x+(r.width-total)*.5F+static_cast<float>(i)*(size+gap),r.y,size,size},.12F,4,
        i<current?Gold:menuStyle::Hover);
}
std::string bonusLabel(const MetaShopDefinition& d, int level) {
    if(d.upgrade==MetaShopUpgrade::Toggler) return level>0 ? "ON" : "OFF";
    return "+"+std::to_string(level*d.bonusPerLevel)+((d.upgrade==MetaShopUpgrade::Rerolls || d.upgrade==MetaShopUpgrade::Banishes || d.upgrade==MetaShopUpgrade::Skips || d.upgrade==MetaShopUpgrade::TomeSlots || d.upgrade==MetaShopUpgrade::WeaponSlots || d.upgrade==MetaShopUpgrade::BuildingSlots)?"":"%");
}
}
Rectangle shopMenuPanelBounds(Vector2 viewport) { return UiMenuLayout::fromViewport(viewport).rect(340,130,1240,820); }
ShopMenuResult drawShopMenu(const GameUi& ui, MetaProgression& profile, ShopMenuState& state,
                            Vector2 viewport, const ShopMenuInput& input) {
    const UiMenuLayout l=UiMenuLayout::fromViewport(viewport);
    ShopMenuResult result{.close=input.close};
    if(!activeMetaShopUpgrade(state.selected)) state.selected=MetaShopUpgrade::SilverGain;
    if(input.selectionStep!=0) {
        const auto current=std::find(ActiveMetaShopUpgrades.begin(),ActiveMetaShopUpgrades.end(),state.selected)-ActiveMetaShopUpgrades.begin();
        const auto index=std::clamp<std::int64_t>(current+static_cast<std::int64_t>(input.selectionStep),0,
            static_cast<std::int64_t>(ActiveMetaShopUpgrades.size()-1));
        state.selected=ActiveMetaShopUpgrades[static_cast<std::size_t>(index)];result.interaction=true;
    }
    panel(shopMenuPanelBounds(viewport),l.scale,menuStyle::Frame);
    panel(l.rect(358,148,1204,72),l.scale,menuStyle::Card);
    text("SHOP",l.rect(382,158,690,50),32*l.scale,Cream,UiFontRole::Display);
    silver(ui,l,1340,169,profile.silver,true,100);
    if(menuStyle::closeButton(l.rect(1490,158,52,52),l.scale,input.pointer,input.click)) result.close=true;
    text("PERMANENT UPGRADES",l.rect(380,233,950,26),16*l.scale,Cream,UiFontRole::Display);
    panel(l.rect(358,266,1204,506),l.scale,menuStyle::Inset);
    const MetaShopDefinition* hoveredUpgrade = nullptr;
    for(std::size_t i=0;i<ActiveMetaShopUpgrades.size();++i) {
        const auto& d=MetaShopDefinitions[static_cast<std::size_t>(ActiveMetaShopUpgrades[i])];const int level=metaShopLevel(profile,d.upgrade);
        const bool unlocked=metaShopAvailable(profile,d.upgrade);
        constexpr float pitch = 1185.0F / MetaShopGridColumns;
        constexpr float width = pitch - 20.0F;
        const float x=378+static_cast<float>(i%MetaShopGridColumns)*pitch,
            y=282+static_cast<float>(i/MetaShopGridColumns)*240;
        const Rectangle tile=l.rect(x,y,width,218);
        const bool hover=CheckCollisionPointRec(input.pointer,tile);
        if (hover) hoveredUpgrade = &d;
        if(hover&&input.click) { state.selected=d.upgrade;result.interaction=true; }
        const bool selected=state.selected==d.upgrade;
        panel(tile,l.scale,menuStyle::selectionFill(selected,hover,!unlocked));
        icon(ui,d.upgrade,l.rect(x+(width-131)*.5F,y+10,131,129),!unlocked);
        text(d.title,l.rect(x+9,y+146,width-18,26),16*l.scale,selected||unlocked?Cream:Muted,UiFontRole::BodyBold,1);
        panel(l.rect(x+8,y+180,width-16,28),l.scale,menuStyle::Inset);
        levels(l.rect(x+19,y+188,width-38,12),level,d.maximumLevel);
        if(!unlocked) text("?",l.rect(x+width-34,y+8,24,28),20*l.scale,Muted,UiFontRole::BodyBold,1);
        if(level==d.maximumLevel) text("MAX",l.rect(x+width-55,y+9,46,22),12*l.scale,Blue,UiFontRole::BodyBold,1);
    }
    const auto& d=MetaShopDefinitions[static_cast<std::size_t>(state.selected)];
    const int level=metaShopLevel(profile,state.selected),price=metaShopCost(profile,state.selected);
    const bool unlocked=metaShopAvailable(profile,state.selected),maxed=level>=d.maximumLevel;
    panel(l.rect(358,788,1204,144),l.scale,menuStyle::Inset);
    icon(ui,d.upgrade,l.rect(378,807,104,105),!unlocked);
    text(d.title,l.rect(510,799,680,30),23*l.scale,Cream,UiFontRole::Display);
    text(d.description,l.rect(510,835,680,29),16*l.scale,Cream);
    text("CURRENT",l.rect(510,875,96,23),13*l.scale,Muted,UiFontRole::BodyBold);
    text(bonusLabel(d,level),l.rect(610,875,82,23),16*l.scale,Blue,UiFontRole::BodyBold);
    if(!maxed) {
        text("NEXT",l.rect(708,875,70,23),13*l.scale,Muted,UiFontRole::BodyBold);
        text(bonusLabel(d,level+1),l.rect(781,875,86,23),16*l.scale,Green,UiFontRole::BodyBold);
    }
    text(std::to_string(level)+" / "+std::to_string(d.maximumLevel),l.rect(1087,875,105,23),15*l.scale,Muted,UiFontRole::BodyBold,2);
    text(d.upgrade==MetaShopUpgrade::SilverGain?"Applies to future quest rewards.":d.upgrade==MetaShopUpgrade::Toggler ? "Configure your next run in Unlocks." : "Applies from your next run.",
        l.rect(510,906,682,20),12*l.scale,Muted);
    if(!unlocked) {
        text("Complete its quest to unlock this upgrade.",l.rect(1220,801,318,40),15*l.scale,Muted,UiFontRole::BodyBold,1);
        if(button(l.rect(1240,852,286,45),"OPEN UNLOCKS",l,input)) {result.openUnlocks=true;result.interaction=true;}
    } else if(maxed) {
        text("MAX LEVEL",l.rect(1230,839,309,42),22*l.scale,Green,UiFontRole::BodyBold,1);
    } else {
        silver(ui,l,1320,800,price,profile.silver>=price);
        const bool enabled=profile.silver>=price;
        const Rectangle buyBounds=l.rect(1240,848,286,45);
        const bool bought=button(buyBounds,"",l,input,enabled);
        const auto purchaseIcon=ui.shopActionTexture(ShopActionIcon::Purchase);
        if(IsTextureValid(purchaseIcon)) DrawTexturePro(purchaseIcon,{0,0,static_cast<float>(purchaseIcon.width),static_cast<float>(purchaseIcon.height)},
            l.rect(1252,853,35,35),{},0,enabled?WHITE:Muted);
        text("BUY",l.rect(1288,848,223,45),18*l.scale,enabled?Cream:Muted,UiFontRole::Display,1);
        if(bought||(input.confirm&&enabled)) {
            result.purchase=purchaseMetaShopUpgrade(profile,state.selected)==MetaShopPurchaseResult::Purchased;
            result.interaction=true;
        }
        if(!enabled) text("NOT ENOUGH SILVER",l.rect(1230,903,309,22),13*l.scale,Red,UiFontRole::BodyBold,1);
    }
    if (hoveredUpgrade) drawItemTooltip(hoveredUpgrade->title,hoveredUpgrade->description,input.pointer,viewport,l.scale);
    return result;
}
} // namespace ian
