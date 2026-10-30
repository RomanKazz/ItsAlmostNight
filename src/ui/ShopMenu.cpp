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
    state.motion.advance(input.deltaTime,input.reduceEffects);
    if(!activeMetaShopUpgrade(state.selected)) state.selected=MetaShopUpgrade::SilverGain;
    if(input.selectionStep!=0) {
        const auto current=std::find(ActiveMetaShopUpgrades.begin(),ActiveMetaShopUpgrades.end(),state.selected)-ActiveMetaShopUpgrades.begin();
        const auto index=std::clamp<std::int64_t>(current+static_cast<std::int64_t>(input.selectionStep),0,
            static_cast<std::int64_t>(ActiveMetaShopUpgrades.size()-1));
        state.selected=ActiveMetaShopUpgrades[static_cast<std::size_t>(index)];result.interaction=true;
    }
    state.motion.select(0,std::to_string(static_cast<int>(state.selected)));
    panel(shopMenuPanelBounds(viewport),l.scale,MenuPageSurface);
    menuPageTitle(l.rect(600,153,720,58),"SHOP",l.scale,state.motion);
    silver(ui,l,1340,169,profile.silver,true,100);
    if(menuStyle::closeButton(l.rect(1490,158,52,52),l.scale,input.pointer,input.click)) result.close=true;
    const MetaShopDefinition* hoveredUpgrade = nullptr;
    for(std::size_t i=0;i<ActiveMetaShopUpgrades.size();++i) {
        const auto& definition=MetaShopDefinitions[static_cast<std::size_t>(ActiveMetaShopUpgrades[i])];
        const int rank=metaShopLevel(profile,definition.upgrade);
        const bool available=metaShopAvailable(profile,definition.upgrade);
        constexpr float pitch=1185.0F/MetaShopGridColumns;
        constexpr float width=pitch-20;
        const float x=378+static_cast<float>(i%MetaShopGridColumns)*pitch;
        const float y=282+static_cast<float>(i/MetaShopGridColumns)*240;
        const Rectangle tile=l.rect(x,y,width,218);
        const bool hover=CheckCollisionPointRec(input.pointer,tile);
        if(hover) hoveredUpgrade=&definition;
        if(hover && input.click) { state.selected=definition.upgrade;result.interaction=true; }
        const bool selected=state.selected==definition.upgrade;
        menuChoicePlate(tile,l.scale,selected,hover,!available,state.motion);
        icon(ui,definition.upgrade,state.motion.icon(l.rect(x+(width-138)*.5F,y+9,138,132),
            static_cast<int>(i),selected,std::to_string(static_cast<int>(definition.upgrade))),!available);
        text(definition.title,l.rect(x+9,y+146,width-18,28),20*l.scale,
            selected || available?Cream:Muted,UiFontRole::Display,1);
        levels(l.rect(x+19,y+188,width-38,12),rank,definition.maximumLevel);
        if(!available) text("?",l.rect(x+width-34,y+8,24,28),20*l.scale,Muted,UiFontRole::Display,1);
        if(rank==definition.maximumLevel) text("MAX",l.rect(x+width-55,y+9,46,22),12*l.scale,Blue,UiFontRole::Display,1);
    }
    state.motion.hover(hoveredUpgrade?std::to_string(static_cast<int>(hoveredUpgrade->upgrade)):"");
    state.motion.select(0,std::to_string(static_cast<int>(state.selected)));
    const auto& d=MetaShopDefinitions[static_cast<std::size_t>(state.selected)];
    const int level=metaShopLevel(profile,state.selected),price=metaShopCost(profile,state.selected);
    const bool unlocked=metaShopAvailable(profile,state.selected),maxed=level>=d.maximumLevel;
    panel(l.rect(358,788,1204,144),l.scale,menuStyle::Inset);
    const float shift=(1-state.motion.reveal())*8;
    text(d.title,l.rect(382,801+shift,790,32),25*l.scale,Cream,UiFontRole::Display);
    text(d.description,l.rect(382,843+shift,790,30),19*l.scale,Cream);
    const auto current=bonusLabel(d,level);
    text(current,l.rect(382,886,125,28),23*l.scale,maxed?Green:Gold,UiFontRole::MenuBodyBold);
    if(!maxed) {
        const auto arrow=l.rect(517,900,26,0);
        DrawLineEx({arrow.x,arrow.y},{arrow.x+arrow.width,arrow.y},2*l.scale,Muted);
        DrawTriangle({arrow.x+arrow.width+5*l.scale,arrow.y},
            {arrow.x+arrow.width-3*l.scale,arrow.y-5*l.scale},
            {arrow.x+arrow.width-3*l.scale,arrow.y+5*l.scale},Muted);
        text(bonusLabel(d,level+1),l.rect(560,886,180,28),23*l.scale,Green,UiFontRole::MenuBodyBold);
    }
    if(!unlocked) {
        text("LOCKED",l.rect(1240,804,286,30),18*l.scale,Muted,UiFontRole::Display,1);
        if(button(l.rect(1240,852,286,45),"OPEN UNLOCKS",l,input)) {result.openUnlocks=true;result.interaction=true;}
    } else if(maxed) {
        text("MAX LEVEL",l.rect(1230,839,309,42),22*l.scale,Green,UiFontRole::BodyBold,1);
    } else {
        const bool enabled=profile.silver>=price;
        const Rectangle buyBounds=l.rect(1240,848,286,45);
        const bool bought=menuPrimaryButton(buyBounds,l.scale,"",input.pointer,input.click,enabled);
        text("BUY",l.rect(1250,848,130,45),21*l.scale,enabled?Cream:Muted,UiFontRole::Display,1);
        silver(ui,l,1385,855,price,enabled,80);
        if(bought||(input.confirm&&enabled)) {
            result.purchase=purchaseMetaShopUpgrade(profile,state.selected)==MetaShopPurchaseResult::Purchased;
            result.interaction=true;
            if(result.purchase) state.motion.feedback=.36F;
        }
        if(!enabled) text("NOT ENOUGH SILVER",l.rect(1230,903,309,22),13*l.scale,Red,UiFontRole::BodyBold,1);
    }
    menuActionBurst({l.rect(1383,870,0,0).x,l.rect(1383,870,0,0).y},52*l.scale,state.motion);
    if (hoveredUpgrade && hoveredUpgrade->upgrade!=state.selected) drawItemTooltip(hoveredUpgrade->title,hoveredUpgrade->description,input.pointer,viewport,l.scale);
    return result;
}
} // namespace ian
