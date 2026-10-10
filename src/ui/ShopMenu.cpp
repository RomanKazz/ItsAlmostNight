#include "ui/UiMenuLayout.hpp"
#include "ui/ShopMenu.hpp"
#include "combat/PlayerWeaponSystem.hpp"
#include "progression/RunUpgrade.hpp"
#include "ui/UiText.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace ian {
namespace {
constexpr Color Cream{250,241,211,255}, Gold{255,205,105,255}, Muted{159,169,167,255}, Blue{87,197,229,255};
constexpr Color Red{244,102,84,255}, Green{131,215,138,255};
void panel(Rectangle r, float scale, Color fill, Color border={116,79,48,255}) {
    DrawRectangleRounded(r,.06F,8,fill);
    DrawRectangleRoundedLinesEx(r,.06F,8,2*scale,border);
}
void text(std::string_view value, Rectangle r, float size, Color color, UiFontRole role=UiFontRole::Body, int align=0) {
    const ScopedUiFontRole font(role);
    const float fit=fitUiTextSize(value,size,size*.6F,r.width,r.height);
    const Vector2 measured=measureUiText(value,fit);
    const float x=align==1?r.x+(r.width-measured.x)*.5F:align==2?r.x+r.width-measured.x:r.x;
    drawUiText(value,{x,r.y+(r.height-measured.y)*.5F},fit,color);
}
bool button(Rectangle r, std::string_view title, const UiMenuLayout& l, const ShopMenuInput& input, bool enabled=true) {
    const bool hover=enabled&&CheckCollisionPointRec(input.pointer,r);
    panel(r,l.scale,!enabled?Color{46,50,51,255}:hover?Color{212,128,51,255}:Color{159,88,38,255},enabled?Gold:Color{88,90,84,255});
    text(title,{r.x+8*l.scale,r.y,r.width-16*l.scale,r.height},18*l.scale,enabled?Cream:Muted,UiFontRole::BodyBold,1);
    return hover&&input.click;
}
void silver(const GameUi& ui, const UiMenuLayout& l, float x, float y, int amount, bool enough=true) {
    ui.drawResourceIcon(l.rect(x,y,32,32),UiResourceIcon::Silver);
    text(std::to_string(amount),l.rect(x+40,y,130,32),22*l.scale,enough?Cream:Red,UiFontRole::BodyBold);
}
void icon(const GameUi& ui, MetaShopUpgrade upgrade, Rectangle r, bool locked) {
    Texture2D texture{};
    switch(upgrade) {
    case MetaShopUpgrade::SilverGain: texture=ui.resourceTexture(UiResourceIcon::Silver);break;
    case MetaShopUpgrade::Luck: texture=ui.resourceTexture(UiResourceIcon::Crystal);break;
    case MetaShopUpgrade::DefenseDamage: texture=ui.coreMenuTexture(CoreMenuTexture::Turret);break;
    case MetaShopUpgrade::DefenseFireRate: texture=ui.runUpgradeTexture(RunUpgradeEffect::TwinBatteries);break;
    case MetaShopUpgrade::BuildingHealth: texture=ui.coreMenuTexture(CoreMenuTexture::Shield);break;
    case MetaShopUpgrade::ProductionSpeed: texture=ui.coreMenuTexture(CoreMenuTexture::Gear);break;
    case MetaShopUpgrade::TomeSlots: texture=ui.runUpgradeTexture(RunUpgradeEffect::Luck);break;
    case MetaShopUpgrade::ResourceYield: texture=ui.weaponTexture(PlayerWeapon::Pickaxe);break;
    case MetaShopUpgrade::Rerolls: {
        const Vector2 c{r.x+r.width*.5F,r.y+r.height*.5F};
        const Color color=locked?Color{9,14,17,255}:Blue;
        DrawRing(c,r.width*.24F,r.width*.41F,33,322,36,{9,14,17,255});
        DrawRing(c,r.width*.27F,r.width*.38F,35,320,36,color);
        const Vector2 tip{c.x+r.width*.41F,c.y+r.height*.13F};
        DrawTriangle({tip.x-r.width*.065F,tip.y-r.height*.275F},
            {tip.x-r.width*.25F,tip.y+r.height*.02F},{tip.x+r.width*.025F,tip.y+r.height*.02F},{9,14,17,255});
        DrawTriangle({tip.x-r.width*.05F,tip.y-r.height*.25F},
            {tip.x-r.width*.22F,tip.y},tip,color);
        return;
    }
    case MetaShopUpgrade::Count: return;
    }
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
        i<current?Blue:Color{76,87,89,255});
}
std::string bonusLabel(const MetaShopDefinition& d, int level) {
    return "+"+std::to_string(level*d.bonusPerLevel)+((d.upgrade==MetaShopUpgrade::Rerolls || d.upgrade==MetaShopUpgrade::TomeSlots)?"":"%");
}
}
Rectangle shopMenuPanelBounds(Vector2 viewport) { return UiMenuLayout::fromViewport(viewport).rect(340,130,1240,820); }
ShopMenuResult drawShopMenu(const GameUi& ui, MetaProgression& profile, ShopMenuState& state,
                            Vector2 viewport, const ShopMenuInput& input) {
    const UiMenuLayout l=UiMenuLayout::fromViewport(viewport);
    ShopMenuResult result{.close=input.close};
    if(static_cast<std::size_t>(state.selected)>=MetaShopUpgradeCount) state.selected=MetaShopUpgrade::SilverGain;
    if(input.selectionStep!=0) {
        const auto index=std::clamp<std::int64_t>(static_cast<int>(state.selected)+static_cast<std::int64_t>(input.selectionStep),0,static_cast<int>(MetaShopUpgradeCount)-1);
        state.selected=static_cast<MetaShopUpgrade>(index);result.interaction=true;
    }
    panel(shopMenuPanelBounds(viewport),l.scale,{26,33,36,250});
    panel(l.rect(358,148,1204,72),l.scale,{35,47,48,255});
    text("SHOP",l.rect(382,158,690,50),32*l.scale,Cream,UiFontRole::Display);
    silver(ui,l,1374,169,profile.silver);
    if(button(l.rect(1505,162,40,40),"X",l,input)) result.close=true;
    text("PERMANENT UPGRADES",l.rect(380,233,950,26),16*l.scale,Muted,UiFontRole::BodyBold);
    panel(l.rect(358,266,1204,506),l.scale,{20,28,31,255},{56,66,67,255});
    for(std::size_t i=0;i<MetaShopUpgradeCount;++i) {
        const auto& d=MetaShopDefinitions[i];const int level=metaShopLevel(profile,d.upgrade);
        const bool unlocked=metaShopAvailable(profile,d.upgrade);
        const float x=378+static_cast<float>(i%5)*237,y=282+static_cast<float>(i/5)*240;
        const Rectangle tile=l.rect(x,y,217,218);
        const bool hover=CheckCollisionPointRec(input.pointer,tile);
        if(hover&&input.click) { state.selected=d.upgrade;result.interaction=true; }
        panel(tile,l.scale,unlocked?Color{49,64,67,255}:Color{34,42,46,255},
            state.selected==d.upgrade?Gold:hover?Cream:Color{82,109,111,255});
        icon(ui,d.upgrade,l.rect(x+43,y+10,131,129),!unlocked);
        text(d.title,l.rect(x+9,y+146,199,26),17*l.scale,unlocked?Cream:Muted,UiFontRole::BodyBold,1);
        panel(l.rect(x+8,y+180,201,28),l.scale,{20,28,31,255},{49,63,66,255});
        levels(l.rect(x+19,y+188,179,12),level,d.maximumLevel);
        if(!unlocked) text("?",l.rect(x+183,y+8,24,28),20*l.scale,Muted,UiFontRole::BodyBold,1);
        if(level==d.maximumLevel) text("MAX",l.rect(x+162,y+9,46,22),12*l.scale,Blue,UiFontRole::BodyBold,1);
    }
    const auto& d=MetaShopDefinitions[static_cast<std::size_t>(state.selected)];
    const int level=metaShopLevel(profile,state.selected),price=metaShopCost(profile,state.selected);
    const bool unlocked=metaShopAvailable(profile,state.selected),maxed=level>=d.maximumLevel;
    panel(l.rect(358,788,1204,144),l.scale,{21,28,30,255},{77,76,60,255});
    icon(ui,d.upgrade,l.rect(378,807,104,105),!unlocked);
    text(d.title,l.rect(510,799,680,30),23*l.scale,Gold,UiFontRole::BodyBold);
    text(d.description,l.rect(510,835,680,29),16*l.scale,Cream);
    text("CURRENT",l.rect(510,875,96,23),13*l.scale,Muted,UiFontRole::BodyBold);
    text(bonusLabel(d,level),l.rect(610,875,82,23),16*l.scale,Blue,UiFontRole::BodyBold);
    if(!maxed) {
        text("NEXT",l.rect(708,875,70,23),13*l.scale,Muted,UiFontRole::BodyBold);
        text(bonusLabel(d,level+1),l.rect(781,875,86,23),16*l.scale,Green,UiFontRole::BodyBold);
    }
    text(std::to_string(level)+" / "+std::to_string(d.maximumLevel),l.rect(1087,875,105,23),15*l.scale,Muted,UiFontRole::BodyBold,2);
    text(d.upgrade==MetaShopUpgrade::SilverGain?"Applies to future quest rewards.":"Applies from your next run.",
        l.rect(510,906,682,20),12*l.scale,Muted);
    if(!unlocked) {
        text("Unlock Card Refresh first.",l.rect(1220,801,318,40),15*l.scale,Muted,UiFontRole::BodyBold,1);
        if(button(l.rect(1240,852,286,45),"OPEN UNLOCKS",l,input)) {result.openUnlocks=true;result.interaction=true;}
    } else if(maxed) {
        text("MAX LEVEL",l.rect(1230,839,309,42),22*l.scale,Green,UiFontRole::BodyBold,1);
    } else {
        silver(ui,l,1320,800,price,profile.silver>=price);
        const bool enabled=profile.silver>=price;
        if(button(l.rect(1240,848,286,45),"BUY",l,input,enabled)||(input.confirm&&enabled)) {
            result.purchase=purchaseMetaShopUpgrade(profile,state.selected)==MetaShopPurchaseResult::Purchased;
            result.interaction=true;
        }
        if(!enabled) text("NOT ENOUGH SILVER",l.rect(1230,903,309,22),13*l.scale,Red,UiFontRole::BodyBold,1);
    }
    return result;
}
} // namespace ian
